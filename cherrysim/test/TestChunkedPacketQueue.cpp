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
#include <CherrySimTester.h>
#include <CherrySimUtils.h>
#include "ConnectionQueueMemoryAllocator.h"
#include "MersenneTwister.h"

TEST(TestChunkedPacketQueue, TestSimpleAllocations)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1 });
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1 });
    simConfig.SetToPerfectConditions();
    //testerConfig.verbose = true;

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);

    NodeIndexSetter setter(0);
    MeshConnections connections = GS->cm.GetMeshConnections(ConnectionDirection::INVALID);

    ASSERT_EQ(connections.count, 1); // With only two nodes, node index 0 should have only one connection after successful clustering.

    // The following grabs deeply into the implementation. It makes sure that all the chunks are given back.
    // This is fine as we won't simulate another step. We only care about the ChunkedPacketQueue.
    MeshConnection* conn = connections.handles[0].GetConnection();
    
    ChunkedPacketQueue& queue = *conn->queue.GetQueueByPriority(DeliveryPriority::HIGH);
    queue.SimReset();

    std::array<u8, 1024> arr;
    for (size_t i = 0; i < arr.size(); i++)
    {
        arr[i] = i;
    }

    // Begin the actual testing.
    for (u32 repeats = 0; repeats < MAX_MESH_PACKET_SIZE - 1; repeats++)
    {
        ASSERT_FALSE(queue.HasPackets());
        u32 messageHandle;
        ASSERT_TRUE(queue.AddMessage(arr.data(), repeats + 1, &messageHandle));
        ASSERT_TRUE(queue.HasPackets());
        u8 readBuffer[1024 * 8];
        ASSERT_EQ(repeats + 1, queue.PeekPacket(readBuffer, sizeof(readBuffer), &messageHandle));
        for (size_t i = 0; i < repeats + 1u; i++)
        {
            ASSERT_EQ(readBuffer[i], arr[i]);
        }
        queue.PopPacket();
        ASSERT_FALSE(queue.HasPackets());
    }

    // Check if splitting works as expected. Each split contains a split header and a send_data_packed.
    ASSERT_FALSE(queue.HasPackets());
    u32 messageHandle;
    queue.SplitAndAddMessage(arr.data(), 65, 20, &messageHandle);
    ASSERT_NE(0, messageHandle);
    ASSERT_TRUE(queue.HasPackets());
    u8 readBuffer[1024 * 8];
    CheckedMemset(readBuffer, 0, sizeof(readBuffer));
    ConnPacketSplitHeader* splitHeader = (ConnPacketSplitHeader*)(readBuffer + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED);
    u8 *payloadPointer = readBuffer + SIZEOF_CONN_PACKET_SPLIT_HEADER + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED;
    constexpr u32 payloadSizeWithoutHeader = 20 - SIZEOF_CONN_PACKET_SPLIT_HEADER;

    for (u32 i = 0; i < 4; i++)
    {
        const u32 expectedSize = (i != 3 ? 20 : 10) + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED;
        u32 messageHandle;
        ASSERT_EQ(expectedSize, queue.PeekPacket(readBuffer, sizeof(readBuffer), &messageHandle));
        ASSERT_EQ(0, memcmp(readBuffer, arr.data(), SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED));
        ASSERT_EQ(splitHeader->splitCounter, i);
        ASSERT_EQ(splitHeader->splitMessageType, i != 3 ? MessageType::SPLIT_WRITE_CMD : MessageType::SPLIT_WRITE_CMD_END);
        ASSERT_EQ(0, memcmp(payloadPointer, arr.data() + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED + payloadSizeWithoutHeader * i, expectedSize - SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED - SIZEOF_CONN_PACKET_SPLIT_HEADER));
        ASSERT_TRUE(queue.HasPackets());
        queue.PopPacket();
    }
    ASSERT_FALSE(queue.HasPackets());


    static_assert(MAX_MESH_PACKET_SIZE == 200, "If this assertion does not hold, the following test may run for a very long time or not at all. Please check!");
    // Using the same loop structure 5 times in a nested manner.
#define PACKET_LOOP(i) for(u32 i = 195; i<MAX_MESH_PACKET_SIZE; i++)
    PACKET_LOOP(p1)
    {
        PACKET_LOOP(p2)
        {
            PACKET_LOOP(p3)
            {
                PACKET_LOOP(p4)
                {
                    PACKET_LOOP(p5)
                    {
#undef PACKET_LOOP
                        u32 messageHandle;
#define ADD_MSG(i) ASSERT_TRUE(queue.AddMessage(arr.data(), i, &messageHandle)); ASSERT_TRUE(queue.HasPackets());
                        ASSERT_FALSE(queue.HasPackets());
                        ADD_MSG(p1);
                        ADD_MSG(p2);
                        ADD_MSG(p3);
                        ADD_MSG(p4);
                        ADD_MSG(p5);
#undef ADD_MSG
                        u8 readBuffer[1024 * 8];
#define CHECK_AND_POP(i) ASSERT_EQ(i, queue.PeekPacket(readBuffer, sizeof(readBuffer), &messageHandle)); \
                        for (size_t loop = 0; loop < i; loop++) \
                        { \
                            ASSERT_EQ(readBuffer[loop], arr[loop]); \
                        } \
                        queue.PopPacket(); 

                        CHECK_AND_POP(p1);
                        CHECK_AND_POP(p2);
                        CHECK_AND_POP(p3);
                        CHECK_AND_POP(p4);
                        CHECK_AND_POP(p5);
#undef CHECK_AND_POP
                        ASSERT_FALSE(queue.HasPackets());

                    }
                }
            }
        }
    }

    struct Message
    {
        size_t size;
        u8 data[MAX_MESH_PACKET_SIZE];
    };
    std::queue<Message> messages;
    std::queue<Message> lookAheadMessages;
    MersenneTwister mt(1);
    // Test with random adds/pops
    for (int repeat = 0; repeat < 10000; repeat++)
    {
        ASSERT_EQ(queue.HasPackets(), messages.size() != 0);
        ASSERT_EQ(queue.HasMoreToLookAhead(), lookAheadMessages.size() != 0);

        // Roll dice if we add, pop, lookahead or rollback.
        const u32 dice = mt.NextU32();
        if (dice < 0xFFFFFFFF / 4 * 1 || messages.size() == 0)
        {
            // Add
            Message message;
            CheckedMemset(&message, 0, sizeof(message));
            message.size = mt.NextU32() % MAX_MESH_PACKET_SIZE + 1;
            for (size_t i = 0; i < message.size; i++)
            {
                message.data[i] = (u8)mt.NextU32();
            }

            u32 messageHandle;
            if (queue.AddMessage(message.data, message.size, &messageHandle))
            {
                messages.push(message);
                lookAheadMessages.push(message);
            }
        }
        else if (dice < 0xFFFFFFFF / 4 * 2)
        {
            // Pop
            u8 peekBuffer[1024];
            u32 messageHandle;
            u16 size = queue.PeekPacket(peekBuffer, sizeof(peekBuffer), &messageHandle);
            ASSERT_EQ(size, messages.front().size);
            for (size_t i = 0; i < messages.front().size; i++)
            {
                ASSERT_EQ(messages.front().data[i], peekBuffer[i]);
            }
            queue.PopPacket();
            if (messages.size() == lookAheadMessages.size()) lookAheadMessages.pop();
            messages.pop();
        }
        else if (dice < 0xFFFFFFFF / 4 * 3 && lookAheadMessages.size() > 0)
        {
            // lookahead
            u8 peekBuffer[1024];
            u16 size = queue.PeekLookAhead(peekBuffer, sizeof(peekBuffer));
            ASSERT_EQ(size, lookAheadMessages.front().size);
            for (size_t i = 0; i < lookAheadMessages.front().size; i++)
            {
                ASSERT_EQ(lookAheadMessages.front().data[i], peekBuffer[i]);
            }
            lookAheadMessages.pop();
            queue.IncrementLookAhead();
        }
        else
        {
            // Rollback
            queue.RollbackLookAhead();
            lookAheadMessages = messages;
        }
    }
}
