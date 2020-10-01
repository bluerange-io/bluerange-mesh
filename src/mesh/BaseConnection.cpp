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
    packetSendQueue(packetSendBuffer, PACKET_SEND_BUFFER_SIZE),
    packetSendQueueHighPrio(packetSendBufferHighPrio, PACKET_SEND_BUFFER_HIGH_PRIO_SIZE),
    partnerAddress(*partnerAddress),
    creationTimeDs(GS->appTimerDs)
{
    //Initialize to defaults
    clusterUpdateCounter = 0;
    nextExpectedClusterUpdateCounter = 1;

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


bool BaseConnection::QueueData(const BaseConnectionSendData &sendData, u8 const * data){
    return QueueData(sendData, data, true);
}

bool BaseConnection::QueueData(const BaseConnectionSendData &sendData, u8 const * data, bool fillTxBuffers)
{
    //Reserve space in our sendQueue for the metadata and our data
    u8* buffer;

    //Select the correct packet Queue
    //TODO: currently we only allow non-split data for high prio
    PacketQueue* activeQueue;

    if(sendData.priority == DeliveryPriority::MESH_INTERNAL_HIGH && sendData.dataLength <= connectionMtu)
    {
        logt("CM", "Queuing in high prio queue");
        activeQueue = &packetSendQueueHighPrio;
    } else {
        logt("CM", "Queuing in normal prio queue");
        activeQueue = &packetSendQueue;
    }

    buffer = activeQueue->Reserve(SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED + sendData.dataLength);

    if(buffer != nullptr){
        activeQueue->numUnsentElements++;

        //Copy both to the reserved space
        BaseConnectionSendDataPacked* sendDataPacked = (BaseConnectionSendDataPacked*)buffer;
        sendDataPacked->characteristicHandle = sendData.characteristicHandle;
        sendDataPacked->deliveryOption = (u8)sendData.deliveryOption;
        sendDataPacked->priority = (u8)sendData.priority;
        sendDataPacked->dataLength = sendData.dataLength;
        sendDataPacked->sendHandle = PACKET_QUEUED_HANDLE_NOT_QUEUED_IN_SD;

        CheckedMemcpy(buffer + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED, data, sendData.dataLength);
        if (fillTxBuffers) FillTransmitBuffers();
        return true;
    } else {
        GS->cm.droppedMeshPackets++;
        droppedPackets++;

        GS->logger.LogCustomCount(CustomErrorTypes::COUNT_DROPPED_PACKETS);

        //TODO: Error handling: What should happen when the queue is full?
        //Currently, additional packets are dropped
        logt("CM", "Send queue is already full");
        SIMSTATCOUNT("sendQueueFull");

        //For safety, we try to fill the transmitbuffers if it got stuck
        if(fillTxBuffers) FillTransmitBuffers();
        return false;
    }
}

void BaseConnection::FillTransmitBuffers()
{
    ErrorType err = ErrorType::SUCCESS;

    if (bufferFull) return;

    DYNAMIC_ARRAY(packetBuffer, connectionMtu);
    BaseConnectionSendData sendDataStruct;
    BaseConnectionSendData* sendData = &sendDataStruct;

    while(IsConnected() && connectionState != ConnectionState::REESTABLISHING && connectionState != ConnectionState::REESTABLISHING_HANDSHAKE)
    {
        //Check if there is important data from the subclass to be sent
        if(packetSendQueue.packetSendPosition == 0){
            TransmitHighPrioData();
        }

        //Next, select the correct Queue from which we should be transmitting
        //TODO: Currently we do not allow message splitting in HighPrio Queue
        PacketQueue* activeQueue;
        if(packetSendQueueHighPrio.numUnsentElements > 0 && packetSendQueue.packetSendPosition == 0){
            //logt("CONN", "Queuing from high prio queue");
            activeQueue = &packetSendQueueHighPrio;
        } else if(packetSendQueue.numUnsentElements > 0){
            //logt("CONN", "Queuing from normal prio queue");
            activeQueue = &packetSendQueue;
        } else {
            return;
        }

        if (activeQueue->_numElements < activeQueue->numUnsentElements) {
            logt("ERROR", "Fail: Queue numElements");
            SIMEXCEPTION(IllegalStateException);
            GS->logger.LogCustomError(CustomErrorTypes::FATAL_QUEUE_NUM_MISMATCH, (u16)(activeQueue == &packetSendQueue));

            GS->cm.ForceDisconnectAllConnections(AppDisconnectReason::QUEUE_NUM_MISMATCH);
            return;
        }

        //Get the next packet from the packet queue that was not yet queued
        SizedData packet = activeQueue->PeekNext(activeQueue->_numElements - activeQueue->numUnsentElements);

        //Unpack data from sendQueue
        BaseConnectionSendDataPacked* sendDataPacked = (BaseConnectionSendDataPacked*)packet.data;
        sendData->characteristicHandle = sendDataPacked->characteristicHandle;
        sendData->deliveryOption = (DeliveryOption)sendDataPacked->deliveryOption;
        sendData->priority = (DeliveryPriority)sendDataPacked->priority;
        sendData->dataLength = sendDataPacked->dataLength;
        u8* data = (packet.data + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED);

        //Check if this packet must be split, if yes make sure that there is no other split packet currently queued, we only allow one split
        //packet to be queued at one time (after one was queued, the packetSendPosition is reset to 0, as long as there are packetSentRemaining
        //we have not received acknowledgements for all parts
        if (packet.length > connectionPayloadSize && activeQueue->packetSendPosition == 0 && activeQueue->packetSentRemaining != 0) {
            return;
        }

        //The subclass is allowed to modify the packet before it is sent, it will place the modified packet into the sentData struct
        //This could be e.g. only a part of the original packet ( a split packet )
        SizedData sentData = ProcessDataBeforeTransmission(sendData, data, packetBuffer);

        if(sentData.length == 0){
            logt("ERROR", "Packet processing failed");
            GS->logger.LogCustomError(CustomErrorTypes::FATAL_PACKET_PROCESSING_FAILED, partnerId);
            DisconnectAndRemove(AppDisconnectReason::INVALID_PACKET);
            return;
        }

        //Send the packet to the SoftDevice
        if(sendData->deliveryOption == DeliveryOption::WRITE_REQ)
        {
            err = GS->gattController.BleWriteCharacteristic(
                    connectionHandle,
                    sendData->characteristicHandle,
                    sentData.data,
                    sentData.length,
                    true);
        }
        else if(sendData->deliveryOption == DeliveryOption::WRITE_CMD)
        {
            err = GS->gattController.BleWriteCharacteristic(
                    connectionHandle,
                    sendData->characteristicHandle,
                    sentData.data,
                    sentData.length,
                    false);
        }
        else
        {
            err = GS->gattController.BleSendNotification(
                    connectionHandle,
                    sendData->characteristicHandle,
                    sentData.data,
                    sentData.length);
        }

        if(err == ErrorType::SUCCESS)
        {
            //FIXME: This is not using the preprocessed data (sentData)
            PacketSuccessfullyQueuedWithSoftdevice(activeQueue, sendDataPacked, data, &sentData);
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

            HandlePacketQueuingFail(*activeQueue, sendDataPacked, (u32)err);

            //Stop queuing packets for this connection to prevent infinite loops
            return;
        }
    }
}

void BaseConnection::HandlePacketQueued(PacketQueue* activeQueue, BaseConnectionSendDataPacked* sendDataPacked)
{
    //Save the queue handle in the packet, decrease the number of packets to be sent from that queue and reset our fail counter for that queue
    sendDataPacked->sendHandle = GetNextQueueHandle();
    activeQueue->numUnsentElements--;
    activeQueue->packetFailedToQueueCounter = 0;

#ifdef SIM_ENABLED
    //if (GS->node.configuration.nodeId == 37 && connectionHandle == 680) printf("Q@NODE %u QUEUED, giving handle %u to packet in %s gid %u (len %u)" EOL, GS->node.configuration.nodeId, sendDataPacked->sendHandle, activeQueue == &packetSendQueue ? "normalQueue" : "highPrioQueue", *((u32*)(((u8*)sendDataPacked) + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED + SIZEOF_CONN_PACKET_HEADER)), sendDataPacked->dataLength);
#endif
}

void BaseConnection::HandlePacketSent(u8 sentUnreliable, u8 sentReliable)
{
    bufferFull = false;

    //logt("CONN", "Data was sent %u, %u", sentUnreliable, sentReliable);

    //TODO: are write cmds and write reqs sent sequentially?
    //TODO: we must not send more than 100 packets from one queue, otherwise, the handles between
    //the queues will not match anymore to the sequence in that the packets were sent
    
    //We must iterate in a loop to delete all packets if more than one was sent
    u8 numSent = sentUnreliable + sentReliable;

    for(u32 i=0; i<numSent; i++){

        //Check if packets were sent manually using a softdevice call and thereby bypassing the sendQueues
        if (manualPacketsSent > 0) {
            manualPacketsSent--;
            continue;
        }

        //Find the queue from which the packet was sent
        PacketQueue* activeQueue;

        SizedData packet = packetSendQueue.PeekNext();
        BaseConnectionSendDataPacked* sendDataPacked = (BaseConnectionSendDataPacked*)packet.data;
        u8 handle = sendDataPacked != nullptr ? sendDataPacked->sendHandle : PACKET_QUEUED_HANDLE_NOT_QUEUED_IN_SD;

        packet = packetSendQueueHighPrio.PeekNext();
        BaseConnectionSendDataPacked* sendDataPackedHighPrio = (BaseConnectionSendDataPacked*)packet.data;
        u8 handleHighPrio = sendDataPackedHighPrio != nullptr ? sendDataPackedHighPrio->sendHandle : PACKET_QUEUED_HANDLE_NOT_QUEUED_IN_SD;

        //If no queue has a handle, the packets must be from the normal queue because it was sending a split packet (but not all parts yet)
        if (handle < PACKET_QUEUED_HANDLE_COUNTER_START && handleHighPrio < PACKET_QUEUED_HANDLE_COUNTER_START) {
            activeQueue = &packetSendQueue;
#ifdef SIM_ENABLED
            if (packetSendQueue.packetSentRemaining == 0) {
                SIMEXCEPTION(IllegalStateException);
            }
#endif
        }
        //Check if we do not have a queued packet in the normal queue
        else if (handle < PACKET_QUEUED_HANDLE_COUNTER_START) {
            activeQueue = &packetSendQueueHighPrio;
        }
        //Check if we do not have a queued packet in the high prio queue
        else if (handleHighPrio < PACKET_QUEUED_HANDLE_COUNTER_START) {
            activeQueue = &packetSendQueue;
        }
        //Check which handle is lower than the other handle using unsigned variables that will wrap
        else {
            //Must be casted to u8, otherwhise type promotion results in an integer!
            if ((u8)(handle - handleHighPrio) < 100) {
                activeQueue = &packetSendQueueHighPrio;
            }
            else {
                activeQueue = &packetSendQueue;
            }
        }


        if(activeQueue->_numElements == 0){
            //TODO: Save Error
            logt("ERROR", "Fail: Queue");
            SIMEXCEPTION(IllegalStateException);

            GS->logger.LogCustomError(CustomErrorTypes::FATAL_HANDLE_PACKET_SENT_ERROR, partnerId);
        }

        //Check if a split packet should be acknowledged
        bool ackForSplitPacket = false;
        if (activeQueue == &packetSendQueue && activeQueue->packetSentRemaining > 0 && sendDataPacked != nullptr && sendDataPacked->dataLength > connectionPayloadSize) {
            activeQueue->packetSentRemaining--;
            ackForSplitPacket = true;
        }

        //Otherwise, either a normal packet or a split packet can be removed
        if (!ackForSplitPacket || activeQueue->packetSentRemaining == 0) {
            SizedData data = activeQueue->PeekNext();

            BaseConnectionSendDataPacked* sendData = (BaseConnectionSendDataPacked*)data.data;

            //We must only remove the packet if it has a handle, it might have only been sent partially so far
            if (sendData->sendHandle != 0) {

#ifdef SIM_ENABLED
                if (GS->node.configuration.nodeId == 37 && connectionHandle == 680) {
                    //printf("Q@NODE %u DISCARDS %s (packetHandle %u), gid %u (%u)" EOL, GS->node.configuration.nodeId, sendData->deliveryOption == (u8)DeliveryOption::WRITE_REQ ? "WRITE_REQ" : "WRITE_CMD", sendData->sendHandle, *((u32*)(packetHeader + 1)), sendData->dataLength);
                }
                //A quick check if a wrong packet was removed (not a 100% check, but helps)
                if (sendData->deliveryOption == (u8)DeliveryOption::WRITE_REQ && !sentReliable) {
                    SIMEXCEPTION(IllegalStateException);
                }
#endif

                DataSentHandler(data.data + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED, data.length - SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED);
                activeQueue->DiscardNext();
            }
        }
    }

    //Log how many packets have been sent
    this->sentUnreliable += sentUnreliable;
    this->sentReliable += sentReliable;
}

void BaseConnection::HandlePacketQueuingFail(PacketQueue& activeQueue, BaseConnectionSendDataPacked* sendDataPacked, u32 err)
{
    activeQueue.packetFailedToQueueCounter++;

    if (
        err != (u32)ErrorType::DATA_SIZE && 
        err != (u32)ErrorType::TIMEOUT && 
        err != (u32)ErrorType::INVALID_ADDR && 
        err != (u32)ErrorType::INVALID_PARAM) {
        //The remaining errors can happen if the gap connection is temporarily lost during reestablishing
        return;
    }

    //Check queuing a packet failed too often so that the connection is probably broken (could also be too many wrong packets)
    if (activeQueue.packetFailedToQueueCounter > BASE_CONNECTION_MAX_SEND_FAIL) {
        DisconnectAndRemove(AppDisconnectReason::TOO_MANY_SEND_RETRIES);
    }
}


void BaseConnection::ResendAllPackets(PacketQueue& queueToReset) const
{
    queueToReset.numUnsentElements = queueToReset._numElements;
    queueToReset.packetSendPosition = 0;
    queueToReset.packetSentRemaining = 0;

    //Clear all send handles
    for (int i = 0; i < queueToReset._numElements; i++)
    {
        SizedData packet = queueToReset.PeekNext(i);
        BaseConnectionSendDataPacked* sendDataPacked = (BaseConnectionSendDataPacked*)packet.data;
        sendDataPacked->sendHandle = PACKET_QUEUED_HANDLE_NOT_QUEUED_IN_SD;
    }
}

//This basic implementation returns the data as is
SizedData BaseConnection::ProcessDataBeforeTransmission(BaseConnectionSendData* sendData, u8* data, u8* packetBuffer)
{
    SizedData result;
    result.data = data;
    result.length = sendData->dataLength <= connectionMtu ? sendData->dataLength : 0;
    return result;
}

void BaseConnection::PacketSuccessfullyQueuedWithSoftdevice(PacketQueue* queue, BaseConnectionSendDataPacked* sendDataPacked, u8* data, SizedData* sentData)
{
    HandlePacketQueued(queue, sendDataPacked);
}

//This function can split a packet if necessary for sending
//WARNING: Can only be used for one characteristic, does not support to be used in parallel
//The implementation must manage packetSendPosition and packetQueue discarding itself
//packetBuffer must have a size of connectionMtu
//The function will return a pointer to the packet and the dataLength (do not use packetBuffer, use this pointer)
SizedData BaseConnection::GetSplitData(const BaseConnectionSendData &sendData, u8* data, u8* packetBuffer) const
{
    SizedData result;
    ConnPacketSplitHeader* resultHeader = (ConnPacketSplitHeader*) packetBuffer;
    
    //If we do not have to split the data, return data unmodified
    if(sendData.dataLength <= connectionPayloadSize){
        result.data = data;
        result.length = sendData.dataLength;
        return result;
    }

    u16 payloadSize = connectionPayloadSize - SIZEOF_CONN_PACKET_SPLIT_HEADER;

    //Check if this is the last packet
    if((packetSendQueue.packetSendPosition+1) * payloadSize >= sendData.dataLength){
        //End packet
        resultHeader->splitMessageType = MessageType::SPLIT_WRITE_CMD_END;
        resultHeader->splitCounter = packetSendQueue.packetSendPosition;
        CheckedMemcpy(
                packetBuffer + SIZEOF_CONN_PACKET_SPLIT_HEADER,
            data + packetSendQueue.packetSendPosition * payloadSize,
            sendData.dataLength - packetSendQueue.packetSendPosition * payloadSize);
        result.data = packetBuffer;
        result.length = (sendData.dataLength - packetSendQueue.packetSendPosition * payloadSize) + SIZEOF_CONN_PACKET_SPLIT_HEADER;
        if(result.length < 5){
            logt("WARNING", "Split packet because of very few bytes, optimisation?");
        }

        char stringBuffer[100];
        Logger::ConvertBufferToHexString(result.data, result.length, stringBuffer, sizeof(stringBuffer));
        logt("CONN_DATA", "SPLIT_END_%u: %s", resultHeader->splitCounter, stringBuffer);

    } else {
        //Intermediate packet
        resultHeader->splitMessageType = MessageType::SPLIT_WRITE_CMD;
        resultHeader->splitCounter = packetSendQueue.packetSendPosition;
        CheckedMemcpy(
                packetBuffer + SIZEOF_CONN_PACKET_SPLIT_HEADER,
            data + packetSendQueue.packetSendPosition * payloadSize,
            payloadSize);
        result.data = packetBuffer;
        result.length = connectionPayloadSize;

        char stringBuffer[100];
        Logger::ConvertBufferToHexString(result.data, result.length, stringBuffer, sizeof(stringBuffer));
        logt("CONN_DATA", "SPLIT_%u: %s", resultHeader->splitCounter, stringBuffer);
    }

    return result;
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
        GS->logger.LogCustomError(CustomErrorTypes::FATAL_PACKET_TOO_BIG, sendData->dataLength);
        packetReassemblyPosition = 0;
        currentMessageIsMissingASplit = true;
        SIMEXCEPTION(PacketTooBigException);
        return nullptr;
    }

    u16 packetReassemblyDestination = packetHeader->splitCounter * (connectionPayloadSize - SIZEOF_CONN_PACKET_SPLIT_HEADER);

    //Check if a packet was missing inbetween
    if(packetReassemblyPosition != packetReassemblyDestination){
        GS->logger.LogCustomError(CustomErrorTypes::WARN_SPLIT_PACKET_MISSING, (packetReassemblyDestination - packetReassemblyPosition));
        packetReassemblyPosition = 0;
        currentMessageIsMissingASplit = true;
        SIMEXCEPTION(SplitMissingException);
        return nullptr;
    }

    //Intermediate packets must always be a full MTU
    if(packetHeader->splitMessageType == MessageType::SPLIT_WRITE_CMD && sendData->dataLength != connectionPayloadSize){
        GS->logger.LogCustomError(CustomErrorTypes::WARN_SPLIT_PACKET_NOT_IN_MTU, sendData->dataLength);
        packetReassemblyPosition = 0;
        currentMessageIsMissingASplit = true;
        SIMEXCEPTION(SplitNotInMTUException);
        return nullptr;
    }

    //Save at correct position in the reassembly buffer
    CheckedMemcpy(
        packetReassemblyBuffer.data() + packetReassemblyDestination,
        data + SIZEOF_CONN_PACKET_SPLIT_HEADER,
        sendData->dataLength - SIZEOF_CONN_PACKET_SPLIT_HEADER);

    packetReassemblyPosition += sendData->dataLength - SIZEOF_CONN_PACKET_SPLIT_HEADER;

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

    connectionMtu = MAX_DATA_SIZE_PER_WRITE;
    connectionPayloadSize = MAX_DATA_SIZE_PER_WRITE;

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

u8 BaseConnection::GetNextQueueHandle()
{
    packetQueuedHandleCounter++;
    if (packetQueuedHandleCounter == 0) packetQueuedHandleCounter = PACKET_QUEUED_HANDLE_COUNTER_START;

    return packetQueuedHandleCounter;
}


SizedData BaseConnection::GetNextPacketToSend(const PacketQueue& queue) const
{
    for (u32 i = 0; i < queue._numElements; i++) {
        SizedData data = queue.PeekNext(i); //TODO: Optimize this, as this will iterate again through all items each loop
        BaseConnectionSendData* sendData = (BaseConnectionSendData*)data.data;

        if (sendData->sendHandle == PACKET_QUEUED_HANDLE_NOT_QUEUED_IN_SD) {
            return data;
        }
    }

    SizedData zeroData = {nullptr, 0};
    return zeroData;
}

u32 BaseConnection::GetAmountOfRemovedConnections()
{
    return GS->amountOfRemovedConnections;
}

#ifdef SIM_ENABLED
//Helper function to print the contents of the queues, not used in the code, but very useful for executing
//in the debugger
void BaseConnection::PrintQueueInfo()
{
    PacketQueue* queue = nullptr;
    for (int i = 0; i < 2; i++)
    {
        if (i == 0) 
        {
            queue = &packetSendQueue;
            printf("------ Normal Queue Last to First (%u), sendRemaining %u ------" EOL, queue->_numElements, queue->packetSentRemaining);
        }
        else if (i == 1)
        {
            queue = &packetSendQueueHighPrio;
            printf("------ High Prio Queue Last to First (%u), sendRemaining %u ------" EOL, queue->_numElements, queue->packetSentRemaining);
        }

        for (int k = 0; k < queue->_numElements; k++) {
            SizedData data = queue->PeekNext(k);
            BaseConnectionSendDataPacked* sendData = (BaseConnectionSendDataPacked*)data.data;
            ConnPacketHeader* header = (ConnPacketHeader*)(data.data + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED);

            const char* type = "INVALID";
            if (sendData->deliveryOption == (u8)DeliveryOption::WRITE_CMD) type = "WRITE_CMD";
            if (sendData->deliveryOption == (u8)DeliveryOption::WRITE_REQ) type = "WRITE_REQ";
            if (sendData->deliveryOption == (u8)DeliveryOption::NOTIFICATION) type = "NOTIF";

            printf("%s len %u, handle %u, messageType %u, (from %u to %u)" EOL, type, sendData->dataLength, sendData->sendHandle, (u32)header->messageType, header->sender, header->receiver);
        }
    }
}
#endif

/* EOF */

