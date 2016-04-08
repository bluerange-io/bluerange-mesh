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
 * The AdvertisingModule is used to broadcast user-data that is not related with
 * the mesh during times where no mesh discovery is ongoing. It is used
 * to broadcast messages to smartphones or other devices from all mesh nodes.
 */

#pragma once

#include <Module.h>

#define ADVERTISING_MODULE_MAX_MESSAGES 1
#define ADVERTISING_MODULE_MAX_MESSAGE_LENGTH 31

class AdvertisingModule: public Module
{
	private:
		struct AdvertisingMessage{
			u8 forceNonConnectable : 1; //Always send this message non-connectable
			u8 forceConnectable : 1; //Message is only sent, when it is possible to send it in connectable mode (if we have a free slave connection)
			u8 reserved : 1;
			u8 messageLength : 5;
			u8 messageData[ADVERTISING_MODULE_MAX_MESSAGE_LENGTH];
		};

		//Module configuration that is saved persistently (size must be multiple of 4)
		struct AdvertisingModuleConfiguration : ModuleConfiguration{
			//Insert more persistent config values here
			//The interval at which the device advertises
			//If multiple messages are configured, they will be distributed round robin
			u16 advertisingIntervalMs;
			//Number of messages
			u8 messageCount;
			u8 reserved;
			AdvertisingMessage messageData[ADVERTISING_MODULE_MAX_MESSAGES];
		};

		u16 maxMessages = ADVERTISING_MODULE_MAX_MESSAGES; //Save this, so that it can be requested

		AdvertisingModuleConfiguration configuration;

		//Set all advertising messages at once, the old configuration will be overwritten
		void SetAdvertisingMessages(u8* data, u16 dataLength);

		#pragma pack(push)
		#pragma pack(1)

		typedef struct
		{
			u8 debugPacketIdentifier;
			nodeID senderId;
			u16 connLossCounter;
			nodeID partners[4];
			i8 rssiVals[3];
			u8 droppedVals[3];

		} AdvertisingModuleDebugMessage;

		#pragma pack(pop)



	public:
		AdvertisingModule(u8 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot);

		void ConfigurationLoadedHandler();

		void ResetToDefaultConfiguration();

		void TimerEventHandler(u16 passedTime, u32 appTimer);

		void NodeStateChangedHandler(discoveryState newState);

		bool TerminalCommandHandler(string commandName, vector<string> commandArgs);
};
