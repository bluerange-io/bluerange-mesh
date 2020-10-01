////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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
