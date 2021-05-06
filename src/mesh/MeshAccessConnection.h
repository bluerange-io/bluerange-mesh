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
    ConnPacketHeader header;
    u8 magicNumber[8];
};
constexpr u8 deadDataMagicNumber[] = { 0xDE, 0xAD, 0xDA, 0xDA, 0x00, 0xFF, 0x77, 0x33 };
static_assert(sizeof(deadDataMagicNumber) == sizeof(DeadDataMessage::magicNumber), "The length of magic numbers does not match.");
STATIC_ASSERT_SIZE(DeadDataMessage, 13);
#pragma pack(pop)

/*
 * The MeshAccessConnection is used by the MeshAccessModule and provides an encryption
 * layer on top of unencrypted BLE connections as this is necessary to transparently
 * connect non-mesh devices such as Smartphones with different operating systems to the mesh.
 */
class MeshAccessConnection
        : public BaseConnection
{
    friend class MeshAccessModule;
private:

    MeshAccessServiceStruct* meshAccessService;
    MeshAccessModule* meshAccessMod;

    bool useCustomKey = false;
    u8 key[16] = {};

    FmKeyId fmKeyId = FmKeyId::ZERO;

    static constexpr u32 MAX_CORRUPTED_MESSAGES = 32;
    u32 amountOfCorruptedMessages = 0;
    bool allowCorruptedEncryptionStart = false;

    u8 sessionEncryptionKey[16] = {};
    u8 sessionDecryptionKey[16] = {};

    u32 encryptionNonce[2] = {};
    u32 decryptionNonce[2] = {};


    bool GenerateSessionKey(const u8* nonce, NodeId centralNodeId, FmKeyId fmKeyId, u8* keyOut);
    void OnCorruptedMessage();

    void LogKeys();

    // If set, the mesh access connection will be closed once GS->appTimerDs 
    // reaches this value. Can e.g. be used to limit connection times to 
    // devices with high battery importance like assets.
    u32 scheduledConnectionRemovalTimeDs = 0;

    u32 anonceMessageHandle = 0;
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
    bool virtualPartnerIdWasOverwritten = false;

    //If this is set to anything other than 0, the connectionState changes will be reported to this node
    NodeId connectionStateSubscriberId = 0;


    MeshAccessConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress, FmKeyId fmKeyId, MeshAccessTunnelType tunnelType, NodeId overwriteVirtualPartnerId = 0);
    virtual ~MeshAccessConnection();
    static BaseConnection* ConnTypeResolver(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8 const * data);

    void SetCustomKey(u8 const * key);

    /*############### Connect ##################*/
    //Returns the unique connection id that was created
    static u32 ConnectAsMaster(FruityHal::BleGapAddr const * address, u16 connIntervalMs, u16 connectionTimeoutSec, FmKeyId fmKeyId, u8 const * customKey, MeshAccessTunnelType tunnelType, NodeId overwriteVirtualId = 0);

    //Will create a connection that collects potential candidates and connects to them
    static u16 SearchAndConnectAsMaster(NetworkId networkId, u32 serialNumberIndex, u16 searchTimeDs, u16 connIntervalMs, u16 connectionTimeoutSec);

    void RegisterForNotifications();

    /*############### Handshake ##################*/
    void StartHandshake(FmKeyId fmKeyId);
    void HandshakeANonce(ConnPacketEncryptCustomStart const * inPacket);
    void OnANonceReceived(ConnPacketEncryptCustomANonce const * inPacket);
    void OnSNonceReceived(ConnPacketEncryptCustomSNonce const * inPacket);
    void OnHandshakeComplete();

    void SendClusterState();
    void NotifyConnectionStateSubscriber(ConnectionState state) const;

    /*############### Encryption ##################*/
    //This will encrypt some data using the current session key with incrementing nonce/counter
    //The data buffer must have enough space to hold the 4 byte MIC at the end
    void EncryptPacket(u8* data, MessageLength dataLength);

    //Decrypts the data in place (dataLength includes MIC) with the session key
    bool DecryptPacket(u8 const * data, u8 * decryptedOut, MessageLength dataLength);


    /*############### Sending ##################*/
    MessageLength ProcessDataBeforeTransmission(u8* message, MessageLength messageLength, MessageLength bufferLength) override final;
    bool SendData(BaseConnectionSendData* sendData, u8 const * data, u32 * messageHandle=nullptr);
    bool SendData(u8 const * data, MessageLength dataLength, bool reliable, u32 * messageHandle=nullptr) override final;
    bool ShouldSendDataToNodeId(NodeId nodeId) const;
    void PacketSuccessfullyQueuedWithSoftdevice(SizedData* sentData) override final;

    /*############### Receiving ##################*/
    void ReceiveDataHandler(BaseConnectionSendData* sendData, u8 const * data) override final;
    void ReceiveMeshAccessMessageHandler(BaseConnectionSendData* sendData, u8 const * data);

    /*############### Handler ##################*/
    void ConnectionSuccessfulHandler(u16 connectionHandle) override final;
    void GATTServiceDiscoveredHandler(FruityHal::BleGattDBDiscoveryEvent &evt) override final;
    void DataSentHandler(const u8* data, MessageLength length, u32 messageHandle) override final;

    void PrintStatus() override final;

    u32 GetAmountOfCorruptedMessaged();

    // Keeps this connection alive for at least timeDs. If no scheduled removal time is set yet, this
    // it setting the scheduled removal time. If a scheduled removal time is already set, this method
    // is only able to increase the time, it never decreases it. This guarantees that different calls
    // to this function are guaranteed that the mesh access connection is alive for as long as they
    // requested.
    void KeepAliveFor(u32 timeDs);

    // Like KeepAliveFor(u32) but only sets a scheduled removal if one has already been set.
    void KeepAliveForIfSet(u32 timeDs);
};

