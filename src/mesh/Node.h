////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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

#include <cstddef>
#include <FmTypes.h>
#ifdef SIM_ENABLED
#include <SystemTest.h>
#endif
#include <LedWrapper.h>
#include <AdvertisingController.h>
#include <ScanController.h>
#include "MeshConnection.h"
#include <RecordStorage.h>
#include <Module.h>
#include <Terminal.h>
#include <array>
#include "ConnectionHandle.h"

constexpr int MAX_RAW_DATA_CHUNK_SIZE = 60;

constexpr int TIME_BEFORE_DISCOVERY_MESSAGE_SENT_SEC = 30;


typedef struct
{
    FruityHal::BleGapAddr    addr;
    FruityHal::BleGapAdvType advType;
    i8                       rssi;
    u8                       attemptsToConnect;
    u32                      receivedTimeDs;
    u32                      lastConnectAttemptDs;
    AdvPacketPayloadJoinMeV0 payload;
}joinMeBufferPacket;

//meshServiceStruct that contains all information about the meshService
typedef struct meshServiceStruct_temporary
{
    u16                             serviceHandle;
    FruityHal::BleGattCharHandles sendMessageCharacteristicHandle;
    FruityHal::BleGattUuid       serviceUuid;
} meshServiceStruct;

#pragma pack(push)
#pragma pack(1)
    //Persistently saved configuration (should be multiple of 4 bytes long)
    //Serves to store settings that are changeable, e.g. by enrolling the node
    struct NodeConfiguration : ModuleConfiguration {
        i8 dBmTX_deprecated;
        FruityHal::BleGapAddr bleAddress; //7 bytes
        EnrollmentState enrollmentState;
        u8 deviceType_deprecated;
        NodeId nodeId;
        NetworkId networkId;
        u8 networkKey[16];
        u8 userBaseKey[16];
        u8 organizationKey[16];
    };
#pragma pack(pop)

/*
 * The node represents a mesh-enabled device and is a mandatory module (id 0).
 * It implements the FruityMesh algorithm together with the ConnectionManager
 * and the MeshConnection. It also implements some core functionality that
 * other layers can use.
 */
class Node: public Module
{

private:
        enum class NodeModuleTriggerActionMessages : u8
        {
            SET_DISCOVERY             = 0,
            RESET_NODE                = 1,
            SET_PREFERRED_CONNECTIONS = 2,
            PING                      = 3,
            START_GENERATE_LOAD       = 4,
            GENERATE_LOAD_CHUNK       = 5,
            EMERGENCY_DISCONNECT      = 6,
        };

        enum class NodeModuleActionResponseMessages : u8
        {
            SET_DISCOVERY_RESULT             = 0,
            SET_PREFERRED_CONNECTIONS_RESULT = 2,
            PING                             = 3,
            START_GENERATE_LOAD_RESULT       = 4,
            EMERGENCY_DISCONNECT_RESULT      = 5,
        };

        #pragma pack(push, 1)
        enum class EmergencyDisconnectErrorCode : u8
        {
            SUCCESS                     = 0,
            NOT_ALL_CONNECTIONS_USED_UP = 1,
            CANT_DISCONNECT_ANYBODY     = 2,
        };

        struct EmergencyDisconnectResponseMessage
        {
            EmergencyDisconnectErrorCode code;
        };
        STATIC_ASSERT_SIZE(EmergencyDisconnectResponseMessage, 1);
        #pragma pack(pop)

        bool stateMachineDisabled = false;

        u32 rebootTimeDs = 0;

        void SendModuleList(NodeId toNode, u8 requestHandle) const;

        bool CreateRawHeader(RawDataHeader* outVal, RawDataActionType type, const char* commandArgs[], const char* requestHandle) const;

        u32 ModifyScoreBasedOnPreferredPartners(u32 score, NodeId partner) const;
        
        joinMeBufferPacket* DetermineBestCluster        (u32(Node::*clusterRatingFunction)(const joinMeBufferPacket& packet) const);
        joinMeBufferPacket* DetermineBestClusterAsSlave ();
        joinMeBufferPacket* DetermineBestClusterAsMaster();

        u32 CalculateClusterScoreAsMaster(const joinMeBufferPacket& packet) const;
        u32 CalculateClusterScoreAsSlave(const joinMeBufferPacket& packet) const;

        bool DoesBiggerKnownClusterExist();

        bool isSendingCapabilities = false;
        bool firstCallForCurrentCapabilityModule = false;
        constexpr static u32 TIME_BETWEEN_CAPABILITY_SENDINGS_DS = SEC_TO_DS(1);
        u32 timeSinceLastCapabilitySentDs = 0;
        u32 capabilityRetrieverModuleIndex = 0;
        u32 capabilityRetrieverLocal = 0;
        u32 capabilityRetrieverGlobal = 0;

        bool isInit = false;


#pragma pack(push)
#pragma pack(1)
        struct GenerateLoadTriggerMessage{
            NodeId target;
            u8 size;
            u8 amount;
            u8 timeBetweenMessagesDs;
        };
#pragma pack(pop)
        u8 generateLoadMessagesLeft = 0;
        u8 generateLoadTimeBetweenMessagesDs = 0;
        u8 generateLoadTimeSinceLastMessageDs = 0;
        u8 generateLoadPayloadSize = 0;
        u8 generateLoadRequestHandle = 0;
        constexpr static u8 generateLoadMagicNumber = 0x91;
        NodeId generateLoadTarget = 0;

        u32 emergencyDisconnectTimerDs = 0; //The time since this node was not involved in any mesh. Can be reset by other means as well, e.g. when an emergency disconnect was sent.
        constexpr static u32 emergencyDisconnectTimerTriggerDs = SEC_TO_DS(/*Two minutes*/ 2 * 60);
        MeshAccessConnectionHandle emergencyDisconnectValidationConnectionUniqueId;
        void ResetEmergencyDisconnect(); //Resets all the emergency disconnect variables and closes the validation connection.

    public:
        DECLARE_CONFIG_AND_PACKED_STRUCT(NodeConfiguration);


        static constexpr int MAX_JOIN_ME_PACKET_AGE_DS = SEC_TO_DS(10);
        static constexpr int JOIN_ME_PACKET_BUFFER_MAX_ELEMENTS = 10;
        std::array<joinMeBufferPacket, JOIN_ME_PACKET_BUFFER_MAX_ELEMENTS> joinMePackets{};
        ClusterId currentAckId = 0;
        u16 connectionLossCounter = 0;
        u16 randomBootNumber = 0;

        AdvJob* meshAdvJobHandle = nullptr;

        DiscoveryState currentDiscoveryState = DiscoveryState::OFF;
        DiscoveryState nextDiscoveryState    = DiscoveryState::INVALID;

        //Timers for state changing
        i32 currentStateTimeoutDs = 0;
        u32 lastDecisionTimeDs = 0;

        u8 noNodesFoundCounter = 0; //Incremented every time that no interesting cluster packets are found

        //Variables (kinda private, but I'm too lazy to write getters)
        ClusterSize clusterSize = 1;
        ClusterId clusterId = 0;

        u32 radioActiveCount = 0;

        bool outputRawData = false;

        u32 disconnectTimestampDs = 0;

        bool initializedByGateway = false; //Can be set to true by a mesh gateway after all configuration has been set

        meshServiceStruct meshService;

        ScanJob * p_scanJob = nullptr;

        bool isInBulkMode = false;

        // Result of the bestCluster calculation
        enum class DecisionResult : u8
        {
            CONNECT_AS_SLAVE, 
            CONNECT_AS_MASTER, 
            NO_NODES_FOUND
        };

        struct DecisionStruct {
            DecisionResult result;
            NodeId preferredPartner;
            u32 establishResult;
        };


        static constexpr int SIZEOF_NODE_MODULE_RESET_MESSAGE = 1;
        typedef struct
        {
            u8 resetSeconds;

        } NodeModuleResetMessage;
        STATIC_ASSERT_SIZE(NodeModuleResetMessage, 1);

#pragma pack(push)
#pragma pack(1)
        typedef struct
        {
            NodeId preferredPartnerIds[Conf::MAX_AMOUNT_PREFERRED_PARTNER_IDS];
            u8 amountOfPreferredPartnerIds;
            PreferredConnectionMode preferredConnectionMode;
        } PreferredConnectionMessage;
        STATIC_ASSERT_SIZE(PreferredConnectionMessage, 18);
#pragma pack(pop)

        //Node
        Node();
        void Init();
        bool IsInit();
        void ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength) override final;
        void ResetToDefaultConfiguration() override final;

        void InitializeMeshGattService();

        //Connection
        void HandshakeTimeoutHandler() const;
        void HandshakeDoneHandler(MeshConnection* connection, bool completedAsWinner); 
        MeshAccessAuthorization CheckMeshAccessPacketAuthorization(BaseConnectionSendData* sendData, u8 const * data, FmKeyId fmKeyId, DataDirection direction) override final;

        //Stuff
        Node::DecisionStruct DetermineBestClusterAvailable(void);
        void UpdateJoinMePacket() const;
        void StartFastJoinMeAdvertising();

        //States
        void ChangeState(DiscoveryState newState);
        void DisableStateMachine(bool disable); //Disables the ChangeState function and does therefore kill all automatic mesh functionality

        void KeepHighDiscoveryActive();

        //Connection handlers
        //Message handlers
        void GapAdvertisementMessageHandler(const FruityHal::GapAdvertisementReportEvent& advertisementReportEvent);
        joinMeBufferPacket* FindTargetBuffer(const AdvPacketJoinMeV0* packet);

        //Timers
        void TimerEventHandler(u16 passedTimeDs) override final;

        //Helpers
        ClusterId GenerateClusterID(void) const;

        Module* GetModuleById(ModuleId id) const;
        Module* GetModuleById(VendorModuleId id) const;

        void SendClusterInfoUpdate(MeshConnection* ignoreConnection, ConnPacketClusterInfoUpdate* packet) const;
        void ReceiveClusterInfoUpdate(MeshConnection* connection, ConnPacketClusterInfoUpdate const * packet);

        void HandOverMasterBitIfNecessary() const;
        
        bool HasAllMasterBits() const;

        void PrintStatus() const;
        void PrintBufferStatus() const;
        void SetTerminalTitle() const;
        CapabilityEntry GetCapability(u32 index, bool firstCall) override final;
        CapabilityEntry GetNextGlobalCapability();

        void Reboot(u32 delayDs, RebootReason reason);
        bool IsRebootScheduled();

        //Receiving
        void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader) override final;

        //Methods of TerminalCommandListener
        #ifdef TERMINAL_ENABLED
        TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override final;
        
        //Helper method for parsing component_act or _sense Terminal command
        ErrorTypeUnchecked SendComponentMessageFromTerminal(MessageType componentMessageType, const char* commandArgs[], u8 commandArgsSize);
        #endif

        //Methods of ConnectionManagerCallback
        void MeshConnectionDisconnectedHandler(AppDisconnectReason appDisconnectReason, ConnectionState connectionStateBeforeDisconnection, u8 hadConnectionMasterBit, i16 connectedClusterSize, u32 connectedClusterId);
        
        bool GetKey(FmKeyId fmKeyId, u8* keyOut) const;
        bool IsPreferredConnection(NodeId id) const;
};
