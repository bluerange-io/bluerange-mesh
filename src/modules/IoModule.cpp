////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2019 M-Way Solutions GmbH
// ** Contact: https://www.blureange.io/licensing
// **
// ** This file is part of the Bluerange/FruityMesh implementation
// **
// ** $BR_BEGIN_LICENSE:GPL-EXCEPT$
// ** Commercial License Usage
// ** Licensees holding valid commercial Bluerange licenses may use this file in
// ** accordance with the commercial license agreement provided with the
// ** Software or, alternatively, in accordance with the terms contained in
// ** a written agreement between them and M-Way Solutions GmbH. 
// ** For licensing terms and conditions see https://www.bluerange.io/terms-conditions. For further
// ** information use the contact form at https://www.bluerange.io/contact.
// **
// ** GNU General Public License Usage
// ** Alternatively, this file may be used under the terms of the GNU
// ** General Public License version 3 as published by the Free Software
// ** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
// ** included in the packaging of this file. Please review the following
// ** information to ensure the GNU General Public License requirements will
// ** be met: https://www.gnu.org/licenses/gpl-3.0.html.
// **
// ** $BR_END_LICENSE$
// **
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////

#define IO_MODULE_CONFIG_VERSION 1

#include <IoModule.h>

#ifdef ACTIVATE_IO_MODULE

#include <Logger.h>
#include <Utility.h>
#include <GlobalState.h>
#include <Node.h>

extern "C"{
#include <stdlib.h>
}

IoModule::IoModule()
	: Module(moduleID::IO_MODULE_ID, "io")
{
	moduleVersion = IO_MODULE_CONFIG_VERSION;

	//Register callbacks n' stuff

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(IoModuleConfiguration);

	//Set defaults
	ResetToDefaultConfiguration();
}

void IoModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = true;
	configuration.moduleVersion = IO_MODULE_CONFIG_VERSION;

	//Set additional config values...
	configuration.ledMode = Config->defaultLedMode;

	SET_FEATURESET_CONFIGURATION(&configuration);
}

void IoModule::ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength)
{
	//Do additional initialization upon loading the config
	currentLedMode = (ledMode)configuration.ledMode;

	//Start the Module...

}

void IoModule::TimerEventHandler(u16 passedTimeDs)
{
	//Do stuff on timer...


	//If the Beacon is in the enrollment network
	if(currentLedMode == ledMode::LED_MODE_CONNECTIONS && GS->node->configuration.networkId == 1){

		GS->ledRed->On();
		GS->ledGreen->Off();
		GS->ledBlue->Off();

	}
	else if (currentLedMode == ledMode::LED_MODE_CONNECTIONS)
	{
		//Calculate the current blink step
		ledBlinkPosition = (ledBlinkPosition + 1) % ((Config->meshMaxConnections + 2) * 2);

		//No Connections: Red blinking, Connected: Green blinking for connection count

		BaseConnections conns = GS->cm->GetBaseConnections(ConnectionDirection::INVALID);
		u8 countHandshakeDone = 0;
		for(u32 i=0; i< conns.count; i++){
			BaseConnection *conn = GS->cm->allConnections[conns.connectionIndizes[i]];
			if(conn != nullptr && conn->handshakeDone()) countHandshakeDone++;
		}
		u8 countConnected = conns.count - countHandshakeDone;

		u8 i = ledBlinkPosition / 2;

		if(i < MAX_NUM_MESH_CONNECTIONS){
			if(ledBlinkPosition % 2 == 0){
				//No connections
				if (conns.count == 0){ GS->ledRed->On(); }
				//Connected and handshake done
				else if(i < countHandshakeDone) { GS->ledGreen->On(); }
				//Connected and handshake not done
				else if(i < conns.count) { GS->ledBlue->On(); }
				//A free connection
				else if(i < MAX_NUM_MESH_CONNECTIONS) {  }
			} else {
				GS->ledRed->Off();
				GS->ledGreen->Off();
				GS->ledBlue->Off();
			}
		}
	}
#ifdef ENABLE_TEST_DEVICES
	else if(currentLedMode == ledMode::LED_MODE_CLUSTERING)
	{
		//A different color for each cluster

		ledBlinkPosition++;

		int c = 0;
		for(int i=0; i<NUM_TEST_COLOUR_IDS; i++){
			NodeId nodeIdFromClusterId = node->clusterId & 0xffff;

			if(Config->testColourIDs[i] == nodeIdFromClusterId){
				c = (i+1) % 8;

				if(c & (1 << 0)) GS->ledRed->On();
				else GS->ledRed->Off();

				if(c & (1 << 1)) GS->ledGreen->On();
				else GS->ledGreen->Off();

				if(c & (1 << 2)) GS->ledBlue->On();
				else GS->ledBlue->Off();

				if(i >= 8 && ledBlinkPosition %2 == 0){
					GS->ledRed->Off();
					GS->ledGreen->Off();
					GS->ledBlue->Off();
				}
			}
		}
	}
#endif
	else if(currentLedMode == ledMode::LED_MODE_ON)
	{
		//All LEDs on (orange when only green and red available)
		GS->ledRed->On();
		GS->ledGreen->On();
		GS->ledBlue->On();
	}
	else if(currentLedMode == ledMode::LED_MODE_OFF)
	{
		GS->ledRed->Off();
		GS->ledGreen->Off();
		GS->ledBlue->Off();
	}
	else if(currentLedMode == ledMode::LED_MODE_ASSET)
	{
		//Constant red

		GS->ledRed->On();
		GS->ledGreen->Off();
		GS->ledBlue->Off();
	}
}

#ifdef TERMINAL_ENABLED
bool IoModule::TerminalCommandHandler(char* commandArgs[],u8 commandArgsSize)
{
	//React on commands, return true if handled, false otherwise
	if(commandArgsSize >= 3 && TERMARGS(2, moduleName))
	{
		if (TERMARGS(0, "action"))
		{
			if(!TERMARGS(2, moduleName)) return false;

			NodeId destinationNode = (TERMARGS(1, "this")) ? GS->node->configuration.nodeId : atoi(commandArgs[1]);

			//Example:
#ifndef SAVE_SPACE_GW_1
			if(commandArgsSize >= 6 && TERMARGS(3,"pinset"))
			{
				//Check how many GPIO ports we want to set
				u8 numExtraParams = (u8) (commandArgsSize - 4);
				u8 numPorts = numExtraParams / 2;
				u8 requestHandle = (numExtraParams % 2 == 0) ? 0 : atoi(commandArgs[commandArgsSize - 1]);

				DYNAMIC_ARRAY(buffer, numPorts*SIZEOF_GPIO_PIN_CONFIG);

				//Encode ports + states into the data
				for(int i=0; i<numPorts; i++){
					gpioPinConfig* p = (gpioPinConfig*) (buffer + i*SIZEOF_GPIO_PIN_CONFIG);
					p->pinNumber = (u8)strtoul(commandArgs[(i*2)+4], nullptr, 10);
					p->direction = GPIO_PIN_CNF_DIR_Output;
					p->inputBufferConnected = GPIO_PIN_CNF_INPUT_Disconnect; // config as output
					p->pull = GPIO_PIN_CNF_PULL_Disabled;
					p->driveStrength = GPIO_PIN_CNF_DRIVE_S0S1;
					p->sense = GPIO_PIN_CNF_SENSE_Disabled;
					p->set = TERMARGS((i*2)+5, "high") ? 1 : 0;
				}

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					(u8)IoModuleTriggerActionMessages::SET_PIN_CONFIG,
					requestHandle,
					buffer,
					numPorts*SIZEOF_GPIO_PIN_CONFIG,
					false
				);
				return true;
			}
			//E.g. action 635 io led on
			else
#endif
			if(commandArgsSize >= 5 && TERMARGS(3,"led"))
			{
				IoModuleSetLedMessage data;

				if(TERMARGS(4, "on")) data.ledMode= ledMode::LED_MODE_ON;
				else if(TERMARGS(4, "cluster")) data.ledMode = ledMode::LED_MODE_CLUSTERING;
				else data.ledMode = Config->defaultLedMode == ledMode::LED_MODE_OFF ? ledMode::LED_MODE_OFF : ledMode::LED_MODE_CONNECTIONS;

				u8 requestHandle = commandArgsSize >= 6 ? atoi(commandArgs[5]) : 0;

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					(u8)IoModuleTriggerActionMessages::SET_LED,
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
	return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif

//void IoModule::ParseTerminalInputList(string commandName, vector<string> commandArgs)


void IoModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader)
{
	//Must call superclass for handling
	Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModule* packet = (connPacketModule*)packetHeader;
		u16 dataFieldLength = sendData->dataLength - SIZEOF_CONN_PACKET_MODULE;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){
			IoModuleTriggerActionMessages actionType = (IoModuleTriggerActionMessages)packet->actionType;
			if(actionType == IoModuleTriggerActionMessages::SET_PIN_CONFIG){

				configuration.ledMode = ledMode::LED_MODE_OFF;
				currentLedMode = ledMode::LED_MODE_OFF;

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
					(u8)IoModuleActionResponseMessages::SET_PIN_CONFIG_RESULT,
					packet->requestHandle,
					nullptr,
					0,
					false
				);
			}
			//A message to switch on the LEDs
			else if(actionType == IoModuleTriggerActionMessages::SET_LED){

				IoModuleSetLedMessage* data = (IoModuleSetLedMessage*)packet->data;

				configuration.ledMode = (ledMode)data->ledMode;
				currentLedMode = (ledMode)data->ledMode;

				if(currentLedMode == ledMode::LED_MODE_ON){
					GS->ledRed->On();
					GS->ledGreen->On();
					GS->ledBlue->On();
				} else {
					GS->ledRed->Off();
					GS->ledGreen->Off();
					GS->ledBlue->Off();
				}

				//send confirmation
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
					packet->header.sender,
					(u8)IoModuleActionResponseMessages::SET_LED_RESPONSE,
					packet->requestHandle,
					nullptr,
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
			IoModuleActionResponseMessages actionType = (IoModuleActionResponseMessages)packet->actionType;
			if(actionType == IoModuleActionResponseMessages::SET_PIN_CONFIG_RESULT)
			{
				logjson("MODULE", "{\"nodeId\":%u,\"type\":\"set_pin_config_result\",\"module\":%u,", packet->header.sender, packet->moduleId);
				logjson("MODULE",  "\"requestHandle\":%u,\"code\":%u}" SEP, packet->requestHandle, 0);
			}
			else if(actionType == IoModuleActionResponseMessages::SET_LED_RESPONSE)
			{
				logjson("MODULE", "{\"nodeId\":%u,\"type\":\"set_led_result\",\"module\":%u,", packet->header.sender, packet->moduleId);
				logjson("MODULE",  "\"requestHandle\":%u,\"code\":%u}" SEP, packet->requestHandle, 0);
			}
		}
	}
}

#endif
