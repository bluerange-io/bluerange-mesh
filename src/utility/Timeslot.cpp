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

#include "Timeslot.h"

#include <GlobalState.h>
#include <IoModule.h>
#include <Logger.h>

#if IS_ACTIVE(TIMESLOT)

Timeslot::Timeslot()
{
}

Timeslot & Timeslot::GetInstance()
{
    return GS->timeslot;
}

void Timeslot::SetRadioSystemEventHandler(TimeslotRadioSystemEventHandlerFn systemEventHandler, void *userData)
{
    if (sessionOpen)
    {
        logt("TIMESLOT", "setting the system event handler while a radio session is open is prohibited");
        return;
    }

    this->systemEventHandler = systemEventHandler;
    this->systemEventHandlerUserData = userData;
}

void Timeslot::SetRadioSignalCallback(TimeslotRadioSignalCallbackFn radioCallback, void *userData)
{
    if (sessionOpen)
    {
        logt("TIMESLOT", "setting the radio callback while a radio session is open is prohibited");
        return;
    }

    this->radioCallback = radioCallback;
    this->radioCallbackUserData = userData;
}

void Timeslot::OpenSession()
{
    if (sessionOpen)
    {
        logt("TIMESLOT", "timeslot session is already open");
        return;
    }

    ErrorType err = FruityHal::TimeslotOpenSession();
    if (err != ErrorType::SUCCESS)
    {
        logt("TIMESLOT", "FruityHal::TimeslotOpenSession failed (%u)", (u32)err);
        return;
    }

    sessionOpen = true;
}

void Timeslot::CloseSession()
{
    if (!sessionOpen)
    {
        logt("TIMESLOT", "timeslot session is not open");
    }

    FruityHal::TimeslotCloseSession();
}

void Timeslot::MakeInitialRequest(u32 initialTimeslotLength)
{
    if (!this->sessionOpen)
    {
        logt("TIMESLOT", "initial request requires an open session");
        return;
    }

    // the first request must be of type 'earliest'
    FruityHal::TimeslotConfigureNextEventEarliest(initialTimeslotLength);
    const auto err = FruityHal::TimeslotRequestNextEvent();
    if (err != ErrorType::SUCCESS)
    {
        logt("TIMESLOT", "TimeslotRequestNextEvent failed (%u)", (u32)err);
        FruityHal::TimeslotCloseSession(); // TODO: handle errors from closing the session
    }
}

void Timeslot::DispatchRadioSystemEvent(FruityHal::SystemEvents systemEvent)
{
    switch (systemEvent)
    {
        case FruityHal::SystemEvents::RADIO_SESSION_CLOSED:
            sessionOpen = false;
            logt("TIMESLOT", "timeslot session is now closed (RADIO_SESSION_CLOSED)");
            FALLTHROUGH;

        case FruityHal::SystemEvents::RADIO_SIGNAL_CALLBACK_INVALID_RETURN:
        case FruityHal::SystemEvents::RADIO_SESSION_IDLE:
        case FruityHal::SystemEvents::RADIO_BLOCKED:
        case FruityHal::SystemEvents::RADIO_CANCELED:
            if (this->systemEventHandler)
            {
                this->systemEventHandler(systemEvent, this->systemEventHandlerUserData);
            }
            break;

        default:
            // other events are handled by other event handlers, so do nothing
            break;
    }
}

FruityHal::RadioCallbackAction Timeslot::DispatchRadioSignalCallback(FruityHal::RadioCallbackSignalType signalType)
{
    if (this->radioCallback)
    {
        return this->radioCallback(signalType, this->radioCallbackUserData);
    }
    else 
    {
        return FruityHal::RadioCallbackAction::END;
    }
}

#endif // IS_ACTIVE(TIMESLOT)
