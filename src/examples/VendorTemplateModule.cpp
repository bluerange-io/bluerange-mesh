////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2022 M-Way Solutions GmbH
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

#if IS_ACTIVE(VENDOR_TEMPLATE_MODULE)

#include <GlobalState.h>
#include <Logger.h>
#include <Utility.h>
#include <Node.h>
#include <IoModule.h>

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

//Use e.g. an enum in a central place to hold the definitions for all of your components
//for your module. You can specify up to 65535 components (different sub-devices)
//Normally, you would place this definition in your header file
enum class VendorTemplateComponents : u16
{
    EXAMPLE_COMPONENT_1 = 0x01,
};

//For each component (and each actionType) you can have a list of up to 65535 different "registers"
//that you can map to any functionality you like
//If you want, to can use seperate mappings for READ, WRITE, .... but it might make sense to keep them
//in the same address range, e.g. similar to the Modbus protocol
//Normally, you would place this definition in your header file
enum class VendorTemplateComponent1Registers : u16
{
    EXAMPLE_COUNTER = 0x01,
    EXAMPLE_LED = 0x02,
};

void VendorTemplateModule::TimerEventHandler(u16 passedTimeDs)
{
    static u8 exampleCounter = 0;

    //Exemplary periodic reporting of a "sensor" value, switch on if desired
    if(false && SHOULD_IV_TRIGGER(GS->appTimerDs, passedTimeDs, SEC_TO_DS(5)))
    {
        //Fill all the necessary header information
        DYNAMIC_ARRAY(buffer, sizeof(ConnPacketComponentMessageVendor) + 1);
        ConnPacketComponentMessageVendor* message = (ConnPacketComponentMessageVendor*)buffer;
        message->componentHeader.header.messageType = MessageType::COMPONENT_SENSE;
        message->componentHeader.header.sender = GS->node.configuration.nodeId;
        //Use NODE_ID_SHORTEST_SINK if other Mesh Nodes do not need to reveice the message
        //Sending the event to NODE_ID_BROADCAST is less common in production setups
        message->componentHeader.header.receiver = NODE_ID_BROADCAST;
        message->componentHeader.moduleId = VENDOR_TEMPLATE_MODULE_ID;
        //UNSPECIFIED is used to report events whereas e.g. READ_RSP is used to report the result of a read request
        message->componentHeader.actionType = (u8)SensorMessageActionType::UNSPECIFIED;
        message->componentHeader.component = (u16)VendorTemplateComponents::EXAMPLE_COMPONENT_1;
        message->componentHeader.registerAddress = (u16)VendorTemplateComponent1Registers::EXAMPLE_COUNTER;
        //Can optionally be used
        message->componentHeader.requestHandle = 0;

        //Assign the counter value to the payload
        message->payload[0] = exampleCounter++;

        //Send the message through the mesh network
        GS->cm.SendMeshMessage(
            buffer,
            SIZEOF_CONN_PACKET_COMPONENT_MESSAGE_VENDOR + 1);
    }
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

    //Parse trigger actions
    if (packetHeader->messageType == MessageType::MODULE_TRIGGER_ACTION && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE_VENDOR) {
        ConnPacketModuleVendor const* packet = (ConnPacketModuleVendor const*)packetHeader;

        //Check if our module is meant and we should trigger an action
        if (packet->moduleId == vendorModuleId) {
            if (packet->actionType == VendorTemplateModuleTriggerActionMessages::COMMAND_ONE_MESSAGE)
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
    if (packetHeader->messageType == MessageType::MODULE_ACTION_RESPONSE && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE_VENDOR) {
        ConnPacketModuleVendor const* packet = (ConnPacketModuleVendor const*)packetHeader;

        //Check if our module is meant and we should trigger an action
        if (packet->moduleId == vendorModuleId)
        {
            if (packet->actionType == VendorTemplateModuleActionResponseMessages::COMMAND_ONE_MESSAGE_RESPONSE)
            {

            }
        }
    }

    //Parse component actuator messages
    if(packetHeader->messageType == MessageType::COMPONENT_ACT && sendData->dataLength >= SIZEOF_CONN_PACKET_COMPONENT_MESSAGE_VENDOR){
        ConnPacketComponentMessageVendor const * packet = (ConnPacketComponentMessageVendor const *)packetHeader;

        //Check if the component_act message was adressed to our module
        if(packet->componentHeader.moduleId == vendorModuleId){
            if(
                sendData->dataLength >= SIZEOF_CONN_PACKET_COMPONENT_MESSAGE_VENDOR + 1
                && (
                    packet->componentHeader.actionType == (u8)ActorMessageActionType::WRITE
                    || packet->componentHeader.actionType == (u8)ActorMessageActionType::WRITE_ACK
                )
                && packet->componentHeader.component == (u8)VendorTemplateComponents::EXAMPLE_COMPONENT_1
                && packet->componentHeader.registerAddress == (u8)VendorTemplateComponent1Registers::EXAMPLE_LED
            ) {
                //Stop the IoModule from changing the LED state
                IoModule* ioMod = (IoModule*)GS->node.GetModuleById(ModuleId::IO_MODULE);
                if(ioMod != nullptr) ioMod->currentLedMode = LedMode::CUSTOM;

                //Change the LED state depending on the payload sent
                if(packet->payload[0]){
                    GS->ledRed.On();
                } else {
                    GS->ledRed.Off();
                }

                logt("TMOD", "LED is now %s", packet->payload[0] ? "on" : "off");

                //Send a response
                if(packet->componentHeader.actionType == (u8)ActorMessageActionType::WRITE_ACK)
                {
                    DYNAMIC_ARRAY(buffer, sizeof(ConnPacketComponentMessageVendor) + 1);
                    ConnPacketComponentMessageVendor* message = (ConnPacketComponentMessageVendor*)buffer;
                    message->componentHeader.header.messageType = MessageType::COMPONENT_SENSE;
                    message->componentHeader.header.sender = GS->node.configuration.nodeId;
                    //Answer the sender of this message
                    message->componentHeader.header.receiver = packetHeader->sender;
                    message->componentHeader.moduleId = VENDOR_TEMPLATE_MODULE_ID;
                    //Use a WRITE_RSP to answer a WRITE_ACK
                    message->componentHeader.actionType = (u8)SensorMessageActionType::WRITE_RSP;
                    message->componentHeader.component = (u16)VendorTemplateComponents::EXAMPLE_COMPONENT_1;
                    message->componentHeader.registerAddress = (u16)VendorTemplateComponent1Registers::EXAMPLE_LED;
                    message->componentHeader.requestHandle = 0;

                    //Assign the current state to the payload
                    message->payload[0] = packet->payload[0];

                    //Send the message through the mesh network
                    GS->cm.SendMeshMessage(
                        buffer,
                        SIZEOF_CONN_PACKET_COMPONENT_MESSAGE_VENDOR + 1);
                }
            }
        }
    }
}

CapabilityEntry VendorTemplateModule::GetCapability(u32 index, bool firstCall)
{
    if (index == 0) 
    {
        CapabilityEntry retVal;
        CheckedMemset(&retVal, 0, sizeof(retVal));
        retVal.type = CapabilityEntryType::METADATA;
        strcpy(retVal.manufacturer, "Example Vendor");
        strcpy(retVal.modelName   , "Example Device");
        strcpy(retVal.revision    , "1");
        
        return retVal;
    }
    else
    {
        return Module::GetCapability(index, firstCall);
    }
}

#endif //IS_ACTIVE(VENDOR_TEMPLATE_MODULE)
