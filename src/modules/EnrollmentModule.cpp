/**

Copyright (c) 2014-2017 "M-Way Solutions GmbH"
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
#include <Node.h>
#include <IoModule.h>

extern "C"{
#include <stdlib.h>
#ifndef SIM_ENABLED
#include <nrf_nvic.h>
#endif
}

/*
Module purpose:
After a node has been flashed, it is in an unconfigured state
This module should allow configuration of network id, network key, nodeID and other necessary parameters
 */


EnrollmentModule::EnrollmentModule(u8 moduleId, Node* node, ConnectionManager* cm, const char* name)
	: Module(moduleId, node, cm, name)
{
	moduleVersion = 1;

	//Register callbacks n' stuff

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
	if(configuration.moduleVersion == this->moduleVersion){/* ... */};

	//Do additional initialization upon loading the config


	//Start the Module...

}

void EnrollmentModule::TimerEventHandler(u16 passedTimeDs, u32 appTimerDs)
{
	if(rebootTimeDs != 0 && rebootTimeDs < appTimerDs){
		logt("ENROLLMOD", "Rebooting");
		//Do not reboot in safe mode
		*GS->rebootMagicNumberPtr = REBOOT_MAGIC_NUMBER;
		FruityHal::SystemReset();
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

bool EnrollmentModule::TerminalCommandHandler(std::string commandName, std::vector<std::string> commandArgs)
{
	//React on commands, return true if handled, false otherwise
	if(commandArgs.size() >= 2 && commandArgs[1] == moduleName)
	{
		nodeID receiver = commandArgs[0] == "this" ? node->persistentConfig.nodeId : atoi(commandArgs[0].c_str());

		if(commandName == "action")
		{
			//Enroll by serial number
			if(commandArgs.size() >= 6 && commandArgs[2] == "serial")
			{
				nodeID newNodeId = atoi(commandArgs[4].c_str());
				networkID newNetworkId = atoi(commandArgs[5].c_str());


				//Build enrollment packet
				EnrollmentModuleSetEnrollmentBySerialMessage enrollmentMessage;
				memset(&enrollmentMessage, 0x00, SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_SERIAL_MESSAGE);

				memcpy(enrollmentMessage.serialNumber, commandArgs[3].c_str(), NODE_SERIAL_NUMBER_LENGTH);
				enrollmentMessage.newNodeId = newNodeId;
				enrollmentMessage.newNetworkId = newNetworkId;

				//If a network key is given, set it
				u8 networkKeySize = 0;
				if(commandArgs.size() > 6 && commandArgs[6].length() == 47){
					Logger::getInstance()->parseHexStringToBuffer(commandArgs[6].c_str(), enrollmentMessage.newNetworkKey, 16);
					networkKeySize = 16;
				} else {
					logt("ERROR", "No networkKey was given or incorrect key, setting to 00...00");
				}

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					receiver,
					EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT_BY_SERIAL,
					commandArgs.size() >= 7 ? atoi(commandArgs[6].c_str()) : 0,
					(u8*)&enrollmentMessage,
					SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_SERIAL_MESSAGE,
					false
				);

				return true;
			}
		}
	}

	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandName, commandArgs);
}

void EnrollmentModule::MeshMessageReceivedHandler(MeshConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader)
{
	//Must call superclass for handling
	Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){
			//If an enrollment by serial is received
			if(packet->actionType == EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT_BY_SERIAL)
			{
				EnrollmentModuleSetEnrollmentBySerialMessage* data = (EnrollmentModuleSetEnrollmentBySerialMessage*)packet->data;

				if(memcmp(data->serialNumber, Config->serialNumber, NODE_SERIAL_NUMBER_LENGTH) == 0)
				{
					logt("ERROR", "Enrollment (by serial) received nodeId:%u, networkid:%u, key[0]=%u, key[1]=%u, key[14]=%u, key[15]=%u", data->newNodeId, data->newNetworkId, data->newNetworkKey[0], data->newNetworkKey[1], data->newNetworkKey[14], data->newNetworkKey[15]);
					
					//Save values to persistent config
					node->persistentConfig.nodeId = data->newNodeId;
					node->persistentConfig.networkId = data->newNetworkId;
					memcpy(&node->persistentConfig.networkKey, data->newNetworkKey, 16);

					logt("ERROR", "new key is %02x:%02x..%02x:%02x",
							node->persistentConfig.networkKey[0],
							node->persistentConfig.networkKey[1],
							node->persistentConfig.networkKey[14],
							node->persistentConfig.networkKey[15]);

					//Cache some of the data in a struct to have it available when it is saved
					SaveEnrollmentAction userData;
					userData.sender = packetHeader->sender;
					userData.enrollmentMethod = enrollmentMethods::BY_SERIAL;
					userData.requestHandle = packet->requestHandle;

					//TODO: Currently, the enrollment is saved in the node settings, save it to the enrollment module settings
					GS->recordStorage->SaveRecord(
						moduleID::NODE_ID,
						(u8*)&(node->persistentConfig),
						sizeof(NodeConfiguration),
						this,
						EnrollmentModuleSaveActions::SAVE_ENROLLMENT_ACTION,
						(u8*)&userData,
						sizeof(SaveEnrollmentAction)
					);
					
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

				//Add null terminator to string
				u8 serialNumber[NODE_SERIAL_NUMBER_LENGTH +1];
				memcpy(serialNumber, data->serialNumber, NODE_SERIAL_NUMBER_LENGTH);
				serialNumber[NODE_SERIAL_NUMBER_LENGTH] = '\0';

				logjson("ENROLLMOD", "{\"nodeId\":%u,\"type\":\"enroll_response_serial\",\"module\":%d,", packet->header.sender, moduleId);
				logjson("ENROLLMOD", "\"requestId\":%u,\"serialNumber\":\"%s\"}" SEP,  packet->requestHandle, serialNumber);
			}
		}
	}
}

void EnrollmentModule::SendEnrollmentResponse(nodeID receiver, u8 enrollmentMethod, u8 requestHandle, u8 result, u8* serialNumber)
{
	//Inform the sender, that the enrollment was successful
	EnrollmentModuleEnrollmentResponse data;
	data.result = result;
	data.enrollmentMethod = enrollmentMethod;
	memcpy(data.serialNumber, Config->serialNumber, NODE_SERIAL_NUMBER_LENGTH);

	SendModuleActionMessage(
		MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
		receiver,
		EnrollmentModuleActionResponseMessages::ENROLLMENT_SUCCESSFUL,
		requestHandle,
		(u8*)&data,
		SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_RESPONSE,
		true
	);
}

void EnrollmentModule::RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength)
{
	//After an enrollment has been saved, send back the ack
	if (userType == EnrollmentModuleSaveActions::SAVE_ENROLLMENT_ACTION)
	{
		SaveEnrollmentAction* data = (SaveEnrollmentAction*)userData;

		SendEnrollmentResponse(data->sender, data->enrollmentMethod, data->requestHandle, resultCode, (u8*)Config->serialNumber);

		//Enable green light, first switch io module led control off
		IoModule* ioModule = (IoModule*)node->GetModuleById(moduleID::IO_MODULE_ID);
		if (ioModule != NULL) {
			ioModule->currentLedMode = ledMode::LED_MODE_CUSTOM;
		}
		GS->ledRed->Off();
		GS->ledGreen->On();
		GS->ledBlue->Off();

		// Reboot after successful enroll
		//TODO: Should ideally listen for an event that the message has been sent
		rebootTimeDs = node->appTimerDs + SEC_TO_DS(2);
	}
}
