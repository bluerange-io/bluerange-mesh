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

#include "gtest/gtest.h"

#include <Exceptions.h>

#include <AutoSenseModule.h>
#include <AutoActModule.h>

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