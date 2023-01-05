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