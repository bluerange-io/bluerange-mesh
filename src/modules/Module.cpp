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


Module::Module(u8 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot)
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
		//FIXME: only meant as hotfix, replace with NewStorage
		memcpy((u8*)configurationPointer, (u8*)(PAGE_SIZE*254+storageSlot*STORAGE_BLOCK_SIZE), configurationLength);
		ConfigurationLoadedHandler();
	}

}

void Module::ConfigurationLoadedHandler()
{
	//Configuration is invalid because the device got flashed
	if(configurationPointer->moduleId == 0xFF)
	{
		logt("MODULE", "Config resetted to default for module %d", moduleId);
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

//Constructs a simple trigger action message and can take aditional payload data
void Module::SendModuleActionMessage(u8 messageType, nodeID toNode, u8 actionType, u8 requestHandle, u8* additionalData, u16 additionalDataSize, bool reliable)
{
	u8 buffer[SIZEOF_CONN_PACKET_MODULE + additionalDataSize];

	connPacketModule* outPacket = (connPacketModule*)buffer;
	outPacket->header.messageType = messageType;
	outPacket->header.sender = node->persistentConfig.nodeId;
	outPacket->header.receiver = toNode;

	outPacket->moduleId = moduleId;
	outPacket->requestHandle = requestHandle;
	outPacket->actionType = actionType;

	if(additionalData != NULL && additionalDataSize > 0)
	{
		memcpy(&outPacket->data, additionalData, additionalDataSize);
	}

	cm->SendMessageToReceiver(NULL, buffer, SIZEOF_CONN_PACKET_MODULE + additionalDataSize, reliable);
}

bool Module::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	//If somebody wants to set the module config over uart, he's welcome
	//First, check if our module is meant
	if(commandArgs.size() >= 2 && commandArgs[1] == moduleName)
	{
		nodeID receiver = commandArgs[0] == "this" ? node->persistentConfig.nodeId : atoi(commandArgs[0].c_str());

		//E.g. UART_MODULE_SET_CONFIG 0 STATUS 00:FF:A0 => command, nodeId (this for current node), moduleId, hex-string
		if(commandName == "set_config")
		{
			if(commandArgs.size() >= 3)
			{
				//calculate configuration size
				const char* configString = commandArgs[2].c_str();
				u16 configLength = (commandArgs[2].length()+1)/3;

				u8 requestHandle = commandArgs.size() >= 4 ? atoi(commandArgs[3].c_str()) : 0;

				//Send the configuration to the destination node
				u8 packetBuffer[configLength + SIZEOF_CONN_PACKET_MODULE];
				connPacketModule* packet = (connPacketModule*)packetBuffer;
				packet->header.messageType = MESSAGE_TYPE_MODULE_CONFIG;
				packet->header.sender = node->persistentConfig.nodeId;
				packet->header.receiver = receiver;

				packet->actionType = ModuleConfigMessages::SET_CONFIG;
				packet->moduleId = moduleId;
				packet->requestHandle = requestHandle;
				//Fill data region with module config
				Logger::getInstance().parseHexStringToBuffer(configString, packet->data, configLength);

				cm->SendMessageToReceiver(NULL, packetBuffer, configLength + SIZEOF_CONN_PACKET_MODULE, true);

				return true;
			}

		}
		else if(commandName == "get_config")
		{
			if(commandArgs.size() == 2)
			{
				connPacketModule packet;
				packet.header.messageType = MESSAGE_TYPE_MODULE_CONFIG;
				packet.header.sender = node->persistentConfig.nodeId;
				packet.header.receiver = receiver;

				packet.moduleId = moduleId;
				packet.actionType = ModuleConfigMessages::GET_CONFIG;

				cm->SendMessageToReceiver(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_MODULE, false);

				return true;
			}
		}
		else if(commandName == "set_active")
		{
			u8 moduleState = commandArgs[2] == "on" ? 1: 0;
			u8 requestHandle = commandArgs.size() >= 4 ? atoi(commandArgs[3].c_str()) : 0;

			connPacketModule packet;
			packet.header.messageType = MESSAGE_TYPE_MODULE_CONFIG;
			packet.header.sender = node->persistentConfig.nodeId;
			packet.header.receiver = receiver;

			packet.moduleId = moduleId;
			packet.actionType = ModuleConfigMessages::SET_ACTIVE;
			packet.requestHandle = requestHandle;
			packet.data[0] = moduleState;

			cm->SendMessageToReceiver(NULL, (u8*) &packet, SIZEOF_CONN_PACKET_MODULE + 1, true);

			return true;
		}
	}

	return false;
}

void Module::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	//We want to handle incoming packets that change the module configuration
	if(
			packetHeader->messageType == MESSAGE_TYPE_MODULE_CONFIG
	){
		connPacketModule* packet = (connPacketModule*) packetHeader;
		if(packet->moduleId == moduleId)
		{

			u16 dataFieldLength = inPacket->dataLength - SIZEOF_CONN_PACKET_MODULE;

			if(packet->actionType == ModuleConfigMessages::SET_CONFIG)
			{
				//Log the config to the terminal
				/*char* buffer[200];
				Logger::getInstance().convertBufferToHexString((u8*)packet->data, dataFieldLength, (char*)buffer);
				logt("MODULE", "rx (%d): %s", dataFieldLength,  buffer);*/

				//Check if this config seems right
				ModuleConfiguration* newConfig = (ModuleConfiguration*)packet->data;
				if(
						newConfig->moduleVersion == configurationPointer->moduleVersion
						&& dataFieldLength == configurationLength
				){
					//Backup the module id because it must not be sent in the packet
					u8 moduleId = configurationPointer->moduleId;
					memcpy(configurationPointer, packet->data, configurationLength);
					configurationPointer->moduleId = moduleId;

					//TODO: Maybe, we want to save this configuration, and afterwards send saveOK message

					//Send set_config_ok message
					connPacketModule outPacket;
					outPacket.header.messageType = MESSAGE_TYPE_MODULE_CONFIG;
					outPacket.header.sender = node->persistentConfig.nodeId;
					outPacket.header.receiver = packet->header.sender;

					outPacket.moduleId = moduleId;
					outPacket.requestHandle = packet->requestHandle;
					outPacket.actionType = ModuleConfigMessages::SET_CONFIG_RESULT;
					outPacket.data[0] = NRF_SUCCESS; //Return ok

					cm->SendMessageToReceiver(NULL, (u8*) &outPacket, SIZEOF_CONN_PACKET_MODULE + 1, false);

					ConfigurationLoadedHandler();

					//SaveModuleConfiguration();
				}
				else
				{
					if(newConfig->moduleVersion != configurationPointer->moduleVersion) uart("ERROR", "{\"type\":\"error\",\"module\":%u,\"code\":1,\"text\":\"wrong config version.\"}" SEP, moduleId);
					else uart("ERROR", "{\"type\":\"error\",\"module\":%u,\"code\":2,\"text\":\"wrong configuration length. \"}" SEP, moduleId);
				}
			}
			else if(packet->actionType == ModuleConfigMessages::GET_CONFIG)
			{

				u8 buffer[SIZEOF_CONN_PACKET_MODULE + configurationLength];

				connPacketModule* outPacket = (connPacketModule*)buffer;
				outPacket->header.messageType = MESSAGE_TYPE_MODULE_CONFIG;
				outPacket->header.sender = node->persistentConfig.nodeId;
				outPacket->header.receiver = packet->header.sender;

				outPacket->moduleId = moduleId;
				outPacket->requestHandle = packet->requestHandle;
				outPacket->actionType = ModuleConfigMessages::CONFIG;

				memcpy(outPacket->data, (u8*)configurationPointer, configurationLength);

				cm->SendMessageToReceiver(NULL, buffer, SIZEOF_CONN_PACKET_MODULE + configurationLength, false);

			}
			else if(packet->actionType == ModuleConfigMessages::SET_ACTIVE)
			{
				//Look for the module and set it active or inactive
				for(u32 i=0; i<MAX_MODULE_COUNT; i++){
					if(node->activeModules[i] && node->activeModules[i]->moduleId == packet->moduleId)
					{
						node->activeModules[i]->configurationPointer->moduleActive = packet->data[0];

						//Send confirmation that the module is now active
						connPacketModule outPacket;
						outPacket.header.messageType = MESSAGE_TYPE_MODULE_CONFIG;
						outPacket.header.sender = node->persistentConfig.nodeId;
						outPacket.header.receiver = packet->header.sender;

						outPacket.moduleId = moduleId;
						outPacket.requestHandle = packet->requestHandle;
						outPacket.actionType = ModuleConfigMessages::SET_ACTIVE_RESULT;
						outPacket.data[0] = NRF_SUCCESS; //Return ok

						cm->SendMessageToReceiver(NULL, (u8*) &outPacket, SIZEOF_CONN_PACKET_MODULE + 1, false);

						break;
					}
				}
			}


			/*
			 * ######################### RESPONSES
			 * */
			if(packet->actionType == ModuleConfigMessages::SET_CONFIG_RESULT)
			{
				uart("MODULE", "{\"nodeId\":%u,\"type\":\"set_config_result\",\"module\":%u,", packet->header.sender, packet->moduleId);
				uart("MODULE",  "\"requestHandle\":%u,\"code\":%u}" SEP, packet->requestHandle, packet->data[0]);
			}
			else if(packet->actionType == ModuleConfigMessages::SET_ACTIVE_RESULT)
			{
				uart("MODULE", "{\"nodeId\":%u,\"type\":\"set_active_result\",\"module\":%u,", packet->header.sender, packet->moduleId);
				uart("MODULE",  "\"requestHandle\":%u,\"code\":%u}" SEP, packet->requestHandle, packet->data[0]);
			}
			else if(packet->actionType == ModuleConfigMessages::CONFIG)
			{
				char* buffer[200];
				Logger::getInstance().convertBufferToHexString(packet->data, dataFieldLength, (char*)buffer, 200);

				uart("MODULE", "{\"nodeId\":%u,\"type\":\"config\",\"module\":%u,\"config\":\"%s\"}" SEP, packet->header.sender, moduleId, buffer);


			}
		}
	}


}
