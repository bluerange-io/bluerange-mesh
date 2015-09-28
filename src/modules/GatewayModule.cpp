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
	if(commandName == "gatewaymod"){
	    //Get the id of the target node
	    nodeID targetNodeId = atoi(commandArgs[0].c_str());
	    logt("GATEWAYMOD", "Trying to ping node %u", targetNodeId);

	    //Send ping packet to that node
	    connPacketModuleAction packet;
	    packet.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
	    packet.header.sender = node->persistentConfig.nodeId;
	    packet.header.receiver = targetNodeId;

	    packet.moduleId = moduleId;
	    packet.actionType = GatewayModuleTriggerActionMessages::TRIGGER_GATEWAY;
	    packet.data[0] = 123;


	    cm->SendMessageToReceiver(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_MODULE_ACTION + 1, true);

	    return true;
	}
	
	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandName, commandArgs);
}

void GatewayModule::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	//Must call superclass for handling
	Module::ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);

	//Filter trigger_action messages
	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModuleAction* packet = (connPacketModuleAction*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){
			//It's a ping message
			if(packet->actionType == GatewayModuleTriggerActionMessages::TRIGGER_GATEWAY){

				//Inform the user
				logt("GATEWAYMOD", "Ping request received with data: %d", packet->data[0]);

				//Send PING_RESPONSE
				connPacketModuleAction outPacket;
				outPacket.header.messageType = MESSAGE_TYPE_MODULE_ACTION_RESPONSE;
				outPacket.header.sender = node->persistentConfig.nodeId;
				outPacket.header.receiver = packetHeader->sender;

				outPacket.moduleId = moduleId;
				outPacket.actionType = GatewayModuleActionResponseMessages::GATEWAY_RESPONSE;
				outPacket.data[0] = packet->data[0];
				outPacket.data[1] = 111;

				cm->SendMessageToReceiver(NULL, (u8*)&outPacket, SIZEOF_CONN_PACKET_MODULE_ACTION + 2, true);

			}
		}
	}

	//Parse Module action_response messages
	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_ACTION_RESPONSE){

		connPacketModuleAction* packet = (connPacketModuleAction*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId)
		{
			//Somebody reported its connections back
			if(packet->actionType == GatewayModuleActionResponseMessages::GATEWAY_RESPONSE){
				logt("GATEWAYMOD", "Ping came back from %u with data %d, %d", packet->header.sender, packet->data[0], packet->data[1]);
			}
		}
	}
}
