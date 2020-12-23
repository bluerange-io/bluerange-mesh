////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2021 M-Way Solutions GmbH
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

#pragma once

#include <new>
#include <array>
#include "FruityHal.h"
#include "TimeManager.h"
#include "AdvertisingController.h"
#include "ScanController.h"
#include "GAPController.h"
#include "GATTController.h"

#include "Config.h"
#include "Boardconfig.h"
#include "ConnectionManager.h"
#include "ConnectionQueueMemoryAllocator.h"
#include "Logger.h"
#include "Terminal.h"
#include "FlashStorage.h"
#include "RecordStorage.h"
#include "LedWrapper.h"
#include "Node.h"
#include "ConnectionAllocator.h"
#include "ModuleAllocator.h"
#if IS_ACTIVE(SIG_MESH)
#include "SigAccessLayer.h"
#endif

constexpr int MAX_MODULE_COUNT = 17;

class ClcComm;
class VsComm;
class WmComm;

class Module;

#ifndef SIM_ENABLED
#ifndef GS
#define GS (&(GlobalState::GetInstance()))
#endif
#endif

/*
 * The GlobalState holds the instances of all modules and all of the data that is not on the stack.
 * This is necessary for the mesh simulator CherrySim to be able to simulate multiple nodes in one process.
 */
class GlobalState
{
    public:
        GlobalState();
#ifndef SIM_ENABLED
        static GlobalState& GetInstance() {
            return instance;
        }
        static GlobalState instance;
#endif

        uint32_t SetEventHandlers(FruityHal::AppErrorHandler appErrorHandler);
        void SetUartHandler(FruityHal::UartEventHandler uartEventHandler);

        //#################### Event Buffer ###########################
#if defined(SIM_ENABLED)
        static constexpr u16 BLE_STACK_EVT_MSG_BUF_SIZE = 18;
        u32 currentEventBuffer[BLE_STACK_EVT_MSG_BUF_SIZE];
        static constexpr u16 SIZE_OF_EVENT_BUFFER = sizeof(currentEventBuffer);
#endif

        //#################### App timer ###########################
        //To keep track of timer ticks
        u32 previousRtcTicks = 0;

        //App timer uses deciseconds because milliseconds will overflow a u32 too fast
        u32 tickRemainderTimesTen = 0;
        u16 passsedTimeSinceLastTimerHandlerDs = 0;
        u16 appTimerRandomOffsetDs = 0;
        u32 appTimerDs = 0; //The app timer is used for all mesh and module timings and keeps track of the time in ds since bootup

        TimeManager timeManager;

        u32 amountOfRemovedConnections = 0;

        //########## Singletons ###############
        //Base
        ScanController scanController;
        AdvertisingController advertisingController;
        GAPController gapController;
        GATTController gattController;

        Node node;
        Conf config;
        Boardconf boardconf;
        ConnectionManager cm;
        ConnectionQueueMemoryAllocator connectionQueueMemoryAllocator;
        Logger logger;
        Terminal terminal;
        FlashStorage flashStorage;
        RecordStorage recordStorage;

#if IS_ACTIVE(SIG_MESH)
        SigAccessLayer sig;
#endif

        LedWrapper ledRed;
        LedWrapper ledGreen;
        LedWrapper ledBlue;
        //########## END Singletons ###############

        //########## Modules ###############
        u32 amountOfModules = 0;
        Module* activeModules[MAX_MODULE_COUNT] = {};
        template<typename T>
        u32 InitializeModule(bool createModule, u16 recordId = RECORD_STORAGE_RECORD_ID_INVALID)
        {
            static_assert(alignof(T) == 4 || alignof(T) == 8, "This code assumes that the alignment of all modules either 4 or 8 (continue reading in comment)");
            // Modules that are compiled with double support will have an alignment of 8, while others only have an alignment of 4
            // To simplify this, we simply pad all modules to 8 byte boundaries when allocating them

            u32 paddedSize = sizeof(T) + (8 - (sizeof(T) % 8)) % 8;

            if (createModule)
            {
                if (amountOfModules >= MAX_MODULE_COUNT) {
                    SIMEXCEPTION(TooManyModulesException);
                }
                void *memoryBlock = moduleAllocator.AllocateMemory(paddedSize);
                if (memoryBlock != nullptr)
                {
                    activeModules[amountOfModules] = new (memoryBlock) T();

                    // FruityMesh core modules use their moduleId as a record storage id, vendor modules must specify the id themselves
                    if (Utility::IsVendorModuleId(activeModules[amountOfModules]->moduleId)) {
                        if (recordId < RECORD_STORAGE_RECORD_ID_VENDOR_MODULE_CONFIG_BASE || recordId > RECORD_STORAGE_RECORD_ID_VENDOR_MODULE_CONFIG_MAX) {
                            recordId = RECORD_STORAGE_RECORD_ID_INVALID;
                            logt("ERROR", "Invalid recordId");
                        }
                        activeModules[amountOfModules]->recordStorageId = recordId;
                    }

                    amountOfModules++;
                }
            }
            return paddedSize;
        }

        ConnectionAllocator connectionAllocator;
        ModuleAllocator moduleAllocator;

        void* halMemory = nullptr;

        //Time when the button 1 was pressed down and how long it was held
        u32 button1PressTimeDs = 0;
        u32 button1HoldTimeDs = 0;

        u32 pendingSysEvent = 0;

        RamRetainStruct* ramRetainStructPtr;
        u32* rebootMagicNumberPtr; //Used to save a magic number for rebooting in safe mode

        u8 scanBuffer[BLE_GAP_SCAN_PACKET_BUFFER_SIZE];

#ifdef SIM_ENABLED
        RamRetainStruct ramRetainStruct;
        RamRetainStruct ramRetainStructPreviousBoot;
        u32 rebootMagicNumber;
#endif
        RamRetainStruct * ramRetainStructPreviousBootPtr;

        FruityHal::UartEventHandler   uartEventHandler = nullptr;
        FruityHal::AppErrorHandler    appErrorHandler = nullptr;
#ifdef SIM_ENABLED
        FruityHal::DBDiscoveryHandler dbDiscoveryHandler = nullptr;
#endif
        u32 numApplicationInterruptHandlers = 0;
        std::array<FruityHal::ApplicationInterruptHandler, 16> applicationInterruptHandlers{};

        //This registers a handler that will be called from the application interrupt level
        //It will be called on every application interrupt and can only be interrupted by higher priority interrupts,
        //not by other parts of the application logic
        void RegisterApplicationInterruptHandler(FruityHal::ApplicationInterruptHandler handler);

        u32 numMainContextHandlers = 0;
        std::array<FruityHal::MainContextHandler, 2> mainContextHandlers;

        //This registers a handler that will be called from the main context (non-interrupt)
        //It allows us to execute logic in the main Thread that will be interrupted by every interrupt priority
        void RegisterMainContextHandler(FruityHal::MainContextHandler handler);
};
