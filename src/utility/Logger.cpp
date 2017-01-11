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

#include <Logger.h>

#include <vector>
#include <algorithm>
#include <iterator>
#include <Terminal.h>
#include <Node.h>

extern "C"
{
#include <ble.h>
#include <ble_hci.h>
#include <nrf_error.h>
#include <cstring>
#include <pstorage.h>
#include <stdarg.h>
#include <app_timer.h>
}

using namespace std;

Logger::errorLogEntry Logger::errorLog[NUM_ERROR_LOG_ENTRIES];
u8 Logger::errorLogPosition = 0;

Logger::Logger()
{
	Terminal::AddTerminalCommandListener(this);
}

void Logger::log_f(bool printLine, const char* file, i32 line, const char* message, ...)
{
	memset(mhTraceBuffer, 0, TRACE_BUFFER_SIZE);

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

void Logger::logTag_f(LogType logType, const char* file, i32 line, const char* tag, const char* message, ...)
{
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
	if (
			//UART communication
			(!Config->terminalPromptMode && (
				logType == UART_COMMUNICATION
				|| tag == "ERROR"
				|| find(logFilter.begin(), logFilter.end(), tag) != logFilter.end()
			))

			//User interaction
			|| (Config->terminalPromptMode && (
				logEverything
				|| logType == TRACE
				|| tag == "ERROR"
				|| find(logFilter.begin(), logFilter.end(), tag) != logFilter.end() //Logtag is activated
			))
		)
	{
		memset(mhTraceBuffer, 0, TRACE_BUFFER_SIZE);

		//Variable argument list must be passed to vsnprintf
		va_list aptr;
		va_start(aptr, message);
		vsnprintf(mhTraceBuffer, TRACE_BUFFER_SIZE, message, aptr);
		va_end(aptr);

		if(Config->terminalPromptMode){
			if (logType == LOG_LINE)
			{
				char tmp[50];
				snprintf(tmp, 50, "[%s@%d %s]: ", file, line, tag);
				log_transport_putstring(tmp);
				log_transport_putstring(mhTraceBuffer);
				log_transport_putstring(EOL);
			}
			else if (logType == LOG_MESSAGE_ONLY || logType == TRACE)
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

void Logger::uart_error_f(UartErrorType type)
{
	switch (type)
	{
		case UartErrorType::NO_ERROR:
			uart("ERROR", "{\"type\":\"error\",\"code\":0,\"text\":\"OK\"}" SEP);
			break;
		case UartErrorType::COMMAND_NOT_FOUND:
			uart("ERROR", "{\"type\":\"error\",\"code\":1,\"text\":\"Command not found\"}" SEP);
			break;
		case UartErrorType::ARGUMENTS_WRONG:
			uart("ERROR", "{\"type\":\"error\",\"code\":2,\"text\":\"Wrong Arguments\"}" SEP);
			break;
		default:
			uart("ERROR", "{\"type\":\"error\",\"code\":99,\"text\":\"Unknown Error\"}" SEP);
			break;
	}
}

void Logger::enableTag(string tag)
{
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
	transform(tag.begin(), tag.end(), tag.begin(), ::toupper);
	logFilterIterator = find(logFilter.begin(), logFilter.end(), tag);
	//Only push tag if not found
	if (logFilterIterator == logFilter.end()){
		logFilter.push_back(tag);
	}
#endif
}

void Logger::disableTag(string tag)
{
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
	transform(tag.begin(), tag.end(), tag.begin(), ::toupper);
	logFilterIterator = find(logFilter.begin(), logFilter.end(), tag);

	if (logFilterIterator != logFilter.end())
	{
		logFilter.erase(logFilterIterator);
	}
#endif
}

void Logger::printEnabledTags()
{
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
	if(logEverything) trace("LOG ALL IS ACTIVE" EOL);
	for (string tag : logFilter)
	{
		trace("%s" EOL, tag.c_str());
	}
#endif
}

void Logger::toggleTag(string tag)
{
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
	transform(tag.begin(), tag.end(), tag.begin(), ::toupper);
	logFilterIterator = find(logFilter.begin(), logFilter.end(), tag);

	if (logFilterIterator == logFilter.end())
	{
		logFilter.push_back(tag);
	}
	else
	{
		logFilter.erase(logFilterIterator);
	}
#endif
}

bool Logger::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
	if (commandName == "debug" && commandArgs.size() == 1)
	{
		if (commandArgs[0] == "all")
		{
			logEverything = !logEverything;
		}
		else if (commandArgs[0] == "none")
		{
			disableAll();
		}
		else
		{
			toggleTag(commandArgs[0]);
		}

		return true;
	}
	else if (commandName == "debugtags")
	{
		printEnabledTags();

		return true;
	}
	else if (commandName == "debugnone")
	{

		return true;
	}
	else if (commandName == "errors")
	{
		for(int i=0; i<errorLogPosition; i++){
			if(errorLog[i].errorType == errorTypes::HCI_ERROR)
			{
				trace("HCI %u %s @%u" EOL, errorLog[i].errorCode, getHciErrorString(errorLog[i].errorCode), errorLog[i].timestamp);
			}
			else if(errorLog[i].errorType == errorTypes::SD_CALL_ERROR)
			{
				trace("SD %u %s @%u" EOL, errorLog[i].errorCode, getNrfErrorString(errorLog[i].errorCode), errorLog[i].timestamp);
			} else {
				trace("CUSTOM %u %u @%u" EOL, errorLog[i].errorType, errorLog[i].errorCode, errorLog[i].timestamp);
			}
		}

		return true;
	}

#endif
	return false;
}

/*################### Error codes #########################*/
const char* Logger::getNrfErrorString(u32 nrfErrorCode)
{
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
	switch (nrfErrorCode)
	{
		case NRF_SUCCESS:
			return "NRF_SUCCESS";
		case NRF_ERROR_SVC_HANDLER_MISSING:
			return "NRF_ERROR_SVC_HANDLER_MISSING";
		case NRF_ERROR_SOFTDEVICE_NOT_ENABLED:
			return "NRF_ERROR_SOFTDEVICE_NOT_ENABLED";
		case NRF_ERROR_INTERNAL:
			return "NRF_ERROR_INTERNAL";
		case NRF_ERROR_NO_MEM:
			return "NRF_ERROR_NO_MEM";
		case NRF_ERROR_NOT_FOUND:
			return "NRF_ERROR_NOT_FOUND";
		case NRF_ERROR_NOT_SUPPORTED:
			return "NRF_ERROR_NOT_SUPPORTED";
		case NRF_ERROR_INVALID_PARAM:
			return "NRF_ERROR_INVALID_PARAM";
		case NRF_ERROR_INVALID_STATE:
			return "NRF_ERROR_INVALID_STATE";
		case NRF_ERROR_INVALID_LENGTH:
			return "NRF_ERROR_INVALID_LENGTH";
		case NRF_ERROR_INVALID_FLAGS:
			return "NRF_ERROR_INVALID_FLAGS";
		case NRF_ERROR_INVALID_DATA:
			return "NRF_ERROR_INVALID_DATA";
		case NRF_ERROR_DATA_SIZE:
			return "NRF_ERROR_DATA_SIZE";
		case NRF_ERROR_TIMEOUT:
			return "NRF_ERROR_TIMEOUT";
		case NRF_ERROR_NULL:
			return "NRF_ERROR_NULL";
		case NRF_ERROR_FORBIDDEN:
			return "NRF_ERROR_FORBIDDEN";
		case NRF_ERROR_INVALID_ADDR:
			return "NRF_ERROR_INVALID_ADDR";
		case NRF_ERROR_BUSY:
			return "NRF_ERROR_BUSY";
		case BLE_ERROR_INVALID_CONN_HANDLE:
			return "BLE_ERROR_INVALID_CONN_HANDLE";
		case BLE_ERROR_INVALID_ATTR_HANDLE:
			return "BLE_ERROR_INVALID_ATTR_HANDLE";
		case BLE_ERROR_NO_TX_PACKETS:
			return "BLE_ERROR_NO_TX_PACKETS";
		case 0xDEADBEEF:
			return "DEADBEEF";
		default:
			return "UNKNOWN_ERROR";
	}
#endif
}

const char* Logger::getBleEventNameString(u16 bleEventId)
{
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
	switch (bleEventId)
	{
		case BLE_EVT_TX_COMPLETE:
			return "BLE_EVT_TX_COMPLETE";
		case BLE_EVT_USER_MEM_REQUEST:
			return "BLE_EVT_USER_MEM_REQUEST";
		case BLE_EVT_USER_MEM_RELEASE:
			return "BLE_EVT_USER_MEM_RELEASE";
		case BLE_GAP_EVT_CONNECTED:
			return "BLE_GAP_EVT_CONNECTED";
		case BLE_GAP_EVT_DISCONNECTED:
			return "BLE_GAP_EVT_DISCONNECTED";
		case BLE_GAP_EVT_CONN_PARAM_UPDATE:
			return "BLE_GAP_EVT_CONN_PARAM_UPDATE";
		case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
			return "BLE_GAP_EVT_SEC_PARAMS_REQUEST";
		case BLE_GAP_EVT_SEC_INFO_REQUEST:
			return "BLE_GAP_EVT_SEC_INFO_REQUEST";
		case BLE_GAP_EVT_PASSKEY_DISPLAY:
			return "BLE_GAP_EVT_PASSKEY_DISPLAY";
		case BLE_GAP_EVT_AUTH_KEY_REQUEST:
			return "BLE_GAP_EVT_AUTH_KEY_REQUEST";
		case BLE_GAP_EVT_AUTH_STATUS:
			return "BLE_GAP_EVT_AUTH_STATUS";
		case BLE_GAP_EVT_CONN_SEC_UPDATE:
			return "BLE_GAP_EVT_CONN_SEC_UPDATE";
		case BLE_GAP_EVT_TIMEOUT:
			return "BLE_GAP_EVT_TIMEOUT";
		case BLE_GAP_EVT_RSSI_CHANGED:
			return "BLE_GAP_EVT_RSSI_CHANGED";
		case BLE_GAP_EVT_ADV_REPORT:
			return "BLE_GAP_EVT_ADV_REPORT";
		case BLE_GAP_EVT_SEC_REQUEST:
			return "BLE_GAP_EVT_SEC_REQUEST";
		case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
			return "BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST";
		case BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP:
			return "BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP";
		case BLE_GATTC_EVT_REL_DISC_RSP:
			return "BLE_GATTC_EVT_REL_DISC_RSP";
		case BLE_GATTC_EVT_CHAR_DISC_RSP:
			return "BLE_GATTC_EVT_CHAR_DISC_RSP";
		case BLE_GATTC_EVT_DESC_DISC_RSP:
			return "BLE_GATTC_EVT_DESC_DISC_RSP";
		case BLE_GATTC_EVT_CHAR_VAL_BY_UUID_READ_RSP:
			return "BLE_GATTC_EVT_CHAR_VAL_BY_UUID_READ_RSP";
		case BLE_GATTC_EVT_READ_RSP:
			return "BLE_GATTC_EVT_READ_RSP";
		case BLE_GATTC_EVT_CHAR_VALS_READ_RSP:
			return "BLE_GATTC_EVT_CHAR_VALS_READ_RSP";
		case BLE_GATTC_EVT_WRITE_RSP:
			return "BLE_GATTC_EVT_WRITE_RSP";
		case BLE_GATTC_EVT_HVX:
			return "BLE_GATTC_EVT_HVX";
		case BLE_GATTC_EVT_TIMEOUT:
			return "BLE_GATTC_EVT_TIMEOUT";
		case BLE_GATTS_EVT_WRITE:
			return "BLE_GATTS_EVT_WRITE";
		case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
			return "BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST";
		case BLE_GATTS_EVT_SYS_ATTR_MISSING:
			return "BLE_GATTS_EVT_SYS_ATTR_MISSING";
		case BLE_GATTS_EVT_HVC:
			return "BLE_GATTS_EVT_HVC";
		case BLE_GATTS_EVT_SC_CONFIRM:
			return "BLE_GATTS_EVT_SC_CONFIRM";
		case BLE_GATTS_EVT_TIMEOUT:
			return "BLE_GATTS_EVT_TIMEOUT";
		default:
			return "Unknown Error";
	}
#endif
}

const char* Logger::getHciErrorString(u8 hciErrorCode)
{
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
	switch (hciErrorCode)
	{

		case BLE_HCI_STATUS_CODE_SUCCESS:
			return "Success";

		case BLE_HCI_STATUS_CODE_UNKNOWN_BTLE_COMMAND:
			return "Unknown BLE Command";

		case BLE_HCI_STATUS_CODE_UNKNOWN_CONNECTION_IDENTIFIER:
			return "Unknown Connection Identifier";

		case BLE_HCI_AUTHENTICATION_FAILURE:
			return "Authentication Failure";

		case BLE_HCI_CONN_FAILED_TO_BE_ESTABLISHED:
			return "Connection Failed to be Established";

		case BLE_HCI_CONN_INTERVAL_UNACCEPTABLE:
			return "Connection Interval Unacceptable";

		case BLE_HCI_CONN_TERMINATED_DUE_TO_MIC_FAILURE:
			return "Connection Terminated due to MIC Failure";

		case BLE_HCI_CONNECTION_TIMEOUT:
			return "Connection Timeout";

		case BLE_HCI_CONTROLLER_BUSY:
			return "Controller Busy";

		case BLE_HCI_DIFFERENT_TRANSACTION_COLLISION:
			return "Different Transaction Collision";

		case BLE_HCI_DIRECTED_ADVERTISER_TIMEOUT:
			return "Directed Adverisement Timeout";

		case BLE_HCI_INSTANT_PASSED:
			return "Instant Passed";

		case BLE_HCI_LOCAL_HOST_TERMINATED_CONNECTION:
			return "Local Host Terminated Connection";

		case BLE_HCI_MEMORY_CAPACITY_EXCEEDED:
			return "Memory Capacity Exceeded";

		case BLE_HCI_PAIRING_WITH_UNIT_KEY_UNSUPPORTED:
			return "Pairing with Unit Key Unsupported";

		case BLE_HCI_REMOTE_DEV_TERMINATION_DUE_TO_LOW_RESOURCES:
			return "Remote Device Terminated Connection due to low resources";

		case BLE_HCI_REMOTE_DEV_TERMINATION_DUE_TO_POWER_OFF:
			return "Remote Device Terminated Connection due to power off";

		case BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION:
			return "Remote User Terminated Connection";

		case BLE_HCI_STATUS_CODE_COMMAND_DISALLOWED:
			return "Command Disallowed";

		case BLE_HCI_STATUS_CODE_INVALID_BTLE_COMMAND_PARAMETERS:
			return "Invalid BLE Command Parameters";

		case BLE_HCI_STATUS_CODE_INVALID_LMP_PARAMETERS:
			return "Invalid LMP Parameters";

		case BLE_HCI_STATUS_CODE_LMP_PDU_NOT_ALLOWED:
			return "LMP PDU Not Allowed";

		case BLE_HCI_STATUS_CODE_LMP_RESPONSE_TIMEOUT:
			return "LMP Response Timeout";

		case BLE_HCI_STATUS_CODE_PIN_OR_KEY_MISSING:
			return "Pin or Key missing";

		case BLE_HCI_STATUS_CODE_UNSPECIFIED_ERROR:
			return "Unspecified Error";

		case BLE_HCI_UNSUPPORTED_REMOTE_FEATURE:
			return "Unsupported Remote Feature";
		default:
			return "Unknown HCI error";
	}
#endif
}

const char* Logger::getGattStatusErrorString(u16 gattStatusCode)
{
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
	switch (gattStatusCode)
	{
		case BLE_GATT_STATUS_SUCCESS:
			return "Success";
		case BLE_GATT_STATUS_UNKNOWN:
			return "Unknown or not applicable status";
		case BLE_GATT_STATUS_ATTERR_INVALID:
			return "ATT Error: Invalid Error Code";
		case BLE_GATT_STATUS_ATTERR_INVALID_HANDLE:
			return "ATT Error: Invalid Attribute Handle";
		case BLE_GATT_STATUS_ATTERR_READ_NOT_PERMITTED:
			return "ATT Error: Read not permitted";
		case BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED:
			return "ATT Error: Write not permitted";
		case BLE_GATT_STATUS_ATTERR_INVALID_PDU:
			return "ATT Error: Used in ATT as Invalid PDU";
		case BLE_GATT_STATUS_ATTERR_INSUF_AUTHENTICATION:
			return "ATT Error: Authenticated link required";
		case BLE_GATT_STATUS_ATTERR_REQUEST_NOT_SUPPORTED:
			return "ATT Error: Used in ATT as Request Not Supported";
		case BLE_GATT_STATUS_ATTERR_INVALID_OFFSET:
			return "ATT Error: Offset specified was past the end of the attribute";
		case BLE_GATT_STATUS_ATTERR_INSUF_AUTHORIZATION:
			return "ATT Error: Used in ATT as Insufficient Authorisation";
		case BLE_GATT_STATUS_ATTERR_PREPARE_QUEUE_FULL:
			return "ATT Error: Used in ATT as Prepare Queue Full";
		case BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND:
			return "ATT Error: Used in ATT as Attribute not found";
		case BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_LONG:
			return "ATT Error: Attribute cannot be read or written using read/write blob requests";
		case BLE_GATT_STATUS_ATTERR_INSUF_ENC_KEY_SIZE:
			return "ATT Error: Encryption key size used is insufficient";
		case BLE_GATT_STATUS_ATTERR_INVALID_ATT_VAL_LENGTH:
			return "ATT Error: Invalid value size";
		case BLE_GATT_STATUS_ATTERR_UNLIKELY_ERROR:
			return "ATT Error: Very unlikely error";
		case BLE_GATT_STATUS_ATTERR_INSUF_ENCRYPTION:
			return "ATT Error: Encrypted link required";
		case BLE_GATT_STATUS_ATTERR_UNSUPPORTED_GROUP_TYPE:
			return "ATT Error: Attribute type is not a supported grouping attribute";
		case BLE_GATT_STATUS_ATTERR_INSUF_RESOURCES:
			return "ATT Error: Encrypted link required";
		case BLE_GATT_STATUS_ATTERR_RFU_RANGE1_BEGIN:
			return "ATT Error: Reserved for Future Use range #1 begin";
		case BLE_GATT_STATUS_ATTERR_RFU_RANGE1_END:
			return "ATT Error: Reserved for Future Use range #1 end";
		case BLE_GATT_STATUS_ATTERR_APP_BEGIN:
			return "ATT Error: Application range begin";
		case BLE_GATT_STATUS_ATTERR_APP_END:
			return "ATT Error: Application range end";
		case BLE_GATT_STATUS_ATTERR_RFU_RANGE2_BEGIN:
			return "ATT Error: Reserved for Future Use range #2 begin";
		case BLE_GATT_STATUS_ATTERR_RFU_RANGE2_END:
			return "ATT Error: Reserved for Future Use range #2 end";
		case BLE_GATT_STATUS_ATTERR_RFU_RANGE3_BEGIN:
			return "ATT Error: Reserved for Future Use range #3 begin";
		case BLE_GATT_STATUS_ATTERR_RFU_RANGE3_END:
			return "ATT Error: Reserved for Future Use range #3 end";
		case BLE_GATT_STATUS_ATTERR_CPS_CCCD_CONFIG_ERROR:
			return "ATT Common Profile and Service Error: Client Characteristic Configuration Descriptor improperly configured";
		case BLE_GATT_STATUS_ATTERR_CPS_PROC_ALR_IN_PROG:
			return "ATT Common Profile and Service Error: Procedure Already in Progress";
		case BLE_GATT_STATUS_ATTERR_CPS_OUT_OF_RANGE:
			return "ATT Common Profile and Service Error: Out Of Range";
		default:
			return "Unknown GATT status";
	}
#endif
}

void Logger::blePrettyPrintAdvData(sizedData advData)
{

	trace("Rx Packet len %d: ", advData.length);

	u32 i = 0;
	sizedData fieldData;
	char hexString[100];
	//Loop through advertising data and parse it
	while (i < advData.length)
	{
		u8 fieldSize = advData.data[i];
		u8 fieldType = advData.data[i + 1];
		fieldData.data = advData.data + i + 2;
		fieldData.length = fieldSize - 1;

		//Print it
		convertBufferToHexString(fieldData.data, fieldData.length, hexString, 100);
		trace("Type %d, Data %s" EOL, fieldType, hexString);

		i += fieldSize + 1;
	}
}

void Logger::logError(errorTypes errorType, u32 errorCode, u16 extraInfo)
{
	errorLog[errorLogPosition].errorType = errorType;
	errorLog[errorLogPosition].errorCode = errorCode;
	errorLog[errorLogPosition].extraInfo = extraInfo;
	errorLog[errorLogPosition].timestamp = Node::getInstance()->globalTimeSec;

	//Will fill the error log until the last entry (last entry does get overwritten with latest value)
	if(errorLogPosition < NUM_ERROR_LOG_ENTRIES-1) errorLogPosition++;
}

//Trivial implementation for converting the timestamp in human readable format
//This does not pay respect to any leap seconds, gap years, whatever
void Logger::convertTimestampToString(u32 timestampSec, u16 remainderTicks, char* buffer)
{
	u32 yearDivider = 60 * 60 * 24 * 365;
	u16 years = timestampSec / yearDivider + 1970;
	timestampSec = timestampSec % yearDivider;

	u32 dayDivider = 60 * 60 * 24;
	u16 days = timestampSec / dayDivider + 1;
	timestampSec = timestampSec % dayDivider;

	u32 hourDivider = 60 * 60;
	u16 hours = timestampSec / hourDivider;
	timestampSec = timestampSec % hourDivider;

	u32 minuteDivider = 60;
	u16 minutes = timestampSec / minuteDivider;
	timestampSec = timestampSec % minuteDivider;

	u32 seconds = timestampSec;

	sprintf(buffer, "approx. %u years, %u days, %02uh:%02um:%02us,%u ticks", years, days, hours, minutes, seconds, remainderTicks);
}

//FIXME: This method does not know the destination buffer length and could crash the system
//It also lets developers run into trouble while debugging....
void Logger::convertBufferToHexString(u8* srcBuffer, u32 srcLength, char* dstBuffer, u16 bufferLength)
{
	memset(dstBuffer, 0x00, bufferLength);

	char* dstBufferStart = dstBuffer;
	for (u32 i = 0; i < srcLength; i++)
	{
		dstBuffer += sprintf(dstBuffer, i < srcLength - 1 ? "%02X:" : "%02X\0", srcBuffer[i]);
		if(dstBuffer - dstBufferStart > bufferLength - 7){
			dstBuffer[0] = '.';
			dstBuffer[1] = '.';
			dstBuffer[2] = '.';
			break;
		}
	};
}

void Logger::parseHexStringToBuffer(const char* hexString, u8* dstBuffer, u16 dstBufferSize)
{
	u32 length = (strlen(hexString)+1)/3;
	if(length > dstBufferSize) logt("ERROR", "too long for dstBuffer");

	for(u32 i = 0; i<length; i++){
		dstBuffer[i] = (u8)strtoul(hexString + (i*3), NULL, 16);
	}
}

void Logger::disableAll()
{
	logFilter.clear();
	logEverything = false;
}

const char* Logger::getPstorageStatusErrorString(u16 operationCode)
{
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
	switch(operationCode){
		case PSTORAGE_CLEAR_OP_CODE:
			return "Error when Clear Operation was requested";

		case PSTORAGE_LOAD_OP_CODE:
			return "Error when Load Operation was requested";

		case PSTORAGE_STORE_OP_CODE:
			return "Error when Store Operation was requested";

		case PSTORAGE_UPDATE_OP_CODE:
			return "Update an already touched storage block";
		default:
			return "Unknown operation code";
	}
#endif
}
