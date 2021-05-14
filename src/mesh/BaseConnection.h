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


#pragma once

#include <FmTypes.h>
#include <Config.h>
#include <PacketQueue.h>
#include <Logger.h>
#include <FruityHal.h>
#include <array>

#include "ChunkedPriorityPacketQueue.h"
#include "SimpleQueue.h"

enum class DeliveryOption : u8 {
    INVALID,
    WRITE_CMD,
    WRITE_REQ,
    NOTIFICATION
};

typedef struct BaseConnectionSendData {
    u16 characteristicHandle;
    DeliveryOption deliveryOption;
    MessageLength dataLength;
} BaseConnectionSendData;

constexpr size_t SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED = 3;
#pragma pack(push)
#pragma pack(1)
typedef struct BaseConnectionSendDataPacked {
    u16 characteristicHandle;
    u8 deliveryOption : 4;
    // Unpacked version also has a dataLength, which is not required for the packed version.
} BaseConnectionSendDataPacked;
#pragma pack(pop)
STATIC_ASSERT_SIZE(BaseConnectionSendDataPacked, SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED);

class Node;
class ConnectionManager;

//The reason why this connection was purposefully disconnected
enum class AppDisconnectReason : u8 {
    UNKNOWN = 0,
    HANDSHAKE_TIMEOUT = 1,
    RECONNECT_TIMEOUT = 2,
    GAP_DISCONNECT_NO_REESTABLISH_REQUESTED = 3,
    SAME_CLUSTERID = 4,
    TOO_MANY_SEND_RETRIES = 5,
    I_AM_SMALLER = 6,
    PARTNER_HAS_MASTERBIT = 7,
    SHOULD_WAIT_AS_SLAVE = 8,
    LEAF_NODE = 9,
    STATIC_NODE = 10,
    QUEUE_NUM_MISMATCH = 11,
    CM_FAIL_NO_SPOT = 12,
    USER_REQUEST = 13,
    CURRENTLY_IN_HANDSHAKE = 14,
    GAP_CONNECTING_TIMEOUT = 15,
    PENDING_TIMEOUT = 16,
    ENROLLMENT_TIMEOUT = 17,
    ENROLLMENT_TIMEOUT2 = 18,
    NETWORK_ID_MISMATCH = 19,
    RECONNECT_BLE_ERROR = 20,
    UNPREFERRED_CONNECTION = 21,
    EMERGENCY_DISCONNECT = 22,
    GAP_ERROR = 23,
    WRONG_PARTNERID = 24,
    ILLEGAL_TUNNELTYPE = 25,
    INVALID_KEY = 26,
    INVALID_PACKET = 27,
    ENROLLMENT_RESPONSE_RECEIVED = 28,
    NEEDED_FOR_ENROLLMENT = 29,
    WRONG_DIRECTION = 30,
    GATTC_TIMEOUT = 31,
    //ASSET_SCHEDULE_REMOVE = 32, //Deprecated as of 03.03.2020
    INVALID_HANDSHAKE_PACKET = 33,
    REBOOT = 34,
    EMERGENCY_DISCONNECT_RESET = 35,
    SCHEDULED_REMOVE = 36,
    SERIAL_CONNECT_TIMEOUT = 37,
    EN_OCEAN_ENROLLED_AND_IN_MESH = 38,
    MULTIPLE_MA_ON_ASSET = 39,
    HANDLE_PACKET_SENT_ERROR = 40,
    MTU_UPGRADE_FAILED = 41,
};


//Connection Direction: In => We are peripheral, Out => We are central
enum class ConnectionDirection : u8{ 
    DIRECTION_IN,    //Can't make it shorter like "IN", because thats a predefined Microsoft macro.
    DIRECTION_OUT, 
    INVALID 
};

enum class DataDirection : u8 {
    DIRECTION_IN,    //Can't make it shorter like "IN", because thats a predefined Microsoft macro.
    DIRECTION_OUT
};

//Possible states for a connection
enum class ConnectionState : u8{
    DISCONNECTED=0, 
    CONNECTING=1, 
    CONNECTED=2, 
    HANDSHAKING=3, 
    HANDSHAKE_DONE=4, 
    REESTABLISHING = 5, 
    REESTABLISHING_HANDSHAKE = 6,
};
//State of connection encryption
enum class EncryptionState : u8{
    NOT_ENCRYPTED=0, 
    ENCRYPTING=1, 
    ENCRYPTED=2
};

/*
 * The BaseConnection is the root class for all BLE connections used within FruityMesh.
 * It includes some basic functionality for packet queuing, sending and receiving and also for state management.
 * Subclasses can use some of this functionality or override it.
 */
class BaseConnection
{
    private: 
        bool currentMessageIsMissingASplit = false;
    protected:
        DeliveryPriority overwritePriority = DeliveryPriority::INVALID;
        u8 dataSentBuffer[MAX_MESH_PACKET_SIZE];
        u8 dataSentLength;
        //Will Queue the data in the packet queue of the connection
        bool QueueData(const BaseConnectionSendData& sendData, u8 const * data, u32* messageHandle=nullptr);
        bool QueueData(const BaseConnectionSendData& sendData, u8 const * data, bool fillTxBuffers, u32* messageHandle=nullptr); // Can be used to avoid infinite recursion in queue and fillTxBuffers

        bool PrepareBaseConnection(FruityHal::BleGapAddr* address, ConnectionType connectionType) const;


    public:
        u16 connectionMtu = MAX_DATA_SIZE_PER_WRITE;
        u16 connectionPayloadSize = MAX_DATA_SIZE_PER_WRITE;

        ConnectionType connectionType = ConnectionType::INVALID;
        ConnectionState connectionState = ConnectionState::CONNECTING;
        EncryptionState encryptionState = EncryptionState::NOT_ENCRYPTED;
        //Backup for the last conneciton state before it was disconnected
        ConnectionState connectionStateBeforeDisconnection = ConnectionState::DISCONNECTED;
        FruityHal::BleHciError disconnectionReason = FruityHal::BleHciError::SUCCESS;
        AppDisconnectReason appDisconnectionReason = AppDisconnectReason::UNKNOWN;


        //################### Connection creation ######################

        //Initializes connection but does not connect
        BaseConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress);
        virtual ~BaseConnection();

        virtual void DisconnectAndRemove(AppDisconnectReason reason);

        //################### Sending ######################
        //Must be implemented in super class
        virtual bool SendData(u8 const * data, MessageLength dataLength, bool reliable, u32 * messageHandle) = 0;
        //Allow a subclass to transmit data before the writeQueue is processed
        virtual bool QueueVitalPrioData() { return false; };
        //Allows a subclass to process data closely before sending it
        virtual MessageLength ProcessDataBeforeTransmission(u8* message, MessageLength messageLength, MessageLength bufferLength);
        //Called after data has been queued in the softdevice, pay attention that data points to the full packet in the queue
        //whereas sentData is the data that was really sent (e.g. the packet was split or preprocessed in some way before sending)
        virtual void PacketSuccessfullyQueuedWithSoftdevice(SizedData* sentData);
        //Fills the tx buffers of the softdevice with the packets from the packet queue
        virtual void FillTransmitBuffers();
        //Gets passed the exact same data that was passed to the HAL. If that data was encrypted, the passed data
        //to this function is encrypted as well (e.g. in the MeshAccessConnection). This means that the data passed
        //to this function is the same as was returned by ProcessDataBeforeTransmission.
        virtual void DataSentHandler(const u8* data, MessageLength length, u32 messageHandle) {};

        //Calls GetPriorityOfMessage of all modules to determine the priority of the message.
        DeliveryPriority GetPriorityOfMessage(const u8* data, MessageLength size);

        //Handler
        virtual void ConnectionSuccessfulHandler(u16 connectionHandle);
        virtual void GapReconnectionSuccessfulHandler(const FruityHal::GapConnectedEvent& connectedEvent);
        virtual bool GapDisconnectionHandler(FruityHal::BleHciError hciDisconnectReason);
        virtual void GATTServiceDiscoveredHandler(FruityHal::BleGattDBDiscoveryEvent& evt);
        //Called, once the MTU of the connection was upgraded. The connection can then increase the packet splitting size
        virtual void ConnectionMtuUpgradedHandler(u16 gattPayloadSize);
        //Called when data from a connection is received
        virtual void ReceiveDataHandler(BaseConnectionSendData* sendData, u8 const * data) = 0;
        //Can be called by subclasses to use the ConnPacketHeader reassembly
        u8 const * ReassembleData(BaseConnectionSendData* sendData, u8 const * data);

#if IS_ACTIVE(CONN_PARAM_UPDATE)
        /// Called in response to a connection parameter update.
        virtual void GapConnParamUpdateHandler(
                const FruityHal::BleGapConnParams & params);
        /// Called on the device in the central role, when the remote device
        /// in the peripheral role requests an update of the connection
        /// parameters.
        virtual void GapConnParamUpdateRequestHandler(
                const FruityHal::BleGapConnParams & params);
#endif

        //Helpers
        virtual void PrintStatus() = 0;

        i8 GetAverageRSSI() const;
        //Must return the number of packets that are queued. (All queues of this connection!)
        virtual u32 GetPendingPackets() const { return queue.GetAmountOfPackets(); };

        void HandlePacketQueued();
        void HandlePacketQueuingFail(u32 err);
        void HandlePacketSent(u8 sentUnreliable, u8 sentReliable);

        //Getter
        bool IsDisconnected() const{ return connectionState == ConnectionState::DISCONNECTED; };
        bool IsConnected() const{ return connectionState >= ConnectionState::CONNECTED; };
        bool HandshakeDone() const{ return connectionState >= ConnectionState::HANDSHAKE_DONE; };

        //Variables
        u8 connectionId;
        const u32 uniqueConnectionId;
        ConnectionDirection direction;

        u8 clusterUpdateCounter : 1;
        u8 nextExpectedClusterUpdateCounter : 1;
        
        //RSSI measurement
        i32 rssiAverageTimes1000 = 0; //The averaged rssi of the connection multiplied by 1000
        i8 lastReportedRssi = 0; //The last rssi measurement that was reported

        //Buffers
        bool bufferFull = false; //Set to true once the softdevice reports that all buffers are full
        u8 manualPacketsSent = 0; //Used to count the packets manually sent to the softdevice using BleWriteCharacteristic, will be decremented first before packets from the queue are removed. Packets must not be sent while the queue is working

        SimpleQueue<DeliveryPriority, 32> queueOrigins;
        ChunkedPriorityPacketQueue queue;

        u32 packetFailedToQueueCounter = 0;

        alignas(4) std::array<u8, PACKET_REASSEMBLY_BUFFER_SIZE> packetReassemblyBuffer{};
        u8 packetReassemblyPosition = 0; //Set to 0 if no reassembly is in progress

        //Partner
        NodeId partnerId = 0;
        u16 connectionHandle = FruityHal::FH_BLE_INVALID_HANDLE; //The handle that is given from the BLE stack to identify a connection
        FruityHal::BleGapAddr partnerAddress;

        //Times
        const u32 creationTimeDs;
        u32 handshakeStartedDs = 0;
        u32 connectionHandshakedTimestampDs = 0; //Set after handshake completed, not modified when reestablishing the connection
        u32 disconnectedTimestampDs = 0;

        //Debug info
        u16 droppedPackets = 0;
        u16 sentReliable = 0;
        u16 sentUnreliable = 0;

        static u32 GetAmountOfRemovedConnections();
};
