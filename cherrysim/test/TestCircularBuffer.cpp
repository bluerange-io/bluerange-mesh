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

