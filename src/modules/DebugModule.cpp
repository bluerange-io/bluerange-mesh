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

#include <DebugModule.h>
#include <Utility.h>
#include <Node.h>
#include <IoModule.h>
#include <AdvertisingController.h>
#include <ScanController.h>

extern "C"{
#include <limits.h>
#include <stdlib.h>
#include <app_timer.h>
#ifndef SIM_ENABLED
#include <nrf_nvic.h>
#endif
}

DebugModule::DebugModule(u8 moduleId, Node* node, ConnectionManager* cm, const char* name)
	: Module(moduleId, node, cm, name)
{
	moduleVersion = 1;

	//Register callbacks n' stuff

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(DebugModuleConfiguration);

	rebootTimeDs = 0;

	flood = 0;
	packetsOut = 0;
	packetsIn = 0;

	pingSentTicks = 0;
	pingHandle = 0;
	pingCount = 0;
	pingCountResponses = 0;

	ResetToDefaultConfiguration();

	//Start module configuration loading
	LoadModuleConfiguration();
}

void DebugModule::ConfigurationLoadedHandler()
{
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if(configuration.moduleVersion == this->moduleVersion){/* ... */};

	//Do additional initialization upon loading the config

}


void DebugModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = true;
	configuration.moduleVersion = 1;
	configuration.debugButtonRemoveEnrollmentDs = 0;
	configuration.debugButtonEnableUartDs = 0;
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

			cm->SendMeshMessage((u8*) &data, SIZEOF_CONN_PACKET_MODULE, DeliveryPriority::DELIVERY_PRIORITY_LOW, flood == 1 ? true : false);
		}
	}

	//Reset if a reset time is set
	if(rebootTimeDs != 0 && rebootTimeDs < appTimerDs){
		logt("DEBUGMOD", "Resetting!");
		FruityHal::SystemReset();
	}


	if(false && SHOULD_IV_TRIGGER(appTimerDs, passedTimeDs, SEC_TO_DS(10)))
	{
		DebugModuleInfoMessage infoMessage;
		memset(&infoMessage, 0x00, sizeof(infoMessage));

		MeshConnections conn = GS->cm->GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);
		for(u32 i=0; i< conn.count; i++){
			if(conn.connections[i]->handshakeDone()){
				infoMessage.droppedPackets += conn.connections[i]->droppedPackets;
				infoMessage.sentPackets += conn.connections[i]->sentReliable + conn.connections[i]->sentUnreliable;
			}
		}

		infoMessage.connectionLossCounter = node->connectionLossCounter;

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



void DebugModule::ButtonHandler(u8 buttonId, u32 holdTimeDs)
{
	logt("ERROR", "sdf %u", configuration.debugButtonRemoveEnrollmentDs);
	//Put beacon into Asset mode, it will broadcast it's nodeId as an assetId
	if(SHOULD_BUTTON_EVT_EXEC(configuration.debugButtonRemoveEnrollmentDs)){
		logt("ERROR", "Resetting to unenrolled mode, wait for reboot");

		GS->node->persistentConfig.networkId = Config->meshNetworkIdentifier;
		memcpy(GS->node->persistentConfig.networkKey, &Config->meshNetworkKey, 16);

		node->SaveConfiguration();

		//Schedule a reboot in a few seconds
		rebootTimeDs = node->appTimerDs + SEC_TO_DS(4);

	}

	if(SHOULD_BUTTON_EVT_EXEC(configuration.debugButtonEnableUartDs)){
		//Enable UART
#ifdef USE_UART
		Terminal::getInstance()->UartEnable(false);
#endif

		//Enable LED
		for(int i=0; i<MAX_MODULE_COUNT; i++){
			if(GS->node->activeModules[i]->moduleId == moduleID::IO_MODULE_ID){
				((IoModule*)GS->node->activeModules[i])->currentLedMode = ledMode::LED_MODE_CONNECTIONS;
			}
		}
	}
}

bool DebugModule::TerminalCommandHandler(std::string commandName, std::vector<std::string> commandArgs)
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
			else if(commandArgs.size() == 4 && commandArgs[2] == "tunnel" && commandArgs[3].length() < 14*3){
				connPacketData3 data;
				data.header.messageType = MESSAGE_TYPE_DATA_3;
				data.header.sender = node->persistentConfig.nodeId;
				data.header.receiver = destinationNode;
				data.payload.len = (u8)((commandArgs[3].length()+1) / 3);
				Logger::getInstance()->parseHexStringToBuffer(commandArgs[3].c_str(), data.payload.data, 15);

				cm->SendMeshMessage(
						(u8*) &data,
						SIZEOF_CONN_PACKET_DATA_3,
						DeliveryPriority::DELIVERY_PRIORITY_LOW,
						false);

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
			else if (commandArgs[2] == "ping" && commandArgs.size() == 5)
			{
				//action 45 debug ping 10 u 7
				//Send 10 pings to node 45, unreliable with handle 7

				//Save Ping sent time
				pingSentTicks = FruityHal::GetRtc();
				pingCount = atoi(commandArgs[3].c_str());
				pingCountResponses = 0;
				u8 pingModeReliable = commandArgs[4] == "r";

				for(int i=0; i<pingCount; i++){
					SendModuleActionMessage(
							MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
							destinationNode,
							DebugModuleTriggerActionMessages::PING,
							0,
							NULL,
							0,
							pingModeReliable
						);
				}
				return true;
			}
			else if (commandArgs[2] == "discovery" && commandArgs.size() == 4)
			{
				//action 45 debug discovery off


				DebugModuleSetDiscoveryMessage data;

				if(commandArgs[3] == "off"){
					data.discoveryMode = 0;
				} else {
					data.discoveryMode = 1;
				}

				SendModuleActionMessage(
						MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
						destinationNode,
						DebugModuleTriggerActionMessages::SET_DISCOVERY,
						0,
						(u8*)&data,
						SIZEOF_DEBUG_MODULE_SET_DISCOVERY_MESSAGE,
						false
					);

				return true;
			}
			else if (commandArgs[2] == "pingpong" && commandArgs.size() == 5)
			{
				//action 45 debug pingpong 10 u 7
				//Send 10 pings to node 45, which will pong it back, then it pings again

				//Save Ping sent time
				pingSentTicks = FruityHal::GetRtc();
				pingCount = atoi(commandArgs[3].c_str());
				u8 pingModeReliable = commandArgs[4] == "r";

				DebugModulePingpongMessage data;
				data.ttl = pingCount * 2 - 1;

				SendModuleActionMessage(
						MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
						destinationNode,
						DebugModuleTriggerActionMessages::PINGPONG,
						0,
						(u8*)&data,
						SIZEOF_DEBUG_MODULE_PINGPONG_MESSAGE,
						pingModeReliable
					);

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
	//Display the free heap
	else if (commandName == "heap")
	{
		u8 checkvar = 1;
		logjson("NODE", "{\"stack\":%u}" SEP, &checkvar - 0x20000000);

		Utility::CheckFreeHeap();


	}
	//Reads a page of the memory (0-256) and prints it
	if(commandName == "readblock")
	{
		u16 blockSize = 1024;

		u32 offset = FLASH_REGION_START_ADDRESS;
		if(commandArgs[0] == "uicr") offset = (u32)NRF_UICR;
		if(commandArgs[0] == "ficr") offset = (u32)NRF_FICR;
		if(commandArgs[0] == "ram") offset = (u32)0x20000000;

		u16 numBlocks = 1;
		if(commandArgs.size() > 1){
			numBlocks = atoi(commandArgs[1].c_str());
		}

		u32 bufferSize = 32;
		DYNAMIC_ARRAY(buffer, bufferSize);
		DYNAMIC_ARRAY(charBuffer, bufferSize * 3 + 1);

		for(int j=0; j<numBlocks; j++){
			u16 block = atoi(commandArgs[0].c_str()) + j;

			for(u32 i=0; i<blockSize/bufferSize; i++)
			{
				memcpy(buffer, (u8*)(block*blockSize+i*bufferSize + offset), bufferSize);
				Logger::getInstance()->convertBufferToHexString(buffer, bufferSize, (char*)charBuffer, bufferSize*3+1);
				trace("0x%08X: %s" EOL,(block*blockSize)+i*bufferSize + offset, charBuffer);
			}
		}

		return true;
	}
	//Prints a map of empty (0) and used (1) memory pages
	else if(commandName == "memorymap")
	{
		u32 offset = FLASH_REGION_START_ADDRESS;
		u16 blockSize = 1024; //Size of a memory block to check
		u16 numBlocks = NRF_FICR->CODESIZE * NRF_FICR->CODEPAGESIZE / blockSize;

		for(u32 j=0; j<numBlocks; j++){
			u32 buffer = 0xFFFFFFFF;
			for(u32 i=0; i<blockSize; i+=4){
				buffer = buffer & *(u32*)(j*blockSize+i+offset);
			}
			if(buffer == 0xFFFFFFFF) trace("0");
			else trace("1");
		}

		trace(EOL);

		return true;
	}
	if (commandName == "log_error")
	{

		u32 errorCode = atoi(commandArgs[0].c_str());
		u16 extra = atoi(commandArgs[1].c_str());

		Logger::getInstance()->logError(Logger::errorTypes::CUSTOM, errorCode, extra);

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
				cm->SendMeshMessage((u8*)&data, SIZEOF_CONN_PACKET_DATA_1, DeliveryPriority::DELIVERY_PRIORITY_LOW, false);
			}

			if(reliable == 1 || reliable == 2){
				data.payload.data[0] = i*2+1;
				data.payload.data[1] = 1;
				cm->SendMeshMessage((u8*)&data, SIZEOF_CONN_PACKET_DATA_1, DeliveryPriority::DELIVERY_PRIORITY_LOW, true);
			}
		}
		return true;
	}
	//Add an advertising job
	else if (commandName == "advadd")
	{
		u8 slots = atoi(commandArgs[0].c_str());
		u8 delay = atoi(commandArgs[1].c_str());
		u8 advDataByte = atoi(commandArgs[3].c_str());

		AdvJob job = {
			AdvJobTypes::ADV_JOB_TYPE_SCHEDULED,
			slots,
			delay,
			MSEC_TO_UNITS(100, UNIT_0_625_MS),
			0,
			0,
			BLE_GAP_ADV_TYPE_ADV_IND,
			{0x02, 0x01, 0x06, 0x05, 0xFF, 0x4D, 0x02, 0xAA, advDataByte},
			9,
			{0},
			0
		};

		AdvertisingController::getInstance()->AddJob(&job);

		return true;
	}
	else if (commandName == "advrem")
	{
		i8 jobNum = atoi(commandArgs[0].c_str());
		AdvertisingController::getInstance()->RemoveJob(&(AdvertisingController::getInstance()->jobs[jobNum]));

		return true;
	}
	else if (commandName == "advjobs")
	{
		AdvertisingController* advCtrl = AdvertisingController::getInstance();
		i8 jobNum = atoi(commandArgs[0].c_str());
		char buffer[150];

		for(u32 i=0; i<advCtrl->currentNumJobs; i++){
			Logger::getInstance()->convertBufferToHexString(advCtrl->jobs[i].advData, advCtrl->jobs[i].advDataLength, buffer, 150);
			trace("Job type:%u, slots:%u, type:%u, advData:%s" EOL, advCtrl->jobs[i].type, advCtrl->jobs[i].slots, advCtrl->jobs[i].type, buffer);
		}

		return true;
	}
	else if (commandName == "feed")
	{
		FruityHal::FeedWatchdog();

		return true;
	}
	else if (commandName == "lping" && commandArgs.size() == 2)
	{
		//A leaf ping will receive a response from all leaf nodes in the mesh
		//and reports the leafs nodeIds together with the number of hops

		//Save Ping sent time
		pingSentTicks = FruityHal::GetRtc();
		pingCount = atoi(commandArgs[0].c_str());
		pingCountResponses = 0;
		u8 pingModeReliable = commandArgs[1] == "r";

		DebugModuleLpingMessage lpingData = {0, 0};

		for(int i=0; i<pingCount; i++){
			SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					NODE_ID_HOPS_BASE + 500,
					DebugModuleTriggerActionMessages::LPING,
					0,
					(u8*)&lpingData,
					SIZEOF_DEBUG_MODULE_LPING_MESSAGE,
					pingModeReliable
				);
		}
		return true;
	}

	if (commandName == "nswrite" && commandArgs.size() == 2)
	{
		u32 addr = atoi(commandArgs[0].c_str());
		u8 buffer[200];
		Logger::getInstance()->parseHexStringToBuffer(commandArgs[1].c_str(), buffer, 200);
		u16 dataLength = (strlen(commandArgs[1].c_str()) + 1)/3;


		NewStorage::getInstance()->WriteData((u32*)buffer, (u32*)addr, dataLength, NULL, 0);

		return true;
	}
	if (commandName == "erasepage" && commandArgs.size() == 1)
	{
		u16 page = atoi(commandArgs[0].c_str());
		NewStorage::getInstance()->ErasePage(page, NULL, 0);

		return true;
	}
	if (commandName == "erasepages" && commandArgs.size() == 2)
	{

		u16 page = atoi(commandArgs[0].c_str());
		u16 numPages = atoi(commandArgs[1].c_str());

		NewStorage::getInstance()->ErasePages(page, numPages, NULL, 0);

		return true;
	}


	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandName, commandArgs);
}

void DebugModule::MeshMessageReceivedHandler(MeshConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader)
{
	//Must call superclass for handling
	Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

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
				rebootTimeDs = node->appTimerDs + SEC_TO_DS(10);

			}
			else if(packet->actionType == DebugModuleTriggerActionMessages::RESET_CONNECTION_LOSS_COUNTER){

				logt("DEBUGMOD", "Resetting connection loss counter");

				node->connectionLossCounter = 0;
				Logger::getInstance()->errorLogPosition = 0;

			}
			else if(packet->actionType == DebugModuleTriggerActionMessages::INFO_MESSAGE){

				DebugModuleInfoMessage* infoMessage = (DebugModuleInfoMessage*) packet->data;

				logjson("DEBUGMOD", "{\"nodeId\":%u,\"type\":\"debug_info\", \"conLoss\":%u,", packet->header.sender, infoMessage->connectionLossCounter);
				logjson("DEBUGMOD", "\"dropped\":%u,", infoMessage->droppedPackets);
				logjson("DEBUGMOD", "\"sent\":%u}" SEP, infoMessage->sentPackets);

			}
			else if(packet->actionType == DebugModuleTriggerActionMessages::CAUSE_HARDFAULT_MESSAGE){
				logt("DEBUGMOD", "receive hardfault");
				CauseHardfault();
			}
			else if(packet->actionType == DebugModuleTriggerActionMessages::PING){
				//We respond to the ping
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
					packet->header.sender,
					DebugModuleActionResponseMessages::PING_RESPONSE,
					packet->requestHandle,
					NULL,
					0,
					sendData->deliveryOption == DeliveryOption::DELIVERY_OPTION_WRITE_REQ
				);

			}
			else if(packet->actionType == DebugModuleTriggerActionMessages::LPING){
				//Only respond to the leaf ping if we are a leaf
				if(GS->cm->GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID).count != 1){
					return;
				}

				//Insert our nodeId into the packet
				DebugModuleLpingMessage* lpingData = (DebugModuleLpingMessage*)packet->data;
				lpingData->hops = 500 - (packetHeader->receiver - NODE_ID_HOPS_BASE);
				lpingData->leafNodeId = GS->node->persistentConfig.nodeId;

				//We respond to the ping
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
					packet->header.sender,
					DebugModuleActionResponseMessages::LPING_RESPONSE,
					packet->requestHandle,
					(u8*)lpingData,
					SIZEOF_DEBUG_MODULE_LPING_MESSAGE,
					sendData->deliveryOption == DeliveryOption::DELIVERY_OPTION_WRITE_REQ
				);

			}
			else if(packet->actionType == DebugModuleTriggerActionMessages::PINGPONG){

				DebugModulePingpongMessage* data = (DebugModulePingpongMessage*)packet->data;

				//Ping should still pong, return it
				if(data->ttl > 0){
					data->ttl--;

					SendModuleActionMessage(
						MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
						packet->header.sender,
						DebugModuleTriggerActionMessages::PINGPONG,
						packet->requestHandle,
						(u8*)data,
						SIZEOF_DEBUG_MODULE_PINGPONG_MESSAGE,
						sendData->deliveryOption == DeliveryOption::DELIVERY_OPTION_WRITE_REQ
					);
				//Arrived at destination, print it
				} else {
					u32 nowTicks;
					u32 timePassed;
					nowTicks = FruityHal::GetRtc();
					timePassed = FruityHal::GetRtcDifference(nowTicks, pingSentTicks);

					u32 timePassedMs = timePassed / (APP_TIMER_CLOCK_FREQ / 1000);

					logjson("DEBUGMOD", "{\"type\":\"pingpong_response\",\"passedTime\":%u}" SEP, timePassedMs);

				}
			}
			else if(packet->actionType == DebugModuleTriggerActionMessages::SET_REESTABLISH_TIMEOUT){

				DebugModuleReestablishTimeoutMessage* data = (DebugModuleReestablishTimeoutMessage*) packet->data;

				//Set the config
				Config->meshExtendedConnectionTimeoutSec = data->reestablishTimeoutSec;

				//Apply for all active connections
				MeshConnections conn = GS->cm->GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);
				for(u32 i=0; i< conn.count; i++){
					conn.connections[i]->reestablishTimeSec = data->reestablishTimeoutSec;
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
			} else if(packet->actionType == DebugModuleTriggerActionMessages::SET_DISCOVERY){

				DebugModuleSetDiscoveryMessage* data = (DebugModuleSetDiscoveryMessage*) packet->data;

				if(data->discoveryMode == 0){
					node->ChangeState(discoveryState::DISCOVERY_OFF);
					//TODO: ADVREF AdvertisingController::getInstance()->SetAdvertisingState(advState::ADV_STATE_OFF);
					ScanController::getInstance()->SetScanState(scanState::SCAN_STATE_OFF);
				} else {
					data->discoveryMode = 1;
				}

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
					packet->header.sender,
					DebugModuleActionResponseMessages::SET_DISCOVERY_RESPONSE,
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
				logjson("DEBUGMOD", "{\"type\":\"reestablish_time_response\",\"nodeId\":%u,\"module\":%u,\"code\":0}" SEP, packet->header.sender, moduleId);

			}
			else if(packet->actionType == DebugModuleActionResponseMessages::PING_RESPONSE){
				//Calculate the time it took to ping the other node

				u32 nowTicks;
				u32 timePassed;
				nowTicks = FruityHal::GetRtc();
				timePassed = FruityHal::GetRtcDifference(nowTicks, pingSentTicks);

				u32 timePassedMs = timePassed / (APP_TIMER_CLOCK_FREQ / 1000);

				trace("p %u ms" EOL, timePassedMs);
				//logjson("DEBUGMOD", "{\"type\":\"ping_response\",\"passedTime\":%u}" SEP, timePassedMs);
			}
			else if(packet->actionType == DebugModuleActionResponseMessages::LPING_RESPONSE){
				//Calculate the time it took to ping the other node

				DebugModuleLpingMessage* lpingData = (DebugModuleLpingMessage*)packet->data;

				u32 nowTicks;
				u32 timePassed;
				nowTicks = FruityHal::GetRtc();
				timePassed = FruityHal::GetRtcDifference(nowTicks, pingSentTicks);

				u32 timePassedMs = timePassed / (APP_TIMER_CLOCK_FREQ / 1000);

				trace("lp %u(%u): %u ms" EOL, lpingData->leafNodeId, lpingData->hops, timePassedMs);
			}
			else if(packet->actionType == DebugModuleActionResponseMessages::SET_DISCOVERY_RESPONSE){
				logjson("DEBUGMOD", "{\"type\":\"set_discovery_response\",\"code\":%u}" SEP, 0);
			}
		}
	}
}

void DebugModule::CauseHardfault()
{
	//Attempts to write to write to address 0, which is in flash
	*((int*)0x0) = 10;
}
