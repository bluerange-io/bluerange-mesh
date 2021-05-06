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


#include <IoModule.h>


#include <Logger.h>
#include <Utility.h>
#include <GlobalState.h>
#include <Node.h>
constexpr u8 IO_MODULE_CONFIG_VERSION = 1;
#include <cstdlib>

IoModule::IoModule()
    : Module(ModuleId::IO_MODULE, "io")
{
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
    configuration.ledMode = Conf::GetInstance().defaultLedMode;

    SET_FEATURESET_CONFIGURATION(&configuration, this);
}

void IoModule::ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength)
{
    //Do additional initialization upon loading the config
    currentLedMode = configuration.ledMode;

    //Start the Module...

}

void IoModule::TimerEventHandler(u16 passedTimeDs)
{
    //Do stuff on timer...

    if (IsIdentificationActive())
    {
        // Check if identification time has run out.
        if (remainingIdentificationTimeDs <= passedTimeDs)
        {
            // Make really sure that identification is inactive.
            remainingIdentificationTimeDs = 0;
        }
        else
        {
            // Toggle all LEDs under our control on every timer tick as long
            // as identification is active.
            GS->ledRed.Toggle();
            GS->ledGreen.Toggle();
            GS->ledBlue.Toggle();
            // Adjust the remaining identification time.
            remainingIdentificationTimeDs -= passedTimeDs;
        }
    }
    else
    {
        // If power optimizations are enabled for board we keep LEDs off - only blink for identification
        if (Boardconfig->powerOptimizationEnabled)
        {
            GS->ledRed.Off();
            GS->ledGreen.Off();
            GS->ledBlue.Off();
            return;
        }

        // Identification overrides any other LED activity / mode.

        //If the Beacon is in the enrollment network
        if(currentLedMode == LedMode::CONNECTIONS && GS->node.configuration.networkId == 1){

            GS->ledRed.On();
            GS->ledGreen.Off();
            GS->ledBlue.Off();

        }
        else if (currentLedMode == LedMode::CONNECTIONS)
        {
            //Calculate the current blink step
            ledBlinkPosition = (ledBlinkPosition + 1) % (((GS->config.meshMaxInConnections + Conf::GetInstance().meshMaxOutConnections) + 2) * 2);

            //No Connections: Red blinking, Connected: Green blinking for connection count

            BaseConnections conns = GS->cm.GetBaseConnections(ConnectionDirection::INVALID);
            u8 countHandshakeDone = 0;
            for(u32 i=0; i< conns.count; i++){
                BaseConnection *conn = conns.handles[i].GetConnection();
                if(conn != nullptr && conn->HandshakeDone()) countHandshakeDone++;
            }
            
            u8 i = ledBlinkPosition / 2;
            if(i < (Conf::GetInstance().meshMaxInConnections + Conf::GetInstance().meshMaxOutConnections)){
                if(ledBlinkPosition % 2 == 0){
                    //No connections
                    if (conns.count == 0){ GS->ledRed.On(); }
                    //Connected and handshake done
                    else if(i < countHandshakeDone) { GS->ledGreen.On(); }
                    //Connected and handshake not done
                    else if(i < conns.count) { GS->ledBlue.On(); }
                    //A free connection
                    else if(i < (GS->config.meshMaxInConnections + Conf::GetInstance().meshMaxOutConnections)) {  }
                } else {
                    GS->ledRed.Off();
                    GS->ledGreen.Off();
                    GS->ledBlue.Off();
                }
            }
        }
        else if(currentLedMode == LedMode::ON)
        {
            //All LEDs on (orange when only green and red available)
            GS->ledRed.On();
            GS->ledGreen.On();
            GS->ledBlue.On();
        }
        else if(currentLedMode == LedMode::OFF)
        {
            GS->ledRed.Off();
            GS->ledGreen.Off();
            GS->ledBlue.Off();
        }
        else if(currentLedMode == LedMode::CUSTOM)
        {
            // Controlled by other module
        }
    }
}

#ifdef TERMINAL_ENABLED
TerminalCommandHandlerReturnType IoModule::TerminalCommandHandler(const char* commandArgs[],u8 commandArgsSize)
{
    //React on commands, return true if handled, false otherwise
    if(commandArgsSize >= 3 && TERMARGS(2, moduleName))
    {
        if (TERMARGS(0, "action"))
        {
            NodeId destinationNode = Utility::TerminalArgumentToNodeId(commandArgs[1]);

            //Example:
#if IS_INACTIVE(GW_SAVE_SPACE)
            if(TERMARGS(3,"pinset"))
            {
                if (commandArgsSize < 6) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
                //Check how many GPIO ports we want to set
                u8 numExtraParams = (u8) (commandArgsSize - 4);
                u8 numPorts = numExtraParams / 2;
                u8 requestHandle = (numExtraParams % 2 == 0) ? 0 : Utility::StringToU8(commandArgs[commandArgsSize - 1]);

                DYNAMIC_ARRAY(buffer, numPorts*SIZEOF_GPIO_PIN_CONFIG);

                //Encode ports + states into the data
                for(int i=0; i<numPorts; i++){
                    gpioPinConfig* p = (gpioPinConfig*) (buffer + i*SIZEOF_GPIO_PIN_CONFIG);
                    p->pinNumber = (u8)strtoul(commandArgs[(i*2)+4], nullptr, 10);
                    p->direction = 1;
                    p->pull = (u8)FruityHal::GpioPullMode::GPIO_PIN_NOPULL;
                    p->set = TERMARGS((i*2)+5, "high") ? 1 : 0;
                }

                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)IoModuleTriggerActionMessages::SET_PIN_CONFIG,
                    requestHandle,
                    buffer,
                    numPorts*SIZEOF_GPIO_PIN_CONFIG,
                    false
                );
                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            //E.g. action 635 io led on
            else
#endif
            if(TERMARGS(3,"led"))
            {
                if (commandArgsSize < 5) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
                IoModuleSetLedMessage data;

                if(TERMARGS(4, "on")) data.ledMode= LedMode::ON;
                else if(TERMARGS(4, "off")) data.ledMode = LedMode::OFF;
                else if(TERMARGS(4, "connections")) data.ledMode = LedMode::CONNECTIONS;
                else return TerminalCommandHandlerReturnType::UNKNOWN;

                u8 requestHandle = commandArgsSize >= 6 ? Utility::StringToU8(commandArgs[5]) : 0;

                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)IoModuleTriggerActionMessages::SET_LED,
                    requestHandle,
                    (u8*)&data,
                    1,
                    false
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }

            if (TERMARGS(3, "identify"))
            {
                if (commandArgsSize < 5) 
                {
                    return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
                }
                // Define the message object.
                IoModuleSetIdentificationMessage data = {};
                // Fill in the identification mode based on the terminal
                // command arguments.
                if (TERMARGS(4, "on"))
                {
                    data.identificationMode = IdentificationMode::IDENTIFICATION_START;
                }
                else if (TERMARGS(4, "off"))
                {
                    data.identificationMode = IdentificationMode::IDENTIFICATION_STOP;
                }
                else
                {
                    return TerminalCommandHandlerReturnType::UNKNOWN;
                }
                // Parse the request handle if available.
                u8 requestHandle = commandArgsSize >= 6 ? Utility::StringToU8(commandArgs[5]) : 0;
                // Turn identification on by sending a start or stop message. This
                // message is also handled by vendor modules to start any vendor
                // identification mechanism.
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)IoModuleTriggerActionMessages::SET_IDENTIFICATION,
                    requestHandle,
                    (u8*)&data,
                    sizeof(IoModuleSetIdentificationMessage),
                    false
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }
        }
    }

    //Must be called to allow the module to get and set the config
    return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif

//void IoModule::ParseTerminalInputList(string commandName, vector<string> commandArgs)


void IoModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader)
{
    //Must call superclass for handling
    Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

    if(packetHeader->messageType == MessageType::MODULE_TRIGGER_ACTION){
        ConnPacketModule const * packet = (ConnPacketModule const *)packetHeader;
        MessageLength dataFieldLength = sendData->dataLength - SIZEOF_CONN_PACKET_MODULE;

        //Check if our module is meant and we should trigger an action
        if(packet->moduleId == moduleId){
            IoModuleTriggerActionMessages actionType = (IoModuleTriggerActionMessages)packet->actionType;
            if(actionType == IoModuleTriggerActionMessages::SET_PIN_CONFIG){

                configuration.ledMode = LedMode::OFF;
                currentLedMode = LedMode::OFF;

                //Parse the data and set the gpio ports to the requested
                for(int i=0; i<dataFieldLength; i+=SIZEOF_GPIO_PIN_CONFIG)
                {
                    gpioPinConfig const * pinConfig = (gpioPinConfig const *)(packet->data + i);

                    if (pinConfig->direction == 0) FruityHal::GpioConfigureInput(pinConfig->pinNumber, (FruityHal::GpioPullMode)pinConfig->pull);
                    else FruityHal::GpioConfigureOutput(pinConfig->pinNumber);

                    if(pinConfig->set) FruityHal::GpioPinSet(pinConfig->pinNumber);
                    else FruityHal::GpioPinClear(pinConfig->pinNumber);
                }

                //Confirmation
                SendModuleActionMessage(
                    MessageType::MODULE_ACTION_RESPONSE,
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

                IoModuleSetLedMessage const * data = (IoModuleSetLedMessage const *)packet->data;

                currentLedMode = data->ledMode;
                configuration.ledMode = data->ledMode;

                //send confirmation
                SendModuleActionMessage(
                    MessageType::MODULE_ACTION_RESPONSE,
                    packet->header.sender,
                    (u8)IoModuleActionResponseMessages::SET_LED_RESPONSE,
                    packet->requestHandle,
                    nullptr,
                    0,
                    false
                );
            }
            else if(actionType == IoModuleTriggerActionMessages::SET_IDENTIFICATION){

                const auto * data = (IoModuleSetIdentificationMessage const *)packet->data;

                switch (data->identificationMode)
                {
                    case IdentificationMode::IDENTIFICATION_START:
                        logt("IOMOD", "identification started by SET_IDENTIFICATION message");
                        // Set the remaining identification time, which
                        // activates identification.
                        remainingIdentificationTimeDs = 300;
                        // Make sure all leds are in the same state.
                        GS->ledRed.Off();
                        GS->ledGreen.Off();
                        GS->ledBlue.Off();
                        break;

                    case IdentificationMode::IDENTIFICATION_STOP:
                        logt("IOMOD", "identification stopped by SET_IDENTIFICATION message");
                        // Set the remaining identification time to zero,
                        // which deactivates the identification.
                        remainingIdentificationTimeDs = 0;
                        break;

                    default:
                        break;
                }

                // Send the action response message.
                SendModuleActionMessage(
                    MessageType::MODULE_ACTION_RESPONSE,
                    packet->header.sender,
                    (u8)IoModuleActionResponseMessages::SET_IDENTIFICATION_RESPONSE,
                    packet->requestHandle,
                    nullptr,
                    0,
                    false
                );
            }
        }
    }

    //Parse Module responses
    if(packetHeader->messageType == MessageType::MODULE_ACTION_RESPONSE){
        ConnPacketModule const * packet = (ConnPacketModule const *)packetHeader;

        //Check if our module is meant and we should trigger an action
        if(packet->moduleId == moduleId)
        {
            IoModuleActionResponseMessages actionType = (IoModuleActionResponseMessages)packet->actionType;
            if(actionType == IoModuleActionResponseMessages::SET_PIN_CONFIG_RESULT)
            {
                logjson_partial("MODULE", "{\"nodeId\":%u,\"type\":\"set_pin_config_result\",\"module\":%u,", packet->header.sender, (u8)ModuleId::IO_MODULE);
                logjson("MODULE",  "\"requestHandle\":%u,\"code\":%u}" SEP, packet->requestHandle, 0);
            }
            else if(actionType == IoModuleActionResponseMessages::SET_LED_RESPONSE)
            {
                logjson_partial("MODULE", "{\"nodeId\":%u,\"type\":\"set_led_result\",\"module\":%u,", packet->header.sender, (u8)ModuleId::IO_MODULE);
                logjson("MODULE",  "\"requestHandle\":%u,\"code\":%u}" SEP, packet->requestHandle, 0);
            }
            else if(actionType == IoModuleActionResponseMessages::SET_IDENTIFICATION_RESPONSE)
            {
                logjson_partial("MODULE", "{\"nodeId\":%u,\"type\":\"identify_response\",\"module\":%u,", packet->header.sender, (u8)ModuleId::IO_MODULE);
                logjson("MODULE",  "\"requestHandle\":%u,\"code\":%u}" SEP, packet->requestHandle, 0);
            }
        }
    }
}

bool IoModule::IsIdentificationActive() const
{
    // The remaining time is non-zero if and only if identification is
    // currently active.
    return remainingIdentificationTimeDs > 0;
}
