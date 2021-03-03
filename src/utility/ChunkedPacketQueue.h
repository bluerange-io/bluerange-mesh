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

#include "FmTypes.h"
#include "ConnectionQueueMemoryAllocator.h"

/*
* A specialized queue implementation for packets that are about to be sent through a connection.
* Other than a start and an end, the queue also stores a third location in its data, the "lookAhead".
* The lookAhead is used to temporarily pop data from the queue while still having the ability to
* rollback and start the peek/pop from the current read location. The implementation guarantees that
* the lookAhead is alway inbetween the start and end (both included). This feature is required for
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


