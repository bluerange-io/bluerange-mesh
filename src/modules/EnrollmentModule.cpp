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

#include <EnrollmentModule.h>
#include <Logger.h>
#include <Utility.h>
#include <Storage.h>
#include <Node.h>

extern "C"{
#include <stdlib.h>
}

/*
Module purpose:
After a node has been flashed, it is in an unconfigured state
This module should allow configuration of network id, network key, nodeID and other necessary parameters
 */


EnrollmentModule::EnrollmentModule(u16 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot)
	: Module(moduleId, node, cm, name, storageSlot)
{
	//Register callbacks n' stuff
	Logger::getInstance().enableTag("ENROLLMOD");

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(EnrollmentModuleConfiguration);

	//Start module configuration loading
	LoadModuleConfiguration();
}

void EnrollmentModule::ConfigurationLoadedHandler()
{
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if(configuration.moduleVersion == 1){/* ... */};

	//Do additional initialization upon loading the config


	//Start the Module...

}

void EnrollmentModule::TimerEventHandler(u16 passedTime, u32 appTimer)
{
	//Do stuff on timer...

}

void EnrollmentModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = false;
	configuration.moduleVersion = 1;

	//Set additional config values...

}

bool EnrollmentModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	//Must be called to allow the module to get and set the config
	Module::TerminalCommandHandler(commandName, commandArgs);

	//React on commands, return true if handled, false otherwise
	if(commandName == "uart_module_trigger_action")
	{
		if(commandArgs[1] != moduleName) return false;

		if(commandArgs.size() == 5 && commandArgs[2] == "enroll")
		{
			nodeID currentNodeId = atoi(commandArgs[0].c_str());

			nodeID futureNodeId = atoi(commandArgs[3].c_str());
			networkID networkId = atoi(commandArgs[4].c_str());

			//Build enrollment packet
			u8 buffer[SIZEOF_CONN_PACKET_MODULE_ACTION + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_MESSAGE];
			connPacketModuleAction* packet = (connPacketModuleAction*)buffer;
			EnrollmentModuleSetEnrollmentMessage* enrollmentMessage = (EnrollmentModuleSetEnrollmentMessage*)packet->data;

			packet->header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
			packet->header.sender = node->persistentConfig.nodeId;
			packet->header.receiver = currentNodeId;

			packet->moduleId = moduleId;
			packet->actionType = EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT;

			enrollmentMessage->networkId = networkId;
			enrollmentMessage->nodeId = futureNodeId;

			cm->SendMessageToReceiver(NULL, buffer, SIZEOF_CONN_PACKET_MODULE_ACTION + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_MESSAGE, true);


			return true;
		}
		else if(commandArgs.size() == 3 && commandArgs[2] == "argument_b")
		{


			return true;
		}

		return true;

	}

	return false;
}

void EnrollmentModule::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	//Must call superclass for handling
	Module::ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModuleAction* packet = (connPacketModuleAction*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){
			if(packet->actionType == EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT){
				EnrollmentModuleSetEnrollmentMessage* data = (EnrollmentModuleSetEnrollmentMessage*)packet->data;

				//Stop all meshing
				node->Stop();

				//Save values to persistent config
				node->persistentConfig.nodeId = data->nodeId;
				node->persistentConfig.networkId = data->networkId;

				node->SaveConfiguration();

				logt("ENROLLMOD", "Enrollment received nodeId:%u, networkid:%u", data->nodeId, data->networkId);

			}
		}
	}

	//Parse Module responses
	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_ACTION_RESPONSE){
		connPacketModuleAction* packet = (connPacketModuleAction*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId)
		{
			/*if(packet->actionType == EnrollmentModuleTriggerActionMessages::MESSAGE)
			{

			}*/
		}
	}
}
