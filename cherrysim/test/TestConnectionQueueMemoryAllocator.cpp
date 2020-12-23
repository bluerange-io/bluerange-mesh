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
#include "gtest/gtest.h"
#include <vector>
#include "ConnectionQueueMemoryAllocator.h"
#include "MersenneTwister.h"

TEST(TestConnectionQueueMemoryAllocator, TestSimpleAllocations) {
    ConnectionQueueMemoryAllocator allocator;
    std::array<ConnectionQueueMemoryChunk*, CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT> chunks{};

    //Test that the memory allocator can successfully be exhausted.
    for (u32 i = 0; i < CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT; i++)
    {
        chunks[i] = allocator.Allocate(true);
        ASSERT_NE(chunks[i], nullptr);
    }

    {
        Exceptions::DisableDebugBreakOnException disabler;
        Exceptions::ExceptionDisabler<AllocatorOutOfMemoryException> aoome;
        //Allocating more chunks than there are available must give us a nullptr
        ASSERT_EQ(allocator.Allocate(true), nullptr);
    }

    //Check that all the chunks returned from the allocator are distinct.
    for (u32 i = 0; i < CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT; i++)
    {
        for (u32 k = i + 1; k < CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT; k++)
        {
            ASSERT_NE(chunks[i], chunks[k]);
        }
    }

    //Deallocate every second chunk.
    for (u32 i = 0; i < CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT; i += 2)
    {
        allocator.Deallocate(chunks[i]);
    }

    //And allocate them again.
    for (u32 i = 0; i < CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT; i += 2)
    {
        chunks[i] = allocator.Allocate(true);
        ASSERT_NE(chunks[i], nullptr);
    }

    {
        Exceptions::DisableDebugBreakOnException disabler;
        Exceptions::ExceptionDisabler<AllocatorOutOfMemoryException> aoome;
        //Allocating more chunks than there are available must give us a nullptr
        ASSERT_EQ(allocator.Allocate(true), nullptr);
    }

    //Write to all chunks...
    for (u32 i = 0; i < CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT; i++)
    {
        if (i != CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT - 1)
        {
            chunks[i]->nextChunk = chunks[i + 1];
        }
        else
        {
            chunks[i]->nextChunk = nullptr;
        }
        for (u32 k = 0; k < CONNECTION_QUEUE_MEMORY_CHUNK_SIZE; k++)
        {
            chunks[i]->data[k] = (u8)(i * 100 + k);
        }
    }
    //And make sure that the exact same data can be read back
    for (u32 i = 0; i < CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT; i++)
    {
        if (i != CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT - 1)
        {
            ASSERT_EQ(chunks[i]->nextChunk, chunks[i + 1]);
        }
        else
        {
            ASSERT_EQ(chunks[i]->nextChunk, nullptr);
        }
        for (u32 k = 0; k < CONNECTION_QUEUE_MEMORY_CHUNK_SIZE; k++)
        {
            ASSERT_EQ(chunks[i]->data[k], (u8)(i * 100 + k));
        }
    }

    //De- and allocating a chunk must zero the data.
    allocator.Deallocate(chunks[0]);
    chunks[0] = allocator.Allocate(true);
    for (u32 i = 0; i < CONNECTION_QUEUE_MEMORY_CHUNK_SIZE; i++)
    {
        ASSERT_EQ(chunks[0]->data[i], 0);
    }
    ASSERT_EQ(chunks[0]->nextChunk, nullptr);
}

static std::array<u8, CONNECTION_QUEUE_MEMORY_CHUNK_SIZE> GenerateUniqueChunkData(ConnectionQueueMemoryChunk* chunk)
{
    MersenneTwister chunkFingerprint((uint32_t)chunk); //Using the chunk memory address as seed to generate unique chunk data.

    std::array<u8, CONNECTION_QUEUE_MEMORY_CHUNK_SIZE> retVal;
    for (u32 i = 0; i < CONNECTION_QUEUE_MEMORY_CHUNK_SIZE; i++)
    {
        retVal[i] = (u8)chunkFingerprint.NextU32();
    }
    return retVal;
}

TEST(TestConnectionQueueMemoryAllocator, TestRandomAllocations) {
    ConnectionQueueMemoryAllocator allocator;
    MersenneTwister randomness(1);
    std::vector<ConnectionQueueMemoryChunk*> chunks;


    for (u32 i = 0; i < 10000; i++)
    {
        //Flip a coin if we allocate or deallocate
        if (chunks.size() == 0 || randomness.NextPsrng(UINT32_MAX / 2))
        {
            //allocate
            if (chunks.size() == CONNECTION_QUEUE_MEMORY_CHUNK_AMOUNT)
            {
                {
                    Exceptions::DisableDebugBreakOnException disabler;
                    Exceptions::ExceptionDisabler<AllocatorOutOfMemoryException> aoome;
                    ASSERT_EQ(allocator.Allocate(true), nullptr);
                }
            }
            else
            {
                ConnectionQueueMemoryChunk* chunk = allocator.Allocate(true);
                ASSERT_NE(chunk, nullptr);
                for (u32 i = 0; i < CONNECTION_QUEUE_MEMORY_CHUNK_SIZE; i++)
                {
                    ASSERT_EQ(chunk->data[i], 0);
                }
                ASSERT_EQ(chunk->nextChunk, nullptr);
                chunk->data = GenerateUniqueChunkData(chunk);
                chunk->nextChunk = chunk;
                chunks.push_back(chunk);
            }
        }
        else
        {
            //deallocate
            const u32 index = randomness.NextU32() % chunks.size();
            ASSERT_EQ(chunks[index]->data, GenerateUniqueChunkData(chunks[index]));
            ASSERT_EQ(chunks[index]->nextChunk, chunks[index]);
            allocator.Deallocate(chunks[index]);
            chunks.erase(chunks.begin() + index);
        }
    }
}
