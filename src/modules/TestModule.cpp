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

#include <TestModule.h>
#include <Utility.h>
#include <Storage.h>
#include <Node.h>

extern "C"{
#include <limits.h>
}

TestModule::TestModule(u16 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot)
	: Module(moduleId, node, cm, name, storageSlot)
{
	//Register callbacks n' stuff
	Logger::getInstance().enableTag("TEST");

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(TestModuleConfiguration);

	ResetToDefaultConfiguration();

	//Start module configuration loading
	LoadModuleConfiguration();
}

void TestModule::ConfigurationLoadedHandler()
{
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if(configuration.moduleVersion == 1){/* ... */};

	//Do additional initialization upon loading the config


}

void TestModule::TimerEventHandler(u16 passedTime, u32 appTimer){

	if(!configuration.moduleActive) return;

	//logt("TEST", "Passed time %d, appTimer:%d", passedTime, appTimer);

	//Reset every few seconds
	if(configuration.rebootTimeMs != 0 && configuration.rebootTimeMs < appTimer){
		logt("TEST", "Resetting!");
		NVIC_SystemReset();
	}
}

void TestModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = false;
	configuration.moduleVersion = 1;
	configuration.rebootTimeMs = 0;
	memcpy(&configuration.testString, "jdhdur", 7);
}

bool TestModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	//Must be called to allow the module to get and set the config
	Module::TerminalCommandHandler(commandName, commandArgs);

	if (commandName == "TESTSAVE")
	{
		char buffer[70];
		Logger::getInstance().convertBufferToHexString((u8*) &configuration, sizeof(TestModuleConfiguration), buffer);

		configuration.rebootTimeMs = 12 * 1000;

		logt("TEST", "Saving config %s (%d)", buffer, sizeof(TestModuleConfiguration));

		SaveModuleConfiguration();

		return true;

	}
	else if (commandName == "TESTLOAD")
	{

		LoadModuleConfiguration();

		return true;

	}
	else if (commandName == "LEDS")
	{
		//Check if user wants to set all LEDs on or off and send that as a broadcast packet to all
		//mesh nodes
		bool state = (commandArgs.size() > 0 && commandArgs[0] == "on") ? true : false;

		connPacketModuleRequest packet;

		packet.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
		packet.header.sender = node->persistentConfig.nodeId;
		packet.header.receiver = NODE_ID_BROADCAST;

		packet.moduleId = moduleId;
		packet.data[0] = TestModuleMessages::LED_MESSAGE;
		packet.data[1] = state;

		cm->SendMessageOverConnections(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_MODULE_REQUEST+2, true);


		return true;

	}
	else
	{
		return false;
	}

}

void TestModule::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	//Must call superclass for handling
	Module::ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);

	//Check if this request is meant for modules in general
	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModuleRequest* packet = (connPacketModuleRequest*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){

			//It's a LED message
			if(packet->data[0] == TestModuleMessages::LED_MESSAGE){
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
}
