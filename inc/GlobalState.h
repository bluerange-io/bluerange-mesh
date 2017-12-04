/*
 * GlobalState.h
 *
 *  Created on: 10.03.2017
 *      Author: marius
 */

#ifndef SRC_GLOBALSTATE_H_
#define SRC_GLOBALSTATE_H_


#include <types.h>
#include <LedWrapper.h>

extern "C"{
#include <ble.h>
#include <ble_gatt.h>
}

class ScanController;
class AdvertisingController;
class GAPController;
class GATTController;

class Conf;
class Boardconf;
class Node;
class ConnectionManager;
class Logger;
class Terminal;
class NewStorage;
class RecordStorage;
class ClcComm;

#ifndef SIM_ENABLED
#ifndef GS
#define GS GlobalState::getInstance()
#endif
#endif

#define REBOOT_MAGIC_NUMBER 0xE0F7213C

//This struct is used for saving information that is retained between reboots
enum RebootReason {REBOOT_REASON_UNKNOWN, REBOOT_REASON_HARDFAULT, REBOOT_REASON_APP_FAULT, REBOOT_REASON_SD_FAULT, REBOOT_REASON_PIN_RESET, REBOOT_REASON_WATCHDOG, REBOOT_REASON_FROM_OFF_STATE};
#pragma pack(push)
#pragma pack(1)
#define RAM_PERSIST_STACKSTRACE_SIZE 8
#define SIZEOF_RAM_RETAIN_STRUCT (RAM_PERSIST_STACKSTRACE_SIZE*4 + 18)
struct RamRetainStruct {
		u8 rebootReason; // e.g. hardfault, softdevice fault, app fault,...
		u32 code1;
		u32 code2;
		u32 code3;
		u8 stacktraceSize;
		u32 stacktrace[RAM_PERSIST_STACKSTRACE_SIZE];
		u32 crc32;
};
#pragma pack(pop)

class GlobalState
{
	public:
		GlobalState();
		static GlobalState* getInstance();
		static GlobalState* instance;

		//#################### Main.cpp ###########################
		//A global buffer for the current event, which must be 4-byte aligned
		#pragma pack(push)
		#pragma pack(4)
#if defined(NRF51) || defined(SIM_ENABLED)
		u32 currentEventBuffer[CEIL_DIV(BLE_STACK_EVT_MSG_BUF_SIZE, sizeof(uint32_t))];
#else
		uint8_t currentEventBuffer[BLE_EVT_LEN_MAX(BLE_GATT_ATT_MTU_DEFAULT)];
#endif
		ble_evt_t* currentEvent;
		const u16 sizeOfEvent = sizeof(currentEventBuffer);
		u16 sizeOfCurrentEvent;
		#pragma pack(pop)

		//Base
		ScanController* scanController = NULL;
		AdvertisingController* advertisingController = NULL;
		GAPController* gapController = NULL;
		GATTController* gattController = NULL;

		//Reference to Node
		Node* node = NULL;
		Conf* config = NULL;
		Boardconf* boardconf = NULL;
		ConnectionManager* cm = NULL;
		Logger* logger = NULL;
		Terminal* terminal = NULL;
		NewStorage* newStorage = NULL;
		RecordStorage* recordStorage = NULL;
		ClcComm* clcComm = NULL;

		LedWrapper* ledRed = NULL;
		LedWrapper* ledGreen = NULL;
		LedWrapper* ledBlue = NULL;

		//Time when the button 1 was pressed down and how long it was held
		u32 button1PressTimeDs = 0;
		u32 button1HoldTimeDs = 0;

		u32 pendingSysEvent = 0;

		RamRetainStruct* ramRetainStructPtr;
		u32* rebootMagicNumberPtr; //Used to save a magic number for rebooting in safe mode
};

#endif /* SRC_GLOBALSTATE_H_ */
