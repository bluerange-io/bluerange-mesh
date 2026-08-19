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

#include "FmTypes.h"
#include "ConnectionQueueMemoryAllocator.h"

/*
* A specialized queue implementation for packets that are about to be sent through a connection.
* Other than a start and an end, the queue also stores a third location in its data, the "lookAhead".
* The lookAhead is used to temporarily pop data from the queue while still having the ability to
* rollback and start the peek/pop from the current read location. The implementation guarantees that
* the lookAhead is always in between the start and end (both included). This feature is required for
* resending data to the HAL in the case of a connection reestablishment because at this point the HAL
* has removed the previous connection and thus forgot about all the data that was sent to it.
 */
class ChunkedPacketQueue
{
private:
    ConnectionQueueMemoryChunk* readChunk      = nullptr;
    ConnectionQueueMemoryChunk* lookAheadChunk = nullptr;
    ConnectionQueueMemoryChunk* writeChunk     = nullptr;
    u32 amountOfPackets = 0;
    u32 messageHandle = 0;
    bool isCurrentlySendingSplitMessage = false;

    struct QueueEntryHeader
    {
        u16 size;
        u16 isSplit : 1;
        u16 isExtended : 1;
        u16 isLastSplit : 1;
        u16 reserved : 13;
    };

    struct ExtendedQueueEntryHeader
    {
        QueueEntryHeader header;
        u32 handle;
    };

    struct ChunkHeadPair
    {
        ConnectionQueueMemoryChunk* chunk;
        u32 head;
    };

    void AddMessageRaw(u8* data, u16 size);
    u16 PeekPacketRaw(u8* outData, u16 outDataSize, const ConnectionQueueMemoryChunk* chunk, u32 head, u32* messageHandle=nullptr) const;
    ChunkHeadPair GetChunkHeadPairOfIndex(u16 index) const;

    DeliveryPriority prio = DeliveryPriority::VITAL;

public:
    ChunkedPacketQueue();
    ~ChunkedPacketQueue();

    ChunkedPacketQueue(const ChunkedPacketQueue&  other) = delete;
    ChunkedPacketQueue(      ChunkedPacketQueue&& other) = delete;
    ChunkedPacketQueue& operator=(const ChunkedPacketQueue&  other) = delete;
    ChunkedPacketQueue& operator=(      ChunkedPacketQueue&& other) = delete;

    bool AddMessage(u8* data, u16 size, u32 * messageHandle, bool isSplit = false);
    u16 PeekPacket      (u8* outData, u16 outDataSize, u32* messageHandle=nullptr) const;
    u16 RandomAccessPeek(u8* outData, u16 outDataSize, u16 index, u32* messageHandle=nullptr) const; //Careful, very expensive!
    void PopPacket();
    bool HasPackets() const;
    bool IsCurrentlySendingSplitMessage() const;

    bool SplitAndAddMessage(u8* data, u16 size, u16 payloadSizePerSplit, u32 * messageHandle);

    bool IsLookAheadAndReadSame() const;
    bool HasMoreToLookAhead() const;
    u16 PeekLookAhead(u8* outData, u16 outDataSize) const;
    void IncrementLookAhead();
    void RollbackLookAhead();
    bool IsRandomAccessIndexLookedAhead(u16 index) const;

    u32 GetAmountOfPackets() const;
    void Print() const;

    DeliveryPriority GetPriority() const;
    void SetPriority(DeliveryPriority prio);

#ifdef SIM_ENABLED
    void SimReset();
#endif
};
