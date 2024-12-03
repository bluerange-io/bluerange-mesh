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
#include "RegisterHandler.h"
#include "GlobalState.h"

#if IS_ACTIVE(REGISTER_HANDLER)

SupervisedValue::SupervisedValue() :
    valueU32(0)
{
    // Do nothing
}

void SupervisedValue::SetReadable(u8  value) { valueU8  = value; type = Type::U8; }
void SupervisedValue::SetReadable(u16 value) { valueU16 = value; type = Type::U16; }
void SupervisedValue::SetReadable(u32 value) { valueU32 = value; type = Type::U32; }
void SupervisedValue::SetReadable(i8  value) { valueI8  = value; type = Type::I8; }
void SupervisedValue::SetReadable(i16 value) { valueI16 = value; type = Type::I16; }
void SupervisedValue::SetReadable(i32 value) { valueI32 = value; type = Type::I32; }
void SupervisedValue::SetWritable(u8  *ptr) { ptrU8  = ptr; type = Type::PTRU8; }
void SupervisedValue::SetWritable(u16 *ptr) { ptrU16 = ptr; type = Type::PTRU16; }
void SupervisedValue::SetWritable(u32 *ptr) { ptrU32 = ptr; type = Type::PTRU32; }
void SupervisedValue::SetWritable(i8  *ptr) { ptrI8  = ptr; type = Type::PTRI8; }
void SupervisedValue::SetWritable(i16 *ptr) { ptrI16 = ptr; type = Type::PTRI16; }
void SupervisedValue::SetWritable(i32 *ptr) { ptrI32 = ptr; type = Type::PTRI32; }

void SupervisedValue::SetWritableRange(u8* data, u32 size)
{
    range_writable = data;
    dynamicRangeSize = size;
    type = Type::DYNAMIC_RANGE_WRITABLE;
}

void SupervisedValue::SetReadableRange(const u8* data, u32 size)
{
    range_readable = data;
    dynamicRangeSize = size;
    type = Type::DYNAMIC_RANGE_READABLE;
}

void SupervisedValue::ToBuffer(u8* buffer, u32 size)
{
    CheckedMemcpy(buffer, GetValuePtrRead(), GetSize() < size ? GetSize() : size);
}

void SupervisedValue::FromBuffer(const u8* buffer, u32 size)
{
    u8* ptr = (u8*)GetValuePtrWrite();
    CheckedMemcpy(ptr, buffer, GetSize() < size ? GetSize() : size);
}

void* SupervisedValue::GetValuePtrWrite()
{
    switch (type)
    {
    case Type::PTRU8:  return ptrU8;
    case Type::PTRU16: return ptrU16;
    case Type::PTRU32: return ptrU32;
    case Type::PTRI8:  return ptrI8;
    case Type::PTRI16: return ptrI16;
    case Type::PTRI32: return ptrI32;
    case Type::DYNAMIC_RANGE_WRITABLE: return range_writable;
    default: SIMEXCEPTION(IllegalStateException); break;
    }

    SIMEXCEPTION(IllegalStateException);
    return nullptr;
}

const void* SupervisedValue::GetValuePtrRead()
{
    switch (type)
    {
    case Type::U8:  return &valueU8;
    case Type::U16: return &valueU16;
    case Type::U32: return &valueU32;
    case Type::I8:  return &valueI8;
    case Type::I16: return &valueI16;
    case Type::I32: return &valueI32;
    case Type::PTRU8:  return ptrU8;
    case Type::PTRU16: return ptrU16;
    case Type::PTRU32: return ptrU32;
    case Type::PTRI8:  return ptrI8;
    case Type::PTRI16: return ptrI16;
    case Type::PTRI32: return ptrI32;
    case Type::DYNAMIC_RANGE_WRITABLE: return range_writable;
    case Type::DYNAMIC_RANGE_READABLE: return range_readable;
    case Type::NONE: SIMEXCEPTION(IllegalStateException); break;
    }

    SIMEXCEPTION(IllegalStateException);
    return nullptr;
}

void SupervisedValue::SetError(RegisterHandlerCode err)
{
    error = err;
}

RegisterHandlerCode SupervisedValue::GetError() const
{
    return error;
}

bool SupervisedValue::IsSet() const
{
    return type != Type::NONE;
}

bool SupervisedValue::IsValue() const
{
    switch (type)
    {
    case Type::U8:
    case Type::U16:
    case Type::U32:
    case Type::I8:
    case Type::I16:
    case Type::I32:
        return true;
    default:
        return false;
    }
}

bool SupervisedValue::IsWritable() const
{
    switch (type)
    {
    case Type::PTRU8:
    case Type::PTRU16:
    case Type::PTRU32:
    case Type::PTRI8:
    case Type::PTRI16:
    case Type::PTRI32:
    case Type::DYNAMIC_RANGE_WRITABLE:
        return true;
    default:
        return false;
    }
}

u32 SupervisedValue::GetSize() const
{
    switch (type)
    {
    case Type::NONE: return 0;
    case Type::U8 : return 1;
    case Type::U16: return 2;
    case Type::U32: return 4;
    case Type::I8 : return 1;
    case Type::I16: return 2;
    case Type::I32: return 4;
    case Type::PTRU8 : return 1;
    case Type::PTRU16: return 2;
    case Type::PTRU32: return 4;
    case Type::PTRI8 : return 1;
    case Type::PTRI16: return 2;
    case Type::PTRI32: return 4;
    case Type::DYNAMIC_RANGE_WRITABLE: return dynamicRangeSize;
    case Type::DYNAMIC_RANGE_READABLE: return dynamicRangeSize;
    }
    SIMEXCEPTION(IllegalStateException);
    return 0;
}

SupervisedValue::Type SupervisedValue::GetType() const
{
    return type;
}

#define READ() oldRecordStorage[i++]
#define WRITE(val) newRecordStorage[writeHead++] = (val)
// TODO replace with CheckedMemcpy
#define COPY(amount) for(u32 warglspargl = 0; warglspargl < (amount); warglspargl++) newRecordStorage[writeHead++] = oldRecordStorage[i++]
#define SKIP(amount) i += (amount)
#define WRITE_NEW_RANGE() WRITE(newRegister); WRITE(newLength); for (u32 m = 0; m < newLength; m++) { WRITE(newValues[m]); }

#ifdef JSTODO_PERSISTENCE
void RegisterHandler::LoadFromFlash()
{
    const u16 baseId = GetRecordBaseId();
    for (u32 k = 0; k < REGISTER_RECORDS_PER_MODULE; k++)
    {
        const u16 id = baseId + k;
        RecordStorageRecord* record = GS->recordStorage.GetRecord(id);
        if (record)
        {
            if (record->recordLength % sizeof(u16) == 1)
            {
                GS->logger.LogCustomError(CustomErrorTypes::ERROR_RECORD_STORAGE_REGISTER_HANDLER, 0);
                SIMEXCEPTION(IllegalStateException);
                continue;
            }
            u16* oldRecordStorage = (u16*)record->data;
            const u16 amountOfU16 = record->recordLength / sizeof(u16);
            for (u32 i = 0; i < amountOfU16; i++)
            {
                // Basic sanity check. We need at least 2 u16 for component, amount_of_ranges
                if(amountOfU16 - i < 2) GS->logger.LogCustomError(CustomErrorTypes::ERROR_RECORD_STORAGE_REGISTER_HANDLER, 1);

                const u16 component = READ();
                const u16 amount_of_ranges = READ();
                for (u32 k = 0; k < amount_of_ranges; k++)
                {
                    // Basic sanity check. We need at least 3 u16 for component, register, value[s]
                    if (amountOfU16 - i < 3) GS->logger.LogCustomError(CustomErrorTypes::ERROR_RECORD_STORAGE_REGISTER_HANDLER, 2);
                    const u16 reg = READ();
                    const u16 length = READ();
                    // Checking that we have enough data left for the length
                    if (amountOfU16 - i < length) GS->logger.LogCustomError(CustomErrorTypes::ERROR_RECORD_STORAGE_REGISTER_HANDLER, 3);
                    SetRegisterValues(component, reg, oldRecordStorage + i, length, nullptr, 0, nullptr, 0, RegisterHandlerSetSource::FLASH);
                    SKIP(length);
                }
            }
        }
    }
}

static bool DoRangesOverlap(u16 s1, u16 l1, u16 s2, u16 l2)
{
    // s = start
    // l = length
    // e = end

    u16 e1 = s1 + l1 - 1;
    u16 e2 = s2 + l2 - 1;
    return (s2 >= s1 && s2 <= e1 + 1)
        || (e2 >= s1 && e2 <= e1 + 1)
        || (s1 >= s2 && s1 <= e2 + 1)
        || (e1 >= s2 && e1 <= e2 + 1);
}

// Checks with how many ranges the new range "overlaps". Touching does count as overlap, too. So these Ranges do overlap (both numbers inclusive)
// [4, 10] and [7, 13]
// [4, 10] and [10, 13]
// [4, 10] and [5, 6]
// But these do not overlap:
// [4, 10] and [11, 13]
static u16 DetermineAmountOfOverlaps(const u16* oldRecordStorage, u32 i, u16 amountOfRanges, u16 newRegister, u16 newLength)
{
    u16 retVal = 0;
    for (u32 k = 0; k < amountOfRanges; k++)
    {
        u16 oldRegister = READ();
        u16 oldLength = READ();
        SKIP(oldLength);
        if (DoRangesOverlap(oldRegister, oldLength, newRegister, newLength))
        {
            retVal++;
        }
    }
    return retVal;
}

static u16 DetermineOverlappingRangeSize(const u16* oldRecordStorage, u32 i, u16 amountOfOverlaps, u16 newRegister, u16 newLength)
{
    const u16 oldRegister = READ();
    const u16 oldLength = READ();
    const u16 start = (oldRegister < newRegister) ? oldRegister : newRegister;
    const u16 oldEnd = oldRegister + oldLength;
    const u16 newEnd = newRegister + newLength;
    const u16 end = (oldEnd > newEnd) ? oldEnd : newEnd;
    u16 length = end - start;
    SKIP(oldLength);
    for (u32 k = 1; k < amountOfOverlaps; k++)
    {
        const u16 reg = READ();
        const u16 len = READ();
        length += len - (start + length - reg);
    }
    return length;
}

u16 InsertRegisterRange(const u8* oldRecordStorage, u16 oldRecordStorageLength, u16 newComponent, u16 newRegister, u16 newLength, const u8* newValues, u8* newRecordStorage)
{
    u16 writeHead = 0;
    bool foundComponent = false;
    for (u32 i = 0; i < oldRecordStorageLength; i++)
    {
        u16 oldComponent = READ();
        u16 amountOfRanges = READ();
        WRITE(oldComponent);
        if (oldComponent != newComponent)
        {
            // We aren't in the right component, copy paste fast forward
            WRITE(amountOfRanges);
            for (u32 k = 0; k < amountOfRanges; k++)
            {
                COPY(1);
                u16 length = READ();
                WRITE(length);
                COPY(length);
            }
        }
        else
        {
            foundComponent = true;
            const u16 amountOfOverlaps = DetermineAmountOfOverlaps(oldRecordStorage, i, amountOfRanges, newRegister, newLength);
            const u16 newAmountOfRanges = amountOfRanges + 1 - amountOfOverlaps;
            WRITE(newAmountOfRanges);
            bool wroteRegister = false;
            for (u32 k = 0; k < amountOfRanges; k++)
            {
                u16 oldRegister = READ();
                u16 oldLength = READ();
                if (DoRangesOverlap(oldRegister, oldLength, newRegister, newLength))
                {
                    i -= 2; // So that we can reread the register and length below. Avoids special cases.
                    const u16 newMergedRangeSize = DetermineOverlappingRangeSize(oldRecordStorage, i, amountOfOverlaps, newRegister, newLength);
                    const u16 start = (oldRegister < newRegister) ? oldRegister : newRegister;
                    DYNAMIC_ARRAY(mergedRange, newMergedRangeSize * sizeof(u16));
                    CheckedMemset(mergedRange, 0, newMergedRangeSize * sizeof(u16));
                    // Write the old ranges to the merge area
                    k += amountOfOverlaps - 1;
                    for (u32 m = 0; m < amountOfOverlaps; m++)
                    {
                        u16 reg = READ();
                        u16 len = READ();
                        u16 offset = reg - start;
                        for (u32 z = 0; z < len; z++)
                        {
                            mergedRange[z + offset] = READ();
                        }
                    }
                    // Write the new range to the merge area
                    u16 offset = newRegister - start;
                    for (u32 z = 0; z < newLength; z++)
                    {
                        mergedRange[z + offset] = newValues[z];
                    }
                    WRITE(start);
                    WRITE(newMergedRangeSize);
                    for (u32 z = 0; z < newMergedRangeSize; z++)
                    {
                        WRITE(mergedRange[z]);
                    }
                    wroteRegister = true;
                }
                else if (newRegister <= oldRegister && !wroteRegister)
                {
                    wroteRegister = true;
                    WRITE_NEW_RANGE();
                    WRITE(oldRegister);
                    WRITE(oldLength);
                    COPY(oldLength);
                }
                else
                {
                    WRITE(oldRegister);
                    WRITE(oldLength);
                    COPY(oldLength);
                }
            }
            if (!wroteRegister)
            {
                WRITE_NEW_RANGE();
            }
        }
    }

    if (!foundComponent)
    {
        // The right component wasn't found, so we have to append it.
        WRITE(newComponent);
        WRITE(1); // Amount of register ranges in this component. As this is the first one, we only have one.
        WRITE_NEW_RANGE();
    }

    return writeHead;
}
#endif

#endif //IS_ACTIVE(REGISTER_HANDLER)
