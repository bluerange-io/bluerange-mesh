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
#include <GATTController.h>
#include <GAPController.h>
#include <Utility.h>
#include <Logger.h>

extern "C"{
#include <app_error.h>
#include <app_timer.h>
}


//The flow for any connection is:
// Connected => Encrypted => Mesh handle discovered => Handshake done
//encryption can be disabled and handle discovery can be skipped (handle from JOIN_ME packet will be used)




ConnectionManager* ConnectionManager::instance = NULL;

ConnectionManager::ConnectionManager(){

	connectionManagerCallback = NULL;

	//Initialize GAP and GATT
	GAPController::bleConfigureGAP();
	GATTController::bleMeshServiceInit();

	//Config->
	doHandshake = true;

	//init vars
	pendingPackets = 0;
	pendingConnection = NULL;
	freeOutConnections = Config->meshMaxOutConnections;
	freeInConnections = Config->meshMaxInConnections;

	//Find out how many tx buffers we have
	u32 err =  sd_ble_tx_buffer_count_get(&txBuffersPerLink);
	APP_ERROR_CHECK(err);

	//Create all connections
	inConnection = connections[0] = new Connection(0, this, Node::getInstance(), Connection::CONNECTION_DIRECTION_IN);

	for(int i=1; i<=Config->meshMaxOutConnections; i++){
		connections[i] = outConnections[i-1] = new Connection(i, this, Node::getInstance(), Connection::CONNECTION_DIRECTION_OUT);
	}

	//Register GAPController callbacks
	GAPController::setDisconnectionHandler(DisconnectionHandler);
	GAPController::setConnectionSuccessfulHandler(ConnectionSuccessfulHandler);
	GAPController::setConnectionEncryptedHandler(ConnectionEncryptedHandler);
	GAPController::setConnectionTimeoutHandler(ConnectionTimeoutHandler);

	//Set GATTController callbacks
	GATTController::setMessageReceivedCallback(messageReceivedCallback);
	GATTController::setHandleDiscoveredCallback(handleDiscoveredCallback);
	GATTController::setDataTransmittedCallback(dataTransmittedCallback);

}


//Send message to a single connection
bool ConnectionManager::SendMessage(Connection* connection, u8* data, u16 dataLength, bool reliable)
{
	//Some checks first
	QueuePacket(connection, data, dataLength, reliable);

	fillTransmitBuffers();

	return true;
}

//Send a message over all connections, except one connection
void ConnectionManager::SendMessageOverConnections(Connection* ignoreConnection, u8* data, u16 dataLength, bool reliable)
{
	for (int i = 0; i < Config->meshMaxConnections; i++)
	{
		if (connections[i] != ignoreConnection && connections[i]->handshakeDone){
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
			packetHeader->receiver == Node::getInstance()->persistentConfig.nodeId
			|| (originConnection == NULL && packetHeader->receiver == NODE_ID_BROADCAST)
	)
	{
		connectionPacket packet;
		packet.connectionHandle = 0; //Not needed
		packet.data = data;
		packet.dataLength = dataLength;
		packet.reliable = reliable;

		//Send directly to our message handler in the node for processing
		Node::getInstance()->messageReceivedCallback(&packet);
	}


	//Packets to the shortest sink
	if(packetHeader->receiver == NODE_ID_SHORTEST_SINK)
	{
		Connection* dest = GetConnectionToShortestSink(NULL);

		//Packets are currently only delivered if a sink is known
		if(dest) SendMessage(dest, data, dataLength, reliable);
	}
	//All other packets will be broadcasted, but we could and should check if the receiver is connected to us
	else if(packetHeader->receiver != Node::getInstance()->persistentConfig.nodeId)
	{
		SendMessageOverConnections(originConnection, data, dataLength, reliable);
	}
}

void ConnectionManager::QueuePacket(Connection* connection, u8* data, u16 dataLength, bool reliable){
	//Print packet as hex
	char stringBuffer[200];
	Logger::getInstance().convertBufferToHexString(data, dataLength, stringBuffer);

	logt("CONN_DATA", "PUT_PACKET(%d):len:%d,type:%d, hex: %s",connection->connectionId, dataLength, data[0], stringBuffer);

	//Save packet
	bool putResult = connection->packetSendQueue->Put(data, dataLength, reliable);

	if(putResult) {
		pendingPackets++;
	} else {
		//TODO: Error handling: What should happen when the queue is full?
		//Currently, additional packets are dropped
		logt("ERROR", "Send queue is already full");
	}
}


Connection* ConnectionManager::getFreeConnection()
{
	for (int i = 0; i < Config->meshMaxConnections; i++)
		{
			if (!connections[i]->isConnected) return connections[i];
		}
	return NULL;
}

//Looks if there is a free connection available
Connection* ConnectionManager::GetFreeOutConnection()
{
	for (int i = 0; i < Config->meshMaxOutConnections; i++)
	{
		if (!outConnections[i]->isConnected) return outConnections[i];
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

	freeOutConnections--;

	//Get a free connection and set it as pending
	pendingConnection = GetFreeOutConnection();

	pendingConnection->writeCharacteristicHandle = writeCharacteristicHandle;

	pendingConnection->Connect(address, writeCharacteristicHandle);

	char addrString[20];
	Logger::getInstance().convertBufferToHexString(pendingConnection->partnerAddress.addr, 6, addrString);

	bool err = GAPController::connectToPeripheral(address);

	logt("CONN", "Connect as Master to %d (%s) %s", partnerId, addrString, err ? "true" : "false");


	return pendingConnection;
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
void ConnectionManager::DisconnectOtherConnections(Connection* connection)
{
	for (int i = 0; i < Config->meshMaxOutConnections+1; i++)
	{
		if (connections[i] != connection && connections[i]->isConnected)
			connections[i]->Disconnect();
	}
}


#define _________________STATIC_HANDLERS____________
//STATIC methods for handling events
//
//

//Called as soon as a new connection is made, either as central or peripheral
void ConnectionManager::ConnectionSuccessfulHandler(ble_evt_t* bleEvent)
{
	logt("CM", "Connection success");

	//FIXME: There is a slim chance that another central connects to us between
	//This call and before switching off advertising.
	//This connection should be deferred until our handshake is finished

	if(Node::getInstance()->currentDiscoveryState == discoveryState::HANDSHAKE){
		logt("ERROR", "CURRENTLY IN HANDSHAKE!!!!!!!!!!");
	}

	ConnectionManager* cm = ConnectionManager::getInstance();

	Connection* c = NULL;

	//We are slave (peripheral)
	if (bleEvent->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_PERIPH)
	{
		c = cm->inConnection;
		cm->freeInConnections--;

		c->ConnectionSuccessfulHandler(bleEvent);
		cm->connectionManagerCallback->ConnectionSuccessfulHandler(bleEvent);

		//If encryption is enabled, we wait for the central to start encrypting
		if(Config->encryptionEnabled)
		{

		}
		//If handshake is disabled,
		else if(!cm->doHandshake){
			c->handshakeDone = true;
		}
	}
	//We are master (central)
	else if (bleEvent->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_CENTRAL)
	{
		c = cm->pendingConnection;

		c->ConnectionSuccessfulHandler(bleEvent);
		cm->connectionManagerCallback->ConnectionSuccessfulHandler(bleEvent);

		//If encryption is enabled, the central starts to encrypt the connection
		if(Config->encryptionEnabled)
		{
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
			c->handshakeDone = true;
		}
	}

	cm->pendingConnection = NULL;
}

//When a connection changes to encrypted
void ConnectionManager::ConnectionEncryptedHandler(ble_evt_t* bleEvent)
{
	ConnectionManager* cm = ConnectionManager::getInstance();
	Connection* c = cm->GetConnectionFromHandle(bleEvent->evt.gap_evt.conn_handle);

	logt("CM", "Connection id %u is now encrypted", c->connectionId);

	//We are peripheral
	if(c->direction == Connection::CONNECTION_DIRECTION_IN)
	{
		if(!cm->doHandshake){
			c->handshakeDone = true;
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
			c->handshakeDone = true;
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



void ConnectionManager::DisconnectionHandler(ble_evt_t* bleEvent)
{
	ConnectionManager* cm = ConnectionManager::getInstance();

	Connection* connection = cm->GetConnectionFromHandle(bleEvent->evt.gap_evt.conn_handle);
	if (connection != NULL)
	{
		if (connection->direction == Connection::CONNECTION_DIRECTION_IN) cm->freeInConnections++;
		else cm->freeOutConnections++;

		//remove pending packets
		cm->pendingPackets -= connection->packetSendQueue->_numElements;

		connection->isConnected = false;

		//Notify the callback of the disconnection before notifying the connection
		//This will ensure that the values are still set in the connection
		cm->connectionManagerCallback->DisconnectionHandler(bleEvent);

		connection->DisconnectionHandler(bleEvent);
	}
}

void ConnectionManager::ConnectionTimeoutHandler(ble_evt_t* bleEvent)
{
	ConnectionManager* cm = ConnectionManager::getInstance();

	cm->pendingConnection = NULL;
	cm->freeOutConnections++;

	cm->connectionManagerCallback->ConnectionTimeoutHandler(bleEvent);
}

void ConnectionManager::messageReceivedCallback(ble_evt_t* bleEvent)
{
	ConnectionManager* cm = ConnectionManager::getInstance();


	//FIXME: must check for reassembly buffer size, if it is bigger, a stack overflow will occur

	Connection* connection = cm->GetConnectionFromHandle(bleEvent->evt.gap_evt.conn_handle);
	if (connection != NULL)
	{
		//TODO: At this point we should check if the write was a valid operation for the mesh
		/*if( bleEvent->evt.gatts_evt.params.write.handle != GATTController::getMeshWriteHandle() ){
			connection->Disconnect();
			logt("ERROR", "Non mesh device was disconnected");
		}*/


		connPacketHeader* packet = (connPacketHeader*)bleEvent->evt.gatts_evt.params.write.data;

		//At first, some special treatment for out timestamp packet
		if(packet->messageType == MESSAGE_TYPE_UPDATE_TIMESTAMP)
		{
			//Set our time to the received timestamp and update the time when we've received this packet
			app_timer_cnt_get(&Node::getInstance()->globalTimeSetAt);
			Node::getInstance()->globalTime = ((connPacketUpdateTimestamp*)packet)->timestamp;

			logt("NODE", "time updated at:%u with timestamp:%u", Node::getInstance()->globalTimeSetAt, (u32)Node::getInstance()->globalTime);
		}


		//Print packet as hex
		char stringBuffer[100];
		Logger::getInstance().convertBufferToHexString(bleEvent->evt.gatts_evt.params.write.data, bleEvent->evt.gatts_evt.params.write.len, stringBuffer);
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
	//TODO: Some error handling would be nice to have


	//Fill with unreliable packets
	for(int i=0; i<Config->meshMaxConnections; i++)
	{
		if(connections[i]->isConnected && connections[i]->packetSendQueue->_numElements > 0)
		{
			while(connections[i]->packetSendQueue->_numElements > 0)
			{
				bool packetCouldNotBeSent = false;

				//Get one packet from the packet queue
				sizedData packet = connections[i]->packetSendQueue->PeekNext();
				bool reliable = packet.data[0];
				u8* data = packet.data + 1;
				u16 dataSize = packet.length - 1;

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
						if(((connPacketHeader*) data)->messageType == MESSAGE_TYPE_UPDATE_TIMESTAMP){
							//Add the time that it took from setting the time until it gets send
							u32 additionalTime;
							u32 rtc1;
							app_timer_cnt_get(&rtc1);
							app_timer_cnt_diff_compute(rtc1, Node::getInstance()->globalTimeSetAt, &additionalTime);

							logt("NODE", "sending time:%u with prevRtc1:%u, rtc1:%u, diff:%u", (u32)Node::getInstance()->globalTime, Node::getInstance()->globalTimeSetAt, rtc1, additionalTime);

							((connPacketUpdateTimestamp*) data)->timestamp = Node::getInstance()->globalTime + additionalTime;


							/*((connPacketUpdateTimestamp*) data)->timestamp = 0;
							((connPacketUpdateTimestamp*) data)->milliseconds = 0;*/
						}


						//Finally, send the packet to the SoftDevice
						err = GATTController::bleWriteCharacteristic(connections[i]->connectionHandle, connections[i]->writeCharacteristicHandle, data, dataSize, true);
						APP_ERROR_CHECK(err);

						if(err == NRF_SUCCESS)
						{
							connections[i]->reliableBuffersFree--;
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

						if(err == NRF_SUCCESS){
							connections[i]->unreliableBuffersFree--;
							connections[i]->packetSendQueue->DiscardNext();
							logt("CONN", "packet to conn %u (txfree: %d)", i, connections[i]->unreliableBuffersFree);
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
		cm->GetConnectionFromHandle(bleEvent->evt.common_evt.conn_handle)->unreliableBuffersFree += bleEvent->evt.common_evt.params.tx_complete.count;

		cm->pendingPackets -= bleEvent->evt.common_evt.params.tx_complete.count;

		//Next, we should continue sending packets if there are any
		if(cm->pendingPackets) cm->fillTransmitBuffers();

	}
	//The EVT_WRITE_RSP comes after a WRITE_REQ and notifies that a buffer
	//for one specific connection has been cleared
	else if (bleEvent->header.evt_id == BLE_GATTC_EVT_WRITE_RSP)
	{
		if(bleEvent->evt.gattc_evt.gatt_status != BLE_GATT_STATUS_SUCCESS)
		{
			logt("CONN", "GATT status problem %d %s", bleEvent->evt.gattc_evt.gatt_status, Logger::getGattStatusErrorString(bleEvent->evt.gattc_evt.gatt_status));

			//TODO: Error handling, but there really shouldn't be an error....;-)
		}
		else
		{
			logt("CONN_DATA", "write_REQ complete");
			Connection* connection = cm->GetConnectionFromHandle(bleEvent->evt.gattc_evt.conn_handle);

			connPacketSplitHeader* header = (connPacketSplitHeader*)(connection->packetSendQueue->PeekNext().data + 1 + connection->packetSendPosition); //+1 to remove reliable byte
			logt("CONN_DATA", "header is type %d and moreData %d (%d-%d)", header->messageType, header->hasMoreParts, connection->packetSendQueue->PeekNext().data, header);

			//Check if the packet has more parts
			if(header->hasMoreParts == 0){
				//Packet was either not split at all or is completely sent
				connection->packetSendPosition = 0;
				connection->packetSendQueue->DiscardNext();
				cm->pendingPackets--;
			} else {
				//Update packet send position if we have more data
				connection->packetSendPosition += MAX_DATA_SIZE_PER_WRITE - SIZEOF_CONN_PACKET_SPLIT_HEADER;
			}

			connection->reliableBuffersFree += 1;


			//Now we continue sending packets
			if(cm->pendingPackets) cm->fillTransmitBuffers();
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
		if(connections[i]->handshakeDone && connections[i]->hopsToSink > -1 && connections[i]->hopsToSink < min){
			min = connections[i]->hopsToSink;
			c = connections[i];
		}
	}
	return c;
}

clusterSIZE ConnectionManager::GetHopsToShortestSink(Connection* excludeConnection)
{
	if(Node::getInstance()->persistentConfig.deviceType == deviceTypes::DEVICE_TYPE_SINK){
		logt("SINK", "HOPS 1");
			return 0;
		} else {

			clusterSIZE min = INT16_MAX;
			Connection* c = NULL;
			for(int i=0; i<Config->meshMaxConnections; i++){
				if(excludeConnection != NULL && connections[i] == excludeConnection) continue;
				if(connections[i]->hopsToSink > -1 && connections[i]->hopsToSink < min){
					min = connections[i]->hopsToSink;
					c = connections[i];
				}
			}

			logt("SINK", "HOPS %d", (c == NULL) ? -1 : c->hopsToSink);
			return (c == NULL) ? -1 : c->hopsToSink;
		}
}
