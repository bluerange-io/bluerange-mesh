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

#include "GlobalState.h"
#ifdef SIM_ENABLED
#include <CherrySim.h>
#endif

#include "Logger.h"

/**
 * The GlobalState was introduced to create multiple instances of FruityMesh
 * in a single process. This lets us do some simulation.
 */

#ifndef SIM_ENABLED
GlobalState GlobalState::instance;
__attribute__ ((section (".noinit"))) RamRetainStruct ramRetainStruct;
__attribute__ ((section (".noinit"))) RamRetainStruct ramRetainStructPreviousBoot;
__attribute__ ((section (".noinit"))) u32 rebootMagicNumber;
__attribute__ ((section (".noinit"))) u32 watchdogExtraInfoFlags;
#endif

GlobalState::GlobalState()
{
    //Some initialization
    ramRetainStructPtr = &ramRetainStruct;
    ramRetainStructPreviousBootPtr = &ramRetainStructPreviousBoot;
    rebootMagicNumberPtr = &rebootMagicNumber;
    watchdogExtraInfoFlagsPtr = &watchdogExtraInfoFlags;
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
    advertismentReceivedTimestamp = 0;
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
