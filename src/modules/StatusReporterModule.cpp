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
	if(configuration.version == 1){/* ... */};

	//Do additional initialization upon loading the config


	//Start the Module...

}



void StatusReporterModule::TimerEventHandler(u16 passedTime, u32 appTimer)
{
	//Every reporting interval, the node should send its status
	if(configuration.reportingIntervalMs != 0 && node->appTimerMs - lastReportingTimer > configuration.reportingIntervalMs){

		SendConnectionInformation(NULL);
		lastReportingTimer = node->appTimerMs;
	}

}

void StatusReporterModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = true;
	configuration.version = 1;

	lastReportingTimer = 0;
	configuration.reportingIntervalMs = 0;
	configuration.samplingIntervalMs = 0;

	//Set additional config values...

}

bool StatusReporterModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	//Must be called to allow the module to get and set the config
	Module::TerminalCommandHandler(commandName, commandArgs);

	//React on commands, return true if handled, false otherwise
	if(commandName == "UART_GET_CONNECTIONS")
	{

		//Print own connections
		if(Terminal::terminalIsInitialized){
				//Print our own data

				uart("QOS_CONN", "{\"nodeId\":%d, \"partners\":[%d,%d,%d,%d], \"rssiValues\":[%d,%d,%d,%d]}", node->persistentConfig.nodeId, cm->connections[0]->partnerId, cm->connections[1]->partnerId, cm->connections[2]->partnerId, cm->connections[3]->partnerId, cm->connections[0]->GetAverageRSSI(), cm->connections[1]->GetAverageRSSI(), cm->connections[2]->GetAverageRSSI(), cm->connections[3]->GetAverageRSSI());
		}

		//Query connection information from all other nodes
		connPacketQosRequest packet;
		packet.header.messageType = MESSAGE_TYPE_QOS_REQUEST;
		packet.header.sender = node->persistentConfig.nodeId;
		packet.header.receiver = 0;

		packet.payload.nodeId = 0;
		packet.payload.type = 0;

		cm->SendMessageOverConnections(NULL, (u8*) &packet, SIZEOF_CONN_PACKET_QOS_REQUEST, false);
	}
	else if(commandName == "UART_MODULE_TRIGGER_ACTION")
	{
		//Check which action


		//E.g. UART_MODULE_TRIGGER_ACTION 635 STATUS led on
		if(commandArgs.size() == 4 && commandArgs[2] == "led")
		{
			connPacketModuleRequest packet;
			packet.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
			packet.header.sender = node->persistentConfig.nodeId;
			packet.header.receiver = atoi(commandArgs[0].c_str());

			packet.moduleId = moduleId;
			packet.data[0] = StatusModuleMessages::LED_MESSAGE;
			packet.data[1] = commandArgs[3] == "on" ? 1: 0;


			cm->SendMessageToReceiver(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_MODULE_REQUEST+2, true);


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
		connPacketModuleRequest* packet = (connPacketModuleRequest*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){
			//It's a LED message
			if(packet->data[0] == StatusModuleMessages::LED_MESSAGE){
				if(packet->data[1])
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
		}
	}

	switch(packetHeader->messageType){
		case MESSAGE_TYPE_QOS_REQUEST:
			if (dataLength == SIZEOF_CONN_PACKET_QOS_REQUEST)
			{
				logt("DATA", "IN <= %d QOS_REQUEST", connection->partnerId);

				connPacketQosRequest* packet = (connPacketQosRequest*) packetHeader;

				//Check if we are requested to send a quality of service packet
				if (packet->payload.nodeId == 0 || packet->payload.nodeId == node->persistentConfig.nodeId)
				{
					if (packet->payload.type == 0)
					{
						SendConnectionInformation(connection);
					}
					else if (packet->payload.type == 1)
					{
						//more
					}
				}
			}
			break;
			//In case we receive a packet with connection information, we print it
		case MESSAGE_TYPE_QOS_CONNECTION_DATA:
			if (dataLength == SIZEOF_CONN_PACKET_QOS_CONNECTION_DATA)
			{
				connPacketQosConnectionData* packet = (connPacketQosConnectionData*) packetHeader;

				if (Terminal::terminalIsInitialized)
				{
					uart("QOS_CONN", "{\"nodeId\":%d, \"partners\":[%d,%d,%d,%d], \"rssiValues\":[%d,%d,%d,%d]}", packet->header.sender, packet->payload.partner1, packet->payload.partner2, packet->payload.partner3, packet->payload.partner4, packet->payload.rssi1, packet->payload.rssi2, packet->payload.rssi3, packet->payload.rssi4);
				}
			}
			break;
	}
}

void StatusReporterModule::SendConnectionInformation(Connection* toConnection)
{

	logt("DATA", "OUT => ? Broadcasting QOS_PACKET");

	connPacketQosConnectionData packet;
	packet.header.messageType = MESSAGE_TYPE_QOS_CONNECTION_DATA;
	packet.header.sender = node->persistentConfig.nodeId;
	packet.header.receiver = NODE_ID_BROADCAST;

	packet.payload.partner1 = cm->connections[0]->partnerId;
	packet.payload.partner2 = cm->connections[1]->partnerId;
	packet.payload.partner3 = cm->connections[2]->partnerId;
	packet.payload.partner4 = cm->connections[3]->partnerId;

	packet.payload.rssi1 = cm->connections[0]->GetAverageRSSI();
	packet.payload.rssi2 = cm->connections[1]->GetAverageRSSI();
	packet.payload.rssi3 = cm->connections[2]->GetAverageRSSI();
	packet.payload.rssi4 = cm->connections[3]->GetAverageRSSI();

	cm->SendMessageToReceiver(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_QOS_CONNECTION_DATA, false);
}
