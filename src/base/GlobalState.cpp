////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// ** BlueRange Mesh – Community Edition (CE)
// ** Copyright (c) 2015-2021 MWAY DIGITAL GmbH, Germany
// ** Copyright (c) 2021-2026 BlueRange GmbH, Germany
// **
// ** This file is part of BlueRange Mesh Community Edition (formerly known as
// ** FruityMesh).
// **
// ** BlueRange Mesh Community Edition is free software: you can redistribute it
// ** and/or modify it under the terms of the GNU General Public License as
// ** published by the Free Software Foundation, either version 3 of the
// ** License, or (at your option) any later version.
// **
// ** BlueRange Mesh Community Edition is distributed in the hope that it will
// ** be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// ** See the GNU General Public License for more details.
// **
// ** You should have received a copy of the GNU General Public License along
// ** with this program. If not, see https://www.gnu.org/licenses/.
// **
// ** IMPORTANT:
// ** Any modification, extension, or derivative work of this file MUST also be
// ** licensed under the GNU General Public License v3 or later and the complete
// ** corresponding source code MUST be made available.
// **
// ** Commercial Use:
// ** If you wish to use this software without the obligations of the GPLv3
// ** (including source code disclosure), a commercial license for
// ** BlueRange Mesh OEM Edition is required.
// **
// ** License violations automatically terminate your rights under this license
// ** and may result in legal action under applicable law.
// ** For further information please use the contact form at:
// ** https://bluerange.io/en/contact
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////

#include "GlobalState.h"
#ifdef SIM_ENABLED
#include <CherrySim.h>
#endif

#include "Logger.h"

#ifndef SIM_ENABLED
GlobalState GlobalState::instance;
__attribute__((section (".noinit"))) RamRetainStruct ramRetainStruct;
__attribute__((section (".noinit"))) RamRetainStruct ramRetainStructPreviousBoot;
__attribute__((section (".noinit"))) u32 rebootMagicNumber;
__attribute__((section(".noinit"))) u32 watchdogExtraInfoFlags;
__attribute__((section(".noinit"))) TemporaryEnrollment temporaryEnrollment;
#endif

/**
 * The GlobalState was introduced to create multiple instances of FruityMesh
 * in a single process. This lets us do some simulation.
 */
GlobalState::GlobalState()
{
    //Some initialization
#ifndef SIM_ENABLED
    ramRetainStructPtr = &ramRetainStruct;
    ramRetainStructPreviousBootPtr = &ramRetainStructPreviousBoot;
    rebootMagicNumberPtr = &rebootMagicNumber;
    watchdogExtraInfoFlagsPtr = &watchdogExtraInfoFlags;
    temporaryEnrollmentPtr = &temporaryEnrollment;
#else
    ramRetainStructPtr = &cherrySimInstance->currentNode->retainedRamMemory.ramRetainStruct;
    ramRetainStructPreviousBootPtr = &cherrySimInstance->currentNode->retainedRamMemory.ramRetainStructPreviousBoot;
    rebootMagicNumberPtr = &cherrySimInstance->currentNode->retainedRamMemory.rebootMagicNumber;
    watchdogExtraInfoFlagsPtr = &cherrySimInstance->currentNode->retainedRamMemory.watchdogExtraInfoFlags;
    temporaryEnrollmentPtr = &cherrySimInstance->currentNode->retainedRamMemory.temporaryEnrollment;
#endif //SIM_ENABLED
    lastSendTimestamp = 0;
    lastReceivedTimestamp = 0;
    timestampInAppTimerHandler = 0;
    eventLooperTriggerTimestamp = 0;
    fruitymeshEventLooperTriggerTimestamp = 0;
    bleEventLooperTriggerTimestamp = 0;
    socEventLooperTriggerTimestamp = 0;
    sinkNodeId = 0;
    inGetRandomLoop = false;
    inPullEventsLoop = false;
    safeBootEnabled = false;
    advertisementReceivedTimestamp = 0;
    lastReceivedFromSinkTimestamp = 0;
#if defined(SIM_ENABLED)
    CheckedMemset(currentEventBuffer, 0, sizeof(currentEventBuffer));
#endif
    if(ramRetainStructPreviousBootPtr->rebootReason != RebootReason::UNKNOWN){
        u32 crc = Utility::CalculateCrc32((u8*)ramRetainStructPreviousBootPtr, sizeof(RamRetainStruct) - 4);
        if(crc != ramRetainStructPreviousBootPtr->crc32){
            CheckedMemset(ramRetainStructPreviousBootPtr, 0x00, sizeof(RamRetainStruct));
        }
    }
    CheckedMemset(scanBuffer, 0, sizeof(scanBuffer));
}

uint32_t GlobalState::SetEventHandlers(FruityHal::AppErrorHandler    appErrorHandler)
{
    this->appErrorHandler    = appErrorHandler;
    return 0;
}

void GlobalState::SetUartHandler(FruityHal::UartEventHandler uartEventHandler)
{
    this->uartEventHandler = uartEventHandler;
}

void GlobalState::RegisterApplicationInterruptHandler(FruityHal::ApplicationInterruptHandler handler)
{
    if (numApplicationInterruptHandlers >= applicationInterruptHandlers.size())
    {
        logt("ERROR", "Could not register application interrupt handler");
        SIMEXCEPTION(BufferTooSmallException);
        logger.LogCustomError(CustomErrorTypes::FATAL_FAILED_TO_REGISTER_APPLICATION_INTERRUPT_HANDLER, 0);
        return;
    }
    applicationInterruptHandlers[numApplicationInterruptHandlers] = handler;
    numApplicationInterruptHandlers++;
}

void GlobalState::RegisterMainContextHandler(FruityHal::MainContextHandler handler)
{
    if (numMainContextHandlers >= mainContextHandlers.size())
    {
        logt("ERROR", "Could not register main context handler");
        SIMEXCEPTION(BufferTooSmallException);
        logger.LogCustomError(CustomErrorTypes::FATAL_FAILED_TO_REGISTER_MAIN_CONTEXT_HANDLER, 0);
        return;
    }
    mainContextHandlers[numMainContextHandlers] = handler;
    numMainContextHandlers++;
}
