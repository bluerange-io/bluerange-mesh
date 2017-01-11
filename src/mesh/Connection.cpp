/**

Copyright (c) 2014-2015 "M-Way Solutions GmbH"
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
#include <Connection.h>
#include <GATTController.h>
#include <GAPController.h>
#include <ConnectionManager.h>
#include <Logger.h>

extern "C"{
#include <ble_hci.h>
}


//The Connection Class does have methods like Connect,... but connections, service
//discovery or encryption are handeled by the Connectionmanager so that we can control
//The parallel flow of multiple connections


Connection::Connection(u8 id, ConnectionManager* cm, Node* node, ConnectionDirection direction)
{
	this->cm = cm;
	this->connectionId = id;
	this->node = node;
	this->direction = direction;
	this->packetSendQueue = new PacketQueue(packetSendBuffer, PACKET_SEND_BUFFER_SIZE);

	ResetValues();
}

void Connection::ResetValues(){
	connectedClusterId = 0;
	unreliableBuffersFree = 0;
	reliableBuffersFree = 1;
	partnerId = 0;
	connectionHandle = BLE_CONN_HANDLE_INVALID;

	connectedClusterSize = 0;
	packetReassemblyPosition = 0;
	packetSendPosition = 0;
	currentConnectionIntervalMs = 0;

	disconnectedTimestampDs = 0;
	reestablishTimeSec = Config->meshExtendedConnectionTimeoutSec;

	connectionState = ConnectionState::DISCONNECTED;
	encryptionState = EncryptionState::NOT_ENCRYPTED;

	connectionMasterBit = 0;

	memset(&clusterAck1Packet, 0x00, sizeof(connPacketClusterAck1));
	memset(&clusterAck2Packet, 0x00, sizeof(connPacketClusterAck2));

	handshakeStartedDs = 0;
	writeCharacteristicHandle = BLE_GATT_HANDLE_INVALID;

	rssiSamplesNum = 0;
	rssiSamplesSum = 0;
	rssiAverage = 0;

	clusterIDBackup = 0;
	clusterSizeBackup = 0;
	hopsToSinkBackup = -1;

	connectionStateBeforeDisconnection = ConnectionState::DISCONNECTED;
	disconnectionReason = BLE_HCI_STATUS_CODE_SUCCESS;

	hopsToSink = -1;

	droppedPackets = 0;
	sentReliable = 0;
	sentUnreliable = 0;

	memset(&currentClusterInfoUpdatePacket, 0x00, sizeof(currentClusterInfoUpdatePacket));
	memset(&lastSentPacket, 0x00, MAX_DATA_SIZE_PER_WRITE);

	this->packetSendQueue->Clean();
}

//Once a connection has been connected in the connection manager, these parameters
//Are passed to the connect function
bool Connection::PrepareConnection(ble_gap_addr_t* address, u16 writeCharacteristicHandle)
{
	ResetValues();

	memcpy(&partnerAddress, address, sizeof(ble_gap_addr_t));
	this->writeCharacteristicHandle = writeCharacteristicHandle;

	this->connectionState = ConnectionState::CONNECTING;

	//Save a snapshot of the current clustering values, these are used in the handshake
	//Changes to these values are only sent after the handshake has finished and the handshake
	//must not use values that are saved in the node because these might have changed in the meantime
	clusterIDBackup = node->clusterId;
	clusterSizeBackup = node->clusterSize;
	hopsToSinkBackup = cm->GetHopsToShortestSink(this);

	return true;
}


/*######## PUBLIC FUNCTIONS ###################################*/

void Connection::DiscoverCharacteristicHandles(void)
{
	GATTController::bleDiscoverHandles(connectionHandle);
}

void Connection::Disconnect()
{
	//Save connection state before disconnection
	connectionStateBeforeDisconnection = connectionState;
	connectionState = ConnectionState::DISCONNECTED;
	//FIXME: This method should be able to disconnect an active connection and disconnect a connection that is in the CONNECTING state

	if(connectionStateBeforeDisconnection == ConnectionState::REESTABLISHING){
		//A disconnect on a reestablishing connection will kill it
		cm->FinalDisconnectionHandler(this);
	} else {
		GAPController::disconnectFromPartner(connectionHandle);
	}
}


/*######## PRIVATE FUNCTIONS ###################################*/

/*######## HANDLER ###################################*/
void Connection::ConnectionSuccessfulHandler(ble_evt_t* bleEvent)
{
	u32 err = 0;

	this->handshakeStartedDs = node->appTimerDs;

	if (direction == CONNECTION_DIRECTION_IN)
		logt("CONN", "Incoming connection %d connected", connectionId);
	else
		logt("CONN", "Outgoing connection %d connected", connectionId);

	connectionHandle = bleEvent->evt.gap_evt.conn_handle;
	err = sd_ble_tx_packet_count_get(this->connectionHandle, &unreliableBuffersFree);
	if(err != NRF_SUCCESS){
		//BLE_ERROR_INVALID_CONN_HANDLE && NRF_ERROR_INVALID_ADDR can be ignored
	}

	//Save connection interval (min and max are the same values in this event)
	currentConnectionIntervalMs = bleEvent->evt.gap_evt.params.connected.conn_params.min_conn_interval;

	connectionState = ConnectionState::CONNECTED;
}

void Connection::ReconnectionSuccessfulHandler(ble_evt_t* bleEvent){
	logt("CONN", "Reconnection Successful");

	connectionHandle = bleEvent->evt.gap_evt.conn_handle;
	sd_ble_tx_packet_count_get(this->connectionHandle, &unreliableBuffersFree);
	reliableBuffersFree = 1;

	connectionState = ConnectionState::HANDSHAKE_DONE;

	//TODO: do we have to get the tx_packet_count or update any other variables?
}

void Connection::StartHandshake(void)
{
	if (connectionState >= ConnectionState::HANDSHAKING)
	{
		logt("HANDSHAKE", "Handshake for connId:%d is already finished or in progress", connectionId);
		return;
	}


	logt("HANDSHAKE", "############ Handshake starting ###############");

	connectionState = ConnectionState::HANDSHAKING;

	//After the Handles have been discovered, we start the Handshake
	connPacketClusterWelcome packet;
	packet.header.messageType = MESSAGE_TYPE_CLUSTER_WELCOME;
	packet.header.sender = node->persistentConfig.nodeId;
	packet.header.receiver = NODE_ID_HOPS_BASE + 1; //Node id is unknown, but this allows us to send the packet only 1 hop

	packet.payload.clusterId = clusterIDBackup;
	packet.payload.clusterSize = clusterSizeBackup;
	packet.payload.meshWriteHandle = GATTController::getMeshWriteHandle(); //Our own write handle


	//Now we set the hop counter to the closest sink
	//If we are sink ourself, we set it to 1, otherwise we use our
	//shortest path to reach a sink and increment it by one.
	//If there is no known sink, we set it to 0.
	packet.payload.hopsToSink = hopsToSinkBackup;


	logt("HANDSHAKE", "OUT => conn(%u) CLUSTER_WELCOME, cID:%x, cSize:%d", connectionId, packet.payload.clusterId, packet.payload.clusterSize);

	cm->SendHandshakeMessage(this, (u8*) &packet, SIZEOF_CONN_PACKET_CLUSTER_WELCOME, true);

}

#define __________________HANDLER__________________
void Connection::DisconnectionHandler()
{
	//Reason?
	logt("DISCONNECT", "Disconnected %u from connId:%u, HCI:%u %s", partnerId, connectionId, disconnectionReason, Logger::getHciErrorString(disconnectionReason));

	//Save connection state before disconnection
	if(connectionState != ConnectionState::DISCONNECTED) connectionStateBeforeDisconnection = connectionState;
	connectionState = ConnectionState::DISCONNECTED;
}


void Connection::ReceivePacketHandler(connectionPacket* inPacket)
{
	if(connectionState == ConnectionState::DISCONNECTED) return;

	u8* data = inPacket->data;
	u16 dataLength = inPacket->dataLength;
	bool reliable = inPacket->reliable;

	//If it is a packet from the handshake, we keep it, otherwise, we forwared it to the node
	connPacketHeader* packetHeader = (connPacketHeader*) data;

	logt("CONN", "Received packet type %d, len %d", packetHeader->messageType, dataLength);

	/*#################### ROUTING ############################*/

	//We are the last receiver for this packet
	if(
			packetHeader->receiver == node->persistentConfig.nodeId //We are the receiver
			|| packetHeader->receiver == NODE_ID_HOPS_BASE + 1 //The packet was meant to travel only one hop
			|| (packetHeader->receiver == NODE_ID_SHORTEST_SINK && node->persistentConfig.deviceType == deviceTypes::DEVICE_TYPE_SINK) //Packet was meant for the shortest sink and we are a sink
	){
		//No packet forwarding needed here.
	}
	//The packet should continue to the shortest sink
	else if(packetHeader->receiver == NODE_ID_SHORTEST_SINK)
	{
		Connection* connection = cm->GetConnectionToShortestSink(NULL);

		if(connection){
			cm->SendMessage(connection, data, dataLength, reliable);
		}
		//We could send it as a broadcast or we just drop it if we do not know any sink
		else {

		}
	}
	//This could be either a packet to a specific node, group, with some hops left or a broadcast packet
	else
	{
		//Some packets need to be modified before relaying them

		//If the packet should travel a number of hops, we decrement that part
		if(packetHeader->sender > NODE_ID_HOPS_BASE && packetHeader->sender < NODE_ID_HOPS_BASE + 31000){
			packetHeader->sender--;
		}

		//Send to all other connections
		if(packetHeader->messageType != MESSAGE_TYPE_CLUSTER_INFO_UPDATE){
			//Do not forward cluster info update packets, these are handeled by the node
			cm->SendMessageOverConnections(this, data, dataLength, reliable);
		}
	}



	/*#################### RECONNETING_HANDSHAKE ############################*/
	if(packetHeader->messageType == MESSAGE_TYPE_RECONNECT)
	{
		if(dataLength == SIZEOF_CONN_PACKET_RECONNECT){

		}
	}


	/*#################### HANDSHAKE ############################*/

	/******* Cluster welcome *******/
	if (packetHeader->messageType == MESSAGE_TYPE_CLUSTER_WELCOME)
	{
		if (dataLength == SIZEOF_CONN_PACKET_CLUSTER_WELCOME)
		{
			//Now, compare that packet with our data and see if he should join our cluster
			connPacketClusterWelcome* packet = (connPacketClusterWelcome*) data;

			//Save mesh write handle
			writeCharacteristicHandle = packet->payload.meshWriteHandle;

			connectionState = ConnectionState::HANDSHAKING;

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
					StartHandshake();
					return;
				}

			}
			else
			{

				//I am the smaller cluster
				logt("HANDSHAKE", "I am smaller");

				//Kill other Connections
				//FIXME: what does the disconnect function do? it should just clear these connections!!!
				cm->ForceDisconnectOtherConnections(this);

				//Because we forcefully killed our connections, we are back at square 1
				//These values will be overwritten by the ACK2 packet that we receive from out partner
				//But if we do never receive an ACK2, this is our new starting point
				node->clusterSize = 1;
				node->clusterId = node->GenerateClusterID();

				//Update my own information on the connection
				this->partnerId = packet->header.sender;

				//Send an update to the connected cluster to increase the size by one
				//This is also the ACK message for our connecting node
				connPacketClusterAck1 packet;

				packet.header.messageType = MESSAGE_TYPE_CLUSTER_ACK_1;
				packet.header.sender = node->persistentConfig.nodeId;
				packet.header.receiver = this->partnerId;

				packet.payload.hopsToSink = -1;

				logt("HANDSHAKE", "OUT => %d CLUSTER_ACK_1, hops:%d", packet.header.receiver, packet.payload.hopsToSink);

				cm->SendHandshakeMessage(this, (u8*) &packet, SIZEOF_CONN_PACKET_CLUSTER_ACK_1, true);

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
		if (dataLength == SIZEOF_CONN_PACKET_CLUSTER_ACK_1)
		{
			//Check if the other node does weird stuff
			if(clusterAck1Packet.header.messageType != 0 || node->currentDiscoveryState != discoveryState::HANDSHAKE){
				//TODO: disconnect
				logt("ERROR", "HANDSHAKE ERROR ACI1 duplicate");
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
			outPacket2.header.sender = node->persistentConfig.nodeId;
			outPacket2.header.receiver = this->partnerId;

			outPacket2.payload.clusterId = clusterIDBackup;
			outPacket2.payload.clusterSize = clusterSizeBackup + 1; // add +1 for the new node itself
			outPacket2.payload.hopsToSink = hopsToSinkBackup;


			logt("HANDSHAKE", "OUT => %d CLUSTER_ACK_2 clustId:%x, clustSize:%d", this->partnerId, outPacket2.payload.clusterId, outPacket2.payload.clusterSize);

			cm->SendHandshakeMessage(this, (u8*) &outPacket2, SIZEOF_CONN_PACKET_CLUSTER_ACK_2, true);

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
		if (dataLength == SIZEOF_CONN_PACKET_CLUSTER_ACK_2)
		{
			if(clusterAck2Packet.header.messageType != 0 || node->currentDiscoveryState != discoveryState::HANDSHAKE){
				//TODO: disconnect
				logt("ERROR", "HANDSHAKE ERROR ACK2 duplicate");
			}

			//Save Ack2 packet for later
			memcpy(&clusterAck2Packet, data, sizeof(connPacketClusterAck2));

			logt("HANDSHAKE", "IN <= %d CLUSTER_ACK_2 clusterID:%x, clusterSize:%d", clusterAck2Packet.header.sender, clusterAck2Packet.payload.clusterId, clusterAck2Packet.payload.clusterSize);

			//Notify Node of handshakeDone
			node->HandshakeDoneHandler(this, false);


		}
		else
		{
			logt("CONN", "wrong size for ACK2");
		}

	}


	/*#################### MESSAGE PROCESSING ############################*/
	else
	{
		//Check wether we should care for this packet
		if(
				packetHeader->receiver == node->persistentConfig.nodeId //Directly addressed at us
				|| packetHeader->receiver == NODE_ID_BROADCAST //broadcast packet for all nodes
				|| (packetHeader->receiver >= NODE_ID_HOPS_BASE && packetHeader->receiver < NODE_ID_HOPS_BASE + 1000) //Broadcasted for a number of hops
				|| (packetHeader->receiver == NODE_ID_SHORTEST_SINK && node->persistentConfig.deviceType == deviceTypes::DEVICE_TYPE_SINK)
		){
			//Forward that Packet to the Node
			cm->connectionManagerCallback->messageReceivedCallback(inPacket);
		}
	}

}

//Handling general events
void Connection::BleEventHandler(ble_evt_t* bleEvent)
{
	if(connectionState == ConnectionState::DISCONNECTED) return;

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

void Connection::PrintStatus(void)
{
	const char* directionString = (direction == CONNECTION_DIRECTION_IN) ? "< IN " : "> OUT";

	trace("%s %u, state:%u, clId:%x, clSize:%d, toSink:%d, Queue:%u-%u(%u), Buf(rel:%u, unrel:%u), mb:%u, pend:%u" EOL, directionString, this->partnerId, this->connectionState, this->connectedClusterId, this->connectedClusterSize, this->hopsToSink, (packetSendQueue->readPointer - packetSendQueue->bufferStart), (packetSendQueue->writePointer - packetSendQueue->bufferStart), packetSendQueue->_numElements, reliableBuffersFree, unreliableBuffersFree, connectionMasterBit, GetPendingPackets());

}

i8 Connection::GetAverageRSSI()
{
	if(connectionState >= ConnectionState::CONNECTED) return rssiAverage;
	else return 0;
}

//Returns the number of packets that this connection has not yet sent
u16 Connection::GetPendingPackets(){
	return packetSendQueue->_numElements
			+ (currentClusterInfoUpdatePacket.header.messageType == MESSAGE_TYPE_CLUSTER_INFO_UPDATE ? 1 : 0);
}

void Connection::ClusterUpdateSentHandler()
{
	//The current cluster info update message has been sent, we can now clear the packet
	memset((u8*)&currentClusterInfoUpdatePacket, 0x00, sizeof(currentClusterInfoUpdatePacket));
}
/* EOF */

