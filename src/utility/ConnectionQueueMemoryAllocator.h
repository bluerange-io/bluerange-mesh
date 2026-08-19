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

#include "PacketQueue.h"
#include "Config.h"
#include <array>

static_assert(CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT >= (TOTAL_NUM_CONNECTIONS + 1) * AMOUNT_OF_SEND_QUEUE_PRIORITIES, "There must be at least enough chunks to support AMOUNT_OF_SEND_QUEUE_PRIORITIES chunks per connection.");
static_assert((CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT - CONNECTION_QUEUE_MEMORY_MAX_CHUNKS_PER_CONNECTION) > 12, "Amount of chunks got dangerously low compared to max chunks per connection.");
static_assert((CONNECTION_QUEUE_MEMORY_MAX_CHUNKS_PER_CONNECTION * TOTAL_NUM_CONNECTIONS) > CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT, "Chunks exist that can never be used!");

class ConnectionQueueMemoryChunk
{
    friend class ConnectionQueueMemoryAllocator;
private:

#ifdef SIM_ENABLED
    static constexpr u32 MEMORY_GUARD_VALUE_START = 0x12344321;
    static constexpr u32 MEMORY_GUARD_VALUE_END   = 0xABCDDCBA;

    u32 memoryGuardStart = MEMORY_GUARD_VALUE_START;
    bool currentlyOwnedByAllocator = true;
#endif

public:
    alignas(4) std::array<u8, CONNECTION_QUEUE_MEMORY_CHUNK_SIZE> data{};
    ConnectionQueueMemoryChunk* nextChunk = nullptr;
    u32 amountOfByteInThisChunk = 0;
    u32 currentReadHead = 0;
    u32 currentLookAheadHead = 0;

    void Reset();

private:
#ifdef SIM_ENABLED
    u32 memoryGuardEnd = MEMORY_GUARD_VALUE_END;
#endif
};

class ConnectionQueueMemoryAllocator {
private:
    std::array<ConnectionQueueMemoryChunk, CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT> chunks{};
    ConnectionQueueMemoryChunk* head = nullptr;
    u32 chunksLeft = CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT;

public:
    ConnectionQueueMemoryAllocator();

    ConnectionQueueMemoryChunk* Allocate(bool isNewConnection = false);
    void Deallocate(ConnectionQueueMemoryChunk* chunk);
    bool IsChunkAvailable(bool isNewConnection = false, u32 amountOfChunks = 1) const;
};
