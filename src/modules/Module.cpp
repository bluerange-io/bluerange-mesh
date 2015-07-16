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

#include <Module.h>
#include <Storage.h>
#include <Node.h>

extern "C"{
#include <stdlib.h>
}


Module::Module(u16 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot)
{
	this->node = node;
	this->cm = cm;
	this->moduleId  = moduleId;
	this->storageSlot = storageSlot;
	this->configurationPointer = NULL;
	this->configurationLength = 0;
	memcpy(moduleName, name, MODULE_NAME_MAX_SIZE);

	Terminal::AddTerminalCommandListener(this);
}

Module::~Module()
{
}


void Module::SaveModuleConfiguration()
{
	Storage::getInstance().QueuedWrite((u8*)configurationPointer, configurationLength, storageSlot, this);
}

void Module::LoadModuleConfiguration()
{
	if(Config->ignorePersistentConfigurationOnBoot){
		//Invalidate config and load default
		configurationPointer->moduleId = 0xFF;
		ResetToDefaultConfiguration();
		ConfigurationLoadedHandler();
	} else {
		//Start to load the saved configuration
		Storage::getInstance().QueuedRead((u8*)configurationPointer, configurationLength, storageSlot, this);
	}

}

void Module::ConfigurationLoadedHandler()
{
	//Configuration is invalid because the device got flashed
	if(configurationPointer->moduleId == 0xFF)
	{
		logt("MODULE", "Config resetted to default");
		ResetToDefaultConfiguration();
	}
	//Configuration has been saved by another module or is wrong
	else if (configurationPointer->moduleId != moduleId)
	{
		logt("ERROR", "Wrong module conf loaded %d vs %d", configurationPointer->moduleId, moduleId);
	}
	//Config-> loaded and ok
	else {
		logt("MODULE", "Module config loaded version:%d", configurationPointer->version);
	}
}



bool Module::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	//If somebody wants to set the module config over uart, he's welcome
	//First, check if our module is meant
	if(commandArgs.size() > 1 && strcmp(moduleName, commandArgs[1].c_str()) == 0)
	{
		//E.g. UART_MODULE_SET_CONFIG 0 STATUS 00:FF:A0 => command, nodeId (this for current node), moduleId, hex-string
		if(commandName == "UART_MODULE_SET_CONFIG" && commandArgs.size() == 3)
		{
			if(commandArgs[0] == "this")
			{
				logt("ERROR", "This is module %s setting config", moduleName);

				//HexString must be like "FF:34:67:22:24"
				const char* hexString = commandArgs[2].c_str();
				u32 length = (commandArgs[2].length()+1)/3;
				u8 buffer[200];
				for(u32 i = 0; i<length; i++){
					buffer[i] = (u8)strtoul(hexString + (i*3), NULL, 16);
				}
				//Check if this config seems right
				ModuleConfiguration* newConfig = (ModuleConfiguration*)buffer;
				if(
						newConfig->version == configurationPointer->version
						&& length == configurationLength
				){
					newConfig->moduleId = configurationPointer->moduleId; //ModuleID must not be transmitted
					logt("ERROR", "Config set");
					memcpy(configurationPointer, buffer, configurationLength);

					ConfigurationLoadedHandler();
				}
				else
				{
					logt("ERROR", "Wrong configuration length:%u vs. %u or version:%d vs %d", length, configurationLength, newConfig->version, configurationPointer->version);
				}
			}
			//Send command to other node
			else
			{
				connPacketModuleRequest packet;
				packet.header.messageType = MESSAGE_TYPE_MODULE_REQUEST;
				packet.header.sender = node->persistentConfig.nodeId;
				packet.header.receiver = atoi(commandArgs[0].c_str());

				packet.moduleId = moduleId;
				packet.moduleRequestType = moduleRequestTypes::MODULE_SET_CONFIGURATION;

				//TODO:Send packet with variable length, and config

			}

		}
		else if(commandName == "UART_MODULE_GET_CONFIG")
		{
			if(commandArgs[0] == "this")
			{
				char* buffer[200];
				Logger::getInstance().convertBufferToHexString((u8*)configurationPointer, configurationLength, (char*)buffer);

				uart("MODULE", "{\"name\":\"%s\", \"config\":\"%s\"}", moduleName, buffer);
			}
			//It's a nodeID, we must send the get_config command to another module
			else
			{
				connPacketModuleRequest packet;
				packet.header.messageType = MESSAGE_TYPE_MODULE_REQUEST;
				packet.header.sender = node->persistentConfig.nodeId;
				packet.header.receiver = atoi(commandArgs[0].c_str());

				packet.moduleId = moduleId;
				packet.moduleRequestType = moduleRequestTypes::MODULE_GET_CONFIGURATION;

				//TODO:Send packet with variable length, leave version field empty
			}
		}
		else if(commandName == "UART_MODULE_SET_ACTIVE")
		{
			if(commandArgs[0] == "this")
			{
				if(commandArgs.size() > 2 && commandArgs[2] == "0"){
					configurationPointer->moduleActive = false;
				} else if(commandArgs.size() > 2 && commandArgs[2] == "1"){
					configurationPointer->moduleActive = true;
				} else {
					configurationPointer->moduleActive = !configurationPointer->moduleActive;
				}
			}
			//Send command to another node
			else
			{
				connPacketModuleRequest packet;
				packet.header.messageType = MESSAGE_TYPE_MODULE_REQUEST;
				packet.header.sender = node->persistentConfig.nodeId;
				packet.header.receiver = atoi(commandArgs[0].c_str());

				packet.moduleId = moduleId;
				packet.moduleRequestType = moduleRequestTypes::MODULE_SET_ACTIVE;
				packet.data[0] = (commandArgs.size() < 3 || commandArgs[2] == "1") ? 1 : 0;

				cm->SendMessageToReceiver(NULL, (u8*) &packet, SIZEOF_CONN_PACKET_MODULE_REQUEST, true);
			}
		}

	} else {
		return false;
	}
	return true;
}

void Module::ConnectionPacketReceivedEventHandler(ble_evt_t* bleEvent, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	//We want to handle incoming packets that change the module configuration
	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_REQUEST)
	{
		connPacketModuleRequest* packet = (connPacketModuleRequest*) packetHeader;
		if(packet->moduleId != moduleId) return;

		if(packet->moduleRequestType == moduleRequestTypes::MODULE_SET_CONFIGURATION){
			//Save the received configuration to the module configuration


		} else if(packet->moduleRequestType == moduleRequestTypes::MODULE_GET_CONFIGURATION){
			//Send the module configuration


		} else if(packet->moduleRequestType == moduleRequestTypes::MODULE_SET_ACTIVE){
			//Look for the module and set it active or inactive
			for(u32 i=0; i<MAX_MODULE_COUNT; i++){
				if(node->activeModules[i] && node->activeModules[i]->moduleId == packet->moduleId){
					logt("MODULE", "Status set to:%u", packet->data[0]);
					node->activeModules[i]->configurationPointer->moduleActive = packet->data[0];
				}
			}
		}
	}
}
