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

constexpr VendorModuleId PING_MODULE_ID = GET_VENDOR_MODULE_ID(0xABCD, 2);
constexpr u8 PING_MODULE_CONFIG_VERSION = 1;

/*
 * The PinModule is a sample implementation of a module for showcasing FruityMesh.
 */
class PingModule: public Module
{
    private:

        //Module configuration that is saved persistently (size must be multiple of 4)
        struct PingModuleConfiguration : ModuleConfiguration{
            //Insert more persistent config values here
        };

        PingModuleConfiguration configuration;

        enum PingModuleTriggerActionMessages{
            TRIGGER_PING=0
        };

        enum PingModuleActionResponseMessages{
            PING_RESPONSE=0
        };

    public:
        PingModule();

        void ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength) override;

        void ResetToDefaultConfiguration() override;

        void TimerEventHandler(u16 passedTimeDs) override;

        void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader) override;

        #ifdef TERMINAL_ENABLED
        TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override;
        #endif
};
