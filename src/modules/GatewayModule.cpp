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
		//action TARGET_NODE_ID gateway MESSAGE
		if(IsGatewayDevice() && commandArgs.size() == 3 && commandArgs[1] == moduleName) {
			//Gateway Device
			//--------------
			//This is a gateway dongle connected via serial to a node http gateway.
			//The command above was received from a CLI, by either a human or a nodejs gateway instance.
			//Incoming gateway message should therefore be forwarded to target node in the mesh.

			nodeID targetNodeId = atoi(commandArgs[0].c_str());
			logt("GATEWAYMOD", "Sending message '%s' to node %u", commandArgs[2].c_str(), targetNodeId);

			connPacketModuleAction packet;
			packet.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
			packet.header.sender = node->persistentConfig.nodeId;
			packet.header.receiver = targetNodeId;

			packet.moduleId = moduleId;
			packet.actionType = GatewayModuleTriggerActionMessages::TRIGGER_GATEWAY;

			vector<u8> convert(commandArgs[2].begin(), commandArgs[2].end());
			for(int i = 0; i < convert.size(); i++) {
			    packet.data[i] = convert[i];
			}

			cm->SendMessageToReceiver(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_MODULE_ACTION + commandArgs[2].length() + 1, true);
			return true;
		}

		//action GATEWAY_NODE_ID gateway REMOTE_NODE_ID MESSAGE
		if(commandArgs.size() == 4 && commandArgs[1] == moduleName) {
			//Remote-message Requesting Device
			//--------------------------------
			//This is any device on the network, probably a non-gateway device, but it is connected to
			//a serial port. Someone has used a CLI to tell this node to send a message to a remote node
			//via a gateway dongle. Incoming serial message should be sent to the gateway with remoteReceiver set.

			nodeID gatewayNodeId = atoi(commandArgs[0].c_str());
			nodeID remoteNodeId = atoi(commandArgs[2].c_str());
			logt("GATEWAYMOD", "Sending message '%s' to gateway %u intended for remote node %u", commandArgs[3].c_str(), gatewayNodeId, remoteNodeId);

			connPacketModuleAction packet;
			packet.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
			packet.header.sender = node->persistentConfig.nodeId;
			packet.header.receiver = gatewayNodeId;
			packet.header.remoteReceiver = remoteNodeId;

			packet.moduleId = moduleId;
			packet.actionType = GatewayModuleTriggerActionMessages::TRIGGER_GATEWAY;

			vector<u8> convert(commandArgs[3].begin(), commandArgs[3].end());
			for(int i = 0; i < convert.size(); i++) {
			    packet.data[i] = convert[i];
			}

			cm->SendMessageToReceiver(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_MODULE_ACTION + commandArgs[3].length() + 1, true);
			return true;
		}
	}
	
	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandName, commandArgs);
}

bool GatewayModule::IsGatewayDevice()
{
	return true;
}

void GatewayModule::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	Module::ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModuleAction* packet = (connPacketModuleAction*)packetHeader;

		if(packet->moduleId == moduleId && packet->actionType == GatewayModuleTriggerActionMessages::TRIGGER_GATEWAY){

			connPacketModuleAction* packet = (connPacketModuleAction*)packetHeader;

			string message = "";	
			for(int i = 0; i < sizeof(packet->data); i++) {
			    message += packet->data[i];
			}

			if(IsGatewayDevice()) {
				logt("GATEWAYMOD", "Outbound message received for gateway: '%s'", message.c_str());
				logt("GATEWAYMOD", "{ \"gateway-message\": { \"sender\": \"%u\", \"receiver\": \"%u\", \"message\": \"%s\" }}", packet->header.sender, packet->header.remoteReceiver, message.c_str());
			} else {
				logt("GATEWAYMOD", "Inbound message received from gateway: '%s'", message.c_str());
			}
		}
	}
}
