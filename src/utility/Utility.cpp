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

#include <Utility.h>
#include <Logger.h>
#include <mini-printf.h>
#include <RecordStorage.h>
#include <Module.h>
#include <cctype>
#include <limits>
#include "GlobalState.h"
#include <FruityHal.h>

u32 Utility::GetSettingsPageBaseAddress()
{
    const bool bootloaderAvailable = (FruityHal::GetBootloaderAddress() != 0xFFFFFFFF);
    const u32 bootloaderAddress = bootloaderAvailable ? FruityHal::GetBootloaderAddress() : FruityHal::GetCodeSize()*FruityHal::GetCodePageSize();
    const u32 appSettingsAddress = bootloaderAddress - (RECORD_STORAGE_NUM_PAGES)* FruityHal::GetCodePageSize();

    return (appSettingsAddress);
}

RecordStorageResultCode Utility::SaveModuleSettingsToFlash(const Module* module, ModuleConfiguration* configurationPointer, const u16 configurationLength, RecordStorageEventListener* listener, u32 userType, u8* userData, u16 userDataLength)
{
    if (module == nullptr || module->recordStorageId == RECORD_STORAGE_RECORD_ID_INVALID) return RecordStorageResultCode::INTERNAL_ERROR;

    return GS->recordStorage.SaveRecord(module->recordStorageId, (u8*)configurationPointer, configurationLength, listener, userType, userData, userDataLength);
}

u32 Utility::GetRandomInteger(void)
{
    ErrorType err = ErrorType::BUSY;
    u32 randomNumber = 0;

    while(err != ErrorType::SUCCESS){
        //A busy loop is fine here because the nordic spec guarantees us, that we will, at some point, get a random number. If not, the node itself is broken.
        err = FruityHal::GetRandomBytes((u8*) &randomNumber, 4);
    }

    return randomNumber;
}

//buffer should have a length of 15 bytes
//major.minor.patch - 111.222.4444
void Utility::GetVersionStringFromInt(const u32 version, char* outputBuffer)
{
    u16 major = version / 10000000UL;
    u16 minor = (version - 10000000UL * major) / 10000UL;
    u16 patch = (version - 10000000UL * major - 10000UL * minor);

    snprintf(outputBuffer, 15, "%u.%u.%u", major, minor, patch);

}

bool Utility::IsVendorModuleId(ModuleId moduleId)
{
    return IsVendorModuleId(GetWrappedModuleId(moduleId));
}

bool Utility::IsVendorModuleId(ModuleIdWrapper moduleId)
{
    if (GetModuleId(moduleId) == ModuleId::VENDOR_MODULE_ID_PREFIX) return true;
    else return false;
}

ModuleIdWrapper Utility::GetWrappedModuleId(u16 vendorId, u8 subId)
{
    ModuleIdWrapperUnion wrappedModuleId;

    wrappedModuleId.prefix = ModuleId::VENDOR_MODULE_ID_PREFIX;
    wrappedModuleId.subId = subId;
    wrappedModuleId.vendorId = vendorId;

    return wrappedModuleId.wrappedModuleId;
}

ModuleIdWrapper Utility::GetWrappedModuleId(ModuleId moduleId)
{
    ModuleIdWrapperUnion wrappedModuleId;

    wrappedModuleId.prefix = moduleId;
    wrappedModuleId.subId = 0xFF;
    wrappedModuleId.vendorId = 0xFFFF;

    return wrappedModuleId.wrappedModuleId;
}

ModuleId Utility::GetModuleId(ModuleIdWrapper wrappedModuleId)
{
    ModuleIdWrapperUnion wrapper;

    wrapper.wrappedModuleId = wrappedModuleId;

    return wrapper.prefix;
}

//This converts a module id string from the terminal into a ModuleIdWrapper, the input can either be
//a module name such as: status; it can be a decimal number for standard modules such as: 123;
//it can also be a hex number for vendor modules such as: 0xABCD01F0
ModuleIdWrapper Utility::GetWrappedModuleIdFromTerminal(const char* commandArg, bool* didError)
{
    if (commandArg != nullptr)
    {
        //First, we try to convert the moduleId to a number (either from hex or from decimal)
        bool didErrorInt = false;
        ModuleIdWrapper moduleId = Utility::StringToU32(commandArg, &didErrorInt);

        //In case it was not possible to convert it to a number, we check the string against all of our available modules
        if (didErrorInt)
        {
            for (u32 i = 0; i < GS->amountOfModules; i++) {
                if (strcmp(GS->activeModules[i]->moduleName, commandArg) == 0) return GS->activeModules[i]->vendorModuleId;
            }
        }
        else {
            if (moduleId < UINT8_MAX) return GetWrappedModuleId((ModuleId)moduleId);
            else return moduleId;
        }
    }

    if(didError != nullptr) *didError = true;
    return INVALID_WRAPPED_MODULE_ID;
}

//ATTENTION, you need to access .data() when using this with printf!
ModuleIdString Utility::GetModuleIdString(ModuleIdWrapper wrappedModuleId)
{
    ModuleIdString out;

    if (IsVendorModuleId(wrappedModuleId)) {
        sprintf(out.data(), "\"0x%08X\"", wrappedModuleId);
    }
    else {
        sprintf(out.data(), "%u", (u8)Utility::GetModuleId(wrappedModuleId));
    }

    return out;
}

int ipow(int base, int exp)
{
    int result = 1;
    while (exp){
        if (exp & 1) result *= base;
        exp >>= 1;
        if (!exp) break; // Strictly speaking this does not make a difference for the return value. However, it fixes a signed integer overflow error of base found by the sanitizers.
        base *= base;
    }
    return result;
}

//Compares a memory region with a byte, useful for checking if a memory is empty with 0x00 (e.g. ram) or with 0xFF (e.g. flash)
bool Utility::CompareMem(const u8 byte, const u8* data, const u16 dataLength){
    for(u32 i=0; i<dataLength; i++){
        if(data[i] != byte) return false;
    }
    return true;
}

void Utility::ToUpperCase(char * str)
{
    while ((*str = toupper(*str))) str++;
}

u32 Utility::GetIndexForSerial(const char* serialNumber, bool *didError){
    u32 index = 0;
    u32 serialLength = strlen(serialNumber);
    u32 charCounter = 0;
    for(u32 i=0; i< serialLength; i++){
        if(i == serialLength - 1 && serialNumber[0] == 'A') continue;
        char currentChar = serialNumber[serialLength - i - 1];
        if (currentChar == '\0') continue;
        const char* charPos = strchr(serialAlphabet, currentChar);
        if (charPos == nullptr)
        {
            if (didError != nullptr) *didError = true;
            SIMEXCEPTION(IllegalArgumentException);
            return INVALID_SERIAL_NUMBER_INDEX;
        }
        u32 charValue = (u32)charPos - (u32)serialAlphabet;
        index += ipow(sizeof(serialAlphabet)-1, charCounter) * charValue;
        charCounter++;
    }
    return index;
}

void Utility::GenerateBeaconSerialForIndex(u32 index, char* serialBuffer)
{
    CheckedMemset(serialBuffer, 0x00, NODE_SERIAL_NUMBER_MAX_CHAR_LENGTH);
    u32 numChars = index < 24300000UL ? 5 : 7; //Small serial numbers use 5 characters, the extended range uses 7 characters
    for(u32 i=0; i< numChars; i++){
        u32 rest = (u32)(index % strlen(serialAlphabet));
        serialBuffer[numChars - i - 1] = serialAlphabet[rest];
        index /= strlen(serialAlphabet);
    }
}

u16 Utility::ByteToAsciiHex(u8 b) {
    u8 asciiHex[2];
    static const char* digits = "0123456789ABCDEF";
    asciiHex[0] = digits[(b & 0xF0) >> 4];
    asciiHex[1] = digits[b & 0xF];

    return *((u16*)asciiHex);
}

//Converts a series of 2,4,6 or 8 hex-chars to an unsigned int
u32 Utility::ByteFromAsciiHex(char* asciiHex, u8 numChars){
    char* h = asciiHex;
    //convert x tuples
    u32 result = 0;
    for(int i=0; i<numChars; i+=2){
        u8 byte = 0;
        //Convert first char
        if(h[i] >= 48 && h[i] <= 57){
            byte += (h[i] - 48) << 4;
        } else if(h[i] >= 65 && h[i] <= 90){
            byte += (h[i] - 55) << 4;
        }
        //Convert and add second char
        if(h[i+1] >= 48 && h[i+1] <= 57){
            byte += (h[i+1] - 48);
        } else if(h[i+1] >= 65 && h[i+1] <= 90){
            byte += (h[i+1] - 55);
        }

        result |= byte << i*4;
    }

    return result;
}

void Utility::LogRebootJson()
{
    logjson("MAIN", "{\"type\":\"reboot\",\"reason\":%u,\"code1\":%u,\"stack\":%u,\"version\":%u,\"blversion\":%u}" SEP, (u32)GS->ramRetainStructPtr->rebootReason, GS->ramRetainStructPtr->code1, GS->ramRetainStructPtr->stacktrace[0], FM_VERSION, FruityHal::GetBootloaderVersion());
}

bool Utility::Contains(const u8 * data, const u32 length, const u8 searchValue)
{
    return memchr(data, searchValue, length) != nullptr;
}

bool Utility::IsPowerOfTwo(u32 val)
{
    if (val == 0) return false; //Edge case
    else return ((val & (val - 1ul)) == 0ul);
}

u32 Utility::NextMultipleOf(const u32 val, const u32 multiple)
{
    if (multiple <= 1) return val;
    const u32 remainder = val % multiple;
    if (remainder == 0) return val;
    return val + multiple - remainder;
}

NodeId Utility::TerminalArgumentToNodeId(const char * arg, bool* didErrorArg)
{
    if (arg == nullptr)
    {
        SIMEXCEPTION(IllegalArgumentException);
        return NODE_ID_INVALID;
    }

    // Special target values
    if (strcmp(arg, "this")     == 0) return GS->node.configuration.nodeId;
    if (strcmp(arg, "max_hops") == 0) return NODE_ID_HOPS_BASE + NODE_ID_HOPS_BASE_SIZE - 1;

    bool didErrorLocal = false;
    NodeId retVal = Utility::StringToU16(arg, &didErrorLocal);
    if (didErrorLocal)
    {
        if (didErrorArg != nullptr) *didErrorArg = true;
        return NODE_ID_INVALID;
    }
    return retVal;
}

bool Utility::IsUnknownRebootReason(RebootReason rebootReason)
{
    return (
           rebootReason == RebootReason::UNKNOWN 
        || rebootReason == RebootReason::UNKNOWN_BUT_BOOTED
        );
}

char * Utility::FindLast(char * str, const char * search)
{
    char* retVal = nullptr;
    while ((str = strstr(str, search)) != nullptr)
    {
        retVal = str;
        str++;
    }
    return retVal;
}

const char* IgnoreZeroPrefix(const char* str)
{
    //We do not remove leading zeros on hex numbers
    if (str[0] == '0' && str[1] == 'x') return str;

    //We remove leading zeros for other stuff, e.g. binary numbers
    while (str[0] == '0' && str[1] != '\0') str++;
    return str;
}

long Utility::StringToLong(const char *str, bool *outDidError)
{
    static_assert(sizeof(long) >= sizeof(i32), "This function is used to parse strings to variables of size i32. Thus long must be at least as big.");
    char *endPtr = nullptr;
    //We store the negativity of the number manually as we don't want to rely on the behavior of strtol as it interprets trailing zeros
    //as octet numbers, something that seems to be not expected by most of our FW devs. See IOT-4166.
    const bool isNegative = str[0] == '-';
    if (isNegative) str++;
    str = IgnoreZeroPrefix(str);

    const long retVal = strtol(str, &endPtr, 0);
    if (endPtr == str || *endPtr != '\0')
    {
        if (outDidError == nullptr) { SIMEXCEPTION(NotANumberStringException); }
        else { *outDidError = true; }
        return 0;
    }

    return isNegative ? -retVal : retVal;
}

unsigned long Utility::StringToUnsignedLong(const char * str, bool *outDidError)
{
    static_assert(sizeof(unsigned long) >= sizeof(u32), "This function is used to parse strings to variables of size u32. Thus unsigned long must be at least as big.");
    char *endPtr = nullptr;
    str = IgnoreZeroPrefix(str);

    const unsigned long retVal = strtoul(str, &endPtr, 0);
    if (endPtr == str || (*endPtr != '\0' && *endPtr != '\r' && *endPtr != '\n'))
    {
        if (outDidError == nullptr) { SIMEXCEPTION(NotANumberStringException); }
        else { *outDidError = true; }
        logt("WARN", "Tried to interpret the none number string \"%s\" as a number", str);
        return 0;
    }

    return retVal;
}

template<typename T>
static T StringToU(const char *str, bool *outDidError)
{
    const unsigned long retVal = Utility::StringToUnsignedLong(str, outDidError);
    if (retVal > std::numeric_limits<T>::max())
    {
        //In case no pointer to didError was given, we should throw an exception
        if (outDidError == nullptr) { SIMEXCEPTION(NumberStringNotInRangeException); }
        else { *outDidError = true; }
        return 0;
    }
    return retVal;
}

u8 Utility::StringToU8(const char * str, bool *outDidError)
{
    return StringToU<u8>(str, outDidError);
}

u16 Utility::StringToU16(const char * str, bool *outDidError)
{
    return StringToU<u16>(str, outDidError);
}

u32 Utility::StringToU32(const char * str, bool *outDidError)
{
    return StringToU<u32>(str, outDidError);
}

template<typename T>
static T StringToI(const char *str, bool *outDidError)
{
    const long retVal = Utility::StringToLong(str, outDidError);
    if (retVal > std::numeric_limits<T>::max() || retVal < std::numeric_limits<T>::min())
    {
        if (outDidError == nullptr) { SIMEXCEPTION(NumberStringNotInRangeException); }
        else { *outDidError = true; }
        return 0;
    }
    return retVal;
}

i8 Utility::StringToI8(const char * str, bool *outDidError)
{
    return StringToI<i8>(str, outDidError);
}

i16 Utility::StringToI16(const char * str, bool *outDidError)
{
    return StringToI<i16>(str, outDidError);
}

i32 Utility::StringToI32(const char * str, bool *outDidError)
{
    return StringToI<i32>(str, outDidError);
}

/*
void Utility::GetVersionStringFromInts(u16 major, char* outputBuffer)
{
    u16 major = version / 10000000;
    u16 minor = (version - 10000000 * major) / 10000;
    u16 patch = (version - 10000000 * major - 10000 * minor);

    sprintf(outputBuffer, "%u.%u.%u", major, minor, patch);

}*/


uint8_t Utility::CalculateCrc8(const u8* data, u16 dataLength)
{
    uint8_t CRC = 0x00;
    uint16_t tmp;

    while (dataLength > 0)
    {
        tmp = CRC << 1;
        tmp += *data;
        CRC = (tmp & 0xFF) + (tmp >> 8);
        data++;
        --dataLength;
    }

    return CRC;
}

//void Utility::CalculateCRC16
/**@brief Function for calculating CRC-16 in blocks.
 *
 * Feed each consecutive data block into this function, along with the current value of p_crc as
 * returned by the previous call of this function. The first call of this function should pass nullptr
 * as the initial value of the crc in p_crc.
 * Conforms to CRC-CCITT (0xFFFF), can be calculated with https://www.lammertbies.nl/comm/info/crc-calculation.html
 *
 * @param[in] p_data The input data block for computation.
 * @param[in] size   The size of the input data block in bytes.
 * @param[in] p_crc  The previous calculated CRC-16 value or nullptr if first call.
 *
 * @return The updated CRC-16 value, based on the input supplied.
 */
uint16_t Utility::CalculateCrc16(const uint8_t * p_data, const uint32_t size, const uint16_t * p_crc){
    uint32_t i;
    uint16_t crc = (p_crc == nullptr) ? 0xffff : *p_crc;

    for (i = 0; i < size; i++)
    {
        crc  = (unsigned char)(crc >> 8) | (crc << 8);
        crc ^= p_data[i];
        crc ^= (unsigned char)(crc & 0xff) >> 4;
        crc ^= (crc << 8) << 4;
        crc ^= ((crc & 0xff) << 4) << 1;
    }

    return crc;
}

u32 Utility::CalculateCrc32(const u8* message, const u32 messageLength, u32 previousCrc) {
    u32 crc = ~previousCrc;
    for(u32 i = 0; i < messageLength; i++) {
        u32 byte = message[i];
        crc = crc ^ byte;
        for (u32 j = 0; j < 8; j++) {
            u32 mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return ~crc;
}

u32 Utility::CalculateCrc32String(const char * message, u32 previousCrc)
{
    u32 length = strlen(message);
    return CalculateCrc32((u8 const *)message, length, previousCrc);
}

//Encrypts a message
void Utility::Aes128BlockEncrypt(const Aes128Block* messageBlock, const Aes128Block* key, Aes128Block* encryptedMessage)
{
    // TODO: what should we do in case of error ?
    FruityHal::EcbEncryptBlock((const u8*)key->data, (const u8*)messageBlock->data, (u8*)encryptedMessage->data);
}

void Utility::XorBytes(const u8* src1, const u8* src2, const u8 numBytes, u8* out) {
    for(u8 i = 0; i < numBytes; i++) {
        out[i] = src1[i] ^ src2[i];
    }
}

void Utility::SwapBytes(u8 *data, const size_t length)
{
    u8 *p = data;
    size_t lo, hi;
    for(lo=0, hi=length-1; hi>lo; lo++, hi--)
    {
        char tmp=p[lo];
        p[lo] = p[hi];
        p[hi] = tmp;
    }
}

u16 Utility::SwapU16( u16 val )
{
    return (val << 8) | (val >> 8 );
}

u32 Utility::SwapU32( u32 val )
{
    val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0xFF00FF );
    return (val << 16) | (val >> 16);
}

void Utility::XorWords(const u32* src1, const u32* src2, const u8 numWords, u32* out) {
    for(u8 i = 0; i < numWords; i++) {
        out[i] = src1[i] ^ src2[i];
    }
}

//This method can be used to implement a configurable back off timer, take a look at TestUtility to see how it is used
bool Utility::ShouldBackOffIvTrigger(u32 timerDs, u16 passedTimeDs, u32 startTimeDs, const u32* backOffIvsDs, u16 backOffIvsSize)
{
    if (startTimeDs == INVALID_BACKOFF_START_TIME) return false;

    for (u32 i = 0; i < backOffIvsSize / sizeof(u32); i++)
    {
        u32 intervalDs = backOffIvsDs[i];
        if (timerDs - startTimeDs - passedTimeDs < intervalDs && timerDs - startTimeDs >= intervalDs) return true;
    }
    return false;
}

u16 Utility::ToAlignedU16(const void* ptr)
{
    u16 retVal = 0;
    CheckedMemcpy(&retVal, ptr, sizeof(retVal));
    return retVal;
}

i16 Utility::ToAlignedI16(const void* ptr)
{
    i16 retVal = 0;
    CheckedMemcpy(&retVal, ptr, sizeof(retVal));
    return retVal;
}

u32 Utility::ToAlignedU32(const void* ptr)
{
    u32 retVal = 0;
    CheckedMemcpy(&retVal, ptr, sizeof(retVal));
    return retVal;
}

i32 Utility::ToAlignedI32(const void* ptr)
{
    i32 retVal = 0;
    CheckedMemcpy(&retVal, ptr, sizeof(retVal));
    return retVal;
}
