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

#include <DeviceOff.h>
#include <Logger.h>
#include <Boardconfig.h>
#include <FruityHal.h>
#include <GlobalState.h>

//The time that the power button has to be pressed to switch the device off
static constexpr u32 HOLD_TIME_TO_ENTER_OFF_DS = SEC_TO_DS(1);

//If the power button is pressed for a longer time, we are not entering power off
static constexpr u32 HOLD_TIME_TO_ENTER_OFF_DS_MAX = SEC_TO_DS(5);

DeviceOff::DeviceOff()
{
}

void DeviceOff::TimerHandler(u16 passedTimeDs)
{
    //If autoPowerOff is configured, check if we exceeded our runtime and power off
    if(
        //We have a generic powerOff
        (autoPowerOffTimeDs != 0 && GS->appTimerDs > autoPowerOffTimeDs)
        || (
                //This is the power off that only triggers if unenrolled
                autoPowerOffIfUnenrolledTimeDs != 0
                && GS->appTimerDs > autoPowerOffIfUnenrolledTimeDs
                && GS->node.configuration.enrollmentState == EnrollmentState::NOT_ENROLLED
            )
    ){
        logt("WARNING", "Auto Power Off time reached");
        PrepareSystemOff();
    }

    // If a power button is configured, check its state
    if (Boardconfig->powerButton != -1)
    {
        if (IsPowerButtonPressed()) {
            powerButtonPressedTimeDs += passedTimeDs;
        } else {
            
            //If the button was pressed for more than a second, we go to system off
            //If it was pressed for more than 5 seconds, we do not power off
            if (
                powerButtonPressedTimeDs >= HOLD_TIME_TO_ENTER_OFF_DS
                && powerButtonPressedTimeDs < HOLD_TIME_TO_ENTER_OFF_DS_MAX)
            {
                PrepareSystemOff();
            }
            //If the power button was shortcly pressed, we shortly blink to let the user know the firmware is running
            else if(powerButtonPressedTimeDs && powerButtonPressedTimeDs < HOLD_TIME_TO_ENTER_OFF_DS)
            {
                //ATTENTION: Do not change blink codes as they are documented in the users manual
                //Shortly blink the green and blue led
                GS->ledGreen.On();
                GS->ledBlue.On();
                FruityHal::DelayMs(200);
                GS->ledGreen.Off();
                GS->ledBlue.Off();

            }
            // => Other power button functionality can be defined elsewhere in the firmware

            powerButtonPressedTimeDs = 0;
        }
    }
}

bool DeviceOff::IsPowerButtonPressed() {
    if(Boardconfig->powerButton == -1) return false;

    if (Boardconfig->powerButtonActiveHigh){
        return FruityHal::GpioPinRead((u32)Boardconfig->powerButton);
    } else {
        return !FruityHal::GpioPinRead((u32)Boardconfig->powerButton);
    }
}

void DeviceOff::PrepareSystemOff()
{
    logt("WARNING", "Preparing Power Off");

    //ATTENTION: Do not change LED blink codes, the user and our manual rely on these!

    //Switch all leds off first
    GS->ledRed.Off();
    GS->ledGreen.Off();
    GS->ledBlue.Off();

    //If the device has a red LED, it will blink red
    //If the device has no red LED, it will blink all other LEDs
    //It blinks five times before going to system off mode
    for(int i=0; i<10; i++){
        if(Boardconfig->led1Pin == -1) {
            GS->ledRed.Toggle();
            GS->ledGreen.Toggle();
            GS->ledBlue.Toggle();
        } else {
            GS->ledRed.Toggle();
        }
        FruityHal::DelayMs(500);
    }

    //First, we are resetting to make sure that nothing is initialized anymore
    //We are using a special RebootReason which is picked up upon reboot
    //The device will then go to SYSTEM_OFF once it enters the HandleReset method
    GS->node.Reboot(1, RebootReason::PREPARE_DEVICE_OFF);
}

void DeviceOff::GotoSystemOff()
{
    //Terminal is not initialized at this point
    log_rtt("Entering SYSTEM_OFF" EOL);

    //Make sure power button is not pressed anymore so that we do not wake up again
    while(IsPowerButtonPressed()) {}
    
    // Clear reset reason before going to sleep
    CheckedMemset(GS->ramRetainStructPtr, 0, sizeof(RamRetainStruct));
    GS->ramRetainStructPtr->rebootReason = RebootReason::FROM_OFF_STATE;
    GS->ramRetainStructPtr->crc32 = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, sizeof(RamRetainStruct) - 4);
    
    // Make sure power button is configured in sense mode and will wake the device up
    if (Boardconfig->powerButtonActiveHigh)
    {
        FruityHal::GpioConfigureInputSense((u32)Boardconfig->powerButton, FruityHal::GpioPullMode::GPIO_PIN_PULLDOWN, FruityHal::GpioSenseMode::GPIO_PIN_HIGHSENSE);
    }
    else
    {
        FruityHal::GpioConfigureInputSense((u32)Boardconfig->powerButton, FruityHal::GpioPullMode::GPIO_PIN_PULLUP, FruityHal::GpioSenseMode::GPIO_PIN_LOWSENSE);
    }

    FruityHal::SystemEnterOff(false);
}

bool DeviceOff::CheckPowerButtonLongPress(u32 delayMs) {
    static const u32 timeQuantum = 100;

    for (u32 i = 0; i < delayMs / timeQuantum; i++) {
        FruityHal::DelayMs(timeQuantum);
        if(!IsPowerButtonPressed()) {
            return false;
        }
    }

    return true;
}

void DeviceOff::HandleReset(void) {
    // Ignore device off functionality if powerButton is not set
    if (Boardconfig->powerButton == -1) return;

    //Terminal is not initialized at this point
    log_rtt("HandleReset" EOL);

    //Get the stored and the hardware reboot reason
    RebootReason rebootReason = GS->ramRetainStructPtr->rebootReason;
    RebootReason halRebootReason = FruityHal::GetRebootReason();

    //Activate GPIO Sensing for Power Button
    if (Boardconfig->powerButtonActiveHigh) {
        FruityHal::GpioConfigureInputSense((u32)Boardconfig->powerButton, FruityHal::GpioPullMode::GPIO_PIN_PULLDOWN, FruityHal::GpioSenseMode::GPIO_PIN_HIGHSENSE);
    } else {
        FruityHal::GpioConfigureInputSense((u32)Boardconfig->powerButton, FruityHal::GpioPullMode::GPIO_PIN_PULLUP, FruityHal::GpioSenseMode::GPIO_PIN_LOWSENSE);
    }

    //If we rebooted with the intention to go to SYSTEM_OFF, we put the device in SYSTEM_OFF
    if(rebootReason == RebootReason::PREPARE_DEVICE_OFF) {
        GotoSystemOff();
    }

    //If we wake up from SYSTEM_OFF and the power button is not pressed anymore,
    //the user wasn't pressing long enough so we go back to sleep
    if(halRebootReason == RebootReason::FROM_OFF_STATE) {
        bool pressedLongEnough = CheckPowerButtonLongPress(1000);

        //Wait until the user has released the button
        u32 counter = 0;
        while(IsPowerButtonPressed()){
            FruityHal::DelayMs(10);
            //We do an emergency exit in case e.g. someone misconfigured the boardconfig so that the tag
            //can still boot after 30 minutes and we can release a hotfix. Our system test would however break
            //as the timeout is too long, so we can detect the issue
            if(counter > 100 * 60 * 30) break;
            counter++;
        };

        if(pressedLongEnough)
        {
            // On wake up set a rebootreason
            CheckedMemset(GS->ramRetainStructPtr, 0, sizeof(RamRetainStruct));
            GS->ramRetainStructPtr->rebootReason = RebootReason::DEVICE_WAKE_UP;
            GS->ramRetainStructPtr->crc32 = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, sizeof(RamRetainStruct) - 4);
            
            //Terminal is not initialized at this point
            log_rtt("Device Wakeup" EOL);
        }
        else
        {
            //Terminal is not initialized at this point
            log_rtt("Still tired, going back to sleep." EOL);

            GotoSystemOff();
        }
    }
}
