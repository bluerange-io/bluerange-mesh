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
	friend class FruitySimServer;
#endif
	friend class ConnectionManager;
	friend class Node;

	private:

		u16 partnerWriteCharacteristicHandle;

		//Mesh variables
		u8 connectionMasterBit;
		ClusterId connectedClusterId;
		ClusterSize connectedClusterSize;
		ClusterSize hopsToSink;

		ClusterId clusterIDBackup;
		ClusterSize clusterSizeBackup;
		ClusterSize hopsToSinkBackup;

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
		void DisconnectAndRemove() override;
//
//		//GATT Handshake
//		void DiscoverCharacteristicHandles() override;
//		void GATTHandleDiscoveredHandler(u16 characteristicHandle);

		//Mesh Handshake
		void StartHandshake() override;
		void ReceiveHandshakePacketHandler(BaseConnectionSendData* sendData, u8* data);
		void SendReconnectionHandshakePacket();
		void ReceiveReconnectionHandshakePacket(connPacketReconnect* packet);

		bool SendHandshakeMessage(u8* data, u8 dataLength, bool reliable);

		void TryReestablishing();

		//Sending Data
		bool TransmitHighPrioData() override;
		sizedData ProcessDataBeforeTransmission(BaseConnectionSendData* sendData, u8* data, u8* packetBuffer) override;
		void PacketSuccessfullyQueuedWithSoftdevice(PacketQueue* queue, BaseConnectionSendDataPacked* sendDataPacked, u8* data, sizedData* sentData) override;


		bool SendData(BaseConnectionSendData* sendData, u8* data);
		bool SendData(u8* data, u8 dataLength, DeliveryPriority priority, bool reliable) override;

		//Receiving Data
		void ReceiveDataHandler(BaseConnectionSendData* sendData, u8* data) override;
		//Called for received mesh messages after data has been processed
		void ReceiveMeshMessageHandler(BaseConnectionSendData* sendData, u8* data);

		//Handler
		bool GapDisconnectionHandler(u8 hciDisconnectReason) override;
		void ReconnectionSuccessfulHandler(ble_evt_t& bleEvent) override;

		//Helpers
		void PrintStatus() override;
		bool GetPendingPackets() override;
};
