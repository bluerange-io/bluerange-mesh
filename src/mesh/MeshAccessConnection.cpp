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

#include <MeshAccessConnection.h>
#include <MeshAccessModule.h>
#include <Node.h>
#include <Logger.h>
#include <Utility.h>
#include <GlobalState.h>
#include "ConnectionAllocator.h"

/**
 * The MeshAccessConnection provides access to a node through a connection that is manually encrypted using
 * AES-128 CCM with either the nodeKey, networkKey, userBaseKey or any derived userKey.
 * A special service is provided and a custom encryption handshake is done when setting up the connection.
 * The packets sent over this connection are in standard mesh format but encrypted, the connection will
 * decrypt and assemble split packets before relaying them.
 *
 * Reading and Writing is done using a tx and rx characteristic that are present on the peripheral side.
 * The central must activate notifications on the tx characteristic and can write to the rx characteristic.
 *
 * To establish a connection, the following has to be done:
 *     - Central connects to peripheral
 *     - Central discovers the MeshAccessService of the peripheral with its rx/tx characteristics and the cccd of the tx characteristic
 *     - Central enables notifications on cccd of tx characteristic
 *     - Peripheral will notice the enabled notification and will instantiate a MeshAccessConnection throught the ResolverConnections
 *     - Central starts handshake by requesting a nonce
 *     - Peripheral anwers with ANonce
 *     - Central answers with SNonce in an encrypted packet (enables auto encrypt/decrypt)
 *     - Peripheral checks encrypted packet, sends encrypted HandshakeDone packet and enables auto encrypt/decrypt
 *
 *    Encryption and MIC calculation uses three AES encryptions at the moment to prevent a discovered packet forgery
 *    attack under certain conditions. Future versions of the handshake may employ different encryption
 *
 * @param id
 * @param direction
 */

//Register the resolver for MeshAccessConnections
#ifndef SIM_ENABLED
uint32_t meshAccessConnTypeResolver __attribute__((section(".ConnTypeResolvers"), used)) = (u32)MeshAccessConnection::ConnTypeResolver;
#endif

MeshAccessConnection::MeshAccessConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress, FmKeyId fmKeyId, MeshAccessTunnelType tunnelType, NodeId overwriteVirtualPartnerId)
    : BaseConnection(id, direction, partnerAddress)
{
    logt("MACONN", "New MeshAccessConnection");

    //Save correct connectionType
    this->connectionType = ConnectionType::MESH_ACCESS;
    this->fmKeyId = fmKeyId;
    CheckedMemset(this->key, 0x00, 16);

    if(direction != ConnectionDirection::DIRECTION_OUT){
        this->handshakeStartedDs = GS->appTimerDs;
    }

    this->tunnelType = tunnelType;

    if (overwriteVirtualPartnerId == 0)
    {
        //The partner is assigned a unique nodeId in our mesh network that is not already taken
        //This is only possible if less than NODE_ID_VIRTUAL_BASE nodes are in the network and if
        //the enrollment ensures that successive nodeIds are used
        this->virtualPartnerId = GS->node.configuration.nodeId + (this->connectionId + 1) * NODE_ID_VIRTUAL_BASE;
        this->virtualPartnerIdWasOverwritten = false;
    }
    else
    {
        //Alternatively the virtualPartnerId can be given as an argument. This is useful e.g.
        //when the meshGw wants to communicate to a device that has an organization wide unique
        //node Id (e.g. an asset).
        this->virtualPartnerId = overwriteVirtualPartnerId;
        this->virtualPartnerIdWasOverwritten = true;
    }

    //Fetch the MeshAccessModule reference
    this->meshAccessMod = (MeshAccessModule*)GS->node.GetModuleById(ModuleId::MESH_ACCESS_MODULE);
    if(meshAccessMod != nullptr){
        this->meshAccessService = &meshAccessMod->meshAccessService;
    } else {
        this->meshAccessService = nullptr;
    }
}

//Can be used to use a custom key for connecting to a partner,
//should be called directly after constructing and before connecting
//Will not work if the partner starts the encryption handshake
void MeshAccessConnection::SetCustomKey(u8 const * key)
{
    CheckedMemcpy(this->key, key, 16);
    useCustomKey = true;
}

MeshAccessConnection::~MeshAccessConnection(){
    NotifyConnectionStateSubscriber(ConnectionState::DISCONNECTED); //Make sure subscribers are informed about a removed connection.
}

BaseConnection* MeshAccessConnection::ConnTypeResolver(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8 const * data)
{
    //Check if data was written to our service rx characteristic
    MeshAccessModule* meshAccessMod = (MeshAccessModule*)GS->node.GetModuleById(ModuleId::MESH_ACCESS_MODULE);
    if(meshAccessMod != nullptr){
        if(
            sendData->characteristicHandle == meshAccessMod->meshAccessService.rxCharacteristicHandle.valueHandle
            || sendData->characteristicHandle == meshAccessMod->meshAccessService.txCharacteristicHandle.cccdHandle
        ){
            return ConnectionAllocator::GetInstance().AllocateMeshAccessConnection(
                    oldConnection->connectionId,
                    oldConnection->direction,
                    &oldConnection->partnerAddress,
                    FmKeyId::ZERO, //fmKeyId unknown at this point, partner must query
                    MeshAccessTunnelType::INVALID, //TunnelType also unknown
                    0 //We don't want to overwrite the virtual nodeId for incomming connections.
                ); 
        }
    }

    return nullptr;
}


#define ________________________CONNECTION_________________________

u32 MeshAccessConnection::ConnectAsMaster(FruityHal::BleGapAddr const * address, u16 connIntervalMs, u16 connectionTimeoutSec, FmKeyId fmKeyId, u8 const * customKey, MeshAccessTunnelType tunnelType, NodeId overwriteVirtualId)
{
    //Only connect when not currently in another connection and when there are free connections
    if (GS->cm.pendingConnection != nullptr) return 0;

    //Check if we already have a MeshAccessConnection to this address and do not allow a second
    MeshAccessConnections conns = GS->cm.GetMeshAccessConnections(ConnectionDirection::INVALID);
    for(u32 i=0; i<conns.count; i++)
    {
        MeshAccessConnectionHandle conn = conns.handles[i];
        if (conn) {
            FruityHal::BleGapAddr partnerAddress = conn.GetPartnerAddress();
            u32 result = memcmp(&partnerAddress, address, FH_BLE_SIZEOF_GAP_ADDR);
            if (result == 0) {
                //TODO wouldn't it be better to return conn->uniqueConnectionId instead?
                //Probably the callee does not care if a real new connection was established,
                //he just wants to have a connection to the specified partner.
                return 0;
            }
        }
    }

    //Create the connection and set it as pending, this is done before starting the GAP connect to avoid race conditions
    for (u32 i = 0; i < TOTAL_NUM_CONNECTIONS; i++){
        if (GS->cm.allConnections[i] == nullptr){
            MeshAccessConnection* conn = ConnectionAllocator::GetInstance().AllocateMeshAccessConnection(i, ConnectionDirection::DIRECTION_OUT, address, fmKeyId, tunnelType, overwriteVirtualId);
            GS->cm.pendingConnection = GS->cm.allConnections[i] = conn;

            //Set the timeout big enough so that it is not killed by the ConnectionManager
            conn->handshakeStartedDs = GS->appTimerDs + SEC_TO_DS(connectionTimeoutSec + 2);

            //If customKey is not nullptr and not set to FF:FF...., we use it
            if(customKey != nullptr && !Utility::CompareMem(0xFF, customKey, 16)){
                ((MeshAccessConnection*)GS->cm.pendingConnection)->SetCustomKey(customKey);
            }
            break;
        }
    }
    if(GS->cm.pendingConnection == nullptr){
        logt("MACONN", "No free connection");
        return 0;
    }

    //Tell the GAP Layer to connect, it will return if it is trying or if there was an error
    ErrorType err = GS->gapController.ConnectToPeripheral(*address, MSEC_TO_UNITS(connIntervalMs, CONFIG_UNIT_1_25_MS), connectionTimeoutSec);

    if (err == ErrorType::SUCCESS)
    {
        logt("MACONN", "Trying to connect");
        return GS->cm.pendingConnection->uniqueConnectionId;
    } else {
        //Clean the connection that has just been created
        GS->cm.DeleteConnection(GS->cm.pendingConnection, AppDisconnectReason::GAP_ERROR);
    }

    return 0;
}


#define ________________________HANDSHAKE_________________________

//Discovery example: https://devzone.nordicsemi.com/question/119274/sd_ble_gattc_characteristics_discover-only-discovers-one-characteristic/

// The Central must register for notifications on the tx characteristic of the peripheral
void MeshAccessConnection::RegisterForNotifications()
{
    logt("MACONN", "Registering for notifications");

    u16 data = 0x0001; //Bit to enable the notifications

    ErrorType err = GS->gattController.BleWriteCharacteristic(connectionHandle, partnerTxCharacteristicCccdHandle, (u8*)&data, sizeof(data), true);
    if(err == ErrorType::SUCCESS){
        manualPacketsSent++;
    }

    //After the write REQ for enabling notifications was queued, we can safely send data
    StartHandshake(fmKeyId);
}

//This method is called by the Central and will start the encryption handshake
void MeshAccessConnection::StartHandshake(FmKeyId fmKeyId)
{
    if(connectionState >= ConnectionState::HANDSHAKING) return;

    logt("MACONN", "-- TX Start Handshake");

    //Save the fmKeyId that we want to use
    this->fmKeyId = fmKeyId;

    connectionState = ConnectionState::HANDSHAKING;
    handshakeStartedDs = GS->appTimerDs; //Refresh handshake timer
    //C=>P: Type=RequestANuonce, fmKeyId=#,Authorize(true/false), Authenticate(true/false)

    ConnPacketEncryptCustomStart packet;
    CheckedMemset(&packet, 0x00, sizeof(ConnPacketEncryptCustomStart));
    packet.header.messageType = MessageType::ENCRYPT_CUSTOM_START;
    packet.header.sender = GS->node.configuration.nodeId;
    packet.header.receiver = virtualPartnerId;
    packet.version = 1;
    packet.fmKeyId = fmKeyId;
    packet.tunnelType = (u8)tunnelType;

    SendData(
        (u8*)&packet,
        SIZEOF_CONN_PACKET_ENCRYPT_CUSTOM_START,
        false);
}

//This method is called by the peripheral after the Encryption Start Handshake packet was received
void MeshAccessConnection::HandshakeANonce(ConnPacketEncryptCustomStart const * inPacket){
    //Process Starthandshake packet
    //P=>C: Type=ANouce (Will stay the same random number until attempt was made), supportedKeyIds=1,2,345,56,...,supportsAuthenticate(true/false)

    logt("MACONN", "-- TX ANonce, fmKeyId %u", (u32)inPacket->fmKeyId);

    connectionState = ConnectionState::HANDSHAKING;

    //C=>P: Type=RequestANuonce, fmKeyId=#,Authorize(true/false), Authenticate(true/false)

    //We do not want to accept certain key types
    fmKeyId = inPacket->fmKeyId;
    partnerId = inPacket->header.sender;

    if (partnerId == NODE_ID_BROADCAST){
        logt("ERROR", "Wrong partnerId");
        DisconnectAndRemove(AppDisconnectReason::WRONG_PARTNERID);
        return;
    }

    //The tunnel type is the opposite of the partners tunnel type
    if(inPacket->tunnelType == (u8)MeshAccessTunnelType::PEER_TO_PEER){
        tunnelType = MeshAccessTunnelType::PEER_TO_PEER;
    } else if(inPacket->tunnelType == (u8)MeshAccessTunnelType::LOCAL_MESH){
        tunnelType = MeshAccessTunnelType::REMOTE_MESH;
    } else if(inPacket->tunnelType == (u8)MeshAccessTunnelType::REMOTE_MESH){
        tunnelType = MeshAccessTunnelType::LOCAL_MESH;
    } else {
        logt("ERROR", "Illegal TunnelType %u", (u32)inPacket->tunnelType);
        DisconnectAndRemove(AppDisconnectReason::ILLEGAL_TUNNELTYPE);
        return;
    }

    ConnPacketEncryptCustomANonce packet;
    CheckedMemset(&packet, 0x00, sizeof(ConnPacketEncryptCustomANonce));
    packet.header.messageType = MessageType::ENCRYPT_CUSTOM_ANONCE;
    packet.header.sender = GS->node.configuration.nodeId;
    packet.header.receiver = virtualPartnerId;

    decryptionNonce[0] = packet.anonce[0] = Utility::GetRandomInteger();
    decryptionNonce[1] = packet.anonce[1] = Utility::GetRandomInteger();

    //Generate the session key for decryption
    bool keyValid = GenerateSessionKey((u8*)decryptionNonce, partnerId, fmKeyId, sessionDecryptionKey);

    if(!keyValid){
        logt("WARNING", "Invalid Key"); //See: IOT-3821
        DisconnectAndRemove(AppDisconnectReason::INVALID_KEY);
        return;
    }

    SendData(
        (u8*)&packet,
        SIZEOF_CONN_PACKET_ENCRYPT_CUSTOM_ANONCE,
        false,
        &anonceMessageHandle);
}

//This method is called by the Central after the ANonce was received
void MeshAccessConnection::OnANonceReceived(ConnPacketEncryptCustomANonce const * inPacket)
{
    logt("MACONN", "-- TX SNonce, anonce %u", inPacket->anonce[1]);

    // Process Handshake ANonce
    // C=>P: EncS(StartEncryptCustom, SNonce),MIC

    partnerId = inPacket->header.sender;

    //Save the partners nonce for use as encryption nonce
    encryptionNonce[0] = inPacket->anonce[0];
    encryptionNonce[1] = inPacket->anonce[1];

    //Send an encrypted packet containing the sNonce
    const u8 len = SIZEOF_CONN_PACKET_ENCRYPT_CUSTOM_SNONCE + MESH_ACCESS_MIC_LENGTH;
    u8 buffer[len];
    CheckedMemset(buffer, 0x00, len);
    ConnPacketEncryptCustomSNonce* packet = (ConnPacketEncryptCustomSNonce*)buffer;
    packet->header.messageType = MessageType::ENCRYPT_CUSTOM_SNONCE;
    packet->header.sender = GS->node.configuration.nodeId;
    packet->header.receiver = virtualPartnerId;

    //Save self-generated nonce to decrypt packets
    decryptionNonce[0] = packet->snonce[0] = Utility::GetRandomInteger();
    decryptionNonce[1] = packet->snonce[1] = Utility::GetRandomInteger();

    //Generate the session keys for encryption and decryption
    bool keyValidA = GenerateSessionKey((u8*)encryptionNonce, GS->node.configuration.nodeId, fmKeyId, sessionEncryptionKey);
    bool keyValidB = GenerateSessionKey((u8*)decryptionNonce, GS->node.configuration.nodeId, fmKeyId, sessionDecryptionKey);

    if(!keyValidA || !keyValidB){
        logt("ERROR", "Invalid Key %u %u", (u32)keyValidA, (u32)keyValidB);
        DisconnectAndRemove(AppDisconnectReason::INVALID_KEY);
        return;
    }

    LogKeys();

    //Pay attention that we must only increment the encryption counter once the
    //message is placed in the SoftDevice, otherwise we will break the message flow


    //Set encryption state to encrypted because we await the next packet to be encrypted, our next one is as well
    encryptionState = EncryptionState::ENCRYPTED;

    SendData(
        (u8*)packet,
        SIZEOF_CONN_PACKET_ENCRYPT_CUSTOM_SNONCE,
        false);

    connectionState = ConnectionState::HANDSHAKE_DONE;

    //Needed by our packet splitting methods, payload is now less than before because of MIC
    connectionPayloadSize = connectionMtu - MESH_ACCESS_MIC_LENGTH;

    //Send the current mesh state to our partner
    SendClusterState();

    NotifyConnectionStateSubscriber(ConnectionState::HANDSHAKE_DONE);

    logt("MACONN", "Handshake done as Central");

    OnHandshakeComplete();
}

//This method is called by the Peripheral after the SNonce was received
void MeshAccessConnection::OnSNonceReceived(ConnPacketEncryptCustomSNonce const * inPacket)
{
    logt("MACONN", "-- TX Handshake Done, snonce %u", encryptionNonce[1]);

    // Process Handshake SNonce
    // P=>C: EncS(EncryptionSuccessful)+MIC

    //Save nonce to encrypt packets for partner
    encryptionNonce[0] = inPacket->snonce[0];
    encryptionNonce[1] = inPacket->snonce[1];

    //Generate key for encryption
    bool keyValid = GenerateSessionKey((u8*)encryptionNonce, partnerId, fmKeyId, sessionEncryptionKey);

    if(!keyValid){
        logt("ERROR", "Invalid Key in HD");
        DisconnectAndRemove(AppDisconnectReason::INVALID_KEY);
        return;
    }

    LogKeys();

    //Send an encrypted packet to say that we are done
    ConnPacketEncryptCustomDone packet;
    CheckedMemset(&packet, 0x00, sizeof(ConnPacketEncryptCustomDone));
    packet.header.messageType = MessageType::ENCRYPT_CUSTOM_DONE;
    packet.header.sender = GS->node.configuration.nodeId;
    packet.header.receiver = virtualPartnerId;
    packet.status = (u8)ErrorType::SUCCESS;

    //From now on, we can just send data the normal way and the encryption is done automatically
    SendData(
        (u8*)&packet,
        SIZEOF_CONN_PACKET_ENCRYPT_CUSTOM_DONE,
        false);

    connectionState = ConnectionState::HANDSHAKE_DONE;
    amountOfCorruptedMessages = 0;
    allowCorruptedEncryptionStart = false;

    //Needed by our packet splitting methods, payload is now less than before because of MIC
    connectionPayloadSize = connectionMtu - MESH_ACCESS_MIC_LENGTH;

    //Send the current mesh state to our partner
    SendClusterState();

    NotifyConnectionStateSubscriber(ConnectionState::HANDSHAKE_DONE);

    logt("MACONN", "Handshake done as Peripheral");

    OnHandshakeComplete();
}

//This method is called by both the Peripheral and the Central after the connectionState was set to HANDSHAKE_DONE for the first time.
void MeshAccessConnection::OnHandshakeComplete()
{
    if (GS->timeManager.IsTimeCorrected())
    {
        TimeSyncInterNetwork tsin = GS->timeManager.GetTimeSyncInterNetworkMessage(virtualPartnerId);
        SendData((u8*)&tsin, sizeof(tsin), false);
    }
}

void MeshAccessConnection::SendClusterState()
{
    ConnPacketClusterInfoUpdate packet;
    CheckedMemset(&packet, 0, sizeof(ConnPacketClusterInfoUpdate));
    packet.header.messageType = MessageType::CLUSTER_INFO_UPDATE;
    packet.header.sender = GS->node.configuration.nodeId;
    packet.header.receiver = NODE_ID_BROADCAST;

    packet.payload.clusterSizeChange = GS->node.GetClusterSize();
    packet.payload.connectionMasterBitHandover = GS->node.HasAllMasterBits();
    packet.payload.hopsToSink = GS->cm.GetMeshHopsToShortestSink(nullptr);

    SendData((u8*)&packet, sizeof(ConnPacketClusterInfoUpdate), false);
}

void MeshAccessConnection::NotifyConnectionStateSubscriber(ConnectionState state) const
{
    if(connectionStateSubscriberId != 0)
    {
        MeshAccessModuleConnectionStateMessage data;
        data.vPartnerId = virtualPartnerId;
        data.state = (u8)state;

        GS->cm.SendModuleActionMessage(
            MessageType::MODULE_GENERAL,
            ModuleId::MESH_ACCESS_MODULE,
            connectionStateSubscriberId,
            (u8)MeshAccessModuleGeneralMessages::MA_CONNECTION_STATE,
            0, //TODO: maybe store the request handle and send it back here?
            (u8*)&data,
            SIZEOF_MA_MODULE_CONNECTION_STATE_MESSAGE,
            false,
            true
        );
    }

}

#define ________________________ENCRYPTION_________________________

//Session Key S generated as Enc#(Anonce, nodeIndex); Enc# is the chosen key

bool MeshAccessConnection::GenerateSessionKey(const u8* nonce, NodeId centralNodeId, FmKeyId fmKeyId, u8* keyOut)
{
    u8 ltKey[16];

    if(useCustomKey){
        logt("MACONN", "Using custom key");
        CheckedMemcpy(ltKey, key, 16);
    } else if(fmKeyId == FmKeyId::ZERO
            && meshAccessMod->IsZeroKeyConnectable(direction)) {
        //If the fmKeyId is FmKeyId::ZERO and we allow unsecure connections, we use
        //the zero encryption key (basically no encryption) if we are not enrolled or 
        //we are the one opening the connection.
        logt("MACONN", "Using key none");
        CheckedMemset(ltKey, 0x00, 16);
    } else if(fmKeyId == FmKeyId::NODE) {
        logt("MACONN", "Using node key");
        CheckedMemcpy(ltKey, RamConfig->GetNodeKey(), 16);
    } else if(fmKeyId == FmKeyId::NETWORK) {
        logt("MACONN", "Using network key");
        CheckedMemcpy(ltKey, GS->node.configuration.networkKey, 16);
    } else if(fmKeyId == FmKeyId::ORGANIZATION) {
        logt("MACONN", "Using orga key");
        CheckedMemcpy(ltKey, GS->node.configuration.organizationKey, 16);
    } else if (fmKeyId == FmKeyId::RESTRAINED) {
        logt("MACONN", "Using restrained key");
        RamConfig->GetRestrainedKey(ltKey);
    }
    else if(fmKeyId >= FmKeyId::USER_DERIVED_START && fmKeyId <= FmKeyId::USER_DERIVED_END)
    {
        logt("MACONN", "Using derived user key %u", (u32)fmKeyId);
        //Construct some cleartext with the user id to construct the user key
        u8 cleartext[16];
        CheckedMemset(cleartext, 0x00, 16);
        CheckedMemcpy(cleartext, &fmKeyId, 4);


        Utility::Aes128BlockEncrypt(
                (Aes128Block*)cleartext,
                (Aes128Block*)GS->node.configuration.userBaseKey,
                (Aes128Block*)ltKey);

    }
    else {
        logt("MACONN", "Invalid key generated");
        //No key
        CheckedMemset(keyOut, 0x00, 16);
        return false;
    }

     //TO_HEX(ltKey, 16);
     //logt("MACONN", "LongTerm Key is %s", ltKeyHex);

    //Check if Long Term Key is empty
    if(Utility::CompareMem(0xFF, ltKey, 16)){
        logt("ERROR", "Key was empty, can not be used");
        return false;
    }

    //Generate cleartext with NodeId and ANonce
    u8 cleartext[16];
    CheckedMemset(cleartext, 0x00, 16);
    CheckedMemcpy(cleartext, (u8*)&centralNodeId, 2);
    CheckedMemcpy(((u8*)cleartext) + 2, nonce, MESH_ACCESS_HANDSHAKE_NONCE_LENGTH);

    //TO_HEX(cleartext, 16);
    //logt("MACONN", "SessionKeyCleartext %s", cleartextHex);

    //Encrypt with our chosen Long Term Key
    Utility::Aes128BlockEncrypt(
            (Aes128Block*)cleartext,
            (Aes128Block*)ltKey,
            (Aes128Block*)keyOut);

    return true;
}

void MeshAccessConnection::OnCorruptedMessage()
{
    amountOfCorruptedMessages++;
    //If this is the first corrupted message in the current handshake cycle.
    if (amountOfCorruptedMessages == 1)
    {
        encryptionState = EncryptionState::NOT_ENCRYPTED;
        connectionState = ConnectionState::CONNECTED;
        this->handshakeStartedDs = GS->appTimerDs + SEC_TO_DS(10);
        allowCorruptedEncryptionStart = true; //The connection partner might have still some packets queued, thus we
                                              //have to allow (but drop) other packets than the encryption start.
    }

    DeadDataMessage msg;
    CheckedMemset(&msg, 0, sizeof(msg));
    msg.header.messageType = MessageType::DEAD_DATA;
    msg.header.sender      = GS->node.configuration.nodeId;
    msg.header.receiver    = this->virtualPartnerId;
    CheckedMemcpy(msg.magicNumber, deadDataMagicNumber, sizeof(deadDataMagicNumber));

    SendData((u8*)&msg, sizeof(DeadDataMessage), false);

    if (amountOfCorruptedMessages >= MAX_CORRUPTED_MESSAGES)
    {
        DisconnectAndRemove(AppDisconnectReason::INVALID_PACKET);
    }
}

void MeshAccessConnection::LogKeys()
{
    //Log encryption and decryption keys
    TO_HEX(sessionEncryptionKey, 16);
    TO_HEX(sessionDecryptionKey, 16);
    logt("MACONN", "EncrKey: %s", sessionEncryptionKeyHex);
    logt("MACONN", "DecrKey: %s", sessionDecryptionKeyHex);
}

/**
 * Encryption is done using a counter chaining mode with AES.
 * The nonce/counter + padding is encrypted with the session key to generate a keystream. This keystream is
 * then xored with the cleartext to produce a ciphertext of variable length.
 * To calculate the MIC, the nonce/counter is incremented, then it is xored with the ciphertext of the message
 * before being encrypted with the session key. The first bytes of this nonce+message ciphertext are then
 * used as the MIC which is appended to the end of the data
 *
 * @param data[in/out] must be big enough to hold the additional bytes for the MIC which is placed at the end
 * @param dataLength[in]
 */
void MeshAccessConnection::EncryptPacket(u8* data, MessageLength dataLength)
{
    TO_HEX(data, dataLength.GetRaw());
    logt("MACONN", "Encrypting %s (%u) with nonce %u", dataHex, dataLength.GetRaw(), encryptionNonce[1]);

    u8 cleartext[16];
    u8 keystream[16];
    u8 ciphertext[16];

    //Generate keystream with nonce
    CheckedMemset(cleartext, 0x00, 16);
    CheckedMemcpy(cleartext, encryptionNonce, MESH_ACCESS_HANDSHAKE_NONCE_LENGTH);

    //TO_HEX(cleartext, 16);
    //logt("MACONN", "Encryption Keystream cleartext %s", cleartextHex);

    Utility::Aes128BlockEncrypt(
            (Aes128Block*)cleartext,
            (Aes128Block*)sessionEncryptionKey,
            (Aes128Block*)keystream);

    //TO_HEX(keystream, 16);
    //logt("MACONN", "Encryption Keystream %s", keystreamHex);

    //Xor cleartext with keystream to get the ciphertext
    CheckedMemcpy(cleartext, data, dataLength.GetRaw());
    Utility::XorBytes(keystream, cleartext, 16, ciphertext);
    CheckedMemcpy(data, ciphertext, dataLength.GetRaw());

    //Increment nonce being used as a counter
    encryptionNonce[1]++;

    //Generate a new Keystream with an updated counter for MIC calculateion
    CheckedMemset(cleartext, 0x00, 16);
    CheckedMemcpy(cleartext, encryptionNonce, MESH_ACCESS_HANDSHAKE_NONCE_LENGTH);

    //TO_HEX_2(cleartext, 16);
    //logt("MACONN", "Encryption Keystream 2 cleartext %s", cleartextHex);

    Utility::Aes128BlockEncrypt( //encrypts nonce
                (Aes128Block*)cleartext,
                (Aes128Block*)sessionEncryptionKey,
                (Aes128Block*)keystream);


    //TO_HEX_2(keystream, 16);
    //logt("MACONN", "Encryption Keystream 2 %s", keystreamHex);

    //To generate the MIC, we xor the new keystream with our cleartext and encrypt it again
    //we therefore create a pair that cannot be reproduced by an attacker (hopefully :-))
    CheckedMemset(cleartext, 0x00, 16);
    CheckedMemcpy(cleartext, data, dataLength.GetRaw());
    Utility::XorBytes(keystream, cleartext, 16, cleartext);
    Utility::Aes128BlockEncrypt(
            (Aes128Block*)cleartext,
            (Aes128Block*)sessionEncryptionKey,
            (Aes128Block*)keystream);

    //Log the keystream generated by the nonce, 4 bytes of keystream are used as MIC
    //u8 keystream2[16];
    //CheckedMemcpy(keystream2, keystream, 16);
    //TO_HEX(keystream2, 16);
    //logt("MACONN", "MIC nonce %u produces Keystream %s", encryptionNonce[1], keystream2Hex);

    //Reset nonce, it is incremented once the packet was successfully queued with the softdevice
    encryptionNonce[1]--;

    //Copy nonce to the end of the packet
    u8* micPtr = data + dataLength;
    CheckedMemcpy(micPtr, keystream, MESH_ACCESS_MIC_LENGTH);

    //Log the encrypted packet
    DYNAMIC_ARRAY(data2, dataLength.GetRaw() + MESH_ACCESS_MIC_LENGTH);
    CheckedMemcpy(data2, data, dataLength.GetRaw() + MESH_ACCESS_MIC_LENGTH);
    TO_HEX(data2, dataLength.GetRaw() + MESH_ACCESS_MIC_LENGTH);
    logt("MACONN", "Encrypted as %s (%u)", data2Hex, dataLength.GetRaw() + MESH_ACCESS_MIC_LENGTH);
}

bool MeshAccessConnection::DecryptPacket(u8 const * data, u8 * decryptedOut, MessageLength dataLength)
{
    if(dataLength < 4) return false;

    TO_HEX(data, dataLength.GetRaw());
    logt("MACONN", "Decrypting %s (%u) with nonce %u", dataHex, dataLength.GetRaw(), decryptionNonce[1]);

    u8 cleartext[16];
    u8 keystream[16];
    u8 ciphertext[16];

    //We need to calculate the MIC from the ciphertext as was done by the sender
    decryptionNonce[1]++;

    //Generate a keystream from the nonce
    CheckedMemset(cleartext, 0x00, 16);
    CheckedMemcpy(cleartext, decryptionNonce, MESH_ACCESS_HANDSHAKE_NONCE_LENGTH);
    Utility::Aes128BlockEncrypt( //encrypts nonce
                (Aes128Block*)cleartext,
                (Aes128Block*)sessionDecryptionKey,
                (Aes128Block*)keystream);

    //Xor the keystream with the ciphertext
    CheckedMemset(ciphertext, 0x00, 16);
    CheckedMemcpy(ciphertext, data, dataLength.GetRaw() - MESH_ACCESS_MIC_LENGTH);
    Utility::XorBytes(ciphertext, keystream, 16, cleartext);
    //Encrypt the resulting cleartext
    Utility::Aes128BlockEncrypt(
            (Aes128Block*)cleartext,
            (Aes128Block*)sessionDecryptionKey,
            (Aes128Block*)keystream);

    //Check if the two MICs match
    u8 const * micPtr = data + (dataLength - MESH_ACCESS_MIC_LENGTH);
    u32 micCheck = memcmp(keystream, micPtr, MESH_ACCESS_MIC_LENGTH);

    //Reset decryptionNonce for decrypting the message
    decryptionNonce[1]--;

    //Generate keystream with nonce
    CheckedMemset(cleartext, 0x00, 16);
    CheckedMemcpy(cleartext, decryptionNonce, MESH_ACCESS_HANDSHAKE_NONCE_LENGTH);
    Utility::Aes128BlockEncrypt(
            (Aes128Block*)cleartext,
            (Aes128Block*)sessionDecryptionKey,
            (Aes128Block*)keystream);

    //TO_HEX(keystream, 16);
    //logt("MACONN", "Keystream %s", keystreamHex);

    //Xor keystream with ciphertext to retrieve original message
    Utility::XorBytes(keystream, data, dataLength.GetRaw() - MESH_ACCESS_MIC_LENGTH, decryptedOut);

    //Increment nonce being used as a counter
    decryptionNonce[1] += 2;

    //u8 keystream2[16];
    //CheckedMemcpy(keystream2, keystream, 16);
    //TO_HEX(keystream2, 16);
    //logt("MACONN", "MIC nonce %u, Keystream %s", decryptionNonce[1], keystream2Hex);


    TO_HEX_2(data, dataLength.GetRaw() - MESH_ACCESS_MIC_LENGTH);
    logt("MACONN", "Decrypted as %s (%u) micValid %u", dataHex, dataLength.GetRaw() - MESH_ACCESS_MIC_LENGTH, micCheck == 0);

    return micCheck == 0;
}


#define ________________________SEND________________________

//This function might modify the packet, can also split bigger packets
MessageLength MeshAccessConnection::ProcessDataBeforeTransmission(u8* message, MessageLength messageLength, MessageLength bufferLength)
{
    //Encrypt packets after splitting if necessary
    if(encryptionState == EncryptionState::ENCRYPTED){
        const u16 processedLength = messageLength.GetRaw() + MESH_ACCESS_MIC_LENGTH;
        if (processedLength > bufferLength)
        {
            SIMEXCEPTION(IllegalStateException);
            GS->logger.LogCustomError(CustomErrorTypes::FATAL_ILLEGAL_PROCCESS_BUFFER_LENGTH, bufferLength.GetRaw());
            logt("ERROR", "!!!FATAL!!! Illegal Process Buffer Length!");
            return 0;
        }
        //We use the given packetBuffer to store the encrypted packet + its MIC
        DYNAMIC_ARRAY(packetBuffer, bufferLength.GetRaw());
        CheckedMemcpy(packetBuffer, message, messageLength.GetRaw());
        EncryptPacket(packetBuffer, messageLength);
        CheckedMemcpy(message, packetBuffer, processedLength)
        return processedLength;
    }

    return messageLength;
}

bool MeshAccessConnection::ShouldSendDataToNodeId(NodeId nodeId) const
{
    return
        //The ID matches
        nodeId == virtualPartnerId
        //Broadcasts, by definition always go everywhere
        || nodeId == NODE_ID_BROADCAST
        //NODE_ID_ANYCAST_THEN_BROADCAST is inteded to be sent through MeshAccessConnections
        || nodeId == NODE_ID_ANYCAST_THEN_BROADCAST
        //A given hops count may also go through a MeshAccessConnection
        || (nodeId >= NODE_ID_HOPS_BASE && nodeId < (NODE_ID_HOPS_BASE + NODE_ID_HOPS_BASE_SIZE))
        //The range of APP_BASE nodeIds is reserved for smartphones etc. that always connect via MeshAccessConnections
        || (nodeId >= NODE_ID_APP_BASE && nodeId < (NODE_ID_APP_BASE + NODE_ID_APP_BASE_SIZE))
        //Organization wide NodeIds. These are commonly used for assets that connect via MeshAccessConnections
        || (nodeId >= NODE_ID_GLOBAL_DEVICE_BASE && nodeId < (NODE_ID_GLOBAL_DEVICE_BASE + NODE_ID_GLOBAL_DEVICE_BASE_SIZE))
        //DFU messages are typically sent to group ids. They must be allowed.
        || (nodeId >= NODE_ID_GROUP_BASE && nodeId < (NODE_ID_GROUP_BASE + NODE_ID_GROUP_BASE_SIZE))
        //Connections using the network key with tunnel type REMOTE_MESH are allowed to send messages to the remote node.
        || (fmKeyId == FmKeyId::NETWORK && tunnelType == MeshAccessTunnelType::REMOTE_MESH && direction == ConnectionDirection::DIRECTION_OUT);
}


bool MeshAccessConnection::SendData(u8 const * data, MessageLength dataLength, bool reliable, u32 * messageHandle)
{
    if (dataLength > MAX_MESH_PACKET_SIZE) {
        SIMEXCEPTION(PacketTooBigException);
        logt("ERROR", "Packet too big for sending!");
        return false;
    }

    if(meshAccessService == nullptr) return false;

    BaseConnectionSendData sendData;

    if(direction == ConnectionDirection::DIRECTION_OUT)
    {
        //The central can write the data to the rx characteristic of the peripheral
        sendData.characteristicHandle = partnerRxCharacteristicHandle;
        sendData.dataLength = dataLength;
        sendData.deliveryOption = reliable ? DeliveryOption::WRITE_REQ : DeliveryOption::WRITE_CMD;
    }
    else
    {
        //The peripheral must send data as notifications from its tx characteristic
        sendData.characteristicHandle = meshAccessService->txCharacteristicHandle.valueHandle;
        sendData.dataLength = dataLength;
        sendData.deliveryOption = DeliveryOption::NOTIFICATION;
    }

    return SendData(&sendData, data, messageHandle);
}

//This is the generic method for sending data
bool MeshAccessConnection::SendData(BaseConnectionSendData* sendData, u8 const * data, u32 * messageHandle)
{
    ConnPacketHeader const * packetHeader = (ConnPacketHeader const *)data;

    logt("MACONN", "MA SendData from %u to %u", packetHeader->sender, packetHeader->receiver);

    MeshAccessAuthorization auth = meshAccessMod->CheckAuthorizationForAll(sendData, data, fmKeyId, DataDirection::DIRECTION_OUT);

    //Block other packets as long as handshake is not done
    if(
        connectionState < ConnectionState::HANDSHAKE_DONE
        && (packetHeader->messageType < MessageType::ENCRYPT_CUSTOM_START
        || packetHeader->messageType > MessageType::ENCRYPT_CUSTOM_DONE)
        && packetHeader->messageType != MessageType::DEAD_DATA
    ){
        return false;
    }

    if(packetHeader->receiver == partnerId){
        logt("MACONN", "Potential wrong destination id, please send to virtualPartnerId");
    }

    //Only allow packets to the virtual partner Id or broadcast
    if(
        tunnelType == MeshAccessTunnelType::PEER_TO_PEER
        || tunnelType == MeshAccessTunnelType::LOCAL_MESH
        || tunnelType == MeshAccessTunnelType::REMOTE_MESH
    ){
        //Do not send packets address to nodes in our mesh, only broadcast or packets addressed to its virtual id
        if(
            packetHeader->receiver > NODE_ID_DEVICE_BASE
            && packetHeader->receiver < NODE_ID_GROUP_BASE
            && packetHeader->receiver != this->virtualPartnerId
            && tunnelType != MeshAccessTunnelType::REMOTE_MESH)
        {
            logt("MACONN", "Not sending");
            return false;
        }

        //Before sending it to our partner, we change the virtual receiver id
        //that was used in our mesh to his normal nodeId
        DYNAMIC_ARRAY(modifiedData, sendData->dataLength.GetRaw());
        if(packetHeader->receiver == this->virtualPartnerId){
            CheckedMemcpy(modifiedData, data, sendData->dataLength.GetRaw());
            ConnPacketHeader * modifiedPacketHeader = (ConnPacketHeader*)modifiedData;
            modifiedPacketHeader->receiver = this->partnerId;
            data = modifiedData;
        }

        //Put packet in the queue for sending
        if(auth != MeshAccessAuthorization::UNDETERMINED && auth != MeshAccessAuthorization::BLACKLIST){
            return QueueData(*sendData, data, messageHandle);
        } else {
            return false;
        }
    //We must allow handshake packets
    } else if (packetHeader->messageType >= MessageType::ENCRYPT_CUSTOM_START && packetHeader->messageType <= MessageType::ENCRYPT_CUSTOM_DONE)
    {
        //Put packet in the queue for sending
        if(auth != MeshAccessAuthorization::UNDETERMINED && auth != MeshAccessAuthorization::BLACKLIST){
            return QueueData(*sendData, data, messageHandle);
        } else {
            return false;
        }
    }

    return false;
}

//Because we are using packet splitting, we must handle packetSendPosition and Discarding here
void MeshAccessConnection::PacketSuccessfullyQueuedWithSoftdevice(SizedData* sentData)
{
    //The queued packet might be encrypted, so we must rely on the saved messageType that is saved
    //by the ProcessDataBeforeTransmission method

    if(encryptionState == EncryptionState::ENCRYPTED){
        encryptionNonce[1] += 2;
    }
    HandlePacketQueued();
}

#define ________________________RECEIVE________________________

//Check if encryption was started, and if yes, decrypt all packets before passing them to
//other functions, deal with the handshake packets as well
void MeshAccessConnection::ReceiveDataHandler(BaseConnectionSendData* sendData, u8 const * data)
{
    if(
        meshAccessMod == nullptr
        || meshAccessService == nullptr
        || (direction == ConnectionDirection::DIRECTION_OUT && partnerTxCharacteristicHandle != sendData->characteristicHandle)
        || (direction == ConnectionDirection::DIRECTION_IN && meshAccessService->rxCharacteristicHandle.valueHandle != sendData->characteristicHandle)
    ){
        return;
    }

    //TO_HEX(data, sendData->dataLength);
    //logt("MACONN", "RX DATA %s", dataHex);

    // If the connection is encrypted (on peripheral after successfully sending
    // the ANONCE packet, on the central before sending the SNONCE packet),
    // try to decrypt the data.
    DYNAMIC_ARRAY(decryptedData, sendData->dataLength.GetRaw());
    if(encryptionState == EncryptionState::ENCRYPTED){
        bool valid = DecryptPacket(data, decryptedData, sendData->dataLength);
        sendData->dataLength -= MESH_ACCESS_MIC_LENGTH;
        data = decryptedData;

        if(!valid){
            if(connectionState < ConnectionState::HANDSHAKE_DONE){
                // Failed decryption during the handshake leads to disconnection.
                logt("WARNING", "Invalid packet during handshake");
                DisconnectAndRemove(AppDisconnectReason::INVALID_HANDSHAKE_PACKET);
            }
            else{
                // The first failed decryption on an established connection
                // (e.g. due to a dropped packet) leads to reset of the
                // encryption and connection state (see OnCorruptedMessage).
                logt("WARNING", "Invalid packet");
                OnCorruptedMessage();
            }
            return;
        }
    }

    ConnPacketHeader const * packetHeader = (ConnPacketHeader const *)data;

    if(connectionState == ConnectionState::CONNECTED)
    {
        if(sendData->dataLength >= SIZEOF_CONN_PACKET_ENCRYPT_CUSTOM_START && packetHeader->messageType == MessageType::ENCRYPT_CUSTOM_START){
            HandshakeANonce((ConnPacketEncryptCustomStart const *) data);
        } else if(!allowCorruptedEncryptionStart) {
            logt("ERROR", "Wrong handshake packet");
            DisconnectAndRemove(AppDisconnectReason::INVALID_HANDSHAKE_PACKET);
        }
    }
    else if(connectionState == ConnectionState::HANDSHAKING)
    {
        if(sendData->dataLength >= SIZEOF_CONN_PACKET_ENCRYPT_CUSTOM_ANONCE && packetHeader->messageType == MessageType::ENCRYPT_CUSTOM_ANONCE){
            OnANonceReceived((ConnPacketEncryptCustomANonce const *) data);
        }
        else if(sendData->dataLength >= SIZEOF_CONN_PACKET_ENCRYPT_CUSTOM_SNONCE && packetHeader->messageType == MessageType::ENCRYPT_CUSTOM_SNONCE){
            OnSNonceReceived((ConnPacketEncryptCustomSNonce const *) data);
        } else {
            logt("ERROR", "Wrong handshake packet");
            DisconnectAndRemove(AppDisconnectReason::INVALID_HANDSHAKE_PACKET);
        }
    }
    else if(connectionState == ConnectionState::HANDSHAKE_DONE){
        GS->logger.LogCustomCount(CustomErrorTypes::COUNT_RECEIVED_SPLIT_OVER_MESH_ACCESS);

        //This will reassemble the data for us
        data = ReassembleData(sendData, data);

        //If the data is null, the packet has not been fully reassembled
        if(data != nullptr){
            //Call our message received handler
            ReceiveMeshAccessMessageHandler(sendData, data);
        }
    }
}

void MeshAccessConnection::ReceiveMeshAccessMessageHandler(BaseConnectionSendData* sendData, u8 const * data)
{
    //We must change the sender because our partner might have a nodeId clash within our network
    ConnPacketHeader const * packetHeader = (ConnPacketHeader const *)data;

    //Some special handling for timestamp updates
    GS->timeManager.HandleUpdateTimestampMessages(packetHeader, sendData->dataLength);

    // Replenish scheduled removal time of mesh access connection through which DFU messages passed.
    // This way we don't remove connections if we are in the middle of a DFU.
    if (sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE
        &&
        (
            packetHeader->messageType == MessageType::MODULE_ACTION_RESPONSE
            || packetHeader->messageType == MessageType::MODULE_TRIGGER_ACTION
            )
        )
    {
        ConnPacketModule const* mod = (ConnPacketModule const*)packetHeader;
        if (mod->moduleId == ModuleId::DFU_MODULE)
        {
            logt("MAMOD", "Received DFU message, replenishing scheduled removal time.");
            KeepAliveForIfSet(MeshAccessModule::meshAccessDfuSurvivalTimeDs);
        }
    }

    //For component_sense message sent from asset, the virtual partner id should not change
    bool replaceSenderId = true;
    if (packetHeader->messageType == MessageType::COMPONENT_SENSE) {
        ConnPacketComponentMessage const *packet = (ConnPacketComponentMessage const *)data;
        if (packet->componentHeader.moduleId == ModuleId::ASSET_MODULE) {
            replaceSenderId = false;
        }
    }
    //If our partner has a global id, we don't have to replace the ID with the virtual partner ID...
    if (partnerId >= NODE_ID_GLOBAL_DEVICE_BASE && partnerId < NODE_ID_GLOBAL_DEVICE_BASE + NODE_ID_GLOBAL_DEVICE_BASE_SIZE)
    {
        //...but only if the virtual partner id was not overwritten. This is because an overwritten virtual partner id always
        //comes from the user which should always be a respected value or else the user sees the connected device under a
        //different nodeId than he specified.
        if (!virtualPartnerIdWasOverwritten)
        {
            replaceSenderId = false;
        }
    }

    //Replace the sender id with our virtual partner id
    DYNAMIC_ARRAY(changedBuffer, sendData->dataLength.GetRaw());
    if(packetHeader->sender == partnerId && replaceSenderId){
        CheckedMemcpy(changedBuffer, data, sendData->dataLength.GetRaw());
        ConnPacketHeader *changedPacketHeader = (ConnPacketHeader*)changedBuffer;
        changedPacketHeader->sender = virtualPartnerId;
        packetHeader = changedPacketHeader;
    }

    MeshAccessAuthorization auth = meshAccessMod->CheckAuthorizationForAll(sendData, (u8 const*)packetHeader, fmKeyId, DataDirection::DIRECTION_IN);

    //Block unauthorized packets
    if(
        auth == MeshAccessAuthorization::UNDETERMINED 
        || auth == MeshAccessAuthorization::BLACKLIST
    ){
        logt("WARNING", "Packet unauthorized");
        return;
    }

    Logger::GetInstance().LogCustomCount(CustomErrorTypes::COUNT_TOTAL_RECEIVED_MESSAGES);

    if(
        tunnelType == MeshAccessTunnelType::PEER_TO_PEER
        || tunnelType == MeshAccessTunnelType::REMOTE_MESH
    ){
        TO_HEX(data, sendData->dataLength.GetRaw());
        logt("MACONN", "Received remote mesh data %s (%u) from %u", dataHex, sendData->dataLength.GetRaw(), packetHeader->sender);

        //Only dispatch to the local node, virtualPartnerId and remote nodeIds are kept in tact
        if(auth <= MeshAccessAuthorization::LOCAL_ONLY) GS->cm.DispatchMeshMessage(this, sendData, packetHeader, true);
    }
    else if(tunnelType == MeshAccessTunnelType::LOCAL_MESH)
    {
        TO_HEX(data, sendData->dataLength.GetRaw());
        logt("MACONN", "Received data for local mesh %s (%u) from %u aka %u", dataHex, sendData->dataLength.GetRaw(), packetHeader->sender, virtualPartnerId);

        //Send to other Mesh-like Connections
        if(auth <= MeshAccessAuthorization::WHITELIST) GS->cm.RouteMeshData(this, sendData, (u8 const*)packetHeader);

        //Dispatch Message throughout the implementation to all modules
        if(auth <= MeshAccessAuthorization::LOCAL_ONLY) GS->cm.DispatchMeshMessage(this, sendData, packetHeader, true);

    //We must allow handshake packets
    } else if (packetHeader->messageType >= MessageType::ENCRYPT_CUSTOM_START && packetHeader->messageType <= MessageType::ENCRYPT_CUSTOM_DONE)
    {
        if(auth <= MeshAccessAuthorization::LOCAL_ONLY) GS->cm.DispatchMeshMessage(this, sendData, packetHeader, true);
    }

#ifdef SIM_ENABLED
    if (packetHeader->messageType == MessageType::CLUSTER_INFO_UPDATE
        && sendData->dataLength >= sizeof(ConnPacketClusterInfoUpdate)
    ) {
        const ConnPacketClusterInfoUpdate* data = (ConnPacketClusterInfoUpdate const *)packetHeader;
        logt("MACONN", "Received ClusterInfoUpdate over MACONN with size:%u and hops:%d", data->payload.clusterSizeChange, data->payload.hopsToSink);
    }
#endif
}


#define ________________________HANDLER________________________

//After connection, both sides must do a service and characteristic discovery for the other rx and tx handle
//Then, they must activate notifications on the tx handle
//After the partner has activated notifications on ones own tx handle, it is possible to transmit data
void MeshAccessConnection::ConnectionSuccessfulHandler(u16 connectionHandle)
{
    //Call super method
    BaseConnection::ConnectionSuccessfulHandler(connectionHandle);

    if(direction == ConnectionDirection::DIRECTION_OUT)
    {
        //First, we need to discover the remote service
        const ErrorType err = GS->gattController.DiscoverService(connectionHandle, meshAccessService->serviceUuid);
        if (err != ErrorType::SUCCESS)
        {
            logt("MACONN", "Failed to start discover service because %u", (u32)err);
        }
    }
}

void MeshAccessConnection::GATTServiceDiscoveredHandler(FruityHal::BleGattDBDiscoveryEvent& evt)
{
    logt("MACONN", "Service discovered %x", evt.serviceUUID.uuid);


    //Once the remote service was discovered, we must register for notifications
    if(evt.serviceUUID.uuid == meshAccessService->serviceUuid.uuid
        && evt.serviceUUID.type == meshAccessService->serviceUuid.type){
        for(u32 j = 0; j < evt.charateristicsCount; j++)
        {
            logt("MACONN", "Found service");
            //Save a reference to the rx handle of our partner
            if(evt.dbChar[j].charUUID.uuid == MA_SERVICE_RX_CHARACTERISTIC_UUID)
            {
                partnerRxCharacteristicHandle = evt.dbChar[j].handleValue;
                logt("MACONN", "Found rx char %u", partnerRxCharacteristicHandle);
            }
            //Save a reference to the rx handle of our partner and its CCCD Handle which is needed to enable notifications
            if(evt.dbChar[j].charUUID.uuid == MA_SERVICE_TX_CHARACTERISTIC_UUID)
            {
                partnerTxCharacteristicHandle = evt.dbChar[j].handleValue;
                partnerTxCharacteristicCccdHandle = evt.dbChar[j].cccdHandle;
                logt("MACONN", "Found tx char %u with cccd %u", partnerTxCharacteristicHandle, partnerTxCharacteristicCccdHandle);
            }
        }
    }

    if(partnerTxCharacteristicCccdHandle != 0){
        RegisterForNotifications();
    }
}

void MeshAccessConnection::DataSentHandler(const u8* data, MessageLength length, u32 messageHandle)
{
    if (messageHandle == anonceMessageHandle)
    {
        anonceMessageHandle = 0;
        //Set encryption state to encrypted because we await the next packet to be encrypted
        encryptionState = EncryptionState::ENCRYPTED;
    }
}

#define ________________________OTHER________________________

void MeshAccessConnection::PrintStatus()
{
    const char* directionString = (direction == ConnectionDirection::DIRECTION_IN) ? "IN " : "OUT";

    trace("%s MA state:%u, Queue:%u, hnd:%u, partnerId/virtual:%u/%u, tunnel %u" EOL,
        directionString,
        (u32)this->connectionState,
        GetPendingPackets(),
        connectionHandle,
        partnerId,
        virtualPartnerId,
        (u32)tunnelType);
}

u32 MeshAccessConnection::GetAmountOfCorruptedMessaged()
{
    return amountOfCorruptedMessages;
}

void MeshAccessConnection::KeepAliveFor(u32 timeDs)
{
    u32 newRemovalTimeDs = GS->appTimerDs + timeDs;
    if (newRemovalTimeDs > scheduledConnectionRemovalTimeDs)
    {
        scheduledConnectionRemovalTimeDs = newRemovalTimeDs;
    }
}

void MeshAccessConnection::KeepAliveForIfSet(u32 timeDs)
{
    if (scheduledConnectionRemovalTimeDs != 0) KeepAliveFor(timeDs);
}
