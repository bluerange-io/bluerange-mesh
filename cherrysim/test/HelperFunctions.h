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

#include "gtest/gtest.h"

#include <Exceptions.h>

#ifndef GITHUB_RELEASE
#include <AutoSenseModule.h>
#include <AutoActModule.h>
#endif //GITHUB_RELEASE

template <typename ExceptionType, typename SetupFn, typename ActionFn>
void RetryOrFail(int maxRetries, SetupFn setupFn, ActionFn actionFn)
{
    int retry;
    for (retry = 0; retry < maxRetries; ++retry)
    {
        setupFn();
        try
        {
            Exceptions::DisableDebugBreakOnException disableDebugBreakOnException;
            actionFn();
            break;
        }
        catch (const ExceptionType &)
        {
            continue;
        }
    }
    ASSERT_LT(retry, maxRetries);
}

#ifndef GITHUB_RELEASE
struct AutoSenseTableEntryBuilder
{
#pragma pack(push)
#pragma pack(1)
    AutoSenseTableEntryV0 entry = { 0 };
#pragma pack(pop)

    AutoSenseTableEntryBuilder() {}

    std::string getEntry() const
    {
        char buffer[256];
        Logger::ConvertBufferToHexString((const u8*)&entry, sizeof(entry), buffer, sizeof(buffer));
        return buffer;
    }
};

struct AutoActTableEntryBuilder
{
#pragma pack(push)
#pragma pack(1)
    AutoActTableEntryV0 entry = { 0 };
    u8 functionList[AutoActModule::MAX_IO_SIZE] = { 0 };
#pragma pack(pop)

    AutoActTableEntryBuilder() {}

    std::string getEntry() const
    {
        char buffer[256];
        Logger::ConvertBufferToHexString((const u8*)&entry, sizeof(entry) - 1 + entry.functionListLength, buffer, sizeof(buffer));
        return buffer;
    }

    void addFunctionNoop()
    {
        entry.functionList[entry.functionListLength] = (u8)AutoActFunction::NO_OP;
        entry.functionListLength++;
    }

    void addFunctionMin(i32 min)
    {
        entry.functionList[entry.functionListLength] = (u8)AutoActFunction::MIN;
        entry.functionListLength++;
        CheckedMemcpy(entry.functionList + entry.functionListLength, &min, sizeof(min));
        entry.functionListLength += sizeof(min);
    }

    void addFunctionMax(i32 max)
    {
        entry.functionList[entry.functionListLength] = (u8)AutoActFunction::MAX;
        entry.functionListLength++;
        CheckedMemcpy(entry.functionList + entry.functionListLength, &max, sizeof(max));
        entry.functionListLength += sizeof(max);
    }

    void addFunctionValueOffset(i32 offset)
    {
        entry.functionList[entry.functionListLength] = (u8)AutoActFunction::VALUE_OFFSET;
        entry.functionListLength++;
        CheckedMemcpy(entry.functionList + entry.functionListLength, &offset, sizeof(offset));
        entry.functionListLength += sizeof(offset);
    }

    void addFunctionIntMult(i32 mult)
    {
        entry.functionList[entry.functionListLength] = (u8)AutoActFunction::INT_MULT;
        entry.functionListLength++;
        CheckedMemcpy(entry.functionList + entry.functionListLength, &mult, sizeof(mult));
        entry.functionListLength += sizeof(mult);
    }

    void addFunctionFloatMult(float mult)
    {
        entry.functionList[entry.functionListLength] = (u8)AutoActFunction::FLOAT_MULT;
        entry.functionListLength++;
        CheckedMemcpy(entry.functionList + entry.functionListLength, &mult, sizeof(mult));
        entry.functionListLength += sizeof(mult);
    }

    void addFunctionDataOffset(u8 offset)
    {
        entry.functionList[entry.functionListLength] = (u8)AutoActFunction::DATA_OFFSET;
        entry.functionListLength++;
        CheckedMemcpy(entry.functionList + entry.functionListLength, &offset, sizeof(offset));
        entry.functionListLength += sizeof(offset);
    }

    void addFunctionDataLength(u8 length)
    {
        entry.functionList[entry.functionListLength] = (u8)AutoActFunction::DATA_LENGTH;
        entry.functionListLength++;
        CheckedMemcpy(entry.functionList + entry.functionListLength, &length, sizeof(length));
        entry.functionListLength += sizeof(length);
    }

    void addFunctionReverseBytes()
    {
        entry.functionList[entry.functionListLength] = (u8)AutoActFunction::REVERSE_BYTES;
        entry.functionListLength++;
    }

    void clearFunctions()
    {
        entry.functionListLength = 0;
        CheckedMemset(functionList, 0, sizeof(functionList));
    }
};
#endif //GITHUB_RELEASE