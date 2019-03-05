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

#include <Terminal.h>
#include <Node.h>
#include <Utility.h>
#include <mini-printf.h>

extern "C"
{
#include <ble.h>
#include <ble_hci.h>
#include <nrf_error.h>
#include <cstring>
#include <stdarg.h>
#include <app_timer.h>
}

using namespace std;

Logger::Logger()
{
	errorLogPosition = 0;
	activeLogTags.zeroData();
}

void Logger::Init()
{
	GS->terminal->AddTerminalCommandListener(this);
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
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
	if (
			//UART communication (json mode)
			(
				Config->terminalMode != TerminalMode::TERMINAL_PROMPT_MODE
				&& (logEverything || logType == LogType::UART_COMMUNICATION || IsTagEnabled(tag))
			)
			//User interaction (prompt mode)
			|| (Config->terminalMode == TerminalMode::TERMINAL_PROMPT_MODE
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

		if(Config->terminalMode == TerminalMode::TERMINAL_PROMPT_MODE){
			if (logType == LogType::LOG_LINE)
			{
				char tmp[50];
#ifndef SIM_ENABLED
				snprintf(tmp, 50, "[%s@%d %s]: ", file, line, tag);
#else
				snprintf(tmp, 50, "%07u:%u:[%s@%d %s]: ", GS->node != nullptr ? GS->appTimerDs : 0, RamConfig->defaultNodeId, file, line, tag);
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
		default:
			logjson("ERROR", "{\"type\":\"error\",\"code\":99,\"text\":\"Unknown Error\"}" SEP);
			break;
	}
}

void Logger::enableTag(const char* tag)
{
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)

	if (strlen(tag) + 1 > MAX_LOG_TAG_LENGTH) {
		logt("ERROR", "Too long");
		return;
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
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)

	if (strcmp(tag, "ERROR") == 0 || strcmp(tag, "WARNING") == 0) {
		return true;
	}
	for (u32 i = 0; i < MAX_ACTIVATE_LOG_TAG_NUM; i++)
	{
		if (strcmp(&activeLogTags[i * MAX_LOG_TAG_LENGTH], tag) == 0) return true;
	}
	return false;

#endif
}

void Logger::disableTag(const char* tag)
{
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)

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
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
	
	if (strlen(tag) + 1 > MAX_LOG_TAG_LENGTH) {
		logt("ERROR", "Too long");
		return;
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
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
	
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
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
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
	else if (TERMARGS(0, "debugnone"))
	{

		return true;
	}
	else if (TERMARGS(0, "errors"))
	{
		for(int i=0; i<errorLogPosition; i++){
			if(errorLog[i].errorType == ErrorTypes::HCI_ERROR)
			{
				trace("HCI %u %s @%u" EOL, errorLog[i].errorCode, getHciErrorString(errorLog[i].errorCode), errorLog[i].timestamp);
			}
			else if(errorLog[i].errorType == ErrorTypes::SD_CALL_ERROR)
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
#endif

/*################### Error codes #########################*/
const char* Logger::getNrfErrorString(u32 nrfErrorCode) const
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
#if defined(NRF51)
		case BLE_ERROR_NO_TX_PACKETS:
			return "BLE_ERROR_NO_TX_PACKETS";
#endif
		case 0xDEADBEEF:
			return "DEADBEEF";
		default:
			return "UNKNOWN_ERROR";
	}
#else
	return nullptr;
#endif
}

const char* Logger::getBleEventNameString(u16 bleEventId) const
{
#if defined(ENABLE_LOGGING) && defined(TERMINAL_ENABLED)
	switch (bleEventId)
	{
#if defined(NRF51)
		case BLE_EVT_TX_COMPLETE:
			return "BLE_EVT_TX_COMPLETE";
#endif
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
#ifdef NRF52
		case BLE_GATTS_EVT_HVN_TX_COMPLETE:
			return "BLE_GATTS_EVT_HVN_TX_COMPLETE";
#endif
		default:
			return "UNKNOWN_EVENT";
	}
#else
	return nullptr;
#endif
}

const char* Logger::getHciErrorString(u8 hciErrorCode) const
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
#else
	return nullptr;
#endif
}

const char* Logger::getGattStatusErrorString(u16 gattStatusCode) const
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
#else
	return nullptr;
#endif
}

void Logger::blePrettyPrintAdvData(sizedData advData) const
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
		convertBufferToHexString(fieldData.data, fieldData.length, hexString, sizeof(hexString));
		trace("Type %d, Data %s" EOL, fieldType, hexString);

		i += fieldSize + 1;
	}
}

void Logger::logError(ErrorTypes errorType, u32 errorCode, u16 extraInfo)
{
	errorLog[errorLogPosition].errorType = errorType;
	errorLog[errorLogPosition].errorCode = errorCode;
	errorLog[errorLogPosition].extraInfo = extraInfo;
	errorLog[errorLogPosition].timestamp = (GS->node != nullptr) ? GS->globalTimeSec : 0;

	//Will fill the error log until the last entry (last entry does get overwritten with latest value)
	if(errorLogPosition < NUM_ERROR_LOG_ENTRIES-1) errorLogPosition++;
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
	errorLog[errorLogPosition].timestamp = GS->globalTimeSec;

	//Will fill the error log until the last entry (last entry does get overwritten with latest value)
	if (errorLogPosition < NUM_ERROR_LOG_ENTRIES - 1) errorLogPosition++;
}

//Trivial implementation for converting the timestamp in human readable format
//This does not pay respect to any leap seconds, gap years, whatever
void Logger::convertTimestampToString(u32 timestampSec, u16 remainderTicks, char* buffer)
{
	u32 gapDays;

	u32 yearDivider = 60 * 60 * 24 * 365;
	u16 years = timestampSec / yearDivider + 1970;
	timestampSec = timestampSec % yearDivider;

	gapDays = (years - 1970) / 4 - 1;
	u32 dayDivider = 60 * 60 * 24;
	u16 days = timestampSec / dayDivider;
	days -= gapDays;
	timestampSec = timestampSec % dayDivider;

	u32 hourDivider = 60 * 60;
	u16 hours = timestampSec / hourDivider;
	timestampSec = timestampSec % hourDivider;

	u32 minuteDivider = 60;
	u16 minutes = timestampSec / minuteDivider;
	timestampSec = timestampSec % minuteDivider;

	u32 seconds = timestampSec;

	snprintf(buffer, 80, "approx. %u years, %u days, %02uh:%02um:%02us,%u ticks", years, days, hours, minutes, seconds, remainderTicks);
}

//FIXME: This method does not know the destination buffer length and could crash the system
//It also lets developers run into trouble while debugging....
void Logger::convertBufferToHexString(const u8* srcBuffer, u32 srcLength, char* dstBuffer, u16 bufferLength)
{
	memset(dstBuffer, 0x00, bufferLength);

	char* dstBufferStart = dstBuffer;
	for (u32 i = 0; i < srcLength; i++)
	{
		//We need to have at least 3 chars to place our .. if the string is too long
		if(dstBuffer - dstBufferStart + 3 < bufferLength){
			dstBuffer += snprintf(dstBuffer, bufferLength, i < srcLength - 1 ? "%02X:" : "%02X\0", srcBuffer[i]);
		} else {
			SIMEXCEPTION(BufferTooSmallException);
			dstBuffer[-3] = '.';
			dstBuffer[-2] = '.';
			dstBuffer[-1] = '\0';
			break;
		}

	};
}

u32 Logger::parseHexStringToBuffer(const char* hexString, u8* dstBuffer, u16 dstBufferSize)
{
	u32 length = (strlen(hexString)+1)/3;
	if(length > dstBufferSize){
		logt("ERROR", "too long for dstBuffer");
		length = dstBufferSize;
		SIMEXCEPTION(BufferTooSmallException);
	}

	for(u32 i = 0; i<length; i++){
		dstBuffer[i] = (u8)strtoul(hexString + (i*3), nullptr, 16);
	}

	return length;
}

void Logger::disableAll()
{
	activeLogTags.zeroData();
	logEverything = false;
}
