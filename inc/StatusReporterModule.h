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

		//Module configuration that is saved persistently (size must be multiple of 4)
		struct StatusReporterModuleConfiguration: ModuleConfiguration
		{
				//Insert more persistent config values here
				u16 samplingIntervalMs;
				u16 reportingIntervalMs;
		};

		StatusReporterModuleConfiguration configuration;

		enum StatusModuleTriggerActionMessages
		{
			SET_LED_MESSAGE = 0, GET_STATUS_MESSAGE = 1, GET_CONNECTIONS_MESSAGE = 2
		};

		enum StatusModuleActionResponseMessages
		{
			STATUS_MESSAGE = 0, CONNECTIONS_MESSAGE = 1
		};

		//####### Module specific message structs (these need to be packed)
		#pragma pack(push)
		#pragma pack(1)

			#define SIZEOF_STATUS_REPORTER_MODULE_CONNECTIONS_MESSAGE 12
			typedef struct
			{
				nodeID partner1;
				nodeID partner2;
				nodeID partner3;
				nodeID partner4;
				u8 rssi1;
				u8 rssi2;
				u8 rssi3;
				u8 rssi4;

			} StatusReporterModuleConnectionsMessage;

			#define SIZEOF_STATUS_REPORTER_MODULE_STATUS_MESSAGE 24
			typedef struct
			{
				u32 chipIdA;
				u32 chipIdB;
				clusterID clusterId;
				clusterSIZE clusterSize;
				ble_gap_addr_t accessAddress;
				u8 freeIn;
				u8 freeOut;
				u8 batteryInfo;

			} StatusReporterModuleStatusMessage;

		#pragma pack(pop)
		//####### Module messages end

		u32 lastReportingTimer;

		void SendConnectionInformation(nodeID toNode);

		void RequestStatusInformation(nodeID targetNode);
		void SendStatusInformation(nodeID toNode);

	public:
		StatusReporterModule(u16 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot);

		void ConfigurationLoadedHandler();

		void ResetToDefaultConfiguration();

		void TimerEventHandler(u16 passedTime, u32 appTimer);

		bool TerminalCommandHandler(string commandName, vector<string> commandArgs);

		void ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength);
};
