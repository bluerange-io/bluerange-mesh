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
#include<Utility.h>
#include <cstring>
#include "FmTypes.h"

/**
 * A simple circular buffer implementation.
 */
template<typename T, int N>
class CircularBuffer
{
private:
    T data[N];
    i32 rotation = 0;

public:
    static constexpr int length = N;

    CircularBuffer()
    {
        for (int i = 0; i < N; i++)
        {
            data[i] = T();
        }
    }

    T* GetRaw()
    {
        return data;
    }

    const T* GetRaw() const
    {
        return data;
    }

    T& operator[](int index) {
#ifdef SIM_ENABLED
        if (index < 0)
        {
            SIMEXCEPTION(IndexOutOfBoundsException); //LCOV_EXCL_LINE assertion
        }
#endif
        return data[(index+rotation) % N];
    }

    const T& operator[](int index) const {
#ifdef SIM_ENABLED
        if (index < 0)
        {
            SIMEXCEPTION(IndexOutOfBoundsException); //LCOV_EXCL_LINE assertion
        }
#endif
        return data[(index + rotation) % N];
    }

    i32 GetRotation() {
        return rotation;
    }

    void SetRotation(i32 rotation) {
        this->rotation = rotation;
    }
};