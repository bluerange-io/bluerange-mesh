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

#include <DeviceOff.h>
#include <Logger.h>
#include <Boardconfig.h>
#include <FruityHal.h>
#include <GlobalState.h>

static constexpr u32 HOLD_TIME_TO_ENTER_OFF_DS = 10;

DeviceOff::DeviceOff()
{
}

static bool IsPowerButtonPressed(void) {
    if (Boardconfig->powerButtonActiveHigh)
    {
        return FruityHal::GpioPinRead((u32)Boardconfig->powerButton);
    }
    else
    {
        return !FruityHal::GpioPinRead((u32)Boardconfig->powerButton);
    }
}

static void Sleep(void) {
    logt("OFF", "Entering POWER OFF");

    while(IsPowerButtonPressed()) {}
    
    // Clear reset reason before going to sleep
    CheckedMemset(GS->ramRetainStructPtr, 0, sizeof(RamRetainStruct));
    GS->ramRetainStructPtr->rebootReason = RebootReason::UNKNOWN_BUT_BOOTED;
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

    // while(true) {}
}

static bool IsDeviceOffReason(RebootReason resetReason) {
    return resetReason == RebootReason::DEVICE_OFF;
}

static bool IsSystemOffReset(RebootReason resetReason) {
    return resetReason == RebootReason::FROM_OFF_STATE;
}

static bool WaitForPowerButton(uint32_t delayMs) {
    static const uint32_t timeQuantum = 100;

    for (uint32_t i = 0; i < delayMs / timeQuantum; i++) {
        FruityHal::DelayMs(timeQuantum);
        if(!IsPowerButtonPressed()) {
            return false;
        }
    }

    return true;
}

void DeviceOff::HandleReset(void) {
    // Ignore device off if powerButton is not set
    if (Boardconfig->powerButton == -1) return;

    logt("OFF", "Reset");

    RebootReason rebootReason = GS->ramRetainStructPtr->rebootReason;
    if (Boardconfig->powerButtonActiveHigh)
    {
        FruityHal::GpioConfigureInputSense((u32)Boardconfig->powerButton, FruityHal::GpioPullMode::GPIO_PIN_PULLDOWN, FruityHal::GpioSenseMode::GPIO_PIN_HIGHSENSE);
    }
    else
    {
        FruityHal::GpioConfigureInputSense((u32)Boardconfig->powerButton, FruityHal::GpioPullMode::GPIO_PIN_PULLUP, FruityHal::GpioSenseMode::GPIO_PIN_LOWSENSE);
    }
    RebootReason otherrebootReason = FruityHal::GetRebootReason();

    if(IsDeviceOffReason(rebootReason) && (IsPowerButtonPressed())) {
        WaitForPowerButton(1000);
        Sleep();
    }

    if(IsSystemOffReset(otherrebootReason) && !IsPowerButtonPressed()) {
        Sleep();
    }

    if(IsSystemOffReset(otherrebootReason) && IsPowerButtonPressed()) {
        if(!WaitForPowerButton(1000)) {
            Sleep();
        }

        // On wake up set a rebootreason
        CheckedMemset(GS->ramRetainStructPtr, 0, sizeof(RamRetainStruct));
        GS->ramRetainStructPtr->rebootReason = RebootReason::DEVICE_WAKE_UP;
        GS->ramRetainStructPtr->crc32 = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, sizeof(RamRetainStruct) - 4);
        logt("OFF", "Wakeup");

        while(IsPowerButtonPressed());
    }
}

void DeviceOff::TimerHandler(u16 passedTimeDs)
{
    // Ignore device off if powerButton is not set
    if (Boardconfig->powerButton == -1) return;

    if (IsPowerButtonPressed())
    {
        buttonPressedTimeDs += passedTimeDs;
        if (buttonPressedTimeDs >= HOLD_TIME_TO_ENTER_OFF_DS)
        {
            // Blink twice before going to system off mode
            GS->ledRed.On();
            GS->ledGreen.On();
            GS->ledBlue.On();
            FruityHal::DelayMs(100);
            GS->ledRed.Off();
            GS->ledGreen.Off();
            GS->ledBlue.Off();
            FruityHal::DelayMs(100);
            GS->ledRed.On();
            GS->ledGreen.On();
            GS->ledBlue.On();
            FruityHal::DelayMs(100);
            GS->ledRed.Off();
            GS->ledGreen.Off();
            GS->ledBlue.Off();
            GS->node.Reboot(1, RebootReason::DEVICE_OFF);
        }
    }
    else
    {
        buttonPressedTimeDs = 0;
    }
}
