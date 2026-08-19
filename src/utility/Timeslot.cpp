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
