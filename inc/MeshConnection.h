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

/*
 * The MeshConnection Class in instantiated once for every possible BLE connection,
 * either as a Master or Slave Connections. It provides methods and event handlers
 * to receive or send messages and handles the mesh-handshake.
 */

#pragma once

#include <BaseConnection.h>


class MeshConnection
	: public BaseConnection
{
#ifdef SIM_ENABLED
	friend class CherrySim;
#endif
	friend class ConnectionManager;
	friend class Node;

	private:

		u16 partnerWriteCharacteristicHandle;

		//Mesh variables
		u8 connectionMasterBit;
		clusterID connectedClusterId;
		clusterSIZE connectedClusterSize;
		clusterSIZE hopsToSink;

		clusterID clusterIDBackup;
		clusterSIZE clusterSizeBackup;
		clusterSIZE hopsToSinkBackup;

		//Timestamp and Clustering messages must be sent immediately and are not queued
		//Multiple updates can accumulate in this variable
		//This packet must not be sent during handshakes
		connPacketClusterInfoUpdate currentClusterInfoUpdatePacket;

		//Handshake
		connPacketClusterAck1 clusterAck1Packet;
		connPacketClusterAck2 clusterAck2Packet;

	public:
		//Init + Destroy
		MeshConnection(u8 id, ConnectionDirection direction, fh_ble_gap_addr_t* partnerAddress, u16 partnerWriteCharacteristicHandle);

		virtual ~MeshConnection();
		static BaseConnection* ConnTypeResolver(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8* data);

		void SaveClusteringSnapshot();
		void Disconnect();
//
//		//GATT Handshake
//		void DiscoverCharacteristicHandles();
//		void GATTHandleDiscoveredHandler(u16 characteristicHandle);

		//Mesh Handshake
		void StartHandshake();
		void ReceiveHandshakePacketHandler(BaseConnectionSendData* sendData, u8* data);

		//Sending Data
		bool TransmitHighPrioData();
		sizedData ProcessDataBeforeTransmission(BaseConnectionSendData* sendData, u8* data, u8* packetBuffer);
		void PacketSuccessfullyQueuedWithSoftdevice(BaseConnectionSendData* sendData, u8* data, sizedData* sentData);

		bool SendHandshakeMessage(u8* data, u8 dataLength, bool reliable);
		bool SendData(BaseConnectionSendData* sendData, u8* data);
		bool SendData(u8* data, u8 dataLength, DeliveryPriority priority, bool reliable);

		//Receiving Data
		void ReceiveDataHandler(BaseConnectionSendData* sendData, u8* data);
		//Called for received mesh messages after data has been processed
		void ReceiveMeshMessageHandler(BaseConnectionSendData* sendData, u8* data);
		//Called for distributing a received message to all modules
		void DispatchMeshMessage(BaseConnectionSendData* sendData, connPacketHeader* packet);

		//Handler
		void DisconnectionHandler();

		//Helpers
		void PrintStatus();
		bool GetPendingPackets();
};
