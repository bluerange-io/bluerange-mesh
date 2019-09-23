////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2019 M-Way Solutions GmbH
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

/*
 * The Logger enables outputting debug data to UART.
 * Any log tag can be used with the logt() command. The message will be logged
 * only if the applicable logtag has been enabled previously.
 * It will also print strings for common error codes.
 */

#pragma once

#include <Config.h>
#include <Boardconfig.h>
#include <Terminal.h>
#include "SimpleArray.h"

constexpr int MAX_ACTIVATE_LOG_TAG_NUM = 30;
constexpr int MAX_LOG_TAG_LENGTH = 11;

#ifdef _MSC_VER
#include <string.h>
#define __FILE_S__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define __FILE_S__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

class Logger : public TerminalCommandListener
{
private:

	SimpleArray<char, MAX_ACTIVATE_LOG_TAG_NUM * MAX_LOG_TAG_LENGTH> activeLogTags;

public:
	Logger();
	static Logger& getInstance();

	void Init();

	//TODO: We could save ram if we pack this
	struct errorLogEntry {
		ErrorTypes errorType;
		u32 extraInfo;
		u32 errorCode;
		u32 timestamp;
	};

	static constexpr int NUM_ERROR_LOG_ENTRIES = 100;
	errorLogEntry errorLog[NUM_ERROR_LOG_ENTRIES];
	u8 errorLogPosition;

	bool logEverything = false;

	enum class LogType : u8 {
		UART_COMMUNICATION, 
		LOG_LINE, 
		LOG_MESSAGE_ONLY, 
		TRACE
	};
	enum class UartErrorType : u8{
		SUCCESS,
		COMMAND_NOT_FOUND, 
		ARGUMENTS_WRONG,
		TOO_MANY_ARGUMENTS
	};

#ifdef __GNUC__
#define CheckPrintfFormating(stringIndex, firstToCheck) __attribute__((format(printf, stringIndex, firstToCheck)))
#else
#define CheckPrintfFormating(...) /*do nothing*/
#endif
	void log_f(bool printLine, const char* file, i32 line, const char* message, ...) const CheckPrintfFormating(5, 6);
	void logTag_f(LogType logType, const char* file, i32 line, const char* tag, const char* message, ...) const CheckPrintfFormating(6, 7);
#undef CheckPrintfFormating

	void logError(ErrorTypes errorType, u32 errorCode, u32 extraInfo);
	void logCustomError(CustomErrorTypes customErrorType, u32 extraInfo);
	void logCount(ErrorTypes errorType, u32 errorCode);
	void logCustomCount(CustomErrorTypes customErrorType);

	void uart_error_f(UartErrorType type) const;

	void disableAll();
	void enableAll();

	//These functions are used to enable/disable a debug tag, it will then be printed to the output
	void enableTag(const char* tag);
	bool IsTagEnabled(const char* tag) const;
	void disableTag(const char* tag);
	void toggleTag(const char* tag);

	//The print function provides an overview over the active debug tags
	void printEnabledTags() const;

	//The Logger implements the Terminal Listener
	#ifdef TERMINAL_ENABLED
	bool TerminalCommandHandler(char* commandArgs[], u8 commandArgsSize) override;
	#endif

	//Other printing functions
	void blePrettyPrintAdvData(SizedData advData) const;
	static void convertBufferToBase64String(const u8* srcBuffer, u32 srcLength, char* dstBuffer, u16 bufferLength);
	static void convertBufferToHexString   (const u8* srcBuffer, u32 srcLength, char* dstBuffer, u16 bufferLength);
public:
	static u32 parseEncodedStringToBuffer(const char* encodedString, u8* dstBuffer, u16 dstBufferSize);
private:
	static u32 parseHexStringToBuffer(const char* hexString, u32 hexStringLength, u8* dstBuffer, u16 dstBufferSize);
	static u32 parseBase64StringToBuffer(const char* base64String, u32 base64StringLength, u8* dstBuffer, u16 dstBufferSize);
};

/*
 * Define macros that rewrite the log function calls to the Logger
 * */

//Used for UART communication between node and attached pc
#if IS_ACTIVE(JSON_LOGGING)
#define logjson(tag, message, ...) Logger::getInstance().log_f(false, "", 0, message, ##__VA_ARGS__)
#define logjson_error(type) Logger::getInstance().uart_error_f(type)
#else
#define logjson(...) do{}while(0)
#define logjson_error(...) do{}while(0)
#endif

//Currently, tracing is always enabled if we have a terminal
#if defined(TERMINAL_ENABLED) && IS_ACTIVE(TRACE)
#define trace(message, ...) Logger::getInstance().log_f(false, "", 0, message, ##__VA_ARGS__)
#else
#define trace(message, ...)  do{}while(0)
#endif

#if IS_ACTIVE(LOGGING)
#define logs(message, ...) Logger::getInstance().log_f(true, __FILE_S__, __LINE__, message, ##__VA_ARGS__)
#define logt(tag, message, ...) Logger::getInstance().logTag_f(Logger::LogType::LOG_LINE, __FILE_S__, __LINE__, tag, message, ##__VA_ARGS__)
#define TO_BASE64(data, dataSize) DYNAMIC_ARRAY(data##Hex, (dataSize)*3+1); Logger::convertBufferToBase64String(data, (dataSize), (char*)data##Hex, (dataSize)*3+1)
#define TO_BASE64_2(data, dataSize) Logger::convertBufferToBase64String(data, (dataSize), (char*)data##Hex, (dataSize)*3+1)
#define TO_HEX(data, dataSize) DYNAMIC_ARRAY(data##Hex, (dataSize)*3+1); Logger::convertBufferToHexString(data, (dataSize), (char*)data##Hex, (dataSize)*3+1)
#define TO_HEX_2(data, dataSize) Logger::convertBufferToHexString(data, (dataSize), (char*)data##Hex, (dataSize)*3+1)

#else //ACTIVATE_LOGGING

#define logs(...) do{}while(0)
#define logt(...) do{}while(0)
#define TO_BASE64(data, dataSize) do{}while(0)
#define TO_BASE64_2(data, dataSize) do{}while(0)
#define TO_HEX(data, dataSize) do{}while(0)
#define TO_HEX_2(data, dataSize) do{}while(0)

#endif //ACTIVATE_LOGGING
