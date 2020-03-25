////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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
#endif

GlobalState::GlobalState()
{
	//Some initialization
	ramRetainStructPtr = &ramRetainStruct;
	ramRetainStructPreviousBootPtr = &ramRetainStructPreviousBoot;
	rebootMagicNumberPtr = &rebootMagicNumber;
	eventLooperHandlers.zeroData();
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

uint32_t GlobalState::SetEventHandlers(
    FruityHal::SystemEventHandler systemEventHandler,
    FruityHal::TimerEventHandler  timerEventHandler,
    FruityHal::ButtonEventHandler buttonEventHandler,
    FruityHal::AppErrorHandler    appErrorHandler,
    FruityHal::StackErrorHandler  stackErrorHandler,
    FruityHal::HardfaultHandler   hardfaultHandler)
{
	this->systemEventHandler = systemEventHandler;
	this->timerEventHandler  = timerEventHandler;
	this->buttonEventHandler = buttonEventHandler;
	this->appErrorHandler    = appErrorHandler;
	this->stackErrorHandler  = stackErrorHandler;
	this->hardfaultHandler   = hardfaultHandler;
	return 0;
}

void GlobalState::SetUartHandler(FruityHal::UartEventHandler uartEventHandler)
{
	this->uartEventHandler = uartEventHandler;
}

void GlobalState::RegisterEventLooperHandler(FruityHal::EventLooperHandler handler)
{
	if (amountOfEventLooperHandlers >= eventLooperHandlers.length)
	{
		SIMEXCEPTION(BufferTooSmallException);
	}
	eventLooperHandlers[amountOfEventLooperHandlers] = handler;
	amountOfEventLooperHandlers++;
}
