////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2019 M-Way Solutions GmbH
// ** Contact: https://www.blureange.io/licensing
// **
// ** This file is part of the Bluerange/FruityMesh implementation
// **
// ** $BR_BEGIN_LICENSE:GPL-EXCEPT$
// ** Commercial License Usage
// ** Licensees holding valid commercial Bluerange licenses may use this file in
// ** accordance with the commercial license agreement provided with the
// ** Software or, alternatively, in accordance with the terms contained in
// ** a written agreement between them and M-Way Solutions GmbH. 
// ** For licensing terms and conditions see https://www.bluerange.io/terms-conditions. For further
// ** information use the contact form at https://www.bluerange.io/contact.
// **
// ** GNU General Public License Usage
// ** Alternatively, this file may be used under the terms of the GNU
// ** General Public License version 3 as published by the Free Software
// ** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
// ** included in the packaging of this file. Please review the following
// ** information to ensure the GNU General Public License requirements will
// ** be met: https://www.gnu.org/licenses/gpl-3.0.html.
// **
// ** $BR_END_LICENSE$
// **
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////

#include <Node.h>
#include <BaseConnection.h>
#include <GATTController.h>
#include <GAPController.h>
#include <ConnectionManager.h>

#include <AppConnection.h>
#include <MeshConnection.h>

extern "C"{
#include <ble_hci.h>
#include <app_timer.h>
}

#define BASE_CONNECTION_MAX_SEND_RETRY 5
#define BASE_CONNECTION_MAX_SEND_FAIL 10

//The Connection Class does have methods like Connect,... but connections, service
//discovery or encryption are handeled by the Connectionmanager so that we can control
//The parallel flow of multiple connections

BaseConnection::BaseConnection(u8 id, ConnectionDirection direction, fh_ble_gap_addr_t* partnerAddress)
	: packetSendQueue(packetSendBuffer, PACKET_SEND_BUFFER_SIZE),
	packetSendQueueHighPrio(packetSendBufferHighPrio, PACKET_SEND_BUFFER_HIGH_PRIO_SIZE)
{
	//Initialize to defaults
	connectionType = ConnectionTypes::CONNECTION_TYPE_INVALID;
	unreliableBuffersFree = 0;
	reliableBuffersFree = 1;
	partnerId = 0;
	connectionHandle = BLE_CONN_HANDLE_INVALID;
	packetReassemblyPosition = 0;
	packetQueuedHandleCounter = PACKET_QUEUED_HANDLE_COUNTER_START;
	currentConnectionIntervalMs = 0;
	connectionHandshakedTimestampDs = 0;
	disconnectedTimestampDs = 0;
	reestablishTimeSec = Config->meshExtendedConnectionTimeoutSec;
	connectionState = ConnectionState::CONNECTING;
	encryptionState = EncryptionState::NOT_ENCRYPTED;
	handshakeStartedDs = 0;
	lastReportedRssi = 0;
	rssiAverageTimes1000 = 0;
	connectionStateBeforeDisconnection = ConnectionState::DISCONNECTED;
	disconnectionReason = BLE_HCI_STATUS_CODE_SUCCESS;
	droppedPackets = 0;
	sentReliable = 0;
	sentUnreliable = 0;
	connectionMtu = MAX_DATA_SIZE_PER_WRITE;
	connectionPayloadSize = MAX_DATA_SIZE_PER_WRITE;
	clusterUpdateCounter = 0;
	nextExpectedClusterUpdateCounter = 1;
	manualPacketsSent = 0;
	forceReestablish = false;
	appDisconnectionReason = AppDisconnectReason::UNKNOWN;

	//Save values from constructor
	this->partnerAddress = *partnerAddress;
	this->connectionId = id;
	this->direction = direction;

	//Generate a unique id for this connection
	GS->cm->uniqueConnectionIdCounter++;
	if(GS->cm->uniqueConnectionIdCounter == 0){
		GS->cm->uniqueConnectionIdCounter = 1;
	}
	uniqueConnectionId = GS->cm->uniqueConnectionIdCounter;

	//Save the creation time
	creationTimeDs = GS->appTimerDs;
}

BaseConnection::~BaseConnection()
{
}

/*######## PUBLIC FUNCTIONS ###################################*/

void BaseConnection::DisconnectAndRemove()
{
	//### STEP 1: Set Variables to disconnected so that other parts can reference these

	//Save connection state before disconnection
	connectionStateBeforeDisconnection = connectionState;
	connectionState = ConnectionState::DISCONNECTED;

	//### STEP 2: Try to disconnect (could already be disconnected or not yet even connected

	//Stop connecting if we were trying to build a connection
	//TODO: This does break sth. (tested in simulator) when used, why? We do not need it, so leave it out
	//if(GS->cm->pendingConnection == this) FruityHal::ConnectCancel();

	//Disconnect if possible
	GS->gapController->disconnectFromPartner(connectionHandle);


	//### STEP 3: Inform all necessary modules / etc...

	//Inform modules about all kinds of disconnections
	for(int i=0; i<MAX_MODULE_COUNT; i++)
	{
		if(GS->activeModules[i] != nullptr && GS->activeModules[i]->configurationPointer->moduleActive){
			GS->activeModules[i]->ConnectionDisconnectedHandler(this);
		}
	}

	//### STEP 4: Inform ConnectionManager to do the final cleanup
	GS->cm->DeleteConnection(this);
}


/*######## PRIVATE FUNCTIONS ###################################*/



#define __________________SENDING__________________


bool BaseConnection::QueueData(const BaseConnectionSendData &sendData, u8* data){
	return QueueData(sendData, data, true);
}

bool BaseConnection::QueueData(const BaseConnectionSendData &sendData, u8* data, bool fillTxBuffers)
{
	if (GS->node->configuration.nodeId == 4) {
		connPacketModule* p = (connPacketModule*)data;
		if (p->moduleId == moduleID::DEBUG_MODULE_ID && p->actionType == 2) {
			u16 pid = ((u16*)(p->data))[1];
			logt("CONN", "Queuing %u", pid);
		}
	}

	//Reserve space in our sendQueue for the metadata and our data
	u8* buffer;

	//Select the correct packet Queue
	//TODO: currently we only allow non-split data for high prio
	PacketQueue* activeQueue;

	if(sendData.priority == DeliveryPriority::HIGH && sendData.dataLength <= connectionMtu)
	{
		logt("CM", "Queuing in high prio queue");
		activeQueue = &packetSendQueueHighPrio;
	} else {
		logt("CM", "Queuing in normal prio queue");
		activeQueue = &packetSendQueue;
	}

	buffer = activeQueue->Reserve(SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED + sendData.dataLength);

	if(buffer != nullptr){
		activeQueue->numUnsentElements++;

		//Copy both to the reserved space
		BaseConnectionSendDataPacked* sendDataPacked = (BaseConnectionSendDataPacked*)buffer;
		sendDataPacked->characteristicHandle = sendData.characteristicHandle;
		sendDataPacked->deliveryOption = (u8)sendData.deliveryOption;
		sendDataPacked->priority = (u8)sendData.priority;
		sendDataPacked->dataLength = sendData.dataLength;
		sendDataPacked->sendHandle = PACKET_QUEUED_HANDLE_NOT_QUEUED_IN_SD;

		memcpy(buffer + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED, data, sendData.dataLength);

		if (fillTxBuffers) FillTransmitBuffers();
		return true;
	} else {
		GS->cm->droppedMeshPackets++;
		droppedPackets++;

		GS->logger->logCount(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_DROPPED_PACKETS);

		//TODO: Error handling: What should happen when the queue is full?
		//Currently, additional packets are dropped
		logt("ERROR", "Send queue is already full");

		//For safety, we try to fill the transmitbuffers if it got stuck
		if(fillTxBuffers) FillTransmitBuffers();
		return false;
	}
}

void BaseConnection::FillTransmitBuffers()
{
	u32 err = 0;

	DYNAMIC_ARRAY(packetBuffer, connectionMtu);
	BaseConnectionSendData sendDataStruct;
	BaseConnectionSendData* sendData = &sendDataStruct;
	u8* data;

	while(isConnected() && connectionState != ConnectionState::REESTABLISHING && connectionState != ConnectionState::REESTABLISHING_HANDSHAKE)
	{
		//Check if there is important data from the subclass to be sent
		if(packetSendQueue.packetSendPosition == 0){
			TransmitHighPrioData();
		}

		//Next, select the correct Queue from which we should be transmitting
		//TODO: Currently we do not allow message splitting in HighPrio Queue
		PacketQueue* activeQueue;
		if(packetSendQueueHighPrio.numUnsentElements > 0 && packetSendQueue.packetSendPosition == 0){
			//logt("CONN", "Queuing from high prio queue");
			activeQueue = &packetSendQueueHighPrio;
		} else if(packetSendQueue.numUnsentElements > 0){
			//logt("CONN", "Queuing from normal prio queue");
			activeQueue = &packetSendQueue;
		} else {
			return;
		}

		if (activeQueue->_numElements < activeQueue->numUnsentElements) {
			logt("ERROR", "Fail: Queue numElements");
			SIMERROR();
			GS->logger->logCount(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_QUEUE_NUM_MISMATCH);

			GS->cm->ForceDisconnectAllConnections(AppDisconnectReason::QUEUE_NUM_MISMATCH);
		}

		//Get the next packet from the packet queue that was not yet queued
		sizedData packet = activeQueue->PeekNext(activeQueue->_numElements - activeQueue->numUnsentElements);

		//Unpack data from sendQueue
		BaseConnectionSendDataPacked* sendDataPacked = (BaseConnectionSendDataPacked*)packet.data;
		sendData->characteristicHandle = sendDataPacked->characteristicHandle;
		sendData->deliveryOption = (DeliveryOption)sendDataPacked->deliveryOption;
		sendData->priority = (DeliveryPriority)sendDataPacked->priority;
		sendData->dataLength = sendDataPacked->dataLength;
		data = (packet.data + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED);

		// Check if we have a free buffer for the chosen DeliveryType
		if (
			(sendData->deliveryOption == DeliveryOption::WRITE_REQ && reliableBuffersFree > 0)
			|| (sendData->deliveryOption == DeliveryOption::WRITE_CMD && unreliableBuffersFree > 0)
			|| (sendData->deliveryOption == DeliveryOption::NOTIFICATION && unreliableBuffersFree > 0)
			) {

			//The subclass is allowed to modify the packet before it is sent, it will place the modified packet into the sentData struct
			//This could be e.g. only a part of the original packet ( a split packet )
			sizedData sentData = ProcessDataBeforeTransmission(sendData, data, packetBuffer);

			if(sentData.length == 0){
				logt("ERROR", "Packet processing failed");
				continue; //FIXME: this could break a connection
			}


			//Send the packet to the SoftDevice
			if(sendData->deliveryOption == DeliveryOption::WRITE_REQ)
			{
				err = GS->gattController->bleWriteCharacteristic(
						connectionHandle,
						sendData->characteristicHandle,
						sentData.data,
						sentData.length,
						true);
			}
			else if(sendData->deliveryOption == DeliveryOption::WRITE_CMD)
			{
				err = GS->gattController->bleWriteCharacteristic(
						connectionHandle,
						sendData->characteristicHandle,
						sentData.data,
						sentData.length,
						false);
			}
			else
			{
				err = GS->gattController->bleSendNotification(
						connectionHandle,
						sendData->characteristicHandle,
						sentData.data,
						sentData.length);
			}

			if(err == NRF_SUCCESS){
				//Consume a buffer because the packet was sent
				if(sendData->deliveryOption == DeliveryOption::WRITE_REQ){
					reliableBuffersFree--;
				} else {
					unreliableBuffersFree--;
				}

				//FIXME: This is not using the preprocessed data (sentData)
				PacketSuccessfullyQueuedWithSoftdevice(activeQueue, sendDataPacked, data, &sentData);

			} else {
				logt("ERROR", "GATT WRITE ERROR 0x%x", err);

				GS->logger->logCount(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_GATT_WRITE_ERROR);

				HandlePacketQueuingFail(*activeQueue, sendDataPacked, err);

				//Stop queuing packets for this connection to prevent infinite loops
				return;
			}

		} else {
			//Go to next connection if a packet (either reliable or unreliable)
			//could not be sent because the corresponding buffers are full
			return;
		}
	}
}

void BaseConnection::HandlePacketQueued(PacketQueue* activeQueue, BaseConnectionSendDataPacked* sendDataPacked)
{
	//Save the queue handle in the packet, decrease the number of packets to be sent from that queue and reset our fail counter for that queue
	sendDataPacked->sendHandle = GetNextQueueHandle();
	activeQueue->numUnsentElements--;
	activeQueue->packetFailedToQueueCounter = 0;
}

void BaseConnection::HandlePacketSent(u8 sentUnreliable, u8 sentReliable)
{
	//logt("CONN", "Data was sent %u, %u", sentUnreliable, sentReliable);

	//TODO: are write cmds and write reqs sent sequentially?
	//TODO: we must not send more than 100 packets from one queue, otherwise, the handles between
	//the queues will not match anymore to the sequence in that the packets were sent
	
	//We must iterate in a loop to delete all packets if more than one was sent
	u8 numSent = sentUnreliable + sentReliable;

	for(u32 i=0; i<numSent; i++){

		//Check if packets were sent manually using a softdevice call and thereby bypassing the sendQueues
		if (manualPacketsSent > 0) {
			manualPacketsSent--;
			continue;
		}

		//Find the queue from which the packet was sent
		PacketQueue* activeQueue;

		sizedData packet = packetSendQueue.PeekNext();
		BaseConnectionSendDataPacked* sendDataPacked = (BaseConnectionSendDataPacked*)packet.data;
		u8 handle = packet.length > 0 ? sendDataPacked->sendHandle : PACKET_QUEUED_HANDLE_NOT_QUEUED_IN_SD;

		packet = packetSendQueueHighPrio.PeekNext();
		sendDataPacked = (BaseConnectionSendDataPacked*)packet.data;
		u8 handleHighPrio = packet.length > 0 ? sendDataPacked->sendHandle : PACKET_QUEUED_HANDLE_NOT_QUEUED_IN_SD;

		//If a split packet has remaining parts, it was from the split queue
		if (packetSendQueue.packetSentRemaining > 0) {
			activeQueue = &packetSendQueue;
		}
		//Check if we do not have a queued packet in the normal queue
		else if (handle < PACKET_QUEUED_HANDLE_COUNTER_START) {
			activeQueue = &packetSendQueueHighPrio;
		}
		//Check if we do not have a queued packet in the high prio queue
		else if (handleHighPrio < PACKET_QUEUED_HANDLE_COUNTER_START) {
			activeQueue = &packetSendQueue;
		}
		//Check which handle is lower than the other handle using unsigned variables that will wrap
		else {
			if (handle - handleHighPrio < 100) {
				activeQueue = &packetSendQueueHighPrio;
			}
			else {
				activeQueue = &packetSendQueue;
			}
		}


		if(activeQueue->_numElements == 0){
			//TODO: Save Error
			logt("ERROR", "Fail: Queue");
			SIMERROR();

			GS->logger->logCount(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_HANDLE_PACKET_SENT_ERROR);
		}

		//Check if a split packet has more than one part that was not yet sent
		if (activeQueue->packetSentRemaining > 0) {
			activeQueue->packetSentRemaining--;
		}
		//Otherwise, either a normal packet or a split packet can be removed
		else {
			//TODO: Call handler that packet was sent

			activeQueue->DiscardNext();
		}
	}

	//Note the buffers that are not free again
	unreliableBuffersFree += sentUnreliable;
	sentUnreliable += sentUnreliable;

	reliableBuffersFree += sentReliable;
	sentReliable += sentReliable;
}

void BaseConnection::HandlePacketQueuingFail(PacketQueue& activeQueue, BaseConnectionSendDataPacked* sendDataPacked, u32 err)
{
	activeQueue.packetFailedToQueueCounter++;

	if (err != NRF_ERROR_DATA_SIZE && err != NRF_ERROR_TIMEOUT && err != NRF_ERROR_INVALID_ADDR && err != NRF_ERROR_INVALID_PARAM && err != NRF_ERROR_DATA_SIZE) {
		//The remaining errors can happen if the gap connection is temporarily lost during reestablishing
		return;
	}

	//Check queuing a packet failed too often so that the connection is probably broken (could also be too many wrong packets)
	if (activeQueue.packetFailedToQueueCounter > BASE_CONNECTION_MAX_SEND_FAIL) {
		appDisconnectionReason = AppDisconnectReason::TOO_MANY_SEND_RETRIES;
		DisconnectAndRemove();
	}
	//Check if a packet failed too often to be retried
	else if (activeQueue.packetFailedToQueueCounter > BASE_CONNECTION_MAX_SEND_RETRY) {
		activeQueue.DiscardNext();
		activeQueue.numUnsentElements--;
		activeQueue.packetFailedToQueueCounter = 0;
		activeQueue.packetSendPosition = 0; //Reset send position if maybe we were in the middle of a split packet
		activeQueue.packetSentRemaining = 0;
	}
}


void BaseConnection::ResendAllPackets(PacketQueue& queueToReset) const
{
	queueToReset.numUnsentElements = queueToReset._numElements;
	queueToReset.packetSendPosition = 0;
	queueToReset.packetSentRemaining = 0;

	//Clear all send handles
	for (int i = 0; i < queueToReset._numElements; i++)
	{
		sizedData packet = queueToReset.PeekNext(i);
		BaseConnectionSendDataPacked* sendDataPacked = (BaseConnectionSendDataPacked*)packet.data;
		sendDataPacked->sendHandle = PACKET_QUEUED_HANDLE_NOT_QUEUED_IN_SD;
	}
}

//This basic implementation returns the data as is
sizedData BaseConnection::ProcessDataBeforeTransmission(BaseConnectionSendData* sendData, u8* data, u8* packetBuffer)
{
	sizedData result;
	result.data = data;
	result.length = sendData->dataLength <= connectionMtu ? sendData->dataLength : 0;
	return result;
}

void BaseConnection::PacketSuccessfullyQueuedWithSoftdevice(PacketQueue* queue, BaseConnectionSendDataPacked* sendDataPacked, u8* data, sizedData* sentData)
{
	HandlePacketQueued(queue, sendDataPacked);
}

//This function can split a packet if necessary for sending
//WARNING: Can only be used for one characteristic, does not support to be used in parallel
//The implementation must manage packetSendPosition and packetQueue discarding itself
//packetBuffer must have a size of connectionMtu
//The function will return a pointer to the packet and the dataLength (do not use packetBuffer, use this pointer)
sizedData BaseConnection::GetSplitData(const BaseConnectionSendData &sendData, u8* data, u8* packetBuffer) const
{
	sizedData result;
	connPacketSplitHeader* resultHeader = (connPacketSplitHeader*) packetBuffer;
	
	//If we do not have to split the data, return data unmodified
	if(sendData.dataLength <= connectionPayloadSize){
		result.data = data;
		result.length = sendData.dataLength;
		return result;
	}

	u8 payloadSize = connectionPayloadSize - SIZEOF_CONN_PACKET_SPLIT_HEADER;

	//Check if this is the last packet
	if((packetSendQueue.packetSendPosition+1) * payloadSize >= sendData.dataLength){
		//End packet
		resultHeader->splitMessageType = MESSAGE_TYPE_SPLIT_WRITE_CMD_END;
		resultHeader->splitCounter = packetSendQueue.packetSendPosition;
		memcpy(
				packetBuffer + SIZEOF_CONN_PACKET_SPLIT_HEADER,
			data + packetSendQueue.packetSendPosition * payloadSize,
			sendData.dataLength - packetSendQueue.packetSendPosition * payloadSize);
		result.data = packetBuffer;
		result.length = (sendData.dataLength - packetSendQueue.packetSendPosition * payloadSize) + SIZEOF_CONN_PACKET_SPLIT_HEADER;
		if(result.length < 5){
			logt("ERROR", "Split packet because of very few bytes, optimisation?");
		}

		char stringBuffer[100];
		GS->logger->convertBufferToHexString(result.data, result.length, stringBuffer, sizeof(stringBuffer));
		logt("CONN_DATA", "SPLIT_END_%u: %s", resultHeader->splitCounter, stringBuffer);

	} else {
		//Intermediate packet
		resultHeader->splitMessageType = MESSAGE_TYPE_SPLIT_WRITE_CMD;
		resultHeader->splitCounter = packetSendQueue.packetSendPosition;
		memcpy(
				packetBuffer + SIZEOF_CONN_PACKET_SPLIT_HEADER,
			data + packetSendQueue.packetSendPosition * payloadSize,
			payloadSize);
		result.data = packetBuffer;
		result.length = connectionPayloadSize;

		char stringBuffer[100];
		GS->logger->convertBufferToHexString(result.data, result.length, stringBuffer, sizeof(stringBuffer));
		logt("CONN_DATA", "SPLIT_%u: %s", resultHeader->splitCounter, stringBuffer);
	}

	return result;
}

#define _________________RECEIVING_________________

//A reassembly function that can reassemble split packets, can be used from subclasses
//Must use connPacketHeader for all packets
u8* BaseConnection::ReassembleData(BaseConnectionSendData* sendData, u8* data)
{
	connPacketSplitHeader* packetHeader = (connPacketSplitHeader*)data;

	//If reassembly is not needed, return packet without modifying
	if(packetHeader->splitMessageType != MESSAGE_TYPE_SPLIT_WRITE_CMD && packetHeader->splitMessageType != MESSAGE_TYPE_SPLIT_WRITE_CMD_END){
		return data;
	}

	//Check if reassembly buffer limit is reached
	if(sendData->dataLength - SIZEOF_CONN_PACKET_SPLIT_HEADER + packetReassemblyPosition > packetReassemblyBuffer.length){
		logt("ERROR", "Packet too big for reassembly");
		packetReassemblyPosition = 0;
		return nullptr;
	}

	u16 packetReassemblyDestination = packetHeader->splitCounter * (connectionPayloadSize - SIZEOF_CONN_PACKET_SPLIT_HEADER);

	//Check if a packet was missing inbetween
	if(packetReassemblyPosition != packetReassemblyDestination){
		packetReassemblyPosition = 0;
		return nullptr;
	}

	//Intermediate packets must always be a full MTU
	if(packetHeader->splitMessageType == MESSAGE_TYPE_SPLIT_WRITE_CMD && sendData->dataLength != connectionPayloadSize){
		packetReassemblyPosition = 0;
		return nullptr;
	}

	//Save at correct position in the reassembly buffer
	memcpy(
		packetReassemblyBuffer.getRaw() + packetReassemblyDestination,
		data + SIZEOF_CONN_PACKET_SPLIT_HEADER,
		sendData->dataLength);

	packetReassemblyPosition += sendData->dataLength - SIZEOF_CONN_PACKET_SPLIT_HEADER;

	//Intermediate packet, no full packet yet received
	if(packetHeader->splitMessageType == MESSAGE_TYPE_SPLIT_WRITE_CMD)
	{
		return nullptr;
	}
	//Final data for multipart packet received, return data
	else if(packetHeader->splitMessageType == MESSAGE_TYPE_SPLIT_WRITE_CMD_END)
	{
		//Modify info for the reassembled packet
		sendData->dataLength = packetReassemblyPosition;
		data = packetReassemblyBuffer.getRaw();

		//Reset the assembly buffer
		packetReassemblyPosition = 0;

		return data;
	}
	return data;
}

#define __________________HANDLER__________________

void BaseConnection::ConnectionSuccessfulHandler(u16 connectionHandle, u16 connInterval)
{
	u32 err = 0;

	this->handshakeStartedDs = GS->appTimerDs;

	if (direction == ConnectionDirection::DIRECTION_IN)
		logt("CONN", "Incoming connection %d connected, iv %u", connectionId, connInterval);
	else
		logt("CONN", "Outgoing connection %d connected, iv %u", connectionId, connInterval);

	this->connectionHandle = connectionHandle;
	err = FruityHal::BleTxPacketCountGet(this->connectionHandle, &unreliableBuffersFree);
	if(err != NRF_SUCCESS){
		//BLE_ERROR_INVALID_CONN_HANDLE && NRF_ERROR_INVALID_ADDR can be ignored
	}

	//Save connection interval (min and max are the same values in this event)
	this->currentConnectionIntervalMs = connInterval;

	connectionState = ConnectionState::CONNECTED;
}

void BaseConnection::ReconnectionSuccessfulHandler(ble_evt_t& bleEvent){
	logt("CONN", "Reconnection Successful");

	connectionHandle = bleEvent.evt.gap_evt.conn_handle;

	//Reinitialize tx buffers
	FruityHal::BleTxPacketCountGet(this->connectionHandle, &unreliableBuffersFree);
	reliableBuffersFree = 1;

	connectionState = ConnectionState::HANDSHAKE_DONE;

	//TODO: do we have to get the tx_packet_count or update any other variables?
}

void BaseConnection::GATTServiceDiscoveredHandler(ble_db_discovery_evt_t &evt)
{

}

//This is called when a connection gets disconnected before it is deleted
bool BaseConnection::GapDisconnectionHandler(u8 hciDisconnectReason)
{
	//Reason?
	logt("CONN", "Disconnected %u from connId:%u, HCI:%u %s", partnerId, connectionId, hciDisconnectReason, GS->logger->getHciErrorString(hciDisconnectReason));

	this->disconnectionReason = hciDisconnectReason;
	this->disconnectedTimestampDs = GS->appTimerDs;

	//Save connection state before disconnection
	if(connectionState != ConnectionState::DISCONNECTED){
		connectionStateBeforeDisconnection = connectionState;
	}
	//Set State to disconnected
	connectionState = ConnectionState::DISCONNECTED;

	return true;
}


//Handling general events
void BaseConnection::BleEventHandler(ble_evt_t& bleEvent)
{
	if(connectionState == ConnectionState::DISCONNECTED) return;

	if(bleEvent.header.evt_id >= BLE_GAP_EVT_BASE && bleEvent.header.evt_id < BLE_GAP_EVT_LAST){
		if(bleEvent.evt.gap_evt.conn_handle == connectionHandle)
		{
			switch(bleEvent.header.evt_id){
				case BLE_GAP_EVT_CONN_PARAM_UPDATE:
				{
					logt("CONN", "new connection params set");
					currentConnectionIntervalMs = bleEvent.evt.gap_evt.params.conn_param_update.conn_params.max_conn_interval;

					break;
				}
			}
		}
	}
}

#define __________________HELPER______________________
/*######## HELPERS ###################################*/

i8 BaseConnection::GetAverageRSSI() const
{
	if(connectionState >= ConnectionState::CONNECTED){
		i32 divisor = 1000;
		//Round to closest rssi
		return ((rssiAverageTimes1000 < 0) ^ (divisor < 0)) ? ((rssiAverageTimes1000 - divisor/2)/divisor) : ((rssiAverageTimes1000 + divisor/2)/divisor);
	}
	else return 0;
}

u8 BaseConnection::GetNextQueueHandle()
{
	packetQueuedHandleCounter++;
	if (packetQueuedHandleCounter == 0) packetQueuedHandleCounter = PACKET_QUEUED_HANDLE_COUNTER_START;

	return packetQueuedHandleCounter;
}


sizedData BaseConnection::GetNextPacketToSend(const PacketQueue& queue) const
{
	for (u32 i = 0; i < queue._numElements; i++) {
		sizedData data = queue.PeekNext(i); //TODO: Optimize this, as this will iterate again through all items each loop
		BaseConnectionSendData* sendData = (BaseConnectionSendData*)data.data;

		if (sendData->sendHandle == PACKET_QUEUED_HANDLE_NOT_QUEUED_IN_SD) {
			return data;
		}
	}

	sizedData zeroData = {nullptr, 0};
	return zeroData;
}

/* EOF */

