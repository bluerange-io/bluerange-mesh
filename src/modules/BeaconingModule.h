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

#pragma once

#include <Module.h>
#include "AdvertisingController.h"
#include <array>

// Be sure to check the advertising controller for the maximum number of supported jobs before increasing this
constexpr int BEACONING_MODULE_MAX_MESSAGES = 1;
constexpr int BEACONING_MODULE_MAX_MESSAGE_LENGTH = 31;

/*
 * The BeaconingModule is used to broadcast user-data that is not related with
 * the mesh during times where no mesh discovery is ongoing. It is used
 * to broadcast messages to smartphones or other devices from all mesh nodes.
 */
class BeaconingModule: public Module
{
    private:
        enum class BeaconingModuleTriggerActionMessages : u8
        {
            ADD_MESSAGE    = 0,
            SET_MESSAGE    = 1,
            REMOVE_MESSAGE = 2,
        };

        enum class BeaconingModuleActionResponseMessages : u8
        {
            ADD_MESSAGE_RESPONSE    = 0,
            SET_MESSAGE_RESPONSE    = 1,
            REMOVE_MESSAGE_RESPONSE = 2,
        };

        #pragma pack(push, 1)
        struct BeaconingMessage{
            u8 messageId_deprecated; //Unused but set to some values in old set_config commands. Must not be used!
            u8 forceNonConnectable_deprecated : 1; //Unused but set to some values in old set_config commands. Must not be used!
            u8 forceConnectable_deprecated : 1; //Unused but set to some values in old set_config commands. Must not be used!
            u8 reserved : 1;
            u8 messageLength : 5;
            std::array<u8, BEACONING_MODULE_MAX_MESSAGE_LENGTH> messageData;
        };

        //Module configuration that is saved persistently
        struct BeaconingModuleConfiguration : ModuleConfiguration{
            //The interval at which the device advertises
            u16 advertisingIntervalMs_deprecated; //Unused but set to some values in old set_config commands. Must not be used!
            //Number of messages
            u8 messageCount_deprecated; //Deprecated as of 24.08.2020. A valid advertisingMessage could also be placed in the middle of all slots. If valid, messageData[slot].messageLength != 0
            i8 txPower_deprecated; //Unused but set to some values in old set_config commands. Must not be used!
            std::array<BeaconingMessage, BEACONING_MODULE_MAX_MESSAGES> messageData;
            //Insert more persistent config values here
        };

        struct AddBeaconingMessageMessage
        {
            u8 messageLength;
            std::array<u8, BEACONING_MODULE_MAX_MESSAGE_LENGTH> messageData;
        };
        STATIC_ASSERT_SIZE(AddBeaconingMessageMessage, 32);
        enum class AddBeaconingMessageResponseCode : u8
        {
            SUCCESS = 0,
            FULL = 1,
            RECORD_STORAGE_ERROR = 2,
        };
        struct AddBeaconingMessageResponse
        {
            AddBeaconingMessageResponseCode code;
        };
        STATIC_ASSERT_SIZE(AddBeaconingMessageResponse, 1);

        struct SetBeaconingMessageMessage
        {
            u8 messageLength;
            u16 slot;
            std::array<u8, BEACONING_MODULE_MAX_MESSAGE_LENGTH> messageData;
        };
        STATIC_ASSERT_SIZE(SetBeaconingMessageMessage, 34);
        enum class SetBeaconingMessageResponseCode : u8
        {
            SUCCESS = 0,
            SLOT_OUT_OF_RANGE = 1,
            RECORD_STORAGE_ERROR = 2,
        };
        struct SetBeaconingMessageResponse
        {
            SetBeaconingMessageResponseCode code;
        };
        STATIC_ASSERT_SIZE(SetBeaconingMessageResponse, 1);

        struct RemoveBeaconingMessageMessage
        {
            u16 slot;
        };
        STATIC_ASSERT_SIZE(RemoveBeaconingMessageMessage, 2);
        enum class RemoveBeaconingMessageResponseCode : u8
        {
            SUCCESS = 0,
            SLOT_OUT_OF_RANGE = 1,
            RECORD_STORAGE_ERROR = 2,
        };
        struct RemoveBeaconingMessageResponse
        {
            RemoveBeaconingMessageResponseCode code;
        };
        STATIC_ASSERT_SIZE(RemoveBeaconingMessageResponse, 1);
        #pragma pack(pop)

        std::array<AdvJob*, BEACONING_MODULE_MAX_MESSAGES> advJobHandles{};

        #pragma pack(push)
        #pragma pack(1)

        typedef struct
        {
            u8 debugPacketIdentifier;
            NodeId senderId;
            u16 connLossCounter;
            std::array<NodeId, 4> partners;
            std::array<i8, 3> rssiVals;
            std::array<u8, 3> droppedVals;

        } BeaconingModuleDebugMessage;

        #pragma pack(pop)


    public:
        DECLARE_CONFIG_AND_PACKED_STRUCT(BeaconingModuleConfiguration);

        BeaconingModule();

        void ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength) override final;

        void ResetToDefaultConfiguration() override final;

        //Receiving
        void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const* packetHeader) override final;

        MeshAccessAuthorization CheckMeshAccessPacketAuthorization(BaseConnectionSendData * sendData, u8 const * data, FmKeyId fmKeyId, DataDirection direction) override final;

#ifdef TERMINAL_ENABLED
        TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override final;
        #endif
};
