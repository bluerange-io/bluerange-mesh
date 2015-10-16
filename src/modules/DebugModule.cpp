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

#include <DebugModule.h>
#include <Utility.h>
#include <Storage.h>
#include <Node.h>

extern "C"{
#include <limits.h>
#include <stdlib.h>
}

DebugModule::DebugModule(u16 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot)
	: Module(moduleId, node, cm, name, storageSlot)
{
	//Register callbacks n' stuff
	Logger::getInstance().enableTag("DEBUGMOD");

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(DebugModuleConfiguration);

	flood = 0;
	packetsOut = 0;
	packetsIn = 0;

	ResetToDefaultConfiguration();

	//Start module configuration loading
	LoadModuleConfiguration();
}

void DebugModule::ConfigurationLoadedHandler()
{
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if(configuration.moduleVersion == 1){/* ... */};

	//Do additional initialization upon loading the config


}

void DebugModule::TimerEventHandler(u16 passedTime, u32 appTimer){

	if(!configuration.moduleActive) return;

	if(appTimer % 1000 == 0 && (packetsIn > 0 || packetsOut > 0))
	{
		logt("DEBUGMOD", "Flood Packets out: %u, in:%u", packetsOut, packetsIn);
	}

	if(flood){
		//FIXME: The packet queue might have problems when it is filled with too many packets
		//This seems to break the softdevice, fix that.

		while(cm->pendingPackets < 6){
			packetsOut++;

			connPacketModule data;
			data.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
			data.header.sender = node->persistentConfig.nodeId;
			data.header.receiver = NODE_ID_HOPS_BASE + 2;

			data.actionType = DebugModuleTriggerActionMessages::FLOOD_MESSAGE;
			data.moduleId = moduleId;
			data.data[0] = 1;

			cm->SendMessageToReceiver(NULL, (u8*) &data, SIZEOF_CONN_PACKET_MODULE, flood == 1 ? true : false);
		}
	}

	//Reset every few seconds
	if(configuration.rebootTimeMs != 0 && configuration.rebootTimeMs < appTimer){
		logt("DEBUGMOD", "Resetting!");
		NVIC_SystemReset();
	}
}

void DebugModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = true;
	configuration.moduleVersion = 1;
	configuration.rebootTimeMs = 0;
	memcpy(&configuration.testString, "jdhdur", 7);
}

bool DebugModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	//React on commands, return true if handled, false otherwise
	if(commandArgs.size() >= 2 && commandArgs[1] == moduleName)
	{
		nodeID destinationNode = (commandArgs[0] == "this") ? node->persistentConfig.nodeId : atoi(commandArgs[0].c_str());


		if(commandName == "action")
		{
			//Send a reset command to a node in the mesh, it will then reboot
			if(commandArgs.size() >= 3 && commandArgs[2] == "reset")
			{
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					DebugModuleTriggerActionMessages::RESET_NODE,
					0,
					NULL,
					0,
					false
				);

				return true;
			}
			//Reset the connection loss counter of any node
			else if(commandArgs[2] == "reset_connection_loss_counter")
			{
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					DebugModuleTriggerActionMessages::RESET_CONNECTION_LOSS_COUNTER,
					0,
					NULL,
					0,
					false
				);

				return true;
			}
			//Flood the network with messages and count them
			else if(commandArgs[2] == "flood")
			{
				//Toggles the flood mode between reliable, unreliable and off
				flood = (flood+1) % 3;
				if(flood == 0) logt("DEBUGMOD", "Flooding is off");
				if(flood == 1) logt("DEBUGMOD", "Flooding with reliable packets");
				if(flood == 2) logt("DEBUGMOD", "Flooding with unreliable packets");

				return true;
			}

		}

	}


	// Some old stuff to reboot the node every once in a while
	if (commandName == "testsave")
		{
		char buffer[70];
		Logger::getInstance().convertBufferToHexString((u8*) &configuration, sizeof(DebugModuleConfiguration), buffer);

		configuration.rebootTimeMs = 12 * 1000;

		logt("DEBUGMOD", "Saving config %s (%d)", buffer, sizeof(DebugModuleConfiguration));

		SaveModuleConfiguration();

		return true;
	}
	else if (commandName == "testload")
	{

		LoadModuleConfiguration();

		return true;
	}


	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandName, commandArgs);
}

void DebugModule::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	//Must call superclass for handling
	Module::ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);

	//Check if this request is meant for modules in general
	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){

			if(packet->actionType == DebugModuleTriggerActionMessages::FLOOD_MESSAGE){
				packetsIn++;
			}
			else if(packet->actionType == DebugModuleTriggerActionMessages::RESET_NODE){

				logt("DEBUGMOD", "Scheduled reboot in 10 seconds");

				//Schedule a reboot in a few seconds
				configuration.rebootTimeMs = node->appTimerMs + 10 * 1000;

			}
			else if(packet->actionType == DebugModuleTriggerActionMessages::RESET_CONNECTION_LOSS_COUNTER){

				logt("DEBUGMOD", "Resetting connection loss counter");

				node->persistentConfig.connectionLossCounter = 0;

			}
		}
	}
}
