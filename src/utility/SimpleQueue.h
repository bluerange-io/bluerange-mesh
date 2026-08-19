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

#include "FmTypes.h"

#include <cstddef>

#ifdef SIM_ENABLED
#include <type_traits>
#endif

/**
 * A simple queue implementation based on a circular buffer of fixed member size and a fixed amount of members.
 */
template <typename T, int N>
class SimpleQueue
{
private:
    T data[N];
    u32 readHead  = 0;
    u32 writeHead = 0;

    void IncHead(u32 &head)
    {
        head++;
        if (head >= N)
            head = 0;
    }

public:
    static constexpr int length = N;

    SimpleQueue()
    {
        for (int i = 0; i < N; i++)
        {
            data[i] = T();
        }
    }

    NO_DISCARD T *GetRaw()
    {
        return data;
    }

    NO_DISCARD const T *GetRaw() const
    {
        return data;
    }

    NO_DISCARD std::size_t GetAmountOfElements() const
    {
        if (writeHead >= readHead)
        {
            return writeHead - readHead;
        }
        else
        {
            return (N - readHead) + writeHead;
        }
    }

    NO_DISCARD bool IsFull() const
    {
        return GetAmountOfElements() >= (length - 1);
    }

    NO_DISCARD bool Push(const T &t)
    {
        if (IsFull())
        {
            return false;
        }

        data[writeHead] = t;
        IncHead(writeHead);

        return true;
    }

    NO_DISCARD bool Pop()
    {
        if (GetAmountOfElements() == 0)
        {
            return false;
        }

        IncHead(readHead);

        return true;
    }

    NO_DISCARD bool TryPeek(T & result)
    {
        if (GetAmountOfElements() == 0)
        {
            return false;
        }

        result = data[readHead];

        return true;
    }

    NO_DISCARD bool TryPeekAndPop(T & result)
    {
        if (TryPeek(result))
        {
            IncHead(readHead);
            return true;
        }

        return false;
    }

    template <typename Predicate>
    NO_DISCARD T * FindByPredicate(Predicate predicate)
    {
        const std::size_t size = GetAmountOfElements();

        if (size == 0)
        {
            return nullptr;
        }

        u32 head = readHead;
        for (std::size_t index = 0; index < size; ++index)
        {
            if (predicate(data[head]))
            {
                return &data[head];
            }

            IncHead(head);
        }

        return nullptr;
    }

    void Reset()
    {
        readHead  = 0;
        writeHead = 0;
    }
};