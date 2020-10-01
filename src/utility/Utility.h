////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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

#include <FmTypes.h>
#include <Config.h>
#include <Module.h>
#include <type_traits>

typedef struct Aes128Block {
    uint8_t data[16];
}Aes128Block;

//A type so that we are able to return all kinds of moduleId as a string for printing
//Contains either a moduleId: 123 or a VendorModuleId including quotes and \0 terminator: "0x12345678"
typedef std::array<char, 13> ModuleIdString;

class Module;
class RecordStorageEventListener;

//Regarding the following macro:
//&((dst)[0])                                        makes sure that we have a pointer, even if an array was passed.
//decltype(&((dst)[0]))                              gets the pointer type, for example T*
//std::remove_pointer<decltype(&((dst)[0]))>::type   removes the pointer from the type, so that we only have the type T.
//The following is not sufficient:
//  decltype((dst)[0])
//Because it would not work with an array of pointers, because decltype((dst)[0]) is a reference in that case, not a ptr!
#define CheckedMemset(dst, val, size) \
{\
    static_assert( std::is_pod  <std::remove_pointer<decltype(&((dst)[0]))>::type>::value \
                || std::is_union<std::remove_pointer<decltype(&((dst)[0]))>::type>::value, "Tried to call memset on non pod type!"); /*CODE_ANALYZER_IGNORE Just a string.*/ \
    memset((dst), (val), (size)); /*CODE_ANALYZER_IGNORE Implementation of CheckedMemset*/ \
}
#define CheckedMemcpy(dst, src, size) \
{\
    static_assert( std::is_pod  <std::remove_pointer<decltype(&((dst)[0]))>::type>::value \
                || std::is_union<std::remove_pointer<decltype(&((dst)[0]))>::type>::value, "Tried to call memcpy on non pod type!"); /*CODE_ANALYZER_IGNORE Just a string.*/ \
    memcpy((dst), (src), (size)); /*CODE_ANALYZER_IGNORE Implementation of CheckedMemcpy*/ \
}

/*
 * The Utility class holds a number of auxiliary functions
 */
namespace Utility
{
    const char serialAlphabet[] = "BCDFGHJKLMNPQRSTVWXYZ123456789";

    //General methods for loading settings
    u32 GetSettingsPageBaseAddress();
    RecordStorageResultCode SaveModuleSettingsToFlash(const Module* module, ModuleConfiguration* configurationPointer, const u16 configurationLength, RecordStorageEventListener* listener, u32 userType, u8* userData, u16 userDataLength);

    //Serial number and version utilities
    u32 GetIndexForSerial(const char* serialNumber, bool *didError = nullptr);
    void GenerateBeaconSerialForIndex(u32 index, char* serialBuffer); //Attention: Serial buffer must be NODE_SERIAL_NUMBER_MAX_CHAR_LENGTH big
    void GetVersionStringFromInt(const u32 version, char* outputBuffer);

    //ModuleId stuff
    bool IsVendorModuleId(ModuleId moduleId);
    bool IsVendorModuleId(ModuleIdWrapper moduleId);

    ModuleIdWrapper GetWrappedModuleId(u16 vendorId, u8 subId);
    ModuleIdWrapper GetWrappedModuleId(ModuleId moduleId);

    ModuleId GetModuleId(ModuleIdWrapper wrappedModuleId);

    ModuleIdWrapper GetWrappedModuleIdFromTerminal(const char* commandArg, bool* didError = nullptr);
    ModuleIdString GetModuleIdString(ModuleIdWrapper wrappedModuleId);

    //Random functionality
    u32 GetRandomInteger(void);

    //CRC calculation
    uint8_t CalculateCrc8(const u8* data, u16 dataLength);
    uint16_t CalculateCrc16(const uint8_t * p_data, const uint32_t size, const uint16_t * p_crc);
    u32 CalculateCrc32(const u8* message, const u32 messageLength, u32 previousCrc = 0);
    u32 CalculateCrc32String(const char* message, u32 previousCrc = 0);

    //Encryption Functionality
    void Aes128BlockEncrypt(const Aes128Block* messageBlock, const Aes128Block* key, Aes128Block* encryptedMessage);
    void XorWords(const u32* src1, const u32* src2, const u8 numWords, u32* out);
    void XorBytes(const u8* src1, const u8* src2, const u8 numBytes, u8* out);

    //Memory modification
    void SwapBytes(u8 *data, const size_t length);//Reverses the direction of bytes according to the length
    u16 SwapU16( u16 val );
    u32 SwapU32( u32 val );
    bool CompareMem(const u8 byte, const u8* data, const u16 dataLength);

    //String manipulation
    void ToUpperCase(char* str);

    //Other
    u16 ByteToAsciiHex(u8 b);
    u32 ByteFromAsciiHex(char* asciiHex, u8 numChars);
    void LogRebootJson();

    bool Contains(const u8* data, const u32 length, const u8 searchValue);

    bool IsPowerOfTwo(u32 val);
    u32 NextMultipleOf(u32 val, u32 multiple);

    NodeId TerminalArgumentToNodeId(const char* arg, bool* didError = nullptr);

    bool IsUnknownRebootReason(RebootReason rebootReason);

    char* FindLast(char* str, const char* search);

    static constexpr u32 INVALID_BACKOFF_START_TIME = UINT32_MAX;
    bool ShouldBackOffIvTrigger(u32 timerDs, u16 passedTimeDs, u32 startTimeDs, const u32* backOffIvsDs, u16 backOffIvsSize);

    u16 ToAlignedU16(const void* ptr);
    i16 ToAlignedI16(const void* ptr);
    u32 ToAlignedU32(const void* ptr);
    i32 ToAlignedI32(const void* ptr);

    //The outDidError varaible can be nullptr, in which case it is ignored.
    //If it's not set to nullptr, the underlying value must be initialized
    //with false. The functions only set it to true or don't change it at
    //all. That way you can use the same variable for several calls.
    long          StringToLong        (const char *str, bool *outDidError = nullptr);
    unsigned long StringToUnsignedLong(const char *str, bool *outDidError = nullptr);
    u8            StringToU8          (const char *str, bool *outDidError = nullptr);
    u16           StringToU16         (const char *str, bool *outDidError = nullptr);
    u32           StringToU32         (const char *str, bool *outDidError = nullptr);
    i8            StringToI8          (const char *str, bool *outDidError = nullptr);
    i16           StringToI16         (const char *str, bool *outDidError = nullptr);
    i32           StringToI32         (const char *str, bool *outDidError = nullptr);

    template<typename T>
    T Clamp(T val, T minValue, T maxValue)
    {
        if (val < minValue) return minValue;
        if (val > maxValue) return maxValue;
        return val;
    }
}

