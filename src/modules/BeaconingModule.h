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
            u8 messageCount_deprecated; //Deprecated as of 24.08.2020. A valid adverisingMessage could also be placed in the middle of all slots. If valid, messageData[slot].messageLength != 0
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

        #ifdef TERMINAL_ENABLED
        TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override final;
        #endif
};
