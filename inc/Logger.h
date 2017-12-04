/**

Copyright (c) 2014-2017 "M-Way Solutions GmbH"
FruityMesh - Bluetooth Low Energy mesh protocol [http://mwaysolutions.com/]

This file is part of FruityMesh

FruityMesh is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

/*
 * The Logger enables outputting debug data to UART.
 * Any log tag can be used with the logt() command. The message will be logged
 * only if the applicable logtag has been enabled previously.
 * It will also print strings for common error codes.
 */

#pragma once


#include <vector>
#include <string>

#include <Config.h>
#include <Boardconfig.h>
#include <Terminal.h>



#ifdef _MSC_VER
#include <string.h>
#define __FILE_S__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define __FILE_S__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

class Logger : public TerminalCommandListener
{
private:
	Logger();

	std::vector<std::string> logFilter;
	std::vector<std::string>::iterator logFilterIterator;

	char mhTraceBuffer[TRACE_BUFFER_SIZE] = { 0 };

public:
	static Logger* getInstance(){
		if(!GS->logger){
			GS->logger = new Logger();
		}
		return GS->logger;
	}

	void Init();

	typedef struct{
			u8 errorType;
			u16 extraInfo;
			u32 errorCode;
			u32 timestamp;
	} errorLogEntry;

	enum errorTypes { SD_CALL_ERROR=0, HCI_ERROR = 1, CUSTOM = 2 };

	enum customErrorTypes{
		BLE_GATTC_EVT_TIMEOUT_FORCED_US=1,
		TRYING_CONNECTION_SUSTAIN=2,
		FINAL_DISCONNECTION=3,
		CONNECTION_SUCCESS=4,
		HANDSHAKE_DONE=5,
		REBOOT=6
	};

#define NUM_ERROR_LOG_ENTRIES 100
	errorLogEntry errorLog[NUM_ERROR_LOG_ENTRIES];
	u8 errorLogPosition;

	bool logEverything = false;

	enum LogType {UART_COMMUNICATION, LOG_LINE, LOG_MESSAGE_ONLY, TRACE};
	enum UartErrorType {NO_ERROR, COMMAND_NOT_FOUND, ARGUMENTS_WRONG};

	void log_f(bool printLine, const char* file, i32 line, const char* message, ...);
	void logTag_f(LogType logType, const char* file, i32 line, const char* tag, const char* message, ...);

	void logError(errorTypes errorType, u32 errorCode, u16 extraInfo);

	void uart_error_f(UartErrorType type);

	void disableAll();

	//These functions are used to enable/disable a debug tag, it will then be printed to the output
	void enableTag(std::string tag);
	void disableTag(std::string tag);
	void toggleTag(std::string tag);

	//The print function provides an overview over the active debug tags
	void printEnabledTags();

	//The Logger implements the Terminal Listener
	bool TerminalCommandHandler(std::string commandName, std::vector<std::string> commandArgs);


	//Functions for resolving error codes
	const char* getNrfErrorString(u32 nrfErrorCode);
	const char* getHciErrorString(u8 hciErrorCode);
	const char* getBleEventNameString(u16 bleEventId);
	const char* getGattStatusErrorString(u16 gattStatusCode);

	//Other printing functions
	void blePrettyPrintAdvData(sizedData advData);
	void convertBufferToHexString(u8* srcBuffer, u32 srcLength, char* dstBuffer, u16 bufferLength);
	u32 parseHexStringToBuffer(const char* hexString, u8* dstBuffer, u16 dstBufferSize);
	void convertTimestampToString(u32 timestamp, u16 remainderTicks, char* buffer);

};

/*
 * Define macros that rewrite the log function calls to the Logger
 * */

//Used for UART communication between node and attached pc
#ifdef ENABLE_JSON_LOGGING
#define logjson(tag, message, ...) Logger::getInstance()->log_f(false, __FILE_S__, __LINE__, message, ##__VA_ARGS__)
#define logjson_error(type) Logger::getInstance()->uart_error_f(type)
#else
#define logjson(...) do{}while(0)
#define logjson_error(...) do{}while(0)
#endif

//Currently, tracing is always enabled
#define trace(message, ...) Logger::getInstance()->log_f(false, "", 0, message, ##__VA_ARGS__)

#ifdef ENABLE_LOGGING
#define logs(message, ...) Logger::getInstance()->log_f(true, __FILE_S__, __LINE__, message, ##__VA_ARGS__)
#define logt(tag, message, ...) Logger::getInstance()->logTag_f(Logger::LOG_LINE, __FILE_S__, __LINE__, tag, message, ##__VA_ARGS__)

#else //ENABLE_LOGGING

#define logs(...) do{}while(0)
#define logt(...) do{}while(0)

#endif //ENABLE_LOGGING
