/**

Copyright (c) 2014-2015 "M-Way Solutions GmbH"
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
#include <Terminal.h>


using namespace std;

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
	Logger(Logger const&) = delete; //Delete clone method
	void operator=(Logger const&) = delete; //Delete equal operator

	vector<string> logFilter;
	vector<string>::iterator logFilterIterator;

	char mhTraceBuffer[TRACE_BUFFER_SIZE] = { 0 };

public:
	static Logger& getInstance(){
		static Logger instance;
		return instance;
	}


#define NUM_ERROR_LOG_ENTRIES 100
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
		FINAL_DISCONNECTION=3
	};

	static errorLogEntry errorLog[NUM_ERROR_LOG_ENTRIES];
	static u8 errorLogPosition;

	bool logEverything = false;

	enum LogType {UART_COMMUNICATION, LOG_LINE, LOG_MESSAGE_ONLY, TRACE};
	enum UartErrorType {NO_ERROR, COMMAND_NOT_FOUND, ARGUMENTS_WRONG};

	void log_f(bool printLine, const char* file, i32 line, const char* message, ...);
	void logTag_f(LogType logType, const char* file, i32 line, const char* tag, const char* message, ...);

	void logError(errorTypes errorType, u32 errorCode, u16 extraInfo);

	void uart_error_f(UartErrorType type);

	void disableAll();

	//These functions are used to enable/disable a debug tag, it will then be printed to the output
	void enableTag(string tag);
	void disableTag(string tag);
	void toggleTag(string tag);

	//The print function provides an overview over the active debug tags
	void printEnabledTags();

	//The Logger implements the Terminal Listener
	bool TerminalCommandHandler(string commandName, vector<string> commandArgs);


	//Functions for resolving error codes
	static const char* getNrfErrorString(u32 nrfErrorCode);
	static const char* getHciErrorString(u8 hciErrorCode);
	static const char* getBleEventNameString(u16 bleEventId);
	static const char* getGattStatusErrorString(u16 gattStatusCode);
	static const char* getPstorageStatusErrorString(u16 operationCode);

	//Other printing functions
	void blePrettyPrintAdvData(sizedData advData);
	void convertBufferToHexString(u8* srcBuffer, u32 srcLength, char* dstBuffer, u16 bufferLength);
	void parseHexStringToBuffer(const char* hexString, u8* dstBuffer, u16 dstBufferSize);
	void convertTimestampToString(u32 timestamp, u16 remainderTicks, char* buffer);

};

/*
 * Define macros that rewrite the log function calls to the Logger
 * */

//Used for UART communication between node and attached pc
#ifdef USE_UART
#define uart(tag, message, ...) Logger::getInstance().log_f(false, __FILE_S__, __LINE__, message, ##__VA_ARGS__)
#define uart_error(type) Logger::getInstance().uart_error_f(type)
#else
#define uart(...) do{}while(0)
#define uart_error(...) do{}while(0)
#endif


#ifdef ENABLE_LOGGING

#define trace(message, ...) Logger::getInstance().logTag_f(Logger::TRACE, __FILE_S__, __LINE__, NULL, message, ##__VA_ARGS__)
#define log(message, ...) Logger::getInstance().log_f(true, __FILE_S__, __LINE__, message, ##__VA_ARGS__)
#define logt(tag, message, ...) Logger::getInstance().logTag_f(Logger::LOG_LINE, __FILE_S__, __LINE__, tag, message, ##__VA_ARGS__)

#else //ENABLE_LOGGING

#define trace(...) do{}while(0)
#define log(...) do{}while(0)
#define logt(...) do{}while(0)

#endif //ENABLE_LOGGING
