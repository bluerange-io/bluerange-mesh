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
#include <GlobalState.h>
#include "ConnectionAllocator.h"

#ifndef SIM_ENABLED
uint32_t meshConnTypeResolver __attribute__((section(".ConnTypeResolvers"), used)) = (u32)MeshConnection::ConnTypeResolver;
#endif

MeshConnection::MeshConnection(u8 id, ConnectionDirection direction, fh_ble_gap_addr_t* partnerAddress, u16 partnerWriteCharacteristicHandle)
	: BaseConnection(id, direction, partnerAddress)
{
	logt("CONN", "New MeshConnection");
	//Initialize to defaults
	connectionType = ConnectionType::FRUITYMESH;
	connectedClusterId = 0;
	connectedClusterSize = 0;
	connectionMasterBit = 0;
	CheckedMemset(&clusterAck1Packet, 0x00, sizeof(connPacketClusterAck1));
	CheckedMemset(&clusterAck2Packet, 0x00, sizeof(connPacketClusterAck2));
	clusterIDBackup = 0;
	clusterSizeBackup = 0;
	hopsToSinkBackup = -1;
	hopsToSink = -1;
	ClearCurrentClusterInfoUpdatePacket();

	//Save values from constructor
	this->partnerWriteCharacteristicHandle = partnerWriteCharacteristicHandle;

	if(direction == ConnectionDirection::DIRECTION_IN){
		GS->cm.freeMeshInConnections--;
	} else if (direction == ConnectionDirection::DIRECTION_OUT){
		GS->cm.freeMeshOutConnections--;
	}
}

MeshConnection::~MeshConnection(){
	logt("CONN", "Deleted MeshConnection because %u", (u32)appDisconnectionReason);

	if (direction == ConnectionDirection::DIRECTION_IN) {
		GS->cm.freeMeshInConnections++;
	}
	else if (direction == ConnectionDirection::DIRECTION_OUT) {
		GS->cm.freeMeshOutConnections++;
	}
}

BaseConnection* MeshConnection::ConnTypeResolver(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8* data)
{
	logt("MACONN", "MeshConnResolver");

	//Check if the message was written to our mesh characteristic
	if(sendData->characteristicHandle == GS->node.meshService.sendMessageCharacteristicHandle.value_handle)
	{
		//Check if we already have an inConnection
		MeshConnections conn = GS->cm.GetMeshConnections(ConnectionDirection::DIRECTION_IN);
		if(conn.count >= Conf::meshMaxInConnections){
			logt("CM", "Too many mesh in connections");
			u32 err = FruityHal::Disconnect(oldConnection->connectionHandle, FruityHal::HciErrorCode::REMOTE_USER_TERMINATED_CONNECTION);
			return nullptr;
		}
		else
		{
			MeshConnection* newConnection = ConnectionAllocator::getInstance().allocateMeshConnection(
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


#define __________________CONNECTIVITY_________________

void MeshConnection::DisconnectAndRemove(AppDisconnectReason reason)
{
	//Make a backup of some important variables on the stack
	ConnectionState connectionStateBeforeDisconnection = this->connectionState == ConnectionState::DISCONNECTED ? this->connectionStateBeforeDisconnection : this->connectionState; // Depending on where this call comes from, the connection was already disconnected or we have to do it here
	u8 hadConnectionMasterBit = this->connectionMasterBit;
	ClusterSize connectedClusterSize = this->connectedClusterSize;
	ClusterId connectedClusterId = this->connectedClusterId;
	AppDisconnectReason appDisconnectionReason = this->appDisconnectionReason != AppDisconnectReason::UNKNOWN ? this->appDisconnectionReason : reason;

	logt("CONN", "before remove %u, %u, %u, %u", (u32)connectionStateBeforeDisconnection,
			hadConnectionMasterBit,
			connectedClusterSize,
			connectedClusterId);

	//Call our lovely modules
	for(u32 i=0; i<GS->amountOfModules; i++){
		if(GS->activeModules[i]->configurationPointer->moduleActive){
			GS->activeModules[i]->MeshConnectionChangedHandler(*this);
		}
	}

	if (connectionStateBeforeDisconnection >= ConnectionState::HANDSHAKE_DONE) {
		logjson("SIM", "{\"type\":\"mesh_disconnect\",\"partnerId\":%u}" SEP, partnerId);

		StatusReporterModule* statusMod = (StatusReporterModule*)GS->node.GetModuleById(ModuleId::STATUS_REPORTER_MODULE);
		if (statusMod != nullptr) {
			statusMod->SendLiveReport(LiveReportTypes::HANDSHAKED_MESH_DISCONNECTED, partnerId, (u8)appDisconnectionReason);
		}
	}

	//Will kill the connection. Deletion is at the end of the function.
	//Do not use members after the following line! USE-AFTER-FREE!
	BaseConnection::DisconnectAndRemove(reason);

	//Use our backup variables to tell the node about the lost connection
	//Is safe to call after deletion. All variables were backuped at the start of this function.
	GS->node.MeshConnectionDisconnectedHandler(
		appDisconnectionReason,
		connectionStateBeforeDisconnection,
		hadConnectionMasterBit,
		connectedClusterSize,
		connectedClusterId);
}

bool MeshConnection::GapDisconnectionHandler(const u8 hciDisconnectReason)
{
	logt("CONN", "disconnection handler"); 

	BaseConnection::GapDisconnectionHandler(hciDisconnectReason);

	//Check if we are a leaf node, do not try to reconnect, probably out of range
	if(direction == ConnectionDirection::DIRECTION_IN && GET_DEVICE_TYPE() == DeviceType::LEAF){
		GS->logger.logCustomError(CustomErrorTypes::INFO_IGNORING_CONNECTION_SUSTAIN_LEAF, partnerId);
		return true;
	}
	//FIXME: Check if our partner is a leaf node, do not try to reconnect, probably out of range
	//FIXME: We need to know the devicetype of our partner which we should send in the handshake message
//	else if(direction == ConnectionDirection::OUT && partnerDeviceType == DeviceType::LEAF){
//		return true;
//	}

	u16 err = 0;
	if (Conf::meshExtendedConnectionTimeoutSec == 0)                                               err |= 1 << 0;	//1
	if (connectionStateBeforeDisconnection != ConnectionState::HANDSHAKE_DONE)                     err |= 1 << 1;	//2
	if (hciDisconnectReason == (u8)FruityHal::HciErrorCode::LOCAL_HOST_TERMINATED_CONNECTION)      err |= 1 << 2;	//4
	if (hciDisconnectReason == (u8)FruityHal::HciErrorCode::REMOTE_USER_TERMINATED_CONNECTION)     err |= 1 << 3;	//8
	if (GS->appTimerDs - connectionHandshakedTimestampDs <= SEC_TO_DS(30))                         err |= 1 << 4;	//16

	//We want to reestablish, so we do not want the connection to be killed, we only want this
	//if reestablishing is activated and if the connection was handshaked for some time
	//For testing, we can also reestablish using the forceReestablish variable
	if(err == 0){
		logt("CONN", "Trying reconnect");

		GS->logger.logCustomError(CustomErrorTypes::INFO_TRYING_CONNECTION_SUSTAIN, partnerId);

		connectionState = ConnectionState::REESTABLISHING;
		disconnectedTimestampDs = GS->appTimerDs;

		if(direction == ConnectionDirection::DIRECTION_OUT){
			//Try to connect to peripheral again, if two connections are establishing at the same time, we
			//will get an error, but we do not care, one will be dropped in that case
			//TODO: Retry connecting if possible to alow multi-reestablishing
			TryReestablishing();
		} else {
			//Actiavate discovery if not already activated so that we can be found
			//GS->node.ChangeState(DiscoveryState::HIGH);

			//TODO: We might want to use directed advertising for a faster reconnection?
		}


		return false;
	}
	//We are okay with the MeshConnection being dropped
	else
	{
		GS->logger.logCustomError(CustomErrorTypes::INFO_IGNORING_CONNECTION_SUSTAIN, err);
		// => CM will kill the connection for us
		return true;
	}
}

void MeshConnection::TryReestablishing()
{
	//Reset the disconnectedTimestamp once we try to reestablish to give multiple connections the chance to reestablish one after the other
	disconnectedTimestampDs = GS->appTimerDs;

	u32 err = GS->gapController.connectToPeripheral(partnerAddress, Conf::getInstance().meshMinConnectionInterval, Conf::meshExtendedConnectionTimeoutSec);

	logt("CONN", "reconnstatus %u", err);
}

void MeshConnection::GapReconnectionSuccessfulHandler(const GapConnectedEvent& connectedEvent)
{
	BaseConnection::GapReconnectionSuccessfulHandler(connectedEvent);

	GS->logger.logCustomError(CustomErrorTypes::INFO_CONNECTION_SUSTAIN_SUCCESS, partnerId);

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
bool MeshConnection::SendHandshakeMessage(u8* data, u16 dataLength, bool reliable)
{
	BaseConnectionSendData sendData;
	sendData.characteristicHandle = partnerWriteCharacteristicHandle;
	sendData.dataLength = dataLength;
	sendData.deliveryOption = reliable ? DeliveryOption::WRITE_REQ : DeliveryOption::WRITE_CMD;
	sendData.priority = DeliveryPriority::MESH_INTERNAL_HIGH;

	if(isConnected()){
		QueueData(sendData, data);

		return true;
	} else {
		return false;
	}
}

//This is a small wrapper for the SendData method
bool MeshConnection::SendData(u8* data, u16 dataLength, DeliveryPriority priority, bool reliable)
{
	if (dataLength > MAX_MESH_PACKET_SIZE) {
		SIMEXCEPTION(PaketTooBigException);
		logt("ERROR", "Packet too big for sending!");
		return false;
	}

	BaseConnectionSendData sendData;
	sendData.characteristicHandle = partnerWriteCharacteristicHandle;
	sendData.dataLength = (u8)dataLength;
	sendData.deliveryOption = reliable ? DeliveryOption::WRITE_REQ : DeliveryOption::WRITE_CMD;
	sendData.priority = priority;

	return SendData(&sendData, data);
}

//This is the generic method for sending data
bool MeshConnection::SendData(BaseConnectionSendData* sendData, u8* data)
{
	if(!handshakeDone()) return false; //Do not allow data being sent when Handshake has not finished yet

	//Print packet as hex
	connPacketHeader* packetHeader = (connPacketHeader*)data;
	char stringBuffer[100];
	Logger::convertBufferToHexString(data, sendData->dataLength, stringBuffer, sizeof(stringBuffer));

	//Mesh connections only support write cmd and req, no notifications,...
	if(sendData->deliveryOption != DeliveryOption::WRITE_CMD
		&& sendData->deliveryOption != DeliveryOption::WRITE_REQ){

		sendData->deliveryOption = DeliveryOption::WRITE_CMD;
	}

	//WARNING: Currently we only support WRITE_CMD to protect against SoftDevice faults
	//The SoftDevice will sometimes malfunction when receiving a lot of WRITE_REQ, also, they slow down the
	//sending of packets by a factor of 14, so we only use them for mesh critical functionality such as clustering
	sendData->deliveryOption = DeliveryOption::WRITE_CMD;

	logt("CONN_DATA", "PUT_PACKET(%d):len:%d,type:%d,prio:%u,hex:%s",
			connectionId, sendData->dataLength, (u32)packetHeader->messageType, (u32)sendData->priority, stringBuffer);

	//Put packet in the queue for sending
	return QueueData(*sendData, data);
}

//Allows a Subclass to send Custom Data before the writeQueue is processed
//should return true if something was sent
bool MeshConnection::TransmitHighPrioData()
{
	if(
		handshakeDone() //Handshake must be finished
		&& currentClusterInfoUpdatePacket.header.messageType != MessageType::INVALID //A cluster update packet must be waiting
		&& ( // and it must provide some kind of update
				currentClusterInfoUpdatePacket.payload.clusterSizeChange != 0
				|| currentClusterInfoUpdatePacket.payload.connectionMasterBitHandover != 0
				|| (currentClusterInfoUpdatePacket.payload.hopsToSink != -1 && GET_DEVICE_TYPE() != DeviceType::SINK)
			)
	){
		//If a clusterUpdate is available we send it immediately
		u8* data = (u8*)&(currentClusterInfoUpdatePacket);

		if(unreliableBuffersFree > 0){
			if(!IsValidMessageType(((connPacketHeader*)data)->messageType)){
				logt("ERROR", "POSSIBLE WRONG DATA TRANSMITTED!");

				GS->logger.logCustomError(CustomErrorTypes::WARN_TX_WRONG_DATA, (u32)((connPacketHeader*)data)->messageType);
			}

			//Use this to queue the clusterUpdate in the high prio queue
			BaseConnectionSendData sendData;
			sendData.characteristicHandle = partnerWriteCharacteristicHandle;
			sendData.dataLength = SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE;
			sendData.deliveryOption = DeliveryOption::WRITE_CMD;
			sendData.priority = DeliveryPriority::MESH_INTERNAL_HIGH;

			//Set the counter for the packet
			currentClusterInfoUpdatePacket.payload.counter = ++clusterUpdateCounter;

			bool queued = QueueData(sendData, data, false);

			if (queued) {
				logt("CONN", "Queued CLUSTER UPDATE for CONN hnd %u", connectionHandle);

				//The current cluster info update message has been sent, we can now clear the packet
				//Because we filled it in the buffer
				ClearCurrentClusterInfoUpdatePacket();
			}
			else {
				SIMSTATCOUNT("highPrioQueueFull");
				logt("ERROR", "Could not queue CLUSTER_UPDATE");

				GS->logger.logCustomError(CustomErrorTypes::WARN_HIGH_PRIO_QUEUE_FULL, partnerId);

				//We must reset our current counter as it was not used
				clusterUpdateCounter--;
			}
		}
	}

	return false;
}

void MeshConnection::ClearCurrentClusterInfoUpdatePacket()
{
	CheckedMemset(&currentClusterInfoUpdatePacket, 0x00, sizeof(currentClusterInfoUpdatePacket));
	currentClusterInfoUpdatePacket.header.messageType = MessageType::CLUSTER_INFO_UPDATE;
	currentClusterInfoUpdatePacket.header.sender = GS->node.configuration.nodeId;
	currentClusterInfoUpdatePacket.payload.hopsToSink = GET_DEVICE_TYPE() == DeviceType::SINK ? 0 : -1;
}

//This function might modify the packet, can also split bigger packets
SizedData MeshConnection::ProcessDataBeforeTransmission(BaseConnectionSendData* sendData, u8* data, u8* packetBuffer)
{
	//Use the split packet from the BaseConnection to process all packets
	return GetSplitData(*sendData, data, packetBuffer);
}

void MeshConnection::PacketSuccessfullyQueuedWithSoftdevice(PacketQueue* queue, BaseConnectionSendDataPacked* sendDataPacked, u8* data, SizedData* sentData)
{
	connPacketHeader* splitPacketHeader = (connPacketHeader*) sentData->data;
	//If this was an intermediate split packet
	if (splitPacketHeader->messageType == MessageType::SPLIT_WRITE_CMD) {
		packetSendQueue.packetSendPosition++;
		packetSendQueue.packetSentRemaining++;
	}
	//The end of a split packet
	else if (splitPacketHeader->messageType == MessageType::SPLIT_WRITE_CMD_END) {
		queue->packetSendPosition = 0;
		packetSendQueue.packetSentRemaining++;

		//Save a queue handle for that packet
		HandlePacketQueued(queue, sendDataPacked);
	}
	//If this was a normal packet
	else {
		packetSendQueue.packetSendPosition = 0;

		//Save a queue handle for that packet
		HandlePacketQueued(queue, sendDataPacked);

		//Check if this was the end of a handshake, if yes, mark handshake as completed
		if (((connPacketHeader*)sentData->data)->messageType == MessageType::CLUSTER_ACK_2)
		{
			//Notify Node of handshakeDone
			GS->node.HandshakeDoneHandler((MeshConnection*)this, true);
		}		
	}
}

void MeshConnection::DataSentHandler(const u8 * data, u16 length)
{
	const connPacketHeader* header = (const connPacketHeader*)data;
	if (header->messageType == MessageType::TIME_SYNC)
	{
		const TimeSyncHeader* header = (const TimeSyncHeader*)data;
		if (header->type == TimeSyncType::INITIAL)
		{
			correctionTicks = GS->timeManager.GetTimePoint() - syncSendingOrdered;
		}
	}
}

#define __________________RECEIVING_________________

void MeshConnection::ReceiveDataHandler(BaseConnectionSendData* sendData, u8* data)
{
	//Only accept packets to our mesh write handle, TODO: could disconnect if other data is received
	if(
		connectionState == ConnectionState::DISCONNECTED
		|| sendData->characteristicHandle != GS->node.meshService.sendMessageCharacteristicHandle.value_handle
	){
		return;
	}

	connPacketHeader* packetHeader = (connPacketHeader*)data;

	char stringBuffer[200];
	Logger::convertBufferToHexString(data, sendData->dataLength, stringBuffer, sizeof(stringBuffer));
	logt("CONN_DATA", "Mesh RX %d,length:%d,deliv:%d,data:%s", (u32)packetHeader->messageType, sendData->dataLength, (u32)sendData->deliveryOption, stringBuffer);

	//This will reassemble the data for us
	data = ReassembleData(sendData, data);

	if(data != nullptr){
		//Route the packet to our other mesh connections
		GS->cm.RouteMeshData(this, sendData, data);

		//Call our handler that dispatches the message throughout our application
		ReceiveMeshMessageHandler(sendData, data);
	}
}

void MeshConnection::ReceiveMeshMessageHandler(BaseConnectionSendData* sendData, u8* data)
{
	connPacketHeader* packetHeader = (connPacketHeader*) data;

	//Some special handling for timestamp updates
	if(packetHeader->messageType == MessageType::UPDATE_TIMESTAMP)
	{
		//Set our time to the received timestamp
		connPacketUpdateTimestamp* packet = (connPacketUpdateTimestamp*)data;
		if (sendData->dataLength >= offsetof(connPacketUpdateTimestamp, offset) + sizeof(packet->offset))
		{
			GS->timeManager.SetTime(packet->timestampSec, 0, packet->offset);
		}
		else
		{
			GS->timeManager.SetTime(packet->timestampSec, 0, 0);
		}

		logt("NODE", "time updated with timestamp:%u", GS->timeManager.GetTime());
	}

	//Some logging
	if(!IsValidMessageType(packetHeader->messageType)){
		logt("ERROR", "POSSIBLE WRONG DATA RECEIVED!");

		GS->logger.logCustomError(CustomErrorTypes::WARN_RX_WRONG_DATA, (u32)packetHeader->messageType);
	}
	//Print packet as hex
	{
		char stringBuffer[100];
		Logger::convertBufferToHexString(data, sendData->dataLength, stringBuffer, sizeof(stringBuffer));
		logt("CONN_DATA", "Received type %d,length:%d,deliv:%d,data:%s", (u32)packetHeader->messageType, sendData->dataLength, (u32)sendData->deliveryOption, stringBuffer);
	}

	if(!handshakeDone() || connectionState == ConnectionState::REESTABLISHING_HANDSHAKE){
		ReceiveHandshakePacketHandler(sendData, data);
	} else {
		//Dispatch message to node and modules
		GS->cm.DispatchMeshMessage(this, sendData, (connPacketHeader*) data, true);
	}
}


#define _________________HANDSHAKE_______________________

void MeshConnection::StartHandshake()
{
	//Save a snapshot of the current clustering values, these are used in the handshake
	//Changes to these values are only sent after the handshake has finished and the handshake
	//must not use values that are saved in the node because these might have changed in the meantime
	clusterIDBackup = GS->node.clusterId;
	clusterSizeBackup = GS->node.clusterSize;
	hopsToSinkBackup = GS->cm.GetMeshHopsToShortestSink(this);
	
	ClearCurrentClusterInfoUpdatePacket();

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
	packet.header.messageType = MessageType::CLUSTER_WELCOME;
	packet.header.sender = GS->node.configuration.nodeId;
	packet.header.receiver = NODE_ID_HOPS_BASE + 1; //Node id is unknown, but this allows us to send the packet only 1 hop

	packet.payload.clusterId = clusterIDBackup;
	packet.payload.clusterSize = clusterSizeBackup;
	packet.payload.meshWriteHandle = GS->node.meshService.sendMessageCharacteristicHandle.value_handle; //Our own write handle

	//Now we set the hop counter to the closest sink
	packet.payload.hopsToSink = GS->cm.GetMeshHopsToShortestSink(this);

	packet.payload.preferredConnectionInterval = 0; //Unused at the moment
	packet.payload.networkId = GS->node.configuration.networkId;

	logt("HANDSHAKE", "OUT => conn(%u) CLUSTER_WELCOME, cID:%x, cSize:%d, hops:%d", connectionId, packet.payload.clusterId, packet.payload.clusterSize, packet.payload.hopsToSink);

	SendHandshakeMessage((u8*) &packet, SIZEOF_CONN_PACKET_CLUSTER_WELCOME_WITH_NETWORK_ID, true);
}

void MeshConnection::ReceiveHandshakePacketHandler(BaseConnectionSendData* sendData, u8* data)
{
	NodeId tempPartnerId = partnerId; //Temp storage in case we delete this.
	connPacketHeader* packetHeader = (connPacketHeader*) data;

	LiveReportHandshakeFailCode handshakeFailCode = LiveReportHandshakeFailCode::SUCCESS;

	/*#################### RECONNETING_HANDSHAKE ############################*/
	if(packetHeader->messageType == MessageType::RECONNECT)
	{
		ReceiveReconnectionHandshakePacket((connPacketReconnect*) data);
	}

	/*#################### HANDSHAKE ############################*/
	/******* Cluster welcome *******/
	else if (packetHeader->messageType == MessageType::CLUSTER_WELCOME)
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
			clusterIDBackup = GS->node.clusterId;
			clusterSizeBackup = GS->node.clusterSize;
			hopsToSinkBackup = GS->cm.GetMeshHopsToShortestSink(this);
			
			ClearCurrentClusterInfoUpdatePacket();

			logt("HANDSHAKE", "############ Handshake starting ###############");


			logt("HANDSHAKE", "IN <= %d CLUSTER_WELCOME clustID:%x, clustSize:%d, toSink:%d", packet->header.sender, packet->payload.clusterId, packet->payload.clusterSize, packet->payload.hopsToSink);

			//PART 1: We do have the same cluster ID. Ouuups, should not have happened, run Forest!
			if (packet->payload.clusterId == clusterIDBackup)
			{
				logt("HANDSHAKE", "CONN %u disconnected because it had the same clusterId before handshake", connectionId);
				this->DisconnectAndRemove(AppDisconnectReason::SAME_CLUSTERID);

				handshakeFailCode = LiveReportHandshakeFailCode::SAME_CLUSTERID;
			}
			//PART 2: This is more probable, he's in a different cluster
			else if (packet->payload.clusterSize < clusterSizeBackup)
			{
				//I am the bigger cluster
				logt("HANDSHAKE", "I am bigger %d vs %d", packet->payload.clusterSize, clusterSizeBackup);

				if(direction == ConnectionDirection::DIRECTION_IN){
					logt("HANDSHAKE", "############ Handshake stopped ###############");
					//We should have connected using an OUT connection, not an IN connection, disconnect
					DisconnectAndRemove(AppDisconnectReason::WRONG_DIRECTION);
					
					handshakeFailCode = LiveReportHandshakeFailCode::WRONG_DIRECTION;
				}

			}
			//Later version of the packet also has the networkId included
			else if (sendData->dataLength >= SIZEOF_CONN_PACKET_CLUSTER_WELCOME_WITH_NETWORK_ID
				&& packet->payload.networkId != GS->node.configuration.networkId)
			{
				//I am the bigger cluster
				logt("HANDSHAKE", "NetworkId Mismatch");
				this->DisconnectAndRemove(AppDisconnectReason::NETWORK_ID_MISMATCH);

				handshakeFailCode = LiveReportHandshakeFailCode::NETWORK_ID_MISMATCH;

			}
			else if (!GS->node.IsPreferredConnection(packet->header.sender) && GS->config.configuration.preferredConnectionMode == PreferredConnectionMode::IGNORED)
			{
				logt("HANDSHAKE", "Unpreferred connection tried to connect. %u", (u32)(packet->header.sender));
				this->DisconnectAndRemove(AppDisconnectReason::UNPREFERRED_CONNECTION);

				handshakeFailCode = LiveReportHandshakeFailCode::UNPREFERRED_CONNECTION;
			}
			else
			{

				//I am the smaller cluster
				logt("HANDSHAKE", "I am smaller, disconnect other connections");

				//Update my own information on the connection
				this->partnerId = packet->header.sender;

				//Send an update to the connected cluster to increase the size by one
				//This is also the ACK message for our connecting node
				connPacketClusterAck1 packet;

				packet.header.messageType = MessageType::CLUSTER_ACK_1;
				packet.header.sender = GS->node.configuration.nodeId;
				packet.header.receiver = this->partnerId;

				packet.payload.hopsToSink = GET_DEVICE_TYPE() == DeviceType::SINK ? 0 : -1;

				logt("HANDSHAKE", "OUT => %d CLUSTER_ACK_1, hops:%d", packet.header.receiver, packet.payload.hopsToSink);

				SendHandshakeMessage((u8*) &packet, SIZEOF_CONN_PACKET_CLUSTER_ACK_1, true);
				
				//Kill other Connections and check if this connection has been removed in the process
				GS->cm.ForceDisconnectOtherMeshConnections(this, AppDisconnectReason::I_AM_SMALLER);

				//Because we forcefully killed our connections, we are back at square 1
				//These values will be overwritten by the ACK2 packet that we receive from out partner
				//But if we do never receive an ACK2, this is our new starting point
				//Setting the size to 1 is a safety precaution
				GS->node.clusterSize = 1;
				GS->node.clusterId = GS->node.GenerateClusterID();
			}
		}
		else
		{
			logt("CONN", "wrong size for CLUSTER_WELCOME");
		}
		/******* Cluster ack 1 (another node confirms that it is joining our cluster, we are bigger) *******/
	}
	else if (packetHeader->messageType == MessageType::CLUSTER_ACK_1)
	{
		if (sendData->dataLength >= SIZEOF_CONN_PACKET_CLUSTER_ACK_1)
		{
			//Check if the other node does weird stuff
			if(clusterAck1Packet.header.messageType != MessageType::INVALID || GS->cm.GetConnectionInHandshakeState() != this){
				//TODO: disconnect? check this in sim
				logt("ERROR", "HANDSHAKE ERROR ACK1 duplicate %u, %u", (u32)clusterAck1Packet.header.messageType, (u32)GS->node.currentDiscoveryState);

				GS->logger.logCustomCount(CustomErrorTypes::COUNT_HANDSHAKE_ACK1_DUPLICATE);
			}

			//Save ACK1 packet for later
			memcpy(&clusterAck1Packet, data, sizeof(connPacketClusterAck1));

			logt("HANDSHAKE", "IN <= %d  CLUSTER_ACK_1, hops:%d", clusterAck1Packet.header.sender, clusterAck1Packet.payload.hopsToSink);


			//Set the master bit for the connection. If the connection would disconnect
			//Then we could keep intact and the other one must dissolve
			this->partnerId = clusterAck1Packet.header.sender;
			this->connectionMasterBit = 1;
			this->hopsToSink = clusterAck1Packet.payload.hopsToSink;
			logt("ERROR", "NODE %u CREATED MASTERBIT", GS->node.configuration.nodeId);

			//Confirm to the new node that it just joined our cluster => send ACK2
			connPacketClusterAck2 outPacket2;
			outPacket2.header.messageType = MessageType::CLUSTER_ACK_2;
			outPacket2.header.sender = GS->node.configuration.nodeId;
			outPacket2.header.receiver = this->partnerId;

			outPacket2.payload.clusterId = clusterIDBackup;
			outPacket2.payload.clusterSize = clusterSizeBackup + 1; // add +1 for the new node itself
			outPacket2.payload.hopsToSink = GS->cm.GetMeshHopsToShortestSink(this);


			logt("HANDSHAKE", "OUT => %d CLUSTER_ACK_2 clustId:%x, clustSize:%d, hops:%d", this->partnerId, outPacket2.payload.clusterId, outPacket2.payload.clusterSize, outPacket2.payload.hopsToSink);

			SendHandshakeMessage((u8*) &outPacket2, SIZEOF_CONN_PACKET_CLUSTER_ACK_2, true);

			//Handshake done connection state ist set in fillTransmitbuffers when the packet is queued


		}
		else
		{
			logt("CONN", "wrong size for ACK1");
		}

		/******* Cluster ack 2 *******/
	}
	else if (packetHeader->messageType == MessageType::CLUSTER_ACK_2)
	{
		if (sendData->dataLength >= SIZEOF_CONN_PACKET_CLUSTER_ACK_2)
		{
			if(clusterAck2Packet.header.messageType != MessageType::INVALID || GS->cm.GetConnectionInHandshakeState() != this){
				//TODO: disconnect
				logt("ERROR", "HANDSHAKE ERROR ACK2 duplicate %u, %u", (u32)clusterAck2Packet.header.messageType, (u32)GS->node.currentDiscoveryState);
				GS->logger.logCustomCount(CustomErrorTypes::COUNT_HANDSHAKE_ACK2_DUPLICATE);
			}

			//Save Ack2 packet for later
			memcpy(&clusterAck2Packet, data, sizeof(connPacketClusterAck2));

			logt("HANDSHAKE", "IN <= %d CLUSTER_ACK_2 clusterID:%x, clusterSize:%d", clusterAck2Packet.header.sender, clusterAck2Packet.payload.clusterId, clusterAck2Packet.payload.clusterSize);

			//Notify Node of handshakeDone
			GS->node.HandshakeDoneHandler(this, false);


		}
		else
		{
			logt("CONN", "wrong size for ACK2");
		}
	}
	else
	{
		SIMEXCEPTION(IllegalStateException);
		logt("WARNING", "Received non-handshake packet in handshake. MessageType %u, I am %u, partner is %u", (u8)packetHeader->messageType, (u32)GS->node.configuration.nodeId, (u32)partnerId);
	}

	StatusReporterModule* statusMod = (StatusReporterModule*)GS->node.GetModuleById(ModuleId::STATUS_REPORTER_MODULE);
	if(statusMod != nullptr && handshakeFailCode != LiveReportHandshakeFailCode::SUCCESS){
		statusMod->SendLiveReport(LiveReportTypes::HANDSHAKE_FAIL, tempPartnerId, (u32)handshakeFailCode);
	}
}

void MeshConnection::SendReconnectionHandshakePacket()
{
	//Can not be done using the send queue because there might be data packets in these queues
	//So instead, we queue the data directly in the softdevice. We can assume that this succeeds most of the time, otherwise reconneciton fails

	logt("HANDSHAKE", "OUT => conn(%u) RECONNECT", connectionId);

	connPacketReconnect packet;
	packet.header.messageType = MessageType::RECONNECT;
	packet.header.sender = GS->node.configuration.nodeId;
	packet.header.receiver = partnerId;

	//TODO: Add a check if the reliable buffer is free?

	u32 err = GS->gattController.bleWriteCharacteristic(
		connectionHandle,
		partnerWriteCharacteristicHandle,
		(u8*)&packet,
		SIZEOF_CONN_PACKET_RECONNECT,
		true);


	logt("HANDSHAKE", "writing to connHnd %u, partnerWriteHnd %u, err %u",connectionHandle, partnerWriteCharacteristicHandle, err);

	//We must account for buffers ourself if we do not use the queue
	if (err == FruityHal::SUCCESS) {
		manualPacketsSent++;
		reliableBuffersFree--;
	}
	else {
		this->DisconnectAndRemove(AppDisconnectReason::RECONNECT_BLE_ERROR);
	}

}

void MeshConnection::ReceiveReconnectionHandshakePacket(connPacketReconnect* packet)
{
	logt("HANDSHAKE", "IN <= partner %u RECONNECT", partnerId);
	if(
		packet->header.sender == partnerId
		&& connectionState == ConnectionState::REESTABLISHING_HANDSHAKE
	){
		connectionState = ConnectionState::HANDSHAKE_DONE;
		disconnectedTimestampDs = 0;

	}
}

#define _________________OTHER_______________________

bool MeshConnection::GetPendingPackets() {
	//Adds 1 if a clusterUpdatePacket must be send
	return packetSendQueue._numElements + packetSendQueueHighPrio._numElements + (currentClusterInfoUpdatePacket.header.messageType == MessageType::INVALID ? 0 : 1);
}
bool MeshConnection::IsValidMessageType(MessageType t)
{
	switch (t) {
		case(MessageType::SPLIT_WRITE_CMD):
		case(MessageType::SPLIT_WRITE_CMD_END):
		case(MessageType::CLUSTER_WELCOME):
		case(MessageType::CLUSTER_ACK_1):
		case(MessageType::CLUSTER_ACK_2):
		case(MessageType::CLUSTER_INFO_UPDATE):
		case(MessageType::RECONNECT):
		case(MessageType::UPDATE_TIMESTAMP):
		case(MessageType::UPDATE_CONNECTION_INTERVAL):
		case(MessageType::ASSET_V2):
		case(MessageType::MODULE_CONFIG):
		case(MessageType::MODULE_TRIGGER_ACTION):
		case(MessageType::MODULE_ACTION_RESPONSE):
		case(MessageType::MODULE_GENERAL):
		case(MessageType::MODULE_RAW_DATA):
		case(MessageType::MODULE_RAW_DATA_LIGHT):
		case(MessageType::MODULES_LIST):
		case(MessageType::DATA_1):
		case(MessageType::CLC_DATA):
		case(MessageType::COMPONENT_SENSE):
		case(MessageType::COMPONENT_ACT):
		case(MessageType::TIME_SYNC):
		case(MessageType::CAPABILITY):
			return true;
		default:
			SIMEXCEPTION(MessageTypeInvalidException);
			return false;
	}

}
;

void MeshConnection::PrintStatus()
{
	const char* directionString = (direction == ConnectionDirection::DIRECTION_IN) ? "IN " : "OUT";

	trace("%s(%d) FM %u, state:%u, cluster:%x(%d), sink:%d, Queue:%u-%u(%u), Buf:%u/%u, mb:%u, hnd:%u, tSync:%u" EOL, directionString, connectionId, this->partnerId, (u32)this->connectionState, this->connectedClusterId, this->connectedClusterSize, this->hopsToSink, (packetSendQueue.readPointer - packetSendQueue.bufferStart), (packetSendQueue.writePointer - packetSendQueue.bufferStart), packetSendQueue._numElements, reliableBuffersFree, unreliableBuffersFree, connectionMasterBit, connectionHandle, (u32)timeSyncState);
}

void MeshConnection::setHopsToSink(ClusterSize hops)
{
	hopsToSink = hops;
}
