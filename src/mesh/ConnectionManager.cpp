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

#include <types.h>
#include <Node.h>
#include <ConnectionManager.h>
#include <AdvertisingController.h>
#include <GATTController.h>
#include <GAPController.h>
#include <Utility.h>
#include <Logger.h>

extern "C"{
#include <app_error.h>
#include <app_timer.h>
#include <ble_hci.h>
}


//The flow for any connection is:
// Connected => Encrypted => Mesh handle discovered => Handshake done
//encryption can be disabled and handle discovery can be skipped (handle from JOIN_ME packet will be used)

ConnectionManager* ConnectionManager::instance = NULL;

ConnectionManager::ConnectionManager(){

	this->node = Node::getInstance();
	connectionManagerCallback = NULL;

	//Initialize GAP and GATT
	GAPController::bleConfigureGAP();
	GATTController::bleMeshServiceInit();

	//Config->
	doHandshake = true;

	//init vars
	pendingConnection = NULL;
	freeOutConnections = Config->meshMaxOutConnections;
	freeInConnections = Config->meshMaxInConnections;

	//Create all connections
	inConnection = connections[0] = new Connection(0, this, node, Connection::CONNECTION_DIRECTION_IN);

	for(int i=1; i<=Config->meshMaxOutConnections; i++){
		connections[i] = outConnections[i-1] = new Connection(i, this, node, Connection::CONNECTION_DIRECTION_OUT);
	}

	//Register GAPController callbacks
	GAPController::setDisconnectionHandler(DisconnectionHandler);
	GAPController::setConnectionSuccessfulHandler(ConnectionSuccessfulHandler);
	GAPController::setConnectionEncryptedHandler(ConnectionEncryptedHandler);
	GAPController::setConnectingTimeoutHandler(ConnectingTimeoutHandler);

	//Set GATTController callbacks
	GATTController::setMessageReceivedCallback(messageReceivedCallback);
	GATTController::setHandleDiscoveredCallback(handleDiscoveredCallback);
	GATTController::setDataTransmittedCallback(dataTransmittedCallback);

}

//This method queues a packet no matter if the connection is currently in handshaking or not
bool ConnectionManager::SendHandshakeMessage(Connection* connection, u8* data, u16 dataLength, bool reliable)
{
	if(connection->isConnected()){
		QueuePacket(connection, data, dataLength, reliable);

		fillTransmitBuffers();

		return true;
	} else {
		return false;
	}
}



//Send message to a single connection (Will not send packet if connection has not finished handshake)
bool ConnectionManager::SendMessage(Connection* connection, u8* data, u16 dataLength, bool reliable)
{
	if(connection->handshakeDone()){
		QueuePacket(connection, data, dataLength, reliable);

		fillTransmitBuffers();

		return true;
	} else {
		return false;
	}
}

//Send a message over all connections, except one connection
void ConnectionManager::SendMessageOverConnections(Connection* ignoreConnection, u8* data, u16 dataLength, bool reliable)
{
	for (int i = 0; i < Config->meshMaxConnections; i++)
	{
		if (connections[i] != ignoreConnection && connections[i]->handshakeDone()){
			QueuePacket(connections[i], data, dataLength, reliable);
		}
	}

	fillTransmitBuffers();
}

//Checks the receiver of the message first and routes it in the right direction
void ConnectionManager::SendMessageToReceiver(Connection* originConnection, u8* data, u16 dataLength, bool reliable)
{
	connPacketHeader* packetHeader = (connPacketHeader*) data;

	//This packet was only meant for us, sth. like a packet to localhost
	//Or if we sent this as a broadcast, we want to handle it ourself as well
	if(
			packetHeader->receiver == node->persistentConfig.nodeId
			|| (originConnection == NULL && packetHeader->receiver == NODE_ID_BROADCAST)
	)
	{
		connectionPacket packet;
		packet.connectionHandle = 0; //Not needed
		packet.data = data;
		packet.dataLength = dataLength;
		packet.reliable = reliable;

		//Send directly to our message handler in the node for processing
		node->messageReceivedCallback(&packet);
	}


	//Packets to the shortest sink
	if(packetHeader->receiver == NODE_ID_SHORTEST_SINK)
	{
		Connection* dest = GetConnectionToShortestSink(NULL);

		//Packets are currently only delivered if a sink is known
		if(dest) SendMessage(dest, data, dataLength, reliable);
	}
	//All other packets will be broadcasted, but we could and should check if the receiver is connected to us
	else if(packetHeader->receiver != node->persistentConfig.nodeId)
	{
		SendMessageOverConnections(originConnection, data, dataLength, reliable);
	}
}

void ConnectionManager::QueuePacket(Connection* connection, u8* data, u16 dataLength, bool reliable){
	//Print packet as hex
	char stringBuffer[400];
	Logger::getInstance().convertBufferToHexString(data, dataLength, stringBuffer, 400);

	logt("CONN_DATA", "PUT_PACKET(%d):len:%d,type:%d, hex: %s",connection->connectionId, dataLength, data[0], stringBuffer);

	//Save packet
	bool putResult = connection->packetSendQueue->Put(data, dataLength, reliable);

	if(!putResult) {
		connection->droppedPackets++;
		//TODO: Error handling: What should happen when the queue is full?
		//Currently, additional packets are dropped
		logt("ERROR", "Send queue is already full");
	}
}


Connection* ConnectionManager::getFreeConnection()
{
	for (int i = 0; i < Config->meshMaxConnections; i++)
		{
			if (connections[i]->isDisconnected()) return connections[i];
		}
	return NULL;
}

u32 ConnectionManager::getNumConnections(){
	u32 count = 0;
	for (int i = 0; i < Config->meshMaxConnections; i++)
	{
		if (!connections[i]->isDisconnected()) count++;
	}
	return count;
}

//Looks if there is a free connection available
Connection* ConnectionManager::GetFreeOutConnection()
{
	for (int i = 0; i < Config->meshMaxOutConnections; i++)
	{
		if (outConnections[i]->isDisconnected()) return outConnections[i];
	}
	return NULL;
}

//Looks through all connections for the right handle and then passes the event to that connection
Connection* ConnectionManager::GetConnectionFromHandle(u16 connectionHandle)
{
	if (connectionHandle == inConnection->connectionHandle)
		return inConnection;
	for (int i = 0; i < Config->meshMaxOutConnections; i++)
	{
		if (outConnections[i]->connectionHandle == connectionHandle)
			return outConnections[i];
	}
	return NULL;
}

//Connects to a peripheral as Master, writecharacteristichandle can be BLE_GATT_HANDLE_INVALID
Connection* ConnectionManager::ConnectAsMaster(nodeID partnerId, ble_gap_addr_t* address, u16 writeCharacteristicHandle)
{
	//Only connect when not currently in another connection or when there are no more free connections
	if (freeOutConnections <= 0 || pendingConnection != NULL) return NULL;

	Connection* connection = GetFreeOutConnection();

	//Disperse connection intervals over time, maybe this leads to less connection losses
	u16 connectionInterval = Config->meshMinConnectionInterval + connection->connectionId;

	//Tell the GAP Layer to connect, it will return if it is trying or if there was an error
	bool status = GAPController::connectToPeripheral(address, connectionInterval, Config->meshConnectingScanTimeout);

	if(status){
		//Get a free connection and set it as pending
		pendingConnection = connection;

		//Put address and writeHandle into the connection
		pendingConnection->PrepareConnection(address, writeCharacteristicHandle);

		char addrString[20];
		Logger::getInstance().convertBufferToHexString(pendingConnection->partnerAddress.addr, 6, addrString, 20);

		logt("CONN", "Connect as Master to %d (%s) %s", partnerId, addrString, status ? "true" : "false");
	}

	return pendingConnection;
}

/** Tries to reestablish connections */
int ConnectionManager::ReestablishConnections(){

	//Check if any of the reestablish connections should be disconnected forever
	for(int i=0; i<Config->meshMaxConnections; i++){
		if(
			connections[i]->connectionState == Connection::ConnectionState::REESTABLISHING
			&& node->appTimerDs - connections[i]->disconnectedTimestampDs > SEC_TO_DS(connections[i]->reestablishTimeSec)
		){
			Disconnect(connections[i]->connectionHandle);
			//FIXME: must implement
			logt("ERROR", "REAL DISCONNECT, must implement");
		}
	}

	//First, check which connection needs reestablishing
	//TODO: We could use a whitelist of connection partner is we need to reestablish
	//multiple
	Connection* connection = NULL;
	for(int i=0; i<Config->meshMaxConnections; i++){
		if(connections[i]->connectionState == Connection::ConnectionState::REESTABLISHING){
			connection = connections[i];
			break;
		}
	}



	//Do not reestablish if nothing was found
	if(connection == NULL) return 0;

	//We have been a Peripheral in this connection, start to advertise
	if(connection->direction == Connection::ConnectionDirection::CONNECTION_DIRECTION_IN){
		logt("CM", "Waiting for node %u to reconnect", connection->partnerId);
		//Update packet to make it connectable
		node->UpdateJoinMePacket();

		return 1;

	} else {
		logt("CM", "Should reconnect to node %u, %02x:..:%02x", connection->partnerId, connection->partnerAddress.addr[0], connection->partnerAddress.addr[5]);

		//try to connect with same settings as previous connection as master
		bool result = GAPController::connectToPeripheral(
				&connection->partnerAddress,
				connection->currentConnectionIntervalMs,
				connection->reestablishTimeSec);

		if(result){
			logt("CM", "Waiting for connection to node %u", connection->partnerId);
			//Put this connection in the pending connection
			//not needed, can be compared to address
			//pendingConnection = connection;

			return 2;

		} else {
			logt("CM", "Connection not possible right now");

			return 0;
		}
	}
}


//Disconnects a specific connection
void ConnectionManager::Disconnect(u16 connectionHandle)
{
	Connection* connection = GetConnectionFromHandle(connectionHandle);
	if (connection != NULL)
	{
		connection->Disconnect();
	}
}

//Disconnects either all connections or all except one
//Cluster updates from this connection should be ignored
void ConnectionManager::ForceDisconnectOtherConnections(Connection* connection)
{
	for (int i = 0; i < Config->meshMaxOutConnections+1; i++)
	{
		if (connections[i] != connection && !connections[i]->isDisconnected()){
			connections[i]->Disconnect();
			connections[i]->connectionStateBeforeDisconnection = Connection::ConnectionState::DISCONNECTED;
		}
	}
}

void ConnectionManager::SetConnectionInterval(u16 connectionInterval)
{
	//Go through all connections that we control as a central
	for(int i=0; i < Config->meshMaxOutConnections; i++){
		if(outConnections[i]->handshakeDone()){
			GAPController::RequestConnectionParameterUpdate(outConnections[i]->connectionHandle, connectionInterval, connectionInterval, 0, Config->meshConnectionSupervisionTimeout);
		}
	}
}

u16 ConnectionManager::GetPendingPackets(){
	u16 pendingPackets = 0;
	for(int i=0; i<Config->meshMaxConnections; i++){
		pendingPackets += connections[i]->GetPendingPackets();
	}
	return pendingPackets;
}




#define _________________HANDSHAKE____________





#define _________________STATIC_HANDLERS____________
//STATIC methods for handling events
//
//

void ConnectionManager::BleEventHandler(ble_evt_t* bleEvent)
{
	ConnectionManager* cm = ConnectionManager::getInstance();

	for(int i=0; i<Config->meshMaxConnections; i++){
		cm->connections[i]->BleEventHandler(bleEvent);
	}
}


//Called as soon as a new connection is made, either as central or peripheral
void ConnectionManager::ConnectionSuccessfulHandler(ble_evt_t* bleEvent)
{
	ConnectionManager* cm = ConnectionManager::getInstance();

	logt("CM", "Connection success");

	Connection* connection = cm->IsConnectionReestablishment(bleEvent);

	/* Part A: We have a connection reestablishment */
	if(connection != NULL)
	{
		connection->ReconnectionSuccessfulHandler(bleEvent);
		cm->fillTransmitBuffers();
		cm->node->ChangeState(discoveryState::DISCOVERY);
		return;
	}
	/* Part B: A normal incoming connection */

	//FIXME: There is a slim chance that another central connects to us between
	//This call and before switching off advertising.
	//This connection should be deferred until our handshake is finished
	//Set a variable here that a handshake is ongoing and block any other handshake
	//From happening in the meantime, just disconnect the intruder

	//If we are currently doing a Handshake, we disconnect this connection
	//beacuse we cannot do two handshakes at the same time
	if(cm->node->currentDiscoveryState == discoveryState::HANDSHAKE){
		logt("ERROR", "CURRENTLY IN HANDSHAKE!!!!!!!!!!");

		sd_ble_gap_disconnect(bleEvent->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
	}


	Connection* c = NULL;

	//We are slave (peripheral)
	if (bleEvent->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_PERIPH)
	{
		c = cm->inConnection;
		cm->freeInConnections--;

		//At this point, the Write handle is still unknown
		c->PrepareConnection(&bleEvent->evt.gap_evt.params.connected.peer_addr, BLE_GATT_HANDLE_INVALID);
		c->ConnectionSuccessfulHandler(bleEvent);
		cm->connectionManagerCallback->ConnectionSuccessfulHandler(bleEvent);

		//If encryption is enabled, we wait for the central to start encrypting
		if(Config->encryptionEnabled)
		{

		}
		//If handshake is disabled,
		else if(!cm->doHandshake){
			c->connectionState = Connection::ConnectionState::HANDSHAKE_DONE;
		}
	}
	//We are master (central)
	else if (bleEvent->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_CENTRAL)
	{
		cm->freeOutConnections--;

		c = cm->pendingConnection;

		//Call Prepare again so that the clusterID and size backup are created with up to date values
		c->PrepareConnection(&bleEvent->evt.gap_evt.params.connected.peer_addr, c->writeCharacteristicHandle);
		c->ConnectionSuccessfulHandler(bleEvent);
		cm->connectionManagerCallback->ConnectionSuccessfulHandler(bleEvent);

		//If encryption is enabled, the central starts to encrypt the connection
		if(Config->encryptionEnabled)
		{
			c->encryptionState = Connection::EncryptionState::ENCRYPTING;
			GAPController::startEncryptingConnection(bleEvent->evt.gap_evt.conn_handle);
		}
		//If no encryption is enabled, we start the handshake
		else if(cm->doHandshake)
		{
			c->StartHandshake();
		}
		//If the handshake is disabled, we just set the variable
		else
		{
			c->connectionState = Connection::ConnectionState::HANDSHAKE_DONE;
		}
	}

	cm->pendingConnection = NULL;
}

Connection* ConnectionManager::IsConnectionReestablishment(ble_evt_t* bleEvent)
{
	//Check if we already have a connection for this peer, identified by its address
	ble_gap_addr_t* peerAddress = &bleEvent->evt.gap_evt.params.connected.peer_addr;
	for(int i=0; i<Config->meshMaxConnections; i++){
		if(connections[i]->connectionState == Connection::ConnectionState::REESTABLISHING){
			if(memcmp(&connections[i]->partnerAddress, peerAddress, sizeof(ble_gap_addr_t)) == 0){
				logt("CM", "Found existing connection id %u" , connections[i]->connectionId);
				return connections[i];
			}
		}
	}
	return NULL;
}

//When a connection changes to encrypted
void ConnectionManager::ConnectionEncryptedHandler(ble_evt_t* bleEvent)
{
	ConnectionManager* cm = ConnectionManager::getInstance();
	Connection* c = cm->GetConnectionFromHandle(bleEvent->evt.gap_evt.conn_handle);

	logt("CM", "Connection id %u is now encrypted", c->connectionId);
	c->encryptionState = Connection::EncryptionState::ENCRYPTED;

	//We are peripheral
	if(c->direction == Connection::CONNECTION_DIRECTION_IN)
	{
		if(!cm->doHandshake){
			c->connectionState = Connection::ConnectionState::HANDSHAKE_DONE;
		}
	}
	//We are central
	else if(c->direction == Connection::CONNECTION_DIRECTION_OUT)
	{
		if(cm->doHandshake)
		{
			c->StartHandshake();
		}
		//If the handshake is disabled, we just set the variable
		else
		{
			c->connectionState = Connection::ConnectionState::HANDSHAKE_DONE;
		}
	}
}

//When the mesh handle has been discovered
void ConnectionManager::handleDiscoveredCallback(u16 connectionHandle, u16 characteristicHandle)
{
	ConnectionManager* cm = ConnectionManager::getInstance();

	Connection* connection = cm->GetConnectionFromHandle(connectionHandle);
	if (connection != NULL)
	{
		connection->writeCharacteristicHandle = characteristicHandle;

		if(cm->doHandshake) connection->StartHandshake();
		else {
			cm->connectionManagerCallback->ConnectionSuccessfulHandler(NULL);
		}
	}
}


//Is called whenever a connection had been established and is now disconnected
//due to a timeout, deliberate disconnection by the localhost, remote, etc,...
//We might however decide to sustain it. it will only be lost after
//the finalDisconnectionHander is called
void ConnectionManager::DisconnectionHandler(ble_evt_t* bleEvent)
{

	ConnectionManager* cm = ConnectionManager::getInstance();
	Connection* connection = cm->GetConnectionFromHandle(bleEvent->evt.gap_evt.conn_handle);

	if(connection == NULL) return;

	//Save disconnction reason
	connection->disconnectionReason = bleEvent->evt.gap_evt.params.disconnected.reason;

	//Check if the connection should be sustained
	//(e.g. If it's been handshaked for more than 7 seconds and ran into a timeout)
	if(
			connection->handshakeDone()
			&& cm->node->appTimerDs - connection->connectionHandshakedTimestampDs > SEC_TO_DS(60)
			//&& bleEvent->evt.gap_evt.params.disconnected.reason == BLE_HCI_CONNECTION_TIMEOUT => problematic since there are multiple reasons, including the ominous BLE_GATTC_EVT_TIMEOUT
			&& connection->reestablishTimeSec != 0
	){
		logt("CM", "Connection should be sustained");

		//Log connection suspension
		Logger::getInstance().logError(Logger::errorTypes::CUSTOM, Logger::customErrorTypes::TRYING_CONNECTION_SUSTAIN, connection->partnerId);

		//Mark the connection as reestablishing, the state machine of the node
		//will then try to reestablish it.
		connection->disconnectedTimestampDs = cm->node->appTimerDs;
		connection->connectionState = Connection::ConnectionState::REESTABLISHING;
	}
	//Connection will not be sustained
	else
	{
		cm->FinalDisconnectionHandler(connection);
	}
}

//Is called when a connection is closed after
void ConnectionManager::FinalDisconnectionHandler(Connection* connection)
{
	ConnectionManager* cm = ConnectionManager::getInstance();

	Logger::getInstance().logError(Logger::errorTypes::CUSTOM, Logger::customErrorTypes::FINAL_DISCONNECTION, connection->partnerId);

	//LOG disconnection reason
	Logger::getInstance().logError(Logger::errorTypes::HCI_ERROR, connection->disconnectionReason, connection->partnerId);

	logt("CM", "Connection %u to %u DISCONNECTED", connection->connectionId, connection->partnerId);

	if (connection->direction == Connection::CONNECTION_DIRECTION_IN) cm->freeInConnections++;
	else cm->freeOutConnections++;

	//If this was the pending connection, we clear it
	if(cm->pendingConnection == connection) cm->pendingConnection = NULL;

	//Notify the connection itself
	connection->DisconnectionHandler();

	//Notify the callback of the disconnection (The Node probably)
	cm->connectionManagerCallback->DisconnectionHandler(connection);

	//Reset connection variables
	connection->ResetValues();
}

//Called when a connecting request times out
void ConnectionManager::ConnectingTimeoutHandler(ble_evt_t* bleEvent)
{
	ConnectionManager* cm = ConnectionManager::getInstance();

	if(cm->pendingConnection != NULL){
		cm->pendingConnection->ResetValues();

		cm->pendingConnection = NULL;
	}

	cm->connectionManagerCallback->ConnectingTimeoutHandler(bleEvent);

}

void ConnectionManager::messageReceivedCallback(ble_evt_t* bleEvent)
{
	ConnectionManager* cm = ConnectionManager::getInstance();

	//Handles BLE_GATTS_EVT_WRITE


	//FIXME: must check for reassembly buffer size, if it is bigger, a stack overflow will occur

	Connection* connection = cm->GetConnectionFromHandle(bleEvent->evt.gatts_evt.conn_handle);
	if (connection != NULL)
	{
		//TODO: At this point we should check if the write was a valid operation for the mesh
		//Invalid actions should cause a disconnect
		/*if( bleEvent->evt.gatts_evt.params.write.handle != GATTController::getMeshWriteHandle() ){
			connection->Disconnect();
			logt("ERROR", "Non mesh device was disconnected");
		}*/


		connPacketHeader* packet = (connPacketHeader*)bleEvent->evt.gatts_evt.params.write.data;

		//At first, some special treatment for out timestamp packet
		if(packet->messageType == MESSAGE_TYPE_UPDATE_TIMESTAMP)
		{
			//Set our time to the received timestamp
			cm->node->globalTimeSec = ((connPacketUpdateTimestamp*)packet)->timestampSec;
			cm->node->globalTimeRemainderTicks = ((connPacketUpdateTimestamp*)packet)->remainderTicks;

			logt("NODE", "time updated with timestamp:%u", (u32)cm->node->globalTimeSec);
		}

		u8 t = ((connPacketHeader*)bleEvent->evt.gatts_evt.params.write.data)->messageType;

		if( t != 20 && t != 21 && t != 22 && t != 23 && t != 30 && t != 31 && t != 50 && t != 51 && t != 52 && t != 53 && t != 56 && t != 57 && t != 60 && t != 61 && t != 62 && t != 80 && t != 81){
			logt("ERROR", "WAAAAAAAAAAAAAHHHHH, WRONG DATAAAAAAAAAAAAAAAAA!!!!!!!!!");
		}

		//Print packet as hex
		char stringBuffer[100];
		Logger::getInstance().convertBufferToHexString(bleEvent->evt.gatts_evt.params.write.data, bleEvent->evt.gatts_evt.params.write.len, stringBuffer, 100);
		logt("CONN_DATA", "Received type %d, hasMore %d, length %d, reliable %d:", ((connPacketHeader*)bleEvent->evt.gatts_evt.params.write.data)->messageType, ((connPacketHeader*)bleEvent->evt.gatts_evt.params.write.data)->hasMoreParts, bleEvent->evt.gatts_evt.params.write.len, bleEvent->evt.gatts_evt.params.write.op);
		logt("CONN_DATA", "%s", stringBuffer);

		//Check if we need to reassemble the packet
		if(connection->packetReassemblyPosition == 0 && packet->hasMoreParts == 0)
		{

			//Single packet, no more data
			connectionPacket p;
			p.connectionHandle = bleEvent->evt.gatts_evt.conn_handle;
			p.data = bleEvent->evt.gatts_evt.params.write.data;
			p.dataLength = bleEvent->evt.gatts_evt.params.write.len;
			p.reliable = bleEvent->evt.gatts_evt.params.write.op == BLE_GATTS_OP_WRITE_CMD ? false : true;

			connection->ReceivePacketHandler(&p);

		}
		//First of a multipart packet, still has more parts
		else if(connection->packetReassemblyPosition == 0 && packet->hasMoreParts)
		{
			//Save at correct position of
			memcpy(
					connection->packetReassemblyBuffer,
					bleEvent->evt.gatts_evt.params.write.data,
					bleEvent->evt.gatts_evt.params.write.len
				);

			connection->packetReassemblyPosition += bleEvent->evt.gatts_evt.params.write.len;

			//Do not notify anyone until packet is finished
			logt("CM", "Received first part of message");
		}
		//Multipart packet, intermediate or last frame
		else if(connection->packetReassemblyPosition != 0)
		{
			memcpy(
				connection->packetReassemblyBuffer + connection->packetReassemblyPosition,
				bleEvent->evt.gatts_evt.params.write.data + SIZEOF_CONN_PACKET_SPLIT_HEADER,
				bleEvent->evt.gatts_evt.params.write.len - SIZEOF_CONN_PACKET_SPLIT_HEADER
			);

			//Intermediate packet
			if(packet->hasMoreParts){
				connection->packetReassemblyPosition += bleEvent->evt.gatts_evt.params.write.len - SIZEOF_CONN_PACKET_SPLIT_HEADER;

				logt("CM", "Received middle part of message");

			//Final packet
			} else {
				logt("CM", "Received last part of message");

				//Notify connection
				connectionPacket p;
				p.connectionHandle = bleEvent->evt.gatts_evt.conn_handle;
				p.data = connection->packetReassemblyBuffer;
				p.dataLength = bleEvent->evt.gatts_evt.params.write.len + connection->packetReassemblyPosition - SIZEOF_CONN_PACKET_SPLIT_HEADER;
				p.reliable = bleEvent->evt.gatts_evt.params.write.op == BLE_GATTS_OP_WRITE_CMD ? false : true;

				//Reset the assembly buffer
				connection->packetReassemblyPosition = 0;

				connection->ReceivePacketHandler(&p);
			}
		}
	}
}

void ConnectionManager::fillTransmitBuffers(){

	u32 err;

	//Fill with unreliable packets
	for(int i=0; i<Config->meshMaxConnections; i++)
	{
		while( connections[i]->isConnected() && connections[i]->GetPendingPackets() > 0)
		{
			bool packetCouldNotBeSent = false;

			bool reliable = false;
			u8* data = NULL;
			u16 dataSize = 0;

			if(!connections[i]->handshakeDone() && connections[i]->packetSendQueue->_numElements < 1){
				//TODO: might want to use a different method then GetPendingPackets if we are not in the handshake
				break;
			}

			//A cluster info update is waiting, this is important to send first (but only if handshake is finished and if no split packet is eing sent)
			if(
					connections[i]->handshakeDone()
					&& connections[i]->currentClusterInfoUpdatePacket.header.messageType != 0
					&& connections[i]->packetSendPosition == 0
			){
				//If a clusterUpdate is available we send it immediately
				reliable = true;
				data = (u8*)&(connections[i]->currentClusterInfoUpdatePacket);
				dataSize = SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE;

				logt("CM", "Filling CLUSTER UPDATE for CONN %u", connections[i]->connectionId);
			}
			//Pick the next packet from the packet queue
			else
			{
				//Get one packet from the packet queue
				sizedData packet = connections[i]->packetSendQueue->PeekNext();
				if(packet.length > 0){
					reliable = packet.data[0];
					data = packet.data + 1;
					dataSize = packet.length - 1;
				} else {
					break;
				}
			}

			//Multi-part messages are only supported reliable
			//Switch packet to reliable if it is a multipart packet
			if(dataSize > MAX_DATA_SIZE_PER_WRITE) reliable = true;
			else ((connPacketHeader*) data)->hasMoreParts = 0;

			//The Next packet should be sent reliably
			if(reliable){
				if(connections[i]->reliableBuffersFree > 0){
					//Check if the packet can be transmitted in one MTU
					//If not, it will be sent with message splitting and reliable
					if(dataSize > MAX_DATA_SIZE_PER_WRITE){

						//We might already have started to transmit the packet
						if(connections[i]->packetSendPosition != 0){
							//we need to modify the data a little and build our split
							//message header. This does overwrite some of the old data
							//but that's already been transmitted
							connPacketSplitHeader* newHeader = (connPacketSplitHeader*)(data + connections[i]->packetSendPosition);
							newHeader->hasMoreParts = (dataSize - connections[i]->packetSendPosition > MAX_DATA_SIZE_PER_WRITE) ? 1: 0;
							newHeader->messageType = ((connPacketHeader*) data)->messageType; //We take it from the start of our packet which should still be intact

							//If the packet has more parts, we send a full packet, otherwise we send the remaining bits
							if(newHeader->hasMoreParts) dataSize = MAX_DATA_SIZE_PER_WRITE;
							else dataSize = dataSize - connections[i]->packetSendPosition;

							//Now we set the data pointer to where we left the last time minus our new header
							data = (u8*)newHeader;

						}
						//Or maybe this is the start of the transmission
						else
						{
							((connPacketHeader*) data)->hasMoreParts = 1;
							//Data is alright, but dataSize must be set to its new value
							dataSize = MAX_DATA_SIZE_PER_WRITE;
						}
					}


					//Update packet timestamp as close as possible before sending it
					//TODO: This could be done more accurate because we receive an event when the
					//Packet was sent, so we could calculate the time between sending and getting the event
					//And send a second packet with the time difference.
					//FIXME: Currently, half of the connection interval is added to make up for this
					if(((connPacketHeader*) data)->messageType == MESSAGE_TYPE_UPDATE_TIMESTAMP){
						logt("NODE", "sending time");

						node->UpdateGlobalTime();
						((connPacketUpdateTimestamp*) data)->timestampSec = node->globalTimeSec;
						((connPacketUpdateTimestamp*) data)->remainderTicks = node->globalTimeRemainderTicks + (connections[i]->currentConnectionIntervalMs * APP_TIMER_CLOCK_FREQ / 1000 / 2);
					}


					//Finally, send the packet to the SoftDevice
					err = GATTController::bleWriteCharacteristic(connections[i]->connectionHandle, connections[i]->writeCharacteristicHandle, data, dataSize, true);

					if(err != NRF_SUCCESS) logt("ERROR", "GATT WRITE ERROR %u", err);

					if(err == NRF_SUCCESS){
						//Consume a buffer because the packet was sent
						connections[i]->reliableBuffersFree--;

						memcpy(connections[i]->lastSentPacket, data, dataSize);

						//A special packet that is not from the packetqueue is emptied as soon as it is in the send buffer
						//If it can not be sent, the connection will be disconnected because of a timeout, so we must not await
						//A success event
						if(((connPacketHeader*)data)->messageType == MESSAGE_TYPE_CLUSTER_INFO_UPDATE){
							connections[i]->ClusterUpdateSentHandler();
						} else {

						}

					} else if(err == NRF_ERROR_DATA_SIZE || err == NRF_ERROR_INVALID_PARAM){
						logt("ERROR", "NRF_ERROR sending %u!!!!!!!!!!!!!", err);
						//Drop the packet if it's faulty
						if(((connPacketHeader*)data)->messageType == MESSAGE_TYPE_CLUSTER_INFO_UPDATE){
							logt("ERROR", "MALFORMED CLUSTER_INFO_UPDATE packet");
						} else {
							logt("ERROR", "MALFORMED DATA PACKET FROM QUEUE");
							connections[i]->packetSendQueue->DiscardNext();
						}
					} else {
						//Will try to send it later
						packetCouldNotBeSent = true;
					}

				} else {
					packetCouldNotBeSent = true;
				}
			}

			//The next packet is to be sent unreliably
			if(!reliable){
				if(connections[i]->unreliableBuffersFree > 0)
				{
					err = GATTController::bleWriteCharacteristic(connections[i]->connectionHandle, connections[i]->writeCharacteristicHandle, data, dataSize, false);

					if(err != NRF_SUCCESS) logt("ERROR", "GATT WRITE ERROR %u", err);

					if(err == NRF_SUCCESS){
						//Consumes a send Buffer
						connections[i]->unreliableBuffersFree--;
						logt("CONN", "packet to conn %u (txfree: %d)", i, connections[i]->unreliableBuffersFree);

						memcpy(connections[i]->lastSentPacket, data, dataSize);
					}

					//In either case (success or faulty packet) we drop the packet
					if(err == NRF_SUCCESS || err == NRF_ERROR_DATA_SIZE || err == NRF_ERROR_INVALID_PARAM){

						connections[i]->packetSendQueue->DiscardNext();

					} else {
						//Will try to send it later
						packetCouldNotBeSent = true;
					}

				} else {
					packetCouldNotBeSent = true;
				}
			}

			//Go to next connection if a packet (either reliable or unreliable)
			//could not be sent because the corresponding buffers are full
			if(packetCouldNotBeSent) break;
		}
	}
}


void ConnectionManager::dataTransmittedCallback(ble_evt_t* bleEvent)
{

	ConnectionManager* cm = ConnectionManager::getInstance();
	//There are two types of events that trigger a dataTransmittedCallback
	//A TX complete event frees a number of transmit buffers
	//These are used for all connections
	if(bleEvent->header.evt_id == BLE_EVT_TX_COMPLETE)
	{

		logt("CONN_DATA", "write_CMD complete (n=%d)", bleEvent->evt.common_evt.params.tx_complete.count);

		//This connection has just been given back some transmit buffers
		Connection* connection = cm->GetConnectionFromHandle(bleEvent->evt.common_evt.conn_handle);
		connection->unreliableBuffersFree += bleEvent->evt.common_evt.params.tx_complete.count;

		connection->sentUnreliable++;

		//Next, we should continue sending packets if there are any
		if(cm->GetPendingPackets()) cm->fillTransmitBuffers();

	}
	//The EVT_WRITE_RSP comes after a WRITE_REQ and notifies that a buffer
	//for one specific connection has been cleared
	else if (bleEvent->header.evt_id == BLE_GATTC_EVT_WRITE_RSP)
	{
		if(bleEvent->evt.gattc_evt.gatt_status != BLE_GATT_STATUS_SUCCESS)
		{
			logt("ERROR", "GATT status problem %d %s", bleEvent->evt.gattc_evt.gatt_status, Logger::getGattStatusErrorString(bleEvent->evt.gattc_evt.gatt_status));

			//TODO: Error handling, but there really shouldn't be an error....;-)
			//FIXME: Handle possible gatt status codes

		}
		else
		{
			logt("CONN_DATA", "write_REQ complete");
			Connection* connection = cm->GetConnectionFromHandle(bleEvent->evt.gattc_evt.conn_handle);

			//Connection could have been disconneced
			if(connection == NULL) return;

			connection->sentReliable++;

			//Check what type of Packet has just been sent
			connPacketHeader* packetHeader = (connPacketHeader*)connection->lastSentPacket;

			if(packetHeader->messageType == MESSAGE_TYPE_CLUSTER_INFO_UPDATE){
				//Nothing to do
			} else {

				logt("CONN_DATA", "Header was type %d hasMoreParts %d", packetHeader->messageType, packetHeader->hasMoreParts);

				//Check if the packet has more parts
				if(packetHeader->hasMoreParts == 0){
					//Packet was either not split at all or is completely sent
					connection->packetSendPosition = 0;

					//Check if this was the end of a handshake, if yes, mark handshake as completed
					if(packetHeader->messageType == MESSAGE_TYPE_CLUSTER_ACK_2)
					{
						//Notify Node of handshakeDone
						cm->node->HandshakeDoneHandler(connection, true);
					}

					//Discard the last packet because it was now successfully sent
					connection->packetSendQueue->DiscardNext();
				} else {
					//Update packet send position if we have more data
					connection->packetSendPosition += MAX_DATA_SIZE_PER_WRITE - SIZEOF_CONN_PACKET_SPLIT_HEADER;
				}
			}

			connection->reliableBuffersFree += 1;


			//Now we continue sending packets
			if(cm->GetPendingPackets()) cm->fillTransmitBuffers();
		}
	}
}


ConnectionManagerCallback::ConnectionManagerCallback(){

}
ConnectionManagerCallback::~ConnectionManagerCallback(){


}

void ConnectionManager::setConnectionManagerCallback(ConnectionManagerCallback* cb)
{
	connectionManagerCallback = cb;
}

Connection* ConnectionManager::GetConnectionToShortestSink(Connection* excludeConnection)
{
	clusterSIZE min = INT16_MAX;
	Connection* c = NULL;
	for(int i=0; i<Config->meshMaxConnections; i++){
		if(excludeConnection != NULL && connections[i] == excludeConnection) continue;
		if(connections[i]->handshakeDone() && connections[i]->hopsToSink > -1 && connections[i]->hopsToSink < min){
			min = connections[i]->hopsToSink;
			c = connections[i];
		}
	}
	return c;
}

clusterSIZE ConnectionManager::GetHopsToShortestSink(Connection* excludeConnection)
{
	if(node->persistentConfig.deviceType == deviceTypes::DEVICE_TYPE_SINK){
		logt("SINK", "HOPS 0, clID:%x, clSize:%d", node->clusterId, node->clusterSize);
			return 0;
		} else {

			clusterSIZE min = INT16_MAX;
			Connection* c = NULL;
			for(int i=0; i<Config->meshMaxConnections; i++){
				if(connections[i] == excludeConnection || !connections[i]->isConnected()) continue;
				if(connections[i]->hopsToSink > -1 && connections[i]->hopsToSink < min){
					min = connections[i]->hopsToSink;
					c = connections[i];
				}
			}

			logt("SINK", "HOPS %d, clID:%x, clSize:%d", (c == NULL) ? -1 : c->hopsToSink, node->clusterId, node->clusterSize);
			return (c == NULL) ? -1 : c->hopsToSink;
		}
}
