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

#ifndef SRC_GLOBALSTATE_H_
#define SRC_GLOBALSTATE_H_

#include <FruityHal.h>

extern "C"{
#include <ble.h>
#include <ble_gatt.h>
}

#define MAX_MODULE_COUNT 16

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
class FlashStorage;
class RecordStorage;
class ClcComm;
class VsComm;

class LedWrapper;

class Module;

#ifndef SIM_ENABLED
#ifndef GS
#define GS (&(GlobalState::getInstance()))
#endif
#endif

class GlobalState
{
	public:
		GlobalState();
		static GlobalState& getInstance() {
			if (!instance) {
				instance = new GlobalState();
			}
			return *instance;
		}
		static GlobalState* instance;

		//#################### Event Buffer ###########################
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

		//#################### App timer ###########################
		//To keep track of timer ticks
		u32 previousRtcTicks = 0;

		//App timer uses deciseconds because milliseconds will overflow a u32 too fast
		u16 passsedTimeSinceLastTimerHandlerDs = 0;
		u16 appTimerRandomOffsetDs = 0;
		u32 appTimerDs = 0; //The app timer is used for all mesh and module timings and keeps track of the time in ds since bootup

		//We also keep a global time in seconds
		u16 globalTimeRemainderTicks = 0; //remainder (second fraction) is saved as ticks (32768 per second) see APP_TIMER_CLOCK_FREQ
		u32 globalTimeSec = 0; // Global time is saved as a u32 unix timestamp in seconds
		bool timeWasSet;

		//########## Singletons ###############
		//Base
		ScanController* scanController = nullptr;
		AdvertisingController* advertisingController = nullptr;
		GAPController* gapController = nullptr;
		GATTController* gattController = nullptr;

		//Reference to Node
		Node* node = nullptr;
		Conf* config = nullptr;
		Boardconf* boardconf = nullptr;
		ConnectionManager* cm = nullptr;
		Logger* logger = nullptr;
		Terminal* terminal = nullptr;
		FlashStorage* flashStorage = nullptr;
		RecordStorage* recordStorage = nullptr;
		ClcComm* clcComm = nullptr;
		VsComm* vsComm = nullptr;

		LedWrapper* ledRed = nullptr;
		LedWrapper* ledGreen = nullptr;
		LedWrapper* ledBlue = nullptr;
		//########## END Singletons ###############

		//########## Modules ###############
		Module* activeModules[MAX_MODULE_COUNT] = { 0 };

		//Time when the button 1 was pressed down and how long it was held
		u32 button1PressTimeDs = 0;
		u32 button1HoldTimeDs = 0;

		u32 pendingSysEvent = 0;

		RamRetainStruct* ramRetainStructPtr;
		u32* rebootMagicNumberPtr; //Used to save a magic number for rebooting in safe mode
};

#endif /* SRC_GLOBALSTATE_H_ */
