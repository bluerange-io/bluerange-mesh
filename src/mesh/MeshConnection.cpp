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

#include <FruityMesh.h>
#include <ConnectionManager.h>
#include <MeshConnection.h>
#include <StatusReporterModule.h>
#include <Node.h>

extern "C"{
#include <ble_hci.h>
#include <app_timer.h>
}

#ifndef SIM_ENABLED
uint32_t meshConnTypeResolver __attribute__((section(".ConnTypeResolvers"), used)) = (u32)MeshConnection::ConnTypeResolver;
#endif

MeshConnection::MeshConnection(u8 id, ConnectionDirection direction, fh_ble_gap_addr_t* partnerAddress, u16 partnerWriteCharacteristicHandle)
	: BaseConnection(id, direction, partnerAddress)
{
	logt("CONN", "New MeshConnection");
	//Initialize to defaults
	connectionType = ConnectionTypes::CONNECTION_TYPE_FRUITYMESH;
	connectedClusterId = 0;
	connectedClusterSize = 0;
	connectionMasterBit = 0;
	memset(&clusterAck1Packet, 0x00, sizeof(connPacketClusterAck1));
	memset(&clusterAck2Packet, 0x00, sizeof(connPacketClusterAck2));
	clusterIDBackup = 0;
	clusterSizeBackup = 0;
	hopsToSinkBackup = -1;
	hopsToSink = -1;
	memset(&currentClusterInfoUpdatePacket, 0x00, sizeof(currentClusterInfoUpdatePacket));

	//Save values from constructor
	this->partnerWriteCharacteristicHandle = partnerWriteCharacteristicHandle;

	if(direction == ConnectionDirection::DIRECTION_IN){
		GS->cm->freeMeshInConnections--;
	} else if (direction == ConnectionDirection::DIRECTION_OUT){
		GS->cm->freeMeshOutConnections--;
	}

	GS->node->MeshConnectionConnectedHandler();
}

MeshConnection::~MeshConnection(){
	logt("CONN", "Deleted MeshConnection because %u", appDisconnectionReason);

	if (direction == ConnectionDirection::DIRECTION_IN) {
		GS->cm->freeMeshInConnections++;
	}
	else if (direction == ConnectionDirection::DIRECTION_OUT) {
		GS->cm->freeMeshOutConnections++;
	}
}

BaseConnection* MeshConnection::ConnTypeResolver(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8* data)
{
	logt("MACONN", "MeshConnResolver");

	//Check if the message was written to our mesh characteristic
	if(sendData->characteristicHandle == GS->node->meshService.sendMessageCharacteristicHandle.value_handle)
	{
		//Check if we already have an inConnection
		MeshConnections conn = GS->cm->GetMeshConnections(ConnectionDirection::DIRECTION_IN);
		if(conn.count >= Config->meshMaxInConnections){
			logt("CM", "Too many mesh in connections");
			u32 err = FruityHal::Disconnect(oldConnection->connectionHandle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
			return nullptr;
		}
		else
		{
			MeshConnection* newConnection = new MeshConnection(
				oldConnection->connectionId,
				oldConnection->direction,
				&oldConnection->partnerAddress,
				BLE_GATT_HANDLE_INVALID);

			newConnection->handshakeStartedDs = oldConnection->handshakeStartedDs;
			newConnection->unreliableBuffersFree = oldConnection->unreliableBuffersFree;

			return newConnection;
		}
	}

	return nullptr;
}


//void MeshConnection::DiscoverCharacteristicHandles()
//{
//	GS->gattController->bleDiscoverHandles(connectionHandle, &GS->node->meshService.serviceUuid);
//}
//
////When the mesh handle has been discovered
//void MeshConnection::GATTHandleDiscoveredHandler(u16 characteristicHandle)
//{
//	partnerWriteCharacteristicHandle = characteristicHandle;
//
//	StartHandshake();
//}


#define __________________CONNECTIVITY_________________

void MeshConnection::DisconnectAndRemove()
{
	//Make a backup of some important variables on the stack
	ConnectionState connectionStateBeforeDisconnection = this->connectionState == ConnectionState::DISCONNECTED ? this->connectionStateBeforeDisconnection : this->connectionState; // Depending on where this call comes from, the connection was already disconnected or we have to do it here
	u8 hadConnectionMasterBit = this->connectionMasterBit;
	ClusterSize connectedClusterSize = this->connectedClusterSize;
	ClusterId connectedClusterId = this->connectedClusterId;

	logt("CONN", "before remove %u, %u, %u, %u", connectionStateBeforeDisconnection,
			hadConnectionMasterBit,
			connectedClusterSize,
			connectedClusterId);

	//Call our lovely modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(GS->activeModules[i] != nullptr && GS->activeModules[i]->configurationPointer->moduleActive){
			GS->activeModules[i]->MeshConnectionChangedHandler(*this);
		}
	}

	if (connectionStateBeforeDisconnection >= ConnectionState::HANDSHAKE_DONE) {
		logjson("SIM", "{\"type\":\"mesh_disconnect\",\"partnerId\":%u}" SEP, partnerId);

		StatusReporterModule* statusMod = (StatusReporterModule*)GS->node->GetModuleById(moduleID::STATUS_REPORTER_MODULE_ID);
		if (statusMod != nullptr) {
			statusMod->SendLiveReport(LiveReportTypes::MESH_DISCONNECTED, partnerId, (u8)appDisconnectionReason);
		}
	}

	//Will kill the connection. Deletion is at the end of the function.
	//Do not use members after the following line! USE-AFTER-FREE!
	BaseConnection::DisconnectAndRemove();

	//Use our backup variables to tell the node about the lost connection
	//Is safe to call after deletion. All variables were backuped at the start of this function.
	GS->node->MeshConnectionDisconnectedHandler(
			connectionStateBeforeDisconnection,
			hadConnectionMasterBit,
			connectedClusterSize,
			connectedClusterId);
}

bool MeshConnection::GapDisconnectionHandler(u8 hciDisconnectReason)
{
	logt("CONN", "disconnection handler");

	BaseConnection::GapDisconnectionHandler(hciDisconnectReason);

	//Check if we are a leaf node, do not try to reconnect, probably out of range
	if(direction == ConnectionDirection::DIRECTION_IN && GS->node->configuration.deviceType == deviceTypes::DEVICE_TYPE_LEAF){
		return true;
	}
	//FIXME: Check if our partner is a leaf node, do not try to reconnect, probably out of range
	//FIXME: We need to know the devicetype of our partner which we should send in the handshake message
//	else if(direction == ConnectionDirection::OUT && partnerDeviceType == deviceTypes::DEVICE_TYPE_LEAF){
//		return true;
//	}

	//We want to reestablish, so we do not want the connection to be killed, we only want this
	//if reestablishing is activated and if the connection was handshaked for some time
	//For testing, we can also reestablish using the forceReestablish variable
	if(
		Config->meshExtendedConnectionTimeoutSec > 0
		&& connectionStateBeforeDisconnection >= ConnectionState::HANDSHAKE_DONE
		&& (
			forceReestablish || (
				hciDisconnectReason != BLE_HCI_LOCAL_HOST_TERMINATED_CONNECTION
				&& hciDisconnectReason != BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION
				//&& hciDisconnectReason == BLE_HCI_MEMORY_CAPACITY_EXCEEDED //Used for forcing reconnection with simulator commands
				&& GS->appTimerDs - connectionHandshakedTimestampDs > SEC_TO_DS(30)
			)
		)

	){
		logt("CONN", "Trying reconnect");

		GS->logger->logError(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_TRYING_CONNECTION_SUSTAIN, partnerId);

		connectionState = ConnectionState::REESTABLISHING;
		disconnectedTimestampDs = GS->appTimerDs;

		if(direction == ConnectionDirection::DIRECTION_OUT){
			//Try to connect to peripheral again, if two connections are establishing at the same time, we
			//will get an error, but we do not care, one will be dropped in that case
			//TODO: Retry connecting if possible to alow multi-reestablishing
			TryReestablishing();
		} else {
			//Actiavate discovery if not already activated so that we can be found
			//GS->node->ChangeState(discoveryState::DISCOVERY_HIGH);

			//TODO: We might want to use directed advertising for a faster reconnection?
		}


		return false;
	}
	//We are okay with the MeshConnection being dropped
	else
	{
		// => CM will kill the connection for us
		return true;
	}
}

void MeshConnection::TryReestablishing()
{
	//Reset the disconnectedTimestamp once we try to reestablish to give multiple connections the chance to reestablish one after the other
	disconnectedTimestampDs = GS->appTimerDs;

	u32 err = GS->gapController->connectToPeripheral(partnerAddress, Config->meshMinConnectionInterval, Config->meshExtendedConnectionTimeoutSec);

	logt("CONN", "reconnstatus %u", err);
}

void MeshConnection::ReconnectionSuccessfulHandler(ble_evt_t& bleEvent)
{
	BaseConnection::ReconnectionSuccessfulHandler(bleEvent);

	GS->logger->logError(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_CONNECTION_SUSTAIN_SUCCESS, partnerId);

	//Set to reestablishing handshake
	connectionState = ConnectionState::REESTABLISHING_HANDSHAKE;
	handshakeStartedDs = GS->appTimerDs;

	//Reset all send queues so that the packets are being sent again
	ResendAllPackets(packetSendQueue);
	ResendAllPackets(packetSendQueueHighPrio);

	//Also reset our reassembly buffer
	packetReassemblyPosition = 0;
}

#define __________________SENDING_________________

//This method queues a packet no matter if the connection is currently in handshaking or not
bool MeshConnection::SendHandshakeMessage(u8* data, u8 dataLength, bool reliable)
{
	BaseConnectionSendData sendData;
	sendData.characteristicHandle = partnerWriteCharacteristicHandle;
	sendData.dataLength = dataLength;
	sendData.deliveryOption = reliable ? DeliveryOption::WRITE_REQ : DeliveryOption::WRITE_CMD;
	sendData.priority = DeliveryPriority::HIGH;

//	//TODO: This is a test if WRITE_CMD will provide the same guarantees as WRITE_REQ
//	sendData.deliveryOption = DeliveryOption::WRITE_CMD;

	if(isConnected()){
		QueueData(sendData, data);

		return true;
	} else {
		return false;
	}
}

//This is a small wrapper for the SendData method
bool MeshConnection::SendData(u8* data, u8 dataLength, DeliveryPriority priority, bool reliable)
{
	BaseConnectionSendData sendData;
	sendData.characteristicHandle = partnerWriteCharacteristicHandle;
	sendData.dataLength = dataLength;
	sendData.deliveryOption = reliable ? DeliveryOption::WRITE_REQ : DeliveryOption::WRITE_CMD;
	sendData.priority = priority;

	//TODO: Currently we only support WRITE_CMD to protect against SoftDevice faults
	sendData.deliveryOption = DeliveryOption::WRITE_CMD;

	return SendData(&sendData, data);
}

//This is the generic method for sending data
bool MeshConnection::SendData(BaseConnectionSendData* sendData, u8* data)
{
	if(!handshakeDone()) return false; //Do not allow data being sent when Handshake has not finished yet

	//Print packet as hex
	connPacketHeader* packetHeader = (connPacketHeader*)data;
	char stringBuffer[450];
	GS->logger->convertBufferToHexString(data, sendData->dataLength, stringBuffer, sizeof(stringBuffer));

	//Mesh connections only support write cmd and req, no notifications,...
	if(sendData->deliveryOption != DeliveryOption::WRITE_CMD
		&& sendData->deliveryOption != DeliveryOption::WRITE_REQ){

		sendData->deliveryOption = DeliveryOption::WRITE_CMD;
	}

	//TODO: Currently we only support WRITE_CMD to protect against SoftDevice faults
	sendData->deliveryOption = DeliveryOption::WRITE_CMD;

	logt("CONN_DATA", "PUT_PACKET(%d):len:%d,type:%d,prio:%u,hex:%s",
			connectionId, sendData->dataLength, packetHeader->messageType, sendData->priority, stringBuffer);

	//Put packet in the queue for sending
	return QueueData(*sendData, data);
}

//Allows a Subclass to send Custom Data before the writeQueue is processed
//should return true if something was sent
bool MeshConnection::TransmitHighPrioData()
{
	if(
		handshakeDone() //Handshake must be finished
		&& currentClusterInfoUpdatePacket.header.messageType != 0 //A cluster update packet must be waiting
	){
		//If a clusterUpdate is available we send it immediately
		u8* data = (u8*)&(currentClusterInfoUpdatePacket);
		u8 dataLength = SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE;

		if(unreliableBuffersFree > 0){
			u8 t = ((connPacketHeader*)data)->messageType;

			if( t != 16 && t != 17 && t != 20 && t != 21 && t != 22 && t != 23 && t != 24 && t != 30 && t != 31 && t != 32 && t != 50 && t != 51 && t != 52 && t != 53 && t != 54 && t != 56 && t != 57 && t != 60 && t != 61 && t != 62 && t != 80 && t != 81 && t != 82 && t != 83){
				logt("ERROR", "POSSIBLE WRONG DATA TRANSMITTED!");

				GS->logger->logCount(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_TX_WRONG_DATA);
			}

			//Use this to queue the clusterUpdate in the high prio queue
			BaseConnectionSendData sendData;
			sendData.characteristicHandle = partnerWriteCharacteristicHandle;
			sendData.dataLength = SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE;
			sendData.deliveryOption = DeliveryOption::WRITE_CMD;
			sendData.priority = DeliveryPriority::HIGH;

			//Set the counter for the packet
			currentClusterInfoUpdatePacket.payload.counter = ++clusterUpdateCounter;

			bool queued = QueueData(sendData, data, false);

			if (queued) {
				logt("CONN", "Queued CLUSTER UPDATE for CONN hnd %u", connectionHandle);

				//The current cluster info update message has been sent, we can now clear the packet
				//Because we filled it in the buffer
				memset((u8*)&currentClusterInfoUpdatePacket, 0x00, sizeof(currentClusterInfoUpdatePacket));
			}
			else {
				SIMSTATCOUNT("highPrioQueueFull");
				logt("ERROR", "Could not queue CLUSTER_UPDATE");

				GS->logger->logCount(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_HIGH_PRIO_QUEUE_FULL);

				//We must reset our current counter as it was not used
				clusterUpdateCounter--;
			}
		}
	}

	return false;
}

//This function might modify the packet, can also split bigger packets
sizedData MeshConnection::ProcessDataBeforeTransmission(BaseConnectionSendData* sendData, u8* data, u8* packetBuffer)
{
	//Update packet timestamp as close as possible before sending it
	//TODO: This could be done more accurate because we receive an event when the
	//Packet was sent, so we could calculate the time between sending and getting the event
	//And send a second packet with the time difference.
	//Currently, half of the connection interval is added to make up for this
	if(((connPacketHeader*) data)->messageType == MESSAGE_TYPE_UPDATE_TIMESTAMP){
		logt("NODE", "sending time");

		UpdateGlobalTime();
		((connPacketUpdateTimestamp*) data)->timestampSec = GS->globalTimeSec;
		((connPacketUpdateTimestamp*) data)->remainderTicks = GS->globalTimeRemainderTicks + (currentConnectionIntervalMs * APP_TIMER_CLOCK_FREQ / 1000 / 2);

		sizedData resultData;
		resultData.data = data;
		resultData.length = sendData->dataLength;
		return resultData;
	}

	//Use the split packet from the BaseConnection to process all packets
	return GetSplitData(*sendData, data, packetBuffer);
}

void MeshConnection::PacketSuccessfullyQueuedWithSoftdevice(PacketQueue* queue, BaseConnectionSendDataPacked* sendDataPacked, u8* data, sizedData* sentData)
{
	connPacketHeader* splitPacketHeader = (connPacketHeader*) sentData->data;
	//If this was an intermediate split packet
	if(splitPacketHeader->messageType == MESSAGE_TYPE_SPLIT_WRITE_CMD){
		packetSendQueue.packetSendPosition++;
		packetSendQueue.packetSentRemaining++;

	//If this was a normal packet or the end of a split packet
	} else {
		packetSendQueue.packetSendPosition = 0;

		//Save a queue handle for that packet
		HandlePacketQueued(queue, sendDataPacked);

		//Check if this was the end of a handshake, if yes, mark handshake as completed
		if (((connPacketHeader*)sentData->data)->messageType == MESSAGE_TYPE_CLUSTER_ACK_2)
		{
			//Notify Node of handshakeDone
			GS->node->HandshakeDoneHandler((MeshConnection*)this, true);
		}		
	}
}

#define __________________RECEIVING_________________

void MeshConnection::ReceiveDataHandler(BaseConnectionSendData* sendData, u8* data)
{
	//Only accept packets to our mesh write handle, TODO: could disconnect if other data is received
	if(
		connectionState == ConnectionState::DISCONNECTED
		|| sendData->characteristicHandle != GS->node->meshService.sendMessageCharacteristicHandle.value_handle
	){
		return;
	}

	connPacketHeader* packetHeader = (connPacketHeader*)data;

	char stringBuffer[200];
	GS->logger->convertBufferToHexString(data, sendData->dataLength, stringBuffer, sizeof(stringBuffer));
	logt("CONN_DATA", "Mesh RX %d,length:%d,deliv:%d,data:%s", packetHeader->messageType, sendData->dataLength, sendData->deliveryOption, stringBuffer);

	//This will reassemble the data for us
	data = ReassembleData(sendData, data);

	if(data != nullptr){
		//Route the packet to our other mesh connections
		GS->cm->RouteMeshData(this, sendData, data);

		//Call our handler that dispatches the message throughout our application
		ReceiveMeshMessageHandler(sendData, data);
	}
}

void MeshConnection::ReceiveMeshMessageHandler(BaseConnectionSendData* sendData, u8* data)
{
	connPacketHeader* packetHeader = (connPacketHeader*) data;

	//Some special handling for timestamp updates
	if(packetHeader->messageType == MESSAGE_TYPE_UPDATE_TIMESTAMP)
	{
		//Set our time to the received timestamp
		GS->globalTimeSec = ((connPacketUpdateTimestamp*)data)->timestampSec;
		GS->globalTimeRemainderTicks = ((connPacketUpdateTimestamp*)data)->remainderTicks;

		logt("NODE", "time updated with timestamp:%u", (u32)GS->globalTimeSec);
	}

	//Some logging
	u8 t = packetHeader->messageType;
	if( t != 16 && t != 17 && t != 20 && t != 21 && t != 22 && t != 23 && t != 24 && t != 30 && t != 31 && t != 32 && t != 50 && t != 51 && t != 52 && t != 53 && t != 54 && t != 56 && t != 57 && t != 60 && t != 61 && t != 62 && t != 80 && t != 81 && t != 82 && t != 83){
		logt("ERROR", "POSSIBLE WRONG DATA RECEIVED!");

		GS->logger->logCount(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_RX_WRONG_DATA);
	}
	//Print packet as hex
	{
		char stringBuffer[450];
		GS->logger->convertBufferToHexString(data, sendData->dataLength, stringBuffer, sizeof(stringBuffer));
		logt("CONN_DATA", "Received type %d,length:%d,deliv:%d,data:%s", packetHeader->messageType, sendData->dataLength, sendData->deliveryOption, stringBuffer);
	}

	if(!handshakeDone() || connectionState == ConnectionState::REESTABLISHING_HANDSHAKE){
		ReceiveHandshakePacketHandler(sendData, data);
	} else {
		//Dispatch message to node and modules
		GS->cm->DispatchMeshMessage(this, sendData, (connPacketHeader*) data, true);
	}
}


#define _________________HANDSHAKE_______________________

void MeshConnection::StartHandshake()
{
	//Save a snapshot of the current clustering values, these are used in the handshake
	//Changes to these values are only sent after the handshake has finished and the handshake
	//must not use values that are saved in the node because these might have changed in the meantime
	clusterIDBackup = GS->node->clusterId;
	clusterSizeBackup = GS->node->clusterSize;
	hopsToSinkBackup = GS->cm->GetMeshHopsToShortestSink(this);
	memset(&currentClusterInfoUpdatePacket, 0x00, sizeof(currentClusterInfoUpdatePacket));

	if (connectionState >= ConnectionState::HANDSHAKING)
	{
		logt("HANDSHAKE", "Handshake for connId:%d is already finished or in progress", connectionId);
		return;
	}


	logt("HANDSHAKE", "############ Handshake starting ###############");

	connectionState = ConnectionState::HANDSHAKING;
	handshakeStartedDs = GS->appTimerDs; //Refresh handshake start time

	//After the Handles have been discovered, we start the Handshake
	connPacketClusterWelcome packet;
	packet.header.messageType = MESSAGE_TYPE_CLUSTER_WELCOME;
	packet.header.sender = GS->node->configuration.nodeId;
	packet.header.receiver = NODE_ID_HOPS_BASE + 1; //Node id is unknown, but this allows us to send the packet only 1 hop

	packet.payload.clusterId = clusterIDBackup;
	packet.payload.clusterSize = clusterSizeBackup;
	packet.payload.meshWriteHandle = GS->node->meshService.sendMessageCharacteristicHandle.value_handle; //Our own write handle

	//Now we set the hop counter to the closest sink
	//If we are sink ourself, we set it to 1, otherwise we use our
	//shortest path to reach a sink and increment it by one.
	//If there is no known sink, we set it to 0.
	packet.payload.hopsToSink = hopsToSinkBackup;

	packet.payload.preferredConnectionInterval = 0; //Unused at the moment
	packet.payload.networkId = GS->node->configuration.networkId;

	logt("HANDSHAKE", "OUT => conn(%u) CLUSTER_WELCOME, cID:%x, cSize:%d", connectionId, packet.payload.clusterId, packet.payload.clusterSize);

	SendHandshakeMessage((u8*) &packet, SIZEOF_CONN_PACKET_CLUSTER_WELCOME_WITH_NETWORK_ID, true);
}

void MeshConnection::ReceiveHandshakePacketHandler(BaseConnectionSendData* sendData, u8* data)
{
	connPacketHeader* packetHeader = (connPacketHeader*) data;

	LiveReportHandshakeFailCode handshakeFailCode = LiveReportHandshakeFailCode::SUCCESS;

	/*#################### RECONNETING_HANDSHAKE ############################*/
	if(packetHeader->messageType == MESSAGE_TYPE_RECONNECT)
	{
		ReceiveReconnectionHandshakePacket((connPacketReconnect*) data);
	}

	/*#################### HANDSHAKE ############################*/
	/******* Cluster welcome *******/
	if (packetHeader->messageType == MESSAGE_TYPE_CLUSTER_WELCOME)
	{
		if (sendData->dataLength >= SIZEOF_CONN_PACKET_CLUSTER_WELCOME)
		{
			//Now, compare that packet with our data and see if he should join our cluster
			connPacketClusterWelcome* packet = (connPacketClusterWelcome*) data;

			//Save mesh write handle
			partnerWriteCharacteristicHandle = packet->payload.meshWriteHandle;

			connectionState = ConnectionState::HANDSHAKING;

			//Save a snapshot of the current clustering values, these are used in the handshake
			//Changes to these values are only sent after the handshake has finished and the handshake
			//must not use values that are saved in the node because these might have changed in the meantime
			clusterIDBackup = GS->node->clusterId;
			clusterSizeBackup = GS->node->clusterSize;
			hopsToSinkBackup = GS->cm->GetMeshHopsToShortestSink(this);
			memset(&currentClusterInfoUpdatePacket, 0x00, sizeof(currentClusterInfoUpdatePacket));

			logt("HANDSHAKE", "############ Handshake starting ###############");


			logt("HANDSHAKE", "IN <= %d CLUSTER_WELCOME clustID:%x, clustSize:%d, toSink:%d", packet->header.sender, packet->payload.clusterId, packet->payload.clusterSize, packet->payload.hopsToSink);

			//PART 1: We do have the same cluster ID. Ouuups, should not have happened, run Forest!
			if (packet->payload.clusterId == clusterIDBackup)
			{
				logt("HANDSHAKE", "CONN %u disconnected because it had the same clusterId before handshake", connectionId);
				this->appDisconnectionReason = AppDisconnectReason::SAME_CLUSTERID;
				this->DisconnectAndRemove();

				handshakeFailCode = LiveReportHandshakeFailCode::SAME_CLUSTERID;
			}
			//PART 2: This is more probable, he's in a different cluster
			else if (packet->payload.clusterSize < clusterSizeBackup)
			{
				//I am the bigger cluster
				logt("HANDSHAKE", "I am bigger % vs %u", packet->payload.clusterSize, clusterSizeBackup);

				if(direction == ConnectionDirection::DIRECTION_IN){
					logt("HANDSHAKE", "############ Handshake restarting ###############");
					//Reset connectionState because StartHandshake will fail otherwise
					connectionState = ConnectionState::CONNECTED;
					StartHandshake();
					return;
				}

			}
			//Later version of the packet also has the networkId included
			else if (sendData->dataLength >= SIZEOF_CONN_PACKET_CLUSTER_WELCOME_WITH_NETWORK_ID
				&& packet->payload.networkId != GS->node->configuration.networkId)
			{
				//I am the bigger cluster
				logt("HANDSHAKE", "NetworkId Mismatch");
				this->appDisconnectionReason = AppDisconnectReason::NETWORK_ID_MISMATCH;
				this->DisconnectAndRemove();

				handshakeFailCode = LiveReportHandshakeFailCode::NETWORK_ID_MISMATCH;

			}
			else
			{

				//I am the smaller cluster
				logt("HANDSHAKE", "I am smaller, disconnect other connections");

				//Because we forcefully killed our connections, we are back at square 1
				//These values will be overwritten by the ACK2 packet that we receive from out partner
				//But if we do never receive an ACK2, this is our new starting point
				GS->node->clusterSize = 1;
				GS->node->clusterId = GS->node->GenerateClusterID();

				//Update my own information on the connection
				this->partnerId = packet->header.sender;

				//Send an update to the connected cluster to increase the size by one
				//This is also the ACK message for our connecting node
				connPacketClusterAck1 packet;

				packet.header.messageType = MESSAGE_TYPE_CLUSTER_ACK_1;
				packet.header.sender = GS->node->configuration.nodeId;
				packet.header.receiver = this->partnerId;

				packet.payload.hopsToSink = -1;

				logt("HANDSHAKE", "OUT => %d CLUSTER_ACK_1, hops:%d", packet.header.receiver, packet.payload.hopsToSink);

				SendHandshakeMessage((u8*) &packet, SIZEOF_CONN_PACKET_CLUSTER_ACK_1, true);

				//Kill other Connections
				//FIXME: what does the disconnect function do? it should just clear these connections!!!
				GS->cm->ForceDisconnectOtherMeshConnections(this, AppDisconnectReason::I_AM_SMALLER);

			}
		}
		else
		{
			logt("CONN", "wrong size for CLUSTER_WELCOME");
		}
		/******* Cluster ack 1 (another node confirms that it is joining our cluster, we are bigger) *******/
	}
	else if (packetHeader->messageType == MESSAGE_TYPE_CLUSTER_ACK_1)
	{
		if (sendData->dataLength >= SIZEOF_CONN_PACKET_CLUSTER_ACK_1)
		{
			//Check if the other node does weird stuff
			if(clusterAck1Packet.header.messageType != 0 || GS->cm->GetConnectionInHandshakeState() != this){
				//TODO: disconnect? check this in sim
				logt("ERROR", "HANDSHAKE ERROR ACK1 duplicate %u, %u", clusterAck1Packet.header.messageType, GS->node->currentDiscoveryState);
			}

			//Save ACK1 packet for later
			memcpy(&clusterAck1Packet, data, sizeof(connPacketClusterAck1));

			logt("HANDSHAKE", "IN <= %d  CLUSTER_ACK_1, hops:%d", clusterAck1Packet.header.sender, clusterAck1Packet.payload.hopsToSink);


			//Set the master bit for the connection. If the connection would disconnect
			//Then we could keep intact and the other one must dissolve
			this->partnerId = clusterAck1Packet.header.sender;
			this->connectionMasterBit = 1;

			//Confirm to the new node that it just joined our cluster => send ACK2
			connPacketClusterAck2 outPacket2;
			outPacket2.header.messageType = MESSAGE_TYPE_CLUSTER_ACK_2;
			outPacket2.header.sender = GS->node->configuration.nodeId;
			outPacket2.header.receiver = this->partnerId;

			outPacket2.payload.clusterId = clusterIDBackup;
			outPacket2.payload.clusterSize = clusterSizeBackup + 1; // add +1 for the new node itself
			outPacket2.payload.hopsToSink = hopsToSinkBackup;


			logt("HANDSHAKE", "OUT => %d CLUSTER_ACK_2 clustId:%x, clustSize:%d", this->partnerId, outPacket2.payload.clusterId, outPacket2.payload.clusterSize);

			SendHandshakeMessage((u8*) &outPacket2, SIZEOF_CONN_PACKET_CLUSTER_ACK_2, true);

			//Handshake done connection state ist set in fillTransmitbuffers when the packet is queued


		}
		else
		{
			logt("CONN", "wrong size for ACK1");
		}

		/******* Cluster ack 2 *******/
	}
	else if (packetHeader->messageType == MESSAGE_TYPE_CLUSTER_ACK_2)
	{
		if (sendData->dataLength >= SIZEOF_CONN_PACKET_CLUSTER_ACK_2)
		{
			if(clusterAck2Packet.header.messageType != 0 || GS->cm->GetConnectionInHandshakeState() != this){
				//TODO: disconnect
				logt("ERROR", "HANDSHAKE ERROR ACK2 duplicate %u, %u", clusterAck2Packet.header.messageType, GS->node->currentDiscoveryState);
			}

			//Save Ack2 packet for later
			memcpy(&clusterAck2Packet, data, sizeof(connPacketClusterAck2));

			logt("HANDSHAKE", "IN <= %d CLUSTER_ACK_2 clusterID:%x, clusterSize:%d", clusterAck2Packet.header.sender, clusterAck2Packet.payload.clusterId, clusterAck2Packet.payload.clusterSize);

			//Notify Node of handshakeDone
			GS->node->HandshakeDoneHandler(this, false);


		}
		else
		{
			logt("CONN", "wrong size for ACK2");
		}
	}

	StatusReporterModule* statusMod = (StatusReporterModule*)GS->node->GetModuleById(moduleID::STATUS_REPORTER_MODULE_ID);
	if(statusMod != nullptr && handshakeFailCode != LiveReportHandshakeFailCode::SUCCESS){
		statusMod->SendLiveReport(LiveReportTypes::HANDSHAKE_FAIL, partnerId, (u32)handshakeFailCode);
	}
}

void MeshConnection::SendReconnectionHandshakePacket()
{
	//Can not be done using the send queue because there might be data packets in these queues
	//So instead, we queue the data directly in the softdevice. We can assume that this succeeds most of the time, otherwise reconneciton fails

	logt("HANDSHAKE", "OUT => conn(%u) RECONNECT", connectionId);

	connPacketReconnect packet;
	packet.header.messageType = MESSAGE_TYPE_RECONNECT;
	packet.header.sender = GS->node->configuration.nodeId;
	packet.header.receiver = partnerId;

	u32 err = GS->gattController->bleWriteCharacteristic(
		connectionHandle,
		partnerWriteCharacteristicHandle,
		(u8*)&packet,
		SIZEOF_CONN_PACKET_RECONNECT,
		false);


	logt("HANDSHAKE", "writing to connHnd %u, partnerWriteHnd %u, err %u",connectionHandle, partnerWriteCharacteristicHandle, err);

	//We must account for buffers ourself if we do not use the queue
	if (err == NRF_SUCCESS) {
		manualPacketsSent++;
		unreliableBuffersFree--;
	}

}

void MeshConnection::ReceiveReconnectionHandshakePacket(connPacketReconnect* packet)
{
	if(
		packet->header.sender == partnerId
		&& connectionState == ConnectionState::REESTABLISHING_HANDSHAKE
	){
		connectionState = ConnectionState::HANDSHAKE_DONE;
		disconnectedTimestampDs = 0;


		//TODO: do we need to
	}
}

#define _________________OTHER_______________________

bool MeshConnection::GetPendingPackets() {
	//Adds 1 if a clusterUpdatePacket must be send
	return packetSendQueue._numElements + packetSendQueueHighPrio._numElements + (currentClusterInfoUpdatePacket.header.messageType == 0 ? 0 : 1);
};

void MeshConnection::PrintStatus()
{
	const char* directionString = (direction == ConnectionDirection::DIRECTION_IN) ? "IN " : "OUT";

	trace("%s(%d) FM %u, state:%u, cluster:%x(%d), sink:%d, Queue:%u-%u(%u), Buf:%u/%u, mb:%u, hnd:%u" EOL, directionString, connectionId, this->partnerId, this->connectionState, this->connectedClusterId, this->connectedClusterSize, this->hopsToSink, (packetSendQueue.readPointer - packetSendQueue.bufferStart), (packetSendQueue.writePointer - packetSendQueue.bufferStart), packetSendQueue._numElements, reliableBuffersFree, unreliableBuffersFree, connectionMasterBit, connectionHandle);
}
