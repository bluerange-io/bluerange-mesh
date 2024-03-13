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