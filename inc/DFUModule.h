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
 * The DFU module should provide DFU over BLE update capabilities in the future
 * but is currently not working.
 * */

#pragma once

#include <Module.h>

extern "C"{
#include <ble.h>
#include <ble_dfu.h>
}

class DFUModule: public Module
{
	private:

		//Module configuration that is saved persistently (size must be multiple of 4)
		struct DFUModuleConfiguration : ModuleConfiguration{

		};

		DFUModuleConfiguration configuration;

		static ble_dfu_init_t   dfus_init;
		static ble_dfu_t         m_dfus;  /**< Structure used to identify the DFU service. */

	public:
		DFUModule(u16 moduleId, Node* node, ConnectionManager* cm, char* name, u16 storageSlot);

		void ConfigurationLoadedHandler();

		void ResetToDefaultConfiguration();

		static void AppDFUHandler(ble_dfu_t * p_dfu, ble_dfu_evt_t * p_evt);

		void BleEventHandler(ble_evt_t* bleEvent);

		void TimerEventHandler(u16 passedTime, u32 appTimer);

		bool TerminalCommandHandler(string commandName, vector<string> commandArgs);
};
