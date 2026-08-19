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

#include "MeshConnection.h"
#include "ResolverConnection.h"
#include "MeshAccessConnection.h"

#if IS_ACTIVE(CLC_CONN)
#ifndef GITHUB_RELEASE
#include "ClcAppConnection.h"
#endif //GITHUB_RELEASE
#endif
#include "Utility.h"
#include <array>

/*
* The ConnectionAllocator is an implementation of a PoolAllocator, specialized on
* Connections. It is able to allocate and deallocate any Connection.
*/
class ConnectionAllocator {
private:
    union AnyConnection
    {
        AnyConnection* nextConnection;

        MeshConnection meshConnection;
        ResolverConnection resolverConnection;
        MeshAccessConnection meshAccessConnection;
#ifndef GITHUB_RELEASE
#if IS_ACTIVE(CLC_CONN)
        ClcAppConnection clcAppConnection;
#endif
#endif //GITHUB_RELEASE
        AnyConnection() { /*do nothing*/ }
        ~AnyConnection() {/*do nothing*/ } //LCOV_EXCL_LINE C++ deletes a destructor by default. MSVC issues a warning for it.
                                          //This suppresses it. However, it is never executed.
    };

    static constexpr AnyConnection* NO_NEXT_CONNECTION = nullptr;
    std::array<AnyConnection, TOTAL_NUM_CONNECTIONS + 1> data{};    //Max + one resolver connection.
    AnyConnection* dataHead = NO_NEXT_CONNECTION;

    AnyConnection* AllocateMemory();


public:
    ConnectionAllocator();
    static ConnectionAllocator& GetInstance();


    MeshConnection*       AllocateMeshConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress, u16 partnerWriteCharacteristicHandle);
    ResolverConnection*   AllocateResolverConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress);
    MeshAccessConnection* AllocateMeshAccessConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress, FmKeyId fmKeyId, MeshAccessTunnelType tunnelType, NodeId overwriteVirtualPartnerId);
#if IS_ACTIVE(CLC_CONN)
#ifndef GITHUB_RELEASE
    ClcAppConnection*     AllocateClcAppConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress);
#endif //GITHUB_RELEASE
#endif

    void Deallocate(BaseConnection* bc);
};
