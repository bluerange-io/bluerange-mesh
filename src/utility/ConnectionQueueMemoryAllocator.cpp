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
        //some kind of memory corruption occurred! This is necessary to check as the Sanitizers
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
        // Wherever you got this chunk from, it is not from this allocator!
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
        //some kind of memory corruption occurred! This is necessary to check as the Sanitizers
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
    //Make sure that there is always enough space for new connections.
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
