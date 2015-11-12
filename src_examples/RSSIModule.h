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

#pragma once

#include <Module.h>

class RSSIModule: public Module
{
	private:
        enum RSSISamplingModes {
            RSSI_SAMPLING_NONE
            , RSSI_SAMPLING_LOW
            , RSSI_SAMPLING_MEDIUM
            , RSSI_SAMPLING_HIGH
        };

		//Module configuration that is saved persistently (size must be multiple of 4)
		struct RSSIModuleConfiguration : ModuleConfiguration {
            u8 connectionRSSISamplingMode;  //typeof RSSISamplingModes
            u8 advertisingRSSISamplingMode; //typeof RSSISamplingModes
			int pingInterval;
			int lastPingTimer;
			int pingCount;
		};

		RSSIModuleConfiguration configuration;

		enum RSSIModuleTriggerActionMessages {
			TRIGGER_PING=0
		};

		enum RSSIModuleActionResponseMessages {
			PING_RESPONSE=0
		};

		bool SendPing(nodeID targetNodeId);
        void update_led_colour();
        void set_led_colour(int colour);

	public:
		RSSIModule(u16 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot);

		void ConfigurationLoadedHandler();
		void ResetToDefaultConfiguration();
		void TimerEventHandler(u16 passedTime, u32 appTimer);
		void ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength);
		bool TerminalCommandHandler(string commandName, vector<string> commandArgs);

        virtual void MeshConnectionChangedHandler(Connection* connection);
        virtual void StartConnectionRSSIMeasurement(Connection* connection);
        virtual void BleEventHandler(ble_evt_t* bleEvent);
};
