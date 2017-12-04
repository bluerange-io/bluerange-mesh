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

#include <ConnectionManager.h>
#include <MeshConnection.h>
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

	if(direction == ConnectionDirection::CONNECTION_DIRECTION_IN){
		GS->cm->freeMeshInConnections--;
	} else if (direction == ConnectionDirection::CONNECTION_DIRECTION_OUT){
		GS->cm->freeMeshOutConnections--;
	}

	GS->node->MeshConnectionConnectedHandler();
}

MeshConnection::~MeshConnection(){
	logt("CONN", "Deleted MeshConnection");

	if (direction == ConnectionDirection::CONNECTION_DIRECTION_IN) {
		GS->cm->freeMeshInConnections++;
	}
	else if (direction == ConnectionDirection::CONNECTION_DIRECTION_OUT) {
		GS->cm->freeMeshOutConnections++;
	}
}

BaseConnection* MeshConnection::ConnTypeResolver(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8* data)
{
	logt("ERROR", "MeshConnResolver");

	//Check if the message was written to our mesh characteristic
	if(sendData->characteristicHandle == GS->node->meshService.sendMessageCharacteristicHandle.value_handle)
	{
		//Check if we already have an inConnection
		MeshConnections conn = GS->cm->GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_IN);
		if(conn.count > Config->meshMaxInConnections){
			logt("CM", "Too many mesh in connections");
			u32 err = sd_ble_gap_disconnect(oldConnection->connectionHandle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
			return NULL;
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

	return NULL;
}


//void MeshConnection::DiscoverCharacteristicHandles()
//{
//	GATTController::getInstance()->bleDiscoverHandles(connectionHandle, &GS->node->meshService.serviceUuid);
//}
//
////When the mesh handle has been discovered
//void MeshConnection::GATTHandleDiscoveredHandler(u16 characteristicHandle)
//{
//	partnerWriteCharacteristicHandle = characteristicHandle;
//
//	StartHandshake();
//}

void MeshConnection::Disconnect()
{
	BaseConnection::Disconnect();

	GS->node->connectionLossCounter++;
}


#define __________________HANDLERS_________________

void MeshConnection::DisconnectionHandler()
{
	BaseConnection::DisconnectionHandler();
}


#define __________________SENDING_________________

//This method queues a packet no matter if the connection is currently in handshaking or not
bool MeshConnection::SendHandshakeMessage(u8* data, u8 dataLength, bool reliable)
{
	BaseConnectionSendData sendData;
	sendData.characteristicHandle = partnerWriteCharacteristicHandle;
	sendData.dataLength = dataLength;
	sendData.deliveryOption = reliable ? DeliveryOption::DELIVERY_OPTION_WRITE_REQ : DeliveryOption::DELIVERY_OPTION_WRITE_CMD;
	sendData.priority = DeliveryPriority::DELIVERY_PRIORITY_HIGH;

//	//TODO: This is a test if WRITE_CMD will provide the same guarantees as WRITE_REQ
//	sendData.deliveryOption = DeliveryOption::DELIVERY_OPTION_WRITE_CMD;

	if(isConnected()){
		QueueData(&sendData, data);

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
	sendData.deliveryOption = reliable ? DeliveryOption::DELIVERY_OPTION_WRITE_REQ : DeliveryOption::DELIVERY_OPTION_WRITE_CMD;
	sendData.priority = priority;

	//TODO: This is a test if WRITE_CMD will provide the same guarantees as WRITE_REQ
	sendData.deliveryOption = DeliveryOption::DELIVERY_OPTION_WRITE_CMD;

	return SendData(&sendData, data);
}

//This is the generic method for sending data
bool MeshConnection::SendData(BaseConnectionSendData* sendData, u8* data)
{
	if(!handshakeDone()) return false; //Do not allow data being sent when Handshake has not finished yet

	//Print packet as hex
	connPacketHeader* packetHeader = (connPacketHeader*)data;
	char stringBuffer[400];
	Logger::getInstance()->convertBufferToHexString(data, sendData->dataLength, stringBuffer, 400);

	logt("CONN_DATA", "PUT_PACKET(%d):len:%d,type:%d,prio:%u,hex:%s",
			connectionId, sendData->dataLength, packetHeader->messageType, sendData->priority, stringBuffer);

	//Put packet in the queue for sending
	return QueueData(sendData, data);
}

//Allows a Subclass to send Custom Data before the writeQueue is processed
//should return true if something was sent
bool MeshConnection::TransmitHighPrioData()
{
	u32 err;

	if(
		handshakeDone() //Handshake must be finished
		&& currentClusterInfoUpdatePacket.header.messageType != 0 //A cluster update packet must be waiting
	){
		//If a clusterUpdate is available we send it immediately
		u8* data = (u8*)&(currentClusterInfoUpdatePacket);
		u8 dataLength = SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE;

		logt("CONN_DATA", "Filling CLUSTER UPDATE for CONN %u", connectionId);

		if(unreliableBuffersFree > 0){
			err = GATTController::getInstance()->bleWriteCharacteristic(
				connectionHandle, partnerWriteCharacteristicHandle, data, SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE, false
			);

			if(err == NRF_SUCCESS){
				sentUnreliable++;
				unreliableBuffersFree--;
				//The current cluster info update message has been sent, we can now clear the packet
				//Because we filled it in the buffer
				memset((u8*)&currentClusterInfoUpdatePacket, 0x00, sizeof(currentClusterInfoUpdatePacket));

				return true;
			} else {
				logt("ERROR", "GATT WRITE ERROR %u", err);
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

		GS->node->UpdateGlobalTime();
		((connPacketUpdateTimestamp*) data)->timestampSec = GS->node->globalTimeSec;
		((connPacketUpdateTimestamp*) data)->remainderTicks = GS->node->globalTimeRemainderTicks + (currentConnectionIntervalMs * APP_TIMER_CLOCK_FREQ / 1000 / 2);

		sizedData resultData;
		resultData.data = data;
		resultData.length = sendData->dataLength;
		return resultData;
	}

	//Use the split packet from the BaseConnection to process all packets
	return GetSplitData(sendData, data, packetBuffer);
}

void MeshConnection::PacketSuccessfullyQueuedWithSoftdevice(BaseConnectionSendData* sendData, u8* data, sizedData* sentData)
{
	connPacketHeader* splitPacketHeader = (connPacketHeader*) sentData->data;
	//If this was an intermediate split packet
	if(splitPacketHeader->messageType == MESSAGE_TYPE_SPLIT_WRITE_CMD){
		packetSendPosition++;

	//If this was a normal packet or the end of a split packet
	} else {
		packetSendPosition = 0;

		//Check if this was the end of a handshake, if yes, mark handshake as completed
		if (((connPacketHeader*)data)->messageType == MESSAGE_TYPE_CLUSTER_ACK_2)
		{
			//Notify Node of handshakeDone
			GS->node->HandshakeDoneHandler((MeshConnection*)this, true);
		}

		//Discard the last packet because it was now successfully sent
		packetSendQueue->DiscardNext();
	}
}

#define __________________RECEIVING_________________

void MeshConnection::ReceiveDataHandler(BaseConnectionSendData* sendData, u8* data)
{
	//Only accept packets to our mesh write handle, TODO: could disconnect if other data is received
	if(
		connectionState == ConnectionState::CONNECTION_STATE_DISCONNECTED
		|| sendData->characteristicHandle != GS->node->meshService.sendMessageCharacteristicHandle.value_handle
	){
		return;
	}

	connPacketHeader* packetHeader = (connPacketHeader*)data;

	char stringBuffer[200];
	Logger::getInstance()->convertBufferToHexString(data, sendData->dataLength, stringBuffer, 200);
	logt("CONN_DATA", "Mesh RX %d,length:%d,deliv:%d,data:%s", packetHeader->messageType, sendData->dataLength, sendData->deliveryOption, stringBuffer);

	//This will reassemble the data for us
	data = ReassembleData(sendData, data);

	if(data != NULL){
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
		GS->node->globalTimeSec = ((connPacketUpdateTimestamp*)data)->timestampSec;
		GS->node->globalTimeRemainderTicks = ((connPacketUpdateTimestamp*)data)->remainderTicks;

		logt("NODE", "time updated with timestamp:%u", (u32)GS->node->globalTimeSec);
	}

	//Some logging
	u8 t = packetHeader->messageType;
	if( t != 16 && t != 17 && t != 20 && t != 21 && t != 22 && t != 23 && t != 30 && t != 31 && t != 50 && t != 51 && t != 52 && t != 53 && t != 56 && t != 57 && t != 60 && t != 61 && t != 62 && t != 80 && t != 81 && t != 82 && t != 83){
		logt("ERROR", "WAAAAAAAAAAAAAHHHHH, WRONG DATAAAAAAAAAAAAAAAAA!!!!!!!!!");
	}
	//Print packet as hex
	char stringBuffer[100];
	Logger::getInstance()->convertBufferToHexString(data, sendData->dataLength, stringBuffer, 100);
	logt("CONN_DATA", "Received type %d,length:%d,deliv:%d,data:%s", packetHeader->messageType, sendData->dataLength, sendData->deliveryOption, stringBuffer);

	if(!handshakeDone()){
		ReceiveHandshakePacketHandler(sendData, data);
	} else {
		if(
			packetHeader->receiver == GS->node->persistentConfig.nodeId //Directly addressed at us
			|| packetHeader->receiver == NODE_ID_BROADCAST //broadcast packet for all nodes
			|| (packetHeader->receiver >= NODE_ID_HOPS_BASE && packetHeader->receiver < NODE_ID_HOPS_BASE + 1000) //Broadcasted for a number of hops
			|| (packetHeader->receiver == NODE_ID_SHORTEST_SINK && GS->node->persistentConfig.deviceType == deviceTypes::DEVICE_TYPE_SINK)
		){
			//Dispatch message to node and modules
			DispatchMeshMessage(sendData, (connPacketHeader*) data);
		}
	}
}

//Dispatches message to node and modules
void MeshConnection::DispatchMeshMessage(BaseConnectionSendData* sendData, connPacketHeader* packet)
{
	//Send message to the node
	GS->node->MeshMessageReceivedHandler(this, sendData, packet);

	//Now we must pass the message to all of our modules for further processing
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(GS->node->activeModules[i] != 0){
			GS->node->activeModules[i]->MeshMessageReceivedHandler(this, sendData, packet);
		}
	}
}


void MeshConnection::StartHandshake()
{
	//Save a snapshot of the current clustering values, these are used in the handshake
	//Changes to these values are only sent after the handshake has finished and the handshake
	//must not use values that are saved in the node because these might have changed in the meantime
	clusterIDBackup = GS->node->clusterId;
	clusterSizeBackup = GS->node->clusterSize;
	hopsToSinkBackup = GS->cm->GetMeshHopsToShortestSink(this);

	if (connectionState >= ConnectionState::CONNECTION_STATE_HANDSHAKING)
	{
		logt("HANDSHAKE", "Handshake for connId:%d is already finished or in progress", connectionId);
		return;
	}


	logt("HANDSHAKE", "############ Handshake starting ###############");

	connectionState = ConnectionState::CONNECTION_STATE_HANDSHAKING;

	//After the Handles have been discovered, we start the Handshake
	connPacketClusterWelcome packet;
	packet.header.messageType = MESSAGE_TYPE_CLUSTER_WELCOME;
	packet.header.sender = GS->node->persistentConfig.nodeId;
	packet.header.receiver = NODE_ID_HOPS_BASE + 1; //Node id is unknown, but this allows us to send the packet only 1 hop

	packet.payload.clusterId = clusterIDBackup;
	packet.payload.clusterSize = clusterSizeBackup;
	packet.payload.meshWriteHandle = GS->node->meshService.sendMessageCharacteristicHandle.value_handle; //Our own write handle


	//Now we set the hop counter to the closest sink
	//If we are sink ourself, we set it to 1, otherwise we use our
	//shortest path to reach a sink and increment it by one.
	//If there is no known sink, we set it to 0.
	packet.payload.hopsToSink = hopsToSinkBackup;


	logt("HANDSHAKE", "OUT => conn(%u) CLUSTER_WELCOME, cID:%x, cSize:%d", connectionId, packet.payload.clusterId, packet.payload.clusterSize);

	SendHandshakeMessage((u8*) &packet, SIZEOF_CONN_PACKET_CLUSTER_WELCOME, true);
}

void MeshConnection::ReceiveHandshakePacketHandler(BaseConnectionSendData* sendData, u8* data)
{
	connPacketHeader* packetHeader = (connPacketHeader*) data;

	/*#################### RECONNETING_HANDSHAKE ############################*/
	if(packetHeader->messageType == MESSAGE_TYPE_RECONNECT)
	{
		if(sendData->dataLength == SIZEOF_CONN_PACKET_RECONNECT){
			//TODO: implement reconnect Handshake
		}
	}

	/*#################### HANDSHAKE ############################*/
	/******* Cluster welcome *******/
	if (packetHeader->messageType == MESSAGE_TYPE_CLUSTER_WELCOME)
	{
		if (sendData->dataLength == SIZEOF_CONN_PACKET_CLUSTER_WELCOME)
		{
			//Now, compare that packet with our data and see if he should join our cluster
			connPacketClusterWelcome* packet = (connPacketClusterWelcome*) data;

			//Save mesh write handle
			partnerWriteCharacteristicHandle = packet->payload.meshWriteHandle;

			connectionState = ConnectionState::CONNECTION_STATE_HANDSHAKING;

			logt("HANDSHAKE", "############ Handshake starting ###############");


			logt("HANDSHAKE", "IN <= %d CLUSTER_WELCOME clustID:%x, clustSize:%d, toSink:%d", packet->header.sender, packet->payload.clusterId, packet->payload.clusterSize, packet->payload.hopsToSink);

			//PART 1: We do have the same cluster ID. Ouuups, should not have happened, run Forest!
			if (packet->payload.clusterId == clusterIDBackup)
			{
				logt("HANDSHAKE", "CONN %u disconnected because it had the same clusterId before handshake", connectionId);
				this->Disconnect();
			}
			//PART 2: This is more probable, he's in a different cluster
			else if (packet->payload.clusterSize < clusterSizeBackup || (packet->payload.clusterSize == clusterSizeBackup && packet->payload.clusterId < clusterIDBackup))
			{
				//I am the bigger cluster
				logt("HANDSHAKE", "I am bigger");

				if(direction == CONNECTION_DIRECTION_IN){
					logt("HANDSHAKE", "############ Handshake restarting ###############");
					//Reset connectionState because StartHandshake will fail otherwise
					connectionState = ConnectionState::CONNECTION_STATE_CONNECTED;
					StartHandshake();
					return;
				}

			}
			else
			{

				//I am the smaller cluster
				logt("HANDSHAKE", "I am smaller, disconnect other connections");

				//Kill other Connections
				//FIXME: what does the disconnect function do? it should just clear these connections!!!
				GS->cm->ForceDisconnectOtherMeshConnections(this);

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
				packet.header.sender = GS->node->persistentConfig.nodeId;
				packet.header.receiver = this->partnerId;

				packet.payload.hopsToSink = -1;

				logt("HANDSHAKE", "OUT => %d CLUSTER_ACK_1, hops:%d", packet.header.receiver, packet.payload.hopsToSink);

				SendHandshakeMessage((u8*) &packet, SIZEOF_CONN_PACKET_CLUSTER_ACK_1, true);

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
		if (sendData->dataLength == SIZEOF_CONN_PACKET_CLUSTER_ACK_1)
		{
			//Check if the other node does weird stuff
			if(clusterAck1Packet.header.messageType != 0 || GS->node->currentDiscoveryState != discoveryState::HANDSHAKE){
				//TODO: disconnect
				logt("ERROR", "HANDSHAKE ERROR ACK1 duplicate %u, %u", clusterAck2Packet.header.messageType, GS->node->currentDiscoveryState);
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
			outPacket2.header.sender = GS->node->persistentConfig.nodeId;
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
		if (sendData->dataLength == SIZEOF_CONN_PACKET_CLUSTER_ACK_2)
		{
			if(clusterAck2Packet.header.messageType != 0 || GS->node->currentDiscoveryState != discoveryState::HANDSHAKE){
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
}

bool MeshConnection::GetPendingPackets() {
	//Adds 1 if a clusterUpdatePacket must be send
	return packetSendQueue->_numElements + (currentClusterInfoUpdatePacket.header.messageType == 0 ? 0 : 1);
};

void MeshConnection::PrintStatus()
{
	const char* directionString = (direction == CONNECTION_DIRECTION_IN) ? "IN " : "OUT";

	trace("%s(%d) FM %u, state:%u, cluster:%x(%d), sink:%d, Queue:%u-%u(%u), Buf:%u/%u, mb:%u, hnd:%u" EOL, directionString, rssiAverage, this->partnerId, this->connectionState, this->connectedClusterId, this->connectedClusterSize, this->hopsToSink, (packetSendQueue->readPointer - packetSendQueue->bufferStart), (packetSendQueue->writePointer - packetSendQueue->bufferStart), packetSendQueue->_numElements, reliableBuffersFree, unreliableBuffersFree, connectionMasterBit, connectionHandle);
}
