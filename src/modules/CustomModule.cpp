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
#include <CustomModule.h>
#include <stdlib.h>

extern "C"{

}

CustomModule::CustomModule(u16 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot)
	: Module(moduleId, node, cm, name, storageSlot)
{
	//Register callbacks n' stuff
	Logger::getInstance().enableTag("CUSTOMMOD");

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(CustomModuleConfiguration);

	//Start module configuration loading
	LoadModuleConfiguration();
}

void CustomModule::ConfigurationLoadedHandler()
{
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if(configuration.moduleVersion == 1){/* ... */};

	//Do additional initialization upon loading the config


	//Start the Module...

}

void CustomModule::TimerEventHandler(u16 passedTime, u32 appTimer)
{
	//Do stuff on timer...

}

void CustomModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = false;
	configuration.moduleVersion = 1;

	//Set additional config values...

}

bool CustomModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	//This is an example of how to send a message to a gateway
	//for broadcast outside the network.
	if(commandName == "uart_module_trigger_action" || commandName == "action")
	{
		//action GATEWAY_NODE_ID custom REMOTE_NODE_ID MESSAGE
		if(commandArgs.size() == 4 && commandArgs[1] == moduleName) {

			nodeID gatewayNodeId = atoi(commandArgs[0].c_str());
			nodeID remoteNodeId = atoi(commandArgs[2].c_str());
			logt("CUSTOMMOD", "Sending message '%s' to gateway %u intended for remote node %u", commandArgs[3].c_str(), gatewayNodeId, remoteNodeId);

			connPacketModuleAction packet;
			packet.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
			packet.header.sender = node->persistentConfig.nodeId;
			packet.header.receiver = gatewayNodeId;
			packet.header.remoteReceiver = remoteNodeId;
			packet.moduleId = 30999; //Gateway Module ID

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

void CustomModule::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	//This is an example of how to receive a message from a gateway
	//that came in from outside the network.
	Module::ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModuleAction* packet = (connPacketModuleAction*)packetHeader;

		if(packet->moduleId == 30999)
		{
			connPacketModuleAction* packet = (connPacketModuleAction*)packetHeader;

			string message = "";
			for(int i = 0; i < sizeof(packet->data); i++) {
			    message += packet->data[i];
			}

			logt("CUSTOMMOD", "Inbound message received from gateway: '%s'", message.c_str());
		}
	}
}
