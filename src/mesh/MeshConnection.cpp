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

MeshConnection::MeshConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress, u16 partnerWriteCharacteristicHandle)
    : BaseConnection(id, direction, partnerAddress)
{
    logt("CONN", "New MeshConnection");
    //Initialize to defaults
    connectionType = ConnectionType::FRUITYMESH;
    connectedClusterId = 0;
    connectedClusterSize = 0;
    connectionMasterBit = 0;
    CheckedMemset(&clusterAck1Packet, 0x00, sizeof(ConnPacketClusterAck1));
    CheckedMemset(&clusterAck2Packet, 0x00, sizeof(ConnPacketClusterAck2));
    clusterIDBackup = 0;
    clusterSizeBackup = 0;
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

BaseConnection* MeshConnection::ConnTypeResolver(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8 const * data)
{
    logt("MACONN", "MeshConnResolver");

    //Check if the message was written to our mesh characteristic
    if(sendData->characteristicHandle == GS->node.meshService.sendMessageCharacteristicHandle.valueHandle)
    {
        //Check if we already have an inConnection
        MeshConnections conn = GS->cm.GetMeshConnections(ConnectionDirection::DIRECTION_IN);
        if(conn.count >= GS->config.meshMaxInConnections){
            logt("CM", "Too many mesh in connections");
            const ErrorType err = FruityHal::Disconnect(oldConnection->connectionHandle, FruityHal::BleHciError::REMOTE_USER_TERMINATED_CONNECTION);
            if (err != ErrorType::SUCCESS)
            {
                logt("CM", "Failed to disconnect because %u", (u32)err);
            }
            return nullptr;
        }
        else
        {
            MeshConnection* newConnection = ConnectionAllocator::GetInstance().AllocateMeshConnection(
                oldConnection->connectionId,
                oldConnection->direction,
                &oldConnection->partnerAddress,
                FH_BLE_GATT_HANDLE_INVALID);

            newConnection->handshakeStartedDs = oldConnection->handshakeStartedDs;
            newConnection->connectionMtu = oldConnection->connectionMtu;
            newConnection->connectionPayloadSize = oldConnection->connectionPayloadSize;

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
    NodeId partnerIdBackup = partnerId;

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

    //WARNING: Make sure to not send packets before the connection was removed as this will result
    //in an infinite loop, causing a stack overflow

    //Will kill the connection. Deletion is at the end of the function.
    //Do not use members after the following line! USE-AFTER-FREE!
    BaseConnection::DisconnectAndRemove(reason);

    //Send a live report into the remaining mesh with the reason for the issue
    if (connectionStateBeforeDisconnection >= ConnectionState::HANDSHAKE_DONE) {
        logjson("SIM", "{\"type\":\"mesh_disconnect\",\"partnerId\":%u}" SEP, partnerIdBackup);

        StatusReporterModule* statusMod = (StatusReporterModule*)GS->node.GetModuleById(ModuleId::STATUS_REPORTER_MODULE);
        if (statusMod != nullptr) {
            statusMod->SendLiveReport(LiveReportTypes::HANDSHAKED_MESH_DISCONNECTED, 0, partnerIdBackup, (u8)appDisconnectionReason);
        }
    }

    //Log the error if reestablishing failed
    if(connectionStateBeforeDisconnection >= ConnectionState::REESTABLISHING){
        GS->logger.LogCustomError(CustomErrorTypes::WARN_CONNECTION_SUSTAIN_FAILED, (u8)appDisconnectionReason);
    }

    //Use our backup variables to tell the node about the lost connection
    //Is safe to call after deletion. All variables were backuped at the start of this function.
    GS->node.MeshConnectionDisconnectedHandler(
        appDisconnectionReason,
        connectionStateBeforeDisconnection,
        hadConnectionMasterBit,
        connectedClusterSize,
        connectedClusterId);
}

bool MeshConnection::GapDisconnectionHandler(const FruityHal::BleHciError hciDisconnectReason)
{
    logt("CONN", "disconnection handler"); 

    BaseConnection::GapDisconnectionHandler(hciDisconnectReason);

#if IS_ACTIVE(CONN_PARAM_UPDATE)
    // Reset the flag indicating that the long term connection inteval was
    // requested, such that after re-establishment the parameters will be
    // updated again.
    if (longTermConnectionIntervalRequested)
    {
        longTermConnectionIntervalRequested = false;
#if IS_ACTIVE(CONN_PARAM_UPDATE_LOGGING)
        logt("CONN", "Long-term connection state was reset due to disconnect");
#endif
    }
#endif

    //Check if we are a leaf node, do not try to reconnect, probably out of range
    if(direction == ConnectionDirection::DIRECTION_IN && GET_DEVICE_TYPE() == DeviceType::LEAF){
        GS->logger.LogCustomError(CustomErrorTypes::INFO_IGNORING_CONNECTION_SUSTAIN_LEAF, partnerId);
        return true;
    }
    //TODO: Check if our partner is a leaf node, do not try to reconnect, probably out of range
    //TODO: We need to know the devicetype of our partner which we should send in the handshake message
//    else if(direction == ConnectionDirection::OUT && partnerDeviceType == DeviceType::LEAF){
//        return true;
//    }

    u16 err = 0;

    //If connection reestablishment is disabled we do not reestablish
    if (Conf::meshExtendedConnectionTimeoutSec == 0) err |= 1 << 0; //1

    //The connection must have done a mesh handshake before, if it disconnects again during reestablishment, we retry reestablishment
    if (connectionStateBeforeDisconnection < ConnectionState::HANDSHAKE_DONE) err |= 1 << 1; //2

    //If the connection was disconnected on purpose, we do not reestablish it
    if (hciDisconnectReason == FruityHal::BleHciError::LOCAL_HOST_TERMINATED_CONNECTION)  err |= 1 << 2;    //4
    if (hciDisconnectReason == FruityHal::BleHciError::REMOTE_USER_TERMINATED_CONNECTION) err |= 1 << 3;    //8

    //The connection must have been stable for some time after the initial handshake
    if (GS->appTimerDs - connectionHandshakedTimestampDs <= SEC_TO_DS(10)) err |= 1 << 4; //16

    //We want to reestablish, so we do not want the connection to be killed, we only want this
    //if reestablishing is activated and if the connection was handshaked for some time
    //For testing, we can also reestablish using the forceReestablish variable
    if(err == 0){
        logt("CONN", "Trying reconnect");

        GS->logger.LogCustomError(CustomErrorTypes::INFO_TRYING_CONNECTION_SUSTAIN, partnerId);

        connectionState = ConnectionState::REESTABLISHING;
        
        //Set the reestablishment started time only if the connection was stable before
        if (connectionStateBeforeDisconnection == ConnectionState::HANDSHAKE_DONE) {
            reestablishmentStartedDs = GS->appTimerDs;
        }

        if(direction == ConnectionDirection::DIRECTION_OUT){
            //Try to connect to the peripheral again
            TryReestablishing();
        } else {
            //Enable fast advertising on the peripheral to guarantee a fast reestablishing
            GS->node.StartFastJoinMeAdvertising();
        }


        return false;
    }
    //We are okay with the MeshConnection being dropped
    else
    {
        GS->logger.LogCustomError(CustomErrorTypes::INFO_IGNORING_CONNECTION_SUSTAIN, err);
        // => CM will kill the connection for us
        return true;
    }
}

#if IS_ACTIVE(CONN_PARAM_UPDATE)
void MeshConnection::GapConnParamUpdateHandler(
        const FruityHal::BleGapConnParams & params)
{
    // Call the base class.
    BaseConnection::GapConnParamUpdateHandler(params);

    // For now skip requesting our long-term connection interval after the
    // parameters were updated once. See TODO below.
    longTermConnectionIntervalRequested = true;

    // TODO: Mark the connection parameters as updated, block or start further
    //       update attempts.
}

void MeshConnection::GapConnParamUpdateRequestHandler(
        const FruityHal::BleGapConnParams & params)
{
    // Call the base class.
    BaseConnection::GapConnParamUpdateRequestHandler(params);

    // For now skip requesting our long-term connection interval after
    // our partner has requested a change from us, since we are currently
    // always accepting the updated parameters. See TODO below.
    longTermConnectionIntervalRequested = true;

    // For now simply accept any request. See TODO below.
    DISCARD(GAPController::GetInstance().RequestConnectionParameterUpdate(
        connectionHandle,
        params.minConnInterval, params.maxConnInterval,
        params.slaveLatency, params.connSupTimeout
    ));
    
    // TODO: Add code that validates the requested parameters (e.g. compare
    //       with our settings for long term connections). If we accept,
    //       call GAPController::RequestConnectionParameterUpdate with the
    //       acceptable parameters, the 'old' / current parameters or
    //       other parameters. If no 'answer' is sent, the request will
    //       automatically time out on the peripheral and the connection
    //       keeps it's current parameters.
}
#endif

void MeshConnection::TryReestablishing()
{
    ErrorType err;
    err = GS->gapController.ConnectToPeripheral(partnerAddress, Conf::GetInstance().meshMinConnectionInterval, Conf::meshExtendedConnectionTimeoutSec);

    //If the call to connect fails, the ConnectionManager must retry connecting periodically
    mustRetryReestablishing = (err != ErrorType::SUCCESS);

    logt("CONN", "reconnstatus %u", (u32)err);
}

void MeshConnection::HandoverMasterBit()
{
    //Remove the masterbit from this connection
    connectionMasterBit = 0;
    //Put the masterbit handover in the correct packet.
    currentClusterInfoUpdatePacket.payload.connectionMasterBitHandover = 1;

    logt("CONN", "SENDING MASTERBIT FROM NODE %u TO NODE %u", GS->node.configuration.nodeId, partnerId);
}

bool MeshConnection::HasConnectionMasterBit()
{
    return connectionMasterBit != 0;
}

void MeshConnection::GapReconnectionSuccessfulHandler(const FruityHal::GapConnectedEvent& connectedEvent)
{
    BaseConnection::GapReconnectionSuccessfulHandler(connectedEvent);

    GS->logger.LogCustomError(CustomErrorTypes::INFO_CONNECTION_SUSTAIN_SUCCESS, partnerId);

    //Set to reestablishing handshake
    connectionState = ConnectionState::REESTABLISHING_HANDSHAKE;
    handshakeStartedDs = GS->appTimerDs;

    //Reset all send queues so that the packets are being sent again
    queue.RollbackLookAhead();
    queueOrigins.Reset();
}

#define __________________SENDING_________________

//This method queues a packet no matter if the connection is currently in handshaking or not
bool MeshConnection::SendHandshakeMessage(u8* data, u16 dataLength, bool reliable)
{
    BaseConnectionSendData sendData;
    sendData.characteristicHandle = partnerWriteCharacteristicHandle;
    sendData.dataLength = dataLength;
    sendData.deliveryOption = reliable ? DeliveryOption::WRITE_REQ : DeliveryOption::WRITE_CMD;

    if(IsConnected()){
        QueueData(sendData, data, nullptr);

        return true;
    } else {
        return false;
    }
}

//This is a small wrapper for the SendData method
bool MeshConnection::SendData(u8 const * data, MessageLength dataLength, bool reliable, u32 * messageHandle)
{
    if (dataLength > MAX_MESH_PACKET_SIZE) {
        SIMEXCEPTION(PacketTooBigException);
        logt("ERROR", "Packet too big for sending!");
        return false;
    }

    BaseConnectionSendData sendData;
    sendData.characteristicHandle = partnerWriteCharacteristicHandle;
    sendData.dataLength = dataLength;
    sendData.deliveryOption = reliable ? DeliveryOption::WRITE_REQ : DeliveryOption::WRITE_CMD;

    return SendData(&sendData, data, messageHandle);
}

//This is the generic method for sending data
bool MeshConnection::SendData(BaseConnectionSendData* sendData, u8 const * data, u32 * messageHandle)
{
    if(!HandshakeDone()) return false; //Do not allow data being sent when Handshake has not finished yet

    //Print packet as hex
    ConnPacketHeader const * packetHeader = (ConnPacketHeader const *)data;
    char stringBuffer[100];
    Logger::ConvertBufferToHexString(data, sendData->dataLength, stringBuffer, sizeof(stringBuffer));

    //Mesh connections only support write cmd and req, no notifications,...
    if(sendData->deliveryOption != DeliveryOption::WRITE_CMD
        && sendData->deliveryOption != DeliveryOption::WRITE_REQ){

        sendData->deliveryOption = DeliveryOption::WRITE_CMD;
    }

    //WARNING: Currently we only support WRITE_CMD to protect against SoftDevice faults
    //The SoftDevice will sometimes malfunction when receiving a lot of WRITE_REQ, also, they slow down the
    //sending of packets by a factor of 14, so we only use them for mesh critical functionality such as clustering
    sendData->deliveryOption = DeliveryOption::WRITE_CMD;

    logt("CONN_DATA", "PUT_PACKET(%d):len:%d,type:%d,hex:%s",
            connectionId, sendData->dataLength.GetRaw(), (u32)packetHeader->messageType, stringBuffer);

    //Put packet in the queue for sending
    return QueueData(*sendData, data, messageHandle);
}

//Allows a Subclass to send Custom Data before the writeQueue is processed
//should return true if something was sent
bool MeshConnection::QueueVitalPrioData()
{
    if(
        HandshakeDone() //Handshake must be finished
        && ClusterInfoUpdateHasData()
    ){
        //If a clusterUpdate is available we send it immediately
        u8* data = (u8*)&(currentClusterInfoUpdatePacket);

        if(!IsValidMessageType(((ConnPacketHeader*)data)->messageType)){
            logt("ERROR", "POSSIBLE WRONG DATA TRANSMITTED!");

            GS->logger.LogCustomError(CustomErrorTypes::WARN_TX_WRONG_DATA, (u32)((ConnPacketHeader*)data)->messageType);
        }

        //Use this to queue the clusterUpdate in the vital prio queue
        BaseConnectionSendData sendData;
        CheckedMemset(&sendData, 0x00, sizeof(BaseConnectionSendData));

        sendData.characteristicHandle = partnerWriteCharacteristicHandle;
        sendData.dataLength = SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE;
        sendData.deliveryOption = DeliveryOption::WRITE_CMD;

        //Set the counter for the packet
        currentClusterInfoUpdatePacket.payload.counter = ++clusterUpdateCounter;

        bool queued = QueueData(sendData, data, false, nullptr);

        if (queued) {
            logt("CONN", "Queued CLUSTER UPDATE for CONN hnd %u", connectionHandle);

            //The current cluster info update message has been sent, we can now clear the packet
            //Because we filled it in the buffer
            ClearCurrentClusterInfoUpdatePacket();
        }
        else {
            SIMSTATCOUNT("vitalPrioQueueFull");
            logt("WARNING", "Could not queue CLUSTER_UPDATE");

            GS->logger.LogCustomError(CustomErrorTypes::WARN_VITAL_PRIO_QUEUE_FULL, partnerId);

            //We must reset our current counter as it was not used
            clusterUpdateCounter--;
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

void MeshConnection::PacketSuccessfullyQueuedWithSoftdevice(SizedData* sentData)
{
    //Save a queue handle for that packet
    HandlePacketQueued();
    
    //Check if this was the end of a handshake, if yes, mark handshake as completed
    if (((ConnPacketHeader*)sentData->data)->messageType == MessageType::CLUSTER_ACK_2)
    {
        //Notify Node of HandshakeDone
        GS->node.HandshakeDoneHandler((MeshConnection*)this, true);
    }
}

void MeshConnection::DataSentHandler(const u8 * data, MessageLength length, u32 messageHandle)
{
    const ConnPacketHeader* header = (const ConnPacketHeader*)data;
    if (header->messageType == MessageType::TIME_SYNC)
    {
        const TimeSyncHeader* header = (const TimeSyncHeader*)data;
        if (header->type == TimeSyncType::INITIAL)
        {
#ifdef SIM_ENABLED
            correctionTicksSuccessfullyWritten = true;
#endif
            correctionTicks = GS->timeManager.GetTimePoint() - syncSendingOrdered;
        }
    }
}

#define __________________RECEIVING_________________

void MeshConnection::ReceiveDataHandler(BaseConnectionSendData* sendData, u8 const * data)
{
    //Only accept packets to our mesh write handle, TODO: could disconnect if other data is received
    if(
        connectionState == ConnectionState::DISCONNECTED
        || sendData->characteristicHandle != GS->node.meshService.sendMessageCharacteristicHandle.valueHandle
    ){
        return;
    }

    ConnPacketHeader const * packetHeader = (ConnPacketHeader const *)data;
    if (packetHeader->sender == GS->sinkNodeId)
    {
        GS->lastReceivedFromSinkTimestamp = FruityHal::GetRtcMs();
    }

    char stringBuffer[200];
    Logger::ConvertBufferToHexString(data, sendData->dataLength, stringBuffer, sizeof(stringBuffer));
    logt("CONN_DATA", "Mesh RX %d,length:%d,deliv:%d,data:%s", (u32)packetHeader->messageType, sendData->dataLength.GetRaw(), (u32)sendData->deliveryOption, stringBuffer);

    //This will reassemble the data for us
    data = ReassembleData(sendData, data);

    if(data != nullptr){
        //Route the packet to our other mesh connections
        GS->cm.RouteMeshData(this, sendData, data);

        //Call our handler that dispatches the message throughout our application
        ReceiveMeshMessageHandler(sendData, data);
    }
}

void MeshConnection::ReceiveMeshMessageHandler(BaseConnectionSendData* sendData, u8 const * data)
{
    ConnPacketHeader const * packetHeader = (ConnPacketHeader const *) data;

    //Some special handling for timestamp updates
    GS->timeManager.HandleUpdateTimestampMessages(packetHeader, sendData->dataLength);

    //Some logging
    if(!IsValidMessageType(packetHeader->messageType)){
        logt("ERROR", "POSSIBLE WRONG DATA RECEIVED!");

        GS->logger.LogCustomCount(CustomErrorTypes::COUNT_WARN_RX_WRONG_DATA);
    }
    //Print packet as hex
    {
        char stringBuffer[100];
        Logger::ConvertBufferToHexString(data, sendData->dataLength, stringBuffer, sizeof(stringBuffer));
        logt("CONN_DATA", "Received type %d,length:%d,deliv:%d,data:%s", (u32)packetHeader->messageType, sendData->dataLength.GetRaw(), (u32)sendData->deliveryOption, stringBuffer);
    }
    Logger::GetInstance().LogCustomCount(CustomErrorTypes::COUNT_TOTAL_RECEIVED_MESSAGES);

    if(!HandshakeDone() || connectionState == ConnectionState::REESTABLISHING_HANDSHAKE){
        ReceiveHandshakePacketHandler(sendData, data);
    } else {
        //Dispatch message to node and modules
        GS->cm.DispatchMeshMessage(this, sendData, (ConnPacketHeader const *) data, true);
    }
}


#define _________________HANDSHAKE_______________________

void MeshConnection::ConnectionMtuUpgradedHandler(u16 gattPayloadSize)
{
    BaseConnection::ConnectionMtuUpgradedHandler(gattPayloadSize);

    //Our central will start the handshake
    if (direction == ConnectionDirection::DIRECTION_OUT) {
        //Depending on the kind of handshake we should do, we call the appropriate method
        if (connectionState < ConnectionState::REESTABLISHING_HANDSHAKE) {
            StartHandshakeAfterMtuExchange();
        }
        else {
            const ErrorType err = SendReconnectionHandshakePacketAfterMtuExchange();
            if (err != ErrorType::SUCCESS)
            {
                logt("CM", "Failed to send reconnection handshake because %u", (u32)err);
            }
        }
    }
}

//This is called in case our node is the central, otherwhise, the handshake is started by the partner
void MeshConnection::StartHandshake()
{
    //Before starting our mesh handshake, we upgrade to a higher MTU if possible
    ErrorType err = GS->cm.RequestDataLengthExtensionAndMtuExchange(this);

    //If we could not upgrade the MTU, we drop the connection
    //Tests have shown that this does not happen in a mesh with 50 nodes over 4 months
    //If it ever happens, we log a fatal error and can then improve the MTU upgrade process
    if(err != ErrorType::SUCCESS){
        GS->logger.LogCustomError(CustomErrorTypes::FATAL_MTU_UPGRADE_FAILED, (u32)err);

        DisconnectAndRemove(AppDisconnectReason::MTU_UPGRADE_FAILED);
    }

    // => The ConnectionMtuUpgradedHandler will be called next
}

void MeshConnection::StartHandshakeAfterMtuExchange()
{
    //Save a snapshot of the current clustering values, these are used in the handshake
    //Changes to these values are only sent after the handshake has finished and the handshake
    //must not use values that are saved in the node because these might have changed in the meantime
    clusterIDBackup = GS->node.clusterId;
    clusterSizeBackup = GS->node.GetClusterSize();
    
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
    ConnPacketClusterWelcome packet;
    packet.header.messageType = MessageType::CLUSTER_WELCOME;
    packet.header.sender = GS->node.configuration.nodeId;
    packet.header.receiver = NODE_ID_HOPS_BASE + 1; //Node id is unknown, but this allows us to send the packet only 1 hop

    packet.payload.clusterId = clusterIDBackup;
    packet.payload.clusterSize = clusterSizeBackup;
    packet.payload.meshWriteHandle = GS->node.meshService.sendMessageCharacteristicHandle.valueHandle; //Our own write handle

    //Now we set the hop counter to the closest sink
    packet.payload.hopsToSink = GS->cm.GetMeshHopsToShortestSink(this);

    packet.payload.preferredConnectionInterval = 0; //Unused at the moment
    packet.payload.networkId = GS->node.configuration.networkId;

    logt("HANDSHAKE", "OUT => conn(%u) CLUSTER_WELCOME, cID:%x, cSize:%d, hops:%d", connectionId, packet.payload.clusterId, packet.payload.clusterSize, packet.payload.hopsToSink);

    SendHandshakeMessage((u8*) &packet, SIZEOF_CONN_PACKET_CLUSTER_WELCOME_WITH_NETWORK_ID, true);
}

void MeshConnection::ReceiveHandshakePacketHandler(BaseConnectionSendData* sendData, u8 const * data)
{
    NodeId tempPartnerId = partnerId; //Temp storage in case we delete this.
    ConnPacketHeader const * packetHeader = (ConnPacketHeader const *) data;

    LiveReportHandshakeFailCode handshakeFailCode = LiveReportHandshakeFailCode::SUCCESS;

    /*#################### RECONNETING_HANDSHAKE ############################*/
    if(packetHeader->messageType == MessageType::RECONNECT)
    {
        ReceiveReconnectionHandshakePacket((ConnPacketReconnect const *) data);
    }

    /*#################### HANDSHAKE ############################*/
    /******* Cluster welcome *******/
    else if (packetHeader->messageType == MessageType::CLUSTER_WELCOME)
    {
        if (sendData->dataLength >= SIZEOF_CONN_PACKET_CLUSTER_WELCOME)
        {
            //Now, compare that packet with our data and see if he should join our cluster
            ConnPacketClusterWelcome const * packet = (ConnPacketClusterWelcome const *) data;

            //Save mesh write handle
            partnerWriteCharacteristicHandle = packet->payload.meshWriteHandle;

            connectionState = ConnectionState::HANDSHAKING;

            //Save a snapshot of the current clustering values, these are used in the handshake
            //Changes to these values are only sent after the handshake has finished and the handshake
            //must not use values that are saved in the node because these might have changed in the meantime
            clusterIDBackup = GS->node.clusterId;
            clusterSizeBackup = GS->node.GetClusterSize();
            
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
                ConnPacketClusterAck1 outPacket;

                outPacket.header.messageType = MessageType::CLUSTER_ACK_1;
                outPacket.header.sender = GS->node.configuration.nodeId;
                outPacket.header.receiver = this->partnerId;

                outPacket.payload.hopsToSink = GET_DEVICE_TYPE() == DeviceType::SINK ? 0 : -1;

                logt("HANDSHAKE", "OUT => %d CLUSTER_ACK_1, hops:%d", outPacket.header.receiver, outPacket.payload.hopsToSink);

                SendHandshakeMessage((u8*) &outPacket, SIZEOF_CONN_PACKET_CLUSTER_ACK_1, true);
                
                //Kill other Connections and check if this connection has been removed in the process
                GS->cm.ForceDisconnectOtherMeshConnections(this, AppDisconnectReason::I_AM_SMALLER);

                //Because we forcefully killed our connections, we are back at square 1
                //These values will be overwritten by the ACK2 packet that we receive from out partner
                //But if we do never receive an ACK2, this is our new starting point
                //Setting the size to 1 is a safety precaution
                GS->node.SetClusterSize(1);
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
            if(clusterAck1Packet.header.messageType != MessageType::INVALID || GS->cm.GetConnectionInHandshakeState().GetUniqueConnectionId() != uniqueConnectionId){
                //TODO: disconnect? check this in sim
                logt("ERROR", "HANDSHAKE ERROR ACK1 duplicate %u, %u", (u32)clusterAck1Packet.header.messageType, (u32)GS->node.currentDiscoveryState);

                GS->logger.LogCustomCount(CustomErrorTypes::COUNT_HANDSHAKE_ACK1_DUPLICATE);
            }

            //Save ACK1 packet for later
            CheckedMemcpy(&clusterAck1Packet, data, sizeof(ConnPacketClusterAck1));

            logt("HANDSHAKE", "IN <= %d  CLUSTER_ACK_1, hops:%d", clusterAck1Packet.header.sender, clusterAck1Packet.payload.hopsToSink);


            //Set the master bit for the connection. If the connection would disconnect
            //Then we could keep intact and the other one must dissolve
            this->partnerId = clusterAck1Packet.header.sender;
            this->connectionMasterBit = 1;
            this->hopsToSink = clusterAck1Packet.payload.hopsToSink;
            logt("HANDSHAKE", "NODE %u CREATED MASTERBIT", GS->node.configuration.nodeId);

            //Confirm to the new node that it just joined our cluster => send ACK2
            ConnPacketClusterAck2 outPacket2;
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
            if(clusterAck2Packet.header.messageType != MessageType::INVALID || GS->cm.GetConnectionInHandshakeState().GetUniqueConnectionId() != uniqueConnectionId){
                //TODO: disconnect
                logt("ERROR", "HANDSHAKE ERROR ACK2 duplicate %u, %u", (u32)clusterAck2Packet.header.messageType, (u32)GS->node.currentDiscoveryState);
                GS->logger.LogCustomCount(CustomErrorTypes::COUNT_HANDSHAKE_ACK2_DUPLICATE);
            }

            //Save Ack2 packet for later
            CheckedMemcpy(&clusterAck2Packet, data, sizeof(ConnPacketClusterAck2));

            logt("HANDSHAKE", "IN <= %d CLUSTER_ACK_2 clusterID:%x, clusterSize:%d", clusterAck2Packet.header.sender, clusterAck2Packet.payload.clusterId, clusterAck2Packet.payload.clusterSize);

            //Notify Node of HandshakeDone
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
        statusMod->SendLiveReport(LiveReportTypes::HANDSHAKE_FAIL, 0, tempPartnerId, (u32)handshakeFailCode);
    }
}

void MeshConnection::SendReconnectionHandshakePacket()
{
    //Before starting our mesh handshake, we upgrade to a higher MTU if possible
    ErrorType err = GS->cm.RequestDataLengthExtensionAndMtuExchange(this);

    //If we could not upgrade the MTU, we drop the connection
    //Tests have shown that this does not happen in a mesh with 50 nodes over 4 months
    //If it ever happens, we log a fatal error and can then improve the MTU upgrade process
    if (err != ErrorType::SUCCESS) {
        GS->logger.LogCustomError(CustomErrorTypes::FATAL_MTU_UPGRADE_FAILED, (u32)err);

        DisconnectAndRemove(AppDisconnectReason::MTU_UPGRADE_FAILED);
    }

    // => The ConnectionMtuUpgradedHandler will be called next
}

ErrorType MeshConnection::SendReconnectionHandshakePacketAfterMtuExchange()
{
    //Can not be done using the send queue because there might be data packets in these queues
    //So instead, we queue the data directly in the softdevice. We can assume that this succeeds most of the time, otherwise reconneciton fails

    logt("HANDSHAKE", "OUT => conn(%u) RECONNECT", connectionId);

    ConnPacketReconnect packet;
    packet.header.messageType = MessageType::RECONNECT;
    packet.header.sender = GS->node.configuration.nodeId;
    packet.header.receiver = partnerId;

    //TODO: Add a check if the reliable buffer is free?

    //We send the reconnect packet
    ErrorType err = GS->gattController.BleWriteCharacteristic(
        connectionHandle,
        partnerWriteCharacteristicHandle,
        (u8*)&packet,
        SIZEOF_CONN_PACKET_RECONNECT,
        false);


    logt("HANDSHAKE", "writing to connHnd %u, partnerWriteHnd %u, err %u",connectionHandle, partnerWriteCharacteristicHandle, (u32)err);

    //TODO: If we notice that we have too many errors here, we can optimize this part and retry sending the reconnect packet

    //We must account for buffers ourself if we do not use the queue
    if (err == ErrorType::SUCCESS) {
        manualPacketsSent++;
    }
    else {
        //We must disconnect, as otherwhise other packets from the queue will get sent, this will break the reestablishment
        //We cannot disconnect the GAP connection on purpose as the partner will then stop the reestablishment
        this->DisconnectAndRemove(AppDisconnectReason::RECONNECT_BLE_ERROR);

        //WARNING: After this error was returned, we must not use the connection reference anymore
        return ErrorType::INVALID_STATE;
    }

    return ErrorType::SUCCESS;
}

void MeshConnection::ReceiveReconnectionHandshakePacket(ConnPacketReconnect const * packet)
{
    logt("HANDSHAKE", "IN <= partner %u RECONNECT", partnerId);
    if(
        packet->header.sender == partnerId
        && connectionState == ConnectionState::REESTABLISHING_HANDSHAKE
    ){
        //Answer the handshake packet
        ErrorType err = SendReconnectionHandshakePacketAfterMtuExchange();

        if (err == ErrorType::SUCCESS) {
            connectionState = ConnectionState::HANDSHAKE_DONE;
            disconnectedTimestampDs = 0;
        }

    }
}

#define _________________OTHER_______________________

u32 MeshConnection::GetPendingPackets() const {
    //Adds 1 if a clusterUpdatePacket must be send
    return BaseConnection::GetPendingPackets() + (ClusterInfoUpdateHasData() ? 1 : 0);
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
        case(MessageType::ASSET_LEGACY):
        case(MessageType::ASSET_GENERIC):
        case(MessageType::SIG_MESH_SIMPLE):
        case(MessageType::MODULE_CONFIG):
        case(MessageType::MODULE_TRIGGER_ACTION):
        case(MessageType::MODULE_ACTION_RESPONSE):
        case(MessageType::MODULE_GENERAL):
        case(MessageType::MODULE_RAW_DATA):
        case(MessageType::MODULE_RAW_DATA_LIGHT):
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
bool MeshConnection::ClusterInfoUpdateHasData() const
{
    return currentClusterInfoUpdatePacket.payload.clusterSizeChange != 0
        || currentClusterInfoUpdatePacket.payload.connectionMasterBitHandover != 0
        || (currentClusterInfoUpdatePacket.payload.hopsToSink != -1 && GET_DEVICE_TYPE() != DeviceType::SINK);
}
;

void MeshConnection::PrintStatus()
{
    const char* directionString = (direction == ConnectionDirection::DIRECTION_IN) ? "IN " : "OUT";

    trace("%s(%d) FM %u, state:%u, cluster:%x(%d), sink:%d, Queue:%u, mb:%u, hnd:%u, tSync:%u, sent:%u, rssi:%d, mtu:%u" EOL,
        directionString,
        connectionId,
        this->partnerId,
        (u32)this->connectionState,
        this->connectedClusterId,
        this->connectedClusterSize,
        this->hopsToSink,
        GetPendingPackets(),
        connectionMasterBit,
        connectionHandle,
        (u32)timeSyncState,
        sentUnreliable,
        GetAverageRSSI(),
        connectionPayloadSize);
}

void MeshConnection::SetHopsToSink(ClusterSize hops)
{
    hopsToSink = hops;
}

ClusterSize MeshConnection::GetHopsToSink()
{
    return hopsToSink;
}

void MeshConnection::SetEnrolledNodesSync(bool sync)
{
    enrolledNodesSynced = sync;
}

bool MeshConnection::GetEnrolledNodesSync()
{
    return enrolledNodesSynced;
}
