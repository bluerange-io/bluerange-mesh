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
#include "ConnectionAllocator.h"
#include <new>
#include "GlobalState.h"

ConnectionAllocator::ConnectionAllocator()
{
    for (unsigned i = 0; i < data.size() - 1; i++) {
        data[i].nextConnection = data.data() + (i + 1);
    }
    data[data.size() - 1].nextConnection = NO_NEXT_CONNECTION;
    dataHead = data.data();
}

ConnectionAllocator & ConnectionAllocator::GetInstance()
{
    return GS->connectionAllocator;
}

ConnectionAllocator::AnyConnection * ConnectionAllocator::AllocateMemory()
{
    if (dataHead == NO_NEXT_CONNECTION)
    {
        SIMEXCEPTION(OutOfMemoryException);                                                       //LCOV_EXCL_LINE assertion
        GS->logger.LogCustomError(CustomErrorTypes::FATAL_CONNECTION_ALLOCATOR_OUT_OF_MEMORY, 0); //LCOV_EXCL_LINE assertion
        return nullptr;                                                                           //LCOV_EXCL_LINE assertion
    }
    AnyConnection* oldHead = dataHead;
    static_assert(sizeof(void*) == 4, "Only 32 bit supported!");
    if (!Utility::CompareMem(0x00, (u8*)oldHead + sizeof(void*), sizeof(AnyConnection) - sizeof(void*))) {
        SIMEXCEPTION(MemoryCorruptionException); //LCOV_EXCL_LINE assertion
    }
    dataHead = dataHead->nextConnection;
    oldHead->nextConnection = 0;

    return oldHead;
}

MeshConnection * ConnectionAllocator::AllocateMeshConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress, u16 partnerWriteCharacteristicHandle)
{
    MeshConnection* retVal = reinterpret_cast<MeshConnection*>(AllocateMemory());
    new (retVal) MeshConnection(id, direction, partnerAddress, partnerWriteCharacteristicHandle);
    return retVal;
}
ResolverConnection * ConnectionAllocator::AllocateResolverConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress)
{
    ResolverConnection* retVal = reinterpret_cast<ResolverConnection*>(AllocateMemory());
    new (retVal) ResolverConnection(id, direction, partnerAddress);
    return retVal;
}
MeshAccessConnection * ConnectionAllocator::AllocateMeshAccessConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress, FmKeyId fmKeyId, MeshAccessTunnelType tunnelType, NodeId overwriteVirtualPartnerId)
{
    MeshAccessConnection* retVal = reinterpret_cast<MeshAccessConnection*>(AllocateMemory());
    new (retVal) MeshAccessConnection(id, direction, partnerAddress, fmKeyId, tunnelType, overwriteVirtualPartnerId);
    return retVal;
}
#if IS_ACTIVE(CLC_CONN)
#ifndef GITHUB_RELEASE
ClcAppConnection * ConnectionAllocator::AllocateClcAppConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress)
{
    ClcAppConnection* retVal = reinterpret_cast<ClcAppConnection*>(AllocateMemory());
    new (retVal) ClcAppConnection(id, direction, partnerAddress);
    return retVal;
}
#endif //GITHUB_RELEASE
#endif

void ConnectionAllocator::Deallocate(BaseConnection * bc)
{
    if (bc == nullptr) return;
    if (Utility::CompareMem(0x00, (u8*)bc, sizeof(AnyConnection))) {
                                                    //Probable reason: You deallocated this connection twice!
        SIMEXCEPTION(MemoryCorruptionException);    //It is highly likely that a valid connection is not full of zeros.
                                                    //Remove this check if this assumption ever breaks and was not a bug.

    }
    if ((void*)bc < data.data() || (void*)bc > data.data() + sizeof(data)) {
        SIMEXCEPTION(NotFromThisAllocatorException);//The allocator does not know this memory and does not own it! Wherever
                                                    //you got this connection from, it was not from this allocator!
    }

    bc->~BaseConnection();
    // The following is valid as we completely own and manage that memory region inside the
    // ConnectionAllocator, calling the destructors and constructors (via placement new) manually.
    // cppcheck-suppress memsetClass
    CheckedMemset((u8*)bc, 0, sizeof(AnyConnection));

    AnyConnection* ac = reinterpret_cast<AnyConnection*>(bc);
    ac->nextConnection = dataHead;
    dataHead = ac;
}
