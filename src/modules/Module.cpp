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

	Logger::getInstance().enableTag("MODULE");
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
	if(Config->ignorePersistentModuleConfigurationOnBoot){
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
		logt("MODULE", "Module %u config loaded version:%d", moduleId, configurationPointer->moduleVersion);
	}
}



bool Module::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	//If somebody wants to set the module config over uart, he's welcome
	//First, check if our module is meant
	if(commandArgs.size() >= 2 && commandArgs[1] == moduleName)
	{
		nodeID receiver = commandArgs[0] == "this" ? node->persistentConfig.nodeId : atoi(commandArgs[0].c_str());

		//E.g. UART_MODULE_SET_CONFIG 0 STATUS 00:FF:A0 => command, nodeId (this for current node), moduleId, hex-string
		if(commandName == "uart_module_set_config" || commandName == "setconf")
		{
			if(commandArgs.size() == 3)
			{
				//calculate configuration size
				const char* configString = commandArgs[2].c_str();
				u16 configLength = (commandArgs[2].length()+1)/3;

				u8 packetBuffer[configLength + SIZEOF_CONN_PACKET_MODULE_REQUEST];
				connPacketModuleRequest* packet = (connPacketModuleRequest*)packetBuffer;
				packet->header.messageType = MESSAGE_TYPE_MODULE_SET_CONFIGURATION;
				packet->header.sender = node->persistentConfig.nodeId;
				packet->header.receiver = receiver;

				packet->moduleId = moduleId;
				//Fill data region with module config
				Logger::getInstance().parseHexStringToBuffer(configString, packet->data, configLength);


				cm->SendMessageToReceiver(NULL, packetBuffer, configLength + SIZEOF_CONN_PACKET_MODULE_REQUEST, true);

				return true;
			}

		}
		else if(commandName == "uart_module_get_config" || commandName == "getconf")
		{
			if(commandArgs.size() == 2 && commandArgs[0] == "this")
			{
				char* buffer[200];
				Logger::getInstance().convertBufferToHexString((u8*)configurationPointer, configurationLength, (char*)buffer);

				uart("MODULE", "{\"name\":\"%s\", \"config\":\"%s\"}" SEP, moduleName, buffer);

				return true;
			}
			//It's a nodeID, we must send the get_config command to another module
			//FIXME: Not yet supported
			else if(commandArgs.size() == 2)
			{
				connPacketModuleRequest packet;
				packet.header.messageType = MESSAGE_TYPE_MODULE_GET_CONFIGURATION;
				packet.header.sender = node->persistentConfig.nodeId;
				packet.header.receiver = receiver;

				packet.moduleId = moduleId;

				//TODO:Send config packet with variable length, leave version field empty

				return true;
			}
		}
		else if(commandName == "uart_module_set_active" || commandName == "setactive")
		{
			connPacketModuleRequest packet;
			packet.header.messageType = MESSAGE_TYPE_MODULE_SET_ACTIVE;
			packet.header.sender = node->persistentConfig.nodeId;
			packet.header.receiver = receiver;

			packet.moduleId = moduleId;
			packet.data[0] = (commandArgs.size() < 3 || commandArgs[2] == "1") ? 1 : 0;

			cm->SendMessageToReceiver(NULL, (u8*) &packet, SIZEOF_CONN_PACKET_MODULE_REQUEST+1, true);

			return true;
		}

	}


	return false;
}

void Module::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	//We want to handle incoming packets that change the module configuration
	if(
			packetHeader->messageType == MESSAGE_TYPE_MODULE_GET_CONFIGURATION
			|| packetHeader->messageType == MESSAGE_TYPE_MODULE_SET_CONFIGURATION
			|| packetHeader->messageType == MESSAGE_TYPE_MODULE_SET_ACTIVE
	)
	{
		connPacketModuleRequest* packet = (connPacketModuleRequest*) packetHeader;
		if(packet->moduleId != moduleId) return;

		if(packetHeader->messageType == MESSAGE_TYPE_MODULE_SET_CONFIGURATION)
		{
			u16 configLength = inPacket->dataLength - SIZEOF_CONN_PACKET_MODULE_REQUEST;

			//Save the received configuration to the module configuration
			char* buffer[200];
			Logger::getInstance().convertBufferToHexString((u8*)packet->data, configLength, (char*)buffer);

			logt("MODULE", "rx (%d): %s", configLength,  buffer);

			//Check if this config seems right
			ModuleConfiguration* newConfig = (ModuleConfiguration*)packet->data;
			if(
					newConfig->moduleVersion == configurationPointer->moduleVersion
					&& configLength == configurationLength
			){
				//Backup the module id because it must not be sent in the packet
				u16 moduleId = configurationPointer->moduleId;
				logt("MODULE", "Config set");
				memcpy(configurationPointer, packet->data, configurationLength);
				configurationPointer->moduleId = moduleId;

				//TODO: Save


				ConfigurationLoadedHandler();
			}
			else
			{
				if(newConfig->moduleVersion != configurationPointer->moduleVersion) uart("ERROR", "{\"module\":%u, \"type\":\"error\", \"code\":1, \"text\":\"wrong config version. \"}" SEP, moduleId);
				else uart("ERROR", "{\"module\":%u, \"type\":\"error\", \"code\":2, \"text\":\"wrong configuration length. \"}" SEP, moduleId);
			}
		}
		else if(packetHeader->messageType == MESSAGE_TYPE_MODULE_GET_CONFIGURATION)
		{
			//TODO: Send the module configuration


		}
		else if(packetHeader->messageType == MESSAGE_TYPE_MODULE_SET_ACTIVE)
		{
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
