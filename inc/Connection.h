/**

Copyright (c) 2014-2015 "M-Way Solutions GmbH"
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

/*
 * The Connection Class in instantiated once for every possible BLE connection,
 * either as a Master or Slave Connections. It provides methods and event handlers
 * to receive or send messages and handles the mesh-handshake.
 */

#pragma once

#include <types.h>
#include <Config.h>
#include <PacketQueue.h>

extern "C"{
	#include <ble.h>
	#include <ble_gap.h>
}

class Node;
class ConnectionManager;

class Connection
{
	private:
		Node* node; //Reference to the own Node
		ConnectionManager* cm;

	public:
		//Types
		enum ConnectionDirection { CONNECTION_DIRECTION_IN, CONNECTION_DIRECTION_OUT };
		enum ConnectionState {DISCONNECTED=0, CONNECTING=1, CONNECTED=2, HANDSHAKING=3, HANDSHAKE_DONE=4, REESTABLISHING=5};
		enum EncryptionState {NOT_ENCRYPTED=0, ENCRYPTING=1, ENCRYPTED=2};

		ConnectionState connectionState;
		EncryptionState encryptionState;


		ConnectionState connectionStateBeforeDisconnection;
		u8 disconnectionReason;

		//Initializes connection but does not connect
		Connection(u8 id, ConnectionManager* cm, Node* node, ConnectionDirection direction);
		void ResetValues();

		void DiscoverCharacteristicHandles(void);
		void StartHandshake(void);

		bool PrepareConnection(ble_gap_addr_t* address, u16 writeCharacteristicHandle);

		void PrepareDisconnect();
		void Disconnect();

		//Handler
		void ConnectionSuccessfulHandler(ble_evt_t* bleEvent);
		void DisconnectionHandler(ble_evt_t* bleEvent);
		void ReceivePacketHandler(connectionPacket* inPacket);
		//void SendNextMessageHandler(ble_evt_t* bleEvent);
		void ClusterUpdateSentHandler();
		
		void BleEventHandler(ble_evt_t* bleEvent);

		//Helpers
		void PrintStatus(void);

		i8 GetAverageRSSI();
		u16 GetPendingPackets();

		//Getter
		bool isDisconnected(){ return connectionState == ConnectionState::DISCONNECTED; };
		bool isConnected(){ return connectionState >= ConnectionState::CONNECTED; };
		bool handshakeDone(){ return connectionState >= ConnectionState::HANDSHAKE_DONE; };

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
		u8 packetSendBuffer[PACKET_SEND_BUFFER_SIZE];
		PacketQueue* packetSendQueue;
		u8 packetSendPosition; //Is used to send messages that consist of multiple parts

		u8 lastSentPacket[MAX_DATA_SIZE_PER_WRITE];

		//Timestamp and Clustering messages must be sent immediately and are not queued
		//Multiple updates can accumulate in this variable
		//This packet must not be sent during handshakes
		connPacketClusterInfoUpdate currentClusterInfoUpdatePacket;

		u8 packetReassemblyBuffer[PACKET_REASSEMBLY_BUFFER_SIZE];
		u8 packetReassemblyPosition; //Set to 0 if no reassembly is in progress

		//Partner
		nodeID partnerId;
		u16 connectionHandle; //The handle that is given from the BLE stack to identify a connection
		ble_gap_addr_t partnerAddress;
		u16 writeCharacteristicHandle;
		u16 currentConnectionInterval;
		u32 connectionHandshakedTimestamp;
		
		//Handshake
		connPacketClusterAck1 clusterAck1Packet;
		connPacketClusterAck2 clusterAck2Packet;

		clusterID clusterIDBackup;
		clusterSIZE clusterSizeBackup;
		clusterSIZE hopsToSinkBackup;

		//Mesh variables
		u8 connectionMasterBit;
		clusterID connectedClusterId;
		clusterSIZE connectedClusterSize;
		clusterSIZE hopsToSink;
		u32 handshakeStarted;

		//Debug info
		u16 droppedPackets;
		u16 sentReliable;
		u16 sentUnreliable;

};
