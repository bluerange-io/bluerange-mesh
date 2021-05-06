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

#pragma pack(push, 1)
//Module configuration that is saved persistently
struct IoModuleConfiguration : ModuleConfiguration {
    LedMode ledMode;
    //Insert more persistent config values here
};
STATIC_ASSERT_SIZE(IoModuleConfiguration, 5);
#pragma pack(pop)

/**
 * The IoModule can be used for controlling different LED behavior and
 * some very basic pin settings.
 */
class IoModule: public Module
{
    public:
        enum class IoModuleTriggerActionMessages : u8{
            SET_PIN_CONFIG = 0,
            GET_PIN_CONFIG = 1,
            GET_PIN_LEVEL = 2,
            SET_LED = 3, //used to trigger a signaling led
            SET_IDENTIFICATION = 4, // set identification state
        };

        enum class IoModuleActionResponseMessages : u8{
            SET_PIN_CONFIG_RESULT = 0,
            PIN_CONFIG = 1,
            PIN_LEVEL = 2,
            SET_LED_RESPONSE = 3,
            SET_IDENTIFICATION_RESPONSE = 4,
        };

    private:
        //Combines a pin and its config
        static constexpr int SIZEOF_GPIO_PIN_CONFIG = 2;
        struct gpioPinConfig{
            u8 pinNumber : 5;
            u8 direction : 1; //configure pin as either input (0) or output (1) 
            u8 inputBufferConnected : 1; //disconnect input buffer when port not used to save energy
            u8 pull : 2; //pull down (1) or up (2) or disable pull (0) on pin (GpioPullMode)
            u8 driveStrength : 3; // GPIO_PIN_CNF_DRIVE_*
            u8 sense : 2; // if configured as input sense either high or low level
            u8 set : 1; // set pin or unset it
        };
        STATIC_ASSERT_SIZE(gpioPinConfig, 2);


    public:
        //####### Module messages (these need to be packed)
        #pragma pack(push)
        #pragma pack(1)

            static constexpr int SIZEOF_IO_MODULE_SET_LED_MESSAGE = 1;
            typedef struct
            {
                LedMode ledMode;

            }IoModuleSetLedMessage;
            STATIC_ASSERT_SIZE(IoModuleSetLedMessage, 1);
            
            static constexpr int SIZEOF_IO_MODULE_SET_IDENTIFICATION_MESSAGE = 1;
            typedef struct
            {
                IdentificationMode identificationMode;

            }IoModuleSetIdentificationMessage;
            STATIC_ASSERT_SIZE(IoModuleSetIdentificationMessage, 1);

        #pragma pack(pop)
        //####### Module messages end

    private:
        u8 ledBlinkPosition = 0;
        /// The remaining identification time in deci-seconds. Identification
        /// is active if this variable holds a non-zero value.
        u32 remainingIdentificationTimeDs = 0;

    public:

        DECLARE_CONFIG_AND_PACKED_STRUCT(IoModuleConfiguration);

        LedMode currentLedMode;

        IoModule();

        void ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength) override final;

        void ResetToDefaultConfiguration() override final;

        void TimerEventHandler(u16 passedTimeDs) override final;

        void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader) override final;

        #ifdef TERMINAL_ENABLED
        TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override final;
        #endif

    private:
        /// Returns true if identification is currently active.
        bool IsIdentificationActive() const;
};
