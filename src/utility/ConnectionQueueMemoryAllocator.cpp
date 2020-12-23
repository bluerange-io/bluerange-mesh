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
#include "ConnectionQueueMemoryAllocator.h"
#include "Utility.h"
#include "GlobalState.h"
#include <new>

ConnectionQueueMemoryAllocator::ConnectionQueueMemoryAllocator()
{
    for (u32 i = 0; i < CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT - 1; i++)
    {
        chunks[i].nextChunk = chunks.data() + i + 1;
    }
    head = chunks.data();
}

ConnectionQueueMemoryChunk* ConnectionQueueMemoryAllocator::Allocate(bool isNewConnection)
{
    if (!IsChunkAvailable(isNewConnection))
    {
        return nullptr;
    }
    ConnectionQueueMemoryChunk* retVal = head;
    head = head->nextChunk;

#ifdef SIM_ENABLED
    if (   retVal->memoryGuardStart != ConnectionQueueMemoryChunk::MEMORY_GUARD_VALUE_START
        || retVal->memoryGuardEnd   != ConnectionQueueMemoryChunk::MEMORY_GUARD_VALUE_END)
    {
        //These values are written to each chunk and must never be overwritten. If they are,
        //some kind of memory corruption occured! This is necessary to check as the Sanitizers
        //do not check for such stuff as we own the complete memory region.
        SIMEXCEPTION(MemoryCorruptionException);
    }

    if (!Utility::CompareMem(0x00, retVal->data.data(), retVal->data.size())) {
        //Probably use after free!
        SIMEXCEPTION(MemoryCorruptionException); //LCOV_EXCL_LINE assertion
    }
#endif
#ifdef SIM_ENABLED
    if (!retVal->currentlyOwnedByAllocator)
    {
        //This must not happen and is a clear indication of some implementation error!
        //Either this allocator gave this chunk out twice or a connection wrote outside of
        //the data region.
        SIMEXCEPTION(IllegalStateException);
    }
#endif
    *retVal = ConnectionQueueMemoryChunk();
#ifdef SIM_ENABLED
    retVal->currentlyOwnedByAllocator = false;
#endif
    chunksLeft--;
    return retVal;
}

void ConnectionQueueMemoryAllocator::Deallocate(ConnectionQueueMemoryChunk* chunk)
{
    if (chunk == nullptr) return;

#ifdef SIM_ENABLED
    bool fromThisAllocator = false;
    for (u32 i = 0; i < CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT; i++)
    {
        if (&chunks[i] == chunk) fromThisAllocator = true;
    }
    if (!fromThisAllocator)
    {
        // Where ever you got this chunk from, it is not from this allocator!
        SIMEXCEPTION(NotFromThisAllocatorException);
    }

    if (chunk->currentlyOwnedByAllocator)
    {
        //Probably this chunk was deallocated twice!
        SIMEXCEPTION(MemoryCorruptionException);
    }

    if (   chunk->memoryGuardStart != ConnectionQueueMemoryChunk::MEMORY_GUARD_VALUE_START
        || chunk->memoryGuardEnd   != ConnectionQueueMemoryChunk::MEMORY_GUARD_VALUE_END)
    {
        //These values are written to each chunk and must never be overwritten. If they are,
        //some kind of memory corruption occured! This is necessary to check as the Sanitizers
        //do not check for such stuff as we own the complete memory region.
        SIMEXCEPTION(MemoryCorruptionException);
    }
#endif

    *chunk = ConnectionQueueMemoryChunk();
    chunk->nextChunk = head;
    head = chunk;
    chunksLeft++;
}

bool ConnectionQueueMemoryAllocator::IsChunkAvailable(bool isNewConnection, u32 amountOfChunks) const
{
    //Make sure that there is always enough place for new connections.
    //A new connection requires at least one chunk for every queue.
    //The total number of connections might be temporarily higher because of a ResolverConnection
    if (!isNewConnection && chunksLeft - amountOfChunks < ((u32)TOTAL_NUM_CONNECTIONS + 1 - GS->cm.GetConnectionsOfType(ConnectionType::INVALID, ConnectionDirection::INVALID).count) * AMOUNT_OF_SEND_QUEUE_PRIORITIES)
    {
        return false;
    }

    if (head == nullptr)
    {
        return false;
    }
    return true;
}

void ConnectionQueueMemoryChunk::Reset()
{
    data = {};
    nextChunk = nullptr;
    amountOfByteInThisChunk = 0;
    currentReadHead = 0;
    currentLookAheadHead = 0;
}
