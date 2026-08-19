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

/*
 * The LED Wrapper provides convenient access to LEDs
 * Thanks to Torbjorn Ovrebekk.
 * https://devzone.nordicsemi.com/question/18377/c-development-using-nrf51-sdk-on-keil/
 * */
#pragma once

#include <FmTypes.h>


class LedWrapper
{
private:
    u32 m_io_pin;
    bool m_active_high;
    bool active;

public:
    LedWrapper(i8 io_num, bool active_high);
    LedWrapper();
    void Init(i8 io_num, bool active_high);
    void On(void);
    void Off(void);
    void Toggle(void);


    // Non-blocking! Must be called repeatedly.
    // Will pulse the LED on/off each time it is called
    // until the given amount of pulses in the given cycle time
    // is reached.
    void Pulse(u32 amountOfPulses, u32 repeatTimeDs);
};
