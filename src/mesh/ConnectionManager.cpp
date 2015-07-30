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

#include <string>

#include <types.h>
#include <Node.h>
#include <ConnectionManager.h>
#include <GATTController.h>
#include <GAPController.h>
#include <Utility.h>
#include <Logger.h>

extern "C"{
#include <app_error.h>
}




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


	//Create all connections
	inConnection = connections[0] = new Connection(0, this, Node::getInstance(), Connection::CONNECTION_DIRECTION_IN);

	for(int i=1; i<=Config->meshMaxOutConnections; i++){
		connections[i] = outConnections[i-1] = new Connection(i, this, Node::getInstance(), Connection::CONNECTION_DIRECTION_OUT);
	}

	//Find out how many tx buffers we have
	u32 err =  sd_ble_tx_buffer_count_get(&txBufferFreeCount);
	APP_ERROR_CHECK(err);

	//Register GAPController callbacks
	GAPController::setDisconnectionHandler(DisconnectionHandler);
	GAPController::setConnectionSuccessfulHandler(ConnectionSuccessfulHandler);
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
	/*char stringBuffer[200];
	Logger::getInstance().convertBufferToHexString(data, dataLength, stringBuffer);

	logt("CONN_QUEUE", "PUT_PACKET(%d):len:%d,type:%d, hex: %s",connection->connectionId, dataLength, data[0], stringBuffer);
*/
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

		if(!cm->doHandshake){
				c->handshakeDone = true;
			}
	}
	//We are master (central)
	else if (bleEvent->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_CENTRAL)
	{
		c = cm->pendingConnection;

		c->ConnectionSuccessfulHandler(bleEvent);
		cm->connectionManagerCallback->ConnectionSuccessfulHandler(bleEvent);

		if(cm->doHandshake) c->StartHandshake();
		else c->handshakeDone = true;
	}

	cm->pendingConnection = NULL;



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
		//Check if we need to reassemble the packet
		connPacketHeader* packet = (connPacketHeader*)bleEvent->evt.gatts_evt.params.write.data;
		if(connection->packetReassemblyPosition == 0 && packet->hasMoreParts == 0)
		{
			//Print packet as hex
			char stringBuffer[100];
			Logger::getInstance().convertBufferToHexString(bleEvent->evt.gatts_evt.params.write.data, bleEvent->evt.gatts_evt.params.write.len, stringBuffer);
			logt("CONN", "Received type %d, hasMore %d, length %d, reliable %d", ((connPacketHeader*)bleEvent->evt.gatts_evt.params.write.data)->messageType, ((connPacketHeader*)bleEvent->evt.gatts_evt.params.write.data)->hasMoreParts, bleEvent->evt.gatts_evt.params.write.len, bleEvent->evt.gatts_evt.params.write.op);
			logt("CONN", "%s", stringBuffer);

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

void ConnectionManager::fillTransmitBuffers()
{
	u32 err = 0;
	//Filling the buffers nicely is not an easy task but is probably
	//possible in a future SoftDevice version
	//meanwhile, here is a trivial implementation


	//Loop through all 4 connections infinitely
	bool continueSending = false;
	int i = -1;
	while(true){

		i = (i+1) % Config->meshMaxConnections;
		if(i == 0) continueSending = false;


		//Check if the connection is connected and has available packets
		if(connections[i]->isConnected && connections[i]->packetSendQueue->_numElements > 0)
		{
			//Get one packet from the packet queue
			sizedData packet = connections[i]->packetSendQueue->PeekNext();




			bool reliable = packet.data[0];
			u8* data = packet.data + 1;
			u16 dataSize = packet.length - 1;


			//Multi-part messages are only supported reliable
			if(dataSize > MAX_DATA_SIZE_PER_WRITE) reliable = true;
			else ((connPacketHeader*) data)->hasMoreParts = 0;


			//Reliable packets
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

					err = GATTController::bleWriteCharacteristic(connections[i]->connectionHandle, connections[i]->writeCharacteristicHandle, data, dataSize, true);
					APP_ERROR_CHECK(err);


					if(err == NRF_SUCCESS){
						connections[i]->reliableBuffersFree--;
					}

					//logt("CONN", "Reliable Write error %d", err);

					//If we have another packet, we continue sending
					if(connections[i]->packetSendQueue->_numElements > 0) continueSending = true;
				}
			//Unreliable packets
			} else {
				if(txBufferFreeCount > 0){
					err = GATTController::bleWriteCharacteristic(connections[i]->connectionHandle, connections[i]->writeCharacteristicHandle, data, dataSize, false);

					if(err == NRF_SUCCESS){
						txBufferFreeCount--;
						connections[i]->packetSendQueue->DiscardNext();
					}

					//logt("CONN", "Unreliable Write error %d", err);

					if(connections[i]->packetSendQueue->_numElements > 0) continueSending = true;
				}
			}
		}


		//Check if all buffers are filled
		if(i == MAXIMUM_CONNECTIONS-1 && !continueSending) break;
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

		logt("CONN", "write_CMD complete (n=%d)", bleEvent->evt.common_evt.params.tx_complete.count);
		//A TX complete event frees a number of transmit buffers that can be used for all
		//connections
		cm->txBufferFreeCount += bleEvent->evt.common_evt.params.tx_complete.count;

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
			//TODO: After a split packet has been sent successfully, we must increment the sendposition
			//and remove the packet only if it is finished



			logt("CONN", "write_REQ complete");
			Connection* connection = cm->GetConnectionFromHandle(bleEvent->evt.gattc_evt.conn_handle);

			connPacketSplitHeader* header = (connPacketSplitHeader*)(connection->packetSendQueue->PeekNext().data + 1 + connection->packetSendPosition); //+1 to remove reliable byte
			logt("CONN", "header is type %d and moreData %d (%d-%d)", header->messageType, header->hasMoreParts, connection->packetSendQueue->PeekNext().data, header);

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
		logt("ERROR", "HOPS 1");
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

			logt("ERROR", "HOPS %d", (c == NULL) ? -1 : c->hopsToSink);
			return (c == NULL) ? -1 : c->hopsToSink;
		}
}
