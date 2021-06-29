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

#pragma once

#include <Config.h>
#include <Boardconfig.h>
#include <Terminal.h>
#ifdef SIM_ENABLED
#include <string>
#endif
#include <array>

constexpr int MAX_ACTIVATE_LOG_TAG_NUM = 40;
constexpr int MAX_LOG_TAG_LENGTH = 11;

/*############ Error Types ################*/
//Errors are saved in RAM and can be requested through the mesh

enum class LoggingError : u8 {
    GENERAL_ERROR = 0, //Defined in "FmTypes.h" (ErrorType)
    HCI_ERROR = 1, //Defined in "FruityHalError.h" (BleHciError)
    CUSTOM = 2, //Defined below (CustomErrorTypes)
    GATT_STATUS = 3, //Defined in "FruityHalError.h" (BleGattEror)
    REBOOT = 4, //Defined below (RebootReason)
    VENDOR = 5, //A placeholder for vendor/3rd party error codes, feel free to use this category for your own implementation of custom error types
};

//There are a number of different error types (by convention)
// FATAL_ are errors that must never occur, mostly implementation faults or critical connectivity issues
// WARN_ is for errors that might occur from time to time and should be investigated if they happen too often
// INFO_ is for noting important events that do not occur often
// COUNT_ is for errors or information that might happen often but should not pass a certain threshold
enum class CustomErrorTypes : u8 {
    FATAL_BLE_GATTC_EVT_TIMEOUT_FORCED_US = 1,
    INFO_TRYING_CONNECTION_SUSTAIN = 2,
    WARN_CONNECTION_SUSTAIN_FAILED_TO_ESTABLISH = 3,
    COUNT_CONNECTION_SUCCESS = 4,
    COUNT_HANDSHAKE_DONE = 5,
    //REBOOT=6, deprectated, use error type REBOOT
    WARN_HANDSHAKE_TIMEOUT = 7,
    WARN_CM_FAIL_NO_SPOT = 8,
    FATAL_QUEUE_NUM_MISMATCH = 9,
    WARN_GATT_WRITE_ERROR = 10,
    WARN_TX_WRONG_DATA = 11,
    DEPRECATED_WARN_RX_WRONG_DATA = 12,     // Use COUNT_WARN_RX_WRONG_DATA instead
    WARN_CLUSTER_UPDATE_FLOW_MISMATCH = 13,
    WARN_VITAL_PRIO_QUEUE_FULL = 14,
    COUNT_NO_PENDING_CONNECTION = 15,
    FATAL_HANDLE_PACKET_SENT_ERROR = 16,
    COUNT_DROPPED_PACKETS = 17,
    COUNT_SENT_PACKETS_RELIABLE = 18,
    COUNT_SENT_PACKETS_UNRELIABLE = 19,
    INFO_ERRORS_REQUESTED = 20,
    INFO_CONNECTION_SUSTAIN_SUCCESS = 21,
    COUNT_JOIN_ME_RECEIVED = 22,
    WARN_CONNECT_AS_MASTER_NOT_POSSIBLE = 23,
    FATAL_PENDING_NOT_CLEARED = 24,
    FATAL_PROTECTED_PAGE_ERASE = 25,
    INFO_IGNORING_CONNECTION_SUSTAIN = 26,
    INFO_IGNORING_CONNECTION_SUSTAIN_LEAF = 27,
    COUNT_GATT_CONNECT_FAILED = 28,
    FATAL_PACKET_PROCESSING_FAILED = 29,
    FATAL_PACKET_TOO_BIG = 30,
    COUNT_HANDSHAKE_ACK1_DUPLICATE = 31,
    COUNT_HANDSHAKE_ACK2_DUPLICATE = 32,
    COUNT_ENROLLMENT_NOT_SAVED = 33,
    COUNT_FLASH_OPERATION_ERROR = 34,
    FATAL_WRONG_FLASH_STORAGE_COMMAND = 35,
    FATAL_ABORTED_FLASH_TRANSACTION = 36,
    FATAL_PACKETQUEUE_PACKET_TOO_BIG = 37,
    FATAL_NO_RECORDSTORAGE_SPACE_LEFT = 38,
    FATAL_RECORD_CRC_WRONG = 39,
    COUNT_3RD_PARTY_TIMEOUT = 40,
    FATAL_CONNECTION_ALLOCATOR_OUT_OF_MEMORY = 41,
    FATAL_CONNECTION_REMOVED_WHILE_TIME_SYNC = 42,
    FATAL_COULD_NOT_RETRIEVE_CAPABILITIES = 43,
    FATAL_INCORRECT_HOPS_TO_SINK = 44,
    WARN_SPLIT_PACKET_MISSING = 45,
    WARN_SPLIT_PACKET_NOT_IN_MTU = 46,
    FATAL_DFU_FLASH_OPERATION_FAILED = 47,
    WARN_RECORD_STORAGE_ERASE_CYCLES_HIGH = 48,
    FATAL_RECORD_STORAGE_ERASE_CYCLES_HIGH = 49,
    FATAL_RECORD_STORAGE_COULD_NOT_FIND_SWAP_PAGE = 50,
    FATAL_MTU_UPGRADE_FAILED = 51,
    WARN_ENROLLMENT_ERASE_FAILED = 52,
    FATAL_RECORD_STORAGE_UNLOCK_FAILED = 53,
    WARN_ENROLLMENT_LOCK_DOWN_FAILED = 54,
    WARN_CONNECTION_SUSTAIN_FAILED = 55,
    COUNT_EMERGENCY_CONNECTION_CANT_DISCONNECT_ANYBODY = 56,
    INFO_EMERGENCY_DISCONNECT_SUCCESSFUL = 57,
    WARN_COULD_NOT_CREATE_EMERGENCY_DISCONNECT_VALIDATION_CONNECTION = 58,
    WARN_UNEXPECTED_REMOVAL_OF_EMERGENCY_DISCONNECT_VALIDATION_CONNECTION = 59,
    WARN_EMERGENCY_DISCONNECT_PARTNER_COULDNT_DISCONNECT_ANYBODY = 60,
    WARN_REQUEST_PROPOSALS_UNEXPECTED_LENGTH = 61,
    WARN_REQUEST_PROPOSALS_TOO_LONG = 62,
    FATAL_SENSOR_PINS_NOT_DEFINED_IN_BOARD_ID = 63,
    COUNT_ACCESS_TO_REMOVED_CONNECTION = 64,
    WARN_ADVERTISING_CONTROLLER_DEACTIVATE_FAILED = 65,
    WARN_GAP_SEC_INFO_REPLY_FAILED = 66,
    WARN_GAP_SEC_DISCONNECT_FAILED = 67,
    FATAL_FAILED_TO_REGISTER_APPLICATION_INTERRUPT_HANDLER = 68,
    FATAL_FAILED_TO_REGISTER_MAIN_CONTEXT_HANDLER = 69,
    FATAL_SIG_PROVISIONING_FAILED = 70,
    FATAL_SIG_ELEMENT_CREATION_FAILED = 71,
    FATAL_SIG_STORAGE_ERROR = 72,
    COUNT_RECEIVED_INVALID_FRUITY_MESH_PACKET = 73,
    FATAL_NEW_CHUNK_TOO_SMALL_FOR_MESSAGE = 74,
    FATAL_NO_CHUNK_FOR_NEW_CONNECTION = 75,
    COUNT_GENERATED_SPLIT_PACKETS = 76,
    COUNT_RECEIVED_SPLIT_OVER_MESH_ACCESS = 77,
    FATAL_ILLEGAL_PROCCESS_BUFFER_LENGTH = 78,
    FATAL_QUEUE_ORIGINS_FULL = 79,
    COUNT_TOTAL_RECEIVED_MESSAGES = 80, // total number of received messages by node (also includes the relay packets)
    COUNT_RECEIVED_MESSAGES = 81, //number of received messages meant to be for that particular node
    COUNT_UART_RX_ERROR = 82,
    INFO_UNUSED_STACK_BYTES = 83,
    FATAL_CONNECTION_REMOVED_WHILE_ENROLLED_NODES_SYNC = 84,
    INFO_UPTIME_RELATIVE = 85,
    INFO_UPTIME_ABSOLUTE = 86,
    COUNT_WARN_RX_WRONG_DATA = 87,
    WATCHDOG_REBOOT = 88,
};

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
private:

    std::array<char, MAX_ACTIVATE_LOG_TAG_NUM * MAX_LOG_TAG_LENGTH> activeLogTags{};

    u32 currentJsonCrc = 0;

#ifdef SIM_ENABLED
    std::string currentString = "";
#endif

public:
    Logger();
    static Logger& GetInstance();

    //TODO: We could save ram if we pack this
    struct ErrorLogEntry {
        LoggingError errorType;
        u32 extraInfo;
        u32 errorCode;
        u32 timestamp;
    };

    static constexpr int NUM_ERROR_LOG_ENTRIES = 100;
    ErrorLogEntry errorLog[NUM_ERROR_LOG_ENTRIES];
    u8 errorLogPosition = 0;

    bool logEverything = false;

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
    static void ConvertBufferToHexString(const u8* srcBuffer, u32 srcLength,           char* dstBuffer, u16 bufferLength);
    static void ConvertBufferToHexString(const u8* srcBuffer, MessageLength srcLength, char* dstBuffer, u16 bufferLength);
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
