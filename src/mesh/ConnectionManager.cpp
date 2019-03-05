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
#include <types.h>
#include <Node.h>
#include <ConnectionManager.h>
#include <AdvertisingController.h>
#include <MeshAccessConnection.h>
#include <GATTController.h>
#include <GAPController.h>
#include <ResolverConnection.h>
#include <StatusReporterModule.h>
#include <Utility.h>
#include <Logger.h>

#ifdef ACTIVATE_MA_MODULE
#include <MeshAccessModule.h>
#endif

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

	pendingConnection = nullptr;
	uniqueConnectionIdCounter = 0;
	droppedMeshPackets = 0;
	sentMeshPackets = 0;

	memset(allConnections, 0x00, sizeof(allConnections));

	//Register GAPController callbacks
	GS->gapController->setGAPControllerHandler(this);

	//Set GATTController callbacks
	GS->gattController->setGATTControllerHandler(this);

}

#define _______________CONNECTIVITY______________

//TODO: Do we route messages to mesh connections or to all connections????
//probably check the destinationId, if the id is within the range of apps, it should be routed
//if the id is for the mesh range, a Module could decide to grab the message and send it to its
//App connections as well

//Checks the receiver of the message first and routes it in the right direction

//TODO: Rename to mesh... we need another implementation
//Connects to a peripheral as Master, writecharacteristichandle can be BLE_GATT_HANDLE_INVALID
void ConnectionManager::ConnectAsMaster(NodeId partnerId, fh_ble_gap_addr_t* address, u16 writeCharacteristicHandle, u16 connectionIv)
{
	//Only connect when not currently in another connection or when there are no more free connections
	if (freeMeshOutConnections < 1 || pendingConnection != nullptr) return;

	//Tell the GAP Layer to connect, it will return if it is trying or if there was an error
	u32 err = GS->gapController->connectToPeripheral(*address, connectionIv, Config->meshConnectingScanTimeout);

	char addrString[20];
	GS->logger->convertBufferToHexString(address->addr, 6, addrString, sizeof(addrString));
	logt("CONN", "Connecting as Master to %d (%02X:%02X:%02X:%02X:%02X:%02X) %s",
		partnerId,
		address->addr[5],
		address->addr[4],
		address->addr[3],
		address->addr[2],
		address->addr[1],
		address->addr[0],
		err == NRF_SUCCESS ? "true" : "false");

	if (err == NRF_SUCCESS)
	{
		StatusReporterModule* statusMod = (StatusReporterModule*)GS->node->GetModuleById(moduleID::STATUS_REPORTER_MODULE_ID);
		if(statusMod != nullptr){
			u32 addrPart;
			memcpy(&addrPart, address->addr, 4);
			statusMod->SendLiveReport(LiveReportTypes::GAP_TRYING_AS_MASTER, partnerId, addrPart);
		}

		//Create the connection and set it as pending
		for (u32 i = 0; i < MAX_NUM_CONNECTIONS; i++){
			if (allConnections[i] == nullptr){
				pendingConnection = allConnections[i] = new MeshConnection(i, ConnectionDirection::DIRECTION_OUT, address, writeCharacteristicHandle);
				break;
			}
		}

	} else {
		GS->logger->logError(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::CONNECT_AS_MASTER_NOT_POSSIBLE, err);
	}
}

void ConnectionManager::DeleteConnection(BaseConnection* connection){
	if(connection == nullptr) return;

	logt("CM", "Cleaning up conn %u", connection->connectionId);

	for(u32 i=0; i<MAX_NUM_CONNECTIONS; i++){
		if(connection == allConnections[i]){
			delete connection;
			allConnections[i] = nullptr;
		}
	}
	if(pendingConnection == connection){
		pendingConnection = nullptr;
	}

	//Update Join me packet after connection was deleted so we have another free one
	GS->node->UpdateJoinMePacket();
}

//TODO: Mesh specific
//Disconnects either all connections or all except one
//Cluster updates from this connection should be ignored
void ConnectionManager::ForceDisconnectOtherMeshConnections(const MeshConnection* ignoreConnection, AppDisconnectReason appDisconnectReason) const
{
	//We can not use GetConnections here as a disconnection of a connection might trigger another force disconnect method.
	for (int i = 0; i < MAX_NUM_CONNECTIONS; i++) {
		BaseConnection* conn = allConnections[i];
		if (conn != nullptr && conn != ignoreConnection && conn->connectionType == ConnectionTypes::CONNECTION_TYPE_FRUITYMESH) {
			conn->appDisconnectionReason = appDisconnectReason;
			conn->DisconnectAndRemove();
		}
	}
}

void ConnectionManager::ForceDisconnectAllConnections(AppDisconnectReason appDisconnectReason) const
{
	//We can not use GetConnections here as a disconnection of a connection might trigger another force disconnect method.
	for (int i = 0; i < MAX_NUM_CONNECTIONS; i++) {
		BaseConnection* conn = allConnections[i];
		if (conn != nullptr) {
			conn->appDisconnectionReason = appDisconnectReason;
			conn->DisconnectAndRemove();
		}
	}
}

//Changes the connection interval of all mesh connections
void ConnectionManager::SetMeshConnectionInterval(u16 connectionInterval) const
{
	//Go through all connections that we control as a central
	MeshConnections conn = GetMeshConnections(ConnectionDirection::DIRECTION_OUT);
	for(u32 i=0; i< conn.count; i++){
		if (conn.connections[i]->handshakeDone()){
			GS->gapController->RequestConnectionParameterUpdate(conn.connections[i]->connectionHandle, connectionInterval, connectionInterval, 0, Config->meshConnectionSupervisionTimeout);
		}
	}
}

void ConnectionManager::GATTServiceDiscoveredHandler(u16 connHandle, ble_db_discovery_evt_t& evt)
{
	//Find the connection that was discovering services and inform it
	BaseConnection* conn = GetConnectionFromHandle(connHandle);
	if(conn != nullptr){
		conn->GATTServiceDiscoveredHandler(evt);
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

	logt("RCONN", "numConnTypeResolvers %u", numConnTypeResolvers);

	//Check if any resolver matches the received data
	for(int i=0; i<numConnTypeResolvers; i++){
		if(resolvers[i] == nullptr) break;

		BaseConnection* newConnection = resolvers[i](oldConnection, sendData, data);

		//If the resolver found a suitable connection upgrade, find the connection reference and replace
		//it with a new instance of our upgraded connection
		if(newConnection != nullptr){
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

void ConnectionManager::SendMeshMessage(u8* data, u8 dataLength, DeliveryPriority priority, bool reliable) const
{
	SendMeshMessage(data, dataLength, priority, reliable, true);
}

void ConnectionManager::SendMeshMessage(u8* data, u8 dataLength, DeliveryPriority priority, bool reliable, bool loopback) const
{
	connPacketHeader* packetHeader = (connPacketHeader*) data;
#ifdef SIM_ENABLED
	if (packetHeader->sender != GS->node->configuration.nodeId && packetHeader->sender != NODE_ID_SHORTEST_SINK)
	{
		SIMEXCEPTION(IllegalSenderException);
	}
#endif
	// ########################## Local Loopback
	if(loopback){
		//Build fake data for our loopback packet
		BaseConnectionSendData sendData;
		sendData.characteristicHandle = BLE_CONN_HANDLE_INVALID;
		sendData.dataLength = dataLength;
		sendData.deliveryOption = reliable ? DeliveryOption::WRITE_REQ : DeliveryOption::WRITE_CMD;

		//This method will dispatch it if the receiver matches this nodeId
		//TODO: Maybe we should pass some local loopback connection reference so that further calls do not operate on nullptr
		DispatchMeshMessage(nullptr, &sendData, (connPacketHeader*)data, true);
	}

	// ########################## Routing to MeshAccess Connections

#ifdef ACTIVATE_MA_MODULE
	//We send the packet to all MeshAccessConnections because any receiverId could be on the other side
	BaseConnections maConn = GetConnectionsOfType(ConnectionTypes::CONNECTION_TYPE_MESH_ACCESS, ConnectionDirection::INVALID);
	for(u32 i=0; i< maConn.count; i++){
		MeshAccessConnection* c = (MeshAccessConnection*)allConnections[maConn.connectionIndizes[i]];
		if (c == nullptr) {
			continue;
		}
		c->SendData(data, dataLength, priority, reliable);
	}
#endif


	// ########################## Sink Routing
	//Packets to the shortest sink, can only be sent to mesh partners
	if (packetHeader->receiver == NODE_ID_SHORTEST_SINK)
	{
		BroadcastMeshPacket(data, dataLength, priority, reliable);

		//TODO: Implement sink routing
//		MeshConnection* dest = GetMeshConnectionToShortestSink(nullptr);
//
//		//FIXME: Packets are currently only delivered if a sink is known
//		if (dest)
//		{
//			dest->SendData(data, dataLength, priority, reliable);
//		}
	}
	else if(packetHeader->receiver == NODE_ID_LOCAL_LOOPBACK)
	{
		// No further routing needed, only for our node
	}
	// ########################## App Specific packets
	// Packets specific to Apps (Like a broadcast channel for specific apps
	else if (packetHeader->receiver > NODE_ID_APP_BASE && packetHeader->receiver < NODE_ID_APP_BASE + NODE_ID_APP_BASE_SIZE)
	{
		//TODO: Broadcast to all nodes, also call the MessageReceivedHandler for AppConnections
	}
	// ########################## Generic broadcast packets
	//All other packets will be broadcasted, but we check if the receiver is connected to us
	else if (packetHeader->receiver != GS->node->configuration.nodeId)
	{
		//Check if the receiver has a handshaked connection with us
		MeshConnection* receiverConn = nullptr;
		MeshConnections conn = GetMeshConnections(ConnectionDirection::INVALID);
		for(u32 i=0; i< conn.count; i++){
			if(conn.connections[i]->handshakeDone() && conn.connections[i]->partnerId == packetHeader->receiver){
				receiverConn = conn.connections[i];
				break;
			}
		}

		//Send to receiver or broadcast if not directly connected to us
		if(receiverConn != nullptr){
			receiverConn->SendData(data, dataLength, priority, reliable);
		} else {
			BroadcastMeshPacket(data, dataLength, priority, reliable);
		}
	}
}

void ConnectionManager::DispatchMeshMessage(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packet, bool checkReceiver) const
{
	//Check if we are part of the firmware group that should receive this image
	bool fwGroupIdMatches = false;
	for(u32 i=0; i<MAX_NUM_FW_GROUP_IDS; i++){
		if(GS->config->fwGroupIds != 0 && GS->config->fwGroupIds[i] == packet->receiver) fwGroupIdMatches = true;
	}

	if(
		!checkReceiver
		|| packet->receiver == GS->node->configuration.nodeId //Directly addressed at us
		|| packet->receiver == NODE_ID_BROADCAST //broadcast packet for all nodes
		|| (packet->receiver >= NODE_ID_HOPS_BASE && packet->receiver < NODE_ID_HOPS_BASE + 1000) //Broadcasted for a number of hops
		|| (packet->receiver == NODE_ID_SHORTEST_SINK && GS->node->configuration.deviceType == deviceTypes::DEVICE_TYPE_SINK)
		|| fwGroupIdMatches
	){
		//Fix local loopback id and replace with out nodeId
		if(packet->receiver == NODE_ID_LOCAL_LOOPBACK) packet->receiver = GS->node->configuration.nodeId;

		//Now we must pass the message to all of our modules for further processing
		for(int i=0; i<MAX_MODULE_COUNT; i++){
			if(GS->activeModules[i] != nullptr && GS->activeModules[i]->configurationPointer->moduleActive){
				GS->activeModules[i]->MeshMessageReceivedHandler(connection, sendData, packet);
			}
		}
	}
}

//A helper method for sending moduleAction messages
void ConnectionManager::SendModuleActionMessage(u8 messageType, u8 moduleId, NodeId toNode, u8 actionType, u8 requestHandle, const u8* additionalData, u16 additionalDataSize, bool reliable) const
{
	DYNAMIC_ARRAY(buffer, SIZEOF_CONN_PACKET_MODULE + additionalDataSize);

	connPacketModule* outPacket = (connPacketModule*)buffer;
	outPacket->header.messageType = messageType;
	outPacket->header.sender = GS->node->configuration.nodeId;
	outPacket->header.receiver = toNode;

	outPacket->moduleId = moduleId;
	outPacket->requestHandle = requestHandle;
	outPacket->actionType = actionType;

	if(additionalData != nullptr && additionalDataSize > 0)
	{
		memcpy(&outPacket->data, additionalData, additionalDataSize);
	}

	GS->cm->SendMeshMessage(buffer, SIZEOF_CONN_PACKET_MODULE + additionalDataSize, DeliveryPriority::LOW, reliable);
}

void ConnectionManager::BroadcastMeshPacket(u8* data, u8 dataLength, DeliveryPriority priority, bool reliable) const
{
	MeshConnections conn = GetMeshConnections(ConnectionDirection::INVALID);
	for(u32 i=0; i< conn.count; i++){
		conn.connections[i]->SendData(data, dataLength, priority, reliable);
	}
}

void ConnectionManager::fillTransmitBuffers() const
{
	BaseConnections conn = GetBaseConnections(ConnectionDirection::INVALID);
	for(u32 i=0; i< conn.count; i++){
		BaseConnection* bc = allConnections[conn.connectionIndizes[i]];
		if (bc != nullptr) {
			bc->FillTransmitBuffers();
		}
	}
}

//FIXME: Some rewrite of this method necessary
void ConnectionManager::GATTDataTransmittedHandler(ble_evt_t &bleEvent)
{
	ConnectionManager* cm = GS->cm;
	//There are two types of events that trigger a dataTransmittedCallback
	//A TX complete event frees a number of transmit buffers
	//These are used for all connections


	u16 txCompleteConnectionHandle = BLE_CONN_HANDLE_INVALID;
	u8 txCompleteCount = 0;

#if defined(NRF51) || defined(SIM_ENABLED)
	if(bleEvent.header.evt_id == BLE_EVT_TX_COMPLETE){
		txCompleteConnectionHandle = bleEvent.evt.common_evt.conn_handle;
		txCompleteCount = bleEvent.evt.common_evt.params.tx_complete.count;
	}
#elif defined(NRF52)
	if(bleEvent.header.evt_id == BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE){
		txCompleteConnectionHandle = bleEvent.evt.gattc_evt.conn_handle;
		txCompleteCount = bleEvent.evt.gattc_evt.params.write_cmd_tx_complete.count;
	} else if (bleEvent.header.evt_id == BLE_GATTS_EVT_HVN_TX_COMPLETE){
		txCompleteConnectionHandle = bleEvent.evt.gatts_evt.conn_handle;
		txCompleteCount = bleEvent.evt.gatts_evt.params.hvn_tx_complete.count;
	}
#endif


	if (txCompleteConnectionHandle != BLE_CONN_HANDLE_INVALID)
	{
		logt("CONN_DATA", "write_CMD complete (n=%d)", txCompleteCount);

		//This connection has just been given back some transmit buffers
		BaseConnection* connection = cm->GetConnectionFromHandle(txCompleteConnectionHandle);
		if (connection == nullptr) return;

		connection->HandlePacketSent(txCompleteCount, 0);

		sentMeshPackets += txCompleteCount;

		for (u32 i = 0; i < txCompleteCount; i++) {
			GS->logger->logCount(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_SENT_PACKETS_UNRELIABLE);
		}

		//Next, we should continue sending packets if there are any
		if (cm->GetPendingPackets())
			cm->fillTransmitBuffers();

	}
	//The EVT_WRITE_RSP comes after a WRITE_REQ and notifies that a buffer
	//for one specific connection has been cleared
	else if (bleEvent.header.evt_id == BLE_GATTC_EVT_WRITE_RSP)
	{
		if (bleEvent.evt.gattc_evt.gatt_status != BLE_GATT_STATUS_SUCCESS)
		{
			logt("ERROR", "GATT status problem %d %s", bleEvent.evt.gattc_evt.gatt_status, GS->logger->getGattStatusErrorString(bleEvent.evt.gattc_evt.gatt_status));


			GS->logger->logCount(ErrorTypes::GATT_STATUS, bleEvent.evt.gattc_evt.gatt_status);

			//TODO: Error handling, but there really shouldn't be an error....;-)
			//FIXME: Handle possible gatt status codes

		}
		else
		{
			logt("CONN_DATA", "write_REQ complete");
			BaseConnection* connection = cm->GetConnectionFromHandle(bleEvent.evt.gattc_evt.conn_handle);

			//Connection could have been disconneced
			if (connection == nullptr) return;

			connection->HandlePacketSent(0, 1);

			sentMeshPackets++;

			GS->logger->logCount(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_SENT_PACKETS_RELIABLE);

			//Now we continue sending packets
			if (cm->GetPendingPackets())
				cm->fillTransmitBuffers();
		}
	}
}

#define _________________RECEIVING____________

void ConnectionManager::GattDataReceivedHandler(ble_evt_t &bleEvent)
{
	BaseConnectionSendData sendData;
	u8* data;

	if(bleEvent.header.evt_id == BLE_GATTS_EVT_WRITE){
		ble_gatts_evt_write_t* writeEvent = (ble_gatts_evt_write_t*) &bleEvent.evt.gatts_evt.params.write;
		connPacketHeader* packetHeader = (connPacketHeader*)writeEvent->data;

		sendData.characteristicHandle = writeEvent->handle;
		sendData.deliveryOption = (writeEvent->op == BLE_GATTS_OP_WRITE_REQ) ? DeliveryOption::WRITE_REQ : DeliveryOption::WRITE_CMD;
		sendData.priority = DeliveryPriority::LOW; //TODO: Prio is unknown, should we send it in the packet?
		sendData.dataLength = (u8)writeEvent->len;
		data = writeEvent->data;

	} else if(bleEvent.header.evt_id == BLE_GATTC_EVT_HVX){
		ble_gattc_evt_hvx_t* hvxEvent = (ble_gattc_evt_hvx_t*) &bleEvent.evt.gattc_evt.params.hvx;
		connPacketHeader* packetHeader = (connPacketHeader*)hvxEvent->data;

		sendData.characteristicHandle = hvxEvent->handle;
		sendData.deliveryOption = DeliveryOption::NOTIFICATION;
		sendData.priority = DeliveryPriority::LOW; //TODO: Prio is unknown, should we send it in the packet?
		sendData.dataLength = (u8)hvxEvent->len;
		data = hvxEvent->data;
	} else {
		return;
	}

	logt("CM", "RX Data size is: %d, handles(%d, %d), delivery %d", sendData.dataLength, bleEvent.evt.gatts_evt.conn_handle, sendData.characteristicHandle, sendData.deliveryOption);

	char stringBuffer[100];
	GS->logger->convertBufferToHexString(data, sendData.dataLength, stringBuffer, sizeof(stringBuffer));
	logt("CM", "%s", stringBuffer);

	//Get the handling connection for this write
	u16 connectionHandle = bleEvent.evt.gatts_evt.conn_handle;
	BaseConnection* connection = GS->cm->GetConnectionFromHandle(connectionHandle);

	//Notify our connection instance that data has been received
	if (connection != nullptr){
		connection->ReceiveDataHandler(&sendData, data);
	}

}

//This method accepts connPackets and distributes it to all other mesh connections
void ConnectionManager::RouteMeshData(BaseConnection* connection, BaseConnectionSendData* sendData, u8* data) const
{
	connPacketHeader* packetHeader = (connPacketHeader*) data;


	/*#################### Modification ############################*/


	/*#################### ROUTING ############################*/

	//We are the last receiver for this packet
	if (packetHeader->receiver == GS->node->configuration.nodeId //We are the receiver
	|| packetHeader->receiver == NODE_ID_HOPS_BASE + 1//The packet was meant to travel only one hop
	|| (packetHeader->receiver == NODE_ID_SHORTEST_SINK && GS->node->configuration.deviceType == deviceTypes::DEVICE_TYPE_SINK)//Packet was meant for the shortest sink and we are a sink
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

//		MeshConnection* connection = GS->cm->GetMeshConnectionToShortestSink(nullptr);
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

void ConnectionManager::BroadcastMeshData(const BaseConnection* ignoreConnection, BaseConnectionSendData* sendData, u8* data) const
{
	//Iterate through all mesh connections except the ignored one and send the packet
	MeshConnections conn = GetMeshConnections(ConnectionDirection::INVALID);
	for(u32 i=0; i< conn.count; i++){
		if (conn.connections[i] != ignoreConnection){
			sendData->characteristicHandle = conn.connections[i]->partnerWriteCharacteristicHandle;
			conn.connections[i]->SendData(sendData, data);
		}
	}

	//Route to all MeshAccess Connections
#ifdef ACTIVATE_MA_MODULE
	//Iterate through all mesh access connetions except the ignored one and send the packet
	BaseConnections conn2 = GetConnectionsOfType(ConnectionTypes::CONNECTION_TYPE_MESH_ACCESS, ConnectionDirection::INVALID);
	for(u32 i=0; i< conn2.count; i++){
		MeshAccessConnection* maconn = (MeshAccessConnection*)allConnections[conn2.connectionIndizes[i]];
		if (maconn != nullptr && maconn != ignoreConnection){
			maconn->SendData(data, sendData->dataLength, sendData->priority, false);
		}
	}
#endif
}

#define _________________CONNECTIONS____________

//Called as soon as a new connection is made, either as central or peripheral
void ConnectionManager::GapConnectionConnectedHandler(ble_evt_t& bleEvent)
{
	u32 err;

	ble_gap_evt_connected_t* connectedEvent = &bleEvent.evt.gap_evt.params.connected;

	StatusReporterModule* statusMod = (StatusReporterModule*)GS->node->GetModuleById(moduleID::STATUS_REPORTER_MODULE_ID);
	if(statusMod != nullptr){
		u32 addrPart;
		memcpy(&addrPart, connectedEvent->peer_addr.addr, 4);

		if(connectedEvent->role == BLE_GAP_ROLE_PERIPH){
			statusMod->SendLiveReport(LiveReportTypes::GAP_CONNECTED_INCOMING, bleEvent.evt.gap_evt.conn_handle, addrPart);
		} else if(connectedEvent->role == BLE_GAP_ROLE_CENTRAL){
			statusMod->SendLiveReport(LiveReportTypes::GAP_CONNECTED_OUTGOING, bleEvent.evt.gap_evt.conn_handle, addrPart);
		}
	}


	logt("CM", "Connection handle %u success as %s, partner:%02x:%02x:%02x:%02x:%02x:%02x", bleEvent.evt.gap_evt.conn_handle, bleEvent.evt.gap_evt.params.connected.role == BLE_GAP_ROLE_CENTRAL ? "Central" : "Peripheral", bleEvent.evt.gap_evt.params.connected.peer_addr.addr[5], bleEvent.evt.gap_evt.params.connected.peer_addr.addr[4], bleEvent.evt.gap_evt.params.connected.peer_addr.addr[3], bleEvent.evt.gap_evt.params.connected.peer_addr.addr[2], bleEvent.evt.gap_evt.params.connected.peer_addr.addr[1], bleEvent.evt.gap_evt.params.connected.peer_addr.addr[0]);

	GS->logger->logCount(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_CONNECTION_SUCCESS);

	BaseConnection* reestablishedConnection = IsConnectionReestablishment(bleEvent);

	/* TODO: Part A: We have a connection reestablishment */
	if (reestablishedConnection != nullptr)
	{
		reestablishedConnection->ReconnectionSuccessfulHandler(bleEvent);

		//Check if there is another connection in reestablishing state that we can try to reconnect
		MeshConnections conns = GetMeshConnections(ConnectionDirection::DIRECTION_OUT);
		for (u32 i = 0; i < conns.count; i++) {
			if (conns.connections[i]->connectionState == ConnectionState::REESTABLISHING) {
				conns.connections[i]->TryReestablishing();
			}
		}

		if (bleEvent.evt.gap_evt.params.connected.role == BLE_GAP_ROLE_PERIPH)
		{
			//The Peripheral should wait until the encryption request was made
			reestablishedConnection->encryptionState = EncryptionState::NOT_ENCRYPTED;
		}
		else if (bleEvent.evt.gap_evt.params.connected.role == BLE_GAP_ROLE_CENTRAL)
		{
			//If encryption is enabled, the central starts to encrypt the connection
			reestablishedConnection->encryptionState = EncryptionState::ENCRYPTING;
			GS->gapController->startEncryptingConnection(bleEvent.evt.gap_evt.conn_handle);
		}

		return;
	}

	/* Part B: A normal incoming/outgoing connection */
	if (GetConnectionInHandshakeState() != nullptr)
	{
		logt("CM", "Currently in handshake, disconnect");

		//If we have a pendingConnection for this, we must clean it
		if(bleEvent.evt.gap_evt.params.connected.role == BLE_GAP_ROLE_CENTRAL){
			if(pendingConnection != nullptr){
				pendingConnection->appDisconnectionReason = AppDisconnectReason::CURRENTLY_IN_HANDSHAKE;
				DeleteConnection(pendingConnection);
			}
		}
		err = FruityHal::Disconnect(bleEvent.evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);

		return;
	}

	BaseConnection* c = nullptr;

	//We are slave (peripheral)
	if (bleEvent.evt.gap_evt.params.connected.role == BLE_GAP_ROLE_PERIPH)
	{
		logt("CM", "Incoming Connection connected");

		//Check if we have a free entry in our connections array
		i8 id = getFreeConnectionSpot();
		if(id < 0){
			SIMERROR();
			logt("ERROR", "Fail: No spot");
			
			//Critical error, drop all connections
			GS->logger->logCount(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_CM_FAIL_NO_SPOT);
			ForceDisconnectAllConnections(AppDisconnectReason::CM_FAIL_NO_SPOT);
		}



		fh_ble_gap_addr_t peerAddress = FruityHal::Convert(&connectedEvent->peer_addr);

		c = allConnections[id] = new ResolverConnection(id, ConnectionDirection::DIRECTION_IN, &peerAddress);
		c->ConnectionSuccessfulHandler(bleEvent.evt.gap_evt.conn_handle, bleEvent.evt.gap_evt.params.connected.conn_params.min_conn_interval);


		//The central may now start encrypting or start the handshake, we just have to wait
	}
	//We are master (central)
	else if (bleEvent.evt.gap_evt.params.connected.role == BLE_GAP_ROLE_CENTRAL)
	{
		//This can happen if the connection has been cleaned up already e.g. by disconnecting all connections but the connection was accepted in the meantime
		if(pendingConnection == nullptr){
			logt("ERROR", "No pending Connection");
			GS->logger->logCount(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_NO_PENDING_CONNECTION);
			err = FruityHal::Disconnect(bleEvent.evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
			return;
		}

		c = pendingConnection;
		pendingConnection = nullptr;

		//Call Prepare again so that the clusterID and size backup are created with up to date values
		c->ConnectionSuccessfulHandler(bleEvent.evt.gap_evt.conn_handle, bleEvent.evt.gap_evt.params.connected.conn_params.min_conn_interval);

		//If encryption is enabled, the central starts to encrypt the connection
		if (Config->encryptionEnabled && c->connectionType == ConnectionTypes::CONNECTION_TYPE_FRUITYMESH){
			c->encryptionState = EncryptionState::ENCRYPTING;
			GS->gapController->startEncryptingConnection(bleEvent.evt.gap_evt.conn_handle);
		}
		//Without encryption we immediately start the handshake
		else {
			c->StartHandshake();
		}
	}
}

//If we wanted to connect but our connection timed out (only outgoing connections)
void ConnectionManager::GapConnectingTimeoutHandler(ble_evt_t& bleEvent)
{
	if (pendingConnection == nullptr)
		return;

	//Save connection type, but clear pending connection, because the handlers might try to
	//create a new connection immediately
	ConnectionTypes connectionType = pendingConnection->connectionType;

	pendingConnection->appDisconnectionReason = AppDisconnectReason::GAP_CONNECTING_TIMEOUT;
	DeleteConnection(pendingConnection);

	//Inform modules about all kinds of connection timeouts
	for (int i = 0; i < MAX_MODULE_COUNT; i++)
	{
		if (GS->activeModules[i] != nullptr && GS->activeModules[i]->configurationPointer->moduleActive)
		{
			GS->activeModules[i]->ConnectionTimeoutHandler(connectionType, &bleEvent);
		}
	}
}

//FIXME: Still needs rewriting
//When a connection changes to encrypted
void ConnectionManager::GapConnectionEncryptedHandler(ble_evt_t& bleEvent)
{
	BaseConnection* c = GetConnectionFromHandle(bleEvent.evt.gap_evt.conn_handle);

	if (c == nullptr) return; //Connection might have been disconnected alread

	logt("CM", "Connection id %u is now encrypted", c->connectionId);
	c->encryptionState = EncryptionState::ENCRYPTED;

	//If we are reestablishing, initiate the reestablishing handshake
	if(c->connectionState == ConnectionState::REESTABLISHING_HANDSHAKE){
		((MeshConnection*)c)->SendReconnectionHandshakePacket();
	}
	//Otherwise, we start the normal mesh handshake, but only if we are central
	else if (c->connectionState == ConnectionState::CONNECTED && c->direction == ConnectionDirection::DIRECTION_OUT){
		c->StartHandshake();
	}
}

//Is called whenever a connection had been established and is now disconnected
//due to a timeout, deliberate disconnection by the localhost, remote, etc,...
//We might however decide to sustain it. it will only be lost after
//the finalDisconnectionHander is called
void ConnectionManager::GapConnectionDisconnectedHandler(ble_evt_t& bleEvent)
{
	StatusReporterModule* statusMod = (StatusReporterModule*)GS->node->GetModuleById(moduleID::STATUS_REPORTER_MODULE_ID);
	if(statusMod != nullptr){
		statusMod->SendLiveReport(LiveReportTypes::GAP_DISCONNECTED, bleEvent.evt.gap_evt.conn_handle, bleEvent.evt.gap_evt.params.disconnected.reason);
	}
	BaseConnection* connection = GetConnectionFromHandle(bleEvent.evt.gap_evt.conn_handle);

	if(connection == nullptr) return;

	logt("CM", "Gap Connection handle %u disconnected", bleEvent.evt.gap_evt.conn_handle);

	GS->logger->logCount(ErrorTypes::HCI_ERROR, bleEvent.evt.gap_evt.params.disconnected.reason);

	//Notify the connection itself
	bool result = connection->GapDisconnectionHandler(bleEvent.evt.gap_evt.params.disconnected.reason);

	//The connection can be disconnected
	if(result){
		logt("WARNING", "Final Disconnect");
		connection->appDisconnectionReason = AppDisconnectReason::GAP_DISCONNECT_NO_REESTABLISH_REQUESTED;
		connection->DisconnectAndRemove();
	}
	// The connection will try to reconnect
	else
	{

	}
}

#define _________________HELPERS____________

i8 ConnectionManager::getFreeConnectionSpot() const
{
	for (int i = 0; i < MAX_NUM_CONNECTIONS; i++){
		if (allConnections[i] == nullptr)
			return i;
	}
	return -1;
}


//Looks through all connections for the right handle and returns the right one
BaseConnection* ConnectionManager::GetConnectionFromHandle(u16 connectionHandle) const
{
	for(u32 i=0; i<MAX_NUM_CONNECTIONS; i++){
		if(allConnections[i] != nullptr && allConnections[i]->connectionHandle == connectionHandle){
			return allConnections[i];
		}
	}
	return nullptr;
}

//Looks through all connections for the right handle and returns the right one
BaseConnection* ConnectionManager::GetConnectionByUniqueId(u16 uniqueConnectionId) const
{
	for(u32 i=0; i<MAX_NUM_CONNECTIONS; i++){
		if(allConnections[i] != nullptr && allConnections[i]->uniqueConnectionId == uniqueConnectionId){
			return allConnections[i];
		}
	}
	return nullptr;
}

BaseConnections ConnectionManager::GetBaseConnections(ConnectionDirection direction) const{
	BaseConnections fc;
	memset(&fc, 0x00, sizeof(BaseConnections));
	for(u32 i=0; i<MAX_NUM_CONNECTIONS; i++){
		if(allConnections[i] != nullptr){
			if(allConnections[i]->direction == direction || direction == ConnectionDirection::INVALID){
				fc.connectionIndizes[fc.count] = i;
				fc.count++;
			}
		}
	}
	return fc;
}

MeshConnections ConnectionManager::GetMeshConnections(ConnectionDirection direction) const{
	MeshConnections fc;
	memset(&fc, 0x00, sizeof(MeshConnections));
	for(u32 i=0; i<MAX_NUM_CONNECTIONS; i++){
		if(allConnections[i] != nullptr){
			if(allConnections[i]->connectionType == ConnectionTypes::CONNECTION_TYPE_FRUITYMESH){
				if(allConnections[i]->direction == direction || direction == ConnectionDirection::INVALID){
					fc.connections[fc.count] = (MeshConnection*)allConnections[i];
					fc.count++;
				}
			}
		}
	}
	return fc;
}

MeshConnection* ConnectionManager::GetConnectionInHandshakeState() const
{

	for(u32 i=0; i<MAX_NUM_CONNECTIONS; i++){
		if(allConnections[i] != nullptr && allConnections[i]->connectionType == ConnectionTypes::CONNECTION_TYPE_FRUITYMESH){
			if(allConnections[i]->connectionState == ConnectionState::HANDSHAKING){
				return (MeshConnection*)allConnections[i];
			}
		}
	}
	return nullptr;
}

BaseConnections ConnectionManager::GetConnectionsOfType(ConnectionTypes connectionType, ConnectionDirection direction) const{
	BaseConnections fc;
	memset(&fc, 0x00, sizeof(BaseConnections));
	for(u32 i=0; i<MAX_NUM_CONNECTIONS; i++){
		if(allConnections[i] != nullptr){
			if(allConnections[i]->connectionType == connectionType || connectionType == ConnectionTypes::CONNECTION_TYPE_INVALID){
				if(allConnections[i]->direction == direction || direction == ConnectionDirection::INVALID){
					fc.connectionIndizes[fc.count] = i;
					fc.count++;
				}
			}
		}
	}
	return fc;
}

//Returns the pending packets of all connection types
u16 ConnectionManager::GetPendingPackets() const
{
	u16 pendingPackets = 0;
	for (u32 i = 0; i < MAX_NUM_CONNECTIONS; i++){
		if(allConnections[i] != nullptr){
			pendingPackets += allConnections[i]->GetPendingPackets();
		}
	}
	return pendingPackets;
}

BaseConnection* ConnectionManager::IsConnectionReestablishment(const ble_evt_t& bleEvent) const
{
	//Check if we already have a connection for this peer, identified by its address
	fh_ble_gap_addr_t peerAddress = FruityHal::Convert(&bleEvent.evt.gap_evt.params.connected.peer_addr);
	for (int i = 0; i < Config->meshMaxConnections; i++)
	{
		if (allConnections[i] != nullptr && allConnections[i]->connectionState == ConnectionState::REESTABLISHING)
		{
			if (memcmp(&allConnections[i]->partnerAddress, &peerAddress, sizeof(fh_ble_gap_addr_t)) == 0)
			{
				logt("CM", "Found existing connection id %u", allConnections[i]->connectionId);
				return allConnections[i];
			}
		}
	}
	return nullptr;
}

//TODO: Only return mesh connections, check
MeshConnection* ConnectionManager::GetMeshConnectionToShortestSink(const MeshConnection* excludeConnection) const
{
	ClusterSize min = INT16_MAX;
	MeshConnection* c = nullptr;
	MeshConnections conn = GetMeshConnections(ConnectionDirection::INVALID);
	for (int i = 0; i < conn.count; i++)
	{
		if (excludeConnection != nullptr && conn.connections[i] == excludeConnection)
			continue;
		if (conn.connections[i]->handshakeDone() && conn.connections[i]->hopsToSink > -1 && conn.connections[i]->hopsToSink < min)
		{
			min = conn.connections[i]->hopsToSink;
			c = conn.connections[i];
		}
	}
	return c;
}

ClusterSize ConnectionManager::GetMeshHopsToShortestSink(const MeshConnection* excludeConnection) const
{
	if (GS->node->configuration.deviceType == deviceTypes::DEVICE_TYPE_SINK)
	{
		logt("SINK", "HOPS 0, clID:%x, clSize:%d", GS->node->clusterId, GS->node->clusterSize);
		return 0;
	}
	else
	{
		ClusterSize min = INT16_MAX;
		MeshConnection* c = nullptr;
		MeshConnections conn = GetMeshConnections(ConnectionDirection::INVALID);
		for(int i=0; i<conn.count; i++)
		{
			if(conn.connections[i] == excludeConnection || !conn.connections[i]->isConnected()) continue;
			if(conn.connections[i]->hopsToSink > -1 && conn.connections[i]->hopsToSink < min)
			{
				min = conn.connections[i]->hopsToSink;
				c = conn.connections[i];
			}
		}

		logt("SINK", "HOPS %d, clID:%x, clSize:%d", (c == nullptr) ? -1 : c->hopsToSink, GS->node->clusterId, GS->node->clusterSize);
		return (c == nullptr) ? -1 : c->hopsToSink;
	}
}

#define _________________EVENTS____________

//Forward ble events to connections
void ConnectionManager::BleEventHandler(ble_evt_t &bleEvent) const
{
	for (int i = 0; i < MAX_NUM_CONNECTIONS; i++) {
		if(allConnections[i] != nullptr) GS->cm->allConnections[i]->BleEventHandler(bleEvent);
	}

	//New RSSI measurement for connection received
	if(bleEvent.header.evt_id == BLE_GAP_EVT_RSSI_CHANGED)
	{
		BaseConnection* connection = GetConnectionFromHandle(bleEvent.evt.gap_evt.conn_handle);
		if (connection != nullptr) {
			i8 rssi = bleEvent.evt.gap_evt.params.rssi_changed.rssi;
			connection->lastReportedRssi = rssi;

			if(connection->rssiAverageTimes1000 == 0) connection->rssiAverageTimes1000 = (i32)rssi * 1000;

			//=> The averaging is done in the timerEventHandler
		}
	}
}

void ConnectionManager::TimerEventHandler(u16 passedTimeDs)
{
	//Check if there are unsent packet (Can happen if the softdevice was busy and it was not possible to queue packets the last time)
	if (SHOULD_IV_TRIGGER(GS->appTimerDs, passedTimeDs, SEC_TO_DS(1)) && GetPendingPackets() > 0) {
		fillTransmitBuffers();
	}

	//Disconnect Connections that have exceeded their handshake timeout
	BaseConnections conns = GetConnectionsOfType(ConnectionTypes::CONNECTION_TYPE_INVALID, ConnectionDirection::INVALID);

	for(u32 i=0; i< conns.count; i++){
		BaseConnection* conn = allConnections[conns.connectionIndizes[i]];
		if (conn == nullptr) {
			continue; //The connection was already removed in a previous iteration.
		}

		//The average rssi is caluclated using a moving average with 5% influece per time step
		conn->rssiAverageTimes1000 = (95 * (i32)conn->rssiAverageTimes1000 + 5000 * (i32)conn->lastReportedRssi) / 100;

		//Check if an implementation failure did not clear the pending connection
		//FIXME: Should use a timeout stored in the connection as we do not know what connectingTimout this connection has
		if(pendingConnection != nullptr && GS->appTimerDs > pendingConnection->creationTimeDs + Config->meshConnectingScanTimeout + SEC_TO_DS(10))
		{
			logt("CM", "Pending timeout");

			GS->logger->logError(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::FATAL_PENDING_NOT_CLEARED, 0);
			pendingConnection->appDisconnectionReason = AppDisconnectReason::PENDING_TIMEOUT;
			DeleteConnection(pendingConnection);
		}
		//Check if a handshake should time out
		else if(
			conn->connectionState >= ConnectionState::CONNECTED
			&& conn->connectionState < ConnectionState::HANDSHAKE_DONE
			&& conn->handshakeStartedDs + Config->meshHandshakeTimeoutDs <= GS->appTimerDs
		){
			logt("CM", "Handshake timeout");

			GS->logger->logCount(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_HANDSHAKE_TIMEOUT);

			conn->appDisconnectionReason = AppDisconnectReason::HANDSHAKE_TIMEOUT;
			conn->DisconnectAndRemove();
		}
		//Check if a connection reestablishment should time out
		else if(
			(conn->connectionState == ConnectionState::REESTABLISHING || conn->connectionState == ConnectionState::REESTABLISHING_HANDSHAKE)
			&& conn->disconnectedTimestampDs + SEC_TO_DS(Config->meshExtendedConnectionTimeoutSec) <= GS->appTimerDs
		){
			logt("CM", "Reconnection timeout");

			GS->logger->logError(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_CONNECTION_SUSTAIN_FAILED_TO_ESTABLISH, conn->partnerId);

			conn->appDisconnectionReason = AppDisconnectReason::RECONNECT_TIMEOUT;
			conn->DisconnectAndRemove();
		}
	}
}

