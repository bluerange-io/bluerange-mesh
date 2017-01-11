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
 * The status reporter module is responsible for measuring battery, connections,
 * etc... and report them back to a sink
 */

#pragma once

#include <Module.h>
#include <Terminal.h>

class StatusReporterModule: public Module
{
	private:

		enum RSSISampingModes{RSSI_SAMLING_NONE=0, RSSI_SAMLING_LOW=1, RSSI_SAMLING_MEDIUM=2, RSSI_SAMLING_HIGH=3};

		#pragma pack(push, 1)
		//Module configuration that is saved persistently
		struct StatusReporterModuleConfiguration: ModuleConfiguration
		{
				u16 connectionReportingIntervalDs;
				u16 statusReportingIntervalDs;
				u8 connectionRSSISamplingMode; //typeof RSSISampingModes
				u8 advertisingRSSISamplingMode; //typeof RSSISampingModes
				u16 nearbyReportingIntervalDs;
				u16 deviceInfoReportingIntervalDs;
				//Insert more persistent config values here
				u32 reserved; //Mandatory, read Module.h
		};
		#pragma pack(pop)

		DECLARE_CONFIG_AND_PACKED_STRUCT(StatusReporterModuleConfiguration);

		enum StatusModuleTriggerActionMessages
		{
			SET_LED = 0,
			GET_STATUS = 1,
			GET_DEVICE_INFO = 2,
			GET_ALL_CONNECTIONS = 3,
			GET_NEARBY_NODES = 4,
			SET_INITIALIZED = 5,
			GET_ERRORS = 6
		};

		enum StatusModuleActionResponseMessages
		{
			SET_LED_RESULT = 0,
			STATUS = 1,
			DEVICE_INFO = 2,
			ALL_CONNECTIONS = 3,
			NEARBY_NODES = 4,
			SET_INITIALIZED_RESULT = 5,
			ERROR_LOG_ENTRY = 6
		};

		//####### Module specific message structs (these need to be packed)
		#pragma pack(push)
		#pragma pack(1)

		typedef struct
			{
				nodeID nodeId;
				i32 rssiSum;
				u16 packetCount;
			} nodeMeasurement;

			#define SIZEOF_STATUS_REPORTER_MODULE_CONNECTIONS_MESSAGE 12
			typedef struct
			{
				nodeID partner1;
				nodeID partner2;
				nodeID partner3;
				nodeID partner4;
				i8 rssi1;
				i8 rssi2;
				i8 rssi3;
				i8 rssi4;

			} StatusReporterModuleConnectionsMessage;

			//This message delivers non- (or not often)changing information
			#define SIZEOF_STATUS_REPORTER_MODULE_DEVICE_INFO_MESSAGE (25 + SERIAL_NUMBER_LENGTH)
			typedef struct
			{
				u16 manufacturerId;
				u8 serialNumber[SERIAL_NUMBER_LENGTH];
				u8 chipId[8];
				ble_gap_addr_t accessAddress;
				networkID networkId;
				u32 nodeVersion;
				u8 dBmRX;
				u8 dBmTX;
				u8 deviceType;

			} StatusReporterModuleDeviceInfoMessage;

			//This message delivers often changing information and info about the incoming connection
			#define SIZEOF_STATUS_REPORTER_MODULE_STATUS_MESSAGE 9
			typedef struct
			{
				clusterSIZE clusterSize;
				nodeID inConnectionPartner;
				i8 inConnectionRSSI;
				u8 freeIn : 2;
				u8 freeOut : 6;
				u8 batteryInfo;
				u8 connectionLossCounter; //Connection losses since reboot
				u8 initializedByGateway : 1; //Set to 0 if node has been resetted and does not know its configuration

			} StatusReporterModuleStatusMessage;

			//This message delivers often changing information and info about the incoming connection
			#define SIZEOF_STATUS_REPORTER_MODULE_ERROR_LOG_ENTRY_MESSAGE 11
			typedef struct
			{
				u8 errorType;
				u16 extraInfo;
				u32 errorCode;
				u32 timestamp;
			} StatusReporterModuleErrorLogEntryMessage;

		#pragma pack(pop)
		//####### Module messages end


#define NUM_NODE_MEASUREMENTS 20
		nodeMeasurement nodeMeasurements[NUM_NODE_MEASUREMENTS];


		void SendStatus(nodeID toNode, u8 messageType);
		void SendDeviceInfo(nodeID toNode, u8 messageType);
		void SendNearbyNodes(nodeID toNode, u8 messageType);
		void SendAllConnections(nodeID toNode, u8 messageType);

		void StartConnectionRSSIMeasurement(Connection* connection);
		void StopConnectionRSSIMeasurement(Connection* connection);

	public:
		StatusReporterModule(u8 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot);

		void ConfigurationLoadedHandler();

		void ResetToDefaultConfiguration();

		void TimerEventHandler(u16 passedTimeDs, u32 appTimerDs);

		bool TerminalCommandHandler(string commandName, vector<string> commandArgs);

		void ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength);

		void BleEventHandler(ble_evt_t* bleEvent);

		void ButtonHandler(u8 buttonId, u32 holdTime);

		void MeshConnectionChangedHandler(Connection* connection);

};
