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
    ErrorType ConnectToPeripheral(const FruityHal::BleGapAddr &address, u16 connectionInterval, u16 timeout) const;

    //Encryption
    void StartEncryptingConnection(u16 connectionHandle) const;

    //Update the connection interval
    ErrorType RequestConnectionParameterUpdate(
            u16 connectionHandle, u16 minConnectionInterval,
            u16 maxConnectionInterval, u16 slaveLatency,
            u16 supervisionTimeout) const;



    //This handler is called with bleEvents from the softdevice
    void GapDisconnectedEventHandler(const FruityHal::GapDisconnectedEvent& disconnectEvent);
    void GapConnectedEventHandler(const FruityHal::GapConnectedEvent& connvectedEvent);
    void GapTimeoutEventHandler(const FruityHal::GapTimeoutEvent& gapTimeoutEvent);
    void GapSecurityInfoRequestEvenetHandler(const FruityHal::GapSecurityInfoRequestEvent& securityInfoRequestEvent);
    void GapConnectionSecurityUpdateEventHandler(const FruityHal::GapConnectionSecurityUpdateEvent& connectionSecurityUpdateEvent);

#if IS_ACTIVE(CONN_PARAM_UPDATE)
    void GapConnParamUpdateEventHandler(const FruityHal::GapConnParamUpdateEvent& connParamUpdateEvent);
    void GapConnParamUpdateRequestEventHandler(const FruityHal::GapConnParamUpdateRequestEvent& connParamUpdateRequestEvent);
#endif
};

