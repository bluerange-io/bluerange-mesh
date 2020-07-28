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


#include <AdvertisingController.h>
#include <AdvertisingModule.h>

#include <Node.h>
#include <IoModule.h>
#include <Logger.h>
#include <Utility.h>
#include <GlobalState.h>
constexpr u8 ADVERTISING_MODULE_CONFIG_VERSION = 1;

//This module allows a number of advertising messages to be configured.
//These will be broadcasted periodically

AdvertisingModule::AdvertisingModule()
    : Module(ModuleId::ADVERTISING_MODULE, "adv")
{
    //Register callbacks n' stuff

    //Save configuration to base class variables
    //sizeof configuration must be a multiple of 4 bytes
    configurationPointer = &configuration;
    configurationLength = sizeof(AdvertisingModuleConfiguration);

    //Set defaults
    ResetToDefaultConfiguration();
}

void AdvertisingModule::ResetToDefaultConfiguration()
{
    //Set default configuration values
    configuration.moduleId = moduleId;
    configuration.moduleActive = true;
    configuration.moduleVersion = ADVERTISING_MODULE_CONFIG_VERSION;

    configuration.advertisingIntervalMs_deprecated = 0;
    configuration.messageCount = 0;
    configuration.txPower_deprecated = 0;

    SET_FEATURESET_CONFIGURATION(&configuration, this);
}

void AdvertisingModule::ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength)
{
#if IS_INACTIVE(GW_SAVE_SPACE)
    //Start the Module...
    //Delete previous jobs if they exist
    for(u32 i=0; i<advJobHandles.size(); i++){
        if(advJobHandles[i] != nullptr) GS->advertisingController.RemoveJob(advJobHandles[i]);
    }

    //Configure Advertising Jobs for all advertising messages
    for(u32 i=0; i < configuration.messageCount; i++){
        AdvJob job = {
            AdvJobTypes::SCHEDULED,
            3, //Slots
            0, //Delay
            MSEC_TO_UNITS(100, CONFIG_UNIT_0_625_MS), //AdvInterval
            0, //AdvChannel
            0, //CurrentSlots
            0, //CurrentDelay
            FruityHal::BleGapAdvType::ADV_IND, //Advertising Mode
            {0}, //AdvData
            0, //AdvDataLength
            {0}, //ScanData
            0 //ScanDataLength
        };

        CheckedMemcpy(&job.advData, configuration.messageData[i].messageData.data(), configuration.messageData[i].messageLength);
        job.advDataLength = configuration.messageData[i].messageLength;

        advJobHandles[i] = GS->advertisingController.AddJob(job);
    }
#endif
}

void AdvertisingModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader const* packetHeader)
{
    //Must call superclass for handling
    Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

    if (packetHeader->messageType == MessageType::MODULE_TRIGGER_ACTION)
    {
        connPacketModule const* packet = (connPacketModule const*)packetHeader;

        //Check if our module is meant and we should trigger an action
        if (packet->moduleId == moduleId)
        {
            if (packet->actionType == (u8)AdvertisingModuleTriggerActionMessages::ADD_MESSAGE && sendData->dataLength >= sizeof(AddAdvertisingMessageMessage)) {
                const AddAdvertisingMessageMessage* message = (const AddAdvertisingMessageMessage*)packet->data;
                
                AddAdvertisingMessageReply reply;
                CheckedMemset(&reply, 0, sizeof(reply));
                reply.code = AddAdvertisingMessageReplyCode::SUCCESS;

                //Check if this message is already in the advertisement. If so, we
                //just reply with a SUCCESS and don't do anything else.
                bool messageAlreadyPresent = false;
                for (u32 i = 0; i < configuration.messageCount; i++)
                {
                    if (configuration.messageData[i].messageData == message->messageData
                        && configuration.messageData[i].messageLength == message->messageLength)
                    {
                        messageAlreadyPresent = true;
                        break;
                    }
                }

                if (!messageAlreadyPresent)
                {
                    if (configuration.messageCount >= ADVERTISING_MODULE_MAX_MESSAGES)
                    {
                        reply.code = AddAdvertisingMessageReplyCode::FULL;
                    }
                    else
                    {
                        AdvertisingMessage& slot = configuration.messageData[configuration.messageCount];
                        CheckedMemset(&slot, 0, sizeof(slot));
                        slot.messageData = message->messageData;
                        slot.messageLength = message->messageLength;
                        configuration.messageCount++;

                        ConfigurationLoadedHandler(nullptr, 0);

                        //Save the module config to flash
                        const RecordStorageResultCode err = Utility::SaveModuleSettingsToFlash(
                            this,
                            this->configurationPointer,
                            this->configurationLength,
                            nullptr,
                            0,
                            nullptr,
                            0);
                        if (err != RecordStorageResultCode::SUCCESS)
                        {
                            reply.code = AddAdvertisingMessageReplyCode::RECORD_STORAGE_ERROR;
                        }
                        else
                        {
                            reply.code = AddAdvertisingMessageReplyCode::SUCCESS;
                        }
                    }
                }

                SendModuleActionMessage(
                    MessageType::MODULE_ACTION_RESPONSE,
                    packetHeader->sender,
                    (u8)AdvertisingModuleActionResponseMessages::ADD_MESSAGE,
                    packet->requestHandle,
                    (const u8*)&reply,
                    sizeof(reply),
                    false
                );
            }
        }
    }

    if (packetHeader->messageType == MessageType::MODULE_ACTION_RESPONSE) {
        connPacketModule const* packet = (connPacketModule const*)packetHeader;
        //Check if our module is meant and we should trigger an action
        if (packet->moduleId == moduleId) {

            if (packet->actionType == (u8)AdvertisingModuleActionResponseMessages::ADD_MESSAGE)
            {
                const AddAdvertisingMessageReply* message = (const AddAdvertisingMessageReply*)packet->data;
                logjson("NODE", "{\"type\":\"adv_add\",\"nodeId\":%d,\"module\":%d,\"requestHandle\":%u,\"code\":%u}" SEP, packetHeader->sender, (u32)moduleId, (u32)packet->requestHandle, (u32)message->code);
            }
        }
    }
}

#ifdef TERMINAL_ENABLED
TerminalCommandHandlerReturnType AdvertisingModule::TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize)
{
    //React on commands, return true if handled, false otherwise
    if (commandArgsSize >= 3 && TERMARGS(2, "adv"))
    {
        if (TERMARGS(0, "action"))
        {
            const NodeId destinationNode = Utility::TerminalArgumentToNodeId(commandArgs[1]);

            if (commandArgsSize >= 5 && TERMARGS(3, "add"))
            {
                //   0     1   2   3            4          5
                //action this adv add 00:11:22:33:44:55:66 17

                bool didError = false;
                AddAdvertisingMessageMessage message;
                CheckedMemset(&message, 0, sizeof(message));
                message.messageLength = Logger::parseEncodedStringToBuffer(commandArgs[4], message.messageData.data(), message.messageData.size(), &didError);
                u8 requestHandle = commandArgsSize > 5 ? Utility::StringToU8(commandArgs[5], &didError) : 0;
                if (didError)
                {
                    return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                }

                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)AdvertisingModuleTriggerActionMessages::ADD_MESSAGE,
                    requestHandle,
                    (const u8*)&message,
                    sizeof(message),
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

