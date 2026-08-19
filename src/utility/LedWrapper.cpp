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

#include <Config.h>
#include <LedWrapper.h>
#include <GlobalState.h>


LedWrapper::LedWrapper(i8 io_num, bool active_high)
{
    Init(io_num, active_high);
}

LedWrapper::LedWrapper()
{
    //Leave uninit
}

void LedWrapper::Init(i8 io_num, bool active_high)
{
    if(io_num == -1){
        active = false;
        return;
    }
    active = true;

    m_io_pin = io_num;
    m_active_high = active_high;
    FruityHal::GpioConfigureOutput(io_num);
    //Initially disable LED
    Off();
}

void LedWrapper::On(void)
{
    if(!active) return;
        if(m_active_high) FruityHal::GpioPinSet(m_io_pin);
        else FruityHal::GpioPinClear(m_io_pin);
}

void LedWrapper::Off(void)
{
    if(!active) return;
        if(m_active_high) FruityHal::GpioPinClear(m_io_pin);
        else FruityHal::GpioPinSet(m_io_pin);
}

void LedWrapper::Toggle(void)
{
    if(!active) return;
        FruityHal::GpioPinToggle(m_io_pin);
}

void LedWrapper::Pulse(u32 amountOfPulses, u32 repeatTimeDs)
{
    //The time for a full cycle of LED pulses until they repeat
    const u32 animationTimeDs = GS->appTimerDs % repeatTimeDs;

    //Even Steps
    if((animationTimeDs / MAIN_TIMER_DS_PER_TICK) % 2 == 0){
        //Calculate the current step (on+off) and check if we are still lower than the
        //given amount of pulses
        if(animationTimeDs / MAIN_TIMER_DS_PER_TICK / 2 < amountOfPulses){
            On();
        } else {
            Off();
        }
    }
    //Uneven Steps
    else {
        Off();
    }
}

/* EOF */
