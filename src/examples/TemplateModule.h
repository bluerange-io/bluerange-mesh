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

#pragma once

#include <Module.h>

/*
 * This is a template for a FruityMesh module.
 * A comment should be here to provide a least a short description of its purpose.
 */
class TemplateModule: public Module
{
    private:

        //Module configuration that is saved persistently (size must be multiple of 4)
        struct TemplateModuleConfiguration : ModuleConfiguration{
            //Insert more persistent config values here
        };

        TemplateModuleConfiguration configuration;

        enum TemplateModuleTriggerActionMessages{
            MESSAGE_0 = 0
        };

        enum TemplateModuleActionResponseMessages{
            MESSAGE_0_RESPONSE = 0
        };

        /*
        //####### Module messages (these need to be packed)
        #pragma pack(push)
        #pragma pack(1)

            #define SIZEOF_TEMPLATE_MODULE_***_MESSAGE 10
            typedef struct
            {
                //Insert values here

            }TemplateModule***Message;

        #pragma pack(pop)
        //####### Module messages end
        */


    public:
        TemplateModule();

        void ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength) override;

        void ResetToDefaultConfiguration() override;

        void TimerEventHandler(u16 passedTimeDs) override;

        void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader const * packetHeader) override;

        #ifdef TERMINAL_ENABLED
        TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override;
        #endif
};
