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

#include "RegisterHandler.h"
#include "Utility.h"
#include <string>

u16 TestRegisterRange(u8** buffer1, u8** buffer2, u16 oldRangeSize, u16 comp, u16 reg, const std::vector<u8>& data)
{
    u16 retVal = InsertRegisterRange(*buffer1, oldRangeSize, comp, reg, data.size(), data.data(), *buffer2);

    CheckedMemset(*buffer1, 255, 128);

    u8* temp = *buffer1;
    *buffer1 = *buffer2;
    *buffer2 = temp;

    return retVal;
}

std::string bufferToString(u8* buffer, u16 size)
{
    std::string retVal = "";
    for (u32 i = 0; i < size;)
    {
        u16 component = Utility::ToAlignedU16(buffer + i); i += 2;
        retVal += std::to_string(component);
        if(i != size - 1) retVal += " ";
        retVal += "[ ";
        u16 amount_of_ranges = Utility::ToAlignedU16(buffer + i); i += 2;
        retVal += std::to_string(amount_of_ranges) + " : ";
        for (u32 k = 0; k < amount_of_ranges; k++)
        {
            u16 reg = Utility::ToAlignedU16(buffer + i); i += 2;
            u16 len = Utility::ToAlignedU16(buffer + i); i += 2;
            retVal += "(";
            retVal += std::to_string(reg) + " ";
            retVal += std::to_string(len) + " ";
            retVal += "{";
            for (u32 m = 0; m < len; m++)
            {
                retVal += std::to_string(buffer[i++]);
                if (m != len - 1) retVal += " ";
            }
            retVal += "}";
            retVal += ")";
        }
        retVal += "]";
    }
    return retVal;
}

void printBuff(u8* buffer, u16 size)
{
    std::cout << bufferToString(buffer, size) << std::endl;
}

TEST(TestRegisterHandler, BasicTests)
{
    // Record Storage Layout:
    // u16 Component
    //   u16 amount_of_ranges
    //     u16 register
    //     u16 length
    //     u16... values
    //   ...
    // ...

    // How to read the bufferToString output by example:
    //   ┍━━━━━━━━━━━━Component━━━━━━━━━━━━━━┑  ┍━━Component━━━┑
    //   │                                   │  │              │
    //   │     ┍━━━━━Range━━━━━┑┍━━━Range━━━┑│  │     ┍━Range━┑│
    //   │     │               ││           ││  │     │       ││ 
    //   │     │    ┍━━━Data━━┑││    ┍Data━┑││  │     │   Data││
    //   │     │    │         │││    │     │││  │     │    │ │││
    // 1 [ 2 : (1 5 {2 3 6 9 8})(8 3 {1 2 3})]1 [ 1 : (2 1 {5})]
    // │   │    │ │  │
    // │   │    │ │  ┕Single data element
    // │   │    │ ┕Length of Data segment
    // │   │    ┕Register
    // │   ┕Amount of Ranges
    // ┕Component number


    u8 buffer1[128] = {};
    u8 buffer2[128] = {};
    u8* b1 = buffer1;
    u8* b2 = buffer2;
    u16 recordSize = 0;


    ASSERT_EQ(bufferToString(b1, recordSize), "");
    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 3, { 1, 2, 3, 4 });
    // X X X|1 2 3 4|X X X X X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "0 [ 1 : (3 4 {1 2 3 4})]");
    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 7, { 5, 6, 7, 8 });
    // X X X|1 2 3 4 5 6 7 8|X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "0 [ 1 : (3 8 {1 2 3 4 5 6 7 8})]");


    recordSize = 0;
    ASSERT_EQ(bufferToString(b1, recordSize), "");
    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 3, { 1, 2 });
    // X X X|1 2|X X X X X X X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "0 [ 1 : (3 2 {1 2})]");

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 7, { 1, 2, 3 });
    // X X X|1 2|X X|1 2 3|X X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "0 [ 2 : (3 2 {1 2})(7 3 {1 2 3})]");

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 5, { 1, 3, 3 });
    // X X X|1 2 1 3 3 2 3|X X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "0 [ 1 : (3 7 {1 2 1 3 3 2 3})]");

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 0, { 9, 7 });
    //|9 7|X|1 2 1 3 3 2 3|X X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "0 [ 2 : (0 2 {9 7})(3 7 {1 2 1 3 3 2 3})]");

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 0, { 4 });
    //|4 7|X|1 2 1 3 3 2 3|X X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "0 [ 2 : (0 2 {4 7})(3 7 {1 2 1 3 3 2 3})]");

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 1, { 8 });
    //|4 8|X|1 2 1 3 3 2 3|X X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "0 [ 2 : (0 2 {4 8})(3 7 {1 2 1 3 3 2 3})]");

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 3, { 5 });
    //|4 8|X|5 2 1 3 3 2 3|X X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "0 [ 2 : (0 2 {4 8})(3 7 {5 2 1 3 3 2 3})]");

    for (int i = 0; i < 20; i++)
    {
        recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 3 + i, { u8((u8(80 - i)) % 10) });
    }
    ASSERT_EQ(bufferToString(b1, recordSize), "0 [ 2 : (0 2 {4 8})(3 20 {0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1})]");
    //|4 8|X|0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1|X X X X

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 2, { 5 });
    //|4 8 5 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1|X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "0 [ 1 : (0 23 {4 8 5 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1})]");

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 2, { 5 });
    //|4 8 5 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1|X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "0 [ 1 : (0 23 {4 8 5 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1})]1 [ 1 : (2 1 {5})]");

    recordSize = 0;
    //X X X X X X X X X X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "");

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 2, { 5 });
    //X X|5|X X X X X X X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "1 [ 1 : (2 1 {5})]");

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 4, { 7 });
    //X X|5|X|7|X X X X X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "1 [ 2 : (2 1 {5})(4 1 {7})]");

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 3, { 6 });
    //X X|5 6 7|X X X X X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "1 [ 1 : (2 3 {5 6 7})]");

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 1, { 2, 3 });
    //X|2 3 6 7|X X X X X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "1 [ 1 : (1 4 {2 3 6 7})]");

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 4, { 9, 8 });
    //X|2 3 6 9 8|X X X X X X X
    ASSERT_EQ(bufferToString(b1, recordSize), "1 [ 1 : (1 5 {2 3 6 9 8})]");

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 8, { 1, 2, 3 });
    //X|2 3 6 9 8|X X|1 2 3|X X
    ASSERT_EQ(bufferToString(b1, recordSize), "1 [ 2 : (1 5 {2 3 6 9 8})(8 3 {1 2 3})]");

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 7, { 1, 2, 3 });
    //X|2 3 6 9 8|X|1 2 3 3|X X
    ASSERT_EQ(bufferToString(b1, recordSize), "1 [ 2 : (1 5 {2 3 6 9 8})(7 4 {1 2 3 3})]");

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 6, { 1, 2, 3 });
    //X|2 3 6 9 8 1 2 3 3 3|X X
    ASSERT_EQ(bufferToString(b1, recordSize), "1 [ 1 : (1 10 {2 3 6 9 8 1 2 3 3 3})]");
}
