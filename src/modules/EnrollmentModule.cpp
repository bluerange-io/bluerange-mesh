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
	//React on commands, return true if handled, false otherwise
	if(commandArgs.size() >= 2 && commandArgs[1] == moduleName)
	{
		if(commandName == "uart_module_trigger_action" || commandName == "action")
		{
			//If we know the previous id of the node, we can address it with this
			if(commandArgs.size() == 5 && commandArgs[2] == "nodeid")
			{
				nodeID currentNodeId = atoi(commandArgs[0].c_str());

				nodeID futureNodeId = atoi(commandArgs[3].c_str());
				networkID networkId = atoi(commandArgs[4].c_str());

				//Build enrollment packet
				u8 buffer[SIZEOF_CONN_PACKET_MODULE_ACTION + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_NODE_ID_MESSAGE];
				connPacketModuleAction* packet = (connPacketModuleAction*)buffer;
				EnrollmentModuleSetEnrollmentByNodeIdMessage* enrollmentMessage = (EnrollmentModuleSetEnrollmentByNodeIdMessage*)packet->data;

				packet->header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
				packet->header.sender = node->persistentConfig.nodeId;
				packet->header.receiver = currentNodeId;

				packet->moduleId = moduleId;
				packet->actionType = EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT_BY_NODE_ID;

				enrollmentMessage->networkId = networkId;
				enrollmentMessage->nodeId = futureNodeId;

				cm->SendMessageToReceiver(NULL, buffer, SIZEOF_CONN_PACKET_MODULE_ACTION + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_NODE_ID_MESSAGE, true);

				return true;
			}
			//If it has a random id, we can broadcast an enrollment packet and use the chipid to address the node
			else if(commandArgs.size() == 7 && commandArgs[2] == "chipid")
			{
				u32 chipIdA = strtoul(commandArgs[3].c_str(), NULL, 10);
				u32 chipIdB = strtoul(commandArgs[4].c_str(), NULL, 10);

				nodeID futureNodeId = atoi(commandArgs[5].c_str());
				networkID networkId = atoi(commandArgs[6].c_str());

				//Build enrollment packet
				u8 buffer[SIZEOF_CONN_PACKET_MODULE_ACTION + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_CHIP_ID_MESSAGE];
				connPacketModuleAction* packet = (connPacketModuleAction*)buffer;
				EnrollmentModuleSetEnrollmentByChipIdMessage* enrollmentMessage = (EnrollmentModuleSetEnrollmentByChipIdMessage*)packet->data;

				packet->header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
				packet->header.sender = node->persistentConfig.nodeId;
				packet->header.receiver = NODE_ID_BROADCAST;

				packet->moduleId = moduleId;
				packet->actionType = EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT_BY_CHIP_ID;

				enrollmentMessage->chipIdA = chipIdA;
				enrollmentMessage->chipIdB = chipIdB;

				enrollmentMessage->nodeId = futureNodeId;
				enrollmentMessage->networkId = networkId;

				cm->SendMessageToReceiver(NULL, buffer, SIZEOF_CONN_PACKET_MODULE_ACTION + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_CHIP_ID_MESSAGE, true);

				return true;
			}
		}
	}

	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandName, commandArgs);
}

void EnrollmentModule::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	//Must call superclass for handling
	Module::ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModuleAction* packet = (connPacketModuleAction*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){
			if(packet->actionType == EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT_BY_NODE_ID)
			{
				EnrollmentModuleSetEnrollmentByNodeIdMessage* data = (EnrollmentModuleSetEnrollmentByNodeIdMessage*)packet->data;

				logt("ENROLLMOD", "Enrollment (by nodeId) received nodeId:%u, networkid:%u", data->nodeId, data->networkId);

				//Stop all meshing
				node->Stop();

				//Save values to persistent config
				node->persistentConfig.nodeId = data->nodeId;
				node->persistentConfig.networkId = data->networkId;

				node->SaveConfiguration();

				//Switch to green LED, user must now reboot the node
				node->currentLedMode = Node::ledMode::LED_MODE_OFF;
				node->LedRed->Off();
				node->LedGreen->On();
				node->LedBlue->Off();

			}
			else if(packet->actionType == EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT_BY_CHIP_ID)
			{
				EnrollmentModuleSetEnrollmentByChipIdMessage* data = (EnrollmentModuleSetEnrollmentByChipIdMessage*)packet->data;

				if(data->chipIdA == NRF_FICR->DEVICEID[0] && data->chipIdB == NRF_FICR->DEVICEID[1])
				{
					logt("ENROLLMOD", "Enrollment (by chipId) received nodeId:%u, networkid:%u", data->nodeId, data->networkId);

					//Stop all meshing
					node->Stop();

					//Save values to persistent config
					node->persistentConfig.nodeId = data->nodeId;
					node->persistentConfig.networkId = data->networkId;

					node->SaveConfiguration();

					//Switch to green LED, user must now reboot the node
					node->currentLedMode = Node::ledMode::LED_MODE_OFF;
					node->LedRed->Off();
					node->LedGreen->On();
					node->LedBlue->Off();
				}
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
