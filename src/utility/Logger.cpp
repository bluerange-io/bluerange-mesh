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

#include <Logger.h>
#include "GlobalState.h"

#include <Terminal.h>
#include <Utility.h>
#include <mini-printf.h>

// Size for tracing messages to the log transport, if it is too short, messages will get truncated
#define TRACE_BUFFER_SIZE 500

Logger::Logger()
{
	errorLogPosition = 0;
	activeLogTags.zeroData();
}

Logger & Logger::getInstance()
{
	return GS->logger;
}

void Logger::Init()
{
	GS->terminal.AddTerminalCommandListener(this);
}

void Logger::log_f(bool printLine, const char* file, i32 line, const char* message, ...) const
{
	char mhTraceBuffer[TRACE_BUFFER_SIZE] = { 0 };

	//Variable argument list must be passed to vnsprintf
	va_list aptr;
	va_start(aptr, message);
	vsnprintf(mhTraceBuffer, TRACE_BUFFER_SIZE, message, aptr);
	va_end(aptr);

	if (printLine)
	{
		char tmp[40];
		snprintf(tmp, 40, "[%s@%d]: ", file, line);
		log_transport_putstring(tmp);
		log_transport_putstring(mhTraceBuffer);
		log_transport_putstring(EOL);
	}
	else
	{
		log_transport_putstring(mhTraceBuffer);
	}
}

void Logger::logTag_f(LogType logType, const char* file, i32 line, const char* tag, const char* message, ...) const
{
#if IS_ACTIVE(LOGGING) && defined(TERMINAL_ENABLED)
	if (
			//UART communication (json mode)
			(
				Conf::getInstance().terminalMode != TerminalMode::PROMPT
				&& (logEverything || logType == LogType::UART_COMMUNICATION || IsTagEnabled(tag))
			)
			//User interaction (prompt mode)
			|| (Conf::getInstance().terminalMode == TerminalMode::PROMPT
				&& (logEverything || logType == LogType::TRACE || IsTagEnabled(tag))
			)
		)
	{
		char mhTraceBuffer[TRACE_BUFFER_SIZE] = { 0 };

		//Variable argument list must be passed to vsnprintf
		va_list aptr;
		va_start(aptr, message);
		vsnprintf(mhTraceBuffer, TRACE_BUFFER_SIZE, message, aptr);
		va_end(aptr);

		if(Conf::getInstance().terminalMode == TerminalMode::PROMPT){
			if (logType == LogType::LOG_LINE)
			{
				char tmp[50];
#ifndef SIM_ENABLED
				snprintf(tmp, 50, "[%s@%d %s]: ", file, line, tag);
#else
				snprintf(tmp, 50, "%07u:%u:[%s@%d %s]: ", GS->node.IsInit() ? GS->appTimerDs : 0, RamConfig->defaultNodeId, file, line, tag);
#endif
				log_transport_putstring(tmp);
				log_transport_putstring(mhTraceBuffer);
				log_transport_putstring(EOL);
			}
			else if (logType == LogType::LOG_MESSAGE_ONLY || logType == LogType::TRACE)
			{
				log_transport_putstring(mhTraceBuffer);
			}
		} else {
			char tmp[150];
			snprintf(tmp, 150, "{\"type\":\"log\",\"tag\":\"%s\",\"file\":\"%s\",\"line\":%d,\"message\":\"", tag, file, line);
			log_transport_putstring(tmp);
			log_transport_putstring(mhTraceBuffer);
			log_transport_putstring("\"}" SEP);
		}
	}
#endif
}

void Logger::uart_error_f(UartErrorType type) const
{
	switch (type)
	{
		case UartErrorType::SUCCESS:
			logjson("ERROR", "{\"type\":\"error\",\"code\":0,\"text\":\"OK\"}" SEP);
			break;
		case UartErrorType::COMMAND_NOT_FOUND:
			logjson("ERROR", "{\"type\":\"error\",\"code\":1,\"text\":\"Command not found\"}" SEP);
			break;
		case UartErrorType::ARGUMENTS_WRONG:
			logjson("ERROR", "{\"type\":\"error\",\"code\":2,\"text\":\"Wrong Arguments\"}" SEP);
			break;
		case UartErrorType::TOO_MANY_ARGUMENTS:
			logjson("ERROR", "{\"type\":\"error\",\"code\":3,\"text\":\"Too many arguments\"}" SEP);
			break;
		default:
			logjson("ERROR", "{\"type\":\"error\",\"code\":%u,\"text\":\"Unknown Error\"}" SEP, (u32)type);
			break;
	}
}

void Logger::enableTag(const char* tag)
{
#if IS_ACTIVE(LOGGING) && defined(TERMINAL_ENABLED)

	if (strlen(tag) + 1 > MAX_LOG_TAG_LENGTH) {
		logt("ERROR", "Too long");				//LCOV_EXCL_LINE assertion
		SIMEXCEPTION(IllegalArgumentException);	//LCOV_EXCL_LINE assertion
		return;									//LCOV_EXCL_LINE assertion
	}

	char tagUpper[MAX_LOG_TAG_LENGTH];
	strcpy(tagUpper, tag);
	Utility::ToUpperCase(tagUpper);

	i32 emptySpot = -1;
	i32 test = 0;
	bool found = false;

	for (i32 i = 0; i < MAX_ACTIVATE_LOG_TAG_NUM; i++) {
		if (activeLogTags[i * MAX_LOG_TAG_LENGTH] == '\0' && emptySpot < 0) emptySpot = i;
		if (strcmp(&activeLogTags[i * MAX_LOG_TAG_LENGTH], tagUpper) == 0) found = true;
	}

	if (!found && emptySpot >= 0) {
		strcpy(&activeLogTags[emptySpot * MAX_LOG_TAG_LENGTH], tagUpper);
	}
	else if (!found && emptySpot < 0) logt("ERROR", "Too many tags");

	return;

#endif
}

bool Logger::IsTagEnabled(const char* tag) const
{
#if IS_ACTIVE(LOGGING) && defined(TERMINAL_ENABLED)

	if (strcmp(tag, "ERROR") == 0 || strcmp(tag, "WARNING") == 0) {
		return true;
	}
	for (u32 i = 0; i < MAX_ACTIVATE_LOG_TAG_NUM; i++)
	{
		if (strcmp(&activeLogTags[i * MAX_LOG_TAG_LENGTH], tag) == 0) return true;
	}
#endif

	return false;
}

void Logger::disableTag(const char* tag)
{
#if IS_ACTIVE(LOGGING) && defined(TERMINAL_ENABLED)

	char tagUpper[MAX_LOG_TAG_LENGTH];
	strcpy(tagUpper, tag);
	Utility::ToUpperCase(tagUpper);

	for (u32 i = 0; i < MAX_ACTIVATE_LOG_TAG_NUM; i++) {
		if (strcmp(&activeLogTags[i * MAX_LOG_TAG_LENGTH], tagUpper) == 0) {
			activeLogTags[i * MAX_LOG_TAG_LENGTH] = '\0';
			return;
		}
	}

#endif
}

void Logger::toggleTag(const char* tag)
{
#if IS_ACTIVE(LOGGING) && defined(TERMINAL_ENABLED)
	
	if (strlen(tag) + 1 > MAX_LOG_TAG_LENGTH) {
		logt("ERROR", "Too long");				//LCOV_EXCL_LINE assertion
		SIMEXCEPTION(IllegalArgumentException);	//LCOV_EXCL_LINE assertion
		return;									//LCOV_EXCL_LINE assertion
	}

	char tagUpper[MAX_LOG_TAG_LENGTH];
	strcpy(tagUpper, tag);
	Utility::ToUpperCase(tagUpper);

	//First, check if it is enabled and disable it after it was found
	bool found = false;
	i32 emptySpot = -1;
	for (u32 i = 0; i < MAX_ACTIVATE_LOG_TAG_NUM; i++) {
		if (activeLogTags[i * MAX_LOG_TAG_LENGTH] == '\0' && emptySpot < 0) emptySpot = i;
		if (strcmp(&activeLogTags[i * MAX_LOG_TAG_LENGTH], tagUpper) == 0) {
			activeLogTags[i * MAX_LOG_TAG_LENGTH] = '\0';
			found = true;
			// => Do not return or break as we are still looking for an empty spot
		}
	}

	//If we haven't found it, we enable it by using the previously found empty spot
	if (!found && emptySpot >= 0) {
		strcpy(&activeLogTags[emptySpot * MAX_LOG_TAG_LENGTH], tagUpper);
	}
	else if (!found && emptySpot < 0) logt("ERROR", "Too many tags");

#endif
}

void Logger::printEnabledTags() const
{
#if IS_ACTIVE(LOGGING) && defined(TERMINAL_ENABLED)
	
	if (logEverything) trace("LOG ALL IS ACTIVE" EOL);
	for (u32 i = 0; i < MAX_ACTIVATE_LOG_TAG_NUM; i++) {
		if (activeLogTags[i * MAX_LOG_TAG_LENGTH] != '\0') {
			trace("%s" EOL, &activeLogTags[i * MAX_LOG_TAG_LENGTH]);
		}
	}

#endif
}

#ifdef TERMINAL_ENABLED
bool Logger::TerminalCommandHandler(char* commandArgs[], u8 commandArgsSize)
{
#if IS_ACTIVE(LOGGING) && defined(TERMINAL_ENABLED)
	if (TERMARGS(0, "debug") && commandArgsSize >= 2)
	{
		if (TERMARGS(1, "all"))
		{
			logEverything = !logEverything;
		}
		else if (TERMARGS(1, "none"))
		{
			disableAll();
		}
		else
		{
			toggleTag(commandArgs[1]);
		}

		return true;
	}
	else if (TERMARGS(0, "debugtags"))
	{
		printEnabledTags();

		return true;
	}
	else if (TERMARGS(0, "errors"))
	{
		for(int i=0; i<errorLogPosition; i++){
			if(errorLog[i].errorType == ErrorTypes::HCI_ERROR)
			{
				trace("HCI %u %s @%u" EOL, errorLog[i].errorCode, FruityHal::getHciErrorString((FruityHal::HciErrorCode)errorLog[i].errorCode), errorLog[i].timestamp);
			}
			else if(errorLog[i].errorType == ErrorTypes::SD_CALL_ERROR)
			{
				trace("SD %u %s @%u" EOL, errorLog[i].errorCode, FruityHal::getGeneralErrorString((FruityHal::GeneralHardwareError)errorLog[i].errorCode), errorLog[i].timestamp);
			} else {
				trace("CUSTOM %u %u @%u" EOL, (u32)errorLog[i].errorType, errorLog[i].errorCode, errorLog[i].timestamp);
			}
		}

		return true;
	}

#endif
	return false;
}
#endif

void Logger::blePrettyPrintAdvData(SizedData advData) const
{

	trace("Rx Packet len %d: ", advData.length);

	u32 i = 0;
	SizedData fieldData;
	char hexString[100];
	//Loop through advertising data and parse it
	while (i < advData.length)
	{
		u8 fieldSize = advData.data[i];
		u8 fieldType = advData.data[i + 1];
		fieldData.data = advData.data + i + 2;
		fieldData.length = fieldSize - 1;

		//Print it
		Logger::convertBufferToHexString(fieldData.data, fieldData.length, hexString, sizeof(hexString));
		trace("Type %d, Data %s" EOL, fieldType, hexString);

		i += fieldSize + 1;
	}
}

void Logger::logError(ErrorTypes errorType, u32 errorCode, u32 extraInfo)
{
	errorLog[errorLogPosition].errorType = errorType;
	errorLog[errorLogPosition].errorCode = errorCode;
	errorLog[errorLogPosition].extraInfo = extraInfo;
	errorLog[errorLogPosition].timestamp = GS->node.IsInit() ? GS->timeManager.GetTime() : 0;

	//Will fill the error log until the last entry (last entry does get overwritten with latest value)
	if(errorLogPosition < NUM_ERROR_LOG_ENTRIES-1) errorLogPosition++;
}

void Logger::logCustomError(CustomErrorTypes customErrorType, u32 extraInfo)
{
	logError(ErrorTypes::CUSTOM, (u32)customErrorType, extraInfo);
}

//can be called multiple times and will increment the extra each time this happens
void Logger::logCount(ErrorTypes errorType, u32 errorCode)
{
	//Check if the erroLogEntry exists already and increment the extra if yes
	for (u32 i = 0; i < errorLogPosition; i++) {
		if (errorLog[i].errorType == errorType && errorLog[i].errorCode == errorCode) {
			errorLog[i].extraInfo++;
			return;
		}
	}

	//Create the entry
	errorLog[errorLogPosition].errorType = errorType;
	errorLog[errorLogPosition].errorCode = errorCode;
	errorLog[errorLogPosition].extraInfo = 1;
	errorLog[errorLogPosition].timestamp = GS->timeManager.GetTime();

	//Will fill the error log until the last entry (last entry does get overwritten with latest value)
	if (errorLogPosition < NUM_ERROR_LOG_ENTRIES - 1) errorLogPosition++;
}

void Logger::logCustomCount(CustomErrorTypes customErrorType)
{
	logCount(ErrorTypes::CUSTOM, (u32)customErrorType);
}

const char* base64Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
void convertBase64Block(const u8 * srcBuffer, u32 blockLength, char* dstBuffer)
{
	if (blockLength == 0 || blockLength > 3) {
		//blockLength must be 1, 2 or 3
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}

	                    dstBuffer[0] = base64Alphabet[((srcBuffer[0] & 0b11111100) >> 2)                                                             ];
	                    dstBuffer[1] = base64Alphabet[((srcBuffer[0] & 0b00000011) << 4) + (((blockLength > 1 ? srcBuffer[1] : 0) & 0b11110000) >> 4)];
	if(blockLength > 1) dstBuffer[2] = base64Alphabet[((srcBuffer[1] & 0b00001111) << 2) + (((blockLength > 2 ? srcBuffer[2] : 0) & 0b11000000) >> 6)];
	else                dstBuffer[2] = '=';
	if(blockLength > 2) dstBuffer[3] = base64Alphabet[ (srcBuffer[2] & 0b00111111)                                                                   ];
	else                dstBuffer[3] = '=';
}

void Logger::convertBufferToBase64String(const u8 * srcBuffer, u32 srcLength, char * dstBuffer, u16 bufferLength)
{
	if (bufferLength == 0) {
		SIMEXCEPTION(BufferTooSmallException); //LCOV_EXCL_LINE assertion
		return;
	}
	bufferLength--; //Reserve one byte for zero termination
	u32 requiredDstBufferLength = ((srcLength + 2) / 3) * 4;
	if (bufferLength < requiredDstBufferLength) {
		SIMEXCEPTION(BufferTooSmallException);
		srcLength = bufferLength / 4 * 3;
	}

	for (u32 i = 0; i < srcLength; i += 3) {
		u32 srcLengthLeft = srcLength - i;
		convertBase64Block(srcBuffer + i, srcLengthLeft > 3 ? 3 : srcLengthLeft, dstBuffer + i / 3 * 4);
	}

	dstBuffer[((srcLength + 2) / 3) * 4] = 0;
}

void Logger::convertBufferToHexString(const u8 * srcBuffer, u32 srcLength, char * dstBuffer, u16 bufferLength)
{
	CheckedMemset(dstBuffer, 0x00, bufferLength);

	char* dstBufferStart = dstBuffer;
	for (u32 i = 0; i < srcLength; i++)
	{
		//We need to have at least 3 chars to place our .. if the string is too long
		if (dstBuffer - dstBufferStart + 3 < bufferLength) {
			dstBuffer += snprintf(dstBuffer, bufferLength, i < srcLength - 1 ? "%02X:" : "%02X\0", srcBuffer[i]);
		}
		else {
			dstBuffer[-3] = '.';
			dstBuffer[-2] = '.';
			dstBuffer[-1] = '\0';
			break;
		}

	};
}

u32 Logger::parseEncodedStringToBuffer(const char * encodedString, u8 * dstBuffer, u16 dstBufferSize)
{
	auto len = strlen(encodedString);
	if (len >= 4 && encodedString[2] != ':') {
		return parseBase64StringToBuffer(encodedString, len, dstBuffer, dstBufferSize);
	}
	else {
		return parseHexStringToBuffer(encodedString, len, dstBuffer, dstBufferSize);
	}
}

u32 Logger::parseHexStringToBuffer(const char* hexString, u32 hexStringLength, u8* dstBuffer, u16 dstBufferSize)
{
	u32 length = (hexStringLength + 1) / 3;
	if(length > dstBufferSize){
		logt("ERROR", "too long for dstBuffer"); //LCOV_EXCL_LINE assertion
		length = dstBufferSize;					 //LCOV_EXCL_LINE assertion
		SIMEXCEPTION(BufferTooSmallException);	 //LCOV_EXCL_LINE assertion
	}

	for(u32 i = 0; i<length; i++){
		dstBuffer[i] = (u8)strtoul(hexString + (i*3), nullptr, 16);
	}

	return length;
}

u32 parseBase64Block(const char* base64Block, u8 * dstBuffer, u16 dstBufferSize)
{
	const char* base64Ptr[4];
	u32 base64Index[4];
	for (int i = 0; i < 4; i++) 
	{
		base64Ptr[i] = strchr(base64Alphabet, base64Block[i]);
		base64Index[i] = (u32)(base64Ptr[i] - base64Alphabet);
	}

	u32 length = 3;
	//Strictly speaking we accept any none base64 char as padding.
	if (base64Ptr[3] == nullptr)
		length--;
	if (base64Ptr[2] == nullptr)
		length--;
	if (base64Ptr[1] == nullptr || base64Ptr[0] == nullptr)
	{
		SIMEXCEPTION(IllegalArgumentException);	 //LCOV_EXCL_LINE assertion
		return 0;								 //LCOV_EXCL_LINE assertion
	}

	if(               dstBufferSize >= 1) dstBuffer[0] = ((base64Index[0] & 0b111111) << 2) + ((base64Index[1] & 0b110000) >> 4);
	if(length >= 2 && dstBufferSize >= 2) dstBuffer[1] = ((base64Index[1] & 0b001111) << 4) + ((base64Index[2] & 0b111100) >> 2);
	if(length >= 3 && dstBufferSize >= 3) dstBuffer[2] = ((base64Index[2] & 0b000011) << 6) + ((base64Index[3] & 0b111111));

	if (length > dstBufferSize) 
	{
		SIMEXCEPTION(BufferTooSmallException);
	}

	return length;
}

u32 Logger::parseBase64StringToBuffer(const char * base64String, const u32 base64StringLength, u8 * dstBuffer, u16 dstBufferSize)
{
	if (base64StringLength % 4 != 0)
	{
		SIMEXCEPTION(IllegalArgumentException); //The base64 string was not padded correctly.
		return 0;
	}

	const u32 amountOfBlocks = base64StringLength / 4;

	u32 amountOfBytes = 0;
	for (u32 i = 0; i < amountOfBlocks; i++)
	{
		amountOfBytes += parseBase64Block(base64String, dstBuffer, (i32)dstBufferSize - (i32)amountOfBytes);
		base64String += 4;
		dstBuffer += 3;
	}
	return amountOfBytes;
}

void Logger::disableAll()
{
	activeLogTags.zeroData();
	logEverything = false;
}

void Logger::enableAll()
{
	logEverything = true;
}
