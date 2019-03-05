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

#define MAX_ACTIVATE_LOG_TAG_NUM 30
#define MAX_LOG_TAG_LENGTH 11

#ifdef _MSC_VER
#include <string.h>
#define __FILE_S__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define __FILE_S__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

enum class ErrorTypes : u8 {
	SD_CALL_ERROR = 0,
	HCI_ERROR = 1,
	CUSTOM = 2,
	GATT_STATUS = 3,
	REBOOT = 4
};

enum class CustomErrorTypes : u8{
	BLE_GATTC_EVT_TIMEOUT_FORCED_US = 1,
	COUNT_TRYING_CONNECTION_SUSTAIN = 2,
	COUNT_CONNECTION_SUSTAIN_FAILED_TO_ESTABLISH = 3,
	COUNT_CONNECTION_SUCCESS = 4,
	COUNT_HANDSHAKE_DONE = 5,
	//REBOOT=6, deprectated, use error type REBOOT
	COUNT_HANDSHAKE_TIMEOUT = 7,
	COUNT_CM_FAIL_NO_SPOT = 8,
	COUNT_QUEUE_NUM_MISMATCH = 9,
	COUNT_GATT_WRITE_ERROR = 10,
	COUNT_TX_WRONG_DATA = 11,
	COUNT_RX_WRONG_DATA = 12,
	COUNT_CLUSTER_UPDATE_FLOW_MISMATCH = 13,
	COUNT_HIGH_PRIO_QUEUE_FULL = 14,
	COUNT_NO_PENDING_CONNECTION = 15,
	COUNT_HANDLE_PACKET_SENT_ERROR = 16,
	COUNT_DROPPED_PACKETS = 17,
	COUNT_SENT_PACKETS_RELIABLE = 18,
	COUNT_SENT_PACKETS_UNRELIABLE = 19,
	COUNT_ERRORS_REQUESTED = 20,
	COUNT_CONNECTION_SUSTAIN_SUCCESS = 21,
	COUNT_JOIN_ME_RECEIVED = 22,
	CONNECT_AS_MASTER_NOT_POSSIBLE = 23,
	FATAL_PENDING_NOT_CLEARED = 24,
	FATAL_PROTECTED_PAGE_ERASE = 25
};

class Logger : public TerminalCommandListener
{
private:
	Logger();

	SimpleArray<char, MAX_ACTIVATE_LOG_TAG_NUM * MAX_LOG_TAG_LENGTH> activeLogTags;

public:
	static Logger& getInstance(){
		if(!GS->logger){
			GS->logger = new Logger();
		}
		return *(GS->logger);
	}

	void Init();

	struct errorLogEntry {
		ErrorTypes errorType;
		u16 extraInfo;
		u32 errorCode;
		u32 timestamp;
	};

#define NUM_ERROR_LOG_ENTRIES 100
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

	void log_f(bool printLine, const char* file, i32 line, const char* message, ...) const;
	void logTag_f(LogType logType, const char* file, i32 line, const char* tag, const char* message, ...) const;

	void logError(ErrorTypes errorType, u32 errorCode, u16 extraInfo);
	void logCount(ErrorTypes errorType, u32 errorCode);

	void uart_error_f(UartErrorType type) const;

	void disableAll();

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


	//Functions for resolving error codes
	const char* getNrfErrorString(u32 nrfErrorCode) const;
	const char* getHciErrorString(u8 hciErrorCode) const;
	const char* getBleEventNameString(u16 bleEventId) const;
	const char* getGattStatusErrorString(u16 gattStatusCode) const;

	//Other printing functions
	void blePrettyPrintAdvData(sizedData advData) const;
	static void convertBufferToHexString(const u8* srcBuffer, u32 srcLength, char* dstBuffer, u16 bufferLength);
	static u32 parseHexStringToBuffer(const char* hexString, u8* dstBuffer, u16 dstBufferSize);
	static void convertTimestampToString(u32 timestamp, u16 remainderTicks, char* buffer);
};

/*
 * Define macros that rewrite the log function calls to the Logger
 * */

//Used for UART communication between node and attached pc
#ifdef ENABLE_JSON_LOGGING
#define logjson(tag, message, ...) Logger::getInstance().log_f(false, "", 0, message, ##__VA_ARGS__)
#define logjson_error(type) Logger::getInstance().uart_error_f(type)
#else
#define logjson(...) do{}while(0)
#define logjson_error(...) do{}while(0)
#endif

//Currently, tracing is always enabled if we have a terminal
#if defined(TERMINAL_ENABLED) && defined(ACTIVATE_TRACE)
#define trace(message, ...) Logger::getInstance().log_f(false, "", 0, message, ##__VA_ARGS__)
#else
#define trace(message, ...)  do{}while(0)
#endif

#ifdef ENABLE_LOGGING
#define logs(message, ...) Logger::getInstance().log_f(true, __FILE_S__, __LINE__, message, ##__VA_ARGS__)
#define logt(tag, message, ...) Logger::getInstance().logTag_f(Logger::LogType::LOG_LINE, __FILE_S__, __LINE__, tag, message, ##__VA_ARGS__)
#define TO_HEX(data, dataSize) DYNAMIC_ARRAY(data##Hex, (dataSize)*3+1); Logger::getInstance().convertBufferToHexString(data, (dataSize), (char*)data##Hex, (dataSize)*3+1)
#define TO_HEX2(data, dataSize) Logger::getInstance().convertBufferToHexString(data, (dataSize), (char*)data##Hex, (dataSize)*3+1)

#else //ENABLE_LOGGING

#define logs(...) do{}while(0)
#define logt(...) do{}while(0)
#define TO_HEX(data, dataSize) do{}while(0)
#define TO_HEX2(data, dataSize) do{}while(0)

#endif //ENABLE_LOGGING
