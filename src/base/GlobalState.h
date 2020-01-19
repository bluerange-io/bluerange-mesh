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

#include <new>
#include "FruityHal.h"
#include "TimeManager.h"
#include "AdvertisingController.h"
#include "ScanController.h"
#include "GAPController.h"
#include "GATTController.h"

#include "Config.h"
#include "Boardconfig.h"
#include "ConnectionManager.h"
#include "Logger.h"
#include "Terminal.h"
#include "FlashStorage.h"
#include "RecordStorage.h"
#include "LedWrapper.h"
#include "Node.h"
#include "ConnectionAllocator.h"
#include "ModuleAllocator.h"

constexpr int MAX_MODULE_COUNT = 17;

class ClcComm;
class VsComm;
class WmComm;

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
#ifndef SIM_ENABLED
		static GlobalState& getInstance() {
			return instance;
		}
		static GlobalState instance;
#endif

		uint32_t SetEventHandlers(
			FruityHal::SystemEventHandler systemEventHandler, FruityHal::TimerEventHandler timerEventHandler,
            FruityHal::ButtonEventHandler buttonEventHandler, FruityHal::AppErrorHandler   appErrorHandler,
            FruityHal::StackErrorHandler  stackErrorHandler, FruityHal::HardfaultHandler  hardfaultHandler);
		void SetUartHandler(FruityHal::UartEventHandler uartEventHandler);

		//#################### Event Buffer ###########################
		//A global buffer for the current event, which must be 4-byte aligned
		#pragma pack(push)
		#pragma pack(4)
#if defined(NRF51) || defined(SIM_ENABLED)
		u32 currentEventBuffer[CEIL_DIV(BLE_STACK_EVT_MSG_BUF_SIZE, sizeof(uint32_t))];
#else
		uint8_t currentEventBuffer[BLE_EVT_LEN_MAX(MAX_MTU_SIZE)];
#endif
		static constexpr u16 SIZE_OF_EVENT_BUFFER = sizeof(currentEventBuffer);
		#pragma pack(pop)

		//#################### App timer ###########################
		//To keep track of timer ticks
		u32 previousRtcTicks = 0;

		//App timer uses deciseconds because milliseconds will overflow a u32 too fast
		u32 tickRemainderTimesTen = 0;
		u16 passsedTimeSinceLastTimerHandlerDs = 0;
		u16 appTimerRandomOffsetDs = 0;
		u32 appTimerDs = 0; //The app timer is used for all mesh and module timings and keeps track of the time in ds since bootup

		TimeManager timeManager;

		//########## Singletons ###############
		//Base
		ScanController scanController;
		AdvertisingController advertisingController;
		GAPController gapController;
		GATTController gattController;

		//Reference to Node
		Node node;
		Conf config;
		Boardconf boardconf;
		ConnectionManager cm;
		Logger logger;
		Terminal terminal;
		FlashStorage flashStorage;
		RecordStorage recordStorage;

		LedWrapper ledRed;
		LedWrapper ledGreen;
		LedWrapper ledBlue;
		//########## END Singletons ###############

		//########## Modules ###############
		u32 amountOfModules = 0;
		Module* activeModules[MAX_MODULE_COUNT] = { 0 };
		template<typename T>
		u32 InitializeModule(bool createModule)
		{
			static_assert(alignof(T) == 4, "This code assumes that the alignment of all modules is the same, which happens to be 4 (continue reading in comment)");
			// If this assumption would be false, we could not simply sum up the size of all modules and allocate
			// as much memory as the sum tells us. We'd have to allocate more. How much more is very hard to tell
			// thus we don't allow different alignments of modules.

			if (createModule)
			{
				if (amountOfModules >= MAX_MODULE_COUNT) {
					SIMEXCEPTION(TooManyModulesException);
				}
				void *memoryBlock = moduleAllocator.allocateMemory(sizeof(T));
				if (memoryBlock != nullptr)
				{
					activeModules[amountOfModules] = new (memoryBlock) T();
					amountOfModules++;
				}
			}
			return sizeof(T);
		}

		ConnectionAllocator connectionAllocator;
		ModuleAllocator moduleAllocator;

		//Time when the button 1 was pressed down and how long it was held
		u32 button1PressTimeDs = 0;
		u32 button1HoldTimeDs = 0;
		ButtonState button1State = ButtonState::INITAL;

		u32 pendingSysEvent = 0;

		RamRetainStruct* ramRetainStructPtr;
		u32* rebootMagicNumberPtr; //Used to save a magic number for rebooting in safe mode

		u8 scanBuffer[BLE_GAP_SCAN_PACKET_BUFFER_SIZE];

#ifdef SIM_ENABLED
		RamRetainStruct ramRetainStruct;
		u32 rebootMagicNumber;
#endif
		RamRetainStruct ramRetainStructPreviousBoot;

		FruityHal::SystemEventHandler systemEventHandler = nullptr;
		FruityHal::TimerEventHandler  timerEventHandler = nullptr;
		FruityHal::UartEventHandler   uartEventHandler = nullptr;
		FruityHal::ButtonEventHandler buttonEventHandler = nullptr;
		FruityHal::AppErrorHandler    appErrorHandler = nullptr;
		FruityHal::StackErrorHandler  stackErrorHandler = nullptr;
		FruityHal::HardfaultHandler   hardfaultHandler = nullptr;
#ifdef SIM_ENABLED
		FruityHal::DBDiscoveryHandler dbDiscoveryHandler = nullptr;
#endif
		u32 amountOfEventLooperHandlers = 0;
		SimpleArray<FruityHal::EventLooperHandler, 16> eventLooperHandlers;
		void RegisterEventLooperHandler(FruityHal::EventLooperHandler handler
		);
};

#endif /* SRC_GLOBALSTATE_H_ */
