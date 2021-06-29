////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2021 M-Way Solutions GmbH
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
#include <FmTypes.h>
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
#include "ScanningModule.h"
#if IS_ACTIVE(SIG_MESH)
#include "SigTypes.h"
#endif

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
    freeMeshOutConnections = Conf::GetInstance().meshMaxOutConnections;
    freeMeshInConnections = Conf::GetInstance().meshMaxInConnections;
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
        if (memcmp(&(conns.handles[i].GetConnection()->partnerAddress), address, sizeof(FruityHal::BleGapAddr)) == 0) {
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
    ErrorType err = GS->gapController.ConnectToPeripheral(*address, connectionIv, Conf::meshConnectingScanTimeout);

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
            CheckedMemcpy(&addrPart, address->addr.data(), 4);
            statusMod->SendLiveReport(LiveReportTypes::GAP_TRYING_AS_MASTER, 0, partnerId, addrPart);
        }

        //Create the connection and set it as pending
        for (u32 i = 0; i < TOTAL_NUM_CONNECTIONS; i++){
            if (allConnections[i] == nullptr){
                pendingConnection = allConnections[i] = ConnectionAllocator::GetInstance().AllocateMeshConnection(i, ConnectionDirection::DIRECTION_OUT, address, writeCharacteristicHandle);
                break;
            }
        }

        return ErrorType::SUCCESS;

    } else {
        GS->logger.LogCustomError(CustomErrorTypes::WARN_CONNECT_AS_MASTER_NOT_POSSIBLE, (u32)err);
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
            ConnectionAllocator::GetInstance().Deallocate(connection);
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
        if (conn != nullptr && conn != ignoreConnection && conn->connectionType == ConnectionType::FRUITYMESH && conn->HandshakeDone()) {
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
        if (conn.handles[i].IsHandshakeDone()){
            DISCARD(GAPController::GetInstance().RequestConnectionParameterUpdate(
                conn.handles[i].GetConnectionHandle(),
                connectionInterval,
                connectionInterval,
                0,
                Conf::meshConnectionSupervisionTimeout
            ));
        }
    }
}

#if IS_ACTIVE(CONN_PARAM_UPDATE)
void ConnectionManager::UpdateConnectionIntervalForLongTermMeshConnections() const
{
    // If the connection intervals for young and long-term connections are the
    // same, skip the update procedure.
    if (        Conf::GetInstance().meshMinLongTermConnectionInterval
                    == Conf::GetInstance().meshMinConnectionInterval
            &&  Conf::GetInstance().meshMaxLongTermConnectionInterval
                    == Conf::GetInstance().meshMaxConnectionInterval)
    {
        return;
    }

    // The logic which kicks off the update.
    const auto updateConnections = [](MeshConnections connectionsToUpdate)
    {
        // Iterate over all the connections.
        for (u32 i=0; i < connectionsToUpdate.count; ++i)
        {
            auto &handle = connectionsToUpdate.handles[i];
            // Skip connections that are not fully connected already.
            if (handle.GetConnectionState() != ConnectionState::HANDSHAKE_DONE)
            {
                continue;
            }
            // Get a pointer to the connection object.
            auto *connection = handle.GetConnection();
            // Skip connections for which the long term connection interval
            // has already been requested.
            if (connection->longTermConnectionIntervalRequested)
            {
                continue;
            }
            // The handshakeStartedDs timestamp is set on successful connection
            // in BaseConnection::ConnectionSuccessfulHandler.
            const auto connectionAgeDs = GS->appTimerDs - connection->handshakeStartedDs;
            // Check that the connection is older than the long term age.
            const bool isCentral = 
                connection->direction == ConnectionDirection::DIRECTION_OUT;
            const auto ageThresholdDs =
                Conf::meshConnectionLongTermAgeDs
                + (isCentral ? 0u : Conf::meshConnectionLongTermAgePeripheralPenaltyDs);
            if (connectionAgeDs >= ageThresholdDs)
            {
                // Actually request the connection parameter update.
                const auto err = GAPController::GetInstance().RequestConnectionParameterUpdate(
                    handle.GetConnectionHandle(),
                    Conf::GetInstance().meshMinLongTermConnectionInterval,
                    Conf::GetInstance().meshMaxLongTermConnectionInterval,
                    Conf::meshPeripheralSlaveLatency,
                    Conf::meshConnectionSupervisionTimeout
                );
                // If the connection parameter update was handled by the
                // SoftDevice we consider the job done.
                if (err == ErrorType::SUCCESS)
                {
                    // Ensure that the long term interval is not requested again for
                    // this connection.
                    connection->longTermConnectionIntervalRequested = true;
                }
            }
        }
    };

    // Fetch all connections with this node in the central and peripheral role.
    updateConnections(GetMeshConnections(ConnectionDirection::DIRECTION_OUT));
    updateConnections(GetMeshConnections(ConnectionDirection::DIRECTION_IN));
}
#endif

void ConnectionManager::GATTServiceDiscoveredHandler(u16 connHandle, FruityHal::BleGattDBDiscoveryEvent& evt)
{
    //Find the connection that was discovering services and inform it
    BaseConnection* conn = GetRawConnectionFromHandle(connHandle);
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
                    ConnectionAllocator::GetInstance().Deallocate(oldConnection);
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

void ConnectionManager::SendMeshMessage(u8* data, u16 dataLength) const
{
    ErrorType err = SendMeshMessageInternal(data, dataLength, false, true, true);
    if (err != ErrorType::SUCCESS) logt("ERROR", "Failed to send mesh message error code: %u", (u32)err);
}

ErrorType ConnectionManager::SendMeshMessageInternal(u8* data, u16 dataLength, bool reliable, bool loopback, bool toMeshAccess) const
{
    ErrorType err = ErrorType::SUCCESS;

    if (dataLength > MAX_MESH_PACKET_SIZE)
    {
        SIMEXCEPTION(PacketTooBigException);
        logt("ERROR", "Packet too big for sending!");
        return ErrorType::INVALID_LENGTH;
    }
    if (dataLength < sizeof(ConnPacketHeader))
    {
        SIMEXCEPTION(PacketTooSmallException);
        logt("ERROR", "Packet too small for sending!");
        return ErrorType::INVALID_LENGTH;
    }

    ConnPacketHeader* packetHeader = (ConnPacketHeader*) data;

    {
        // NOTE: The way the amountOfSplitPackets are calculated has a slight bias as it always
        //       takes all connections into account for MTU calculation, even if the message is not
        //       sent to some connection at all. This was done on purpose as the message sending
        //       logic is already quite complicated. We don't want to add to this complexity just for
        //       a slightly better logging number. In most scenarios the MTU is the same between all
        //       connections. The only cases where this is currently not correct is when we either have
        //       a MeshAccessConnection or if we don't send out the message at all (e.g. a loopback).
        //       Both these cases can be ignored in the vast majority of the cases.
        const u32 smallestMtu = GetSmallestMtuOfAllConnections();
        const u32 amountOfSplitPackets = Utility::MessageLengthToAmountOfSplitPackets(dataLength, smallestMtu);
        GS->logger.LogCustomCount(CustomErrorTypes::COUNT_GENERATED_SPLIT_PACKETS, amountOfSplitPackets);
    }

    // ########################## Local Loopback
    if(loopback){
        //Build fake data for our loopback packet
        BaseConnectionSendData sendData;
        sendData.characteristicHandle = FruityHal::FH_BLE_INVALID_HANDLE;
        sendData.dataLength = dataLength;
        sendData.deliveryOption = reliable ? DeliveryOption::WRITE_REQ : DeliveryOption::WRITE_CMD;

        //This method will dispatch it if the receiver matches this nodeId
        //TODO: Maybe we should pass some local loopback connection reference so that further calls do not operate on nullptr
        DispatchMeshMessage(nullptr, &sendData, (ConnPacketHeader*)data, true);
    }

    // ########################## Routing to MeshAccess Connections
    //We send the packet to all MeshAccessConnections because any receiverId could be on the other side
    if (toMeshAccess) {
        MeshAccessConnections maConn = GetMeshAccessConnections(ConnectionDirection::INVALID);
        for (u32 i = 0; i < maConn.count; i++) {
            MeshAccessConnectionHandle mach = maConn.handles[i];
            if (!mach 
                || 
                (
                    GET_DEVICE_TYPE() != DeviceType::ASSET // Assets only have mesh access connections. They should not filter anything that they want to send through them.
                    && !mach.ShouldSendDataToNodeId(packetHeader->receiver)
                )) {
                continue;
            }
            if (packetHeader->receiver == NODE_ID_ANYCAST_THEN_BROADCAST) {
                packetHeader->receiver = NODE_ID_BROADCAST;
                bool result = mach.SendData(data, dataLength, reliable);
                if (result)
                {
                    return ErrorType::SUCCESS;
                }
                else
                {
                    return ErrorType::INTERNAL;
                }
            }
            else {
                bool result = mach.SendData(data, dataLength, reliable);
                if (result == false)
                {
                    err = ErrorType::INTERNAL;
                }
            }

        }
    }

    // ########################## Sink Routing
    //Packets to the shortest sink, can only be sent to mesh partners
    if (packetHeader->receiver == NODE_ID_SHORTEST_SINK)
    {
        MeshConnectionHandle dest = GetMeshConnectionToShortestSink(nullptr);

        if (GS->config.enableSinkRouting && dest)
        {
            bool result = dest.SendData(data, dataLength, reliable);
            if (result == false)
            {
                err = ErrorType::INTERNAL;
            }
        }
        // If message was adressed to sink but there is no route to sink broadcast message
        else
        {
            bool result = BroadcastMeshPacket(data, dataLength, reliable);
            if (result == false)
            {
                err = ErrorType::INTERNAL;
            }
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
        MeshConnectionHandle receiverConn;
        MeshConnections conn = GetMeshConnections(ConnectionDirection::INVALID);
        for(u32 i=0; i< conn.count; i++){
            if(conn.handles[i].IsHandshakeDone() && conn.handles[i].GetPartnerId() == packetHeader->receiver){
                receiverConn = conn.handles[i];
                break;
            }
        }

        //Send to receiver or broadcast if not directly connected to us
        if(receiverConn){
            bool result = receiverConn.SendData(data, dataLength, reliable);
            if (result == false)
            {
                err = ErrorType::INTERNAL;
            }
        } else {
            bool result = BroadcastMeshPacket(data, dataLength, reliable);
            if (result == false)
            {
                err = ErrorType::INTERNAL;
            }
        }
    }

    return err;
}

void ConnectionManager::DispatchMeshMessage(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packet, bool checkReceiver) const
{
    if(
        !checkReceiver
        || IsReceiverOfNodeId(packet->receiver)
    ){
        if(!IsValidFruityMeshPacket((const u8*)packet, sendData->dataLength)){
            SIMEXCEPTION(IllegalFruityMeshPacketException);
            Logger::GetInstance().LogCustomCount(CustomErrorTypes::COUNT_RECEIVED_INVALID_FRUITY_MESH_PACKET);
            return;
        }

        Logger::GetInstance().LogCustomCount(CustomErrorTypes::COUNT_RECEIVED_MESSAGES);

        //Fix local loopback id and replace with our nodeId
        DYNAMIC_ARRAY(modifiedBuffer, sendData->dataLength.GetRaw());
        if (packet->receiver == NODE_ID_LOCAL_LOOPBACK)
        {
            CheckedMemcpy(modifiedBuffer, packet, sendData->dataLength.GetRaw());
            ConnPacketHeader * modifiedPacket = (ConnPacketHeader*)modifiedBuffer;
            modifiedPacket->receiver = GS->node.configuration.nodeId;
            packet = modifiedPacket;
        }

        //Now we must pass the message to all of our modules for further processing
        BaseConnection* connectionToSendToModules = connection; //In case one of the modules MeshMessageReceivedHandlers remove the connection, we pass nullptr to the other modules.
        const u32 connectionToSendToModulesUniqueId = connectionToSendToModules != nullptr ? connectionToSendToModules->uniqueConnectionId : 0;
        for(u32 i=0; i<GS->amountOfModules; i++){
            //We forward the message to a module if it is either active or if its configuration should be changed
            if (GS->activeModules[i]->configurationPointer->moduleActive || packet->messageType == MessageType::MODULE_CONFIG) {
                if (connectionToSendToModules != nullptr) {
                    if (!GS->cm.GetConnectionByUniqueId(connectionToSendToModulesUniqueId).IsValid())
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
ErrorTypeUnchecked ConnectionManager::SendModuleActionMessage(MessageType messageType, ModuleId moduleId, NodeId toNode, u8 actionType, u8 requestHandle, const u8* additionalData, u16 additionalDataSize, bool reliable, bool loopback) const
{
    if (toNode == NODE_ID_INVALID) return ErrorTypeUnchecked::INVALID_PARAM;

    DYNAMIC_ARRAY(buffer, SIZEOF_CONN_PACKET_MODULE + additionalDataSize);

    ConnPacketModule* outPacket = (ConnPacketModule*)buffer;
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
    return (ErrorTypeUnchecked)GS->cm.SendMeshMessageInternal(buffer, SIZEOF_CONN_PACKET_MODULE + additionalDataSize, false, loopback, true);
}

ErrorTypeUnchecked ConnectionManager::SendModuleActionMessage(MessageType messageType, VendorModuleId moduleId, NodeId toNode, u8 actionType, u8 requestHandle, const u8* additionalData, u16 additionalDataSize, bool reliable, bool loopback) const
{
    if(!Utility::IsVendorModuleId(moduleId)){
        return SendModuleActionMessage(messageType, Utility::GetModuleId(moduleId), toNode, actionType, requestHandle, additionalData, additionalDataSize, reliable, loopback);
    }

    if (toNode == NODE_ID_INVALID) return ErrorTypeUnchecked::INVALID_PARAM;

    DYNAMIC_ARRAY(buffer, SIZEOF_CONN_PACKET_MODULE_VENDOR + additionalDataSize);
    CheckedMemset(buffer, 0, SIZEOF_CONN_PACKET_MODULE_VENDOR + additionalDataSize);

    ConnPacketModuleVendor* outPacket = (ConnPacketModuleVendor*)buffer;
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
    return (ErrorTypeUnchecked)GS->cm.SendMeshMessageInternal(buffer, SIZEOF_CONN_PACKET_MODULE_VENDOR + additionalDataSize, false, loopback, true);
}

bool ConnectionManager::BroadcastMeshPacket(u8* data, u16 dataLength, bool reliable) const
{
    bool ret = true;
    MeshConnections conn = GetMeshConnections(ConnectionDirection::INVALID);
    ConnPacketHeader* packetHeader = (ConnPacketHeader*)data;
    for(u32 i=0; i< conn.count; i++){
        // We might have connections that will be dropped, because eg. nodes are in the same cluster. This is very rare,
        // but can happen right after or during clustering. We don't want to send data over those connections.
        if (conn.handles[i].IsHandshakeDone() == false) continue;

        if (packetHeader->receiver == NODE_ID_ANYCAST_THEN_BROADCAST) {
            packetHeader->receiver = NODE_ID_BROADCAST;
            bool result = conn.handles[i].SendData(data, dataLength, reliable);
            ret = result && ret;
            return ret;
        }
        else {
            bool result = conn.handles[i].SendData(data, dataLength, reliable);
            ret = result && ret; 
        }
    }

    return ret;
}

ConnectionManager & ConnectionManager::GetInstance()
{
    return GS->cm;
}

void ConnectionManager::FillTransmitBuffers() const
{
    BaseConnections conn = GetBaseConnections(ConnectionDirection::INVALID);
    for(u32 i=0; i< conn.count; i++){
        if (conn.handles[i]) {
            conn.handles[i].FillTransmitBuffers();
        }
    }
}

void ConnectionManager::GattDataTransmittedEventHandler(const FruityHal::GattDataTransmittedEvent& gattDataTransmitted)
{
    //There are two types of events that trigger a dataTransmittedCallback
    //A TX complete event frees a number of transmit buffers
    //These are used for all connections

    if (gattDataTransmitted.IsConnectionHandleValid())
    {
        logt("CONN_DATA", "write_CMD complete (n=%d)", gattDataTransmitted.GetCompleteCount());

        //This connection has just been given back some transmit buffers
        BaseConnection* connection = GetRawConnectionFromHandle(gattDataTransmitted.GetConnectionHandle());
        if (connection == nullptr) return;

        connection->HandlePacketSent(gattDataTransmitted.GetCompleteCount(), 0);

        sentMeshPacketsUnreliable += gattDataTransmitted.GetCompleteCount();


        for (u32 i = 0; i < gattDataTransmitted.GetCompleteCount(); i++) {
            GS->logger.LogCustomCount(CustomErrorTypes::COUNT_SENT_PACKETS_UNRELIABLE);
        }

        //Next, we should continue sending packets if there are any
        if (ConnectionManager::GetInstance().GetPendingPackets())
        {
            ConnectionManager::GetInstance().FillTransmitBuffers();
        }
    }
}

void ConnectionManager::GattcWriteResponseEventHandler(const FruityHal::GattcWriteResponseEvent & writeResponseEvent)
{
    //The EVT_WRITE_RSP comes after a WRITE_REQ and notifies that a buffer
    //for one specific connection has been cleared

    if (writeResponseEvent.GetGattStatus() != FruityHal::BleGattEror::SUCCESS)
    {
        logt("ERROR", "GATT status problem %d %s", (u8)writeResponseEvent.GetGattStatus(), Logger::GetGattStatusErrorString(writeResponseEvent.GetGattStatus()));

        GS->logger.LogCount(LoggingError::GATT_STATUS, (u32)writeResponseEvent.GetGattStatus());

        //TODO: Error handling, but there really shouldn't be an error....;-)
        //FIXME: Handle possible gatt status codes

    }
    else
    {
        logt("CONN_DATA", "write_REQ complete");
        BaseConnection* connection = GetRawConnectionFromHandle(writeResponseEvent.GetConnectionHandle());

        //Connection could have been disconneced
        if (connection == nullptr) return;

        connection->HandlePacketSent(0, 1);

        sentMeshPacketsReliable++;

        GS->logger.LogCustomCount(CustomErrorTypes::COUNT_SENT_PACKETS_RELIABLE);

        //Now we continue sending packets
        if (ConnectionManager::GetInstance().GetPendingPackets())
            ConnectionManager::GetInstance().FillTransmitBuffers();
    }
}

#define _________________RECEIVING____________

void ConnectionManager::ForwardReceivedDataToConnection(u16 connectionHandle, BaseConnectionSendData & sendData, u8 const * data)
{
    logt("CM", "RX Data size is: %d, handles(%d, %d), delivery %d", sendData.dataLength.GetRaw(), connectionHandle, sendData.characteristicHandle, (u32)sendData.deliveryOption);

    char stringBuffer[100];
    Logger::ConvertBufferToHexString(data, sendData.dataLength, stringBuffer, sizeof(stringBuffer));
    logt("CM", "%s", stringBuffer);
    //Get the handling connection for this write
    BaseConnection* connection = GS->cm.GetRawConnectionFromHandle(connectionHandle);

    //Notify our connection instance that data has been received
    if (connection != nullptr) {
        connection->ReceiveDataHandler(&sendData, data);
    }
}

void ConnectionManager::GattsWriteEventHandler(const FruityHal::GattsWriteEvent& gattsWriteEvent)
{
    BaseConnectionSendData sendData;
    sendData.characteristicHandle = gattsWriteEvent.GetAttributeHandle();
    sendData.deliveryOption = (gattsWriteEvent.IsWriteRequest()) ? DeliveryOption::WRITE_REQ : DeliveryOption::WRITE_CMD;
    sendData.dataLength = gattsWriteEvent.GetLength();

    ForwardReceivedDataToConnection(gattsWriteEvent.GetConnectionHandle(), sendData, gattsWriteEvent.GetData() /*bleEvent.evt.gatts_evt.params.write->data*/);
}

void ConnectionManager::GattcHandleValueEventHandler(const FruityHal::GattcHandleValueEvent & handleValueEvent)
{
    BaseConnectionSendData sendData;

    sendData.characteristicHandle = handleValueEvent.GetHandle();
    sendData.deliveryOption = DeliveryOption::NOTIFICATION;
    sendData.dataLength = handleValueEvent.GetLength();


    ForwardReceivedDataToConnection(handleValueEvent.GetConnectionHandle(), sendData, handleValueEvent.GetData());
}

//This method accepts connPackets and distributes it to all other mesh connections
void ConnectionManager::RouteMeshData(BaseConnection* connection, BaseConnectionSendData* sendData, u8 const * data) const
{
    ConnPacketHeader const * packetHeader = (ConnPacketHeader const *) data;


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
        MeshConnectionHandle connectionSink = GS->cm.GetMeshConnectionToShortestSink(connection);

        if(GS->config.enableSinkRouting && connectionSink && !(routingDecision & ROUTING_DECISION_BLOCK_TO_MESH))
        {
            connectionSink.SendData(sendData, data);
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
        DYNAMIC_ARRAY(modifiedMessage, sendData->dataLength.GetRaw());
        if(packetHeader->receiver > NODE_ID_HOPS_BASE && packetHeader->receiver < NODE_ID_HOPS_BASE + 1000)
        {
            CheckedMemcpy(modifiedMessage, data, sendData->dataLength.GetRaw());
            ConnPacketHeader* modifiedPacketHeader = (ConnPacketHeader*)modifiedMessage;
            modifiedPacketHeader->receiver--;
            packetHeader = modifiedPacketHeader;
        }

        //TODO: We can refactor this to use the new MessageRoutingInterceptor
        //Do not forward ...
        //        ... cluster info update packets, these are handeled by the node
        //        ... timestamps, these are only directly sent to one node and propagate through the mesh by other means
        if(packetHeader->messageType != MessageType::CLUSTER_INFO_UPDATE
            && packetHeader->messageType != MessageType::UPDATE_TIMESTAMP)
        {
            //Send to all other connections
            BroadcastMeshData(connection, sendData, (const u8*)packetHeader, routingDecision);
        }
    }
}

void ConnectionManager::BroadcastMeshData(const BaseConnection* ignoreConnection, BaseConnectionSendData* sendData, u8 const * data, RoutingDecision routingDecision) const
{
    //Iterate through all mesh connections except the ignored one and send the packet
    if (!(routingDecision & ROUTING_DECISION_BLOCK_TO_MESH)) {
        MeshConnections conn = GetMeshConnections(ConnectionDirection::INVALID);
        for (u32 i = 0; i < conn.count; i++) {
            if (conn.handles[i] && conn.handles[i].GetConnection() != ignoreConnection) {
                sendData->characteristicHandle = ((MeshConnection*)conn.handles[i].GetConnection())->partnerWriteCharacteristicHandle;
                ((MeshConnection*)conn.handles[i].GetConnection())->SendData(sendData, data);
            }
        }
    }

    //Route to all MeshAccess Connections
    //Iterate through all mesh access connetions except the ignored one and send the packet
    if (!(routingDecision & ROUTING_DECISION_BLOCK_TO_MESH_ACCESS)) {
        MeshAccessConnections conn2 = GetMeshAccessConnections(ConnectionDirection::INVALID);
        for (u32 i = 0; i < conn2.count; i++) {
            MeshAccessConnectionHandle maconn = conn2.handles[i];
            if (maconn && maconn.GetConnection() != ignoreConnection) {
                maconn.SendData(data, sendData->dataLength, false);
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

bool ConnectionManager::IsValidFruityMeshPacket(const u8* data, MessageLength dataLength) const
{
    //After a packet was decripted and reassembled, it must at least have a full header
    if(dataLength < SIZEOF_CONN_PACKET_HEADER){
        SIMEXCEPTION(MessageTooSmallException);
        return false;
    }
    
    const ConnPacketHeader* header = (const ConnPacketHeader*)data;

    if (dataLength < MessageTypeToMinimumPacketSize(header->messageType))
    {
        SIMEXCEPTION(MessageTooSmallException);
        return false;
    }

    return true;
}

u32 ConnectionManager::MessageTypeToMinimumPacketSize(MessageType messageType)
{
    switch (messageType)
    {
    case MessageType::SPLIT_WRITE_CMD:
        return SIZEOF_CONN_PACKET_SPLIT_HEADER;
    case MessageType::SPLIT_WRITE_CMD_END:
        return SIZEOF_CONN_PACKET_SPLIT_HEADER;
    case MessageType::CLUSTER_WELCOME:
        return SIZEOF_CONN_PACKET_PAYLOAD_CLUSTER_WELCOME;
    case MessageType::CLUSTER_ACK_1:
        return SIZEOF_CONN_PACKET_CLUSTER_ACK_1;
    case MessageType::CLUSTER_ACK_2:
        return SIZEOF_CONN_PACKET_CLUSTER_ACK_2;
    case MessageType::CLUSTER_INFO_UPDATE:
        return SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE;
    case MessageType::RECONNECT:
        return SIZEOF_CONN_PACKET_RECONNECT;
    case MessageType::ENCRYPT_CUSTOM_START:
        return SIZEOF_CONN_PACKET_ENCRYPT_CUSTOM_START;
    case MessageType::ENCRYPT_CUSTOM_ANONCE:
        return SIZEOF_CONN_PACKET_ENCRYPT_CUSTOM_ANONCE;
    case MessageType::ENCRYPT_CUSTOM_SNONCE:
        return SIZEOF_CONN_PACKET_ENCRYPT_CUSTOM_SNONCE;
    case MessageType::ENCRYPT_CUSTOM_DONE:
        return SIZEOF_CONN_PACKET_ENCRYPT_CUSTOM_DONE;
    case MessageType::UPDATE_TIMESTAMP:
        return SIZEOF_CONN_PACKET_UPDATE_TIMESTAMP;
    case MessageType::UPDATE_CONNECTION_INTERVAL:
        return SIZEOF_CONN_PACKET_UPDATE_CONNECTION_INTERVAL;
    case MessageType::ASSET_LEGACY:
        return SIZEOF_SCAN_MODULE_TRACKED_ASSET_LEGACY;
    case MessageType::CAPABILITY:
        return sizeof(CapabilityRequestedMessage);
    case MessageType::ASSET_GENERIC:
        return ScanningModule::SIZEOF_TRACKED_ASSET_MESSAGE_WITH_CONN_PACKET_HEADER;
#if IS_ACTIVE(SIG_MESH)
    case MessageType::SIG_MESH_SIMPLE:
        return SIZEOF_SIMPLE_SIG_MESSAGE;
#endif
    case MessageType::MODULE_CONFIG:
        return SIZEOF_CONN_PACKET_MODULE;
    case MessageType::MODULE_TRIGGER_ACTION:
        return SIZEOF_CONN_PACKET_MODULE;
    case MessageType::MODULE_ACTION_RESPONSE:
        return SIZEOF_CONN_PACKET_MODULE;
    case MessageType::MODULE_GENERAL:
        return SIZEOF_CONN_PACKET_MODULE;
    case MessageType::MODULE_RAW_DATA:
        return SIZEOF_CONN_PACKET_MODULE;
    case MessageType::MODULE_RAW_DATA_LIGHT:
        return SIZEOF_CONN_PACKET_MODULE;
    case MessageType::COMPONENT_ACT:
        return SIZEOF_CONN_PACKET_COMPONENT_MESSAGE;
    case MessageType::COMPONENT_SENSE:
        return SIZEOF_CONN_PACKET_COMPONENT_MESSAGE;
    case MessageType::TIME_SYNC:
        return sizeof(TimeSyncHeader);
    case MessageType::DEAD_DATA:
        return sizeof(DeadDataMessage);
    case MessageType::DATA_1:
        return SIZEOF_CONN_PACKET_HEADER;
    case MessageType::DATA_1_VITAL:
        return SIZEOF_CONN_PACKET_HEADER;
    case MessageType::CLC_DATA:
        return SIZEOF_CONN_PACKET_HEADER;
    case MessageType::INVALID:
        // Fall-through
    case MessageType::RESERVED_BIT_END:
        // Fall-through
    case MessageType::RESERVED_BIT_START:
        // Fall-through
    default:
        //Either not valid or it is missing in the list.
        SIMEXCEPTION(NotAValidMessageTypeException);
        return 0xFFFFFFFF; //Make sure that an unknown MessageType is never accepted.
    }
}

#define _________________CONNECTIONS____________

//Called as soon as a new connection is made, either as central or peripheral
void ConnectionManager::GapConnectionConnectedHandler(const FruityHal::GapConnectedEvent & connectedEvent)
{
    ErrorType err;
    FruityHal::BleGapAddrBytes peerAddr = connectedEvent.GetPeerAddr();

    StatusReporterModule* statusMod = (StatusReporterModule*)GS->node.GetModuleById(ModuleId::STATUS_REPORTER_MODULE);
    if(statusMod != nullptr){
        u32 addrPart;
        CheckedMemcpy(&addrPart, peerAddr.data(), 4);

        if(connectedEvent.GetRole() == FruityHal::GapRole::PERIPHERAL){
            statusMod->SendLiveReport(LiveReportTypes::GAP_CONNECTED_INCOMING, 0, connectedEvent.GetConnectionHandle(), addrPart);
        } else if(connectedEvent.GetRole() == FruityHal::GapRole::CENTRAL){
            statusMod->SendLiveReport(LiveReportTypes::GAP_CONNECTED_OUTGOING, 0, connectedEvent.GetConnectionHandle(), addrPart);
        }
    }


    logt("CM", "Connection handle %u success as %s, partner:%02x:%02x:%02x:%02x:%02x:%02x", 
        connectedEvent.GetConnectionHandle(), 
        connectedEvent.GetRole() == FruityHal::GapRole::CENTRAL ? "Central" : "Peripheral", 
        peerAddr[5], 
        peerAddr[4],
        peerAddr[3], 
        peerAddr[2], 
        peerAddr[1], 
        peerAddr[0]);

    GS->logger.LogCustomCount(CustomErrorTypes::COUNT_CONNECTION_SUCCESS);

    BaseConnection* reestablishedConnection = IsConnectionReestablishment(connectedEvent);

    /* TODO: Part A: We have a connection reestablishment */
    if (reestablishedConnection != nullptr)
    {
        reestablishedConnection->GapReconnectionSuccessfulHandler(connectedEvent);

        //Check if there is another connection in reestablishing state that we can try to reconnect
        MeshConnections conns = GetMeshConnections(ConnectionDirection::DIRECTION_OUT);
        for (u32 i = 0; i < conns.count; i++) {
            if (conns.handles[i].GetConnectionState() == ConnectionState::REESTABLISHING) {
                conns.handles[i].TryReestablishing();
            }
        }

        if (connectedEvent.GetRole() == FruityHal::GapRole::PERIPHERAL)
        {
            //The Peripheral should wait until the encryption request was made
            reestablishedConnection->encryptionState = EncryptionState::NOT_ENCRYPTED;
        }
        else if (connectedEvent.GetRole() == FruityHal::GapRole::CENTRAL)
        {
            //If encryption is enabled, the central starts to encrypt the connection
            reestablishedConnection->encryptionState = EncryptionState::ENCRYPTING;
            GS->gapController.StartEncryptingConnection(connectedEvent.GetConnectionHandle());
        }

        return;
    }

    /* Part B: A normal incoming/outgoing connection */
    if (GetConnectionInHandshakeState().IsValid())
    {
        logt("CM", "Currently in handshake, disconnect");

        //If we have a pendingConnection for this, we must clean it
        if(connectedEvent.GetRole() == FruityHal::GapRole::CENTRAL){
            if(pendingConnection != nullptr){
                DeleteConnection(pendingConnection, AppDisconnectReason::CURRENTLY_IN_HANDSHAKE);
            }
        }
        err = FruityHal::Disconnect(connectedEvent.GetConnectionHandle(), FruityHal::BleHciError::REMOTE_USER_TERMINATED_CONNECTION);

        return;
    }

    BaseConnection* c = nullptr;

    //We are slave (peripheral)
    if (connectedEvent.GetRole() == FruityHal::GapRole::PERIPHERAL)
    {
        logt("CM", "Incoming Connection connected");

        //Check if we have a free entry in our connections array
        //It might happen that we have not, because we have not yet received a disconnect event but a connection was already disconnected
        i8 id = GetFreeConnectionSpot();
        if(id < 0){
            logt("CM", "No spot available");
            
            //We must drop the connection
            GS->logger.LogCustomError(CustomErrorTypes::WARN_CM_FAIL_NO_SPOT, 0);
            const ErrorType err = FruityHal::Disconnect(connectedEvent.GetConnectionHandle(), FruityHal::BleHciError::REMOTE_USER_TERMINATED_CONNECTION);
            if (err != ErrorType::SUCCESS)
            {
                logt("ERROR", "Failed to drop connection because %u", (u32)err);
            }

            return;
        }



        FruityHal::BleGapAddr peerAddress;
        CheckedMemset(&peerAddress, 0, sizeof(peerAddress));
        peerAddress.addr_type = (FruityHal::BleGapAddrType)connectedEvent.GetPeerAddrType();
        peerAddress.addr = connectedEvent.GetPeerAddr();

        c = allConnections[id] = ConnectionAllocator::GetInstance().AllocateResolverConnection(id, ConnectionDirection::DIRECTION_IN, &peerAddress);
        c->ConnectionSuccessfulHandler(connectedEvent.GetConnectionHandle());


        //The central may now start encrypting or start the handshake, we just have to wait
    }
    //We are master (central)
    else if (connectedEvent.GetRole() == FruityHal::GapRole::CENTRAL)
    {
        //This can happen if the connection has been cleaned up already e.g. by disconnecting all connections but the connection was accepted in the meantime
        if(pendingConnection == nullptr){
            logt("WARNING", "No pending Connection");
            GS->logger.LogCustomCount(CustomErrorTypes::COUNT_NO_PENDING_CONNECTION);
            err = FruityHal::Disconnect(connectedEvent.GetConnectionHandle(), FruityHal::BleHciError::REMOTE_USER_TERMINATED_CONNECTION);
            return;
        }

        c = pendingConnection;
        pendingConnection = nullptr;

        //Call Prepare again so that the clusterID and size backup are created with up to date values
        c->ConnectionSuccessfulHandler(connectedEvent.GetConnectionHandle());

        //If encryption is enabled, the central starts to encrypt the connection
        if (Conf::encryptionEnabled && c->connectionType == ConnectionType::FRUITYMESH){
            c->encryptionState = EncryptionState::ENCRYPTING;
            GS->gapController.StartEncryptingConnection(connectedEvent.GetConnectionHandle());
        }
    }
}

//If we wanted to connect but our connection timed out (only outgoing connections)
void ConnectionManager::GapConnectingTimeoutHandler(const FruityHal::GapTimeoutEvent &gapTimeoutEvent)
{
    if (pendingConnection == nullptr)
        return;

    DeleteConnection(pendingConnection, AppDisconnectReason::GAP_CONNECTING_TIMEOUT);
}

//FIXME: Still needs rewriting
//When a connection changes to encrypted
void ConnectionManager::GapConnectionEncryptedHandler(const FruityHal::GapConnectionSecurityUpdateEvent &connectionSecurityUpdateEvent)
{
    BaseConnection* c = GetRawConnectionFromHandle(connectionSecurityUpdateEvent.GetConnectionHandle());

    if (c == nullptr) return; //Connection might have been disconnected already

    logt("CM", "Connection id %u is now encrypted", c->connectionId);
    c->encryptionState = EncryptionState::ENCRYPTED;

    GapConnectionReadyForHandshakeHandler(c);
}

//This is called as soon as a connection has undergone e.g. encryption / mtu exchange / ... and is now ready to send data
void ConnectionManager::GapConnectionReadyForHandshakeHandler(BaseConnection* c)
{
    //If we are reestablishing, initiate the reestablishing handshake from the central side
    if(c->connectionType == ConnectionType::FRUITYMESH
        && c->direction == ConnectionDirection::DIRECTION_OUT)
    {
        if (c->connectionState == ConnectionState::REESTABLISHING_HANDSHAKE
            )
        {
            ((MeshConnection*)c)->SendReconnectionHandshakePacket();
        }
        else if(c->connectionState == ConnectionState::CONNECTED)
        {
            ((MeshConnection*)c)->StartHandshake();
        }
    }
}

ErrorType ConnectionManager::RequestDataLengthExtensionAndMtuExchange(BaseConnection* c)
{
    //Request a higher MTU for the GATT Layer, errors are ignored as there are non that need to be handeled
    ErrorType err = FruityHal::BleGattMtuExchangeRequest(c->connectionHandle, FruityHal::BleGattGetMaxMtu());

    //Request Data Length Extension (DLE) for the Link Layer packets, errors are ignored as there are non that need to be handeled
    if (err == ErrorType::SUCCESS) {
        err = FruityHal::BleGapDataLengthExtensionRequest(c->connectionHandle);
    }

    return err;
}

void ConnectionManager::MtuUpdatedHandler(u16 connHandle, u16 mtu)
{
    BaseConnection* conn = GetRawConnectionFromHandle(connHandle);

    if(conn == nullptr) return;

    conn->ConnectionMtuUpgradedHandler(mtu - FruityHal::ATT_HEADER_SIZE);
}

//Is called whenever a connection had been established and is now disconnected
//due to a timeout, deliberate disconnection by the localhost, remote, etc,...
//We might however decide to sustain it. it will only be lost after
//the finalDisconnectionHander is called
void ConnectionManager::GapConnectionDisconnectedHandler(const FruityHal::GapDisconnectedEvent& disconnectedEvent)
{
    BaseConnection* connection = GetRawConnectionFromHandle(disconnectedEvent.GetConnectionHandle());

    StatusReporterModule* statusMod = (StatusReporterModule*)GS->node.GetModuleById(ModuleId::STATUS_REPORTER_MODULE);
    if (statusMod != nullptr) {
        statusMod->SendLiveReport(LiveReportTypes::WARN_GAP_DISCONNECTED, 0, connection == nullptr ? 0 : connection->partnerId, (u32)disconnectedEvent.GetReason());
    }

    if(connection == nullptr) return;


    logt("CM", "Gap Connection handle %u disconnected", disconnectedEvent.GetConnectionHandle());

    GS->logger.LogCount(LoggingError::HCI_ERROR, (u32)disconnectedEvent.GetReason());

    //Notify the connection itself
    bool result = connection->GapDisconnectionHandler(disconnectedEvent.GetReason());

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
    logt("WARNING", "BLE_GATTC_EVT_TIMEOUT");

    BaseConnectionHandle connection = GetConnectionFromHandle(gattcTimeoutEvent.GetConnectionHandle());

    connection.DisconnectAndRemove(AppDisconnectReason::GATTC_TIMEOUT);

    GS->logger.LogCustomError(CustomErrorTypes::FATAL_BLE_GATTC_EVT_TIMEOUT_FORCED_US, 0);
}

#define _________________HELPERS____________

i8 ConnectionManager::GetFreeConnectionSpot() const
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
BaseConnectionHandle ConnectionManager::GetConnectionFromHandle(u16 connectionHandle) const
{
    BaseConnection* bc = GetRawConnectionFromHandle(connectionHandle);
    if (bc)
    {
        return BaseConnectionHandle(*bc);
    }
    else
    {
        return BaseConnectionHandle();
    }
}

//Looks through all connections for the right handle and returns the right one
BaseConnectionHandle ConnectionManager::GetConnectionByUniqueId(u32 uniqueConnectionId) const
{
    BaseConnection* bc = GetRawConnectionByUniqueId(uniqueConnectionId);
    if (bc)
    {
        return BaseConnectionHandle(*bc);
    }
    else
    {
        return BaseConnectionHandle();
    }
}

MeshAccessConnectionHandle ConnectionManager::GetMeshAccessConnectionByUniqueId(u32 uniqueConnectionId) const
{
    BaseConnection* bc = GetRawConnectionByUniqueId(uniqueConnectionId);
    if (bc && bc->connectionType == ConnectionType::MESH_ACCESS)
    {
        return MeshAccessConnectionHandle(*(MeshAccessConnection*)bc);
    }
    else
    {
        return MeshAccessConnectionHandle();
    }
}

BaseConnections ConnectionManager::GetBaseConnections(ConnectionDirection direction) const{
    BaseConnections fc;
    for(u32 i=0; i<TOTAL_NUM_CONNECTIONS; i++){
        if(allConnections[i] != nullptr){
            if(allConnections[i]->direction == direction || direction == ConnectionDirection::INVALID){
                fc.handles[fc.count] = BaseConnectionHandle(*allConnections[i]);
                fc.count++;
            }
        }
    }
    return fc;
}

MeshConnections ConnectionManager::GetMeshConnections(ConnectionDirection direction) const{
    MeshConnections fc;
    for(u32 i=0; i<TOTAL_NUM_CONNECTIONS; i++){
        if(allConnections[i] != nullptr){
            if(allConnections[i]->connectionType == ConnectionType::FRUITYMESH){
                if(allConnections[i]->direction == direction || direction == ConnectionDirection::INVALID){
                    fc.handles[fc.count] = MeshConnectionHandle(*(MeshConnection*)allConnections[i]);
                    fc.count++;
                }
            }
        }
    }
    return fc;
}

MeshAccessConnections ConnectionManager::GetMeshAccessConnections(ConnectionDirection direction) const
{
    MeshAccessConnections fc;
    for (u32 i = 0; i < TOTAL_NUM_CONNECTIONS; i++) {
        if (allConnections[i] != nullptr) {
            if (allConnections[i]->connectionType == ConnectionType::MESH_ACCESS) {
                if (allConnections[i]->direction == direction || direction == ConnectionDirection::INVALID) {
                    fc.handles[fc.count] = MeshAccessConnectionHandle(*(MeshAccessConnection*)allConnections[i]);
                    fc.count++;
                }
            }
        }
    }
    return fc;
}

MeshConnectionHandle ConnectionManager::GetConnectionInHandshakeState() const
{
    for(u32 i=0; i<TOTAL_NUM_CONNECTIONS; i++){
        if(allConnections[i] != nullptr && allConnections[i]->connectionType == ConnectionType::FRUITYMESH){
            if(allConnections[i]->connectionState == ConnectionState::HANDSHAKING){
                return MeshConnectionHandle(*(MeshConnection*)allConnections[i]);
            }
        }
    }
    return MeshConnectionHandle();
}

BaseConnections ConnectionManager::GetConnectionsOfType(ConnectionType connectionType, ConnectionDirection direction) const{
    BaseConnections fc;
    for(u32 i=0; i<TOTAL_NUM_CONNECTIONS; i++){
        if(allConnections[i] != nullptr){
            if(allConnections[i]->connectionType == connectionType || connectionType == ConnectionType::INVALID){
                if(allConnections[i]->direction == direction || direction == ConnectionDirection::INVALID){
                    fc.handles[fc.count] = BaseConnectionHandle(*allConnections[i]);
                    fc.count++;
                }
            }
        }
    }
    return fc;
}

u16 ConnectionManager::GetSmallestMtuOfAllConnections() const
{
    u16 retVal = 0xFFFF;
    BaseConnections conns = GetBaseConnections(ConnectionDirection::INVALID);
    for (u32 i = 0; i < conns.count; i++)
    {
        if (conns.handles[i])
        {
            BaseConnection* c = conns.handles[i].GetConnection();
            if (c->connectionMtu < retVal)
            {
                retVal = c->connectionMtu;
            }
        }
    }

    return retVal;
}

//Looks through all connections for the right handle and returns the right one
MeshConnectionHandle ConnectionManager::GetMeshConnectionToPartner(NodeId partnerId) const
{
    for (u32 i = 0; i < TOTAL_NUM_CONNECTIONS; i++) {
        if (
            allConnections[i] != nullptr
            && allConnections[i]->connectionType == ConnectionType::FRUITYMESH
            && allConnections[i]->partnerId == partnerId
        ) {
            return MeshConnectionHandle(*(MeshConnection*)allConnections[i]);
        }
    }
    return MeshConnectionHandle();
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
            if (connectedEvent.GetPeerAddr() == allConnections[i]->partnerAddress.addr
                && (FruityHal::BleGapAddrType)connectedEvent.GetPeerAddrType() == allConnections[i]->partnerAddress.addr_type)
            {
                logt("CM", "Found existing connection id %u", allConnections[i]->connectionId);
                return allConnections[i];
            }
        }
    }
    return nullptr;
}

BaseConnection * ConnectionManager::GetRawConnectionByUniqueId(u32 uniqueConnectionId) const
{
    for (u32 i = 0; i < TOTAL_NUM_CONNECTIONS; i++) {
        if (allConnections[i] != nullptr && allConnections[i]->uniqueConnectionId == uniqueConnectionId) {
            return allConnections[i];
        }
    }
    return nullptr;
}

BaseConnection * ConnectionManager::GetRawConnectionFromHandle(u16 connectionHandle) const
{
    for (u32 i = 0; i < TOTAL_NUM_CONNECTIONS; i++) {
        if (allConnections[i] != nullptr && allConnections[i]->connectionHandle == connectionHandle) {
            return allConnections[i];
        }
    }
    return nullptr;
}

//TODO: Only return mesh connections, check
MeshConnectionHandle ConnectionManager::GetMeshConnectionToShortestSink(const BaseConnection* excludeConnection) const
{
    ClusterSize min = INT16_MAX;
    MeshConnectionHandle c;
    MeshConnections conn = GetMeshConnections(ConnectionDirection::INVALID);
    for (int i = 0; i < conn.count; i++)
    {
        if (excludeConnection != nullptr && conn.handles[i].GetConnection() == excludeConnection)
            continue;
        if (conn.handles[i].IsHandshakeDone() && conn.handles[i].GetHopsToSink() > -1 && conn.handles[i].GetHopsToSink() < min)
        {
            min = conn.handles[i].GetHopsToSink();
            c = conn.handles[i];
        }
    }
    return c;
}

ClusterSize ConnectionManager::GetMeshHopsToShortestSink(const BaseConnection* excludeConnection) const
{
    if (GET_DEVICE_TYPE() == DeviceType::SINK)
    {
        logt("SINK", "HOPS 0, clID:%x, clSize:%d", GS->node.clusterId, GS->node.GetClusterSize());
        return 0;
    }
    else
    {
        ClusterSize min = INT16_MAX;
        MeshConnectionHandle c;
        MeshConnections conn = GetMeshConnections(ConnectionDirection::INVALID);
        for(int i=0; i<conn.count; i++)
        {
            if(conn.handles[i].GetConnection() == excludeConnection || !conn.handles[i].IsHandshakeDone()) continue;
            if(conn.handles[i].GetHopsToSink() > -1 && conn.handles[i].GetHopsToSink() < min)
            {
                min = conn.handles[i].GetHopsToSink();
                c = conn.handles[i];
            }
        }

        const ClusterSize hopsToSink = c ? c.GetHopsToSink() : -1;

        logt("SINK", "HOPS %d, clID:%x, clSize:%d", hopsToSink, GS->node.clusterId, GS->node.GetClusterSize());
        return hopsToSink;
    }
}

#define _________________EVENTS____________

void ConnectionManager::GapRssiChangedEventHandler(const FruityHal::GapRssiChangedEvent & rssiChangedEvent) const
{
    BaseConnection* connection = GetRawConnectionFromHandle(rssiChangedEvent.GetConnectionHandle());
    if (connection != nullptr) {
        i8 rssi = rssiChangedEvent.GetRssi();
        connection->lastReportedRssi = rssi;

        if (connection->rssiAverageTimes1000 == 0) connection->rssiAverageTimes1000 = (i32)rssi * 1000;

        //=> The averaging is done in the timerEventHandler
    }
}

void ConnectionManager::TimerEventHandler(u16 passedTimeDs)
{
    //Check if there are unsent packet (Can happen if the softdevice was busy and it was not possible to queue packets the last time)
    if (SHOULD_IV_TRIGGER(GS->appTimerDs, passedTimeDs, SEC_TO_DS(1)) && GetPendingPackets() > 0) {
        FillTransmitBuffers();
    }

    {
        //Go through all connections to do periodic cleanup tasks and other periodic work
        BaseConnections conns = GetConnectionsOfType(ConnectionType::INVALID, ConnectionDirection::INVALID);

        for (u32 i = 0; i < conns.count; i++) {
            BaseConnection* conn = conns.handles[i].GetConnection();
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
                    GS->logger.LogCustomError(CustomErrorTypes::FATAL_PENDING_NOT_CLEARED, error);

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

                GS->logger.LogCustomError(CustomErrorTypes::WARN_HANDSHAKE_TIMEOUT, conn->partnerId);

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

                GS->logger.LogCustomError(CustomErrorTypes::WARN_CONNECTION_SUSTAIN_FAILED_TO_ESTABLISH, conn->partnerId);

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
            MeshConnection* conn = static_cast<MeshConnection*>(conns.handles[i].GetConnection());
            if (conn == nullptr)
            {
                // The Connection was already removed
                SIMEXCEPTION(IllegalStateException);
                GS->logger.LogCustomError(CustomErrorTypes::FATAL_CONNECTION_REMOVED_WHILE_TIME_SYNC, 1000);
                continue;
            }

            if (conn->HandshakeDone() == false) continue;

            if (conn->timeSyncState == MeshConnection::TimeSyncState::UNSYNCED)
            {
                alignas(u32) TimeSyncInitial dataToSend = GS->timeManager.GetTimeSyncIntialMessage(conn->partnerId);

                conn->syncSendingOrdered = GS->timeManager.GetTimePoint();

                logt("TSYNC", "Sending out TimeSyncInitial, NodeId: %u, partner: %u", (u32)GS->node.configuration.nodeId, (u32)conn->partnerId);

                GS->cm.SendMeshMessage(
                    (u8*)&dataToSend,
                    sizeof(TimeSyncInitial));
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
#ifdef SIM_ENABLED
                if (!conn->correctionTicksSuccessfullyWritten)
                {
                    // Implementation error! This means that we tried to send out a correction
                    // that wasn't even written by the MessageSentHandler. Must not happen!
                    // IOT-4554: Activate the following line once this ticket is fixed!
                    SIMEXCEPTION(IllegalStateException);
                }
#endif

                logt("TSYNC", "Sending out TimeSyncCorrection, NodeId: %u, partner: %u", (u32)GS->node.configuration.nodeId, (u32)conn->partnerId);

                GS->cm.SendMeshMessage(
                    (u8*)&dataToSend,
                    sizeof(TimeSyncCorrection));
            }
        }
    }

    // Enolled nodes syncing
    timeSinceLastEnrolledNodesSyncDs += passedTimeDs;
    if(timeSinceLastEnrolledNodesSyncDs >= ENROLLED_NODES_SYNC_INTERVALS_DS)
    {
        timeSinceLastEnrolledNodesSyncDs = 0;
        MeshConnections conns = GetMeshConnections(ConnectionDirection::INVALID);

        for (u32 i = 0; i < conns.count; i++)
        {
            MeshConnectionHandle handle = conns.handles[i];
            if (!handle) 
            {
                // The Connection was already removed
                SIMEXCEPTION(IllegalStateException);
                GS->logger.LogCustomError(CustomErrorTypes::FATAL_CONNECTION_REMOVED_WHILE_ENROLLED_NODES_SYNC, 1000);
                continue;
            }

            if (handle.IsHandshakeDone() == false) continue;

            if (handle.GetEnrolledNodesSync() == false)
            {
                GS->node.SendEnrolledNodes(GS->node.configuration.numberOfEnrolledDevices, handle.GetPartnerId());
            }
        }
    }

#if IS_ACTIVE(CONN_PARAM_UPDATE)
    // Connection interval update for long term connections.
    UpdateConnectionIntervalForLongTermMeshConnections();
#endif
}

void ConnectionManager::ResetTimeSync()
{
    BaseConnections conns = GetConnectionsOfType(ConnectionType::FRUITYMESH, ConnectionDirection::INVALID);

    for (u32 i = 0; i < conns.count; i++)
    {
        MeshConnection* conn = static_cast<MeshConnection*>(conns.handles[i].GetConnection());
        if (conn == nullptr)
        {
            // The Connection was already removed
            SIMEXCEPTION(IllegalStateException);
            GS->logger.LogCustomError(CustomErrorTypes::FATAL_CONNECTION_REMOVED_WHILE_TIME_SYNC, 2000);
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
        const MeshConnection* conn = static_cast<MeshConnection*>(conns.handles[i].GetConnection());
        if (conn == nullptr)
        {
            // The Connection was already removed, should not happen
            SIMEXCEPTION(IllegalStateException);
            GS->logger.LogCustomError(CustomErrorTypes::FATAL_CONNECTION_REMOVED_WHILE_TIME_SYNC, 5000);
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
        MeshConnection* conn = static_cast<MeshConnection*>(conns.handles[i].GetConnection());
        if (conn == nullptr)
        {
            // The Connection was already removed
            SIMEXCEPTION(IllegalStateException);
            GS->logger.LogCustomError(CustomErrorTypes::FATAL_CONNECTION_REMOVED_WHILE_TIME_SYNC, 3000);
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
        MeshConnection* conn = static_cast<MeshConnection*>(conns.handles[i].GetConnection());
        if (conn == nullptr)
        {
            // The Connection was already removed
            SIMEXCEPTION(IllegalStateException);
            GS->logger.LogCustomError(CustomErrorTypes::FATAL_CONNECTION_REMOVED_WHILE_TIME_SYNC, 4000);
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

void ConnectionManager::SetEnrolledNodesReceived(NodeId sender)
{
    MeshConnections conns = GetMeshConnections(ConnectionDirection::INVALID);

    for (u32 i = 0; i < conns.count; i++)
    {
        MeshConnectionHandle handle = conns.handles[i];
        if (!handle) 
        {
            // The Connection was already removed
            SIMEXCEPTION(IllegalStateException);
            GS->logger.LogCustomError(CustomErrorTypes::FATAL_CONNECTION_REMOVED_WHILE_ENROLLED_NODES_SYNC, 1000);
            continue;
        }


        if (handle.GetPartnerId() == sender)
        {
            handle.SetEnrolledNodesSync(true);    
        }
        else
        {
            handle.SetEnrolledNodesSync(false);
        }
    }
}

void ConnectionManager::SetEnrolledNodesReplyReceived(NodeId sender, u16 enrolledNodes)
{
    MeshConnections conns = GetMeshConnections(ConnectionDirection::INVALID);

    for (u32 i = 0; i < conns.count; i++)
    {
        MeshConnectionHandle handle = conns.handles[i];
        if (!handle) 
        {
            // The Connection was already removed
            SIMEXCEPTION(IllegalStateException);
            GS->logger.LogCustomError(CustomErrorTypes::FATAL_CONNECTION_REMOVED_WHILE_ENROLLED_NODES_SYNC, 1000);
            continue;
        }

        if (handle.GetPartnerId() == sender)
        {
            if ((handle.GetEnrolledNodesSync() == false) && (enrolledNodes == GS->node.configuration.numberOfEnrolledDevices))
            {
                handle.SetEnrolledNodesSync(true);
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

