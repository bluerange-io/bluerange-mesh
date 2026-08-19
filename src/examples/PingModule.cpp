////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// ** BlueRange Mesh – Community Edition (CE)
// ** Copyright (c) 2015-2021 MWAY DIGITAL GmbH, Germany
// ** Copyright (c) 2021-2026 BlueRange GmbH, Germany
// **
// ** This file is part of BlueRange Mesh Community Edition (formerly known as
// ** FruityMesh).
// **
// ** BlueRange Mesh Community Edition is free software: you can redistribute it
// ** and/or modify it under the terms of the GNU General Public License as
// ** published by the Free Software Foundation, either version 3 of the
// ** License, or (at your option) any later version.
// **
// ** BlueRange Mesh Community Edition is distributed in the hope that it will
// ** be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// ** See the GNU General Public License for more details.
// **
// ** You should have received a copy of the GNU General Public License along
// ** with this program. If not, see https://www.gnu.org/licenses/.
// **
// ** IMPORTANT:
// ** Any modification, extension, or derivative work of this file MUST also be
// ** licensed under the GNU General Public License v3 or later and the complete
// ** corresponding source code MUST be made available.
// **
// ** Commercial Use:
// ** If you wish to use this software without the obligations of the GPLv3
// ** (including source code disclosure), a commercial license for
// ** BlueRange Mesh OEM Edition is required.
// **
// ** License violations automatically terminate your rights under this license
// ** and may result in legal action under applicable law.
// ** For further information please use the contact form at:
// ** https://bluerange.io/en/contact
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <Utility.h>
#include <Node.h>
#include <PingModule.h>
#include <stdlib.h>

PingModule::PingModule()
    : Module(PING_MODULE_ID, "ping")
{
    //Register callbacks n' stuff

    //Save configuration to base class variables
    //sizeof configuration must be a multiple of 4 bytes
    configurationPointer = &configuration;
    configurationLength = sizeof(PingModuleConfiguration);

    //Set defaults
    ResetToDefaultConfiguration();
}

void PingModule::ResetToDefaultConfiguration()
{
    //Set default configuration values
    configuration.moduleId = moduleId;
    configuration.moduleActive = true;
    configuration.moduleVersion = PING_MODULE_CONFIG_VERSION;

    //Set additional config values...

}

void PingModule::ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength)
{
    //Do additional initialization upon loading the config


    //Start the Module...

}

void PingModule::TimerEventHandler(u16 passedTimeDs)
{
    //Do stuff on timer...

}

#ifdef TERMINAL_ENABLED
TerminalCommandHandlerReturnType PingModule::TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize)
{
    //React on commands, return true if handled, false otherwise
    if(TERMARGS(0, "pingmod")){
        //Get the id of the target node
        NodeId targetNodeId = Utility::StringToU16(commandArgs[1]);
        logt("PINGMOD", "Trying to ping node %u", targetNodeId);

        //Some data
        u8 data[1];
        data[0] = 123;

        //Send ping packet to that node
        SendModuleActionMessage(
                MessageType::MODULE_TRIGGER_ACTION,
                targetNodeId,
                PingModuleTriggerActionMessages::TRIGGER_PING,
                0,
                data,
                1, //size of payload
                false
        );

        return TerminalCommandHandlerReturnType::SUCCESS;
    }

    //Must be called to allow the module to get and set the config
    return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif

void PingModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader)
{
    //Must call superclass for handling
    Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

    //Filter trigger_action messages
    if(packetHeader->messageType == MessageType::MODULE_TRIGGER_ACTION){
        ConnPacketModuleVendor const * packet = (ConnPacketModuleVendor const *)packetHeader;

        //Check if our module is meant and we should trigger an action
        if(packet->moduleId == vendorModuleId && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE_VENDOR){
            //It's a ping message
            if(packet->actionType == PingModuleTriggerActionMessages::TRIGGER_PING){

                //Inform the user
                logt("PINGMOD", "Ping request received with data: %d", packet->data[0]);

                u8 data[2];
                data[0] = packet->data[0];
                data[1] = 111;

                //Send ping packet to that node
                SendModuleActionMessage(
                        MessageType::MODULE_ACTION_RESPONSE,
                        packetHeader->sender,
                        PingModuleActionResponseMessages::PING_RESPONSE,
                        0,
                        data,
                        2,
                        false
                );
            }
        }
    }

    //Parse Module action_response messages
    if(packetHeader->messageType == MessageType::MODULE_ACTION_RESPONSE && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE_VENDOR){

        ConnPacketModuleVendor const * packet = (ConnPacketModuleVendor const *)packetHeader;

        //Check if our module is meant and we should trigger an action
        if(packet->moduleId == vendorModuleId)
        {
            //Somebody reported its connections back
            if(packet->actionType == PingModuleActionResponseMessages::PING_RESPONSE){
                logt("PINGMOD", "Ping came back from %u with data %d, %d", packet->header.sender, packet->data[0], packet->data[1]);
            }
        }
    }
}
