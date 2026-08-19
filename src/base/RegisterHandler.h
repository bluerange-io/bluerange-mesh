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

#include "FmTypes.h"
#include "RecordStorage.h"

// The following register ranges should be used per convention
constexpr u32 REGISTER_INFORMATION_BASE   =     0;
constexpr u32 REGISTER_CONFIGURATION_BASE = 10000;
constexpr u32 REGISTER_CONTROL_BASE       = 20000;
constexpr u32 REGISTER_DATA_BASE          = 30000;

constexpr u32 REGISTER_WRITABLE_RANGE_BASE = REGISTER_CONFIGURATION_BASE;
constexpr u32 REGISTER_WRITABLE_RANGE_SIZE = REGISTER_DATA_BASE - REGISTER_CONFIGURATION_BASE;

enum class RegisterHandlerCode : u8
{
    SUCCESS                   = 0,
    SUCCESS_CHANGE            = 1, // The write operation was successful, but the values were changed by the RegisterHandler.
    LOCATION_DISABLED         = 2, // The location that was written to is not managed by the register handler and it will not send any errors

    // The following codes are not yet used, but are reserved for successes with additional information in the future.
    SUCCESS_RESERVED_3        = 3,
    SUCCESS_RESERVED_4        = 4,
    SUCCESS_RESERVED_5        = 5,
    SUCCESS_RESERVED_6        = 6,
    SUCCESS_RESERVED_7        = 7,
    SUCCESS_RESERVED_8        = 8,
    SUCCESS_RESERVED_9        = 9,
    SUCCESS_RESERVED_10       = 10,

    // Errors
    LOCATION_UNSUPPORTED      = 11,
    VALUE_NOT_SET             = 12,
    ILLEGAL_VALUE             = 13,
    ILLEGAL_WRITE_LOCATION    = 14,
    PERSISTED_ID_MISMATCH     = 15,
    ILLEGAL_LENGTH            = 16,
    PERSISTED_ID_OUT_OF_RANGE = 17,
    NOT_IMPLEMENTED           = 18,
    NOT_WRITABLE              = 19,

    RECORD_STORAGE_CODES_START = 100,
    RECORD_STORAGE_CODES_END   = 150,
};

enum class RegisterHandlerStage : u8
{
    SUCCESS              = 0,
    EARLY_CHECK          = 1,
    GENERAL_CHECK        = 2,
    CHECK_VALUES         = 3,
    ADDR_FAIL            = 4,
    EARLY_RECORD_STORAGE = 5,
    LATE_RECORD_STORAGE  = 6,
};

struct RegisterHandlerCodeStage
{
    RegisterHandlerCode code;
    RegisterHandlerStage stage;
};

enum /*non-class*/ RegisterGeneralChecks
{
    RGC_SUCCESS              = 0,
    RGC_LOCATION_DISABLED    = 1 << 0, //RegisterHandler is not enabled for the given location, so it will also not return any errors
    RGC_LOCATION_UNSUPPORTED = 1 << 1, //RegisterHandler is enabled for the location but does not support the given read/write action, an error will be returned
    RGC_NULL_TERMINATED      = 1 << 2,
    RGC_NO_MIDDLE_NULL       = 1 << 3,



    RGC_STRING = RGC_NULL_TERMINATED | RGC_NO_MIDDLE_NULL,
};

enum class RegisterHandlerSetSource
{
    MESH,  // We received a message that told us to set this value.
    FLASH, // The node just booted, we are getting informed that this was the previous value that was marked to be persisted.
    INTERNAL, // Manually called from some other source internal to this node.
};

// Record Storage Layout:
// u16 Component
//   u16 amount_of_ranges
//     u16 register
//     u16 length
//     u8... values
//   ...
// ...
struct RegisterRange
{
    u16 reg;
    u16 length;
    u8 values[1]; // More may follow
};
// Serializes and combines an existing Record Storage entry with some new Register Range. The function assumes that newRecordStorage is
// guaranteed to be big enough and that the oldRecordStorage data is healthy. It performs NO sanity checks! It returns how many
// registers (u16s) have actually been written.
u16 InsertRegisterRange(const u8* oldRecordStorage, u16 oldRecordStorageLength, u16 newComponent, u16 newRegister, u16 newLength, const u8* newValues, u8* newRecordStorage);

/*
 * A universal value that keeps track of which type it has.
 */
class SupervisedValue
{
public:
    enum class Type
    {
        NONE,

        U8,
        U16,
        U32,
        I8,
        I16,
        I32,

        PTRU8,
        PTRU16,
        PTRU32,
        PTRI8,
        PTRI16,
        PTRI32,

        DYNAMIC_RANGE_WRITABLE,
        DYNAMIC_RANGE_READABLE,
    };

private:
    Type type = Type::NONE;
    union
    {
        u8  valueU8;
        u16 valueU16;
        u32 valueU32;
        i8  valueI8;
        i16 valueI16;
        i32 valueI32;

        u8  *ptrU8;
        u16 *ptrU16;
        u32 *ptrU32;
        i8  *ptrI8;
        i16 *ptrI16;
        i32 *ptrI32;

        u8* range_writable;
        const u8* range_readable;
    };
    u32 dynamicRangeSize = 0;
    RegisterHandlerCode error = RegisterHandlerCode::SUCCESS;
public:
    explicit SupervisedValue();

    void SetReadable(u8  value);
    void SetReadable(u16 value);
    void SetReadable(u32 value);
    void SetReadable(i8  value);
    void SetReadable(i16 value);
    void SetReadable(i32 value);
    void SetReadableRange(const u8* data, u32 size);
    void SetWritable(u8  *ptr);
    void SetWritable(u16 *ptr);
    void SetWritable(u32 *ptr);
    void SetWritable(i8  *ptr);
    void SetWritable(i16 *ptr);
    void SetWritable(i32 *ptr);
    void SetWritableRange(u8* data, u32 size);

    void ToBuffer(u8* buffer, u32 size);
    void FromBuffer(const u8* buffer, u32 size);
    void* GetValuePtrWrite();
    const void* GetValuePtrRead();

    void SetError(RegisterHandlerCode err);
    RegisterHandlerCode GetError() const;

    bool IsSet() const;
    bool IsValue() const;
    bool IsWritable() const;
    // Returns size in bytes
    u32 GetSize() const;
    Type GetType() const;
};
