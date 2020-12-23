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
#pragma once

#ifdef SIM_ENABLED
#include <type_traits>
#endif
#include<Utility.h>
#include <cstring>
#include "FmTypes.h"

template<typename T, int N>

/**
 * A simple circular buffer implementation.
 */
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