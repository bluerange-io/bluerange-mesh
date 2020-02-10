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
#include <BaseConnection.h>

class MeshAccessModule;
struct MeshAccessServiceStruct;

constexpr int MESH_ACCESS_MIC_LENGTH = 4;
constexpr int MESH_ACCESS_HANDSHAKE_NONCE_LENGTH = 8;

enum class MeshAccessTunnelType: u8
{
	PEER_TO_PEER = 0,
	REMOTE_MESH = 1,
	LOCAL_MESH = 2,
	MESH_TO_MESH_NOT_IMPLEMENTED = 3,
	INVALID = 0xFF
};

#pragma pack(push)
#pragma pack(1)
struct DeadDataMessage
{
	connPacketHeader header;
	u8 magicNumber[8];
};
constexpr u8 deadDataMagicNumber[] = { 0xDE, 0xAD, 0xDA, 0xDA, 0x00, 0xFF, 0x77, 0x33 };
static_assert(sizeof(deadDataMagicNumber) == sizeof(DeadDataMessage::magicNumber), "The length of magic numbers does not match.");
STATIC_ASSERT_SIZE(DeadDataMessage, 13);
#pragma pack(pop)

class MeshAccessConnection
		: public BaseConnection
{

private:

	MeshAccessServiceStruct* meshAccessService;
	MeshAccessModule* meshAccessMod;

	bool useCustomKey = false;
	u8 key[16] = { 0 };

	FmKeyId fmKeyId = FmKeyId::ZERO;

	static constexpr u32 MAX_CORRUPTED_MESSAGES = 32;
	u32 amountOfCorruptedMessages = 0;
	bool allowCorruptedEncryptionStart = false;

	u8 sessionEncryptionKey[16] = { 0 };
	u8 sessionDecryptionKey[16] = { 0 };

	u32 encryptionNonce[2] = { 0 };
	u32 decryptionNonce[2] = { 0 };


	MessageType lastProcessedMessageType;


	bool GenerateSessionKey(const u8* nonce, NodeId centralNodeId, FmKeyId fmKeyId, u8* keyOut);
	void OnCorruptedMessage();

	void LogKeys();

public:

	//The tunnel type describes the direction in which the MeshAccess connection works
	//We cannot support a mesh on both sides without additional routing data, therefore
	//we have to decide whether we route data through the remote mesh or through the local mesh
	MeshAccessTunnelType tunnelType;

	u16 partnerRxCharacteristicHandle = 0;
	u16 partnerTxCharacteristicHandle = 0;
	u16 partnerTxCharacteristicCccdHandle = 0;

	//Each MeshAccess connection is assigned a virtual partner id, that is used as a temporary NodeId in
	//our own mesh network in order to participate
	NodeId virtualPartnerId;

	//If this is set to anything other than 0, the connectionState changes will be reported to this node
	NodeId connectionStateSubscriberId = 0;


	MeshAccessConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr* partnerAddress, FmKeyId fmKeyId, MeshAccessTunnelType tunnelType);
	virtual ~MeshAccessConnection();
	static BaseConnection* ConnTypeResolver(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8* data);

	void SetCustomKey(u8* key);

	/*############### Connect ##################*/
	//Returns the unique connection id that was created
	static u32 ConnectAsMaster(FruityHal::BleGapAddr* address, u16 connIntervalMs, u16 connectionTimeoutSec, FmKeyId fmKeyId, u8* customKey, MeshAccessTunnelType tunnelType);

	//Will create a connection that collects potential candidates and connects to them
	static u16 SearchAndConnectAsMaster(NetworkId networkId, u32 serialNumberIndex, u16 searchTimeDs, u16 connIntervalMs, u16 connectionTimeoutSec);

	void RegisterForNotifications();

	/*############### Handshake ##################*/
	void StartHandshake(FmKeyId fmKeyId);
	void HandshakeANonce(connPacketEncryptCustomStart* inPacket);
	void HandshakeSNonce(connPacketEncryptCustomANonce* inPacket);
	void HandshakeDone(connPacketEncryptCustomSNonce* inPacket);

	void SendClusterState();
	void NotifyConnectionStateSubscriber(ConnectionState state) const;

	/*############### Encryption ##################*/
	//This will encrypt some data using the current session key with incrementing nonce/counter
	//The data buffer must have enough space to hold the 4 byte MIC at the end
	void EncryptPacket(u8* data, u16 dataLength);

	//Decrypts the data in place (dataLength includes MIC) with the session key
	bool DecryptPacket(u8* data, u16 dataLength);


	/*############### Sending ##################*/
	SizedData ProcessDataBeforeTransmission(BaseConnectionSendData* sendData, u8* data, u8* packetBuffer) override;
	bool SendData(BaseConnectionSendData* sendData, u8* data);
	bool SendData(u8* data, u16 dataLength, DeliveryPriority priority, bool reliable) override;
	bool ShouldSendDataToNodeId(NodeId nodeId) const;
	void PacketSuccessfullyQueuedWithSoftdevice(PacketQueue* queue, BaseConnectionSendDataPacked* sendDataPacked, u8* data, SizedData* sentData) override;

	/*############### Receiving ##################*/
	void ReceiveDataHandler(BaseConnectionSendData* sendData, u8* data) override;
	void ReceiveMeshAccessMessageHandler(BaseConnectionSendData* sendData, u8* data);

	/*############### Handler ##################*/
	void ConnectionSuccessfulHandler(u16 connectionHandle) override;
	bool GapDisconnectionHandler(FruityHal::BleHciError hciDisconnectReason) override;
	void GATTServiceDiscoveredHandler(FruityHal::BleGattDBDiscoveryEvent &evt) override;

	void PrintStatus() override;

	u32 getAmountOfCorruptedMessaged();
};

