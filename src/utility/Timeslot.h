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

#pragma once

#include <FruityHal.h>

#include <Config.h>
#include <Boardconfig.h>
#include <Terminal.h>
#ifdef SIM_ENABLED
#include <string>
#endif
#include <array>

#if IS_ACTIVE(TIMESLOT)

/// The function type of the radio signal callback.
using TimeslotRadioSignalCallbackFn = FruityHal::RadioCallbackAction (*)(FruityHal::RadioCallbackSignalType signalType, void *userData);

/// The function type of the system event handler.
using TimeslotRadioSystemEventHandlerFn = void (*)(FruityHal::SystemEvents systemEvent, void *userData);

class Timeslot
{
    bool sessionOpen = false;

    TimeslotRadioSystemEventHandlerFn systemEventHandler = nullptr;
    void *systemEventHandlerUserData = nullptr;

    TimeslotRadioSignalCallbackFn radioCallback = nullptr;
    void *radioCallbackUserData = nullptr;

public:
    Timeslot();

    static Timeslot &GetInstance();

    /// Sets the system event handler callback and it's associated user data.
    ///
    /// The userData is passed on to the callback invocation and can e.g. be
    /// used to save a this pointer.
    ///
    /// Precondition:
    /// - The timeslot session must **not** be open.
    void SetRadioSystemEventHandler(TimeslotRadioSystemEventHandlerFn systemEventHandler, void *userData);

    /// Sets the radio signal callback and it's associated user data.
    ///
    /// The userData is passed on to the callback invocation and can e.g. be
    /// used to save a this pointer.
    ///
    /// Precondition:
    /// - The timeslot session must **not** be open.
    void SetRadioSignalCallback(TimeslotRadioSignalCallbackFn radioCallback, void *userData);

    /// Opens the timeslot session. This is a precondition to all timeslot
    /// related APIs.
    void OpenSession();

    /// Closes an active timeslot session.
    void CloseSession();

    /// Returns true if the timeslot session is currently open.
    bool IsSessionOpen() const { return sessionOpen; }

    /// Makes the initial request for a timeslot and kicks of timeslot processing.
    /// Preconditions:
    /// - The timeslot session must be opened before calling this function.
    void MakeInitialRequest(u32 initialTimeslotLength);

    /// Called by the HAL in the system event loop.
    void DispatchRadioSystemEvent(FruityHal::SystemEvents systemEvent);

    /// Called by the HAL in the radio signal callback (interrupt handler).
    FruityHal::RadioCallbackAction DispatchRadioSignalCallback(FruityHal::RadioCallbackSignalType signalType);
};

#endif // IS_ACTIVE(TIMESLOT)
