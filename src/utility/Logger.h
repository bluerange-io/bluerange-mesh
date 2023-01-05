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

#include <Config.h>
#include <Boardconfig.h>
#include <Terminal.h>
#include <ErrorLog.h>

#ifdef SIM_ENABLED
#include <string>
#endif

#include <array>
#include <limits>

constexpr int MAX_ACTIVATE_LOG_TAG_NUM = 40;
constexpr int MAX_LOG_TAG_LENGTH = 11;

#ifdef _MSC_VER
#include <string.h>
#define __FILE_S__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define __FILE_S__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

/*
 * The Logger enables outputting debug data to UART.
 * Any log tag can be used with the logt() command. The message will be logged
 * only if the applicable logtag has been enabled previously.
 * It will also print strings for common error codes.
 */
class Logger
{
public:
    enum class LogType : u8 {
        UART_COMMUNICATION,
        LOG_LINE,
        LOG_MESSAGE_ONLY,
        TRACE
    };

    enum class UartErrorType : u8{
        SUCCESS            = 0,
        COMMAND_NOT_FOUND  = 1,
        ARGUMENTS_WRONG    = 2,
        TOO_MANY_ARGUMENTS = 3,
        TOO_FEW_ARGUMENTS  = 4,
        WARN_DEPRECATED    = 5,
        CRC_INVALID        = 6,
        CRC_MISSING        = 7,
        INTERNAL_ERROR     = 8,
    };

private:
    std::array<char, MAX_ACTIVATE_LOG_TAG_NUM * MAX_LOG_TAG_LENGTH> activeLogTags = {};

    u32 currentJsonCrc = 0;

#ifdef SIM_ENABLED
    std::string currentString = "";
#endif

    ErrorLog errorLog;

public:
    static constexpr int NUM_ERROR_LOG_ENTRIES = ErrorLog::STORAGE_SIZE;

    bool logEverything = false;

public:
    Logger();

    static Logger& GetInstance();

#ifdef __GNUC__
#define CheckPrintfFormating(stringIndex, firstToCheck) __attribute__((format(printf, stringIndex, firstToCheck)))
#else
#define CheckPrintfFormating(...) /*do nothing*/
#endif
    void Log_f(bool printLine, bool isJson, bool isEndOfMessage, bool skipJsonEvent, const char* file, i32 line, const char* message, ...) CheckPrintfFormating(8, 9);
    void LogTag_f(LogType logType, const char* file, i32 line, const char* tag, const char* message, ...) const CheckPrintfFormating(6, 7);
#undef CheckPrintfFormating

    void LogError(LoggingError errorType, u32 errorCode, u32 extraInfo);
    void LogCustomError(CustomErrorTypes customErrorType, u32 extraInfo);
    void LogCount(LoggingError errorType, u32 errorCode, u32 amount = 1);
    void LogCustomCount(CustomErrorTypes customErrorType, u32 amount = 1);

    bool PopErrorLogEntry(ErrorLogEntry & entry);

    ErrorLog &GetErrorLog()
    {
        return errorLog;
    }

    void UartError_f(UartErrorType type) const;

    void DisableAll();
    void EnableAll();

    //These functions are used to enable/disable a debug tag, it will then be printed to the output
    void EnableTag(const char* tag);
    bool IsTagEnabled(const char* tag) const;
    void DisableTag(const char* tag);
    void ToggleTag(const char* tag);

    u32 GetAmountOfEnabledTags();

    //The print function provides an overview over the active debug tags
    void PrintEnabledTags() const;

    #ifdef TERMINAL_ENABLED
    TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize);
    #endif

    static const char* GetErrorLogErrorType(LoggingError type);
    static const char* GetErrorLogCustomError(CustomErrorTypes type);
    static const char* GetGattStatusErrorString(FruityHal::BleGattEror gattStatusCode);
    static const char* GetGeneralErrorString(ErrorType nrfErrorCode);
    static const char* GetHciErrorString(FruityHal::BleHciError hciErrorCode);
    static const char* GetErrorLogRebootReason(RebootReason type);
    static const char* GetErrorLogError(LoggingError type, u32 code);

    //Other printing functions
    void BlePrettyPrintAdvData(SizedData advData) const;
    static void ConvertBufferToBase64String(const u8* srcBuffer, u32 srcLength,           char* dstBuffer, u16 bufferLength);
    static void ConvertBufferToBase64String(const u8* srcBuffer, MessageLength srcLength, char* dstBuffer, u16 bufferLength);
    static u32 ConvertBufferToHexString(const u8* srcBuffer, u32 srcLength,           char* dstBuffer, u16 bufferLength);
    static u32 ConvertBufferToHexString(const u8* srcBuffer, MessageLength srcLength, char* dstBuffer, u16 bufferLength);
public:
    static u32 ParseEncodedStringToBuffer(const char* encodedString, u8* dstBuffer, u16 dstBufferSize, bool *didError = nullptr);
private:
    static u32 ParseHexStringToBuffer(const char* hexString, u32 hexStringLength, u8* dstBuffer, u16 dstBufferSize, bool *didError);
    static u32 ParseBase64StringToBuffer(const char* base64String, u32 base64StringLength, u8* dstBuffer, u16 dstBufferSize, bool *didError);
};

/*
 * Define macros that rewrite the log function calls to the Logger
 * */

//Used for UART communication between node and attached pc
#if IS_ACTIVE(JSON_LOGGING)
//TODO: The skip_event macros are currently a workaround that should be removed once a proper solution to avoid
//endless recursions with JSONHandlers has been found. The same applies to the skipJsonEvent parameter of Log_f
#define logjson(tag, message, ...) Logger::GetInstance().Log_f(false, true, true, false, "", 0, message, ##__VA_ARGS__)
#define logjson_skip_event(tag, message, ...) Logger::GetInstance().Log_f(false, true, true, true, "", 0, message, ##__VA_ARGS__)
#define logjson_partial(tag, message, ...) Logger::GetInstance().Log_f(false, true, false, false, "", 0, message, ##__VA_ARGS__)
#define logjson_partial_skip_event(tag, message, ...) Logger::GetInstance().Log_f(false, true, false, true, "", 0, message, ##__VA_ARGS__)
#define logjson_error(type) Logger::GetInstance().UartError_f(type)
#else
#define logjson(tag, message, ...) do{}while(0)
#define logjson_skip_event(tag, message, ...) do{}while(0)
#define logjson_partial(tag, message, ...) do{}while(0)
#define logjson_partial_skip_event(tag, message, ...) do{}while(0)
#define logjson_error(...)         do{}while(0)
#endif

//Currently, tracing is always enabled if we have a terminal
#if defined(TERMINAL_ENABLED) && IS_ACTIVE(TRACE)
#define trace(message, ...) Logger::GetInstance().Log_f(false, false, true, false, "", 0, message, ##__VA_ARGS__)
#else
#define trace(message, ...) do{}while(0)
#endif

#if IS_ACTIVE(LOGGING)
#define logs(message, ...) Logger::GetInstance().Log_f(true, false, true, false, __FILE_S__, __LINE__, message, ##__VA_ARGS__)
#define logt(tag, message, ...) Logger::GetInstance().LogTag_f(Logger::LogType::LOG_LINE, __FILE_S__, __LINE__, tag, message, ##__VA_ARGS__)
#define TO_BASE64(data, dataSize) DYNAMIC_ARRAY(data##Hex, (dataSize)*3+1); Logger::ConvertBufferToBase64String(data, (dataSize), (char*)data##Hex, (dataSize)*3+1)
#define TO_BASE64_2(data, dataSize) Logger::ConvertBufferToBase64String(data, (dataSize), (char*)data##Hex, (dataSize)*3+1)
#define TO_HEX(data, dataSize) DYNAMIC_ARRAY(data##Hex, (dataSize)*3+1); Logger::ConvertBufferToHexString(data, (dataSize), (char*)data##Hex, (dataSize)*3+1)
#define TO_HEX_2(data, dataSize) Logger::ConvertBufferToHexString(data, (dataSize), (char*)data##Hex, (dataSize)*3+1)

#else //ACTIVATE_LOGGING

#define logs(message, ...)          do{}while(0)
#define logt(tag, message, ...)     do{}while(0)
#define TO_BASE64(data, dataSize)   do{}while(0)
#define TO_BASE64_2(data, dataSize) do{}while(0)
#define TO_HEX(data, dataSize)      do{}while(0)
#define TO_HEX_2(data, dataSize)    do{}while(0)

#endif //ACTIVATE_LOGGING