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
 * This is a test Module for testing stuff
 */

#pragma once

#include <Module.h>

class DebugModule: public Module
{
	private:

		#pragma pack(push, 1)
		//Module configuration that is saved persistently
		struct DebugModuleConfiguration : ModuleConfiguration{
			u8 debugButtonRemoveEnrollmentDs;
			u8 debugButtonEnableUartDs;
			//Insert more persistent config values here
		};
		#pragma pack(pop)

		u32 rebootTimeDs; //Time until reboot

		//Counters for flood messages
		u8 flood;
		u32 packetsOut;
		u32 packetsIn;

		//Counters for ping
		u32 pingSentTicks;
		u8 pingHandle;
		u16 pingCount;
		u16 pingCountResponses;

		DebugModuleConfiguration configuration;


		enum DebugModuleTriggerActionMessages{
			RESET_NODE = 0,
			RESET_CONNECTION_LOSS_COUNTER = 1,
			FLOOD_MESSAGE = 2,
			INFO_MESSAGE = 3,
			CAUSE_HARDFAULT_MESSAGE = 4,
			SET_REESTABLISH_TIMEOUT = 5,
			PING = 6,
			PINGPONG = 7,
			SET_DISCOVERY = 8,
			LPING = 9

		};

		enum DebugModuleActionResponseMessages{
			REESTABLISH_TIMEOUT_RESPONSE = 5,
			PING_RESPONSE = 6,
			SET_DISCOVERY_RESPONSE = 8,
			LPING_RESPONSE = 9
		};

		#pragma pack(push)
		#pragma pack(1)

		#define SIZEOF_DEBUG_MODULE_INFO_MESSAGE 6
		typedef struct
		{
			u16 connectionLossCounter;
			u16 droppedPackets;
			u16 sentPackets;


		} DebugModuleInfoMessage;

		#define SIZEOF_DEBUG_MODULE_REESTABLISH_TIMEOUT_MESSAGE 2
		typedef struct
		{
			u16 reestablishTimeoutSec;

		} DebugModuleReestablishTimeoutMessage;

		#define SIZEOF_DEBUG_MODULE_PINGPONG_MESSAGE 1
		typedef struct
		{
			u8 ttl;

		} DebugModulePingpongMessage;

		#define SIZEOF_DEBUG_MODULE_LPING_MESSAGE 4
		typedef struct
		{
			nodeID leafNodeId;
			u16 hops;

		} DebugModuleLpingMessage;


		#define SIZEOF_DEBUG_MODULE_SET_DISCOVERY_MESSAGE 1
		typedef struct
		{
			u8 discoveryMode;

		} DebugModuleSetDiscoveryMessage;

		#pragma pack(pop)

		void CauseHardfault();


	public:
		DebugModule(u8 moduleId, Node* node, ConnectionManager* cm, const char* name);

		void ConfigurationLoadedHandler();

		void ResetToDefaultConfiguration();

		void TimerEventHandler(u16 passedTimeDs, u32 appTimerDs);

		void ButtonHandler(u8 buttonId, u32 holdTimeDs);

		bool TerminalCommandHandler(std::string commandName, std::vector<std::string> commandArgs);

		void MeshMessageReceivedHandler(MeshConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader);

};

