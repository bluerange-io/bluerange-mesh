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

#include "FmTypes.h"
#include "FruityHal.h"

/*
 * The GAP Controller wraps SoftDevice calls for initiating and accepting connections
 * It should also provide encryption in the future.
 */
class GAPController
{
public:
    static GAPController& GetInstance();
    //Initialize the GAP module
    void BleConfigureGAP() const;

    //Connects to a peripheral with the specified address and calls the corresponding callbacks
    ErrorType ConnectToPeripheral(const FruityHal::BleGapAddr &address, u16 connectionInterval, u16 timeout, u16 overwriteSlaveLatency = GAP_CONTROLLER_USE_CONFIGURED_SLAVE_LATENCY, bool maxScanDutyCycle = false) const;

    //Encryption
    void StartEncryptingConnection(u16 connectionHandle) const;

    //Update the connection interval
    ErrorType RequestConnectionParameterUpdate(
            u16 connectionHandle, u16 minConnectionInterval,
            u16 maxConnectionInterval, u16 slaveLatency,
            u16 supervisionTimeout) const;



    //This handler is called with bleEvents from the softdevice
    void GapDisconnectedEventHandler(const FruityHal::GapDisconnectedEvent& disconnectEvent);
    void GapConnectedEventHandler(const FruityHal::GapConnectedEvent& connectedEvent);
    void GapTimeoutEventHandler(const FruityHal::GapTimeoutEvent& gapTimeoutEvent);
    void GapSecurityInfoRequestEventHandler(const FruityHal::GapSecurityInfoRequestEvent& securityInfoRequestEvent);
    void GapConnectionSecurityUpdateEventHandler(const FruityHal::GapConnectionSecurityUpdateEvent& connectionSecurityUpdateEvent);

#if IS_ACTIVE(CONN_PARAM_UPDATE)
    void GapConnParamUpdateEventHandler(const FruityHal::GapConnParamUpdateEvent& connParamUpdateEvent);
    void GapConnParamUpdateRequestEventHandler(const FruityHal::GapConnParamUpdateRequestEvent& connParamUpdateRequestEvent);
#endif
};

