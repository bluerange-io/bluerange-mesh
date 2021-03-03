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

