////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2021 M-Way Solutions GmbH
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

#include <VendorTemplateModule.h>
#include <GlobalState.h>
#include <Logger.h>
#include <Utility.h>
#include <Node.h>

VendorTemplateModule::VendorTemplateModule()
    : Module(VENDOR_TEMPLATE_MODULE_ID, "template")
{
    //Register callbacks n' stuff

    //Enable the logtag for our vendor module template
    GS->logger.EnableTag("TMOD");

    //Save configuration to base class variables
    //sizeof configuration must be a multiple of 4 bytes
    vendorConfigurationPointer = &configuration;
    configurationLength = sizeof(VendorTemplateModuleConfiguration);

    //Set defaults
    ResetToDefaultConfiguration();
}

void VendorTemplateModule::ResetToDefaultConfiguration()
{
    //Set default configuration values
    configuration.moduleId = vendorModuleId;
    configuration.moduleActive = true;
    configuration.moduleVersion = VENDOR_TEMPLATE_MODULE_CONFIG_VERSION;

    //Set additional config values...

    //This line allows us to have different configurations of this module depending on the featureset
    SET_FEATURESET_CONFIGURATION_VENDOR(&configuration, this);
}

void VendorTemplateModule::ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength)
{
    VendorModuleConfiguration* newConfig = (VendorModuleConfiguration*)migratableConfig;

    //Version migration can be added here, e.g. if module has version 2 and config is version 1
    if(newConfig != nullptr && newConfig->moduleVersion == 1){/* ... */};

    //Do additional initialization upon loading the config


    //Start the Module...

}

void VendorTemplateModule::TimerEventHandler(u16 passedTimeDs)
{
    //Do stuff on timer...

}

#ifdef TERMINAL_ENABLED
TerminalCommandHandlerReturnType VendorTemplateModule::TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize)
{
    //React on commands, return true if handled, false otherwise
    if(commandArgsSize >= 3 && TERMARGS(2, moduleName))
    {
        if(TERMARGS(0, "action"))
        {
            if(!TERMARGS(2, moduleName)) return TerminalCommandHandlerReturnType::UNKNOWN;

            if(commandArgsSize >= 5 && TERMARGS(3, "one"))
            {
                logt("TMOD", "Command one executed");

                VendorTemplateModuleCommandOneMessage data;
                data.exampleValue = Utility::StringToU8(commandArgs[4]);

                //PART 1 of sending a message: Some command is entered and a request message is sent
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    NODE_ID_BROADCAST,
                    (u8)VendorTemplateModuleTriggerActionMessages::COMMAND_ONE_MESSAGE,
                    0,
                    (u8*)&data,
                    SIZEOF_VENDOR_TEMPLATE_MODULE_COMMAND_ONE_MESSAGE,
                    true
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            else if(commandArgsSize >= 4 && TERMARGS(3, "two"))
            {
                logt("TMOD", "Command two executed");

                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    NODE_ID_BROADCAST,
                    (u8)VendorTemplateModuleTriggerActionMessages::COMMAND_TWO_MESSAGE,
                    0,
                    nullptr,
                    0,
                    true,
                    true
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }

            return TerminalCommandHandlerReturnType::UNKNOWN;

        }
    }

    //Must be called to allow the module to get and set the config
    return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif


void VendorTemplateModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader)
{
    //Must call superclass for handling
    Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

    if(packetHeader->messageType == MessageType::MODULE_TRIGGER_ACTION && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE_VENDOR){
        ConnPacketModuleVendor const * packet = (ConnPacketModuleVendor const *)packetHeader;

        //Check if our module is meant and we should trigger an action
        if(packet->moduleId == vendorModuleId){
            if(packet->actionType == VendorTemplateModuleTriggerActionMessages::COMMAND_ONE_MESSAGE)
            {
                const VendorTemplateModuleCommandOneMessage* data = (const VendorTemplateModuleCommandOneMessage*)packet->data;

                logt("TMOD", "Got command one message with %u", data->exampleValue);
            }
            else if (packet->actionType == VendorTemplateModuleTriggerActionMessages::COMMAND_TWO_MESSAGE)
            {
                logt("TMOD", "Got command two message");
            }
        }
    }

    //Parse Module responses
    if(packetHeader->messageType == MessageType::MODULE_ACTION_RESPONSE && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE_VENDOR){
        ConnPacketModuleVendor const * packet = (ConnPacketModuleVendor const *)packetHeader;

        //Check if our module is meant and we should trigger an action
        if(packet->moduleId == vendorModuleId)
        {
            if(packet->actionType == VendorTemplateModuleActionResponseMessages::COMMAND_ONE_MESSAGE_RESPONSE)
            {

            }
        }
    }
}
