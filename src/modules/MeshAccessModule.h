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

#include <array>
#include <Module.h>

#include <MeshAccessConnection.h>
#include <AdvertisingController.h>

typedef struct MeshAccessServiceStruct
{
    FruityHal::BleGattUuid          serviceUuid;
    u16                                serviceHandle;
    FruityHal::BleGattCharHandles    txCharacteristicHandle;
    FruityHal::BleGattCharHandles    rxCharacteristicHandle;
} MeshAccessServiceStruct;


#pragma pack(push)
#pragma pack(1)
//Service Data (max. 24 byte)
constexpr int SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA_LEGACY = 16; //The definition has changed on 30.07.2020.
constexpr int SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA = 24;
typedef struct
{
    AdvStructureServiceDataAndType data;

    NetworkId networkId; //Mesh network id
    u8 isEnrolled : 1; // Flag if this beacon is enrolled
    u8 isSink : 1;
    u8 isZeroKeyConnectable : 1;
    u8 IsConnectable : 1;
    u8 interestedInConnection : 1;
    u8 reserved : 3;
    u32 serialIndex; //SerialNumber index of the beacon
    std::array<ModuleId, 3> moduleIds; //Additional subServices offered with their data

    // CAREFUL!
    // The following data is potentially sliced off in older versions of this adv struct!
    // Make sure that a received packet is at least SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA
    // big before using these values!
    struct
    {
        DeviceType deviceType;
        u8 reserved[3];
        u8 reservedForEncryption[4];
    } potentiallySlicedOff;
}advStructureMeshAccessServiceData;
STATIC_ASSERT_SIZE(advStructureMeshAccessServiceData, 24);

typedef struct
{
    u8 len;
    ModuleId moduleId;
    //Some more data

}advStructureMeshAccessServiceSubServiceData;
STATIC_ASSERT_SIZE(advStructureMeshAccessServiceSubServiceData, 2);


constexpr int SIZEOF_MESH_ACCESS_SERVICE_DATA_ADV_MESSAGE_LEGACY = (SIZEOF_ADV_STRUCTURE_FLAGS + SIZEOF_ADV_STRUCTURE_UUID16 + SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA_LEGACY);
constexpr int SIZEOF_MESH_ACCESS_SERVICE_DATA_ADV_MESSAGE        = (SIZEOF_ADV_STRUCTURE_FLAGS + SIZEOF_ADV_STRUCTURE_UUID16 + SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA);
typedef struct
{
    AdvStructureFlags flags;
    AdvStructureUUID16 serviceUuids;
    advStructureMeshAccessServiceData serviceData;

}meshAccessServiceAdvMessage;
STATIC_ASSERT_SIZE(meshAccessServiceAdvMessage, SIZEOF_MESH_ACCESS_SERVICE_DATA_ADV_MESSAGE);

#pragma pack(pop)

constexpr u8 MA_SERVICE_BASE_UUID[] = { 0x58, 0x18, 0x05, 0xA0, 0x07, 0x0C, 0xFD, 0x93, 0x3C, 0x42, 0xCE, 0xAC, 0x00, 0x00, 0x00, 0x00 };

constexpr int MA_SERVICE_SERVICE_CHARACTERISTIC_UUID = 0x0001;
constexpr int MA_SERVICE_RX_CHARACTERISTIC_UUID = 0x0002;
constexpr int MA_SERVICE_TX_CHARACTERISTIC_UUID = 0x0003;
constexpr int MA_CHARACTERISTIC_MAX_LENGTH = 20;


enum class MeshAccessModuleTriggerActionMessages : u8{
    MA_CONNECT = 0,
    MA_DISCONNECT = 1,
    SERIAL_CONNECT = 2,
};

enum class MeshAccessModuleActionResponseMessages : u8{
    SERIAL_CONNECT = 2,
};

enum class MeshAccessModuleGeneralMessages : u8{
    MA_CONNECTION_STATE = 0,
};

enum class MeshAccessSerialConnectError : u8 {
    SUCCESS                      = 0,
    TIMEOUT_REACHED              = 1,
    OVERWRITTEN_BY_OTHER_REQUEST = 2,
};

//####### Module messages (these need to be packed)
#pragma pack(push)
#pragma pack(1)

    constexpr int SIZEOF_MA_MODULE_CONNECT_MESSAGE = 28;
    typedef struct
    {
        FruityHal::BleGapAddr targetAddress;
        FmKeyId fmKeyId;
        std::array<u8, 16> key;
        u8 tunnelType : 2;
        u8 reserved;

    }MeshAccessModuleConnectMessage;
    STATIC_ASSERT_SIZE(MeshAccessModuleConnectMessage, 29);

    constexpr int SIZEOF_MA_MODULE_DISCONNECT_MESSAGE = 7;
    typedef struct
    {
        FruityHal::BleGapAddr targetAddress;

    }MeshAccessModuleDisconnectMessage;
    STATIC_ASSERT_SIZE(MeshAccessModuleDisconnectMessage, 7);

    struct MeshAccessModuleSerialConnectMessage
    {
        u32 serialNumberIndexToConnectTo;
        FmKeyId fmKeyId;
        u8 key[16];
        NodeId nodeIdAfterConnect;
        u32 connectionInitialKeepAliveSeconds;

        bool operator==(const MeshAccessModuleSerialConnectMessage& other) const;
        bool operator!=(const MeshAccessModuleSerialConnectMessage& other) const;

    };
    STATIC_ASSERT_SIZE(MeshAccessModuleSerialConnectMessage, 30);

    struct MeshAccessModuleSerialConnectResponse
    {
        MeshAccessSerialConnectError code;
        NodeId partnerId;
    };
    STATIC_ASSERT_SIZE(MeshAccessModuleSerialConnectResponse, 3);

    constexpr int SIZEOF_MA_MODULE_CONNECTION_STATE_MESSAGE = 3;
    typedef struct
    {
        NodeId vPartnerId;
        u8 state;

    }MeshAccessModuleConnectionStateMessage;
    STATIC_ASSERT_SIZE(MeshAccessModuleConnectionStateMessage, 3);

#pragma pack(pop)
//####### Module messages end

#pragma pack(push)
#pragma pack(1)
    //Module configuration that is saved persistently (size must be multiple of 4)
    struct MeshAccessModuleConfiguration : ModuleConfiguration{
        //Insert more persistent config values here
    };
#pragma pack(pop)

/**
 * The MeshAccessModule manages all MeshAccessConnections and is used to either
 * set up connections to nodes in a different network (e.g. during enrollment).
 * Also, to be able to connect to Smartphones and being connectable.
 * It also manages the MeshAccess advertising job for broadcasting.
 */
class MeshAccessModule: public Module
{
    public:
        MeshAccessServiceStruct meshAccessService;
        static constexpr bool allowInboundConnections = true; //Whether incoming connections are allowed at all (No gatt service, no advertising)
        u8 enableAdvertising = true; //Advertise the meshaccessPacket connectable
        u8 disableIfInMesh = false; //Once a mesh connection is active, disable advertising
        bool allowUnenrolledUnsecureConnections = false; //whether or not unsecure connections should be allowed when unenrolled

        static constexpr u32 meshAccessDfuSurvivalTimeDs = SEC_TO_DS(60);
        static constexpr u32 meshAccessInterestedInConnectionInitialKeepAliveDs = SEC_TO_DS(10);
    private:

        std::array<ModuleId, 3> moduleIdsToAdvertise{};

        void RegisterGattService();
        bool gattRegistered;

        AdvJob* discoveryJobHandle;

        bool logNearby;
        char logWildcard[6];

        MeshAccessModuleSerialConnectMessage meshAccessSerialConnectMessage;
        u32 meshAccessSerialConnectMessageReceiveTimeDs = 0;
        static constexpr u32 meshAccessSerialConnectMessageTimeoutDs = SEC_TO_DS(15);
        NodeId meshAccessSerialConnectSender = 0;
        u8 meshAccessSerialConnectRequestHandle = 0;
        u32 meshAccessSerialConnectConnectionId = 0;

        void ReceivedMeshAccessConnectMessage(ConnPacketModule const * packet, MessageLength packetLength) const;
        void ReceivedMeshAccessDisconnectMessage(ConnPacketModule const * packet, MessageLength packetLength) const;
        void ReceivedMeshAccessConnectionStateMessage(ConnPacketModule const * packet, MessageLength packetLength) const;
        void ReceivedMeshAccessSerialConnectMessage(ConnPacketModule const * packet, MessageLength packetLength);

        void ResetSerialConnectAttempt(bool cleanupConnection);
        void SendMeshAccessSerialConnectResponse(MeshAccessSerialConnectError code, NodeId partnerId = 0);

        void OnFoundSerialIndexWithAddr(const FruityHal::BleGapAddr& addr, u32 serialNumberIndex);
    public:
        DECLARE_CONFIG_AND_PACKED_STRUCT(MeshAccessModuleConfiguration);

        MeshAccessModule();
        void UpdateMeshAccessBroadcastPacket(u16 advIntervalMs = 0);

        void ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength) override final;

        void ResetToDefaultConfiguration() override final;

        void TimerEventHandler(u16 passedTimeDs) override final;

        void MeshConnectionChangedHandler(MeshConnection& connection) override final;

        //Boradcast messages
        void AddModuleIdToAdvertise(ModuleId moduleId);
        void DisableBroadcast();

        //Authorization
        MeshAccessAuthorization CheckMeshAccessPacketAuthorization(BaseConnectionSendData* sendData, u8 const * data, FmKeyId fmKeyId, DataDirection direction) override final;
        MeshAccessAuthorization CheckAuthorizationForAll(BaseConnectionSendData* sendData, u8 const * data, FmKeyId fmKeyId, DataDirection direction) const;

        //Priority
        virtual DeliveryPriority GetPriorityOfMessage(const u8* data, MessageLength size) override;

        //Messages
        void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader) override final;
        void MeshAccessMessageReceivedHandler(MeshAccessConnection* connection, BaseConnectionSendData* sendData, u8* data) const;

        #ifdef TERMINAL_ENABLED
        TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override final;
        #endif
        void GapAdvertisementReportEventHandler(const FruityHal::GapAdvertisementReportEvent& advertisementReportEvent) override final;

        bool IsZeroKeyConnectable(const ConnectionDirection direction);
};

