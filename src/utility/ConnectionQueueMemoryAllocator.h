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
