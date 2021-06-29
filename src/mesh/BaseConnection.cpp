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

#include <Node.h>
#include <BaseConnection.h>
#include <GATTController.h>
#include <GAPController.h>
#include <ConnectionManager.h>
#include <GlobalState.h>
#include <MeshConnection.h>

constexpr int BASE_CONNECTION_MAX_SEND_FAIL  = 10;

/*
Note: The Connection Class does have methods like Connect,... but connections, service
discovery or encryption are handeled by the Connectionmanager so that we can control
The parallel flow of multiple connections.
*/
BaseConnection::BaseConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress)
    : connectionId(id),
    uniqueConnectionId(GS->cm.GenerateUniqueConnectionId()),
    direction(direction),
    partnerAddress(*partnerAddress),
    creationTimeDs(GS->appTimerDs)
{
    //Initialize to defaults
    clusterUpdateCounter = 0;
    nextExpectedClusterUpdateCounter = 1;
    CheckedMemset(dataSentBuffer, 0x00, sizeof(dataSentBuffer));
    dataSentLength = 0;

    GS->cm.NotifyNewConnection();
}

BaseConnection::~BaseConnection()
{
    logt("CONN", "Deleted Connection, type %u, discR: %u, appDiscR: %u", (u32)this->connectionType, (u32)this->disconnectionReason, (u32)this->appDisconnectionReason);
    GS->amountOfRemovedConnections++;
    GS->cm.NotifyDeleteConnection();
}

/*######## PUBLIC FUNCTIONS ###################################*/

void BaseConnection::DisconnectAndRemove(AppDisconnectReason reason)
{
    //### STEP 1: Set Variables to disconnected so that other parts can reference these

    //Save the disconnect reason only if unknown so that it does not get overwritten by another call
    if(appDisconnectionReason == AppDisconnectReason::UNKNOWN) appDisconnectionReason = reason;

    //Save connection state before disconnection
    connectionStateBeforeDisconnection = connectionState;
    connectionState = ConnectionState::DISCONNECTED;

    //### STEP 2: Try to disconnect (could already be disconnected or not yet even connected

    //Stop connecting if we were trying to build a connection
    //KNOWNISSUE: This does break sth. (tested in simulator) when used
    //if(GS->cm.pendingConnection == this) FruityHal::ConnectCancel();

    //Disconnect if possible
    const ErrorType err = FruityHal::Disconnect(connectionHandle, FruityHal::BleHciError::REMOTE_USER_TERMINATED_CONNECTION);
    if (err != ErrorType::SUCCESS)
    {
        logt("CM", "Could not disconnect because %u", (u32)err);
    }

    //### STEP 3: Inform ConnectionManager to do the final cleanup
    GS->cm.DeleteConnection(this, reason);
}


/*######## PRIVATE FUNCTIONS ###################################*/



#define __________________SENDING__________________


bool BaseConnection::QueueData(const BaseConnectionSendData &sendData, u8 const * data, u32* messageHandle){
    return QueueData(sendData, data, true, messageHandle);
}

bool BaseConnection::QueueData(const BaseConnectionSendData &sendData, u8 const * data, bool fillTxBuffers, u32* messageHandle)
{
    const u32 bufferSize = SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED + sendData.dataLength.GetRaw();
    DYNAMIC_ARRAY(buffer, bufferSize);
    CheckedMemset(buffer, 0, bufferSize);

    BaseConnectionSendDataPacked* sendDataPacked = (BaseConnectionSendDataPacked*)buffer;
    sendDataPacked->characteristicHandle = sendData.characteristicHandle;
    sendDataPacked->deliveryOption = (u8)sendData.deliveryOption;

    CheckedMemcpy(buffer + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED, data, sendData.dataLength.GetRaw());

    const bool successfullyQueued = queue.SplitAndAddMessage(overwritePriority == DeliveryPriority::INVALID ? GetPriorityOfMessage(data, sendData.dataLength) : overwritePriority, buffer, bufferSize, connectionPayloadSize, messageHandle);

    if(successfullyQueued){
        if (fillTxBuffers) FillTransmitBuffers();
        return true;
    } else {
        GS->cm.droppedMeshPackets++;
        droppedPackets++;

        GS->logger.LogCustomCount(CustomErrorTypes::COUNT_DROPPED_PACKETS);

        //TODO: Error handling: What should happen when the queue is full?
        //Currently, additional packets are dropped
        logt("CM", "Send queue is already full");
        SIMSTATCOUNT(Logger::GetErrorLogCustomError(CustomErrorTypes::COUNT_DROPPED_PACKETS));

        //For safety, we try to fill the transmitbuffers if it got stuck
        if(fillTxBuffers) FillTransmitBuffers();
        return false;
    }
}

void BaseConnection::FillTransmitBuffers()
{
    ErrorType err = ErrorType::SUCCESS;

    if (bufferFull) return;

    while(IsConnected() && connectionState != ConnectionState::REESTABLISHING && connectionState != ConnectionState::REESTABLISHING_HANDSHAKE)
    {
        if (queueOrigins.IsFull())
        {
            // QueueOrigins are full. We have to continue with the next connection.
            // Consider increasing the size of the queueOrigins queue to fit the number
            // of supporter entries in the HAL TransmitBuffer.
            SIMEXCEPTION(IllegalStateException);
            GS->logger.LogCustomError(CustomErrorTypes::FATAL_QUEUE_ORIGINS_FULL, queueOrigins.GetAmountOfElements());
            logt("WARNING", "Queue Origins are full!");
            bufferFull = true;
            return;
        }
        //Check if there is important data from the subclass to be sent
        if(queue.IsCurrentlySendingSplitMessage() == false){
            QueueVitalPrioData();
        }
        //Next, select the correct Queue from which we should be transmitting
        QueuePriorityPair queuePriorityPair = queue.GetSendQueue();
        ChunkedPacketQueue* activeQueue = queuePriorityPair.queue;
        if (!activeQueue) return;

        //Get the next packet from the packet queue that was not yet queued
        DYNAMIC_ARRAY(queueBuffer, connectionMtu + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED);
        const u16 packetLength = activeQueue->PeekLookAhead(queueBuffer, connectionMtu + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED) - SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED;

        //Unpack data from sendQueue
        BaseConnectionSendDataPacked* sendDataPacked = (BaseConnectionSendDataPacked*)queueBuffer;
        u8* data = (queueBuffer + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED);

        //The subclass is allowed to modify the packet before it is sent, it will place the modified packet into the data buffer.
        //This could be encryption of the data.
        const MessageLength processedMessageLength = ProcessDataBeforeTransmission(data, packetLength, connectionMtu);

        if(processedMessageLength.IsZero()){
            logt("ERROR", "Packet processing failed");
            GS->logger.LogCustomError(CustomErrorTypes::FATAL_PACKET_PROCESSING_FAILED, partnerId);
            DisconnectAndRemove(AppDisconnectReason::INVALID_PACKET);
            return;
        }

        //Send the packet to the SoftDevice
        if((DeliveryOption)sendDataPacked->deliveryOption == DeliveryOption::WRITE_REQ)
        {
            err = GS->gattController.BleWriteCharacteristic(
                    connectionHandle,
                    sendDataPacked->characteristicHandle,
                    data,
                    processedMessageLength,
                    true);
        }
        else if((DeliveryOption)sendDataPacked->deliveryOption == DeliveryOption::WRITE_CMD)
        {
            err = GS->gattController.BleWriteCharacteristic(
                    connectionHandle,
                    sendDataPacked->characteristicHandle,
                    data,
                    processedMessageLength,
                    false);
        }
        else
        {
            err = GS->gattController.BleSendNotification(
                    connectionHandle,
                    sendDataPacked->characteristicHandle,
                    data,
                    processedMessageLength);
        }

        if(err == ErrorType::SUCCESS) 
        {
            SizedData sizedData;
            sizedData.data = data;
            sizedData.length = processedMessageLength.GetRaw();
            queueOrigins.Push(queuePriorityPair.priority);
            activeQueue->IncrementLookAhead();
            PacketSuccessfullyQueuedWithSoftdevice(&sizedData);
        }
        else if(err == ErrorType::BUSY)
        {
            return;
        }
        else if(err == ErrorType::RESOURCES){
            //No free buffers in the softdevice, so packet could not be queued, go to next connection
            //Also set the bufferFull variable
            bufferFull = true;
            return;
        }
        else
        {
            const char* tag = "WARNING";
            if (
                err != ErrorType::BLE_INVALID_CONN_HANDLE // May happen e.g. if the connection is not fully created yet or was destroyed already.
                )
            {
                tag = "ERROR";
            }
            logt(tag, "GATT WRITE ERROR 0x%x on handle %u", (u32)err, connectionHandle);

            GS->logger.LogCustomError(CustomErrorTypes::WARN_GATT_WRITE_ERROR, (u32)err);

            HandlePacketQueuingFail((u32)err);

            //Stop queuing packets for this connection to prevent infinite loops
            return;
        }
    }
}

void BaseConnection::HandlePacketQueued()
{
    packetFailedToQueueCounter = 0;
}

void BaseConnection::HandlePacketSent(u8 sentUnreliable, u8 sentReliable)
{
    bufferFull = false;

    //logt("CONN", "Data was sent %u, %u", sentUnreliable, sentReliable);

    //TODO: are write cmds and write reqs sent sequentially?
    //TODO: we must not send more than 100 packets from one queue, otherwise, the handles between
    //TODO: We should think about the terminology of "Splits", "Messages", and "Packets". How do we want to use these terms in the future?
    //the queues will not match anymore to the sequence in that the packets were sent
    
    //We must iterate in a loop to delete all packets if more than one was sent
    u8 numSent = sentUnreliable + sentReliable;

    for(u32 i=0; i<numSent; i++){

        //Check if packets were sent manually using a softdevice call and thereby bypassing the sendQueues
        if (manualPacketsSent > 0) {
            manualPacketsSent--;
            continue;
        }

        if (queueOrigins.GetAmountOfElements() == 0)
        {
            logt("ERROR", "!!!FATAL!!! QueueOrigins");
            SIMEXCEPTION(IllegalStateException);

            GS->logger.LogCustomError(CustomErrorTypes::FATAL_HANDLE_PACKET_SENT_ERROR, partnerId);
            DisconnectAndRemove(AppDisconnectReason::HANDLE_PACKET_SENT_ERROR);
            return;
        }

        const DeliveryPriority queueOrigin = queueOrigins.Peek();
        queueOrigins.Pop();
        //Find the queue from which the packet was sent
        ChunkedPacketQueue* activeQueue = queue.GetQueueByPriority(queueOrigin);

        if(activeQueue->HasPackets() == false)
        {
            logt("ERROR", "!!!FATAL!!! Queue");
            SIMEXCEPTION(IllegalStateException);

            GS->logger.LogCustomError(CustomErrorTypes::FATAL_HANDLE_PACKET_SENT_ERROR, partnerId);
            DisconnectAndRemove(AppDisconnectReason::HANDLE_PACKET_SENT_ERROR);
            return;
        }
        DYNAMIC_ARRAY(queueBuffer, connectionMtu + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED);
        u32 messageHandle;
        const u16 length = activeQueue->PeekPacket(queueBuffer, connectionMtu + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED, &messageHandle);

        BaseConnectionSendDataPacked* sendData = (BaseConnectionSendDataPacked*)queueBuffer;

#ifdef SIM_ENABLED
        //A quick check if a wrong packet was removed (not a 100% check, but helps)
        if (sendData->deliveryOption == (u8)DeliveryOption::WRITE_REQ && !sentReliable) {
            SIMEXCEPTION(IllegalStateException);
        }
#endif
        if (messageHandle == 0)
        {
            CheckedMemcpy(&dataSentBuffer[dataSentLength], queueBuffer + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED + SIZEOF_CONN_PACKET_SPLIT_HEADER, length - SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED - SIZEOF_CONN_PACKET_SPLIT_HEADER);
            dataSentLength += (length - SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED - SIZEOF_CONN_PACKET_SPLIT_HEADER);
            activeQueue->PopPacket();
            continue;
        }

        if (dataSentLength != 0)
        {
            CheckedMemcpy(&dataSentBuffer[dataSentLength], queueBuffer + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED + SIZEOF_CONN_PACKET_SPLIT_HEADER, length - SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED - SIZEOF_CONN_PACKET_SPLIT_HEADER);
            dataSentLength += (length - SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED - SIZEOF_CONN_PACKET_SPLIT_HEADER);
            DataSentHandler(dataSentBuffer, dataSentLength, messageHandle);
#ifdef SIM_ENABLED
            char stringBuffer[1000];
            Logger::ConvertBufferToBase64String(dataSentBuffer, dataSentLength, stringBuffer, sizeof(stringBuffer));
            logt("CONN", "DataSentHandler: %s", stringBuffer);
#endif
        }
        else
        {
            DataSentHandler(queueBuffer + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED, length - SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED, messageHandle);
#ifdef SIM_ENABLED
            char stringBuffer[1000];
            Logger::ConvertBufferToBase64String(queueBuffer + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED, length - SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED, stringBuffer, sizeof(stringBuffer));
            logt("CONN", "DataSentHandler: %s", stringBuffer);
#endif
        }


        activeQueue->PopPacket();
        dataSentLength = 0;
    }

    //Log how many packets have been sent
    this->sentUnreliable += sentUnreliable;
    this->sentReliable += sentReliable;
}

void BaseConnection::HandlePacketQueuingFail(u32 err)
{
    packetFailedToQueueCounter++;

    if (
        err != (u32)ErrorType::DATA_SIZE && 
        err != (u32)ErrorType::TIMEOUT && 
        err != (u32)ErrorType::INVALID_ADDR && 
        err != (u32)ErrorType::INVALID_PARAM) {
        //The remaining errors can happen if the gap connection is temporarily lost during reestablishing
        return;
    }

    //Check queuing a packet failed too often so that the connection is probably broken (could also be too many wrong packets)
    if (packetFailedToQueueCounter > BASE_CONNECTION_MAX_SEND_FAIL) {
        DisconnectAndRemove(AppDisconnectReason::TOO_MANY_SEND_RETRIES);
    }
}

//This basic implementation returns the data as is
MessageLength BaseConnection::ProcessDataBeforeTransmission(u8* message, MessageLength messageLength, MessageLength bufferLength)
{
    return messageLength;
}

void BaseConnection::PacketSuccessfullyQueuedWithSoftdevice(SizedData* sentData)
{
    HandlePacketQueued();
}

#define _________________RECEIVING_________________

//A reassembly function that can reassemble split packets, can be used from subclasses
//Must use ConnPacketHeader for all packets
u8 const * BaseConnection::ReassembleData(BaseConnectionSendData* sendData, u8 const * data)
{
    ConnPacketSplitHeader const * packetHeader = (ConnPacketSplitHeader const *)data;

    //If reassembly is not needed, return packet without modifying
    if(packetHeader->splitMessageType != MessageType::SPLIT_WRITE_CMD && packetHeader->splitMessageType != MessageType::SPLIT_WRITE_CMD_END){
        currentMessageIsMissingASplit = false;
        return data;
    }

    //If this is the first split we can reset the packet reassembly position to
    //protect us against the drop of the last split of the previous message.
    if (packetHeader->splitCounter == 0)
    {
        packetReassemblyPosition = 0;
        currentMessageIsMissingASplit = false;
    }

    //Check if reassembly buffer limit is reached
    if(sendData->dataLength - (u32)SIZEOF_CONN_PACKET_SPLIT_HEADER + packetReassemblyPosition > packetReassemblyBuffer.size()){
        logt("ERROR", "Packet too big for reassembly");
        GS->logger.LogCustomError(CustomErrorTypes::FATAL_PACKET_TOO_BIG, sendData->dataLength.GetRaw());
        packetReassemblyPosition = 0;
        currentMessageIsMissingASplit = true;
        SIMEXCEPTION(PacketTooBigException);
        return nullptr;
    }

    u16 packetReassemblyDestination = packetHeader->splitCounter * (connectionPayloadSize - SIZEOF_CONN_PACKET_SPLIT_HEADER);

    //Check if a packet was missing inbetween
    if(packetReassemblyPosition < packetReassemblyDestination){
        GS->logger.LogCustomError(CustomErrorTypes::WARN_SPLIT_PACKET_MISSING, (packetReassemblyDestination - packetReassemblyPosition));
        packetReassemblyPosition = 0;
        currentMessageIsMissingASplit = true;
        SIMEXCEPTION(SplitMissingException);
        return nullptr;
    }

    //Intermediate packets must always be a full MTU
    //This is not strictly necessary but the implementation should guarantee this
    //This handling will lead to a splitPacketMissing exception as well as it drops intermediate packets
    if(packetHeader->splitMessageType == MessageType::SPLIT_WRITE_CMD && sendData->dataLength != connectionPayloadSize){
        GS->logger.LogCustomError(CustomErrorTypes::WARN_SPLIT_PACKET_NOT_IN_MTU, sendData->dataLength.GetRaw());
        packetReassemblyPosition = 0;
        currentMessageIsMissingASplit = true;
        SIMEXCEPTION(SplitNotInMTUException);
        return nullptr;
    }

    //Save at correct position in the reassembly buffer
    CheckedMemcpy(
        packetReassemblyBuffer.data() + packetReassemblyDestination,
        data + SIZEOF_CONN_PACKET_SPLIT_HEADER,
        sendData->dataLength.GetRaw() - SIZEOF_CONN_PACKET_SPLIT_HEADER);

    packetReassemblyPosition += (sendData->dataLength - SIZEOF_CONN_PACKET_SPLIT_HEADER).GetRaw();

    //Intermediate packet, no full packet yet received
    if(packetHeader->splitMessageType == MessageType::SPLIT_WRITE_CMD)
    {
        return nullptr;
    }
    //Final data for multipart packet received, return data
    else if(packetHeader->splitMessageType == MessageType::SPLIT_WRITE_CMD_END)
    {
        //Modify info for the reassembled packet
        sendData->dataLength = packetReassemblyPosition;
        data = packetReassemblyBuffer.data();

        //Reset the assembly buffer
        packetReassemblyPosition = 0;

        if (currentMessageIsMissingASplit)
        {
            currentMessageIsMissingASplit = false;
            return nullptr;
        }
        else
        {
            return data;
        }
    }
    return data;
}

#define __________________HANDLER__________________

DeliveryPriority BaseConnection::GetPriorityOfMessage(const u8* data, MessageLength size)
{
    //The highest priority (lowest ordinal) returned from a Module will be taken
    DeliveryPriority prio = DeliveryPriority::INVALID;
    for (u32 i = 0; i < GS->amountOfModules; i++) {
        if (GS->activeModules[i]->configurationPointer->moduleActive) {
            DeliveryPriority newPrio = GS->activeModules[i]->GetPriorityOfMessage(data, size);
            if (newPrio < prio) {
                prio = newPrio;
            }
        }
    }
    //A mesh node in a heterogenous network might not know this message, so we should not default to a LOW priority.
    //Additionally, the implementation should rarely care about priority and should use MEDIUM most of the time.
    if (prio == DeliveryPriority::INVALID) prio = DeliveryPriority::MEDIUM;
    return prio;
}

void BaseConnection::ConnectionSuccessfulHandler(u16 connectionHandle)
{
    this->handshakeStartedDs = GS->appTimerDs;

    if (direction == ConnectionDirection::DIRECTION_IN)
        logt("CONN", "Incoming connection %d connected", connectionId);
    else
        logt("CONN", "Outgoing connection %d connected", connectionId);

    this->connectionHandle = connectionHandle;

    connectionState = ConnectionState::CONNECTED;
}

void BaseConnection::GapReconnectionSuccessfulHandler(const FruityHal::GapConnectedEvent& connectedEvent){
    logt("CONN", "Reconnection Successful");

    //=> We expect the MTU to be the exact same value as the previous connection, otherwhise it gets dropped, so we do not need to reset it here even though a new gap connection will start with a smaller MTU

    connectionHandle = connectedEvent.GetConnectionHandle();

    connectionState = ConnectionState::HANDSHAKE_DONE;
}

void BaseConnection::ConnectionMtuUpgradedHandler(u16 gattPayloadSize)
{
    //MTU in our case means the available payload in an ATT packet, payload is the same
    //as we do not have any overhead for custom encryption
    this->connectionMtu = gattPayloadSize;
    this->connectionPayloadSize = gattPayloadSize;
}

void BaseConnection::GATTServiceDiscoveredHandler(FruityHal::BleGattDBDiscoveryEvent &evt)
{

}

//This is called when a connection gets disconnected before it is deleted
bool BaseConnection::GapDisconnectionHandler(FruityHal::BleHciError hciDisconnectReason)
{
    //Reason?
    logt("CONN", "Disconnected %u from connId:%u, HCI:%u %s", partnerId, connectionId, (u32)hciDisconnectReason, Logger::GetHciErrorString((FruityHal::BleHciError)hciDisconnectReason));

    this->disconnectionReason = (FruityHal::BleHciError)hciDisconnectReason;
    this->disconnectedTimestampDs = GS->appTimerDs;

    //Save connection state before disconnection
    if(connectionState != ConnectionState::DISCONNECTED){
        connectionStateBeforeDisconnection = connectionState;
    }
    //Set State to disconnected
    connectionState = ConnectionState::DISCONNECTED;

    return true;
}

#if IS_ACTIVE(CONN_PARAM_UPDATE)
void BaseConnection::GapConnParamUpdateHandler(
        const FruityHal::BleGapConnParams & params)
{
#if IS_ACTIVE(CONN_PARAM_UPDATE_LOGGING)
    logt(
        "CONN",
        "Connection parameter update on connection with id=%u "
        "(max=%u, min=%u, sl=%u, st=%u) as %s",
        connectionId, params.maxConnInterval, params.minConnInterval,
        params.slaveLatency, params.connSupTimeout,
        (direction == ConnectionDirection::DIRECTION_OUT) ? "central" : "peripheral"
    );
#endif
}

void BaseConnection::GapConnParamUpdateRequestHandler(
        const FruityHal::BleGapConnParams & params)
{
#if IS_ACTIVE(CONN_PARAM_UPDATE_LOGGING)
    logt(
        "CONN",
        "Connection parameter update request on connection with id=%u "
        "(max=%u, min=%u, sl=%u, st=%u) as %s",
        connectionId, params.maxConnInterval, params.minConnInterval,
        params.slaveLatency, params.connSupTimeout,
        (direction == ConnectionDirection::DIRECTION_OUT) ? "central" : "peripheral"
    );
#endif

    // Connection parameter update _requests_ are only happening on devices in
    // the central role, as a result of the remote peripheral device requesting
    // the parameter change.
    if (direction != ConnectionDirection::DIRECTION_OUT)
    {
#if IS_ACTIVE(CONN_PARAM_UPDATE_LOGGING)
        logt("ERROR", "Received connection parameter update request as peripheral.");
#endif
        SIMEXCEPTION(IllegalStateException);
    }
}
#endif

#define __________________HELPER______________________
/*######## HELPERS ###################################*/
i8 BaseConnection::GetAverageRSSI() const
{
    if(connectionState >= ConnectionState::CONNECTED){
        constexpr i32 divisor = 1000;
        //Round to closest rssi
        return (rssiAverageTimes1000 < 0) ? ((rssiAverageTimes1000 - divisor/2)/divisor) : ((rssiAverageTimes1000 + divisor/2)/divisor);
    }
    else return 0;
}

u32 BaseConnection::GetAmountOfRemovedConnections()
{
    return GS->amountOfRemovedConnections;
}


/* EOF */

