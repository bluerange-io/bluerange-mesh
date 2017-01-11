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

DebugModule::DebugModule(u8 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot)
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


void DebugModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = true;
	configuration.moduleVersion = 1;
	configuration.rebootTimeDs = 0;
}

void DebugModule::TimerEventHandler(u16 passedTimeDs, u32 appTimerDs){

	if(!configuration.moduleActive) return;

	if(SHOULD_IV_TRIGGER(appTimerDs, passedTimeDs, SEC_TO_DS(5)) && (packetsIn > 0 || packetsOut > 0))
	{
		logt("DEBUGMOD", "Flood Packets out: %u, in:%u", packetsOut, packetsIn);
	}

	if(flood){

		while(cm->GetPendingPackets() < 6){
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

	//Reset if a reset time is set
	if(configuration.rebootTimeDs != 0 && configuration.rebootTimeDs < appTimerDs){
		logt("DEBUGMOD", "Resetting!");
		NVIC_SystemReset();
	}


	if(false && SHOULD_IV_TRIGGER(appTimerDs, passedTimeDs, SEC_TO_DS(10)))
	{
		DebugModuleInfoMessage infoMessage;
		memset(&infoMessage, 0x00, sizeof(infoMessage));
		for(int i=0; i<Config->meshMaxConnections; i++){
			if(cm->connections[i]->handshakeDone()){
				infoMessage.droppedPackets += cm->connections[i]->droppedPackets;
				infoMessage.sentPackets += cm->connections[i]->sentReliable + cm->connections[i]->sentUnreliable;
			}
		}

		infoMessage.connectionLossCounter = node->persistentConfig.connectionLossCounter;

		SendModuleActionMessage(
			MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
			NODE_ID_BROADCAST,
			DebugModuleTriggerActionMessages::INFO_MESSAGE,
			0,
			(u8*)&infoMessage,
			SIZEOF_DEBUG_MODULE_INFO_MESSAGE,
			false
		);


	}
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
			//Tell any node to generate a hardfault
			else if(commandArgs[2] == "hardfault")
			{
				logt("DEBUGMOD", "send hardfault");
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					DebugModuleTriggerActionMessages::CAUSE_HARDFAULT_MESSAGE,
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
			//Sets the reestablishing time, in the debug module until ready
			else if(commandArgs[2] == "set_reestablish_time" && commandArgs.size() == 4)
			{
				u16 timeout = atoi(commandArgs[3].c_str());

				DebugModuleReestablishTimeoutMessage data;

				data.reestablishTimeoutSec = timeout;

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					DebugModuleTriggerActionMessages::SET_REESTABLISH_TIMEOUT,
					0,
					(u8*)&data,
					SIZEOF_DEBUG_MODULE_REESTABLISH_TIMEOUT_MESSAGE,
					true
				);

				return true;
			}
		}

	}

	//Reads a page of the memory (0-256) and prints it
	if(commandName == "readpage")
	{
		u16 page = atoi(commandArgs[0].c_str());
		u32 bufferSize = 32;
		u8 buffer[bufferSize];
		char charBuffer[bufferSize*3+1];

		for(u32 i=0; i<PAGE_SIZE/bufferSize; i++)
		{
			memcpy(buffer, (u8*)(page*PAGE_SIZE+i*bufferSize), bufferSize);
			Logger::getInstance().convertBufferToHexString(buffer, bufferSize, charBuffer, bufferSize*3+1);
			trace("0x%08X :%s" EOL,(page*PAGE_SIZE)+i*bufferSize, charBuffer);
		}

		return true;
	}
	//Prints a map of empty (0) and used (1) memory pages
	else if(commandName == "memorymap")
	{
		for(u32 j=0; j<256; j++){
			u32 buffer = 0xFFFFFFFF;
			for(u32 i=0; i<NRF_FICR->CODEPAGESIZE; i+=4){
				buffer = buffer & *(u32*)(j*NRF_FICR->CODEPAGESIZE+i);
			}
			if(buffer == 0xFFFFFFFF) trace("0");
			else trace("1");
		}

		return true;
	}
	if (commandName == "log_error")
	{

		u32 errorCode = atoi(commandArgs[0].c_str());
		u16 extra = atoi(commandArgs[1].c_str());

		Logger::getInstance().logError(Logger::errorTypes::CUSTOM, errorCode, extra);

		return true;
		}

	// Some old stuff to reboot the node every once in a while
	if (commandName == "testsave")
		{
		char buffer[70];
		Logger::getInstance().convertBufferToHexString((u8*) &configuration, sizeof(DebugModuleConfiguration), buffer, 70);

		configuration.rebootTimeDs = SEC_TO_DS(12);

		logt("DEBUGMOD", "Saving config %s (%d)", buffer, sizeof(DebugModuleConfiguration));

		SaveModuleConfiguration();

		return true;
	}
	else if (commandName == "testload")
	{

		LoadModuleConfiguration();

		return true;
	}
	else if (commandName == "send")
	{
		//parameter 1: R=reliable, U=unreliable, B=both
		//parameter 2: count

		connPacketData1 data;
		data.header.messageType = MESSAGE_TYPE_DATA_1;
		data.header.sender = node->persistentConfig.nodeId;
		data.header.receiver = 0;

		data.payload.length = 7;
		data.payload.data[2] = 7;


		u8 reliable = (commandArgs.size() < 1 || commandArgs[0] == "b") ? 2 : (commandArgs[0] == "u" ? 0 : 1);

		//Second parameter is number of messages
		u8 count = commandArgs.size() > 1 ? atoi(commandArgs[1].c_str()) : 5;

		for (int i = 0; i < count; i++)
		{
			if(reliable == 0 || reliable == 2){
				data.payload.data[0] = i*2;
				data.payload.data[1] = 0;
				cm->SendMessage(cm->inConnection, (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, false);
				cm->SendMessage(cm->outConnections[0], (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, false);
				cm->SendMessage(cm->outConnections[1], (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, false);
				cm->SendMessage(cm->outConnections[2], (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, false);
			}

			if(reliable == 1 || reliable == 2){
				data.payload.data[0] = i*2+1;
				data.payload.data[1] = 1;
				cm->SendMessage(cm->inConnection, (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, true);
				cm->SendMessage(cm->outConnections[0], (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, true);
				cm->SendMessage(cm->outConnections[1], (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, true);
				cm->SendMessage(cm->outConnections[2], (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, true);
			}
		}
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
				configuration.rebootTimeDs = node->appTimerDs + SEC_TO_DS(10);

			}
			else if(packet->actionType == DebugModuleTriggerActionMessages::RESET_CONNECTION_LOSS_COUNTER){

				logt("DEBUGMOD", "Resetting connection loss counter");

				node->persistentConfig.connectionLossCounter = 0;
				Logger::getInstance().errorLogPosition = 0;

			}
			else if(packet->actionType == DebugModuleTriggerActionMessages::INFO_MESSAGE){

				DebugModuleInfoMessage* infoMessage = (DebugModuleInfoMessage*) packet->data;

				uart("DEBUGMOD", "{\"nodeId\":%u,\"type\":\"debug_info\", \"conLoss\":%u,", packet->header.sender, infoMessage->connectionLossCounter);
				uart("DEBUGMOD", "\"dropped\":%u,", infoMessage->droppedPackets);
				uart("DEBUGMOD", "\"sent\":%u}" SEP, infoMessage->sentPackets);

			}
			else if(packet->actionType == DebugModuleTriggerActionMessages::CAUSE_HARDFAULT_MESSAGE){
				logt("DEBUGMOD", "receive hardfault");
				CauseHardfault();
			}
			else if(packet->actionType == DebugModuleTriggerActionMessages::SET_REESTABLISH_TIMEOUT){

				DebugModuleReestablishTimeoutMessage* data = (DebugModuleReestablishTimeoutMessage*) packet->data;

				//Set the config
				Config->meshExtendedConnectionTimeoutSec = data->reestablishTimeoutSec;

				//Apply for all active connections
				for(int i=0; i<Config->meshMaxConnections; i++){
					cm->connections[i]->reestablishTimeSec = data->reestablishTimeoutSec;
				}

				logt("DEBUGMOD", "SustainTime set to %u", data->reestablishTimeoutSec);

				//Acknowledge over the mesh
				SendModuleActionMessage(
						MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
						packet->header.sender,
						DebugModuleActionResponseMessages::REESTABLISH_TIMEOUT_RESPONSE,
						0,
						NULL,
						0,
						false
					);
			}
		}
	}
	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_ACTION_RESPONSE){
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant
		if(packet->moduleId == moduleId){
			if(packet->actionType == DebugModuleActionResponseMessages::REESTABLISH_TIMEOUT_RESPONSE){
				uart("DEBUGMOD", "{\"type\":\"reestablish_time_response\",\"nodeId\":%u,\"module\":%u,\"code\":0}" SEP, packet->header.sender, moduleId);

			}

		}
	}
}

void DebugModule::CauseHardfault()
{
	//Needs a for loop, the compiler will otherwise detect the unaligned access and will write generate a workaround
	for(int i=0; i<1000; i++){
		u32 * cause_hardfault = (u32 *) i;
		u32 value = *cause_hardfault;
		logt("DEBUGMOD", "%d", value);
	}
}
