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
#include <adv_packets.h>
#include <conn_packets.h>
#include <LedWrapper.h>
#include <AdvertisingController.h>
#include <ConnectionManager.h>
#include "MeshConnection.h"
#include <SimpleBuffer.h>
#include <RecordStorage.h>
#include <Module.h>
#include <Terminal.h>
#include <ButtonListener.h>

extern "C"
{
#include <ble.h>
}

#pragma pack(push)
#pragma pack(1)
		//Persistently saved configuration (should be multiple of 4 bytes long)
		//Serves to store settings that are changeable, e.g. by enrolling the node
		struct NodeConfiguration : ModuleConfiguration {
			i8 dBmTX; //Transmit power used for meshing

			//TODO: These should probably be put in the enrollment module
			deviceTypes deviceType : 8;
			nodeID nodeId;
			networkID networkId;
			u8 networkKey[BLE_GAP_SEC_KEY_LEN]; //16 bytes
			fh_ble_gap_addr_t nodeAddress; //7 bytes

			//Insert more persistent config values here
			u32 reserved; //Mandatory, read Module.h
		};
#pragma pack(pop)

typedef struct
{
	u8 bleAddressType;  /**< See @ref BLE_GAP_ADDR_TYPES. */
	u8 bleAddress[BLE_GAP_ADDR_LEN];  /**< 48-bit address, LSB format. */
	u8 connectable;
	i8 rssi;
	u32 receivedTimeDs;
	advPacketPayloadJoinMeV0 payload;
}joinMeBufferPacket;

//meshServiceStruct that contains all information about the meshService
typedef struct meshServiceStruct_temporary
{
	u16                     		serviceHandle;
	ble_gatts_char_handles_t		sendMessageCharacteristicHandle;
	ble_uuid_t						serviceUuid;
} meshServiceStruct;

class Node:
		public TerminalCommandListener,
		public RecordStorageEventListener,
		public ButtonListener
{
	private:


		enum NodeModuleTriggerActionMessages
		{
			SET_DISCOVERY = 0
		};

		enum NodeModuleActionResponseMessages
		{
			SET_DISCOVERY_RESULT = 0
		};

		bool stateMachineDisabled = false;


		//Buffer that keeps a predefined number of join me packets
		#define MAX_JOIN_ME_PACKET_AGE_DS (10 * 10)
		#define JOIN_ME_PACKET_BUFFER_MAX_ELEMENTS 10
		joinMeBufferPacket raw_joinMePacketBuffer[JOIN_ME_PACKET_BUFFER_MAX_ELEMENTS];


		void SendModuleList(nodeID toNode, u8 requestHandle);


	public:
		static Node* getInstance()
		{
			return GS->node;
		}

		SimpleBuffer* joinMePacketBuffer;
		clusterID currentAckId;
		u16 connectionLossCounter;

		AdvJob* meshAdvJobHandle;

		NodeConfiguration persistentConfig;


		//Array that holds all active modules
		Module* activeModules[MAX_MODULE_COUNT] = {0};

		discoveryState currentDiscoveryState;
		discoveryState nextDiscoveryState;

		//The globel time is saved in seconds as UNIX timestamp
		u32 globalTimeSec;
		u32 previousRtcTicks;
		u16 globalTimeRemainderTicks; //remainder (second fraction) is saved as ticks (32768 per second) see APP_TIMER_CLOCK_FREQ

		//All of the mesh timings are save in deciseconds (Ds) because milliseconds
		//will overflow a u32 too fast
		u32 appTimerDs;
		i32 currentStateTimeoutDs;
		u16 passsedTimeSinceLastTimerHandlerDs;
		u32 lastDecisionTimeDs;
		u16 appTimerRandomOffsetDs;

		u8 noNodesFoundCounter; //Incremented every time that no interesting cluster packets are found

		//Variables (kinda private, but I'm too lazy to write getters)
		clusterSIZE clusterSize;
		clusterID clusterId;

		u32 radioActiveCount;

		bool outputRawData;

		bool initializedByGateway; //Can be set to true by a mesh gateway after all configuration has been set

		meshServiceStruct meshService;

		// Result of the bestCluster calculation
		enum decisionResult
		{
			DECISION_CONNECT_AS_SLAVE, DECISION_CONNECT_AS_MASTER, DECISION_NO_NODES_FOUND
		};

		//Node
		Node();

		void Initialize();
		void LoadDefaults();
		void InitializeMeshGattService();
		void CharacteristicsDiscoveredHandler(ble_evt_t* bleEvent);

		//Connection
		void HandshakeTimeoutHandler();
		void HandshakeDoneHandler(MeshConnection* connection, bool completedAsWinner);

		//Stuff
		Node::decisionResult DetermineBestClusterAvailable(void);
		void UpdateJoinMePacket();

		//States
		void ChangeState(discoveryState newState);
		void DisableStateMachine(bool disable); //Disables the ChangeState function and does therefore kill all automatic mesh functionality
		void Stop();

		//Persistent configuration
		void SaveConfiguration();

		//Connection handlers
		//Message handlers
		void AdvertisementMessageHandler(ble_evt_t* bleEvent);
		joinMeBufferPacket* findTargetBuffer(advPacketJoinMeV0* packet);

		//Timers and Stuff handler
		static void RadioEventHandler(bool radioActive);
		void TimerTickHandler(u16 timerDs);
		void UpdateGlobalTime();

		//Helpers
		clusterID GenerateClusterID(void);

		Module* GetModuleById(moduleID id);

		void SendClusterInfoUpdate(MeshConnection* ignoreConnection, connPacketClusterInfoUpdate* packet);
		void SendModuleActionMessage(u8 messageType, nodeID toNode, u8 actionType, u8 requestHandle, u8* additionalData, u16 additionalDataSize, bool reliable);
		void ReceiveClusterInfoUpdate(MeshConnection* connection, connPacketClusterInfoUpdate* packet);

		void HandOverMasterBitIfNecessary(MeshConnection * connection);
		
		bool HasAllMasterBits();

		u32 CalculateClusterScoreAsMaster(joinMeBufferPacket* packet);
		u32 CalculateClusterScoreAsSlave(joinMeBufferPacket* packet);
		void PrintStatus();
		void PrintBufferStatus();
		void SetTerminalTitle();

		void StartConnectionRSSIMeasurement(MeshConnection* connection);
		void StopConnectionRSSIMeasurement(MeshConnection* connection);

		//Receiving
		void MeshMessageReceivedHandler(MeshConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader);

		//Uart communication
		void UartSetCampaign();

		//Methods of TerminalCommandListener
		bool TerminalCommandHandler(std::string commandName, std::vector<std::string> commandArgs);

		//Will be called if a button has been clicked
		void ButtonHandler(u8 buttonId, u32 holdTimeDs);

		//Implements Storage Callback for loading the configuration
		void ConfigurationLoadedHandler();

		//Methods of ConnectionManagerCallback
		void MeshConnectionDisconnectedHandler(MeshConnection* connection);
		void MeshConnectionConnectedHandler();
		void MeshConnectingTimeoutHandler(ble_evt_t* bleEvent);

		//RecordStorage Listener
		void RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength);


		u8 GetBatteryRuntime();

};
