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

#include <Logger.h>
#include <StatusReporterModule.h>
#include <Utility.h>
#include <Storage.h>
#include <Node.h>
#include <Config.h>
#include <stdlib.h>

extern "C"{

}

StatusReporterModule::StatusReporterModule(u16 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot)
	: Module(moduleId, node, cm, name, storageSlot)
{
	//Register callbacks n' stuff
	Logger::getInstance().enableTag("REPORT");

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(StatusReporterModuleConfiguration);

	lastReportingTimer = 0;

	//Start module configuration loading
	LoadModuleConfiguration();
}

void StatusReporterModule::ConfigurationLoadedHandler()
{
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if(configuration.moduleVersion == 1){/* ... */};

	//Do additional initialization upon loading the config


	//Start the Module...

}



void StatusReporterModule::TimerEventHandler(u16 passedTime, u32 appTimer)
{
	//Every reporting interval, the node should send its status
	if(configuration.reportingIntervalMs != 0 && node->appTimerMs - lastReportingTimer > configuration.reportingIntervalMs){

		SendConnectionInformation(NODE_ID_BROADCAST);
		lastReportingTimer = node->appTimerMs;
	}

}

void StatusReporterModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = false;
	configuration.moduleVersion = 1;

	lastReportingTimer = 0;
	configuration.reportingIntervalMs = 30 * 1000;
	configuration.samplingIntervalMs = 0;

	//Set additional config values...

}

bool StatusReporterModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	//Must be called to allow the module to get and set the config
	Module::TerminalCommandHandler(commandName, commandArgs);

	//React on commands, return true if handled, false otherwise
	if(commandName == "uart_module_trigger_action")
	{
		//Rewrite "this" to our own node id, this will actually build the packet
		//But reroute it to our own node
		nodeID destinationNode = (commandArgs[0] == "this") ? node->persistentConfig.nodeId : atoi(commandArgs[0].c_str());
		if(commandArgs[1] != "status") return false;


		//E.g. UART_MODULE_TRIGGER_ACTION 635 STATUS led on
		if(commandArgs.size() == 4 && commandArgs[2] == "led")
		{
			connPacketModuleAction packet;
			packet.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
			packet.header.sender = node->persistentConfig.nodeId;
			packet.header.receiver = destinationNode;

			packet.moduleId = moduleId;
			packet.actionType = StatusModuleTriggerActionMessages::SET_LED_MESSAGE;
			packet.data[0] = commandArgs[3] == "on" ? 1: 0;


			cm->SendMessageToReceiver(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_MODULE_ACTION + 1, true);

			return true;
		}
		else if(commandArgs.size() == 3 && commandArgs[2] == "get_status")
		{
			SendStatusInformation(destinationNode);

			return true;
		}
		else if(commandArgs.size() == 3 && commandArgs[2] == "get_connections")
		{
			connPacketModuleAction packet;
			packet.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
			packet.header.sender = node->persistentConfig.nodeId;
			packet.header.receiver = destinationNode;

			packet.moduleId = moduleId;
			packet.actionType = StatusModuleTriggerActionMessages::GET_CONNECTIONS_MESSAGE;


			cm->SendMessageToReceiver(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_MODULE_ACTION + 1, true);

			return true;
		}


		return false;
	}
	else
	{
		return false;
	}
	return true;
}

void StatusReporterModule::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	//Must call superclass for handling
	Module::ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModuleAction* packet = (connPacketModuleAction*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){
			//It's a LED message
			if(packet->actionType == StatusModuleTriggerActionMessages::SET_LED_MESSAGE){
				if(packet->data[0])
				{
					//Switch LED on
					node->currentLedMode = Node::ledMode::LED_MODE_OFF;

					node->LedRed->On();
					node->LedGreen->On();
					node->LedBlue->On();
				}
				else
				{
					//Switch LEDs back to connection signaling
					node->currentLedMode = Node::ledMode::LED_MODE_CONNECTIONS;
				}
			}
			//We were queried for our status
			else if(packet->actionType == StatusModuleTriggerActionMessages::GET_STATUS_MESSAGE)
			{
				//TODO: Build the status response packet and do not just print st. to the console
				node->UartGetStatus();

			}
			//We were queried for our connections
			else if(packet->actionType == StatusModuleTriggerActionMessages::GET_CONNECTIONS_MESSAGE)
			{
				//Build response and send
				u16 packetSize = SIZEOF_CONN_PACKET_MODULE_ACTION + SIZEOF_STATUS_REPORTER_MODULE_CONNECTIONS_MESSAGE;
				u8 buffer[packetSize];
				connPacketModuleAction* outPacket = (connPacketModuleAction*)buffer;

				outPacket->header.messageType = MESSAGE_TYPE_MODULE_ACTION_RESPONSE;
				outPacket->header.receiver = packetHeader->sender;
				outPacket->header.sender = node->persistentConfig.nodeId;

				outPacket->moduleId = moduleId;
				outPacket->actionType = StatusModuleActionResponseMessages::CONNECTIONS_MESSAGE;

				StatusReporterModuleConnectionsMessage* outPacketData = (StatusReporterModuleConnectionsMessage*)(outPacket->data);

				outPacketData->partner1 = cm->connections[0]->partnerId;
				outPacketData->partner2 = cm->connections[1]->partnerId;
				outPacketData->partner3 = cm->connections[2]->partnerId;
				outPacketData->partner4 = cm->connections[3]->partnerId;

				outPacketData->rssi1 = cm->connections[0]->GetAverageRSSI();
				outPacketData->rssi2 = cm->connections[1]->GetAverageRSSI();
				outPacketData->rssi3 = cm->connections[2]->GetAverageRSSI();
				outPacketData->rssi4 = cm->connections[3]->GetAverageRSSI();


				cm->SendMessageToReceiver(NULL, buffer, packetSize, true);

			}
		}
	}

	//Parse Module responses
	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_ACTION_RESPONSE){

		connPacketModuleAction* packet = (connPacketModuleAction*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId)
		{
			//Somebody reported its connections back
			if(packet->actionType == StatusModuleActionResponseMessages::CONNECTIONS_MESSAGE)
			{
				StatusReporterModuleConnectionsMessage* packetData = (StatusReporterModuleConnectionsMessage*) (packet->data);
				uart("STATUSMOD", "{\"module\":%d, \"type\":\"response\", \"msgType\":\"connections\", \"nodeId\":%d, \"partners\":[%d,%d,%d,%d], \"rssiValues\":[%d,%d,%d,%d]}", moduleId, packet->header.sender, packetData->partner1, packetData->partner2, packetData->partner3, packetData->partner4, packetData->rssi1, packetData->rssi2, packetData->rssi3, packetData->rssi4);
			}
			else if(packet->actionType == StatusModuleActionResponseMessages::STATUS_MESSAGE)
			{
				//TODO: print status information to console
				logt("STATUSMOD", "STATUSREQUEST");
			}
		}
	}
}

//This method sends the node's status over the network
void StatusReporterModule::SendStatusInformation(nodeID toNode)
{
	connPacketModuleAction packet;
	packet.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
	packet.header.sender = node->persistentConfig.nodeId;
	packet.header.receiver = toNode;

	packet.moduleId = moduleId;
	packet.actionType = StatusModuleTriggerActionMessages::GET_STATUS_MESSAGE;


	//TODO: this information needs to be completed: Fill witgh data


	cm->SendMessageToReceiver(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_MODULE_ACTION, true);
}

//This method sends information about the current connections over the network
void StatusReporterModule::SendConnectionInformation(nodeID toNode)
{
	//Build response and send
	u16 packetSize = SIZEOF_CONN_PACKET_MODULE_ACTION + SIZEOF_STATUS_REPORTER_MODULE_CONNECTIONS_MESSAGE;
	u8 buffer[packetSize];
	connPacketModuleAction* outPacket = (connPacketModuleAction*)buffer;

	outPacket->header.messageType = MESSAGE_TYPE_MODULE_ACTION_RESPONSE;
	outPacket->header.receiver = NODE_ID_BROADCAST;
	outPacket->header.sender = node->persistentConfig.nodeId;

	outPacket->moduleId = moduleId;
	outPacket->actionType = StatusModuleActionResponseMessages::CONNECTIONS_MESSAGE;

	StatusReporterModuleConnectionsMessage* outPacketData = (StatusReporterModuleConnectionsMessage*)(outPacket->data);

	outPacketData->partner1 = cm->connections[0]->partnerId;
	outPacketData->partner2 = cm->connections[1]->partnerId;
	outPacketData->partner3 = cm->connections[2]->partnerId;
	outPacketData->partner4 = cm->connections[3]->partnerId;

	outPacketData->rssi1 = cm->connections[0]->GetAverageRSSI();
	outPacketData->rssi2 = cm->connections[1]->GetAverageRSSI();
	outPacketData->rssi3 = cm->connections[2]->GetAverageRSSI();
	outPacketData->rssi4 = cm->connections[3]->GetAverageRSSI();


	cm->SendMessageToReceiver(NULL, buffer, packetSize, true);
}
