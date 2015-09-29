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
#include <Utility.h>
#include <Storage.h>
#include <Node.h>
#include <GatewayModule.h>
#include <stdlib.h>

extern "C"{

}

const bool isGateway = true;

GatewayModule::GatewayModule(u16 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot)
	: Module(moduleId, node, cm, name, storageSlot)
{
	//Register callbacks n' stuff
	Logger::getInstance().enableTag("GATEWAYMOD");

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(GatewayModuleConfiguration);

	//Start module configuration loading
	LoadModuleConfiguration();
}

void GatewayModule::ConfigurationLoadedHandler()
{
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if(configuration.moduleVersion == 1){/* ... */};

	//Do additional initialization upon loading the config


	//Start the Module...

}

void GatewayModule::TimerEventHandler(u16 passedTime, u32 appTimer)
{
	//Do stuff on timer...

}

void GatewayModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = false;
	configuration.moduleVersion = 1;

	//Set additional config values...

}

bool GatewayModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	if(commandName == "uart_module_trigger_action" || commandName == "action")
	{
		if(isGateway && commandArgs.size() == 3 && commandArgs[1] == moduleName) {
			//This is a gateway dongle connected via serial to a node http gateway.
			//Incoming gateway message should therefore be forwarded to target node in the mesh.

			nodeID targetNodeId = atoi(commandArgs[0].c_str());
			int message = atoi(commandArgs[2].c_str());
			logt("GATEWAYMOD", "Sending message %d to node %u", message, targetNodeId);

			connPacketModuleAction packet;
			packet.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
			packet.header.sender = node->persistentConfig.nodeId;
			packet.header.receiver = targetNodeId;

			packet.moduleId = moduleId;
			packet.actionType = GatewayModuleTriggerActionMessages::TRIGGER_GATEWAY;
			packet.data[0] = message;

			cm->SendMessageToReceiver(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_MODULE_ACTION + 1, true);
			return true;
		}

		if(commandArgs.size() == 4 && commandArgs[1] == moduleName) {
			//This is a regular dongle connected via serial to a serial port.
			//Someone is using a CLI command to push a gateway message out over a node http gateway.
			//Incoming serial message should be sent to the gateway with remoteReceiver set.

			nodeID gatewayNodeId = atoi(commandArgs[0].c_str());
			nodeID remoteNodeId = atoi(commandArgs[2].c_str());
			int message = atoi(commandArgs[3].c_str());
			logt("GATEWAYMOD", "Sending message %d to gateway %u intended for remote node %u", message, gatewayNodeId, remoteNodeId);

			connPacketModuleAction packet;
			packet.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
			packet.header.sender = node->persistentConfig.nodeId;
			packet.header.receiver = gatewayNodeId;
			packet.header.remoteReceiver = remoteNodeId;

			packet.moduleId = moduleId;
			packet.actionType = GatewayModuleTriggerActionMessages::TRIGGER_GATEWAY;
			packet.data[0] = message;

			cm->SendMessageToReceiver(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_MODULE_ACTION + 1, true);
			return true;
		}
	}
	
	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandName, commandArgs);
}

void GatewayModule::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	Module::ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModuleAction* packet = (connPacketModuleAction*)packetHeader;

		if(packet->moduleId == moduleId && packet->actionType == GatewayModuleTriggerActionMessages::TRIGGER_GATEWAY){
			if(isGateway) {
				logt("GATEWAYMOD", "{ \"gateway-message\": { \"sender\": \"%u\", \"receiver\": \"%u\", \"message\": \"%d\" }}", packet->header.sender, packet->header.remoteReceiver, packet->data[0]);
			} else {
				logt("GATEWAYMOD", "Gateway message received with data: %d", packet->data[0]);
			}
		}
	}
}
