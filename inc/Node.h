/**

Copyright (c) 2014-2015 "M-Way Solutions GmbH"
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
#include <adv_packets.h>
#include <conn_packets.h>
#include <LedWrapper.h>
#include <ConnectionManager.h>
#include <Connection.h>
#include <SimpleBuffer.h>
#include <Storage.h>
#include <Module.h>
#include <Terminal.h>

extern "C"
{
#include <ble.h>
}

typedef struct
{
	u8 bleAddressType;  /**< See @ref BLE_GAP_ADDR_TYPES. */
	u8 bleAddress[BLE_GAP_ADDR_LEN];  /**< 48-bit address, LSB format. */
	u8 connectable;
	i8 rssi;
	u32 receivedTime;
	advPacketPayloadJoinMeV0 payload;
}joinMeBufferPacket;

class Node:
		public TerminalCommandListener,
		public ConnectionManagerCallback,
		public StorageEventListener
{
	private:
		static Node* instance;
		static ConnectionManager* cm;

		nodeID ackFieldDebugCopy;

		bool stateMachineDisabled = false;

		//Persistently saved configuration (should be multiple of 4 bytes long)
		struct NodeConfiguration{
			u8 version;
			ble_gap_addr_t nodeAddress; //7 bytes
			networkID networkId;
			nodeID nodeId;
			u8 longTermKey[BLE_GAP_SEC_KEY_LEN]; //16 bytes
			u16 connectionLossCounter;
			deviceTypes deviceType;
			u16 reserved;
		};

		//For our test devices
		typedef struct{
			u32 chipID;
			nodeID id;
			deviceTypes deviceType;
			char name[4];
			ble_gap_addr_t addr;
		} testDevice;


		#define NUM_TEST_DEVICES 10
		static testDevice testDevices[];


		void InitWithTestDeviceSettings();


	public:
		static Node* getInstance()
		{
			return instance;
		}

		LedWrapper* LedRed;
		LedWrapper* LedGreen;
		LedWrapper* LedBlue;

		SimpleBuffer* joinMePacketBuffer;

		NodeConfiguration persistentConfig;


		//Array that holds all active modules
		Module* activeModules[MAX_MODULE_COUNT] = {0};

		discoveryState currentDiscoveryState;
		discoveryState nextDiscoveryState;

		u16 passsedTimeSinceLastTimerHandler;
		i32 currentStateTimeoutMs;
		u32 appTimerMs;
		u32 lastDecisionTimeMs;
		u8 noNodesFoundCounter; //Incremented every time that no interesting cluster packets are found

		//Variables (kinda private, but I'm too lazy to write getters)
		clusterSIZE clusterSize;
		clusterID clusterId;

		u32 radioActiveCount;
		u32 lastRadioActiveCountResetTimerMs;

		enum ledMode
		{
			LED_MODE_OFF, LED_MODE_CONNECTIONS, LED_MODE_RADIO
		};
		ledMode currentLedMode;

		bool outputRawData;


		// Result of the bestCluster calculation
		enum decisionResult
		{
			DECISION_CONNECT_AS_SLAVE, DECISION_CONNECT_AS_MASTER, DECISION_NO_NODES_FOUND
		};

		//Node
		Node(networkID networkId);

		//Connection
		void HandshakeDoneHandler(Connection* connection);

		//Stuff
		Node::decisionResult DetermineBestClusterAvailable(void);
		void UpdateJoinMePacket(joinMeBufferPacket* ackCluster);
		void UpdateScanResponsePacket(u8* newData, u8 length);

		//States
		void ChangeState(discoveryState newState);
		void DisableStateMachine(bool disable); //Disables the ChangeState function and does therefore kill all automatic mesh functionality


		//Connection handlers
		//Message handlers
		void AdvertisementMessageHandler(ble_evt_t* bleEvent);
		//Timers and Stuff handler
		void RadioEventHandler(bool radioActive);
		void TimerTickHandler(u16 timerMs);

		//Helpers
		clusterID GenerateClusterID(void);
		void UpdateClusterInfo(Connection* connection, connPacketClusterInfoUpdate* packet);
		u32 CalculateClusterScoreAsMaster(joinMeBufferPacket* packet);
		u32 CalculateClusterScoreAsSlave(joinMeBufferPacket* packet);
		void PrintStatus(void);
		void PrintBufferStatus(void);
		void PrintSingleLineStatus(void);


		//Uart communication
		void UartGetStatus();
		void UartSetCampaign();

		//Methods of TerminalCommandListener
		bool TerminalCommandHandler(string commandName, vector<string> commandArgs);

		//Implements Storage Callback for loading the configuration
		void ConfigurationLoadedHandler();

		//Methods of ConnectionManagerCallback
		void DisconnectionHandler(ble_evt_t* bleEvent);
		void ConnectionSuccessfulHandler(ble_evt_t* bleEvent);
		void ConnectionTimeoutHandler(ble_evt_t* bleEvent);
		void messageReceivedCallback(connectionPacket* inPacket);

		//Set to true, to reset the system every 25 seconds and lock it when we find invalid states
		static bool lookingForInvalidStateErrors;

};
