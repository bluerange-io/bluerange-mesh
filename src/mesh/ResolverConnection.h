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

#include <FmTypes.h>
#include <BaseConnection.h>


/*
 * The ResolverConnection is instantiated as soon as an incoming connection
 * (A central connecting to us) is made to our device. Because we do not know
 * at first, which kind of connection type the central is interested in
 * (e.g. MeshConnection, MeshAccessConnection, ....), we have to instantiate
 * a Resolver Connection first. Once we determined the connection type, we replace
 * the ResolverConnection by the correct instance for a connection type.
 */
class ResolverConnection
        : public BaseConnection
{
private:
public:
    ResolverConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress);

    void ConnectionSuccessfulHandler(u16 connectionHandle) override;

    void ReceiveDataHandler(BaseConnectionSendData* sendData, u8 const * data) override;

    void PrintStatus() override;

    bool SendData(u8 const * data, MessageLength dataLength, bool reliable, u32 * messageHandle=nullptr) override;

};

