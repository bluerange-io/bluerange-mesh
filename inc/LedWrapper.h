/**

Copyright (c) 2014-2015 "M-Way Solutions GmbH"
FruityMesh - Bluetooth Low Energy mesh protocol [http://mwaysolutions.com/]

This file is part of FruityMesh

FruityMesh is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

/*
 * The LED Wrapper provides conveniant access to LEDs
 * Thanks to Torbjørn Øvrebekk.
 * https://devzone.nordicsemi.com/question/18377/c-development-using-nrf51-sdk-on-keil/
 * */

#pragma once

#include <types.h>


class LedWrapper
{
private: 
    u32 m_io_msk;
    bool m_active_high;
    bool active;

public:
    LedWrapper(i8 io_num, bool active_high);
    void On(void);
    void Off(void);
    void Toggle(void);
};

