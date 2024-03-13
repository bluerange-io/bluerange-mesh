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

// A general purpose memory manager. All memory is stored in one contiguous memory block, while keeping track of the start
// addresses of all requested memory parts. Data is managed by "slots". When you want to register, unregister, or access a
// memory region you have to specify which slot you want to use. The values of each slot needs to be externally managed,
// the SlotStorage keeps no track of what each slot means, only how many bytes are there and where they are.
// One way to conceptually understand the SlotStorage is that it's an array of length NUMBER_SLOTS, of which each element
// may freely vary in size, as long as the sum of all elements length is smaller than STORAGE_SIZE.
// Accessing and modifying slot data is a constant operation, but registering and unregistering slots can be rather
// expensive and should thus be avoided if possible.
template<int NUMBER_SLOTS, int STORAGE_SIZE>
class SlotStorage
{
private:
    static_assert(NUMBER_SLOTS <= 254, "The SlotStorage currently only supports up to 254 slots!"); // 255 is reserved for INVALID_SLOT
    static_assert(STORAGE_SIZE <= ((1 << 16) - 2), "The SlotStorage currently only supports a storage size up to 2^16 - 2"); // 2^16 - 1 is reserved for INVALID_ADDR

    static constexpr u8  INVALID_SLOT = 0xff;
    static constexpr u16 INVALID_ADDR = 0xffff;

    std::array<u8, STORAGE_SIZE> storage = {};
    std::array<u16, NUMBER_SLOTS> startAddr = {};
    u16 endAddr = 0;

    u8 getNextUsedSlot(u8 slot) const
    {
        for (u8 i = slot + 1; i < NUMBER_SLOTS; i++)
        {
            if (startAddr[i] != INVALID_ADDR) return i;
        }
        return INVALID_SLOT;
    }
    u8 getPreviousUsedSlot(u8 slot) const
    {
        u8 retVal = INVALID_SLOT;
        for (u8 i = 0; i < slot; i++)
        {
            if (startAddr[i] != INVALID_ADDR) retVal = i;
        }
        return retVal;
    }

public:
    SlotStorage() 
    {
        for (u8 i = 0; i < NUMBER_SLOTS; i++)
        {
            startAddr[i] = INVALID_ADDR;
        }
    }

    u8* get(u8 slot)
    {
        // WARNING! The returned address is only valid until some slot is
        //          registered or unregistered. A single slot registration
        //          or unregistration renders all previous get calls invalid!
        //          The safest appraoch is to never store the address
        //          anywhere and always ask the SlotStorage for the address.
        // WARNING! The returned address has no promises on alignment! It's
        //          the callers responsibility to make sure the data is
        //          aligned to what ever is required.
        if (slot >= NUMBER_SLOTS)
        {
            SIMEXCEPTION(IllegalArgumentException);
            return nullptr;
        }
        if (startAddr[slot] == INVALID_ADDR)
        {
            // Most likely a bug? Did you mean to call isSlotRegistered?
            SIMEXCEPTION(IllegalArgumentException);
            return nullptr;
        }
        return storage.data() + startAddr[slot];
    }
    bool isSlotRegistered(u8 slot) const
    {
        if (slot >= NUMBER_SLOTS)
        {
            SIMEXCEPTION(IllegalArgumentException);
            return false;
        }
        return startAddr[slot] != INVALID_ADDR;
    }
    u16 spaceLeft() const
    {
        return STORAGE_SIZE - endAddr;
    }
    u16 spaceUsed() const
    {
        return endAddr;
    }
    u16 getSizeOfSlot(u8 slot) const
    {
        if (!isSlotRegistered(slot))
        {
            // Bug?
            SIMEXCEPTION(IllegalArgumentException);
            return INVALID_ADDR;
        }
        const u8 nextSlot = getNextUsedSlot(slot);
        if (nextSlot == INVALID_SLOT)
        {
            return endAddr - startAddr[slot];
        }
        else
        {
            return startAddr[nextSlot] - startAddr[slot];
        }
    }
    bool isEnoughSpaceLeftForSlotWithSize(u16 size) const
    {
        return spaceLeft() >= size;
    }
    u8* registerOrGetSlot(u8 slot, u16 size)
    {
        if (isSlotRegistered(slot) && getSizeOfSlot(slot) == size)
        {
            return get(slot);
        }
        else
        {
            return registerSlot(slot, size);
        }
    }
    u8* registerSlot(u8 slot, u16 size)
    {
        if (isSlotRegistered(slot))
        {
            u16 existingSize = getSizeOfSlot(slot);
            if (existingSize == size)
            {
                u8* addr = get(slot);
                CheckedMemset(addr, 0, size);
                return addr;
            }
            else if (existingSize >= size)
            {
                unregisterSlot(slot);
            }
            else
            {
                u16 extraSize = size - existingSize;
                if (spaceLeft() >= extraSize)
                {
                    unregisterSlot(slot);
                }
                else
                {
                    SIMEXCEPTION(BufferTooSmallException);
                    return nullptr;
                }
            }
        }
        else
        {
            if (spaceLeft() < size)
            {
                SIMEXCEPTION(BufferTooSmallException);
                return nullptr;
            }
        }
        u8 previousUsedSlot = getPreviousUsedSlot(slot);
        u16 previousSlotEndAddr = 0;
        if (previousUsedSlot != INVALID_SLOT)
        {
            previousSlotEndAddr = startAddr[previousUsedSlot] + getSizeOfSlot(previousUsedSlot);
        }
        if (previousSlotEndAddr != endAddr)
        {
            u16 memToMove = endAddr - previousSlotEndAddr;
            u8* src = storage.data() + previousSlotEndAddr + size;
            u8* dst = storage.data() + previousSlotEndAddr;
            CheckedMemmove(src, dst, memToMove);
            for (u8 i = slot + 1; i < NUMBER_SLOTS; i++)
            {
                if (startAddr[i] != INVALID_ADDR)
                {
                    startAddr[i] += size;
                }
            }
        }
        endAddr += size;
        startAddr[slot] = previousSlotEndAddr;
        return storage.data() + previousSlotEndAddr;
    }
    void unregisterSlot(u8 slot)
    {
        if (!isSlotRegistered(slot)) return;
        const u8 nextSlot = getNextUsedSlot(slot);
        const u16 slotSize = getSizeOfSlot(slot);
        if (nextSlot == INVALID_SLOT)
        {
            // Special case handling, the slot was the last one.
            startAddr[slot] = INVALID_ADDR;
            endAddr -= slotSize;
            return;
        }
        u8* slotAddr = get(slot);
        u8* nextSlotAddr = get(nextSlot);
        u16 memToMove = endAddr - startAddr[nextSlot];
        CheckedMemmove(slotAddr, nextSlotAddr, memToMove);
        endAddr -= slotSize;
        for (u8 i = slot + 1; i < NUMBER_SLOTS; i++)
        {
            if (startAddr[i] != INVALID_ADDR)
            {
                startAddr[i] -= slotSize;
            }
        }
        startAddr[slot] = INVALID_ADDR;
    }
    
};