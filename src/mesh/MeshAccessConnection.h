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
#include <AppConnection.h>

#ifdef ACTIVATE_MA_MODULE

class MeshAccessModule;
struct MeshAccessServiceStruct;

#define MESH_ACCESS_MIC_LENGTH 4
#define MESH_ACCESS_HANDSHAKE_NONCE_LENGTH 8

enum class MeshAccessTunnelType: u8
{
	PEER_TO_PEER = 0,
	REMOTE_MESH = 1,
	LOCAL_MESH = 2,
	MESH_TO_MESH_NOT_IMPLEMENTED = 3,
	INVALID = 0xFF
};

class MeshAccessConnection
		: public AppConnection
{

private:

	MeshAccessServiceStruct* meshAccessService;
	MeshAccessModule* meshAccessMod;

	bool useCustomKey;
	u8 key[16];

	u32 fmKeyId;

	u8 sessionEncryptionKey[16];
	u8 sessionDecryptionKey[16];

	u32 encryptionNonce[2];
	u32 decryptionNonce[2];


	u8 lastProcessedMessageType;


	bool GenerateSessionKey(u8* nonce, NodeId centralNodeId, u32 fmKeyId, u8* keyOut);

public:

	//The tunnel type describes the direction in which the MeshAccess connection works
	//We cannot support a mesh on both sides without additional routing data, therefore
	//we have to decide whether we route data through the remote mesh or through the local mesh
	MeshAccessTunnelType tunnelType;

	u16 partnerRxCharacteristicHandle;
	u16 partnerTxCharacteristicHandle;
	u16 partnerTxCharacteristicCccdHandle;

	//Each MeshAccess connection is assigned a virtual partner id, that is used as a temporary NodeId in
	//our own mesh network in order to participate
	NodeId virtualPartnerId;

	//If this is set to anything other than 0, the connectionState changes will be reported to this node
	NodeId connectionStateSubscriberId;


	MeshAccessConnection(u8 id, ConnectionDirection direction, fh_ble_gap_addr_t* partnerAddress, u32 fmKeyId, MeshAccessTunnelType tunnelType);
	virtual ~MeshAccessConnection();
	static BaseConnection* ConnTypeResolver(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8* data);

	void SetCustomKey(u8* key);

	/*############### Connect ##################*/
	//Returns the unique connection id that was created
	static u16 ConnectAsMaster(fh_ble_gap_addr_t* address, u16 connIntervalMs, u16 connectionTimeoutSec, u32 fmKeyId, u8* customKey, MeshAccessTunnelType tunnelType);

	//Will create a connection that collects potential candidates and connects to them
	static u16 SearchAndConnectAsMaster(NetworkId networkId, u32 serialNumberIndex, u16 searchTimeDs, u16 connIntervalMs, u16 connectionTimeoutSec);

	void RegisterForNotifications();

	/*############### Handshake ##################*/
	void StartHandshake(u16 fmKeyId);
	void HandshakeANonce(connPacketEncryptCustomStart* inPacket);
	void HandshakeSNonce(connPacketEncryptCustomANonce* inPacket);
	void HandshakeDone(connPacketEncryptCustomSNonce* inPacket);

	void NotifyConnectionStateSubscriber(ConnectionState state) const;

	/*############### Encryption ##################*/
	//This will encrypt some data using the current session key with incrementing nonce/counter
	//The data buffer must have enough space to hold the 4 byte MIC at the end
	void EncryptPacket(u8* data, u16 dataLength);

	//Decrypts the data in place (dataLength includes MIC) with the session key
	bool DecryptPacket(u8* data, u16 dataLength);


	/*############### Sending ##################*/
	sizedData ProcessDataBeforeTransmission(BaseConnectionSendData* sendData, u8* data, u8* packetBuffer) override;
	bool SendData(BaseConnectionSendData* sendData, u8* data);
	bool SendData(u8* data, u8 dataLength, DeliveryPriority priority, bool reliable) override;
	void PacketSuccessfullyQueuedWithSoftdevice(PacketQueue* queue, BaseConnectionSendDataPacked* sendDataPacked, u8* data, sizedData* sentData) override;

	/*############### Receiving ##################*/
	void ReceiveDataHandler(BaseConnectionSendData* sendData, u8* data) override;
	void ReceiveMeshAccessMessageHandler(BaseConnectionSendData* sendData, u8* data);

	/*############### Handler ##################*/
	void ConnectionSuccessfulHandler(u16 connectionHandle, u16 connInterval) override;
	bool GapDisconnectionHandler(u8 hciDisconnectReason) override;
	void GATTServiceDiscoveredHandler(ble_db_discovery_evt_t &evt) override;

	void BleEventHandler(ble_evt_t &bleEvent) override;

	void PrintStatus() override;

};

#endif
