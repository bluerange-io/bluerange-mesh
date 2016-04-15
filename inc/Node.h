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


		bool stateMachineDisabled = false;

		//Persistently saved configuration (should be multiple of 4 bytes long)
		struct NodeConfiguration{
			u32 version;
			ble_gap_addr_t nodeAddress; //7 bytes
			networkID networkId;
			nodeID nodeId;
			u8 networkKey[BLE_GAP_SEC_KEY_LEN]; //16 bytes
			u16 connectionLossCounter; //TODO: connection loss counter is not saved persistently, move it.
			deviceTypes deviceType;
			//TODO: don't know if we need receiver sensitivity,...
			u8 dBmTX; //The average RSSI, received in a distance of 1m with a tx power of +0 dBm
			u8 dBmRX; //Receiver sensitivity (or receied power from a packet sent at 1m distance with +0dBm?)
			u8 reserved;
			u8 reserved2;
		};

		void SendModuleList(nodeID toNode, u8 requestHandle);


	public:
		static Node* getInstance()
		{
			return instance;
		}

		static ConnectionManager* cm;

		SimpleBuffer* joinMePacketBuffer;
		clusterID currentAckId;

		NodeConfiguration persistentConfig;


		//Array that holds all active modules
		Module* activeModules[MAX_MODULE_COUNT] = {0};

		discoveryState currentDiscoveryState;
		discoveryState nextDiscoveryState;

		//The global time is saved as 32768 ticks per second (APP_TIMER_CLOCK_FREQ)
		//Any node that uses a prescaler must therefore adapt to this

		u64 globalTime; //Hooray to the compiler for abstracting 64bit operations :-)
		u32 globalTimeSetAt; //The value of the RTC timer when the globalTime was set

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

		u8 ledBlinkPosition;

		ledMode currentLedMode;

		bool outputRawData;

		bool initializedByGateway; //Can be set to true by a mesh gateway after all configuration has been set


		// Result of the bestCluster calculation
		enum decisionResult
		{
			DECISION_CONNECT_AS_SLAVE, DECISION_CONNECT_AS_MASTER, DECISION_NO_NODES_FOUND
		};

		//Node
		Node(networkID networkId);

		//Connection
		void HandshakeTimeoutHandler();
		void HandshakeDoneHandler(Connection* connection, bool completedAsWinner);

		//Stuff
		Node::decisionResult DetermineBestClusterAvailable(void);
		void UpdateJoinMePacket();
		void UpdateScanResponsePacket(u8* newData, u8 length);

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
		void TimerTickHandler(u16 timerMs);

		//Helpers
		clusterID GenerateClusterID(void);

		void SendClusterInfoUpdate(Connection* ignoreConnection, connPacketClusterInfoUpdate* packet);
		void ReceiveClusterInfoUpdate(Connection* connection, connPacketClusterInfoUpdate* packet);

		u32 CalculateClusterScoreAsMaster(joinMeBufferPacket* packet);
		u32 CalculateClusterScoreAsSlave(joinMeBufferPacket* packet);
		void PrintStatus(void);
		void PrintBufferStatus(void);
		void PrintSingleLineStatus(void);
		void SetTerminalTitle();

		void StartConnectionRSSIMeasurement(Connection* connection);
		void StopConnectionRSSIMeasurement(Connection* connection);

		//Uart communication
		void UartSetCampaign();

		//Methods of TerminalCommandListener
		bool TerminalCommandHandler(string commandName, vector<string> commandArgs);

		//Implements Storage Callback for loading the configuration
		void ConfigurationLoadedHandler();

		//Methods of ConnectionManagerCallback
		void DisconnectionHandler(ble_evt_t* bleEvent);
		void ConnectionSuccessfulHandler(ble_evt_t* bleEvent);
		void ConnectingTimeoutHandler(ble_evt_t* bleEvent);
		void messageReceivedCallback(connectionPacket* inPacket);

		u8 GetBatteryRuntime();

};
