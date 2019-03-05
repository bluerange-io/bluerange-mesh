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
 * The Node class is the heart and soul of this implementation. It uses a state
 * machine and timers to control the behaviour of the node.
 * It uses the FruityMesh algorithm to build up connections with surrounding nodes
 *
 */

#pragma once

#include <types.h>
#ifdef SIM_ENABLED
#include <SystemTest.h>
#endif
#include <GlobalState.h>
#include <LedWrapper.h>
#include <AdvertisingController.h>
#include <ConnectionManager.h>
#include "MeshConnection.h"
#include <RecordStorage.h>
#include <Module.h>
#include <Terminal.h>
#include "SimpleArray.h"

extern "C"
{
#include <ble.h>
}

constexpr int MAX_RAW_CHUNK_SIZE = 60;

enum class RawDataProtocol : u8
{
	UNSPECIFIED               = 0,
	HTTP                      = 1,
	GZIPPED_JSON              = 2,
	START_OF_USER_DEFINED_IDS = 200,
	LAST_ID                   = 255
};

enum class RawDataActionType : u8
{
	START          = 0,
	START_RECEIVED = 1,
	CHUNK          = 2,
	REPORT         = 3,
	ERROR_T        = 4
};

enum class RawDataErrorType : u8
{
	UNEXPECTED_END_OF_TRANSMISSION = 0,
	NOT_IN_A_TRANSMISSION          = 1,
	MALFORMED_MESSAGE              = 2,
	START_OF_USER_DEFINED_ERRORS   = 200,
	LAST_ID                        = 255
};

#pragma pack(push)
#pragma pack(1)
struct RawDataHeader
{
	connPacketHeader connHeader;
	u8 moduleId;
	u8 requestHandle;
	RawDataActionType actionType;
};

struct RawDataLight
{
	connPacketHeader connHeader;
	u8 moduleId;
	u8 requestHandle;
	RawDataProtocol protocolId;

	u8 payload[1];
};
static_assert(sizeof(RawDataLight) == 9, "SIZEOF ASSERT");

struct RawDataStart
{
	RawDataHeader header;

	u32 numChunks : 24;
	u32 protocolId : 8; //RawDataProtocol
	u32 fmKeyId;
	u32 reserved;
};
static_assert(sizeof(RawDataStart) == 20, "SIZEOF ASSERT");

struct RawDataStartReceived
{
	RawDataHeader header;

	u8 reserved[12];
};
static_assert(sizeof(RawDataStartReceived) == 20, "SIZEOF ASSERT");

enum class RawDataErrorDestination : u8
{
	SENDER   = 1,
	RECEIVER = 2,
	BOTH     = 3
};

struct RawDataError
{
	RawDataHeader header;
	RawDataErrorType type;
	RawDataErrorDestination destination;

	u8 reserved[10];
};
static_assert(sizeof(RawDataError) == 20, "SIZEOF ASSERT");

struct RawDataChunk
{
	RawDataHeader header;

	u32 chunkId : 24;
	u32 reserved : 8;
	u8 payload[1];
};
static_assert(sizeof(RawDataChunk) == 13, "SIZEOF ASSERT");
static_assert(offsetof(RawDataChunk, payload) % 4 == 0, "Payload should be 4 byte aligned!");

struct RawDataReport
{
	RawDataHeader header;
	u32 missings[3];
};
static_assert(sizeof(RawDataReport) == 20, "SIZEOF ASSERT");
#pragma pack(pop)

typedef struct
{
	u8 bleAddressType;  /**< See @ref BLE_GAP_ADDR_TYPES. */
	u8 bleAddress[BLE_GAP_ADDR_LEN];  /**< 48-bit address, LSB format. */
	u8 connectable;
	i8 rssi;
	u32 receivedTimeDs;
	u32 lastConnectAttemptDs;
	advPacketPayloadJoinMeV0 payload;
}joinMeBufferPacket;

//meshServiceStruct that contains all information about the meshService
typedef struct meshServiceStruct_temporary
{
	u16                     		serviceHandle;
	ble_gatts_char_handles_t		sendMessageCharacteristicHandle;
	ble_uuid_t						serviceUuid;
} meshServiceStruct;

#pragma pack(push)
#pragma pack(1)
	//Persistently saved configuration (should be multiple of 4 bytes long)
	//Serves to store settings that are changeable, e.g. by enrolling the node
	struct NodeConfiguration : ModuleConfiguration {
		i8 dBmTX; //Transmit power used for meshing
		fh_ble_gap_addr_t bleAddress; //7 bytes
		u8 enrollmentState;
		u8 deviceType; // one of deviceTypes enum
		NodeId nodeId;
		NetworkId networkId;
		u8 networkKey[16];
		u8 userBaseKey[16];
		u8 organizationKey[16];
	};
#pragma pack(pop)

class Node: public Module
{

private:
		enum class NodeModuleTriggerActionMessages : u8
		{
			SET_DISCOVERY = 0,
			RESET_NODE = 1
		};

		enum class NodeModuleActionResponseMessages : u8
		{
			SET_DISCOVERY_RESULT = 0
		};

		bool stateMachineDisabled = false;

		u32 rebootTimeDs;

		void SendModuleList(NodeId toNode, u8 requestHandle) const;

		void sendRawError(NodeId receiver, u8 moduleId, RawDataErrorType type, RawDataErrorDestination destination, u8 requestHandle) const;
		bool createRawHeader(RawDataHeader* outVal, RawDataActionType type, char* commandArgs[], char* requestHandle) const;

	public:
		DECLARE_CONFIG_AND_PACKED_STRUCT(NodeConfiguration);


		#define MAX_JOIN_ME_PACKET_AGE_DS (10 * 10)
		#define JOIN_ME_PACKET_BUFFER_MAX_ELEMENTS 10
		SimpleArray<joinMeBufferPacket, JOIN_ME_PACKET_BUFFER_MAX_ELEMENTS> joinMePackets;
		ClusterId currentAckId;
		u16 connectionLossCounter;
		u16 randomBootNumber;

		AdvJob* meshAdvJobHandle;

		discoveryState currentDiscoveryState;
		discoveryState nextDiscoveryState;

		//Timers for state changing
		i32 currentStateTimeoutDs;
		u32 lastDecisionTimeDs;

		u8 noNodesFoundCounter; //Incremented every time that no interesting cluster packets are found

		//Variables (kinda private, but I'm too lazy to write getters)
		ClusterSize clusterSize;
		ClusterId clusterId;

		u32 radioActiveCount;

		bool outputRawData;

		bool initializedByGateway; //Can be set to true by a mesh gateway after all configuration has been set

		meshServiceStruct meshService;

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


		#define SIZEOF_NODE_MODULE_RESET_MESSAGE 1
		typedef struct
		{
			u8 resetSeconds;

		} NodeModuleResetMessage;
		STATIC_ASSERT_SIZE(NodeModuleResetMessage, 1);

		//Node
		Node();
		void ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength) override;
		void ResetToDefaultConfiguration() override;

		void InitializeMeshGattService();

		//Connection
		void HandshakeTimeoutHandler() const;
		void HandshakeDoneHandler(MeshConnection* connection, bool completedAsWinner);

		//Stuff
		Node::DecisionStruct DetermineBestClusterAvailable(void);
		void UpdateJoinMePacket() const;

		//States
		void ChangeState(discoveryState newState);
		void DisableStateMachine(bool disable); //Disables the ChangeState function and does therefore kill all automatic mesh functionality
		void Stop();

		void KeepHighDiscoveryActive();

		//Connection handlers
		//Message handlers
		void AdvertisementMessageHandler(ble_evt_t& bleEvent);
		joinMeBufferPacket* findTargetBuffer(advPacketJoinMeV0* packet);

		//Timers
		void TimerEventHandler(u16 passedTimeDs) override;

		//Helpers
		ClusterId GenerateClusterID(void) const;

		Module* GetModuleById(moduleID id) const;

		void SendClusterInfoUpdate(MeshConnection* ignoreConnection, connPacketClusterInfoUpdate* packet) const;
		void ReceiveClusterInfoUpdate(MeshConnection* connection, connPacketClusterInfoUpdate* packet);

		void HandOverMasterBitIfNecessary(MeshConnection * connection) const;
		
		bool HasAllMasterBits() const;

		u32 CalculateClusterScoreAsMaster(joinMeBufferPacket* packet) const;
		u32 CalculateClusterScoreAsSlave(joinMeBufferPacket* packet) const;
		void PrintStatus() const;
		void PrintBufferStatus() const;
		void SetTerminalTitle() const;

		void StartConnectionRSSIMeasurement(MeshConnection& connection);
		void StopConnectionRSSIMeasurement(const MeshConnection& connection);

		void Reboot(u32 delayDs);

		//Receiving
		void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader) override;

		//Uart communication
		void UartSetCampaign();

		//Methods of TerminalCommandListener
		#ifdef TERMINAL_ENABLED
		bool TerminalCommandHandler(char* commandArgs[], u8 commandArgsSize) override;
		#endif

		//Will be called if a button has been clicked
		void ButtonHandler(u8 buttonId, u32 holdTimeDs) USE_BUTTONS_OVERRIDE;

		//Methods of ConnectionManagerCallback
		void MeshConnectionDisconnectedHandler(ConnectionState connectionStateBeforeDisconnection, u8 hadConnectionMasterBit, i16 connectedClusterSize, u32 connectedClusterId);
		void MeshConnectionConnectedHandler() const;
		void MeshConnectingTimeoutHandler(ble_evt_t* bleEvent);

		//RecordStorage Listener
		void RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength) override;

		bool GetKey(u32 fmKeyId, u8* keyOut) const;

#ifdef ENABLE_FAKE_NODE_POSITIONS
		void modifyEventForFakePositions(ble_evt_t* bleEvent) const;
#endif
};
