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


#pragma once

#include <types.h>
#include <Config.h>
#include <PacketQueue.h>
#include <Logger.h>
#include "SimpleArray.h"

extern "C"{
	#include <ble.h>
	#include <ble_gap.h>
	#include <ble_db_discovery.h>
}

#define PACKET_QUEUED_HANDLE_NOT_QUEUED_IN_SD 0
#define PACKET_QUEUED_HANDLE_COUNTER_START 10


enum class DeliveryOption : u8 {
	INVALID,
	WRITE_CMD,
	WRITE_REQ,
	NOTIFICATION
};
enum class DeliveryPriority : u8{
	HIGH=0, //Must only be used for mesh relevant data
	MEDIUM=1,
	LOW=2,
	LOWEST=3,
	INVALID=4,
};


typedef struct BaseConnectionSendData {
	u16 characteristicHandle;
	DeliveryOption deliveryOption;
	DeliveryPriority priority;
	u8 sendHandle;
	u8 dataLength;
} BaseConnectionSendData;

#define SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED 5
#pragma pack(push)
#pragma pack(1)
typedef struct BaseConnectionSendDataPacked {
	u16 characteristicHandle;
	u8 deliveryOption : 4;
	u8 priority : 4;
	u8 sendHandle;
	u8 dataLength;
} BaseConnectionSendDataPacked;
#pragma pack(pop)
STATIC_ASSERT_SIZE(BaseConnectionSendDataPacked, 5);

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
	NETWORK_ID_MISMATCH = 19
};


//Connection Direction: In => We are peripheral, Out => We are central
enum class ConnectionDirection : u8{ 
	DIRECTION_IN,	//Can't make it shorter like "IN", because thats a predefined Microsoft macro.
	DIRECTION_OUT, 
	INVALID 
};
//Possible states for a connection
enum class ConnectionState{ //jstodo: this cant be u8 because else some tests fail. Check! Reproduce in commit a36ceefd75890b39c93e25ad7a44381444782aca
	DISCONNECTED=0, 
	CONNECTING=1, 
	CONNECTED=2, 
	HANDSHAKING=3, 
	HANDSHAKE_DONE=4, 
	REESTABLISHING = 5, 
	REESTABLISHING_HANDSHAKE = 6
};
//State of connection encryption
enum class EncryptionState : u8{
	NOT_ENCRYPTED=0, 
	ENCRYPTING=1, 
	ENCRYPTED=2
};

class BaseConnection
{
	protected:
		//Will Queue the data in the packet queue of the connection
		bool QueueData(const BaseConnectionSendData& sendData, u8* data);
		bool QueueData(const BaseConnectionSendData& sendData, u8* data, bool fillTxBuffers); // Can be used to avoid infinite recursion in queue and fillTxBuffers

		bool PrepareBaseConnection(fh_ble_gap_addr_t* address, ConnectionTypes connectionType) const;

		u16 connectionMtu;
		u16 connectionPayloadSize;

	public:

		ConnectionTypes connectionType;
		ConnectionState connectionState;
		EncryptionState encryptionState;
		//Backup for the last conneciton state before it was disconnected
		ConnectionState connectionStateBeforeDisconnection;
		u8 disconnectionReason;
		AppDisconnectReason appDisconnectionReason;


		//################### Connection creation ######################

		//Initializes connection but does not connect
		BaseConnection(u8 id, ConnectionDirection direction, fh_ble_gap_addr_t* partnerAddress);
		virtual ~BaseConnection();

		//GATT Handshake
		virtual void DiscoverCharacteristicHandles(){};
		virtual void GATTHandleDiscoveredHandler(u16 characteristicHandle){};

		//Custom handshake
		virtual void StartHandshake(){};


		virtual void DisconnectAndRemove();

		//################### Sending ######################
		//Must be implemented in super class
		virtual bool SendData(u8* data, u8 dataLength, DeliveryPriority priority, bool reliable) = 0;
		//Allow a subclass to transmit data before the writeQueue is processed
		virtual bool TransmitHighPrioData() { return false; };
		//Allows a subclass to process data closely before sending it
		virtual sizedData ProcessDataBeforeTransmission(BaseConnectionSendData* sendData, u8* data, u8* packetBuffer);
		//Called after data has been queued in the softdevice, pay attention that data points to the full packet in the queue
		//whereas sentData is the data that was really sent (e.g. the packet was split or preprocessed in some way before sending)
		virtual void PacketSuccessfullyQueuedWithSoftdevice(PacketQueue* queue, BaseConnectionSendDataPacked* sendDataPacked, u8* data, sizedData* sentData);
		//Fills the tx buffers of the softdevice with the packets from the packet queue
		virtual void FillTransmitBuffers();

		//Handler
		virtual void ConnectionSuccessfulHandler(u16 connectionHandle, u16 connInterval);
		virtual void ReconnectionSuccessfulHandler(ble_evt_t& bleEvent);
		virtual bool GapDisconnectionHandler(u8 hciDisconnectReason);
		virtual void GATTServiceDiscoveredHandler(ble_db_discovery_evt_t& evt);
		//Called when data from a connection is received
		virtual void ReceiveDataHandler(BaseConnectionSendData* sendData, u8* data){};
		//Can be called by subclasses to use the connPacketHeader reassembly
		u8* ReassembleData(BaseConnectionSendData* sendData, u8* data);
		sizedData GetSplitData(const BaseConnectionSendData &sendData, u8* data, u8* packetBuffer) const;


		virtual void BleEventHandler(ble_evt_t& bleEvent);

		//Helpers
		virtual void PrintStatus(){};

		i8 GetAverageRSSI() const;
		//Must return the number of packets that are queued. (Not just in the Packetqueues, also HighPrioData!)
		virtual bool GetPendingPackets() { return packetSendQueue._numElements + packetSendQueueHighPrio._numElements; };


		sizedData GetNextPacketToSend(const PacketQueue& queue) const;

		void HandlePacketQueued(PacketQueue* activeQueue, BaseConnectionSendDataPacked* sendDataPacked);
		void HandlePacketQueuingFail(PacketQueue& activeQueue, BaseConnectionSendDataPacked* sendDataPacked, u32 err);
		void HandlePacketSent(u8 sentUnreliable, u8 sentReliable);

		void ResendAllPackets(PacketQueue& queueToReset) const;

		u8 GetNextQueueHandle();

		//Getter
		bool isDisconnected() const{ return connectionState == ConnectionState::DISCONNECTED; };
		bool isConnected() const{ return connectionState >= ConnectionState::CONNECTED; };
		bool handshakeDone() const{ return connectionState >= ConnectionState::HANDSHAKE_DONE; };

		//Variables
		u8 connectionId;
		u16 uniqueConnectionId;
		ConnectionDirection direction;

		u8 clusterUpdateCounter : 1;
		u8 nextExpectedClusterUpdateCounter : 1;
		
		//RSSI measurement
		i32 rssiAverageTimes1000; //The averaged rssi of the connection multiplied by 1000
		i8 lastReportedRssi; //The last rssi measurement that was reported

		//Buffers
		u8 unreliableBuffersFree; //Number of
		u8 reliableBuffersFree; //reliable transmit buffers that are available currently to this connection

		u8 manualPacketsSent; //Used to count the packets manually sent to the softdevice using bleWriteCharacteristic, will be decremented first before packets from the queue are removed. Packets must not be sent while the queue is working

		//Normal Prio Queue
		u32 packetSendBuffer[PACKET_SEND_BUFFER_SIZE/sizeof(u32)];
		PacketQueue packetSendQueue;

		//High Prio Queue
		u32 packetSendBufferHighPrio[PACKET_SEND_BUFFER_HIGH_PRIO_SIZE/sizeof(u32)];
		PacketQueue packetSendQueueHighPrio;

		u8 packetQueuedHandleCounter; //Used to assign handles to queued packets

		SimpleArray<u8, PACKET_REASSEMBLY_BUFFER_SIZE> packetReassemblyBuffer;
		u8 packetReassemblyPosition; //Set to 0 if no reassembly is in progress

		//Partner
		NodeId partnerId;
		u16 connectionHandle; //The handle that is given from the BLE stack to identify a connection
		fh_ble_gap_addr_t partnerAddress;
		u16 currentConnectionIntervalMs;

		bool forceReestablish;

		//Times
		u32 creationTimeDs;
		u32 handshakeStartedDs;
		u32 connectionHandshakedTimestampDs;
		u16 reestablishTimeSec;
		u32 disconnectedTimestampDs;

		//Debug info
		u16 droppedPackets;
		u16 sentReliable;
		u16 sentUnreliable;

};
