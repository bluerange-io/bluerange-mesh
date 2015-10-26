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
#include <RSSIModule.h>
#include <stdlib.h>
#include <LedWrapper.h>

#define kTimerInterval 		1*1000
#define kPinNumberRed		18
#define kPinNumberGreen   	19
#define kPinNumberBlue   	20

extern "C"{
#include "nrf_gpio.h"
#include <app_error.h>
}

RSSIModule::RSSIModule(u16 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot)
	: Module(moduleId, node, cm, name, storageSlot)
{
	//Register callbacks n' stuff
	Logger::getInstance().enableTag("PINGMOD");

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(RSSIModuleConfiguration);

	//Start module configuration loading
	LoadModuleConfiguration();

    configuration.moduleActive = true;
}

void RSSIModule::ConfigurationLoadedHandler()
{
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if(configuration.moduleVersion == 1){/* ... */};

	//Do additional initialization upon loading the config
	configuration.pingInterval = kTimerInterval;
	configuration.lastPingTimer = 0;
	configuration.pingCount = 0;
    configuration.connectionRSSISamplingMode = RSSISamplingModes::RSSI_SAMPLING_HIGH;
    configuration.advertisingRSSISamplingMode = RSSISamplingModes::RSSI_SAMPLING_HIGH;

	nrf_gpio_cfg_output(kPinNumberRed);
	nrf_gpio_cfg_output(kPinNumberGreen);
	nrf_gpio_cfg_output(kPinNumberBlue);

	//Start the Module...
	logt("PINGMOD", "ConfigLoaded");
}

void RSSIModule::TimerEventHandler(u16 passedTime, u32 appTimer)
{
	if(configuration.pingInterval != 0 && node->appTimerMs - configuration.lastPingTimer > configuration.pingInterval)
	{
		configuration.lastPingTimer = node->appTimerMs;

		SendPing(DEST_BOARD_ID); 

	}
}

void RSSIModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = false;
	configuration.moduleVersion = 1;
	configuration.pingInterval = kTimerInterval;
	configuration.pingCount = 0;
	configuration.lastPingTimer = 0;
    configuration.connectionRSSISamplingMode = RSSISamplingModes::RSSI_SAMPLING_HIGH;
    configuration.advertisingRSSISamplingMode = RSSISamplingModes::RSSI_SAMPLING_HIGH;

	//Set additional config values...
	logt("PINGMOD", "Reset");
}

bool RSSIModule::SendPing(nodeID targetNodeId)
{
	// logt("PINGMOD", "Trying to ping node %u from %u", targetNodeId, node->persistentConfig.nodeId);

        //Send ping packet to that node
        connPacketModule packet;
        packet.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
        packet.header.sender = node->persistentConfig.nodeId;
        packet.header.receiver = targetNodeId;

        packet.moduleId = moduleId;
        packet.actionType = RSSIModuleTriggerActionMessages::TRIGGER_PING;
       	packet.data[0] = configuration.pingCount++;

        cm->SendMessageToReceiver(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_MODULE + 1, true);
	return(true);
}

bool RSSIModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	if(commandArgs.size() >= 2 && commandArgs[1] == moduleName)
	{
		//React on commands, return true if handled, false otherwise
		if(commandName == "pingmod"){
			nodeID targetNodeId = atoi(commandArgs[0].c_str());

			return(SendPing(targetNodeId));
		}
	}

	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandName, commandArgs);
}

void RSSIModule::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	//Must call superclass for handling
	Module::ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);

    connPacketModule* packet = (connPacketModule*) packetHeader;

    switch(packetHeader->messageType)
    {
        case MESSAGE_TYPE_MODULE_TRIGGER_ACTION:
            //Check if our module is meant and we should trigger an action
            if(packet->moduleId == moduleId)
            {
                switch(packet->actionType)
                {
                    case RSSIModuleTriggerActionMessages::TRIGGER_PING:
//                        logt("PINGMOD", "Ping request received from %u with data: %d", packetHeader->sender, packet->data[0]);

                        //Send PING_RESPONSE
                        connPacketModule outPacket;
                        outPacket.header.messageType = MESSAGE_TYPE_MODULE_ACTION_RESPONSE;
                        outPacket.header.sender = node->persistentConfig.nodeId;
                        outPacket.header.receiver = packetHeader->sender;

                        outPacket.moduleId = moduleId;
                        outPacket.actionType = RSSIModuleActionResponseMessages::PING_RESPONSE;
                        outPacket.data[0] = packet->data[0];
                        outPacket.data[1] = packet->data[0];

                        cm->SendMessageToReceiver(NULL, (u8*)&outPacket, SIZEOF_CONN_PACKET_MODULE + 2, true);

                        {
                            int a = cm->connections[0]->GetAverageRSSI();
                            int b = cm->connections[1]->GetAverageRSSI();
                            int c = cm->connections[2]->GetAverageRSSI();
                            int d = cm->connections[3]->GetAverageRSSI();
                            int sum = -(a+b+c+d);

                            if(sum == 0)
                            {
                                    nrf_gpio_pin_write(kPinNumberRed, 0);
                                    nrf_gpio_pin_write(kPinNumberGreen, 0);
                                    nrf_gpio_pin_write(kPinNumberBlue, 0);
                            } else
                            if(sum >= 80) {
                                    nrf_gpio_pin_write(kPinNumberRed, 255);
                                    nrf_gpio_pin_write(kPinNumberGreen, 0);
                                    nrf_gpio_pin_write(kPinNumberBlue, 0);
                            } else
                            if (sum >= 70) {
                                nrf_gpio_pin_write(kPinNumberRed, 153);
                                nrf_gpio_pin_write(kPinNumberGreen, 76);
                                nrf_gpio_pin_write(kPinNumberBlue, 0);
                            } else {
                                nrf_gpio_pin_write(kPinNumberRed, 0);
                                nrf_gpio_pin_write(kPinNumberGreen, 255);
                                nrf_gpio_pin_write(kPinNumberBlue, 0);
                            }
			
                        	logt("PINGMOD", "RSSI: [%d] [%d] [%d] [%d] Sum: %d", a, b, c, d, sum);
                        }
                        break;

                     default:
                         logt("PINGMOD", "RSSIModuleTriggerActionMessages::TRIGGER_PING: Unknown action: %d", packet->actionType);
                         break;
                }
            }
            break;

        case MESSAGE_TYPE_MODULE_ACTION_RESPONSE:
            //Check if our module is meant and we should trigger an action
            if(packet->moduleId == moduleId)
            {
                switch(packet->actionType)
                {
                    case RSSIModuleActionResponseMessages::PING_RESPONSE:
//                        logt("PINGMOD", "MESSAGE_TYPE_MODULE_ACTION_RESPONSE: Got response.");
                        break;

                     default:
                         logt("PINGMOD", "MESSAGE_TYPE_MODULE_ACTION_RESPONSE: Unknown action: %d", packet->actionType);
                         break;
                }
            }
            break;
    }

}

void RSSIModule::MeshConnectionChangedHandler(Connection* connection)
{
    logt("PINGMOD", "MeshConnectionChangedHandler");
    //New connection has just been made
    if(connection->handshakeDone){
        logt("PINGMOD", "MeshConnectionChangedHandler::Handshake");
        if(Config->enableConnectionRSSIMeasurement){
            logt("PINGMOD", "RSSI");
            if(configuration.connectionRSSISamplingMode == RSSISamplingModes::RSSI_SAMPLING_HIGH){
                logt("PINGMOD", "MeshConnectionChangedHandler::sampling");
                StartConnectionRSSIMeasurement(connection);
            }
        }
    }
}

void RSSIModule::StartConnectionRSSIMeasurement(Connection* connection){
    u32 err = 0;

    if (connection->isConnected)
    {
        //Reset old values
        connection->rssiSamplesNum = 0;
        connection->rssiSamplesSum = 0;

        err = sd_ble_gap_rssi_start(connection->connectionHandle, 0, 0);
        APP_ERROR_CHECK(err);

        logt("PINGMOD", "************* RSSI measurement started for connection %u with code %u", connection->connectionId, err);
    } else
    {
        logt("PINGMOD", "************* RSSI measurement not connected");
    }
}

//This handler receives all ble events and can act on them
void RSSIModule::BleEventHandler(ble_evt_t* bleEvent) {
    //New RSSI measurement for connection received
    if(bleEvent->header.evt_id == BLE_GAP_EVT_RSSI_CHANGED)
    {
        Connection* connection = cm->GetConnectionFromHandle(bleEvent->evt.gap_evt.conn_handle);
        i8 rssi = bleEvent->evt.gap_evt.params.rssi_changed.rssi;

        connection->rssiSamplesNum++;
        connection->rssiSamplesSum += rssi;

        if(connection->rssiSamplesNum > 50){
            connection->rssiAverage = connection->rssiSamplesSum / connection->rssiSamplesNum;

            connection->rssiSamplesNum = 0;
            connection->rssiSamplesSum = 0;

            logt("PINGMOD", "New RSSI average %d", connection->rssiAverage);
        }


    }
}
