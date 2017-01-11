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


EnrollmentModule::EnrollmentModule(u8 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot)
	: Module(moduleId, node, cm, name, storageSlot)
{
	//Register callbacks n' stuff
	Logger::getInstance().enableTag("ENROLLMOD");

	rebootTimeDs = 0;

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

void EnrollmentModule::TimerEventHandler(u16 passedTimeDs, u32 appTimerDs)
{
	if(rebootTimeDs != 0 && rebootTimeDs < appTimerDs){
		logt("ENROLLMOD", "Rebooting");
		NVIC_SystemReset();
	}
}

void EnrollmentModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = true;
	configuration.moduleVersion = 1;

	//Set additional config values...

}

bool EnrollmentModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	//React on commands, return true if handled, false otherwise
	if(commandArgs.size() >= 2 && commandArgs[1] == moduleName)
	{
		nodeID receiver = commandArgs[0] == "this" ? node->persistentConfig.nodeId : atoi(commandArgs[0].c_str());

		if(commandName == "action")
		{
			//If we know the previous id of the node, we can address it with this
			if(commandArgs.size() >= 5 && commandArgs[2] == "nodeid")
			{
				nodeID futureNodeId = atoi(commandArgs[3].c_str());
				networkID networkId = atoi(commandArgs[4].c_str());


				//Build enrollment packet
				u8 buffer[SIZEOF_CONN_PACKET_MODULE + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_NODE_ID_MESSAGE];
				connPacketModule* packet = (connPacketModule*)buffer;
				EnrollmentModuleSetEnrollmentByNodeIdMessage* enrollmentMessage = (EnrollmentModuleSetEnrollmentByNodeIdMessage*)packet->data;

				packet->header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
				packet->header.sender = node->persistentConfig.nodeId;
				packet->header.receiver = receiver;

				packet->moduleId = moduleId;
				packet->actionType = EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT_BY_NODE_ID;
				packet->requestHandle = commandArgs.size() >= 6 ? atoi(commandArgs[5].c_str()) : 0;

				enrollmentMessage->networkId = networkId;
				enrollmentMessage->nodeId = futureNodeId;

				//If a network key is given, set it
				if(commandArgs.size() > 5){
					u8 networkKey[16];
					Logger::getInstance().parseHexStringToBuffer(commandArgs[5].c_str(), enrollmentMessage->networkKey, 16);
				}

				cm->SendMessageToReceiver(NULL, buffer, SIZEOF_CONN_PACKET_MODULE + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_NODE_ID_MESSAGE, true);

				return true;
			}
			//If it has a random id, we can broadcast an enrollment packet and use the chipid to address the node
			//Example: action 0 enroll chipid [chipIdA] [chipIdB] [futureNodeId] [furutreNetworkId]
			else if(commandArgs.size() >= 7 && commandArgs[2] == "chipid")
			{
				u32 chipIdA = strtoul(commandArgs[3].c_str(), NULL, 10);
				u32 chipIdB = strtoul(commandArgs[4].c_str(), NULL, 10);

				nodeID futureNodeId = atoi(commandArgs[5].c_str());
				networkID networkId = atoi(commandArgs[6].c_str());

				//Build enrollment packet
				u8 buffer[SIZEOF_CONN_PACKET_MODULE + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_CHIP_ID_MESSAGE];
				connPacketModule* packet = (connPacketModule*)buffer;
				EnrollmentModuleSetEnrollmentByChipIdMessage* enrollmentMessage = (EnrollmentModuleSetEnrollmentByChipIdMessage*)packet->data;

				packet->header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
				packet->header.sender = node->persistentConfig.nodeId;
				packet->header.receiver = receiver;

				packet->moduleId = moduleId;
				packet->actionType = EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT_BY_CHIP_ID;
				packet->requestHandle = commandArgs.size() >= 8 ? atoi(commandArgs[7].c_str()) : 0;

				enrollmentMessage->chipIdA = chipIdA;
				enrollmentMessage->chipIdB = chipIdB;

				enrollmentMessage->nodeId = futureNodeId;
				enrollmentMessage->networkId = networkId;

				//If a network key is given, set it
				if(commandArgs.size() > 7){
					u8 networkKey[16];
					Logger::getInstance().parseHexStringToBuffer(commandArgs[7].c_str(), enrollmentMessage->networkKey, 16);
				}

				cm->SendMessageToReceiver(NULL, buffer, SIZEOF_CONN_PACKET_MODULE + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_CHIP_ID_MESSAGE, true);

				return true;
			}
			//Enroll by serial number
			else if(commandArgs.size() >= 6 && commandArgs[2] == "serial")
			{
				nodeID newNodeId = atoi(commandArgs[4].c_str());
				networkID newNetworkId = atoi(commandArgs[5].c_str());


				//Build enrollment packet
				u8 buffer[SIZEOF_CONN_PACKET_MODULE + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_SERIAL_MESSAGE];
				connPacketModule* packet = (connPacketModule*)buffer;
				EnrollmentModuleSetEnrollmentBySerialMessage* enrollmentMessage = (EnrollmentModuleSetEnrollmentBySerialMessage*)packet->data;

				packet->header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
				packet->header.sender = node->persistentConfig.nodeId;
				packet->header.receiver = receiver;

				packet->moduleId = moduleId;
				packet->actionType = EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT_BY_SERIAL;
				packet->requestHandle = commandArgs.size() >= 7 ? atoi(commandArgs[6].c_str()) : 0;

				memcpy(enrollmentMessage->serialNumber, commandArgs[3].c_str(), SERIAL_NUMBER_LENGTH);
				enrollmentMessage->newNodeId = newNodeId;
				enrollmentMessage->newNetworkId = newNetworkId;

				//If a network key is given, set it
				if(commandArgs.size() > 6){
					u8 networkKey[16];
					Logger::getInstance().parseHexStringToBuffer(commandArgs[6].c_str(), enrollmentMessage->newNetworkKey, 16);
				}

				cm->SendMessageToReceiver(NULL, buffer, SIZEOF_CONN_PACKET_MODULE + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_SERIAL_MESSAGE, true);

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
		connPacketModule* packet = (connPacketModule*)packetHeader;

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
				node->currentLedMode = ledMode::LED_MODE_OFF;
				LedRed->Off();
				LedGreen->On();
				LedBlue->Off();

				SendEnrollmentResponse(NODE_ID_BROADCAST, enrollmentMethods::BY_NODE_ID, packet->requestHandle, 0, (u8*)Config->serialNumber);

			}
			else if(packet->actionType == EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT_BY_CHIP_ID)
			{
				EnrollmentModuleSetEnrollmentByChipIdMessage* data = (EnrollmentModuleSetEnrollmentByChipIdMessage*)packet->data;

				if(data->chipIdA == NRF_FICR->DEVICEID[0] && data->chipIdB == NRF_FICR->DEVICEID[1])
				{
					logt("ENROLLMOD", "Enrollment (by chipId) received nodeId:%u, networkid:%u, key[0]=%u, key[10]=%u, key[15]=%u", data->nodeId, data->networkId, data->networkKey[0], data->networkKey[10], data->networkKey[15]);

					//Stop all meshing
					node->Stop();

					//Save values to persistent config
					node->persistentConfig.nodeId = data->nodeId;
					node->persistentConfig.networkId = data->networkId;
					memcpy(&node->persistentConfig.networkKey, data->networkKey, 16);

					node->SaveConfiguration();

					//Switch to green LED, user must now reboot the node
					node->currentLedMode = ledMode::LED_MODE_OFF;
					LedRed->Off();
					LedGreen->On();
					LedBlue->Off();


					SendEnrollmentResponse(NODE_ID_BROADCAST, enrollmentMethods::BY_CHIP_ID, packet->requestHandle, 0, (u8*)Config->serialNumber);
				}
			}
			//If an enrollment by serial is received
			else if(packet->actionType == EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT_BY_SERIAL)
			{
				EnrollmentModuleSetEnrollmentBySerialMessage* data = (EnrollmentModuleSetEnrollmentBySerialMessage*)packet->data;

				if(memcmp(data->serialNumber, Config->serialNumber, SERIAL_NUMBER_LENGTH) == 0)
				{
					logt("ENROLLMOD", "Enrollment (by serial) received nodeId:%u, networkid:%u, key[0]=%u, key[10]=%u, key[15]=%u", data->newNodeId, data->newNetworkId, data->newNetworkKey[0], data->newNetworkKey[10], data->newNetworkKey[15]);

					//Stop all meshing
					node->Stop();

					//Save values to persistent config
					node->persistentConfig.nodeId = data->newNodeId;
					node->persistentConfig.networkId = data->newNetworkId;
					memcpy(&node->persistentConfig.networkKey, data->newNetworkKey, 16);

					node->SaveConfiguration();

					//Switch to green LED, user must now reboot the node
					node->currentLedMode = ledMode::LED_MODE_OFF;
					LedRed->On();
					LedGreen->On();
					LedBlue->On();

					//FIXME: Should only send response after enrollment is saved
					SendEnrollmentResponse(NODE_ID_BROADCAST, enrollmentMethods::BY_SERIAL, packet->requestHandle, 0, (u8*)Config->serialNumber);

					//FIXME: Hotfix until NewStorage supports page swapping
					//We wait some time until the enrollment is saved
					rebootTimeDs = node->appTimerDs + SEC_TO_DS(8);
				}
			}
		}
	}

	//Parse Module responses
	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_ACTION_RESPONSE){
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId)
		{
			if(packet->actionType == EnrollmentModuleActionResponseMessages::ENROLLMENT_SUCCESSFUL)
			{
				EnrollmentModuleEnrollmentResponse* data = (EnrollmentModuleEnrollmentResponse*)packet->data;


				const char* enrollmentMethodString = "";
				if(data->enrollmentMethod == enrollmentMethods::BY_NODE_ID) enrollmentMethodString = "node_id";
				else if(data->enrollmentMethod == enrollmentMethods::BY_CHIP_ID) enrollmentMethodString = "chip_id";
				else if(data->enrollmentMethod == enrollmentMethods::BY_SERIAL) enrollmentMethodString = "serial";

				//Add null terminator to string
				u8 serialNumber[SERIAL_NUMBER_LENGTH+1];
				memcpy(serialNumber, data->serialNumber, SERIAL_NUMBER_LENGTH);
				serialNumber[SERIAL_NUMBER_LENGTH] = '\0';

				uart("ENROLLMOD", "{\"nodeId\":%u,\"type\":\"enroll_response_%s\",\"module\":%d,", packet->header.sender, enrollmentMethodString, moduleId);
				uart("ENROLLMOD", "\"requestId\":%u,\"serialNumber\":\"%s\"}" SEP,  packet->requestHandle, serialNumber);
			}
		}
	}
}

void EnrollmentModule::SendEnrollmentResponse(nodeID receiver, u8 enrollmentMethod, u8 requestHandle, u8 result, u8* serialNumber)
{
	//Inform the sender, that the enrollment was successful
	u8 buffer[SIZEOF_CONN_PACKET_MODULE + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_RESPONSE];
	connPacketModule* packet = (connPacketModule*)buffer;
	EnrollmentModuleEnrollmentResponse* data = (EnrollmentModuleEnrollmentResponse*)packet->data;

	packet->header.messageType = MESSAGE_TYPE_MODULE_ACTION_RESPONSE;
	packet->header.sender = node->persistentConfig.nodeId;
	packet->header.receiver = receiver;

	packet->moduleId = moduleId;
	packet->actionType = EnrollmentModuleActionResponseMessages::ENROLLMENT_SUCCESSFUL;
	packet->requestHandle = requestHandle;

	data->result = result;
	data->enrollmentMethod = enrollmentMethod;
	memcpy(data->serialNumber, Config->serialNumber, SERIAL_NUMBER_LENGTH);


	cm->SendMessageToReceiver(NULL, buffer, SIZEOF_CONN_PACKET_MODULE + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_RESPONSE, true);

}
