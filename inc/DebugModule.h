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
 * This is a test Module for testing stuff
 */

#pragma once

#include <Module.h>

class DebugModule: public Module
{
	private:

		//Module configuration that is saved persistently (size must be multiple of 4)
		struct DebugModuleConfiguration : ModuleConfiguration{
			u32 rebootTimeMs; //Time until reboot
			char testString[12];
		};

		//Counters for flood messages
		u8 flood;
		u32 packetsOut;
		u32 packetsIn;

		DebugModuleConfiguration configuration;


		enum DebugModuleTriggerActionMessages{
			RESET_NODE = 0,
			RESET_CONNECTION_LOSS_COUNTER = 1,
			FLOOD_MESSAGE = 2,
			INFO_MESSAGE = 3,
			CAUSE_HARDFAULT_MESSAGE = 4

		};

		enum DebugModuleActionResponseMessages{

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

		#pragma pack(pop)

		void CauseHardfault();

	public:
		DebugModule(u8 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot);

		void ConfigurationLoadedHandler();

		void ResetToDefaultConfiguration();

		void TimerEventHandler(u16 passedTime, u32 appTimer);

		bool TerminalCommandHandler(string commandName, vector<string> commandArgs);

		void ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength);

};

