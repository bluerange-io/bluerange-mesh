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
#include "gtest/gtest.h"
#include "CircularBuffer.h"
#include "FmTypes.h"
#include <Utility.h>
TEST(CircularBuffer, TestLength) {
    {
        CircularBuffer<u8, 42> arr;
        ASSERT_EQ(arr.length, 42);
    }

    {
        CircularBuffer<u16, 12> arr;
        ASSERT_EQ(arr.length, 12);
    }

}

TEST(TestCircularBuffer, TestCircularAccess) {
    CircularBuffer<int, 10> arr;
    int exceedIndex = 2;
    for (int i = 0; i < arr.length + exceedIndex; i++)
    {
        arr[i] = i;
    }

    ASSERT_EQ(arr[0], 10);

    ASSERT_EQ(arr[1], 11);


}

TEST(TestCircularBuffer, TestCircularAccessInRotation) {
    CircularBuffer<int, 10> arr;
    i32 rotation = 2;
    arr.SetRotation(rotation);

    ASSERT_EQ(arr.GetRotation(), rotation);

    for (int i = 0; i < arr.length; i++)
    {
        arr[i] = i;
    }
    arr.SetRotation(0);
    ASSERT_EQ(arr[0], 8);
    ASSERT_EQ(arr[1], 9);
}
