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

    /// Opens the timeslot session. This is a precodition to all timeslot
    /// related APIs.
    void OpenSession();

    /// Closes an active timeslot session.
    void CloseSession();

    /// Returns true if the timeslot session is currently open.
    bool IsSessionOpen() const { return sessionOpen; }

    /// Makes the inital request for a timeslot and kicks of timeslot processing.
    /// Preconditions:
    /// - The timeslot session must be opened before calling this function.
    void MakeInitialRequest(u32 initialTimeslotLength);

    /// Called by the HAL in the system event loop.
    void DispatchRadioSystemEvent(FruityHal::SystemEvents systemEvent);

    /// Called by the HAL in the radio signal callback (interrupt handler).
    FruityHal::RadioCallbackAction DispatchRadioSignalCallback(FruityHal::RadioCallbackSignalType signalType);
};

#endif // IS_ACTIVE(TIMESLOT)
