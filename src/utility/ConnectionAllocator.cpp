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
