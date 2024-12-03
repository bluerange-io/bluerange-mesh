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
#include "gtest/gtest.h"

#include "RegisterHandler.h"
#include "Utility.h"
#include <string>

// About JSTODO_PERSISTENCE: CAREFUL! The previous persistence implementation still assumed u16 instead of u8 for the data.
#ifdef JSTODO_PERSISTENCE
u16 TestRegisterRange(u16** buffer1, u16** buffer2, u16 oldRangeSize, u16 comp, u16 reg, std::vector<u16> data)
{
    u16 retVal = InsertRegisterRange(*buffer1, oldRangeSize, comp, reg, data.size(), data.data(), *buffer2);

    CheckedMemset(*buffer1, 255, 128 * sizeof(u16));

    u16* temp = *buffer1;
    *buffer1 = *buffer2;
    *buffer2 = temp;

    return retVal;
}

std::string bufferToString(u16* buffer, u16 size)
{
    std::string retVal = "";
    for (u32 i = 0; i < size;)
    {
        retVal += std::to_string(buffer[i++]);
        if(i != size - 1) retVal += " ";
        retVal += "[ ";
        u16 amount = buffer[i++];
        retVal += std::to_string(amount) + " : ";
        for (u32 k = 0; k < amount; k++)
        {
            retVal += "(";
            retVal += std::to_string(buffer[i++]) + " ";
            u16 length = buffer[i++];
            retVal += std::to_string(length) + " ";
            retVal += "{";
            for (u32 m = 0; m < length; m++)
            {
                retVal += std::to_string(buffer[i++]);
                if (m != length - 1) retVal += " ";
            }
            retVal += "}";
            retVal += ")";
        }
        retVal += "]";
    }
    return retVal;
}

void printBuff(u16* buffer, u16 size)
{
    std::cout << bufferToString(buffer, size) << std::endl;
}

TEST(TestRegisterHandler, BatteryTest)
{
    // Record Storage Layout:
    // u16 Component
    //   u16 amount_of_ranges
    //     u16 register
    //     u16 length
    //     u16... values
    //   ...
    // ...
    u16 buffer1[128];
    u16 buffer2[128];
    u16* b1 = buffer1;
    u16* b2 = buffer2;
    u16 recordSize = 0;

    printBuff(b1, recordSize);
    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 3, { 1, 2 });
    // X X X|1 2|X X X X X X X X X X
    printBuff(b1, recordSize);

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 7, { 1, 2, 3 });
    // X X X|1 2|X X|1 2 3|X X X X X
    printBuff(b1, recordSize);

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 5, { 1, 3, 3 });
    // X X X|1 2 1 3 3 2 3|X X X X X
    printBuff(b1, recordSize);

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 0, { 9, 7 });
    //|9 7|X|1 2 1 3 3 2 3|X X X X X
    printBuff(b1, recordSize);

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 0, { 4 });
    //|4 7|X|1 2 1 3 3 2 3|X X X X X
    printBuff(b1, recordSize);

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 1, { 8 });
    //|4 8|X|1 2 1 3 3 2 3|X X X X X
    printBuff(b1, recordSize);

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 3, { 5 });
    //|4 8|X|5 2 1 3 3 2 3|X X X X X
    printBuff(b1, recordSize);

    for (int i = 0; i < 20; i++)
    {
        recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 3 + i, { u16((u16(80 - i)) % 10) });
        printBuff(b1, recordSize);
    }
    //|4 8|X|0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1|X X X X

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 0, 2, { 5 });
    //|4 8 5 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1|X X X X
    printBuff(b1, recordSize);

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 2, { 5 });
    //|4 8 5 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1|X X X X
    printBuff(b1, recordSize);

    recordSize = 0;
    //X X X X X X X X X X X X X
    printBuff(b1, recordSize);

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 2, { 5 });
    //X X|5|X X X X X X X X X X
    printBuff(b1, recordSize);

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 4, { 7 });
    //X X|5|X|7|X X X X X X X X
    printBuff(b1, recordSize);

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 3, { 6 });
    //X X|5 6 7|X X X X X X X X
    printBuff(b1, recordSize);

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 1, { 2, 3 });
    //X|2 3 6 7|X X X X X X X X
    printBuff(b1, recordSize);

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 4, { 9, 8 });
    //X|2 3 6 9 8|X X X X X X X
    printBuff(b1, recordSize);

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 8, { 1, 2, 3 });
    //X|2 3 6 9 8|X X|1 2 3|X X
    printBuff(b1, recordSize);

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 7, { 1, 2, 3 });
    //X|2 3 6 9 8|X|1 2 3 3|X X
    printBuff(b1, recordSize);

    recordSize = TestRegisterRange(&b1, &b2, recordSize, 1, 6, { 1, 2, 3 });
    //X|2 3 6 9 8 1 2 3 3 3|X X
    printBuff(b1, recordSize);
}
#endif