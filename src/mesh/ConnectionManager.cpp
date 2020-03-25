////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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
#include <GlobalState.h>
#include "ConnectionAllocator.h"
#include <MeshAccessModule.h>

#ifdef SIM_ENABLED
#include <CherrySim.h>
#endif

//When data is received, it is first processed until packets are available, once these have undergone some basic
//checks, they are considered Messages that are then dispatched to the node and modules

//The flow for any connection is:
// Connected => Encrypted => Mesh handle discovered => Handshake done
//encryption can be disabled and handle discovery can be skipped (handle from JOIN_ME packet will be used)

ConnectionManager::ConnectionManager()
{
	CheckedMemset(allConnections, 0x00, sizeof(allConnections));
}

void ConnectionManager::Init()
{
	freeMeshOutConnections = Conf::getInstance().meshMaxOutConnections;
	freeMeshInConnections = Conf::getInstance().meshMaxInConnections;
}
#define _______________CONNECTIVITY______________

//TODO: Do we route messages to mesh connections or to all connections????
//probably check the destinationId, if the id is within the range of apps, it should be routed
//if the id is for the mesh range, a Module could decide to grab the message and send it to its
//App connections as well

//Checks the receiver of the message first and routes it in the right direction

//TODO: Rename to mesh... we need another implementation
//Connects to a peripheral as Master, writecharacteristichandle can be BLE_GATT_HANDLE_INVALID
ErrorType ConnectionManager::ConnectAsMaster(NodeId partnerId, FruityHal::BleGapAddr* address, u16 writeCharacteristicHandle, u16 connectionIv)
{
	//Only connect if we are not connected to this partner already
	BaseConnections conns = GetConnectionsOfType(ConnectionType::INVALID, ConnectionDirection::INVALID);
	for (u32 i = 0; i < conns.count; i++) {
		if (memcmp(&(allConnections[conns.connectionIndizes[i]]->partnerAddress), address, sizeof(FruityHal::BleGapAddr)) == 0) {
			return ErrorType::INVALID_STATE;
		}
	}

	//Only connect when not currently in another connection or when there are no more free connections
	if (freeMeshOutConnections < 1 || pendingConnection != nullptr) return ErrorType::INVALID_ADDR;

	//Don't connect if the partner is not a preferred connection while we are in preferred connection mode ignore.
	if (!GS->node.IsPreferredConnection(partnerId) && GS->config.configuration.preferredConnectionMode == PreferredConnectionMode::IGNORED)
	{
		return ErrorType::FORBIDDEN;
	}
	//Tell the GAP Layer to connect, it will return if it is trying or if there was an error
	ErrorType err = GS->gapController.connectToPeripheral(*address, connectionIv, Conf::meshConnectingScanTimeout);

	logt("CONN", "Connecting as Master to %d (%02X:%02X:%02X:%02X:%02X:%02X) %s",
		partnerId,
		address->addr[5],
		address->addr[4],
		address->addr[3],
		address->addr[2],
		address->addr[1],
		address->addr[0],
		err == ErrorType::SUCCESS ? "true" : "false");

	if (err == ErrorType::SUCCESS)
	{
		StatusReporterModule* statusMod = (StatusReporterModule*)GS->node.GetModuleById(ModuleId::STATUS_REPORTER_MODULE);
		if(statusMod != nullptr){
			u32 addrPart;
			CheckedMemcpy(&addrPart, address->addr, 4);
			statusMod->SendLiveReport(LiveReportTypes::GAP_TRYING_AS_MASTER, 0, partnerId, addrPart);
		}

		//Create the connection and set it as pending
		for (u32 i = 0; i < TOTAL_NUM_CONNECTIONS; i++){
			if (allConnections[i] == nullptr){
				pendingConnection = allConnections[i] = ConnectionAllocator::getInstance().allocateMeshConnection(i, ConnectionDirection::DIRECTION_OUT, address, writeCharacteristicHandle);
				break;
			}
		}

		return ErrorType::SUCCESS;

	} else {
		GS->logger.logCustomError(CustomErrorTypes::WARN_CONNECT_AS_MASTER_NOT_POSSIBLE, (u32)err);
	}

	//FIXME_HAL: After HAL refactoring, we can return the proper error code
	return ErrorType::INTERNAL;
}

void ConnectionManager::DeleteConnection(BaseConnection* connection, AppDisconnectReason reason){
	if(connection == nullptr) return;

	logt("CM", "Cleaning up conn %u", connection->connectionId);

	for(u32 i=0; i<TOTAL_NUM_CONNECTIONS; i++){
		if(connection == allConnections[i]){
			allConnections[i] = nullptr;
			if (connection->appDisconnectionReason == AppDisconnectReason::UNKNOWN)
			{
				connection->appDisconnectionReason = reason;
			}
			ConnectionAllocator::getInstance().deallocate(connection);
		}
	}
	if(pendingConnection == connection){
		pendingConnection = nullptr;
	}

	//Update Join me packet after connection was deleted so we have another free one
	GS->node.UpdateJoinMePacket();
}

//TODO: Mesh specific
//Disconnects either all connections or all except one
//Cluster updates from this connection should be ignored
void ConnectionManager::ForceDisconnectOtherMeshConnections(const MeshConnection* ignoreConnection, AppDisconnectReason appDisconnectReason) const
{
	//We can not use GetConnections here as a disconnection of a connection might trigger another force disconnect method.
	for (int i = 0; i < TOTAL_NUM_CONNECTIONS; i++) {
		BaseConnection* conn = allConnections[i];
		if (conn != nullptr && conn != ignoreConnection && conn->connectionType == ConnectionType::FRUITYMESH) {
			conn->DisconnectAndRemove(appDisconnectReason);
		}
	}
}

void ConnectionManager::ForceDisconnectOtherHandshakedMeshConnections(const MeshConnection* ignoreConnection, AppDisconnectReason appDisconnectReason) const
{
	//We can not use GetConnections here as a disconnection of a connection might trigger another force disconnect method.
	for (int i = 0; i < TOTAL_NUM_CONNECTIONS; i++) {
		BaseConnection* conn = allConnections[i];
		if (conn != nullptr && conn != ignoreConnection && conn->connectionType == ConnectionType::FRUITYMESH && conn->handshakeDone()) {
			conn->DisconnectAndRemove(appDisconnectReason);
		}
	}
}

void ConnectionManager::ForceDisconnectAllConnections(AppDisconnectReason appDisconnectReason) const
{
	//We can not use GetConnections here as a disconnection of a connection might trigger another force disconnect method.
	for (int i = 0; i < TOTAL_NUM_CONNECTIONS; i++) {
		BaseConnection* conn = allConnections[i];
		if (conn != nullptr) {
			conn->DisconnectAndRemove(appDisconnectReason);
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
			GS->gapController.RequestConnectionParameterUpdate(conn.connections[i]->connectionHandle, connectionInterval, connectionInterval, 0, Conf::meshConnectionSupervisionTimeout);
		}
	}
}

void ConnectionManager::GATTServiceDiscoveredHandler(u16 connHandle, FruityHal::BleGattDBDiscoveryEvent& evt)
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
void ConnectionManager::ResolveConnection(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8 const * data)
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
			for(int i=0; i<TOTAL_NUM_CONNECTIONS; i++){
				if(allConnections[i] == oldConnection){
					//First, we must update the pointer because the new connection might look for itself in the array
					allConnections[i] = newConnection;

					newConnection->ConnectionSuccessfulHandler(oldConnection->connectionHandle);
					newConnection->ReceiveDataHandler(sendData, data);

					//Delete old connection and replace pointer with new connection
					ConnectionAllocator::getInstance().deallocate(oldConnection);
					return;
				}
			}
		}
	}


}

void ConnectionManager::NotifyNewConnection()
{
	MeshAccessModule *meshAccessModule = (MeshAccessModule*)GS->node.GetModuleById(ModuleId::MESH_ACCESS_MODULE);
	if (meshAccessModule != nullptr)
	{
		meshAccessModule->UpdateMeshAccessBroadcastPacket();
	}
}

void ConnectionManager::NotifyDeleteConnection()
{
	MeshAccessModule *meshAccessModule = (MeshAccessModule*)GS->node.GetModuleById(ModuleId::MESH_ACCESS_MODULE);
	if (meshAccessModule != nullptr)
	{
		meshAccessModule->UpdateMeshAccessBroadcastPacket();
	}
}

#define _________________SENDING____________

void ConnectionManager::SendMeshMessage(u8* data, u16 dataLength, DeliveryPriority priority) const
{
	SendMeshMessageInternal(data, dataLength, priority, false, true, true);
}

void ConnectionManager::SendMeshMessageInternal(u8* data, u16 dataLength, DeliveryPriority priority, bool reliable, bool loopback, bool toMeshAccess) const
{
	if (dataLength > MAX_MESH_PACKET_SIZE)
	{
		SIMEXCEPTION(PaketTooBigException);
		logt("ERROR", "Packet too big for sending!");
		return;
	}
	if (dataLength < sizeof(connPacketHeader))
	{
		SIMEXCEPTION(PaketTooSmallException);
		logt("ERROR", "Packet too small for sending!");
		return;
	}

	connPacketHeader* packetHeader = (connPacketHeader*) data;

	// ########################## Local Loopback
	if(loopback){
		//Build fake data for our loopback packet
		BaseConnectionSendData sendData;
		sendData.characteristicHandle = FruityHal::FH_BLE_INVALID_HANDLE;
		sendData.dataLength = dataLength;
		sendData.deliveryOption = reliable ? DeliveryOption::WRITE_REQ : DeliveryOption::WRITE_CMD;

		//This method will dispatch it if the receiver matches this nodeId
		//TODO: Maybe we should pass some local loopback connection reference so that further calls do not operate on nullptr
		DispatchMeshMessage(nullptr, &sendData, (connPacketHeader*)data, true);
	}

	// ########################## Routing to MeshAccess Connections
	//We send the packet to all MeshAccessConnections because any receiverId could be on the other side
	if (toMeshAccess) {
		BaseConnections maConn = GetConnectionsOfType(ConnectionType::MESH_ACCESS, ConnectionDirection::INVALID);
		for (u32 i = 0; i < maConn.count; i++) {
			MeshAccessConnection* c = (MeshAccessConnection*)allConnections[maConn.connectionIndizes[i]];
			if (c == nullptr 
				|| 
				(
					GET_DEVICE_TYPE() != DeviceType::ASSET // Assets only have mesh access connections. They should not filter anything that they want to send through them.
					&& !c->ShouldSendDataToNodeId(packetHeader->receiver)
				)) {
				continue;
			}
			if (packetHeader->receiver == NODE_ID_ANYCAST_THEN_BROADCAST) {
				packetHeader->receiver = NODE_ID_BROADCAST;
				c->SendData(data, dataLength, priority, reliable);
				return;
			}
			else {
				c->SendData(data, dataLength, priority, reliable);
			}

		}
	}

	// ########################## Sink Routing
	//Packets to the shortest sink, can only be sent to mesh partners
	if (packetHeader->receiver == NODE_ID_SHORTEST_SINK)
	{
		MeshConnection* dest = GetMeshConnectionToShortestSink(nullptr);

		if (GS->config.enableSinkRouting && dest)
		{
			dest->SendData(data, dataLength, priority, reliable);
		}
		// If message was adressed to sink but there is no route to sink broadcast message
		else
		{
			BroadcastMeshPacket(data, dataLength, priority, reliable);
		}
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
	else if (packetHeader->receiver != GS->node.configuration.nodeId)
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

void ConnectionManager::DispatchMeshMessage(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader const * packet, bool checkReceiver) const
{
	if(
		!checkReceiver
		|| IsReceiverOfNodeId(packet->receiver)
	){
		//Fix local loopback id and replace with out nodeId
		DYNAMIC_ARRAY(modifiedBuffer, sendData->dataLength);
		if (packet->receiver == NODE_ID_LOCAL_LOOPBACK)
		{
			CheckedMemcpy(modifiedBuffer, packet, sendData->dataLength);
			connPacketHeader * modifiedPacket = (connPacketHeader*)modifiedBuffer;
			modifiedPacket->receiver = GS->node.configuration.nodeId;
			packet = modifiedPacket;
		}

		//Now we must pass the message to all of our modules for further processing
		BaseConnection* connectionToSendToModules = connection; //In case one of the modules MeshMessageReceivedHandlers remove the connection, we pass nullptr to the other modules.
		const u32 connectionToSendToModulesUniqueId = connectionToSendToModules != nullptr ? connectionToSendToModules->uniqueConnectionId : 0;
		for(u32 i=0; i<GS->amountOfModules; i++){
			if(GS->activeModules[i]->configurationPointer->moduleActive){
				if (connectionToSendToModules != nullptr) {
					if (GS->cm.GetConnectionByUniqueId(connectionToSendToModulesUniqueId) == nullptr)
					{
						//The connection was removed in a MeshMessageReceivedHandler from one of our modules.
						connectionToSendToModules = nullptr;
					}
				}
				GS->activeModules[i]->MeshMessageReceivedHandler(connectionToSendToModules, sendData, packet);
			}
		}
	}
}

//A helper method for sending moduleAction messages
void ConnectionManager::SendModuleActionMessage(MessageType messageType, ModuleId moduleId, NodeId toNode, u8 actionType, u8 requestHandle, const u8* additionalData, u16 additionalDataSize, bool reliable, bool loopback) const
{
	DYNAMIC_ARRAY(buffer, SIZEOF_CONN_PACKET_MODULE + additionalDataSize);

	connPacketModule* outPacket = (connPacketModule*)buffer;
	outPacket->header.messageType = messageType;
	outPacket->header.sender = GS->node.configuration.nodeId;
	outPacket->header.receiver = toNode;

	outPacket->moduleId = moduleId;
	outPacket->requestHandle = requestHandle;
	outPacket->actionType = actionType;

	if (additionalData != nullptr && additionalDataSize > 0)
	{
		CheckedMemcpy(&outPacket->data, additionalData, additionalDataSize);
	}

	//TODO: reliable is currently not supported and by default false. The input is ignored
	GS->cm.SendMeshMessageInternal(buffer, SIZEOF_CONN_PACKET_MODULE + additionalDataSize, DeliveryPriority::LOW, false, loopback, true);
}

void ConnectionManager::BroadcastMeshPacket(u8* data, u16 dataLength, DeliveryPriority priority, bool reliable) const
{
	MeshConnections conn = GetMeshConnections(ConnectionDirection::INVALID);
	connPacketHeader* packetHeader = (connPacketHeader*)data;
	for(u32 i=0; i< conn.count; i++){
		if (packetHeader->receiver == NODE_ID_ANYCAST_THEN_BROADCAST) {
			packetHeader->receiver = NODE_ID_BROADCAST;
			conn.connections[i]->SendData(data, dataLength, priority, reliable);
			return;
		}
		else {
			conn.connections[i]->SendData(data, dataLength, priority, reliable);
		}
	}
}

ConnectionManager & ConnectionManager::getInstance()
{
	return GS->cm;
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

void ConnectionManager::GattDataTransmittedEventHandler(const FruityHal::GattDataTransmittedEvent& gattDataTransmitted)
{
	//There are two types of events that trigger a dataTransmittedCallback
	//A TX complete event frees a number of transmit buffers
	//These are used for all connections

	if (gattDataTransmitted.isConnectionHandleValid())
	{
		logt("CONN_DATA", "write_CMD complete (n=%d)", gattDataTransmitted.getCompleteCount());

		//This connection has just been given back some transmit buffers
		BaseConnection* connection = ConnectionManager::getInstance().GetConnectionFromHandle(gattDataTransmitted.getConnectionHandle());
		if (connection == nullptr) return;

		connection->HandlePacketSent(gattDataTransmitted.getCompleteCount(), 0);

		sentMeshPacketsUnreliable += gattDataTransmitted.getCompleteCount();


		for (u32 i = 0; i < gattDataTransmitted.getCompleteCount(); i++) {
			GS->logger.logCustomCount(CustomErrorTypes::COUNT_SENT_PACKETS_UNRELIABLE);
		}

		//Next, we should continue sending packets if there are any
		if (ConnectionManager::getInstance().GetPendingPackets())
		{
			ConnectionManager::getInstance().fillTransmitBuffers();
		}
	}
}

void ConnectionManager::GattcWriteResponseEventHandler(const FruityHal::GattcWriteResponseEvent & writeResponseEvent)
{
	//The EVT_WRITE_RSP comes after a WRITE_REQ and notifies that a buffer
	//for one specific connection has been cleared

	if (writeResponseEvent.getGattStatus() != FruityHal::BleGattEror::SUCCESS)
	{
		logt("ERROR", "GATT status problem %d %s", (u8)writeResponseEvent.getGattStatus(), Logger::getGattStatusErrorString(writeResponseEvent.getGattStatus()));

		GS->logger.logCount(LoggingError::GATT_STATUS, (u32)writeResponseEvent.getGattStatus());

		//TODO: Error handling, but there really shouldn't be an error....;-)
		//FIXME: Handle possible gatt status codes

	}
	else
	{
		logt("CONN_DATA", "write_REQ complete");
		BaseConnection* connection = ConnectionManager::getInstance().GetConnectionFromHandle(writeResponseEvent.getConnectionHandle());

		//Connection could have been disconneced
		if (connection == nullptr) return;

		connection->HandlePacketSent(0, 1);

		sentMeshPacketsReliable++;

		GS->logger.logCustomCount(CustomErrorTypes::COUNT_SENT_PACKETS_RELIABLE);

		//Now we continue sending packets
		if (ConnectionManager::getInstance().GetPendingPackets())
			ConnectionManager::getInstance().fillTransmitBuffers();
	}
}

#define _________________RECEIVING____________

void ConnectionManager::ForwardReceivedDataToConnection(u16 connectionHandle, BaseConnectionSendData & sendData, u8 const * data)
{
	logt("CM", "RX Data size is: %d, handles(%d, %d), delivery %d", sendData.dataLength, connectionHandle, sendData.characteristicHandle, (u32)sendData.deliveryOption);

	char stringBuffer[100];
	Logger::convertBufferToHexString(data, sendData.dataLength, stringBuffer, sizeof(stringBuffer));
	logt("CM", "%s", stringBuffer);
	//Get the handling connection for this write
	BaseConnection* connection = GS->cm.GetConnectionFromHandle(connectionHandle);

	//Notify our connection instance that data has been received
	if (connection != nullptr) {
		connection->ReceiveDataHandler(&sendData, data);
	}
}

void ConnectionManager::GattsWriteEventHandler(const FruityHal::GattsWriteEvent& gattsWriteEvent)
{
	BaseConnectionSendData sendData;
	sendData.characteristicHandle = gattsWriteEvent.getAttributeHandle();
	sendData.deliveryOption = (gattsWriteEvent.isWriteRequest()) ? DeliveryOption::WRITE_REQ : DeliveryOption::WRITE_CMD;
	sendData.priority = DeliveryPriority::LOW; //TODO: Prio is unknown, should we send it in the packet?
	sendData.dataLength = gattsWriteEvent.getLength();

	ForwardReceivedDataToConnection(gattsWriteEvent.getConnectionHandle(), sendData, gattsWriteEvent.getData() /*bleEvent.evt.gatts_evt.params.write->data*/);
}

void ConnectionManager::GattcHandleValueEventHandler(const FruityHal::GattcHandleValueEvent & handleValueEvent)
{
	BaseConnectionSendData sendData;

	sendData.characteristicHandle = handleValueEvent.getHandle();
	sendData.deliveryOption = DeliveryOption::NOTIFICATION;
	sendData.priority = DeliveryPriority::LOW; //TODO: Prio is unknown, should we send it in the packet?
	sendData.dataLength = handleValueEvent.getLength();


	ForwardReceivedDataToConnection(handleValueEvent.getConnectionHandle(), sendData, handleValueEvent.getData());
}

//This method accepts connPackets and distributes it to all other mesh connections
void ConnectionManager::RouteMeshData(BaseConnection* connection, BaseConnectionSendData* sendData, u8 const * data) const
{
	connPacketHeader const * packetHeader = (connPacketHeader const *) data;


	/*#################### Modification ############################*/
	//We ask all our modules to decide if this packet should be routed, the modules could also modify the packet content
	RoutingDecision routingDecision = 0;
	for (u32 i = 0; i < GS->amountOfModules; i++) {
		if (GS->activeModules[i]->configurationPointer->moduleActive) {
			routingDecision |= GS->activeModules[i]->MessageRoutingInterceptor(connection, sendData, packetHeader);
		}
	}

	/*#################### ROUTING ############################*/

	//We are the last receiver for this packet
	if (packetHeader->receiver == GS->node.configuration.nodeId //We are the receiver
	|| packetHeader->receiver == NODE_ID_HOPS_BASE + 1//The packet was meant to travel only one hop
	|| (packetHeader->receiver == NODE_ID_SHORTEST_SINK && GET_DEVICE_TYPE() == DeviceType::SINK)//Packet was meant for the shortest sink and we are a sink
	)
	{
		//No packet forwarding needed here.
	}
	//The packet should continue to the shortest sink
	else if(packetHeader->receiver == NODE_ID_SHORTEST_SINK)
	{
		MeshConnection* connectionSink = GS->cm.GetMeshConnectionToShortestSink(connection);

		if(GS->config.enableSinkRouting && connectionSink && !(routingDecision & ROUTING_DECISION_BLOCK_TO_MESH))
		{
			connectionSink->SendData(sendData, data);
		}
		// If message was adressed to sink but there is no route to sink broadcast message
		else
		{
			BroadcastMeshData(connection, sendData, data, routingDecision);
		}
	}
	//This could be either a packet to a specific node, group, with some hops left or a broadcast packet
	else
	{
		//If the packet should travel a number of hops, we decrement that part
		DYNAMIC_ARRAY(modifiedMessage, sendData->dataLength);
		if(packetHeader->receiver > NODE_ID_HOPS_BASE && packetHeader->receiver < NODE_ID_HOPS_BASE + 1000)
		{
			CheckedMemcpy(modifiedMessage, data, sendData->dataLength);
			connPacketHeader* modifiedPacketHeader = (connPacketHeader*)modifiedMessage;
			modifiedPacketHeader->receiver--;
			packetHeader = modifiedPacketHeader;
		}

		//TODO: We can refactor this to use the new MessageRoutingInterceptor
		//Do not forward ...
		//		... cluster info update packets, these are handeled by the node
		//		... timestamps, these are only directly sent to one node and propagate through the mesh by other means
		if(packetHeader->messageType != MessageType::CLUSTER_INFO_UPDATE
			&& packetHeader->messageType != MessageType::UPDATE_TIMESTAMP)
		{
			//Send to all other connections
			BroadcastMeshData(connection, sendData, data, routingDecision);
		}
	}
}

void ConnectionManager::BroadcastMeshData(const BaseConnection* ignoreConnection, BaseConnectionSendData* sendData, u8 const * data, RoutingDecision routingDecision) const
{
	//Iterate through all mesh connections except the ignored one and send the packet
	if (!(routingDecision & ROUTING_DECISION_BLOCK_TO_MESH)) {
		MeshConnections conn = GetMeshConnections(ConnectionDirection::INVALID);
		for (u32 i = 0; i < conn.count; i++) {
			if (conn.connections[i] != ignoreConnection) {
				sendData->characteristicHandle = conn.connections[i]->partnerWriteCharacteristicHandle;
				conn.connections[i]->SendData(sendData, data);
			}
		}
	}

	//Route to all MeshAccess Connections
	//Iterate through all mesh access connetions except the ignored one and send the packet
	if (!(routingDecision & ROUTING_DECISION_BLOCK_TO_MESH_ACCESS)) {
		BaseConnections conn2 = GetConnectionsOfType(ConnectionType::MESH_ACCESS, ConnectionDirection::INVALID);
		for (u32 i = 0; i < conn2.count; i++) {
			MeshAccessConnection* maconn = (MeshAccessConnection*)allConnections[conn2.connectionIndizes[i]];
			if (maconn != nullptr && maconn != ignoreConnection) {
				maconn->SendData(data, sendData->dataLength, sendData->priority, false);
			}
		}
	}
}

bool ConnectionManager::IsReceiverOfNodeId(NodeId nodeId) const
{
	//Check if we are part of the firmware group that should receive this image
	for (u32 i = 0; i < MAX_NUM_FW_GROUP_IDS; i++) {
		if (GS->config.fwGroupIds[i] == nodeId) return true;
	}

	if (nodeId == GS->node.configuration.nodeId)                                            return true;
	if (nodeId == NODE_ID_BROADCAST)                                                        return true;
	if (nodeId >= NODE_ID_HOPS_BASE && nodeId < NODE_ID_HOPS_BASE + NODE_ID_HOPS_BASE_SIZE) return true;
	if (nodeId == NODE_ID_SHORTEST_SINK && GET_DEVICE_TYPE() == DeviceType::SINK)           return true;

	return false;
}

#define _________________CONNECTIONS____________

//Called as soon as a new connection is made, either as central or peripheral
void ConnectionManager::GapConnectionConnectedHandler(const FruityHal::GapConnectedEvent & connectedEvent)
{
	ErrorType err;

	StatusReporterModule* statusMod = (StatusReporterModule*)GS->node.GetModuleById(ModuleId::STATUS_REPORTER_MODULE);
	if(statusMod != nullptr){
		u32 addrPart;
		CheckedMemcpy(&addrPart, connectedEvent.getPeerAddr(), 4);

		if(connectedEvent.getRole() == FruityHal::GapRole::PERIPHERAL){
			statusMod->SendLiveReport(LiveReportTypes::GAP_CONNECTED_INCOMING, 0, connectedEvent.getConnectionHandle(), addrPart);
		} else if(connectedEvent.getRole() == FruityHal::GapRole::CENTRAL){
			statusMod->SendLiveReport(LiveReportTypes::GAP_CONNECTED_OUTGOING, 0, connectedEvent.getConnectionHandle(), addrPart);
		}
	}


	logt("CM", "Connection handle %u success as %s, partner:%02x:%02x:%02x:%02x:%02x:%02x", 
		connectedEvent.getConnectionHandle(), 
		connectedEvent.getRole() == FruityHal::GapRole::CENTRAL ? "Central" : "Peripheral", 
		connectedEvent.getPeerAddr()[5], 
		connectedEvent.getPeerAddr()[4],
		connectedEvent.getPeerAddr()[3], 
		connectedEvent.getPeerAddr()[2], 
		connectedEvent.getPeerAddr()[1], 
		connectedEvent.getPeerAddr()[0]);

	GS->logger.logCustomCount(CustomErrorTypes::COUNT_CONNECTION_SUCCESS);

	BaseConnection* reestablishedConnection = IsConnectionReestablishment(connectedEvent);

	/* TODO: Part A: We have a connection reestablishment */
	if (reestablishedConnection != nullptr)
	{
		reestablishedConnection->GapReconnectionSuccessfulHandler(connectedEvent);

		//Check if there is another connection in reestablishing state that we can try to reconnect
		MeshConnections conns = GetMeshConnections(ConnectionDirection::DIRECTION_OUT);
		for (u32 i = 0; i < conns.count; i++) {
			if (conns.connections[i]->connectionState == ConnectionState::REESTABLISHING) {
				conns.connections[i]->TryReestablishing();
			}
		}

		if (connectedEvent.getRole() == FruityHal::GapRole::PERIPHERAL)
		{
			//The Peripheral should wait until the encryption request was made
			reestablishedConnection->encryptionState = EncryptionState::NOT_ENCRYPTED;
		}
		else if (connectedEvent.getRole() == FruityHal::GapRole::CENTRAL)
		{
			//If encryption is enabled, the central starts to encrypt the connection
			reestablishedConnection->encryptionState = EncryptionState::ENCRYPTING;
			GS->gapController.startEncryptingConnection(connectedEvent.getConnectionHandle());
		}

		return;
	}

	/* Part B: A normal incoming/outgoing connection */
	if (GetConnectionInHandshakeState() != nullptr)
	{
		logt("CM", "Currently in handshake, disconnect");

		//If we have a pendingConnection for this, we must clean it
		if(connectedEvent.getRole() == FruityHal::GapRole::CENTRAL){
			if(pendingConnection != nullptr){
				DeleteConnection(pendingConnection, AppDisconnectReason::CURRENTLY_IN_HANDSHAKE);
			}
		}
		err = FruityHal::Disconnect(connectedEvent.getConnectionHandle(), FruityHal::BleHciError::REMOTE_USER_TERMINATED_CONNECTION);

		return;
	}

	BaseConnection* c = nullptr;

	//We are slave (peripheral)
	if (connectedEvent.getRole() == FruityHal::GapRole::PERIPHERAL)
	{
		logt("CM", "Incoming Connection connected");

		//Check if we have a free entry in our connections array
		//It might happen that we have not, because we have not yet received a disconnect event but a connection was already disconnected
		i8 id = getFreeConnectionSpot();
		if(id < 0){
			logt("ERROR", "No spot available");
			
			//We must drop the connection
			GS->logger.logCustomError(CustomErrorTypes::WARN_CM_FAIL_NO_SPOT, 0);
			FruityHal::Disconnect(connectedEvent.getConnectionHandle(), FruityHal::BleHciError::REMOTE_USER_TERMINATED_CONNECTION);

			return;
		}



		FruityHal::BleGapAddr peerAddress;
		CheckedMemset(&peerAddress, 0, sizeof(peerAddress));
		peerAddress.addr_type = (FruityHal::BleGapAddrType)connectedEvent.getPeerAddrType();
		CheckedMemcpy(peerAddress.addr, connectedEvent.getPeerAddr(), sizeof(peerAddress.addr));

		c = allConnections[id] = ConnectionAllocator::getInstance().allocateResolverConnection(id, ConnectionDirection::DIRECTION_IN, &peerAddress);
		c->ConnectionSuccessfulHandler(connectedEvent.getConnectionHandle());


		//The central may now start encrypting or start the handshake, we just have to wait
	}
	//We are master (central)
	else if (connectedEvent.getRole() == FruityHal::GapRole::CENTRAL)
	{
		//This can happen if the connection has been cleaned up already e.g. by disconnecting all connections but the connection was accepted in the meantime
		if(pendingConnection == nullptr){
			logt("ERROR", "No pending Connection");
			GS->logger.logCustomCount(CustomErrorTypes::COUNT_NO_PENDING_CONNECTION);
			err = FruityHal::Disconnect(connectedEvent.getConnectionHandle(), FruityHal::BleHciError::REMOTE_USER_TERMINATED_CONNECTION);
			return;
		}

		c = pendingConnection;
		pendingConnection = nullptr;

		//Call Prepare again so that the clusterID and size backup are created with up to date values
		c->ConnectionSuccessfulHandler(connectedEvent.getConnectionHandle());

		//If encryption is enabled, the central starts to encrypt the connection
		if (Conf::encryptionEnabled && c->connectionType == ConnectionType::FRUITYMESH){
			c->encryptionState = EncryptionState::ENCRYPTING;
			GS->gapController.startEncryptingConnection(connectedEvent.getConnectionHandle());
		}
		//Without encryption we immediately start the handshake
		else {
			c->StartHandshake();
		}
	}
}

//If we wanted to connect but our connection timed out (only outgoing connections)
void ConnectionManager::GapConnectingTimeoutHandler(const FruityHal::GapTimeoutEvent &gapTimeoutEvent)
{
	if (pendingConnection == nullptr)
		return;

	//Save connection type, but clear pending connection, because the handlers might try to
	//create a new connection immediately
	ConnectionType connectionType = pendingConnection->connectionType;

	DeleteConnection(pendingConnection, AppDisconnectReason::GAP_CONNECTING_TIMEOUT);
}

//FIXME: Still needs rewriting
//When a connection changes to encrypted
void ConnectionManager::GapConnectionEncryptedHandler(const FruityHal::GapConnectionSecurityUpdateEvent &connectionSecurityUpdateEvent)
{
	BaseConnection* c = GetConnectionFromHandle(connectionSecurityUpdateEvent.getConnectionHandle());

	if (c == nullptr) return; //Connection might have been disconnected already

	logt("CM", "Connection id %u is now encrypted", c->connectionId);
	c->encryptionState = EncryptionState::ENCRYPTED;

	GapConnectionReadyForHandshakeHandler(c);
}

//This is called as soon as a connection has undergone e.g. encryption / mtu exchange / ... and is now ready to send data
void ConnectionManager::GapConnectionReadyForHandshakeHandler(BaseConnection* c)
{
	//If we are reestablishing, initiate the reestablishing handshake from the central side
	if(
			c->connectionType == ConnectionType::FRUITYMESH
			&& c->connectionState == ConnectionState::REESTABLISHING_HANDSHAKE
			&& c->direction == ConnectionDirection::DIRECTION_OUT
	){
		((MeshConnection*)c)->SendReconnectionHandshakePacket();
	}
	//Otherwise, we start the normal mesh handshake, but only if we are central
	else if (c->connectionState == ConnectionState::CONNECTED && c->direction == ConnectionDirection::DIRECTION_OUT){
		c->StartHandshake();
	}
}

u32 ConnectionManager::RequestDataLengthExtensionAndMtuExchange(BaseConnection* c)
{
	u32 err;

	//Request a higher MTU for the GATT Layer, errors are ignored as there are non that need to be handeled
	err = FruityHal::BleGattMtuExchangeRequest(c->connectionHandle, FruityHal::BleGattGetMaxMtu());

	//Request Data Length Extension (DLE) for the Link Layer packets, errors are ignored as there are non that need to be handeled
	if (err == (u32)ErrorType::SUCCESS) {
		err = FruityHal::BleGapDataLengthExtensionRequest(c->connectionHandle);
	}

	return err;
}

void ConnectionManager::MtuUpdatedHandler(u16 connHandle, u16 mtu)
{
	BaseConnection* conn = GetConnectionFromHandle(connHandle);

	if(conn == nullptr) return;

	conn->ConnectionMtuUpgradedHandler(mtu - FruityHal::ATT_HEADER_SIZE);
}

//Is called whenever a connection had been established and is now disconnected
//due to a timeout, deliberate disconnection by the localhost, remote, etc,...
//We might however decide to sustain it. it will only be lost after
//the finalDisconnectionHander is called
void ConnectionManager::GapConnectionDisconnectedHandler(const FruityHal::GapDisconnectedEvent& disconnectedEvent)
{
	BaseConnection* connection = GetConnectionFromHandle(disconnectedEvent.getConnectionHandle());

	StatusReporterModule* statusMod = (StatusReporterModule*)GS->node.GetModuleById(ModuleId::STATUS_REPORTER_MODULE);
	if (statusMod != nullptr) {
		statusMod->SendLiveReport(LiveReportTypes::WARN_GAP_DISCONNECTED, 0, connection == nullptr ? 0 : connection->partnerId, (u32)disconnectedEvent.getReason());
	}

	if(connection == nullptr) return;


	logt("CM", "Gap Connection handle %u disconnected", disconnectedEvent.getConnectionHandle());

	GS->logger.logCount(LoggingError::HCI_ERROR, (u32)disconnectedEvent.getReason());

	//Notify the connection itself
	bool result = connection->GapDisconnectionHandler(disconnectedEvent.getReason());

	//The connection can be disconnected
	if(result){
		logt("WARNING", "Final Disconnect");
		connection->DisconnectAndRemove(AppDisconnectReason::GAP_DISCONNECT_NO_REESTABLISH_REQUESTED);
	}
	// The connection will try to reconnect
	else
	{

	}
}

void ConnectionManager::GattcTimeoutEventHandler(const FruityHal::GattcTimeoutEvent & gattcTimeoutEvent)
{
	//A GATTC Timeout occurs if a WRITE_RSP is not received within 30s
	//This essentially marks the end of a connection, we'll have to disconnect
	logt("ERROR", "BLE_GATTC_EVT_TIMEOUT");

	BaseConnection* connection = GetConnectionFromHandle(gattcTimeoutEvent.getConnectionHandle());

	connection->DisconnectAndRemove(AppDisconnectReason::GATTC_TIMEOUT);

	GS->logger.logCustomError(CustomErrorTypes::FATAL_BLE_GATTC_EVT_TIMEOUT_FORCED_US, 0);
}

#define _________________HELPERS____________

i8 ConnectionManager::getFreeConnectionSpot() const
{
	for (int i = 0; i < TOTAL_NUM_CONNECTIONS; i++){
		if (allConnections[i] == nullptr)
			return i;
	}
	return -1;
}

bool ConnectionManager::HasFreeConnection(ConnectionDirection direction) const
{
	switch (direction)
	{
	case ConnectionDirection::DIRECTION_IN:
		return freeMeshInConnections > 0;
	case ConnectionDirection::DIRECTION_OUT:
		return freeMeshOutConnections > 0;
	default:
		return false;
	}
}


//Looks through all connections for the right handle and returns the right one
BaseConnection* ConnectionManager::GetConnectionFromHandle(u16 connectionHandle) const
{
	for(u32 i=0; i<TOTAL_NUM_CONNECTIONS; i++){
		if(allConnections[i] != nullptr && allConnections[i]->connectionHandle == connectionHandle){
			return allConnections[i];
		}
	}
	return nullptr;
}

//Looks through all connections for the right handle and returns the right one
BaseConnection* ConnectionManager::GetConnectionByUniqueId(u32 uniqueConnectionId) const
{
	for(u32 i=0; i<TOTAL_NUM_CONNECTIONS; i++){
		if(allConnections[i] != nullptr && allConnections[i]->uniqueConnectionId == uniqueConnectionId){
			return allConnections[i];
		}
	}
	return nullptr;
}

BaseConnections ConnectionManager::GetBaseConnections(ConnectionDirection direction) const{
	BaseConnections fc;
	CheckedMemset(&fc, 0x00, sizeof(BaseConnections));
	for(u32 i=0; i<TOTAL_NUM_CONNECTIONS; i++){
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
	CheckedMemset(&fc, 0x00, sizeof(MeshConnections));
	for(u32 i=0; i<TOTAL_NUM_CONNECTIONS; i++){
		if(allConnections[i] != nullptr){
			if(allConnections[i]->connectionType == ConnectionType::FRUITYMESH){
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

	for(u32 i=0; i<TOTAL_NUM_CONNECTIONS; i++){
		if(allConnections[i] != nullptr && allConnections[i]->connectionType == ConnectionType::FRUITYMESH){
			if(allConnections[i]->connectionState == ConnectionState::HANDSHAKING){
				return (MeshConnection*)allConnections[i];
			}
		}
	}
	return nullptr;
}

BaseConnections ConnectionManager::GetConnectionsOfType(ConnectionType connectionType, ConnectionDirection direction) const{
	BaseConnections fc;
	CheckedMemset(&fc, 0x00, sizeof(BaseConnections));
	for(u32 i=0; i<TOTAL_NUM_CONNECTIONS; i++){
		if(allConnections[i] != nullptr){
			if(allConnections[i]->connectionType == connectionType || connectionType == ConnectionType::INVALID){
				if(allConnections[i]->direction == direction || direction == ConnectionDirection::INVALID){
					fc.connectionIndizes[fc.count] = i;
					fc.count++;
				}
			}
		}
	}
	return fc;
}

//Looks through all connections for the right handle and returns the right one
MeshConnection* ConnectionManager::GetMeshConnectionToPartner(NodeId partnerId) const
{
	for (u32 i = 0; i < TOTAL_NUM_CONNECTIONS; i++) {
		if (
			allConnections[i] != nullptr
			&& allConnections[i]->connectionType == ConnectionType::FRUITYMESH
			&& allConnections[i]->partnerId == partnerId
		) {
			return (MeshConnection*)allConnections[i];
		}
	}
	return nullptr;
}

//Returns the pending packets of all connection types
u16 ConnectionManager::GetPendingPackets() const
{
	u16 pendingPackets = 0;
	for (u32 i = 0; i < TOTAL_NUM_CONNECTIONS; i++){
		if(allConnections[i] != nullptr){
			pendingPackets += allConnections[i]->GetPendingPackets();
		}
	}
	return pendingPackets;
}

BaseConnection* ConnectionManager::IsConnectionReestablishment(const FruityHal::GapConnectedEvent& connectedEvent) const
{
	//Check if we already have a connection for this peer, identified by its address
	for (int i = 0; i < TOTAL_NUM_CONNECTIONS; i++)
	{
		if (allConnections[i] != nullptr && allConnections[i]->connectionState == ConnectionState::REESTABLISHING)
		{
			if (memcmp(connectedEvent.getPeerAddr(), allConnections[i]->partnerAddress.addr, 6) == 0
				&& (FruityHal::BleGapAddrType)connectedEvent.getPeerAddrType() == allConnections[i]->partnerAddress.addr_type)
			{
				logt("CM", "Found existing connection id %u", allConnections[i]->connectionId);
				return allConnections[i];
			}
		}
	}
	return nullptr;
}

BaseConnections ConnectionManager::GetConnectionsByUniqueId(u32 uniqueConnectionId) const
{
	BaseConnections fc;
	CheckedMemset(&fc, 0x00, sizeof(BaseConnections));
	for (u32 i = 0; i < TOTAL_NUM_CONNECTIONS; i++) {
		if (allConnections[i] != nullptr) {
			if (allConnections[i]->uniqueConnectionId == uniqueConnectionId) {
				fc.connectionIndizes[fc.count] = i;
				fc.count++;
			}
		}
	}
	if (fc.count > 1)
	{
		// This function must not return more than one connection,
		// as a >UNIQUE<ConnectionId must not be used twice.
		SIMEXCEPTION(IllegalStateException);
	}
	return fc;
}

//TODO: Only return mesh connections, check
MeshConnection* ConnectionManager::GetMeshConnectionToShortestSink(const BaseConnection* excludeConnection) const
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

ClusterSize ConnectionManager::GetMeshHopsToShortestSink(const BaseConnection* excludeConnection) const
{
	if (GET_DEVICE_TYPE() == DeviceType::SINK)
	{
		logt("SINK", "HOPS 0, clID:%x, clSize:%d", GS->node.clusterId, GS->node.clusterSize);
		return 0;
	}
	else
	{
		ClusterSize min = INT16_MAX;
		MeshConnection* c = nullptr;
		MeshConnections conn = GetMeshConnections(ConnectionDirection::INVALID);
		for(int i=0; i<conn.count; i++)
		{
			if(conn.connections[i] == excludeConnection || !conn.connections[i]->handshakeDone()) continue;
			if(conn.connections[i]->hopsToSink > -1 && conn.connections[i]->hopsToSink < min)
			{
				min = conn.connections[i]->hopsToSink;
				c = conn.connections[i];
			}
		}

		const ClusterSize hopsToSink = (c == nullptr) ? -1 : c->hopsToSink;

		logt("SINK", "HOPS %d, clID:%x, clSize:%d", hopsToSink, GS->node.clusterId, GS->node.clusterSize);
		return hopsToSink;
	}
}

#define _________________EVENTS____________

void ConnectionManager::GapRssiChangedEventHandler(const FruityHal::GapRssiChangedEvent & rssiChangedEvent) const
{
	BaseConnection* connection = GetConnectionFromHandle(rssiChangedEvent.getConnectionHandle());
	if (connection != nullptr) {
		i8 rssi = rssiChangedEvent.getRssi();
		connection->lastReportedRssi = rssi;

		if (connection->rssiAverageTimes1000 == 0) connection->rssiAverageTimes1000 = (i32)rssi * 1000;

		//=> The averaging is done in the timerEventHandler
	}
}

void ConnectionManager::TimerEventHandler(u16 passedTimeDs)
{
	//Check if there are unsent packet (Can happen if the softdevice was busy and it was not possible to queue packets the last time)
	if (SHOULD_IV_TRIGGER(GS->appTimerDs, passedTimeDs, SEC_TO_DS(1)) && GetPendingPackets() > 0) {
		fillTransmitBuffers();
	}

	{
		//Go through all connections to do periodic cleanup tasks and other periodic work
		BaseConnections conns = GetConnectionsOfType(ConnectionType::INVALID, ConnectionDirection::INVALID);

		for (u32 i = 0; i < conns.count; i++) {
			BaseConnection* conn = allConnections[conns.connectionIndizes[i]];
			if (conn == nullptr) {
				continue; //The connection was already removed in a previous iteration.
			}

			//The average rssi is caluclated using a moving average with 5% influece per time step
			conn->rssiAverageTimes1000 = (95 * (i32)conn->rssiAverageTimes1000 + 5000 * (i32)conn->lastReportedRssi) / 100;

			//Check if an implementation failure did not clear the pending connection
			//FIXME: Should use a timeout stored in the connection as we do not know what connectingTimout this connection has
			if (pendingConnection != nullptr)
			{
				const u32 timeoutTimeDs = (
					pendingConnection->handshakeStartedDs > 0 ?
						//If the handshake has started, we calculate the timeout based on the handshake...
						Conf::meshHandshakeTimeoutDs + pendingConnection->handshakeStartedDs
						//...else we calculate the timeout based on the creation.
						: pendingConnection->creationTimeDs
					);
				if (GS->appTimerDs > timeoutTimeDs + SEC_TO_DS(10))
				{
					logt("ERROR", "Fatal: Pending timeout");
					u32 error = (((u8)pendingConnection->appDisconnectionReason))
						+ (((u8)(pendingConnection->handshakeStartedDs > 0)) << 8)
						+ (((u8)pendingConnection->direction) << 16)
						+ (((u8)pendingConnection->connectionState) << 24);
					GS->logger.logCustomError(CustomErrorTypes::FATAL_PENDING_NOT_CLEARED, error);

					SIMEXCEPTION(IllegalStateException);

					DeleteConnection(pendingConnection, AppDisconnectReason::PENDING_TIMEOUT);
				}
			}
			//Check if a handshake should time out
			else if (
				conn->connectionState >= ConnectionState::CONNECTED
				&& conn->connectionState < ConnectionState::HANDSHAKE_DONE
				&& conn->handshakeStartedDs + Conf::meshHandshakeTimeoutDs <= GS->appTimerDs
				) {
				logt("CM", "Handshake timeout in state %u", (u32)conn->connectionState);

				GS->logger.logCustomError(CustomErrorTypes::WARN_HANDSHAKE_TIMEOUT, conn->partnerId);

				conn->DisconnectAndRemove(AppDisconnectReason::HANDSHAKE_TIMEOUT);
			}
			//Check if a connection reestablishment must be retried
			else if (conn->connectionType == ConnectionType::FRUITYMESH && ((MeshConnection*)conn)->mustRetryReestablishing) {
				logt("CM", "Retrying reestablishing");

				((MeshConnection*)conn)->TryReestablishing();
			}
			//Check if a connection reestablishment should time out
			else if (
				conn->connectionType == ConnectionType::FRUITYMESH
				&& (conn->connectionState == ConnectionState::REESTABLISHING || conn->connectionState == ConnectionState::REESTABLISHING_HANDSHAKE)
				&& ((MeshConnection*)conn)->reestablishmentStartedDs + SEC_TO_DS(Conf::meshExtendedConnectionTimeoutSec) <= GS->appTimerDs
				) {
				logt("CM", "Reconnection timeout");

				GS->logger.logCustomError(CustomErrorTypes::WARN_CONNECTION_SUSTAIN_FAILED_TO_ESTABLISH, conn->partnerId);

				conn->DisconnectAndRemove(AppDisconnectReason::RECONNECT_TIMEOUT);
			}
		}
	}

	// Time Syncing
	timeSinceLastTimeSyncIntervalDs += passedTimeDs;
	if(GS->timeManager.IsTimeCorrected() && timeSinceLastTimeSyncIntervalDs >= TIME_BETWEEN_TIME_SYNC_INTERVALS_DS)
	{
		timeSinceLastTimeSyncIntervalDs = 0;
		BaseConnections conns = GetConnectionsOfType(ConnectionType::FRUITYMESH, ConnectionDirection::INVALID);

		for (u32 i = 0; i < conns.count; i++)
		{
			MeshConnection* conn = static_cast<MeshConnection*>(allConnections[conns.connectionIndizes[i]]);
			if (conn == nullptr)
			{
				// The Connection was already removed
				SIMEXCEPTION(IllegalStateException);
				GS->logger.logCustomError(CustomErrorTypes::FATAL_CONNECTION_REMOVED_WHILE_TIME_SYNC, 1000);
				continue;
			}

			if (conn->handshakeDone() == false) continue;

			if (conn->timeSyncState == MeshConnection::TimeSyncState::UNSYNCED)
			{
				alignas(u32) TimeSyncInitial dataToSend = GS->timeManager.GetTimeSyncIntialMessage(conn->partnerId);

				conn->syncSendingOrdered = GS->timeManager.GetTimePoint();

				logt("TSYNC", "Sending out TimeSyncInitial, NodeId: %u, partner: %u", (u32)GS->node.configuration.nodeId, (u32)conn->partnerId);

				GS->cm.SendMeshMessage(
					(u8*)&dataToSend,
					sizeof(TimeSyncInitial),
					DeliveryPriority::LOW);
			}
			else if (conn->timeSyncState == MeshConnection::TimeSyncState::INITIAL_SENT)
			{
				alignas(u32) TimeSyncCorrection dataToSend;
				CheckedMemset(&dataToSend, 0, sizeof(dataToSend));
				dataToSend.header.header.messageType = MessageType::TIME_SYNC;
				dataToSend.header.header.receiver = conn->partnerId;
				dataToSend.header.header.sender = GS->node.configuration.nodeId;
				dataToSend.header.type = TimeSyncType::CORRECTION;
				dataToSend.correctionTicks = conn->correctionTicks;

				logt("TSYNC", "Sending out TimeSyncCorrection, NodeId: %u, partner: %u", (u32)GS->node.configuration.nodeId, (u32)conn->partnerId);

				GS->cm.SendMeshMessage(
					(u8*)&dataToSend,
					sizeof(TimeSyncCorrection),
					DeliveryPriority::LOW
					);
			}
		}
	}
}

void ConnectionManager::ResetTimeSync()
{
	BaseConnections conns = GetConnectionsOfType(ConnectionType::FRUITYMESH, ConnectionDirection::INVALID);

	for (u32 i = 0; i < conns.count; i++)
	{
		MeshConnection* conn = static_cast<MeshConnection*>(allConnections[conns.connectionIndizes[i]]);
		if (conn == nullptr)
		{
			// The Connection was already removed
			SIMEXCEPTION(IllegalStateException);
			GS->logger.logCustomError(CustomErrorTypes::FATAL_CONNECTION_REMOVED_WHILE_TIME_SYNC, 2000);
			continue;
		}

		conn->timeSyncState = MeshConnection::TimeSyncState::UNSYNCED;
	}
}

bool ConnectionManager::IsAnyConnectionCurrentlySyncing()
{
	BaseConnections conns = GetConnectionsOfType(ConnectionType::FRUITYMESH, ConnectionDirection::INVALID);

	for (u32 i = 0; i < conns.count; i++)
	{
		const MeshConnection* conn = static_cast<MeshConnection*>(allConnections[conns.connectionIndizes[i]]);
		if (conn == nullptr)
		{
			// The Connection was already removed, should not happen
			SIMEXCEPTION(IllegalStateException);
			GS->logger.logCustomError(CustomErrorTypes::FATAL_CONNECTION_REMOVED_WHILE_TIME_SYNC, 5000);
			continue;
		}
		if (conn->timeSyncState == MeshConnection::TimeSyncState::INITIAL_SENT)
		{
			return true;
		}
	}

	return false;
}

void ConnectionManager::TimeSyncInitialReplyReceivedHandler(const TimeSyncInitialReply & reply)
{
	BaseConnections conns = GetConnectionsOfType(ConnectionType::FRUITYMESH, ConnectionDirection::INVALID);

	for (u32 i = 0; i < conns.count; i++)
	{
		MeshConnection* conn = static_cast<MeshConnection*>(allConnections[conns.connectionIndizes[i]]);
		if (conn == nullptr)
		{
			// The Connection was already removed
			SIMEXCEPTION(IllegalStateException);
			GS->logger.logCustomError(CustomErrorTypes::FATAL_CONNECTION_REMOVED_WHILE_TIME_SYNC, 3000);
			continue;
		}

		if (conn->partnerId == reply.header.header.sender)
		{
			if (conn->timeSyncState != MeshConnection::TimeSyncState::UNSYNCED)
			{
				// Our time Syncing was interrupted by a third node. This can happen e.g. when two nodes in the mesh were synced roughly at the same time
				// and their time syncs now propagate through the mesh simultaneously. In such a case we just resync the connection.
				conn->timeSyncState = MeshConnection::TimeSyncState::UNSYNCED;
			}
			else
			{
				conn->timeSyncState = MeshConnection::TimeSyncState::INITIAL_SENT;
			}
		}
	}
}

void ConnectionManager::TimeSyncCorrectionReplyReceivedHandler(const TimeSyncCorrectionReply & reply)
{
	BaseConnections conns = GetConnectionsOfType(ConnectionType::FRUITYMESH, ConnectionDirection::INVALID);

	for (u32 i = 0; i < conns.count; i++)
	{
		MeshConnection* conn = static_cast<MeshConnection*>(allConnections[conns.connectionIndizes[i]]);
		if (conn == nullptr)
		{
			// The Connection was already removed
			SIMEXCEPTION(IllegalStateException);
			GS->logger.logCustomError(CustomErrorTypes::FATAL_CONNECTION_REMOVED_WHILE_TIME_SYNC, 4000);
			continue;
		}

		if (conn->partnerId == reply.header.header.sender)
		{
			logt("TSYNC", "Setting CORRECTION_SENT for node %u with partner %u", (u32)GS->node.configuration.nodeId, conn->partnerId);
			if (conn->timeSyncState != MeshConnection::TimeSyncState::INITIAL_SENT)
			{
				// Our time Syncing was interrupted by a third node. This can happen e.g. when two nodes in the mesh were synced roughly at the same time
				// and their time syncs now propagate through the mesh simultaneously. In such a case we just resync the connection.
				conn->timeSyncState = MeshConnection::TimeSyncState::UNSYNCED;
			}
			else
			{
				conn->timeSyncState = MeshConnection::TimeSyncState::CORRECTION_SENT;
			}
		}
	}
}

u32 ConnectionManager::GenerateUniqueConnectionId()
{
	//Generate a unique id for a connection
	uniqueConnectionIdCounter++;
	if (uniqueConnectionIdCounter == 0) {
		uniqueConnectionIdCounter = 1;
	}
	return uniqueConnectionIdCounter;
}

