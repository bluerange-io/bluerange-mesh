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

#pragma once

#include <Module.h>
#include <RegisterHandler.h>
#include <AutoSenseModule.h>

//Set these maximums in your featureset
#ifndef REGISTER_DIGITAL_OUT_NUM_MAX
#define REGISTER_DIGITAL_OUT_NUM_MAX 10
#endif
#ifndef REGISTER_DIGITAL_IN_NUM_MAX
#define REGISTER_DIGITAL_IN_NUM_MAX 10
#endif
#ifndef REGISTER_DIGITAL_IN_TOGGLE_PARIS_NUM_MAX
#define REGISTER_DIGITAL_IN_TOGGLE_PARIS_NUM_MAX 10
#endif

#pragma pack(push, 1)
//Module configuration that is saved persistently
struct IoModuleConfiguration : ModuleConfiguration {
    LedMode ledMode;
    //Insert more persistent config values here
};
STATIC_ASSERT_SIZE(IoModuleConfiguration, 5);
#pragma pack(pop)

enum class IoModuleComponent :u16 {
    BASIC_REGISTER_HANDLER_FUNCTIONALITY = 0
};

/**
 * The IoModule can be used for controlling different LED behavior and
 * some very basic pin settings.
 */
class IoModule:
    public Module,
    public AutoSenseModuleDataProvider
{
    public:

        //The default time the identification is active before being automatically stopped
        //Can be configured through the featureset
        u8 defaultIdentificationTimeDs = SEC_TO_DS(10);

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
        STATIC_ASSERT_SIZE(gpioPinConfig, SIZEOF_GPIO_PIN_CONFIG);

        VibrationPins vibrationPins = {};
        BuzzerPins buzzerPins = {};

        // Limit the number of readable pins with action pinread
        static constexpr u8 MAX_NUM_GPIO_READ_PINS = 5;
        // This symbol is used to indicate that there are no more pins to be read.
        // It is placed after the last pin in the array.
        static constexpr u8 END_OF_PIN_ARRAY_SYMBOL = 0xff;

        u8 ledBlinkPosition = 0;
        /// The remaining identification time in deci-seconds. Identification
        /// is active if this variable holds a non-zero value.
        u32 remainingIdentificationTimeDs = 0;

        /// Returns true if identification is currently active.
        bool IsIdentificationActive() const;
        void StopIdentification();


    public:
        //####### Module messages (these need to be packed)
        #pragma pack(push)
        #pragma pack(1)

            static constexpr int SIZEOF_IO_MODULE_SET_LED_MESSAGE = 1;
            typedef struct
            {
                LedMode ledMode;

            }IoModuleSetLedMessage;
            STATIC_ASSERT_SIZE(IoModuleSetLedMessage, SIZEOF_IO_MODULE_SET_LED_MESSAGE);

            static constexpr int SIZEOF_IO_MODULE_GET_PIN_MESSAGE = 2;
            typedef struct
            {
                u8 pinNumber;
                u8 pinLevel;
            }IoModuleGetPinMessage;
            STATIC_ASSERT_SIZE(IoModuleGetPinMessage, SIZEOF_IO_MODULE_GET_PIN_MESSAGE);
            
            static constexpr int SIZEOF_IO_MODULE_SET_IDENTIFICATION_MESSAGE = 1;
            typedef struct
            {
                IdentificationMode identificationMode;

            }IoModuleSetIdentificationMessage;
            STATIC_ASSERT_SIZE(IoModuleSetIdentificationMessage, SIZEOF_IO_MODULE_SET_IDENTIFICATION_MESSAGE);

        #pragma pack(pop)
        //####### Module messages end

        DECLARE_CONFIG_AND_PACKED_STRUCT(IoModuleConfiguration);

        LedMode currentLedMode;

        IoModule();

        void ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength) override final;

        void ResetToDefaultConfiguration() override final;

        void TimerEventHandler(u16 passedTimeDs) override final;

        void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader) override final;

        MeshAccessAuthorization CheckMeshAccessPacketAuthorization(BaseConnectionSendData * sendData, u8 const * data, FmKeyId fmKeyId, DataDirection direction) override final;

        #ifdef TERMINAL_ENABLED
        TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override final;
        #endif

        enum class DigitalInReadMode : u8 {
            ON_DEMAND = 0, // Default
            INTERRUPT = 1
        };

        //Information Registers
        constexpr static u16 REGISTER_DIO_OUTPUT_NUM                     = 100;
        constexpr static u16 REGISTER_DIO_INPUT_NUM                      = 101;
        constexpr static u16 REGISTER_DIO_INPUT_TOGGLE_PAIR_NUM          = 102;

        //Control Registers
        constexpr static u32 REGISTER_DIO_OUTPUT_STATE_START             = 20000;
        
        //Data Registers
        constexpr static u32 REGISTER_DIO_INPUT_STATE_START              = 30000;
        constexpr static u32 REGISTER_DIO_TOGGLE_PAIR_START              = 30100;
        constexpr static u32 REGISTER_DIO_INPUT_LAST_ACTIVE_TIME_START   = 31000;
        constexpr static u32 REGISTER_DIO_INPUT_LAST_HOLD_TIME_START     = 31400;

        // AutoSenseModuleDataProvider
        void RequestData(u16 component, u16 register_, u8 length, AutoSenseModuleDataConsumer* provideTo) override;

        static void GpioHandler(u32 pin, FruityHal::GpioTransition transition);

        //#### Configuration for Generic Register Access
    TESTER_PUBLIC:
        // Digital Outputs that are accessible via Registers
        typedef struct {
            u8 pin;
            u8 activeHigh;
            u8 state;
        } DigitalOutPinSetting;
        u8 numDigitalOutPinSettings = 0;
        std::array<DigitalOutPinSetting, REGISTER_DIGITAL_OUT_NUM_MAX> digitalOutPinSettings = {};

        // Digital Inputs that are accessible via Registers
        typedef struct {
            u8 pin;
            u8 activeHigh;
            DigitalInReadMode readMode;
            u32 lastActiveTimeDs;
            u32 lastHoldTimeDs;
        } DigitalInPinSetting;
        u8 numDigitalInPinSettings = 0;
        std::array<DigitalInPinSetting, REGISTER_DIGITAL_IN_NUM_MAX> digitalInPinSettings = {};

        //Toggle Pairs contain two indices referencing interrupt based digital inputs
        typedef struct {
            u8 pinIndexA;
            u8 pinIndexB;
        } DigitalInTogglePair;
        u8 numDigitalInTogglePairSettings = 0;
        std::array<DigitalInTogglePair, REGISTER_DIGITAL_IN_TOGGLE_PARIS_NUM_MAX> digitalInTogglePairSettings = {};

    public:
        //Should be called through setCustomModuleSettings in the boardconfig
        void AddDigitalOutForBoard(u8 pin, bool activeHigh) {
            if (!moduleStarted && numDigitalOutPinSettings < REGISTER_DIGITAL_OUT_NUM_MAX) {
                digitalOutPinSettings[numDigitalOutPinSettings].pin = pin;
                digitalOutPinSettings[numDigitalOutPinSettings].activeHigh = activeHigh ? 1 : 0;
                numDigitalOutPinSettings++;
            } else {
                logt("ERROR", "AddDigitalOutForBoard failed");
            }
        }

        //Add a digital input pin
        void AddDigitalInForBoard(u8 pin, bool activeHigh, DigitalInReadMode readMode) {
            if (!moduleStarted && numDigitalInPinSettings < REGISTER_DIGITAL_IN_NUM_MAX) {
                digitalInPinSettings[numDigitalInPinSettings].pin = pin;
                digitalInPinSettings[numDigitalInPinSettings].activeHigh = activeHigh ? 1 : 0;
                digitalInPinSettings[numDigitalInPinSettings].readMode = readMode;
                numDigitalInPinSettings++;
            }
            else {
                logt("ERROR", "AddDigitalInForBoard failed");
            }
        }

        //Specify the pin indices (indices are assigned in the order that the digital inputs were created)
        //pin A switches the toggle to OFF, pin B switches to ON
        void AddTogglePairForBoard(i8 pinIndexA, u8 pinIndexB) {
            if (
                !moduleStarted
                && numDigitalInTogglePairSettings < REGISTER_DIGITAL_IN_TOGGLE_PARIS_NUM_MAX
                && pinIndexA != pinIndexB
                && pinIndexA < numDigitalInPinSettings // PinIndexA and B must be configured as digital inputs before
                && pinIndexB < numDigitalInPinSettings
                && digitalInPinSettings[pinIndexA].readMode == DigitalInReadMode::INTERRUPT // PinIndexA and B must be configured interrupt based
                && digitalInPinSettings[pinIndexB].readMode == DigitalInReadMode::INTERRUPT
            ) {

                digitalInTogglePairSettings[numDigitalInTogglePairSettings].pinIndexA = pinIndexA;
                digitalInTogglePairSettings[numDigitalInTogglePairSettings].pinIndexB = pinIndexB;
                numDigitalInTogglePairSettings++;
            }
            else {
                logt("ERROR", "AddTogglePairForBoard failed");
            }
        }

#if IS_ACTIVE(REGISTER_HANDLER)
    protected:
        virtual RegisterGeneralChecks GetGeneralChecks(u16 component, u16 reg, u16 length) const override final;
        virtual void MapRegister(u16 component, u16 register_, SupervisedValue& out, u32& persistedId) override final;
        virtual void ChangeValue(u16 component, u16 register_, u8* values, u16 length) override final;
#endif //IS_ACTIVE(REGISTER_HANDLER)
};

