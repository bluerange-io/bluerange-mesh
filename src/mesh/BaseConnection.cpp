/**

Copyright (c) 2014-2017 "M-Way Solutions GmbH"
FruityMesh - Bluetooth Low Energy mesh protocol [http://mwaysolutions.com/]

This file is part of FruityMesh

FruityMesh is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

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


//The Connection Class does have methods like Connect,... but connections, service
//discovery or encryption are handeled by the Connectionmanager so that we can control
//The parallel flow of multiple connections

BaseConnection::BaseConnection(u8 id, ConnectionDirection direction, fh_ble_gap_addr_t* partnerAddress)
{
	//Initialize to defaults
	connectionType = ConnectionTypes::CONNECTION_TYPE_INVALID;
	packetSendQueue = new PacketQueue((u8*)packetSendBuffer, PACKET_SEND_BUFFER_SIZE);
	unreliableBuffersFree = 0;
	reliableBuffersFree = 1;
	partnerId = 0;
	connectionHandle = BLE_CONN_HANDLE_INVALID;
	packetReassemblyPosition = 0;
	packetSendPosition = 0;
	currentConnectionIntervalMs = 0;
	connectionHandshakedTimestampDs = 0;
	disconnectedTimestampDs = 0;
	reestablishTimeSec = Config->meshExtendedConnectionTimeoutSec;
	connectionState = ConnectionState::CONNECTION_STATE_CONNECTING;
	encryptionState = EncryptionState::NOT_ENCRYPTED;
	handshakeStartedDs = 0;
	rssiSamplesNum = 0;
	rssiSamplesSum = 0;
	rssiAverage = 0;
	connectionStateBeforeDisconnection = ConnectionState::CONNECTION_STATE_DISCONNECTED;
	disconnectionReason = BLE_HCI_STATUS_CODE_SUCCESS;
	droppedPackets = 0;
	sentReliable = 0;
	sentUnreliable = 0;
	connectionMtu = MAX_DATA_SIZE_PER_WRITE;

	//Save values from constructor
	this->partnerAddress = *partnerAddress;
	this->connectionId = id;
	this->direction = direction;
}

BaseConnection::~BaseConnection()
{
	delete packetSendQueue;
}

/*######## PUBLIC FUNCTIONS ###################################*/

void BaseConnection::Disconnect()
{
	//Save connection state before disconnection
	connectionStateBeforeDisconnection = connectionState;
	connectionState = ConnectionState::CONNECTION_STATE_DISCONNECTED;
	//FIXME: This method should be able to disconnect an active connection and disconnect a connection that is in the CONNECTING state

	if(connectionStateBeforeDisconnection == ConnectionState::CONNECTION_STATE_REESTABLISHING){
		//A disconnect on a reestablishing connection will kill it
		GS->cm->FinalDisconnectionHandler(this);
	} else {
		GAPController::getInstance()->disconnectFromPartner(connectionHandle);
	}
}


/*######## PRIVATE FUNCTIONS ###################################*/

/*######## HANDLER ###################################*/
void BaseConnection::ConnectionSuccessfulHandler(u16 connectionHandle, u16 connInterval)
{
	u32 err = 0;

	this->handshakeStartedDs = GS->node->appTimerDs;

	if (direction == CONNECTION_DIRECTION_IN)
		logt("CONN", "Incoming connection %d connected", connectionId);
	else
		logt("CONN", "Outgoing connection %d connected", connectionId);

	this->connectionHandle = connectionHandle;
	err = FruityHal::BleTxPacketCountGet(this->connectionHandle, &unreliableBuffersFree);
	if(err != NRF_SUCCESS){
		//BLE_ERROR_INVALID_CONN_HANDLE && NRF_ERROR_INVALID_ADDR can be ignored
	}

	//Save connection interval (min and max are the same values in this event)
	this->currentConnectionIntervalMs = connInterval;

	connectionState = ConnectionState::CONNECTION_STATE_CONNECTED;
}

void BaseConnection::ReconnectionSuccessfulHandler(ble_evt_t* bleEvent){
	logt("CONN", "Reconnection Successful");

	connectionHandle = bleEvent->evt.gap_evt.conn_handle;

	//Reinitialize tx buffers
	FruityHal::BleTxPacketCountGet(this->connectionHandle, &unreliableBuffersFree);
	reliableBuffersFree = 1;

	connectionState = ConnectionState::CONNECTION_STATE_HANDSHAKE_DONE;

	//TODO: do we have to get the tx_packet_count or update any other variables?
}



#define __________________SENDING__________________

bool BaseConnection::QueueData(BaseConnectionSendData* sendData, u8* data)
{
	//Reserve space in our sendQueue for the metadata and our data
	u8* buffer = packetSendQueue->Reserve(SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED + sendData->dataLength);

	if(buffer != NULL){
		//Copy both to the reserved space
		BaseConnectionSendDataPacked* sendDataPacked = (BaseConnectionSendDataPacked*)buffer;
		sendDataPacked->characteristicHandle = sendData->characteristicHandle;
		sendDataPacked->deliveryOption = sendData->deliveryOption;
		sendDataPacked->priority = sendData->priority;
		sendDataPacked->dataLength = sendData->dataLength;

		memcpy(buffer + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED, data, sendData->dataLength);

		FillTransmitBuffers();
		return true;
	} else {
		droppedPackets++;
		//TODO: Error handling: What should happen when the queue is full?
		//Currently, additional packets are dropped
		logt("ERROR", "Send queue is already full");
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

	while( isConnected())
	{
		//Check if there is important data from the subclass to be sent
		if(packetSendPosition == 0){
			TransmitHighPrioData();
		}

		//Quit sending if there are not packets to be sent
		if(packetSendQueue->_numElements <= 0) return;

		bool packetCouldNotBeSent = false;

		//Get one packet from the packet queue
		sizedData packet = packetSendQueue->PeekNext();
		
		if(packet.length > 0){
			//Unpack data from sendQueue
			BaseConnectionSendDataPacked* sendDataPacked = (BaseConnectionSendDataPacked*) packet.data;
			sendData->characteristicHandle = sendDataPacked->characteristicHandle;
			sendData->deliveryOption = (DeliveryOption)sendDataPacked->deliveryOption;
			sendData->priority = (DeliveryPriority)sendDataPacked->priority;
			sendData->dataLength = sendDataPacked->dataLength;
			data = (packet.data + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED);
		} else {
			//Packet Queue was empty
			return;
		}

		// Check if we have a free buffer for the chosen DeliveryType
		if (
			(sendData->deliveryOption == DeliveryOption::DELIVERY_OPTION_WRITE_REQ && reliableBuffersFree > 0)
			|| (sendData->deliveryOption == DeliveryOption::DELIVERY_OPTION_WRITE_CMD && unreliableBuffersFree > 0)
			|| (sendData->deliveryOption == DeliveryOption::DELIVERY_OPTION_NOTIFICATION && unreliableBuffersFree > 0)
			) {

			//The subclass is allowed to modify the packet before it is sent
			sizedData sentData = ProcessDataBeforeTransmission(sendData, data, packetBuffer);

			if(sentData.length == 0){
				logt("ERROR", "Packet processing failed");
				continue;
			}

			//Send the packet to the SoftDevice
			if(sendData->deliveryOption == DeliveryOption::DELIVERY_OPTION_WRITE_REQ)
			{
				err = GATTController::getInstance()->bleWriteCharacteristic(
						connectionHandle,
						sendData->characteristicHandle,
						sentData.data,
						sentData.length,
						true);
			}
			else if(sendData->deliveryOption == DeliveryOption::DELIVERY_OPTION_WRITE_CMD)
			{
				err = GATTController::getInstance()->bleWriteCharacteristic(
						connectionHandle,
						sendData->characteristicHandle,
						sentData.data,
						sentData.length,
						false);
			}
			else
			{
				err = GATTController::getInstance()->bleSendNotification(
						connectionHandle,
						sendData->characteristicHandle,
						sentData.data,
						sentData.length);
			}

			if(err == NRF_SUCCESS){
				//Consume a buffer because the packet was sent
				if(sendData->deliveryOption == DeliveryOption::DELIVERY_OPTION_WRITE_REQ){
					reliableBuffersFree--;
				} else {
					unreliableBuffersFree--;
				}

				PacketSuccessfullyQueuedWithSoftdevice(sendData, data, &sentData);

			} else if(err == NRF_ERROR_DATA_SIZE || err == NRF_ERROR_INVALID_PARAM){
				logt("ERROR", "GATT WRITE ERROR %u, malformed packet", err);
				packetSendQueue->DiscardNext(); // throw away packet
			} else {
				logt("ERROR", "GATT WRITE ERROR %u", err);
				//Will try to send it later
				packetCouldNotBeSent = true;
			}

		} else {
			packetCouldNotBeSent = true;
		}

		//Go to next connection if a packet (either reliable or unreliable)
		//could not be sent because the corresponding buffers are full
		if(packetCouldNotBeSent) return;
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

void BaseConnection::PacketSuccessfullyQueuedWithSoftdevice(BaseConnectionSendData* sendData, u8* data, sizedData* sentData)
{
	//Discard the last packet because it was now successfully sent
	packetSendQueue->DiscardNext();
}

//This function can split a packet if necessary for sending
//WARNING: Can only be used for one characteristic, does not support to be used in parallel
//The implementation must manage packetSendPosition and packetQueue discarding itself
//packetBuffer must have a size of connectionMtu
//The function will return a pointer to the packet and the dataLength
sizedData BaseConnection::GetSplitData(BaseConnectionSendData* sendData, u8* data, u8* packetBuffer)
{
	sizedData result;
	connPacketSplitHeader* resultHeader = (connPacketSplitHeader*) packetBuffer;
	
	//If we do not have to split the data, return data unmodified
	if(sendData->dataLength <= connectionMtu){
		result.data = data;
		result.length = sendData->dataLength;
		return result;
	}

	u8 payloadSize = connectionMtu - SIZEOF_CONN_PACKET_SPLIT_HEADER;

	//Check if this is the last packet
	if((packetSendPosition+1) * payloadSize >= sendData->dataLength){
		//End packet
		resultHeader->splitMessageType = MESSAGE_TYPE_SPLIT_WRITE_CMD_END;
		resultHeader->splitCounter = packetSendPosition;
		memcpy(
				packetBuffer + SIZEOF_CONN_PACKET_SPLIT_HEADER,
			data + packetSendPosition * payloadSize,
			sendData->dataLength - packetSendPosition * payloadSize);
		result.data = packetBuffer;
		result.length = (sendData->dataLength - packetSendPosition * payloadSize) + SIZEOF_CONN_PACKET_SPLIT_HEADER;
		if(result.length < 5){
			logt("ERROR", "Split packet because of very few bytes, optimisation?");
		}

		char stringBuffer[100];
		Logger::getInstance()->convertBufferToHexString(result.data, result.length, stringBuffer, 100);
		logt("CONN_DATA", "SPLIT_END_%u: %s", resultHeader->splitCounter, stringBuffer);

	} else {
		//Intermediate packet
		resultHeader->splitMessageType = MESSAGE_TYPE_SPLIT_WRITE_CMD;
		resultHeader->splitCounter = packetSendPosition;
		memcpy(
				packetBuffer + SIZEOF_CONN_PACKET_SPLIT_HEADER,
			data + packetSendPosition * payloadSize,
			payloadSize);
		result.data = packetBuffer;
		result.length = connectionMtu;

		char stringBuffer[100];
		Logger::getInstance()->convertBufferToHexString(result.data, result.length, stringBuffer, 100);
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
	if(sendData->dataLength - SIZEOF_CONN_PACKET_SPLIT_HEADER + packetReassemblyPosition > PACKET_REASSEMBLY_BUFFER_SIZE){
		logt("ERROR", "Packet too big for reassembly");
		packetReassemblyPosition = 0;
		return NULL;
	}

	u16 packetReassemblyDestination = packetHeader->splitCounter * (connectionMtu - SIZEOF_CONN_PACKET_SPLIT_HEADER);

	//Check if a packet was missing inbetween
	if(packetReassemblyPosition != packetReassemblyDestination){
		packetReassemblyPosition = 0;
		return NULL;
	}

	//Intermediate packets must always be a full MTU
	if(packetHeader->splitMessageType == MESSAGE_TYPE_SPLIT_WRITE_CMD && sendData->dataLength != connectionMtu){
		packetReassemblyPosition = 0;
		return NULL;
	}

	//Save at correct position in the reassembly buffer
	memcpy(
		packetReassemblyBuffer + packetReassemblyDestination,
		data + SIZEOF_CONN_PACKET_SPLIT_HEADER,
		sendData->dataLength);

	packetReassemblyPosition += sendData->dataLength - SIZEOF_CONN_PACKET_SPLIT_HEADER;

	//Intermediate packet, no full packet yet received
	if(packetHeader->splitMessageType == MESSAGE_TYPE_SPLIT_WRITE_CMD)
	{
		return NULL;
	}
	//Final data for multipart packet received, return data
	else if(packetHeader->splitMessageType == MESSAGE_TYPE_SPLIT_WRITE_CMD_END)
	{
		//Modify info for the reassembled packet
		sendData->dataLength = packetReassemblyPosition;
		data = packetReassemblyBuffer;

		//Reset the assembly buffer
		packetReassemblyPosition = 0;

		return data;
	}
	return data;
}

#define __________________HANDLER__________________
//This is called when a connection gets disconnected before it is deleted
void BaseConnection::DisconnectionHandler()
{
	//Reason?
	logt("DISCONNECT", "Disconnected %u from connId:%u, HCI:%u %s", partnerId, connectionId, disconnectionReason, Logger::getInstance()->getHciErrorString(disconnectionReason));

	//Save connection state before disconnection
	if(connectionState != ConnectionState::CONNECTION_STATE_DISCONNECTED) connectionStateBeforeDisconnection = connectionState;
	connectionState = ConnectionState::CONNECTION_STATE_DISCONNECTED;

}


//Handling general events
void BaseConnection::BleEventHandler(ble_evt_t* bleEvent)
{
	if(connectionState == ConnectionState::CONNECTION_STATE_DISCONNECTED) return;

	if(bleEvent->header.evt_id >= BLE_GAP_EVT_BASE && bleEvent->header.evt_id < BLE_GAP_EVT_LAST){
		if(bleEvent->evt.gap_evt.conn_handle != connectionHandle) return;

		switch(bleEvent->header.evt_id){
			case BLE_GAP_EVT_CONN_PARAM_UPDATE:
			{
				logt("CONN", "new connection params set");
				currentConnectionIntervalMs = bleEvent->evt.gap_evt.params.conn_param_update.conn_params.max_conn_interval;

				break;
			}
		}
	}
}

#define __________________HELPER______________________
/*######## HELPERS ###################################*/

i8 BaseConnection::GetAverageRSSI()
{
	if(connectionState >= ConnectionState::CONNECTION_STATE_CONNECTED) return rssiAverage;
	else return 0;
}

/* EOF */

