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

extern "C"{
#include <nrf_gpio.h>
}

class IoModule: public Module
{
	private:

		#pragma pack(push, 1)
		//Module configuration that is saved persistently
		struct IoModuleConfiguration : ModuleConfiguration{
			//Insert more persistent config values here
			u32 reserved; //Mandatory, read Module.h
		};
		#pragma pack(pop)

		DECLARE_CONFIG_AND_PACKED_STRUCT(IoModuleConfiguration);

		enum IoModuleTriggerActionMessages{
			SET_PIN_CONFIG = 0,
			GET_PIN_CONFIG = 1,
			GET_PIN_LEVEL = 2,
			SET_LED = 3 //used to trigger a signaling led
		};

		enum IoModuleActionResponseMessages{
			SET_PIN_CONFIG_RESULT = 0,
			PIN_CONFIG = 1,
			PIN_LEVEL = 2,
			SET_LED_RESPONSE = 3
		};

		//Combines a pin and its config
		#define SIZEOF_GPIO_PIN_CONFIG 2
		struct gpioPinConfig{
			u8 pinNumber : 5;
			u8 direction : 1; //configure pin as either input or output (nrf_gpio_pin_dir_t)
			u8 inputBufferConnected : 1; //disconnect input buffer when port not used to save energy
			u8 pull : 2; //pull down (1) or up (3) or disable pull (0) on pin (nrf_gpio_pin_pull_t)
			u8 driveStrength : 3; // GPIO_PIN_CNF_DRIVE_*
			u8 sense : 2; // if configured as input sense either high or low level
			u8 set : 1; // set pin or unset it
		};


		//####### Module messages (these need to be packed)
		#pragma pack(push)
		#pragma pack(1)

			#define SIZEOF_IO_MODULE_SET_LED_MESSAGE 1
			typedef struct
			{
				u8 ledMode;

			}IoModuleSetLedMessage;

		#pragma pack(pop)
		//####### Module messages end



	public:
		IoModule(u8 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot);

		void ConfigurationLoadedHandler();

		void ResetToDefaultConfiguration();

		void TimerEventHandler(u16 passedTimeDs, u32 appTimerDs);

		//void BleEventHandler(ble_evt_t* bleEvent);

		void ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength);

		//void NodeStateChangedHandler(discoveryState newState);

		bool TerminalCommandHandler(string commandName, vector<string> commandArgs);
};
