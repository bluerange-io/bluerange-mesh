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

#include <IoModule.h>
#include <Logger.h>
#include <Utility.h>
#include <Storage.h>
#include <Node.h>

extern "C"{
#include <stdlib.h>
}

IoModule::IoModule(u8 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot)
	: Module(moduleId, node, cm, name, storageSlot)
{
	//Register callbacks n' stuff
	Logger::getInstance().enableTag("IOMOD");

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(IoModuleConfiguration);

	//Start module configuration loading
	LoadModuleConfiguration();
}

void IoModule::ConfigurationLoadedHandler()
{
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if(configuration.moduleVersion == 1){/* ... */};

	//Do additional initialization upon loading the config


	//Start the Module...

}

void IoModule::TimerEventHandler(u16 passedTime, u32 appTimer)
{
	//Do stuff on timer...

}

void IoModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = false;
	configuration.moduleVersion = 1;

	//Set additional config values...

}

bool IoModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	//React on commands, return true if handled, false otherwise
	if(commandArgs.size() >= 2 && commandArgs[1] == moduleName)
	{
		if(commandName == "action")
		{
			if(commandArgs[1] != moduleName) return false;

			nodeID destinationNode = (commandArgs[0] == "this") ? node->persistentConfig.nodeId : atoi(commandArgs[0].c_str());

			//Example:
			if(commandArgs.size() >= 5 && commandArgs[2] == "pinset")
			{
				//Check how many GPIO ports we want to set
				u8 numExtraParams = commandArgs.size() - 3;
				u8 numPorts = numExtraParams / 2;
				u8 requestHandle = (numExtraParams % 2 == 0) ? 0 : atoi(commandArgs[commandArgs.size()-1].c_str());

				u8 buffer[numPorts*SIZEOF_GPIO_PIN_CONFIG];

				//Encode ports + states into the data
				for(int i=0; i<numPorts; i++){
					gpioPinConfig* p = (gpioPinConfig*) (buffer + i*SIZEOF_GPIO_PIN_CONFIG);
					p->pinNumber = (u8)strtoul(commandArgs[(i*2)+3].c_str(), NULL, 10);
					p->direction = GPIO_PIN_CNF_DIR_Output;
					p->inputBufferConnected = GPIO_PIN_CNF_INPUT_Disconnect; // config as output
					p->pull = GPIO_PIN_CNF_PULL_Disabled;
					p->driveStrength = GPIO_PIN_CNF_DRIVE_S0S1;
					p->sense = GPIO_PIN_CNF_SENSE_Disabled;
					p->set = commandArgs[(i*2)+4] == "high" ? 1 : 0;
				}

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					IoModuleTriggerActionMessages::SET_PIN_CONFIG,
					requestHandle,
					buffer,
					numPorts*SIZEOF_GPIO_PIN_CONFIG,
					false
				);

				return true;
			}
			else if(commandArgs.size() == 3 && commandArgs[2] == "get")
			{
				u8 requestHandle = commandArgs.size() >= 6 ? atoi(commandArgs[5].c_str()) : 0;

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					IoModuleTriggerActionMessages::GET_PIN_CONFIG,
					requestHandle,
					NULL,
					0,
					false
				);

				return true;
			}
			//E.g. action 635 io led on
			else if(commandArgs.size() >= 4 && commandArgs[2] == "led")
			{
				IoModuleSetLedMessage data;

				if(commandArgs[3] == "on") data.ledMode= ledMode::LED_MODE_ON;
				else if(commandArgs[3] == "cluster") data.ledMode = ledMode::LED_MODE_CLUSTERING;
				else data.ledMode = ledMode::LED_MODE_CONNECTIONS;

				u8 requestHandle = commandArgs.size() >= 5 ? atoi(commandArgs[4].c_str()) : 0;

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					IoModuleTriggerActionMessages::SET_LED,
					requestHandle,
					(u8*)&data,
					1,
					false
				);

				return true;
			}

			return false;

		}
	}

	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandName, commandArgs);
}

//void IoModule::ParseTerminalInputList(string commandName, vector<string> commandArgs)


void IoModule::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	//Must call superclass for handling
	Module::ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModule* packet = (connPacketModule*)packetHeader;
		u16 dataFieldLength = dataLength - SIZEOF_CONN_PACKET_MODULE;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){
			if(packet->actionType == IoModuleTriggerActionMessages::SET_PIN_CONFIG){

				node->currentLedMode = ledMode::LED_MODE_OFF;

				//Parse the data and set the gpio ports to the requested
				for(int i=0; i<dataFieldLength; i+=SIZEOF_GPIO_PIN_CONFIG)
				{
					gpioPinConfig* pinConfig = (gpioPinConfig*)(packet->data + i);

					NRF_GPIO->PIN_CNF[pinConfig->pinNumber] =
							  (pinConfig->sense << GPIO_PIN_CNF_SENSE_Pos)
					        | (pinConfig->driveStrength << GPIO_PIN_CNF_DRIVE_Pos)
					        | (pinConfig->pull << GPIO_PIN_CNF_PULL_Pos)
					        | (pinConfig->inputBufferConnected << GPIO_PIN_CNF_INPUT_Pos)
					        | (pinConfig->direction << GPIO_PIN_CNF_DIR_Pos);

					if(pinConfig->set) NRF_GPIO->OUTSET = (1UL << pinConfig->pinNumber);
					else NRF_GPIO->OUTCLR = (1UL << pinConfig->pinNumber);
				}

				//Confirmation
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
					packet->header.sender,
					IoModuleActionResponseMessages::SET_PIN_CONFIG_RESULT,
					packet->requestHandle,
					NULL,
					0,
					false
				);
			}
			//A message to switch on the LEDs
			else if(packet->actionType == IoModuleTriggerActionMessages::SET_LED){

				IoModuleSetLedMessage* data = (IoModuleSetLedMessage*)packet->data;

				node->currentLedMode = (ledMode)data->ledMode;

				//send confirmation
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
					packet->header.sender,
					IoModuleActionResponseMessages::SET_LED_RESPONSE,
					packet->requestHandle,
					NULL,
					0,
					false
				);
			}
		}
	}

	//Parse Module responses
	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_ACTION_RESPONSE){
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId)
		{
			if(packet->actionType == IoModuleActionResponseMessages::SET_PIN_CONFIG_RESULT)
			{
				uart("MODULE", "{\"nodeId\":%u,\"type\":\"set_pin_config_result\",\"module\":%u,", packet->header.sender, packet->moduleId);
				uart("MODULE",  "\"requestHandle\":%u,\"code\":%u}" SEP, packet->requestHandle, 0);
			}
			else if(packet->actionType == IoModuleActionResponseMessages::SET_LED_RESPONSE)
			{
				uart("MODULE", "{\"nodeId\":%u,\"type\":\"set_led_result\",\"module\":%u,", packet->header.sender, packet->moduleId);
				uart("MODULE",  "\"requestHandle\":%u,\"code\":%u}" SEP, packet->requestHandle, 0);
			}
		}
	}
}

