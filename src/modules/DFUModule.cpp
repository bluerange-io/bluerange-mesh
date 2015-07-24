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

#include <DFUModule.h>
#include <Utility.h>
#include <Storage.h>

extern "C"{
#include <ble.h>
#include <ble_dfu.h>
#include <app_error.h>
}


#define DFU_REV_MAJOR                    0x00                                       /** DFU Major revision number to be exposed. */
#define DFU_REV_MINOR                    0x01                                       /** DFU Minor revision number to be exposed. */
#define DFU_REVISION                     ((DFU_REV_MAJOR << 8) | DFU_REV_MINOR)     /** DFU Revision number to be exposed. Combined of major and minor versions. */


//The DFU module does not work and is unfinished, it should enable DFU updates
//by directly connecting to the DFU service and in a later stage a DFU update over the mesh

ble_dfu_init_t DFUModule::dfus_init;
ble_dfu_t DFUModule::m_dfus;

DFUModule::DFUModule(u16 moduleId, Node* node, ConnectionManager* cm, char* name, u16 storageSlot)
	: Module(moduleId, node, cm, name, storageSlot)
{
	//Register callbacks n' stuff
	Logger::getInstance().enableTag("DFU");

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(DFUModuleConfiguration);

	ResetToDefaultConfiguration();

	//Start module configuration loading
	LoadModuleConfiguration();
}

void DFUModule::ConfigurationLoadedHandler()
{
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if(configuration.moduleVersion == 1){/* ... */};

	//Do additional initialization upon loading the config


	u32 err;

	// Initialize the Device Firmware Update Service.
	memset(&dfus_init, 0, sizeof(dfus_init));
	dfus_init.evt_handler   = AppDFUHandler;
	dfus_init.error_handler = NULL;
	dfus_init.evt_handler   = AppDFUHandler;
	dfus_init.revision      = DFU_REVISION;
	err = ble_dfu_init(&m_dfus, &dfus_init);
	APP_ERROR_CHECK(err);

}



void DFUModule::TimerEventHandler(u16 passedTime, u32 appTimer){


}

void DFUModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = false;
	configuration.moduleVersion = 1;
}

void DFUModule::BleEventHandler(ble_evt_t* bleEvent)
{
	ble_dfu_on_ble_evt(&m_dfus, bleEvent);
}

void DFUModule::AppDFUHandler(ble_dfu_t* p_dfu, ble_dfu_evt_t* DFUEvent)
{
	logt("DFU", "Event connHandle:%d", p_dfu->conn_handle);
}

bool DFUModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	//Must be called to allow the module to get and set the config
	Module::TerminalCommandHandler(commandName, commandArgs);

	if (commandName == "DFU")
	{
		return true;

	}
	else
	{
		return false;
	}

}
