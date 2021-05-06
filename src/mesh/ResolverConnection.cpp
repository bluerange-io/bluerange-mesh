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

#include <ResolverConnection.h>
#include <Logger.h>
#include <ConnectionManager.h>
#include <GlobalState.h>

/**
 * The ResolverConnection must first determine the correct connection type from a small handshake
 * started by the master.
 *
 * @param id
 * @param direction
 */

ResolverConnection::ResolverConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress)
    : BaseConnection(id, direction, partnerAddress)
{
    logt("RCONN", "New Resolver Connection");

    connectionType = ConnectionType::RESOLVER;
}

void ResolverConnection::ConnectionSuccessfulHandler(u16 connectionHandle)
{
    BaseConnection::ConnectionSuccessfulHandler(connectionHandle);

    connectionState = ConnectionState::HANDSHAKING;
}

void ResolverConnection::ReceiveDataHandler(BaseConnectionSendData* sendData, u8 const * data)
{
    logt("RCONN", "Resolving Connection with received data");

    //If we receive any data, we use it to resolve the connection type
    GS->cm.ResolveConnection(this, sendData, data);
}

bool ResolverConnection::SendData(u8 const * data, MessageLength dataLength, bool reliable, u32 * messageHandle)
{
    return false;
};

void ResolverConnection::PrintStatus()
{
    const char* directionString = (direction == ConnectionDirection::DIRECTION_IN) ? "IN " : "OUT";

    trace("%s RSV state:%u, Queue:%u, hnd:%u" EOL,
        directionString,
        (u32)this->connectionState,
        GetPendingPackets(),
        connectionHandle);
}
