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
