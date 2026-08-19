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

#ifdef SIM_ENABLED
#include <type_traits>
#endif
#include <Utility.h>
#include "FmTypes.h"

// A class for creating and manipulating bitmasks of a specified number of bits.
template<int NUMBER_BITS>
class BitMask
{
private:
    static constexpr u32 BITS_IN_BYTE = 8;
    static constexpr u32 NUMBER_BYTES = Utility::NextMultipleOf(NUMBER_BITS, BITS_IN_BYTE) / BITS_IN_BYTE;
    u8 storage[NUMBER_BYTES] = {};

public:
    BitMask() {}

    u8* getRaw()
    {
        return storage;
    }

    u32 getNumberBytes() const
    {
        return NUMBER_BYTES;
    }

    bool get(u32 index) const
    {
        if (index >= NUMBER_BITS)
        {
            SIMEXCEPTION(IllegalArgumentException);
            return false;
        }
        const u32 byte = index / BITS_IN_BYTE;
        const u32 bit = index % BITS_IN_BYTE;

        return 1UL & (storage[byte] >> bit);
    }

    void set(u32 index, bool value)
    {
        if (index >= NUMBER_BITS)
        {
            SIMEXCEPTION(IllegalArgumentException);
            return;
        }
        const u32 byte = index / BITS_IN_BYTE;
        const u32 bit = index % BITS_IN_BYTE;

        if (value)
        {
            storage[byte] |= 1UL << bit;
        }
        else
        {
            storage[byte] &= ~(1UL << bit);
        }
    }
};