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

#include <types.h>
#include <Node.h>
#include <ConnectionManager.h>
#include <AdvertisingController.h>
#include <GATTController.h>
#include <GAPController.h>
#include <ResolverConnection.h>
#include <Utility.h>
#include <Logger.h>

extern "C"
{
#include <app_error.h>
#include <ble_hci.h>
}

//When data is received, it is first processed until packets are available, once these have undergone some basic
//checks, they are considered Messages that are then dispatched to the node and modules

//The flow for any connection is:
// Connected => Encrypted => Mesh handle discovered => Handshake done
//encryption can be disabled and handle discovery can be skipped (handle from JOIN_ME packet will be used)

ConnectionManager::ConnectionManager() :
		GAPControllerHandler(), GATTControllerHandler()
{
	//init vars
	freeMeshOutConnections = Config->meshMaxOutConnections;
	freeMeshInConnections = Config->meshMaxInConnections;

	pendingConnection = NULL;

	memset(allConnections, 0x00, sizeof(allConnections));

	//Register GAPController callbacks
	GAPController::getInstance()->setGAPControllerHandler(this);

	//Set GATTController callbacks
	GATTController::getInstance()->setGATTControllerHandler(this);

}

#define _______________CONNECTIVITY______________

//TODO: Do we route messages to mesh connections or to all connections????
//probably check the destinationId, if the id is within the range of apps, it should be routed
//if the id is for the mesh range, a Module could decide to grab the message and send it to its
//App connections as well

//Checks the receiver of the message first and routes it in the right direction

//TODO: Rename to mesh... we need another implementation
//Connects to a peripheral as Master, writecharacteristichandle can be BLE_GATT_HANDLE_INVALID
void ConnectionManager::ConnectAsMaster(nodeID partnerId, fh_ble_gap_addr_t* address, u16 writeCharacteristicHandle)
{
	//Only connect when not currently in another connection or when there are no more free connections
	if (freeMeshOutConnections < 1 || pendingConnection != NULL) return;

	//Create the connection and set it as pending, this is done before starting the GAP connect to avoid race conditions
	for (u32 i = 0; i < MAX_NUM_CONNECTIONS; i++){
		if (allConnections[i] == NULL){
			pendingConnection = allConnections[i] = new MeshConnection(i, ConnectionDirection::CONNECTION_DIRECTION_OUT, address, writeCharacteristicHandle);
			break;
		}
	}
	if(pendingConnection == NULL){
		logt("ERROR", "No free connection");
	}

	//Disperse connection intervals over time, maybe this leads to less connection losses
	//u16 connectionInterval = Config->meshMinConnectionInterval + connection->connectionId;

	//Tell the GAP Layer to connect, it will return if it is trying or if there was an error
	bool status = GAPController::getInstance()->connectToPeripheral(address, Config->meshMinConnectionInterval, Config->meshConnectingScanTimeout);

	if (status)
	{
		char addrString[20];
		Logger::getInstance()->convertBufferToHexString(address->addr, 6, addrString, 20);
		logt("CONN", "Connecting as Master to %d (%02X:%02X:%02X:%02X:%02X:%02X) %s",
			partnerId,
			address->addr[5],
			address->addr[4],
			address->addr[3],
			address->addr[2],
			address->addr[1],
			address->addr[0],
			status ? "true" : "false");
	} else {
		//Clean the connection that has just been created
		DeleteConnection(pendingConnection);
	}
}

void ConnectionManager::DeleteConnection(BaseConnection* connection){
	for(u32 i=0; i<MAX_NUM_CONNECTIONS; i++){
		if(connection == allConnections[i]){
			delete connection;
			allConnections[i] = NULL;
		}
	}
	if(pendingConnection == connection){
		pendingConnection = NULL;
	}
}

////TODO: porobably MeshConnection
///** Tries to reestablish connections */
//int ConnectionManager::ReestablishConnections()
//{
//	//Check if any of the reestablish connections should be disconnected forever
//	for (u32 i = 0; i < MAX_NUM_CONNECTIONS; i++)
//	{
//		if (connections[i]->connectionState == ConnectionState::CONNECTION_STATE_REESTABLISHING && GS->node->appTimerDs - connections[i]->disconnectedTimestampDs > SEC_TO_DS(connections[i]->reestablishTimeSec)
//		)
//		{
//			Disconnect(connections[i]->connectionHandle);
//			//FIXME: must implement
//			logt("ERROR", "REAL DISCONNECT, must implement");
//		}
//	}
//
//	//First, check which connection needs reestablishing
//	//TODO: We could use a whitelist of connection partner is we need to reestablish
//	//multiple
//	MeshConnection* connection = NULL;
//	for(int i=0; i<Config->meshMaxConnections; i++)
//	{
//		if(connections[i]->connectionState == ConnectionState::CONNECTION_STATE_REESTABLISHING)
//		{
//			connection = connections[i];
//			break;
//		}
//	}
//
//	//Do not reestablish if nothing was found
//	if(connection == NULL) return 0;
//
//	//We have been a Peripheral in this connection, start to advertise
//	if(connection->direction == ConnectionDirection::CONNECTION_DIRECTION_IN)
//	{
//		logt("CM", "Waiting for node %u to reconnect", connection->partnerId);
//		//Update packet to make it connectable
//		GS->node->UpdateJoinMePacket();
//
//		return 1;
//
//	}
//	else
//	{
//		logt("CM", "Should reconnect to node %u, %02x:..:%02x", connection->partnerId, connection->partnerAddress.addr[0], connection->partnerAddress.addr[5]);
//
//		//try to connect with same settings as previous connection as master
//		bool result = GAPController::getInstance()->connectToPeripheral(
//		&connection->partnerAddress,
//		connection->currentConnectionIntervalMs,
//		connection->reestablishTimeSec);
//
//		if(result)
//		{
//			logt("CM", "Waiting for connection to node %u", connection->partnerId);
//			//Put this connection in the pending connection
//			//not needed, can be compared to address
//			//pendingConnection = connection;
//
//			return 2;
//
//		}
//		else
//		{
//			logt("CM", "Connection not possible right now");
//
//			return 0;
//		}
//	}
//}

//Disconnects a specific connection
void ConnectionManager::Disconnect(u16 connectionHandle)
{
	BaseConnection* connection = GetConnectionFromHandle(connectionHandle);
	if (connection != NULL)
	{
		connection->Disconnect();
	}
}

//TODO: Mesh specific
//Disconnects either all connections or all except one
//Cluster updates from this connection should be ignored
void ConnectionManager::ForceDisconnectOtherMeshConnections(MeshConnection* connection)
{
	MeshConnections conn = GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);
	for(u32 i=0; i< conn.count; i++){
		if (conn.connections[i] != connection){
			conn.connections[i]->Disconnect();
			conn.connections[i]->connectionStateBeforeDisconnection = ConnectionState::CONNECTION_STATE_DISCONNECTED;
		}
	}
}

//Changes the connection interval of all mesh connections
void ConnectionManager::SetMeshConnectionInterval(u16 connectionInterval)
{
	//Go through all connections that we control as a central
	MeshConnections conn = GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_OUT);
	for(u32 i=0; i< conn.count; i++){
		if (conn.connections[i]->handshakeDone()){
			GAPController::getInstance()->RequestConnectionParameterUpdate(conn.connections[i]->connectionHandle, connectionInterval, connectionInterval, 0, Config->meshConnectionSupervisionTimeout);
		}
	}
}

#define _________________RESOLVING____________
//This part deals with resolving the correct connection type once a peripheral connection is established
//A single handshake packet sent by the central is used to determine the connectionType and to upgrade the
//connection to its real type

//Upgrade a connection to another connection type after it has been determined
void ConnectionManager::ResolveConnection(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8* data)
{
	//ConnectionTypeResolvers are collected in a special linker section
	u8 numConnTypeResolvers = (((u32)__stop_conn_type_resolvers) - ((u32)__start_conn_type_resolvers)) / sizeof(u32);
	ConnTypeResolver* resolvers = (ConnTypeResolver*)__start_conn_type_resolvers;

	logt("ERROR", "numConnTypeResolvers %u", numConnTypeResolvers);

	//Check if any resolver matches the received data
	for(int i=0; i<numConnTypeResolvers; i++){
		if(resolvers[i] == NULL) break;

		BaseConnection* newConnection = resolvers[i](oldConnection, sendData, data);

		//If the resolver found a suitable connection upgrade, find the connection reference and replace
		//it with a new instance of our upgraded connection
		if(newConnection != NULL){
			for(int i=0; i<MAX_NUM_CONNECTIONS; i++){
				if(allConnections[i] == oldConnection){
					newConnection->ConnectionSuccessfulHandler(oldConnection->connectionHandle, oldConnection->currentConnectionIntervalMs);
					newConnection->ReceiveDataHandler(sendData, data);

					//Delete old connection and replace pointer with new connection
					delete oldConnection;
					allConnections[i] = newConnection;
					return;
				}
			}
		}
	}


}

#define _________________SENDING____________

void ConnectionManager::SendMeshMessage(u8* data, u8 dataLength, DeliveryPriority priority, bool reliable)
{
	connPacketHeader* packetHeader = (connPacketHeader*) data;

	// ##########################
	//This packet was only meant for us, sth. like a packet to localhost
	//Or if we sent this as a broadcast, we want to handle it ourself as well
	if (packetHeader->receiver == GS->node->persistentConfig.nodeId || packetHeader->receiver == NODE_ID_BROADCAST)
	{
		//Build fake data for our loopback packet
		BaseConnectionSendData sendData;
		sendData.characteristicHandle = BLE_CONN_HANDLE_INVALID;
		sendData.dataLength = dataLength;
		sendData.deliveryOption = reliable ? DeliveryOption::DELIVERY_OPTION_WRITE_REQ : DeliveryOption::DELIVERY_OPTION_WRITE_CMD;

		//TODO: Maybe we should pass some local loopback connection reference

		//Send message to the node
		GS->node->MeshMessageReceivedHandler(NULL, &sendData, (connPacketHeader*)data);

		//Now we must pass the message to all of our modules for further processing
		for(int i=0; i<MAX_MODULE_COUNT; i++){
			if(GS->node->activeModules[i] != 0){
				GS->node->activeModules[i]->MeshMessageReceivedHandler(NULL, &sendData, (connPacketHeader*)data);
			}
		}
	}

	// ##########################
	//Packets to the shortest sink, can only be sent to mesh partners
	if (packetHeader->receiver == NODE_ID_SHORTEST_SINK)
	{
		BroadcastMeshPacket(data, dataLength, priority, reliable);

		//TODO: Implement sink routing
//		MeshConnection* dest = GetMeshConnectionToShortestSink(NULL);
//
//		//FIXME: Packets are currently only delivered if a sink is known
//		if (dest)
//		{
//			dest->SendData(data, dataLength, priority, reliable);
//		}
	}

	// ##########################
	// Packets specific to Apps (Like a broadcast channel for specific apps
	else if (packetHeader->receiver > NODE_ID_APP_BASE && packetHeader->receiver < NODE_ID_APP_BASE + NODE_ID_APP_BASE_SIZE)
	{
		//TODO: Broadcast to all nodes, also call the MessageReceivedHandler for AppConnections
	}

	//All other packets will be broadcasted, but we could and should check if the receiver is connected to us
	else if (packetHeader->receiver != GS->node->persistentConfig.nodeId)
	{
		BroadcastMeshPacket(data, dataLength, priority, reliable);
	}
}

void ConnectionManager::BroadcastMeshPacket(u8* data, u8 dataLength, DeliveryPriority priority, bool reliable)
{
	MeshConnections conn = GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);
	for(u32 i=0; i< conn.count; i++){
		conn.connections[i]->SendData(data, dataLength, priority, reliable);
	}
}

void ConnectionManager::fillTransmitBuffers()
{
	MeshConnections conn = GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);
	for(u32 i=0; i< conn.count; i++){
		conn.connections[i]->FillTransmitBuffers();
	}
}

//FIXME: Some rewrite of this method necessary
void ConnectionManager::GATTDataTransmittedHandler(ble_evt_t* bleEvent)
{
	ConnectionManager* cm = ConnectionManager::getInstance();
	//There are two types of events that trigger a dataTransmittedCallback
	//A TX complete event frees a number of transmit buffers
	//These are used for all connections


	u16 txCompleteConnectionHandle = BLE_CONN_HANDLE_INVALID;
	u8 txCompleteCount = 0;

#if defined(NRF51) || defined(SIM_ENABLED)
	if(bleEvent->header.evt_id == BLE_EVT_TX_COMPLETE){
		txCompleteConnectionHandle = bleEvent->evt.common_evt.conn_handle;
		txCompleteCount = bleEvent->evt.common_evt.params.tx_complete.count;
	}
#elif defined(NRF52)
	if(bleEvent->header.evt_id == BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE){
		txCompleteConnectionHandle = bleEvent->evt.gattc_evt.conn_handle;
		txCompleteCount = bleEvent->evt.gattc_evt.params.write_cmd_tx_complete.count;
	} else if (bleEvent->header.evt_id == BLE_GATTS_EVT_HVN_TX_COMPLETE){
		txCompleteConnectionHandle = bleEvent->evt.gatts_evt.conn_handle;
		txCompleteCount = bleEvent->evt.gatts_evt.params.hvn_tx_complete.count;
	}
#endif


	if (txCompleteConnectionHandle != BLE_CONN_HANDLE_INVALID)
	{
		logt("CONN_DATA", "write_CMD complete (n=%d)", txCompleteCount);

		//This connection has just been given back some transmit buffers
		BaseConnection* connection = cm->GetConnectionFromHandle(txCompleteConnectionHandle);
		if (connection == NULL)
			return;

		connection->unreliableBuffersFree += txCompleteCount;

		connection->sentUnreliable++;

		//Next, we should continue sending packets if there are any
		if (cm->GetPendingPackets())
			cm->fillTransmitBuffers();

	}
	//The EVT_WRITE_RSP comes after a WRITE_REQ and notifies that a buffer
	//for one specific connection has been cleared
	else if (bleEvent->header.evt_id == BLE_GATTC_EVT_WRITE_RSP)
	{
		if (bleEvent->evt.gattc_evt.gatt_status != BLE_GATT_STATUS_SUCCESS)
		{
			logt("ERROR", "GATT status problem %d %s", bleEvent->evt.gattc_evt.gatt_status, Logger::getInstance()->getGattStatusErrorString(bleEvent->evt.gattc_evt.gatt_status));

			//TODO: Error handling, but there really shouldn't be an error....;-)
			//FIXME: Handle possible gatt status codes

		}
		else
		{
			logt("CONN_DATA", "write_REQ complete");
			BaseConnection* connection = cm->GetConnectionFromHandle(bleEvent->evt.gattc_evt.conn_handle);

			//Connection could have been disconneced
			if (connection == NULL)
				return;

			connection->sentReliable++;
			connection->reliableBuffersFree += 1;

			//Now we continue sending packets
			if (cm->GetPendingPackets())
				cm->fillTransmitBuffers();
		}
	}
}

#define _________________RECEIVING____________

void ConnectionManager::GattDataReceivedHandler(ble_evt_t* bleEvent)
{
	ble_gatts_evt_write_t* writeEvent = (ble_gatts_evt_write_t*) &bleEvent->evt.gatts_evt.params.write;
	connPacketHeader* packetHeader = (connPacketHeader*)writeEvent->data;

	BaseConnectionSendData sendData;
	sendData.characteristicHandle = writeEvent->handle;
	sendData.deliveryOption = (writeEvent->op == BLE_GATTS_OP_WRITE_REQ) ? DeliveryOption::DELIVERY_OPTION_WRITE_REQ : DeliveryOption::DELIVERY_OPTION_WRITE_CMD;
	sendData.priority = DeliveryPriority::DELIVERY_PRIORITY_LOW; //TODO: Prio is unknown, should we send it in the packet?
	sendData.dataLength = (u8)writeEvent->len;

	//Get the handling connection for this write
	u16 connectionHandle = bleEvent->evt.gatts_evt.conn_handle;
	BaseConnection* connection = GS->cm->GetConnectionFromHandle(connectionHandle);

	//Notify our connection instance that data has been received
	if (connection != NULL){
		connection->ReceiveDataHandler(&sendData, writeEvent->data);
	}

}

//This method accepts connPackets and distributes it to all other mesh connections
void ConnectionManager::RouteMeshData(MeshConnection* connection, BaseConnectionSendData* sendData, u8* data)
{
	connPacketHeader* packetHeader = (connPacketHeader*) data;


	/*#################### Modification ############################*/


	/*#################### ROUTING ############################*/

	//We are the last receiver for this packet
	if (packetHeader->receiver == GS->node->persistentConfig.nodeId //We are the receiver
	|| packetHeader->receiver == NODE_ID_HOPS_BASE + 1//The packet was meant to travel only one hop
	|| (packetHeader->receiver == NODE_ID_SHORTEST_SINK && GS->node->persistentConfig.deviceType == deviceTypes::DEVICE_TYPE_SINK)//Packet was meant for the shortest sink and we are a sink
	)
	{
		//No packet forwarding needed here.
	}
	//The packet should continue to the shortest sink
	else if(packetHeader->receiver == NODE_ID_SHORTEST_SINK)
	{
		//Send to all other connections (as long as sink routing is not implemented)
		BroadcastMeshData(connection, sendData, data);

		//TODO: implement sink routing

//		MeshConnection* connection = GS->cm->GetMeshConnectionToShortestSink(NULL);
//
//		if(connection)
//		{
//			connection->SendData(sendData, data);
//		}
//		//We could send it as a broadcast or we just drop it if we do not know any sink
//		else
//		{
//
//		}
	}
	//This could be either a packet to a specific node, group, with some hops left or a broadcast packet
	else
	{
		//If the packet should travel a number of hops, we decrement that part
		if(packetHeader->receiver > NODE_ID_HOPS_BASE && packetHeader->receiver < NODE_ID_HOPS_BASE + 1000)
		{
			packetHeader->receiver--;
		}

		//Do not forward cluster info update packets, these are handeled by the node
		if(packetHeader->messageType != MESSAGE_TYPE_CLUSTER_INFO_UPDATE)
		{
			//Send to all other connections
			BroadcastMeshData(connection, sendData, data);
		}
	}
}

void ConnectionManager::BroadcastMeshData(MeshConnection* ignoreConnection, BaseConnectionSendData* sendData, u8* data)
{
	//Iterate through all mesh connections except the ignored one and send the packet
	MeshConnections conn = GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);
	for(u32 i=0; i< conn.count; i++){
		if (conn.connections[i] != ignoreConnection){
			sendData->characteristicHandle = conn.connections[i]->partnerWriteCharacteristicHandle;
			conn.connections[i]->SendData(sendData, data);
		}
	}
}

#define _________________MESH_CONNECTIONS____________

//TODO: Also rewrite Message routing, unify relay and SendMessage function
//TODO: Still a lot of work in this function
//TODO: Generalize and support meshConnections and AppConnections
//Called as soon as a new connection is made, either as central or peripheral
void ConnectionManager::GapConnectionConnectedHandler(ble_evt_t* bleEvent)
{
	u32 err;

	ble_gap_evt_connected_t* connectedEvent = &bleEvent->evt.gap_evt.params.connected;

	logt("CM", "Connection handle %u success as %s, partner:%02x:%02x:%02x:%02x:%02x:%02x", bleEvent->evt.gap_evt.conn_handle, bleEvent->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_CENTRAL ? "Central" : "Peripheral", bleEvent->evt.gap_evt.params.connected.peer_addr.addr[5], bleEvent->evt.gap_evt.params.connected.peer_addr.addr[4], bleEvent->evt.gap_evt.params.connected.peer_addr.addr[3], bleEvent->evt.gap_evt.params.connected.peer_addr.addr[2], bleEvent->evt.gap_evt.params.connected.peer_addr.addr[1], bleEvent->evt.gap_evt.params.connected.peer_addr.addr[0]);

	Logger::getInstance()->logError(Logger::errorTypes::CUSTOM, Logger::customErrorTypes::CONNECTION_SUCCESS, 0);

//	BaseConnection* connection = IsConnectionReestablishment(bleEvent);
//
//	/* Part A: We have a connection reestablishment */
//	if (connection != NULL)
//	{
//		connection->ReconnectionSuccessfulHandler(bleEvent);
//		fillTransmitBuffers();
//		GS->node->ChangeState(discoveryState::DISCOVERY);
//		return;
//	}
	/* Part B: A normal incoming/outgoing connection */

	//FIXME: There is a slim chance that another central connects to us between
	//This call and before switching off advertising.
	//This connection should be deferred until our handshake is finished
	//Set a variable here that a handshake is ongoing and block any other handshake
	//From happening in the meantime, just disconnect the intruder
	//If we are currently doing a Handshake, we disconnect this connection
	//beacuse we cannot do two handshakes at the same time
	if (GS->node->currentDiscoveryState == discoveryState::HANDSHAKE)
	{
		logt("ERROR", "CURRENTLY IN HANDSHAKE!!!!!!!!!!");

		//If we have a pendingConnection for this, we must clean it
		if(bleEvent->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_CENTRAL){
			if(pendingConnection != NULL){
				DeleteConnection(pendingConnection);
			}
		}
		err = sd_ble_gap_disconnect(bleEvent->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);

		return;
	}

	BaseConnection* c = NULL;

	//We are slave (peripheral)
	if (bleEvent->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_PERIPH)
	{
		logt("ERROR", "Incoming Connection connected");

		//Check if we have a free entry in our connections array
		BaseConnection** inConnection = getFreeConnectionSpot();
		if(inConnection == NULL){
			logt("ERROR", "No spot");
			APP_ERROR_CHECK(FRUITYMESH_ERROR_NO_FREE_CONNECTION_SLOTS);// This is a critical error
		}



		fh_ble_gap_addr_t peerAddress = FruityHal::Convert(&connectedEvent->peer_addr);

		c = *inConnection = new ResolverConnection(0, ConnectionDirection::CONNECTION_DIRECTION_IN, &peerAddress);
		c->ConnectionSuccessfulHandler(bleEvent->evt.gap_evt.conn_handle, bleEvent->evt.gap_evt.params.connected.conn_params.min_conn_interval);


		//The central may now start encrypting or start the handshake, we just have to wait
	}
	//We are master (central)
	else if (bleEvent->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_CENTRAL)
	{

		if(pendingConnection == NULL){
			logt("ERROR", "CM fail");
			err = sd_ble_gap_disconnect(bleEvent->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
			return;
		}

		c = pendingConnection;
		pendingConnection = NULL;

		//Call Prepare again so that the clusterID and size backup are created with up to date values
		c->ConnectionSuccessfulHandler(bleEvent->evt.gap_evt.conn_handle, bleEvent->evt.gap_evt.params.connected.conn_params.min_conn_interval);

		//If encryption is enabled, the central starts to encrypt the connection
		if (Config->encryptionEnabled){
			c->encryptionState = EncryptionState::ENCRYPTING;
			GAPController::getInstance()->startEncryptingConnection(bleEvent->evt.gap_evt.conn_handle);
		}
		//Without encryption we immediately start the handshake
		else {
			c->StartHandshake();
		}
	}
}

//If we wanted to connect but our connection timed out (only outgoing connections)
void ConnectionManager::GapConnectingTimeoutHandler(ble_evt_t* bleEvent)
{
	if (pendingConnection == NULL)
		return;

	//Save connection type, but clear pending connection, because the handlers might try to
	//create a new connection immediately
	ConnectionTypes connectionType = pendingConnection->connectionType;

	DeleteConnection(pendingConnection);

	if (connectionType == ConnectionTypes::CONNECTION_TYPE_FRUITYMESH)
	{
		GS->node->MeshConnectingTimeoutHandler(bleEvent);
	}

	//Inform modules about all kinds of connection timeouts
	for (int i = 0; i < MAX_MODULE_COUNT; i++)
	{
		if (GS->node->activeModules[i] != 0)
		{
			GS->node->activeModules[i]->ConnectionTimeoutHandler(connectionType, bleEvent);
		}
	}
}

//FIXME: Still needs rewriting
//When a connection changes to encrypted
void ConnectionManager::GapConnectionEncryptedHandler(ble_evt_t* bleEvent)
{
	BaseConnection* c = GetConnectionFromHandle(bleEvent->evt.gap_evt.conn_handle);

	logt("CM", "Connection id %u is now encrypted", c->connectionId);
	c->encryptionState = EncryptionState::ENCRYPTED;

	//If we are the central, we now have to start the handshake
	if (c->direction == CONNECTION_DIRECTION_OUT){
		c->StartHandshake();
	}
}

//Is called whenever a connection had been established and is now disconnected
//due to a timeout, deliberate disconnection by the localhost, remote, etc,...
//We might however decide to sustain it. it will only be lost after
//the finalDisconnectionHander is called
void ConnectionManager::GapConnectionDisconnectedHandler(ble_evt_t* bleEvent)
{
	logt("CM", "Connection handle %u disconnected", bleEvent->evt.gap_evt.conn_handle);

	BaseConnection* connection = GetConnectionFromHandle(bleEvent->evt.gap_evt.conn_handle);

	if (connection == NULL)
		return;

	//Save disconnction reason
	connection->disconnectionReason = bleEvent->evt.gap_evt.params.disconnected.reason;

	//Check if the connection should be sustained
	//(e.g. If it's been handshaked for more than 7 seconds and ran into a timeout)
	if (connection->handshakeDone() && GS->node->appTimerDs - connection->connectionHandshakedTimestampDs > SEC_TO_DS(60)
	//&& bleEvent->evt.gap_evt.params.disconnected.reason == BLE_HCI_CONNECTION_TIMEOUT => problematic since there are multiple reasons, including the ominous BLE_GATTC_EVT_TIMEOUT
	&& connection->reestablishTimeSec != 0
	)
	{
		logt("CM", "Connection should be sustained");

		//Log connection suspension
		Logger::getInstance()->logError(Logger::errorTypes::CUSTOM, Logger::customErrorTypes::TRYING_CONNECTION_SUSTAIN, connection->partnerId);

		//Mark the connection as reestablishing, the state machine of the node
		//will then try to reestablish it.
		connection->disconnectedTimestampDs = GS->node->appTimerDs;
		connection->connectionState = ConnectionState::CONNECTION_STATE_REESTABLISHING;
	}
	//Connection will not be sustained
	else
	{
		FinalDisconnectionHandler(connection);
	}
}

//Is called when a connection is closed after
void ConnectionManager::FinalDisconnectionHandler(BaseConnection* connection)
{
	Logger::getInstance()->logError(Logger::errorTypes::CUSTOM, Logger::customErrorTypes::FINAL_DISCONNECTION, connection->partnerId);

	//LOG disconnection reason
	Logger::getInstance()->logError(Logger::errorTypes::HCI_ERROR, connection->disconnectionReason, connection->partnerId);

	logt("CM", "Connection %u to %u DISCONNECTED: %s", connection->connectionId, connection->partnerId, Logger::getInstance()->getHciErrorString(connection->disconnectionReason));

	//If this was the pending connection, we clear it
	if (pendingConnection == connection)
		pendingConnection = NULL;

	//Notify the connection itself
	connection->DisconnectionHandler();

	//Inform node about mesh disconnections
	if (connection->connectionType == ConnectionTypes::CONNECTION_TYPE_FRUITYMESH)
	{
		GS->node->MeshConnectionDisconnectedHandler((MeshConnection*)connection);
	}

	//Inform modules about all kinds of disconnections
	for(int i=0; i<MAX_MODULE_COUNT; i++)
	{
		if(GS->node->activeModules[i] != 0)
		{
			GS->node->activeModules[i]->ConnectionDisconnectedHandler((MeshConnection*)connection);
		}
	}

	//Delete the connection
	DeleteConnection(connection);
}

#define _________________HELPERS____________

//TODO: Rename with Mesh..
BaseConnection** ConnectionManager::getFreeConnectionSpot()
{
	for (int i = 0; i < MAX_NUM_CONNECTIONS; i++){
		if (allConnections[i] == NULL)
			return allConnections + i;
	}
	return NULL;
}


//Looks through all connections for the right handle and returns the right one
BaseConnection* ConnectionManager::GetConnectionFromHandle(u16 connectionHandle)
{
	for(u32 i=0; i<MAX_NUM_CONNECTIONS; i++){
		if(allConnections[i] != NULL && allConnections[i]->connectionHandle == connectionHandle){
			return allConnections[i];
		}
	}
	return NULL;
}

BaseConnections ConnectionManager::GetBaseConnections(ConnectionDirection direction){
	BaseConnections fc;
	memset(&fc, 0x00, sizeof(BaseConnections));
	for(u32 i=0; i<MAX_NUM_CONNECTIONS; i++){
		if(allConnections[i] != NULL){
			if(allConnections[i]->direction == direction || direction == ConnectionDirection::CONNECTION_DIRECTION_INVALID){
				fc.connections[fc.count] = allConnections[i];
				fc.count++;
			}
		}
	}
	return fc;
}

MeshConnections ConnectionManager::GetMeshConnections(ConnectionDirection direction){
	MeshConnections fc;
	memset(&fc, 0x00, sizeof(MeshConnections));
	for(u32 i=0; i<MAX_NUM_CONNECTIONS; i++){
		if(allConnections[i] != NULL){
			if(allConnections[i]->connectionType == ConnectionTypes::CONNECTION_TYPE_FRUITYMESH){
				if(allConnections[i]->direction == direction || direction == ConnectionDirection::CONNECTION_DIRECTION_INVALID){
					fc.connections[fc.count] = (MeshConnection*)allConnections[i];
					fc.count++;
				}
			}
		}
	}
	return fc;
}

BaseConnections ConnectionManager::GetConnectionsOfType(ConnectionTypes connectionType, ConnectionDirection direction){
	BaseConnections fc;
	memset(&fc, 0x00, sizeof(BaseConnections));
	for(u32 i=0; i<MAX_NUM_CONNECTIONS; i++){
		if(allConnections[i] != NULL){
			if(allConnections[i]->connectionType == connectionType){
				if(allConnections[i]->direction == direction || direction == ConnectionDirection::CONNECTION_DIRECTION_INVALID){
					fc.connections[fc.count] = allConnections[i];
					fc.count++;
				}
			}
		}
	}
	return fc;
}

//Returns the pending packets of all connection types
u16 ConnectionManager::GetPendingPackets()
{
	u16 pendingPackets = 0;
	for (u32 i = 0; i < MAX_NUM_CONNECTIONS; i++){
		if(allConnections[i] != NULL){
			pendingPackets += allConnections[i]->GetPendingPackets();
		}
	}
	return pendingPackets;
}

//BaseConnection* ConnectionManager::IsConnectionReestablishment(ble_evt_t* bleEvent)
//{
//	//Check if we already have a connection for this peer, identified by its address
//	ble_gap_addr_t* peerAddress = &bleEvent->evt.gap_evt.params.connected.peer_addr;
//	for (int i = 0; i < Config->meshMaxConnections; i++)
//	{
//		if (connections[i]->connectionState == ConnectionState::CONNECTION_STATE_REESTABLISHING)
//		{
//			if (memcmp(&connections[i]->partnerAddress, peerAddress, sizeof(ble_gap_addr_t)) == 0)
//			{
//				logt("CM", "Found existing connection id %u", connections[i]->connectionId);
//				return connections[i];
//			}
//		}
//	}
//	return NULL;
//}

//TODO: Only return mesh connections, check
MeshConnection* ConnectionManager::GetMeshConnectionToShortestSink(MeshConnection* excludeConnection)
{
	clusterSIZE min = INT16_MAX;
	MeshConnection* c = NULL;
	MeshConnections conn = GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);
	for (int i = 0; i < conn.count; i++)
	{
		if (excludeConnection != NULL && conn.connections[i] == excludeConnection)
			continue;
		if (conn.connections[i]->handshakeDone() && conn.connections[i]->hopsToSink > -1 && conn.connections[i]->hopsToSink < min)
		{
			min = conn.connections[i]->hopsToSink;
			c = conn.connections[i];
		}
	}
	return c;
}

clusterSIZE ConnectionManager::GetMeshHopsToShortestSink(MeshConnection* excludeConnection)
{
	if (GS->node->persistentConfig.deviceType == deviceTypes::DEVICE_TYPE_SINK)
	{
		logt("SINK", "HOPS 0, clID:%x, clSize:%d", GS->node->clusterId, GS->node->clusterSize);
		return 0;
	}
	else
	{
		clusterSIZE min = INT16_MAX;
		MeshConnection* c = NULL;
		MeshConnections conn = GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);
		for(int i=0; i<conn.count; i++)
		{
			if(conn.connections[i] == excludeConnection || !conn.connections[i]->isConnected()) continue;
			if(conn.connections[i]->hopsToSink > -1 && conn.connections[i]->hopsToSink < min)
			{
				min = conn.connections[i]->hopsToSink;
				c = conn.connections[i];
			}
		}

		logt("SINK", "HOPS %d, clID:%x, clSize:%d", (c == NULL) ? -1 : c->hopsToSink, GS->node->clusterId, GS->node->clusterSize);
		return (c == NULL) ? -1 : c->hopsToSink;
	}
}

#define _________________EVENTS____________

//Forward ble events to connections
void ConnectionManager::BleEventHandler(ble_evt_t* bleEvent)
{
	for (int i = 0; i < MAX_NUM_CONNECTIONS; i++) {
		if(allConnections[i] != NULL) GS->cm->allConnections[i]->BleEventHandler(bleEvent);
	}
}

void ConnectionManager::TimerEventHandler(u16 passedTimeDs, u32 appTimerDs)
{
	//Disconnect ResolverConnections that have exceeded their handshake timeout
	BaseConnections conns = GetConnectionsOfType(ConnectionTypes::CONNECTION_TYPE_RESOLVER, ConnectionDirection::CONNECTION_DIRECTION_INVALID);
	if(conns.count > 0){
		if(conns.connections[0]->connectionState == ConnectionState::CONNECTION_STATE_HANDSHAKING && conns.connections[0]->handshakeStartedDs + Config->meshHandshakeTimeoutDs <= GS->node->appTimerDs){
			conns.connections[0]->Disconnect();
		}
	}
}

