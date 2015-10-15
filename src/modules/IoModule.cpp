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

#include <IoModule.h>
#include <Logger.h>
#include <Utility.h>
#include <Storage.h>
#include <Node.h>

extern "C"{
#include <stdlib.h>
}

IoModule::IoModule(u16 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot)
	: Module(moduleId, node, cm, name, storageSlot)
{
	//Register callbacks n' stuff
	Logger::getInstance().enableTag("IOMOD");

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(IoModuleConfiguration);

	//Start module configuration loading
	LoadModuleConfiguration();
}

void IoModule::ConfigurationLoadedHandler()
{
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if(configuration.moduleVersion == 1){/* ... */};

	//Do additional initialization upon loading the config


	//Start the Module...

}

void IoModule::TimerEventHandler(u16 passedTime, u32 appTimer)
{
	//Do stuff on timer...

}

void IoModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = false;
	configuration.moduleVersion = 1;

	//Set additional config values...

}

bool IoModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	//React on commands, return true if handled, false otherwise
	if(commandArgs.size() >= 2 && commandArgs[1] == moduleName)
	{
		if(commandName == "action")
		{
			if(commandArgs[1] != moduleName) return false;

			nodeID destinationNode = (commandArgs[0] == "this") ? node->persistentConfig.nodeId : atoi(commandArgs[0].c_str());

			if(commandArgs.size() >= 5 && commandArgs[2] == "set")
			{
				u8 requestHandle = commandArgs.size() >= 6 ? atoi(commandArgs[5].c_str()) : 0;

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					IoModuleTriggerActionMessages::SET_IO,
					requestHandle,
					NULL,
					0,
					false
				);

				return true;
			}
			else if(commandArgs.size() == 3 && commandArgs[2] == "get")
			{
				u8 requestHandle = commandArgs.size() >= 6 ? atoi(commandArgs[5].c_str()) : 0;

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					IoModuleTriggerActionMessages::GET_IO,
					requestHandle,
					NULL,
					0,
					false
				);

				return true;
			}

			return false;

		}
	}

	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandName, commandArgs);
}


void IoModule::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	//Must call superclass for handling
	Module::ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){
			if(packet->actionType == IoModuleTriggerActionMessages::SET_IO){
				node->LedRed->On();
				node->LedGreen->On();
				node->LedBlue->On();
			}
		}
	}

	//Parse Module responses
	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_ACTION_RESPONSE){
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId)
		{
			if(packet->actionType == IoModuleActionResponseMessages::SET_IO_RESULT)
			{

			}
		}
	}
}

