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


#include <AdvertisingController.h>
#include <BeaconingModule.h>

#include <Node.h>
#include <IoModule.h>
#include <Logger.h>
#include <Utility.h>
#include <GlobalState.h>
constexpr u8 BEACONING_MODULE_CONFIG_VERSION = 1;

//This module allows a number of advertising messages to be configured.
//These will be broadcasted periodically

//The name is "adv" because the BeaconingModule was called "AdvertisingModule" before.
//Accepting both "adv" for downwards compatibility and "bcn" for clarity would have 
//created too much overhead.

BeaconingModule::BeaconingModule()
    : Module(ModuleId::BEACONING_MODULE, "adv")
{
    //Register callbacks n' stuff

    //Save configuration to base class variables
    //sizeof configuration must be a multiple of 4 bytes
    configurationPointer = &configuration;
    configurationLength = sizeof(BeaconingModuleConfiguration);

    //Set defaults
    ResetToDefaultConfiguration();
}

void BeaconingModule::ResetToDefaultConfiguration()
{
    //Set default configuration values
    configuration.moduleId = moduleId;
    configuration.moduleActive = true;
    configuration.moduleVersion = BEACONING_MODULE_CONFIG_VERSION;

    configuration.advertisingIntervalMs_deprecated = 0;
    configuration.messageCount_deprecated = 0;
    configuration.txPower_deprecated = 0;

    SET_FEATURESET_CONFIGURATION(&configuration, this);
}

void BeaconingModule::ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength)
{
#if IS_INACTIVE(GW_SAVE_SPACE)
    //Start the Module...
    //Delete previous jobs if they exist
    for(u32 i=0; i<advJobHandles.size(); i++){
        if(advJobHandles[i] != nullptr) GS->advertisingController.RemoveJob(advJobHandles[i]);
    }

    //Configure Advertising Jobs for all advertising messages
    for(u32 i=0; i < BEACONING_MODULE_MAX_MESSAGES; i++){
        if (configuration.messageData[i].messageLength == 0) continue;

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

void BeaconingModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const* packetHeader)
{
    //Must call superclass for handling
    Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

    if (packetHeader->messageType == MessageType::MODULE_TRIGGER_ACTION)
    {
        ConnPacketModule const* packet = (ConnPacketModule const*)packetHeader;

        //Check if our module is meant and we should trigger an action
        if (packet->moduleId == moduleId)
        {
            if (packet->actionType == (u8)BeaconingModuleTriggerActionMessages::ADD_MESSAGE && sendData->dataLength >= sizeof(AddBeaconingMessageMessage)) {
                const AddBeaconingMessageMessage* message = (const AddBeaconingMessageMessage*)packet->data;

                AddBeaconingMessageResponse reply;
                CheckedMemset(&reply, 0, sizeof(reply));
                reply.code = AddBeaconingMessageResponseCode::SUCCESS;

                //Check if this message is already in the advertisement. If so, we
                //just reply with a SUCCESS and don't do anything else.
                bool messageAlreadyPresent = false;
                for (u32 i = 0; i < BEACONING_MODULE_MAX_MESSAGES; i++)
                {
                    if (configuration.messageData[i].messageLength == 0) continue;

                    if (configuration.messageData[i].messageData == message->messageData
                        && configuration.messageData[i].messageLength == message->messageLength)
                    {
                        messageAlreadyPresent = true;
                        break;
                    }
                }

                if (!messageAlreadyPresent)
                {
                    u32 freeSlot = 0xFFFFFFFF;
                    for (u32 i = 0; i < BEACONING_MODULE_MAX_MESSAGES; i++)
                    {
                        if (configuration.messageData[i].messageLength == 0)
                        {
                            freeSlot = i;
                            break;
                        }
                    }
                    if (freeSlot == 0xFFFFFFFF)
                    {
                        reply.code = AddBeaconingMessageResponseCode::FULL;
                    }
                    else
                    {
                        BeaconingMessage& slot = configuration.messageData[freeSlot];
                        CheckedMemset(&slot, 0, sizeof(slot));
                        slot.messageData = message->messageData;
                        slot.messageLength = message->messageLength;

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
                            reply.code = AddBeaconingMessageResponseCode::RECORD_STORAGE_ERROR;
                        }
                        else
                        {
                            reply.code = AddBeaconingMessageResponseCode::SUCCESS;
                        }
                    }
                }

                SendModuleActionMessage(
                    MessageType::MODULE_ACTION_RESPONSE,
                    packetHeader->sender,
                    (u8)BeaconingModuleActionResponseMessages::ADD_MESSAGE_RESPONSE,
                    packet->requestHandle,
                    (const u8*)&reply,
                    sizeof(reply),
                    false
                );
            }
            if (packet->actionType == (u8)BeaconingModuleTriggerActionMessages::SET_MESSAGE && sendData->dataLength >= sizeof(SetBeaconingMessageMessage)) {
                const SetBeaconingMessageMessage* message = (const SetBeaconingMessageMessage*)packet->data;

                SetBeaconingMessageResponse reply;
                CheckedMemset(&reply, 0, sizeof(reply));
                reply.code = SetBeaconingMessageResponseCode::SUCCESS;

                if (message->slot >= BEACONING_MODULE_MAX_MESSAGES)
                {
                    reply.code = SetBeaconingMessageResponseCode::SLOT_OUT_OF_RANGE;
                }
                else if (configuration.messageData[message->slot].messageData != message->messageData
                    || configuration.messageData[message->slot].messageLength != message->messageLength)
                {
                    BeaconingMessage& slot = configuration.messageData[message->slot];
                    CheckedMemset(&slot, 0, sizeof(slot));
                    slot.messageData = message->messageData;
                    slot.messageLength = message->messageLength;

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
                        reply.code = SetBeaconingMessageResponseCode::RECORD_STORAGE_ERROR;
                    }
                    else
                    {
                        reply.code = SetBeaconingMessageResponseCode::SUCCESS;
                    }
                }

                SendModuleActionMessage(
                    MessageType::MODULE_ACTION_RESPONSE,
                    packetHeader->sender,
                    (u8)BeaconingModuleActionResponseMessages::SET_MESSAGE_RESPONSE,
                    packet->requestHandle,
                    (const u8*)&reply,
                    sizeof(reply),
                    false
                );
            }
            if (packet->actionType == (u8)BeaconingModuleTriggerActionMessages::REMOVE_MESSAGE && sendData->dataLength >= sizeof(RemoveBeaconingMessageMessage)) {
                const RemoveBeaconingMessageMessage* message = (const RemoveBeaconingMessageMessage*)packet->data;

                RemoveBeaconingMessageResponse reply;
                CheckedMemset(&reply, 0, sizeof(reply));
                reply.code = RemoveBeaconingMessageResponseCode::SUCCESS;

                if (message->slot >= BEACONING_MODULE_MAX_MESSAGES)
                {
                    reply.code = RemoveBeaconingMessageResponseCode::SLOT_OUT_OF_RANGE;
                }
                else
                {
                    BeaconingMessage& slot = configuration.messageData[message->slot];
                    CheckedMemset(&slot, 0, sizeof(slot));

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
                        reply.code = RemoveBeaconingMessageResponseCode::RECORD_STORAGE_ERROR;
                    }
                    else
                    {
                        reply.code = RemoveBeaconingMessageResponseCode::SUCCESS;
                    }
                }

                SendModuleActionMessage(
                    MessageType::MODULE_ACTION_RESPONSE,
                    packetHeader->sender,
                    (u8)BeaconingModuleActionResponseMessages::REMOVE_MESSAGE_RESPONSE,
                    packet->requestHandle,
                    (const u8*)&reply,
                    sizeof(reply),
                    false
                );
            }
        }
    }

    if (packetHeader->messageType == MessageType::MODULE_ACTION_RESPONSE) {
        ConnPacketModule const* packet = (ConnPacketModule const*)packetHeader;
        //Check if our module is meant and we should trigger an action
        if (packet->moduleId == moduleId) {

            if (packet->actionType == (u8)BeaconingModuleActionResponseMessages::ADD_MESSAGE_RESPONSE)
            {
                const AddBeaconingMessageResponse* message = (const AddBeaconingMessageResponse*)packet->data;
                logjson("NODE", "{\"type\":\"adv_add_response\",\"nodeId\":%d,\"module\":%u,\"requestHandle\":%u,\"code\":%u}" SEP,
                    packetHeader->sender,
                    (u8)ModuleId::BEACONING_MODULE,
                    (u32)packet->requestHandle,
                    (u32)message->code);
            }
            if (packet->actionType == (u8)BeaconingModuleActionResponseMessages::SET_MESSAGE_RESPONSE)
            {
                const SetBeaconingMessageResponse* message = (const SetBeaconingMessageResponse*)packet->data;
                logjson("NODE", "{\"type\":\"adv_set_response\",\"nodeId\":%d,\"module\":%d,\"requestHandle\":%u,\"code\":%u}" SEP, packetHeader->sender, (u32)moduleId, (u32)packet->requestHandle, (u32)message->code);
            }
            if (packet->actionType == (u8)BeaconingModuleActionResponseMessages::REMOVE_MESSAGE_RESPONSE)
            {
                const RemoveBeaconingMessageResponse* message = (const RemoveBeaconingMessageResponse*)packet->data;
                logjson("NODE", "{\"type\":\"adv_remove_response\",\"nodeId\":%d,\"module\":%d,\"requestHandle\":%u,\"code\":%u}" SEP, packetHeader->sender, (u32)moduleId, (u32)packet->requestHandle, (u32)message->code);
            }
        }
    }
}

#ifdef TERMINAL_ENABLED
TerminalCommandHandlerReturnType BeaconingModule::TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize)
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
                AddBeaconingMessageMessage message;
                CheckedMemset(&message, 0, sizeof(message));
                message.messageLength = Logger::ParseEncodedStringToBuffer(commandArgs[4], message.messageData.data(), message.messageData.size(), &didError);
                u8 requestHandle = commandArgsSize > 5 ? Utility::StringToU8(commandArgs[5], &didError) : 0;
                if (didError)
                {
                    return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                }

                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)BeaconingModuleTriggerActionMessages::ADD_MESSAGE,
                    requestHandle,
                    (const u8*)&message,
                    sizeof(message),
                    false
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }

            if (commandArgsSize >= 6 && TERMARGS(3, "set"))
            {
                //   0     1   2   3  4          5           6
                //action this adv add 0 00:11:22:33:44:55:66 17

                bool didError = false;
                SetBeaconingMessageMessage message;
                CheckedMemset(&message, 0, sizeof(message));
                message.slot = Utility::StringToU16(commandArgs[4], &didError);
                message.messageLength = Logger::ParseEncodedStringToBuffer(commandArgs[5], message.messageData.data(), message.messageData.size(), &didError);
                u8 requestHandle = commandArgsSize > 6 ? Utility::StringToU8(commandArgs[6], &didError) : 0;
                if (didError)
                {
                    return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                }

                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)BeaconingModuleTriggerActionMessages::SET_MESSAGE,
                    requestHandle,
                    (const u8*)&message,
                    sizeof(message),
                    false
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }

            if (commandArgsSize >= 5 && TERMARGS(3, "remove"))
            {
                //   0     1   2     3   4  5
                //action this adv remove 0 17

                bool didError = false;
                RemoveBeaconingMessageMessage message;
                CheckedMemset(&message, 0, sizeof(message));
                message.slot = Utility::StringToU16(commandArgs[4], &didError);
                u8 requestHandle = commandArgsSize > 5 ? Utility::StringToU8(commandArgs[5], &didError) : 0;
                if (didError)
                {
                    return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                }

                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)BeaconingModuleTriggerActionMessages::REMOVE_MESSAGE,
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

