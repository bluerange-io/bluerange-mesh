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


#pragma once

#include <types.h>
#include <Config.h>
#include <PacketQueue.h>
#include <conn_packets.h>
#include <Logger.h>

extern "C"{
	#include <ble.h>
	#include <ble_gap.h>
}


enum DeliveryOption {
	DELIVERY_OPTION_INVALID,
	DELIVERY_OPTION_WRITE_CMD,
	DELIVERY_OPTION_WRITE_REQ,
	DELIVERY_OPTION_NOTIFICATION
};
enum DeliveryPriority {
	DELIVERY_PRIORITY_HIGH=0,
	DELIVERY_PRIORITY_MEDIUM=1,
	DELIVERY_PRIORITY_LOW=2,
	DELIVERY_PRIORITY_LOWEST=3,
	DELIVERY_PRIORITY_INVALID=4,
};


typedef struct BaseConnectionSendData {
	u16 characteristicHandle;
	DeliveryOption deliveryOption;
	DeliveryPriority priority;
	u8 dataLength;
} BaseConnectionSendData;

#define SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED 4
#pragma pack(push)
#pragma pack(1)
typedef struct BaseConnectionSendDataPacked {
	u16 characteristicHandle;
	u8 deliveryOption : 4;
	u8 priority : 4;
	u8 dataLength;
} BaseConnectionSendDataPacked;
#pragma pack(pop)

class Node;
class ConnectionManager;


//Connection Direction: In => We are peripheral, Out => We are central
enum ConnectionDirection { CONNECTION_DIRECTION_IN, CONNECTION_DIRECTION_OUT, CONNECTION_DIRECTION_INVALID };
//Possible states for a connection
enum ConnectionState {CONNECTION_STATE_DISCONNECTED=0, CONNECTION_STATE_CONNECTING=1, CONNECTION_STATE_CONNECTED=2, CONNECTION_STATE_HANDSHAKING=3, CONNECTION_STATE_HANDSHAKE_DONE=4, CONNECTION_STATE_REESTABLISHING=5};
//State of connection encryption
enum EncryptionState {NOT_ENCRYPTED=0, ENCRYPTING=1, ENCRYPTED=2};

class BaseConnection
{
	protected:
		//Will Queue the data in the packet queue of the connection
		bool QueueData(BaseConnectionSendData* sendData, u8* data);

		bool PrepareBaseConnection(fh_ble_gap_addr_t* address, ConnectionTypes connectionType);

		u16 connectionMtu;

	public:

		ConnectionTypes connectionType;
		ConnectionState connectionState;
		EncryptionState encryptionState;
		//Backup for the last conneciton state before it was disconnected
		ConnectionState connectionStateBeforeDisconnection;
		u8 disconnectionReason;


		//################### Connection creation ######################

		//Initializes connection but does not connect
		BaseConnection(u8 id, ConnectionDirection direction, fh_ble_gap_addr_t* partnerAddress);
		virtual ~BaseConnection();

		//GATT Handshake
		virtual void DiscoverCharacteristicHandles(){};
		virtual void GATTHandleDiscoveredHandler(u16 characteristicHandle){};

		//Custom handshake
		virtual void StartHandshake(){};


		virtual void Disconnect();

		//################### Sending ######################
		//Must be implemented in super class
		virtual bool SendData(u8* data, u8 dataLength, DeliveryPriority priority, bool reliable) = 0;
		//Allow a subclass to transmit data before the writeQueue is processed
		virtual bool TransmitHighPrioData() { return false; };
		//Allows a subclass to process data closely before sending it
		virtual sizedData ProcessDataBeforeTransmission(BaseConnectionSendData* sendData, u8* data, u8* packetBuffer);
		//Called after data has been queued in the softdevice
		virtual void PacketSuccessfullyQueuedWithSoftdevice(BaseConnectionSendData* sendData, u8* data, sizedData* sentData);
		//Fills the tx buffers of the softdevice with the packets from the packet queue
		virtual void FillTransmitBuffers();

		//Handler
		virtual void ConnectionSuccessfulHandler(u16 connectionHandle, u16 connInterval);
		virtual void ReconnectionSuccessfulHandler(ble_evt_t* bleEvent);
		virtual void DisconnectionHandler();
		//Called when data from a connection is received
		virtual void ReceiveDataHandler(BaseConnectionSendData* sendData, u8* data){};
		//Can be called by subclasses to use the connPacketHeader reassembly
		u8* ReassembleData(BaseConnectionSendData* sendData, u8* data);
		sizedData GetSplitData(BaseConnectionSendData* sendData, u8* data, u8* packetBuffer);


		void BleEventHandler(ble_evt_t* bleEvent);

		//Helpers
		virtual void PrintStatus(){};

		i8 GetAverageRSSI();
		//Must return the number of packets that are queued. (Not just in the Packetqueue, also HighPrioData!)
		virtual bool GetPendingPackets() { return packetSendQueue->_numElements; };

		//Getter
		bool isDisconnected(){ return connectionState == ConnectionState::CONNECTION_STATE_DISCONNECTED; };
		bool isConnected(){ return connectionState >= ConnectionState::CONNECTION_STATE_CONNECTED; };
		bool handshakeDone(){ return connectionState >= ConnectionState::CONNECTION_STATE_HANDSHAKE_DONE; };

		//Variables
		u8 connectionId;
		ConnectionDirection direction;
		
		//RSSI measurement
		u16 rssiSamplesSum; //Absolut sum of sampled rssi packets
		u16 rssiSamplesNum; //Number of samples
		i8 rssiAverage; //The averaged rssi of the last measurement

		//Buffers
		u8 unreliableBuffersFree; //Number of
		u8 reliableBuffersFree; //reliable transmit buffers that are available currently to this connection
		u32 packetSendBuffer[PACKET_SEND_BUFFER_SIZE/sizeof(u32)];
		PacketQueue* packetSendQueue;
		u8 packetSendPosition; //Is used to send messages that consist of multiple parts

		u8 packetReassemblyBuffer[PACKET_REASSEMBLY_BUFFER_SIZE];
		u8 packetReassemblyPosition; //Set to 0 if no reassembly is in progress

		//Partner
		nodeID partnerId;
		u16 connectionHandle; //The handle that is given from the BLE stack to identify a connection
		fh_ble_gap_addr_t partnerAddress;
		u16 currentConnectionIntervalMs;

		//Times
		u32 handshakeStartedDs;
		u32 connectionHandshakedTimestampDs;
		u16 reestablishTimeSec;
		u32 disconnectedTimestampDs;

		//Debug info
		u16 droppedPackets;
		u16 sentReliable;
		u16 sentUnreliable;

};
