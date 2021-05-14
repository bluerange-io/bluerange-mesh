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

#include <PacketQueue.h>
#include "GlobalState.h"
#include <Logger.h>
#include <cstring>
#include "Utility.h"
#include "ChunkedPacketQueue.h"

// Adds a message. Private as the method does not check for size or nullptrs, the caller has to do this.
void ChunkedPacketQueue::AddMessageRaw(u8* data, u16 size)
{
    writeChunk->amountOfByteInThisChunk = Utility::NextMultipleOf(writeChunk->amountOfByteInThisChunk, sizeof(u32));
    const u32 sizeLeftInCurrentWriteChunk = CONNECTION_QUEUE_MEMORY_CHUNK_SIZE > writeChunk->amountOfByteInThisChunk ? CONNECTION_QUEUE_MEMORY_CHUNK_SIZE - writeChunk->amountOfByteInThisChunk : 0;

    if (sizeLeftInCurrentWriteChunk >= size)
    {
        // The data fits completely in the current writeChunk
        CheckedMemcpy(writeChunk->data.data() + writeChunk->amountOfByteInThisChunk, data, size);
        writeChunk->amountOfByteInThisChunk += size;
        writeChunk->amountOfByteInThisChunk = Utility::NextMultipleOf(writeChunk->amountOfByteInThisChunk, sizeof(u32));
    }
    else
    {
        // The data must be split between the current writeChunk and a new chunk
        if (sizeLeftInCurrentWriteChunk > 0)
        {
            CheckedMemcpy(writeChunk->data.data() + writeChunk->amountOfByteInThisChunk, data, sizeLeftInCurrentWriteChunk);
            writeChunk->amountOfByteInThisChunk += sizeLeftInCurrentWriteChunk;
            writeChunk->amountOfByteInThisChunk = Utility::NextMultipleOf(writeChunk->amountOfByteInThisChunk, sizeof(u32));
        }
        ConnectionQueueMemoryChunk* newChunk = GS->connectionQueueMemoryAllocator.Allocate();
        if (!newChunk)
        {
            // Implementation error! The calling function should have made sure that there is a chunk available!
            SIMEXCEPTION(IllegalStateException);
            return;
        }
        writeChunk->nextChunk = newChunk;
        writeChunk = newChunk;
        CheckedMemcpy(writeChunk->data.data(), data + sizeLeftInCurrentWriteChunk, size - sizeLeftInCurrentWriteChunk);
        writeChunk->amountOfByteInThisChunk += size - sizeLeftInCurrentWriteChunk;
        writeChunk->amountOfByteInThisChunk = Utility::NextMultipleOf(writeChunk->amountOfByteInThisChunk, sizeof(u32));
    }
}

u16 ChunkedPacketQueue::PeekPacketRaw(u8* outData, u16 outDataSize, const ConnectionQueueMemoryChunk* chunk, u32 head, u32* messageHandle) const
{
    const QueueEntryHeader* header = (const QueueEntryHeader*)(chunk->data.data() + head);
    const u16 headerSize = header->isExtended ? sizeof(ExtendedQueueEntryHeader) : sizeof(QueueEntryHeader);
    if (header->reserved != 0)
    {
        SIMEXCEPTION(MemoryCorruptionException);
        return 0;
    }
    if (outDataSize < header->size)
    {
        SIMEXCEPTION(IllegalStateException);
        return 0;
    }
    if (header->size == 0)
    {
        SIMEXCEPTION(MemoryCorruptionException);
        return 0;
    }
    if (outData == nullptr)
    {
        SIMEXCEPTION(IllegalArgumentException);
        return 0;
    }

    // If the following static_assert failes, the messageStart calculation would be wrong.
    static_assert(sizeof(ExtendedQueueEntryHeader) % sizeof(u32) == 0, "Sizeof ExtendedQueueEntryHeader must be a multiple of 4!");
    static_assert(sizeof(QueueEntryHeader) % sizeof(u32) == 0, "Sizeof QueueEntryHeader must be a multiple of 4!");
    const u32 messageStartOffset = head + headerSize;
    if (messageHandle != nullptr)
    {
        if (header->isExtended)
        {
            const ExtendedQueueEntryHeader* header = (const ExtendedQueueEntryHeader*)(chunk->data.data() + head);
            *messageHandle = header->handle;
        }
        else
        {
            *messageHandle = 0;
        }
    }
    
    if (messageStartOffset + header->size < CONNECTION_QUEUE_MEMORY_CHUNK_SIZE)
    {
        // The message can be read from a single chunk.
        CheckedMemcpy(outData, chunk->data.data() + messageStartOffset, header->size);
    }
    else
    {
        // The message is split across two chunks.
        const u32 amountOfDataInFirstChunk = CONNECTION_QUEUE_MEMORY_CHUNK_SIZE > messageStartOffset ? CONNECTION_QUEUE_MEMORY_CHUNK_SIZE - messageStartOffset : 0;
        if (amountOfDataInFirstChunk > 0)
        {
            CheckedMemcpy(outData, chunk->data.data() + messageStartOffset, amountOfDataInFirstChunk);
        }
        const u32 amountOfDataInSecondChunk = header->size - amountOfDataInFirstChunk;
        // In rare case it might happen that header is split among 2 packets. We need to read data from second chunk with proper offset.
        const u32 secondChunkOffset = messageStartOffset > CONNECTION_QUEUE_MEMORY_CHUNK_SIZE ? (messageStartOffset % CONNECTION_QUEUE_MEMORY_CHUNK_SIZE) : 0;
        if (amountOfDataInSecondChunk > 0)
        {
            CheckedMemcpy(outData + amountOfDataInFirstChunk, chunk->nextChunk->data.data() + secondChunkOffset, amountOfDataInSecondChunk);
        }
    }

    return header->size;
}

ChunkedPacketQueue::ChunkHeadPair ChunkedPacketQueue::GetChunkHeadPairOfIndex(u16 index) const
{

    if (index >= amountOfPackets)
    {
        SIMEXCEPTION(IllegalArgumentException);
        return ChunkHeadPair{ nullptr, 0 };
    }

    // Search for the chunk and the head
    ConnectionQueueMemoryChunk* currentChunk = readChunk;
    u32 currentHead = readChunk->currentReadHead;
    for (u16 i = 0; i < index; i++)
    {
        const QueueEntryHeader* header = (const QueueEntryHeader*)(currentChunk->data.data() + currentHead);
        const u16 headerSize = header->isExtended ? sizeof(ExtendedQueueEntryHeader) : sizeof(QueueEntryHeader);
        currentHead += headerSize;
        currentHead += header->size;
        currentHead = Utility::NextMultipleOf(currentHead, sizeof(u32));
        if (currentHead >= CONNECTION_QUEUE_MEMORY_CHUNK_SIZE)
        {
            if (currentChunk->nextChunk == nullptr)
            {
                // (Probably) An implementation error! The random access peek reached the
                // end of the chunk linked list, but did not yet reach the searched index.
                // This may also be some MemoryCorruption.
                SIMEXCEPTION(IllegalStateException);
                return ChunkHeadPair{ nullptr, 0 };
            }
            currentChunk = currentChunk->nextChunk;
            currentHead -= CONNECTION_QUEUE_MEMORY_CHUNK_SIZE;
        }
    }

    return ChunkHeadPair{ currentChunk, currentHead };
}

ChunkedPacketQueue::ChunkedPacketQueue()
{
    readChunk = GS->connectionQueueMemoryAllocator.Allocate(true);
    writeChunk = readChunk;
    lookAheadChunk = readChunk;
    if (!readChunk)
    {
        //This must never happen. If it does, it indicates an implementation error.
        //The connectionQueueMemoryAllocator must always have enough chunks to support
        //new connections.
        SIMEXCEPTION(IllegalStateException);
        GS->logger.LogCustomError(CustomErrorTypes::FATAL_NO_CHUNK_FOR_NEW_CONNECTION, 0);

        //Probably some code existed that refused to give chunks back to the allocator.
        //If this happens, the safest thing that we can do is reboot after some time so that the
        //chunks are freed up. The relatively long reboot time of 60 seconds is chosen so that
        //the node can still be safed via manual connects. It makes sure that per reboot cycle
        //we at least have a time window of 60 seconds to send DFU data.
        GS->node.Reboot(SEC_TO_DS(60), RebootReason::NO_CHUNK_FOR_NEW_CONNECTION);

        logt("ERROR", "!!!FATAL!!! A new connection was unable to allocate a queue chunk.");
    }
}

ChunkedPacketQueue::~ChunkedPacketQueue()
{
    while (readChunk)
    {
        ConnectionQueueMemoryChunk* const next = readChunk->nextChunk;
        GS->connectionQueueMemoryAllocator.Deallocate(readChunk);
        readChunk = next;
    }
}

bool ChunkedPacketQueue::SplitAndAddMessage(u8* data, const u16 size, const u16 payloadSizePerSplit, u32 * messageHandle)
{
    if (size > MAX_MESH_PACKET_SIZE + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED)
    {
        SIMEXCEPTION(IllegalArgumentException);
        return false;
    }
    if (data == nullptr)
    {
        SIMEXCEPTION(IllegalArgumentException);
        return false;
    }
    if (size < SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED + 1)
    {
        SIMEXCEPTION(IllegalArgumentException);
        return false;
    }
    if (size <= payloadSizePerSplit + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED)
    {
        return AddMessage(data, size, messageHandle, false);
    }

    const u32 amountOfSplits = Utility::MessageLengthToAmountOfSplitPackets(size, payloadSizePerSplit);
    const u32 sizeInQueueOfStartingSplits = Utility::NextMultipleOf(Utility::NextMultipleOf(payloadSizePerSplit - SIZEOF_CONN_PACKET_SPLIT_HEADER, sizeof(QueueEntryHeader)) + SIZEOF_CONN_PACKET_SPLIT_HEADER, sizeof(QueueEntryHeader)) + sizeof(QueueEntryHeader);
    const u32 sizeInQueueOfLastSplit = Utility::NextMultipleOf(Utility::NextMultipleOf(size - (payloadSizePerSplit - SIZEOF_CONN_PACKET_SPLIT_HEADER) * (amountOfSplits - 1), sizeof(ExtendedQueueEntryHeader)) + SIZEOF_CONN_PACKET_SPLIT_HEADER, sizeof(ExtendedQueueEntryHeader)) + sizeof(ExtendedQueueEntryHeader);
    const u32 sizeInQueue = sizeInQueueOfStartingSplits * (amountOfSplits - 1) + sizeInQueueOfLastSplit;
    const u32 amountOfExtraChunks = (sizeInQueue - (CONNECTION_QUEUE_MEMORY_CHUNK_SIZE - readChunk->amountOfByteInThisChunk)) / CONNECTION_QUEUE_MEMORY_CHUNK_SIZE + 1;
    if (CONNECTION_QUEUE_MEMORY_CHUNK_SIZE - readChunk->amountOfByteInThisChunk < sizeInQueue && GS->connectionQueueMemoryAllocator.IsChunkAvailable(false, amountOfExtraChunks + (u32)prio) == false)
    {
        // If there is no memory left for this message.
        return false;
    }

    DYNAMIC_ARRAY(payloadBuffer, payloadSizePerSplit + SIZEOF_CONN_PACKET_SPLIT_HEADER + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED);
    CheckedMemcpy(payloadBuffer, data, SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED);
    data += SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED;
    ConnPacketSplitHeader* resultHeader = (ConnPacketSplitHeader*)(payloadBuffer + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED);
    u8* payload = payloadBuffer + SIZEOF_CONN_PACKET_SPLIT_HEADER + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED;
    u16 sizeLeft = size - SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED;
    u8 splitCounter = 0;

    while (sizeLeft > 0)
    {
        u16 sizeOfThisSplit = 0;
        bool isSplit = false;
        if (sizeLeft > payloadSizePerSplit - SIZEOF_CONN_PACKET_SPLIT_HEADER)
        {
            resultHeader->splitMessageType = MessageType::SPLIT_WRITE_CMD;
            sizeOfThisSplit = payloadSizePerSplit - SIZEOF_CONN_PACKET_SPLIT_HEADER;
            isSplit = true;
        }
        else
        {
            resultHeader->splitMessageType = MessageType::SPLIT_WRITE_CMD_END;
            sizeOfThisSplit = sizeLeft;
            isSplit = false;
        }
        resultHeader->splitCounter = splitCounter++;
        CheckedMemcpy(payload, data, sizeOfThisSplit);
        data += sizeOfThisSplit;
        sizeLeft -= sizeOfThisSplit;

        const bool successfullyAdded = AddMessage(payloadBuffer, sizeOfThisSplit + SIZEOF_CONN_PACKET_SPLIT_HEADER + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED, messageHandle, isSplit);
        if (!successfullyAdded)
        {
            // Must never happen! A check happened earlier if we are able to allocate enough chunks for the message.
            SIMEXCEPTION(IllegalStateException);
            logt("ERROR", "!!! FATAL !!! No queue space after check!");
            GS->node.Reboot(SEC_TO_DS(60), RebootReason::IMPLEMENTATION_ERROR_NO_QUEUE_SPACE_AFTER_CHECK);
            return false;
        }
    }

    return true;
}

bool ChunkedPacketQueue::AddMessage(u8* data, u16 size, u32 * messageHandle, bool isSplit)
{
    if (size > MAX_MESH_PACKET_SIZE)
    {
        SIMEXCEPTION(IllegalArgumentException);
        return false;
    }
    if (data == nullptr)
    {
        SIMEXCEPTION(IllegalArgumentException);
        return false;
    }

    // The following assumption is because the implementation never splits data over mutliple chunks. Thus, if one
    // chunk is completely full, the message must always be placable in a new chunk (as long as one is available).
    // A "split" in this context means a split across multiple chunks, NOT across multiple packets.
    static_assert(MAX_MESH_PACKET_SIZE + sizeof(ExtendedQueueEntryHeader) <= CONNECTION_QUEUE_MEMORY_CHUNK_SIZE,
        "The implementation of this class assumes that a maximum packet size plus the size of a header always fits in a freshly allocated chunk.");

    if (isSplit)
    {
        const u16 sizeInQueue = Utility::NextMultipleOf(size + sizeof(ExtendedQueueEntryHeader), sizeof(ExtendedQueueEntryHeader));
        if (CONNECTION_QUEUE_MEMORY_CHUNK_SIZE - writeChunk->amountOfByteInThisChunk < sizeInQueue && GS->connectionQueueMemoryAllocator.IsChunkAvailable(false, 1 + (u32)prio) == false)
        {
            // If there is no memory left for this message.
            return false;
        }
        if (messageHandle != nullptr) *messageHandle = 0;
        QueueEntryHeader header;
        CheckedMemset(&header, 0, sizeof(header));
        header.size = size;
        header.isSplit = isSplit;
        AddMessageRaw((u8*)&header, sizeof(header));
        AddMessageRaw(data, size);
        amountOfPackets++;

        if (lookAheadChunk->currentLookAheadHead == CONNECTION_QUEUE_MEMORY_CHUNK_SIZE)
        {
            // Edge case! If we have looked ahead through all the available messages, hit exactly the end of
            // the last chunk and then add a new message, the lookAhead is not moved to the new chunk.
            lookAheadChunk = lookAheadChunk->nextChunk;
        }
    }
    else
    {
        this->messageHandle++;
        const u16 sizeInQueue = Utility::NextMultipleOf(size + sizeof(ExtendedQueueEntryHeader), sizeof(ExtendedQueueEntryHeader));
        if (CONNECTION_QUEUE_MEMORY_CHUNK_SIZE - writeChunk->amountOfByteInThisChunk < sizeInQueue && GS->connectionQueueMemoryAllocator.IsChunkAvailable(false, 1 + (u32)prio) == false)
        {
            // If there is no memory left for this message.
            return false;
        }

        ExtendedQueueEntryHeader header;
        CheckedMemset(&header, 0, sizeof(header));
        header.header.size = size;
        header.header.isSplit = isSplit;
        header.header.isExtended = true;
        header.handle = this->messageHandle;
        if (messageHandle != nullptr) *messageHandle = this->messageHandle;
        AddMessageRaw((u8*)&header, sizeof(header));
        AddMessageRaw(data, size);
        amountOfPackets++;

        if (lookAheadChunk->currentLookAheadHead == CONNECTION_QUEUE_MEMORY_CHUNK_SIZE)
        {
            // Edge case! If we have looked ahead through all the available messages, hit exactly the end of
            // the last chunk and then add a new message, the lookAhead is not moved to the new chunk.
            lookAheadChunk = lookAheadChunk->nextChunk;
        }
        
    }

    return true;
}

u16 ChunkedPacketQueue::PeekPacket(u8* outData, u16 outDataSize, u32* messageHandle) const
{
    if (!HasPackets())
    {
        SIMEXCEPTION(IllegalStateException);
        return 0;
    }
    return PeekPacketRaw(outData, outDataSize, readChunk, readChunk->currentReadHead, messageHandle);
}

u16 ChunkedPacketQueue::RandomAccessPeek(u8* outData, u16 outDataSize, u16 index, u32* messageHandle) const
{
    const ChunkHeadPair pair = GetChunkHeadPairOfIndex(index);
    return PeekPacketRaw(outData, outDataSize, pair.chunk, pair.head, messageHandle);
}

void ChunkedPacketQueue::PopPacket()
{
    if (!HasPackets())
    {
        SIMEXCEPTION(IllegalStateException);
        return;
    }
    const bool needToMoveLookAhead = IsLookAheadAndReadSame(); // If the look ahead is the same as the read, we have to move the look ahead with the read as else the look ahead would point to invalid data.
    const QueueEntryHeader* header = ((QueueEntryHeader*)(readChunk->data.data() + readChunk->currentReadHead));
    const u16 size = header->size;
    const u16 headerSize = header->isExtended ? sizeof(ExtendedQueueEntryHeader) : sizeof(QueueEntryHeader);
    const u16 sizeToPop = size + headerSize;
    const u16 oldReadHead = readChunk->currentReadHead;
    readChunk->currentReadHead += sizeToPop;
    readChunk->currentReadHead = Utility::NextMultipleOf(readChunk->currentReadHead, sizeof(u32));
    if (needToMoveLookAhead) readChunk->currentLookAheadHead = readChunk->currentReadHead;
    if (readChunk->currentReadHead >= CONNECTION_QUEUE_MEMORY_CHUNK_SIZE && readChunk != writeChunk)
    {
        auto oldReadChunk = readChunk;
        readChunk = readChunk->nextChunk;
        if (needToMoveLookAhead) lookAheadChunk = readChunk;
        GS->connectionQueueMemoryAllocator.Deallocate(oldReadChunk);
        const u16 sizeRemovedFromFirstChunk = (CONNECTION_QUEUE_MEMORY_CHUNK_SIZE > oldReadHead ? CONNECTION_QUEUE_MEMORY_CHUNK_SIZE - oldReadHead : 0);
        if (sizeToPop > sizeRemovedFromFirstChunk)
        {
            // If the end of the popped message reaches into the next chunk
            const u16 sizeToPopFromSecondChunk = sizeToPop - sizeRemovedFromFirstChunk;
            readChunk->currentReadHead += sizeToPopFromSecondChunk;
            readChunk->currentReadHead = Utility::NextMultipleOf(readChunk->currentReadHead, sizeof(u32));
            if (needToMoveLookAhead) readChunk->currentLookAheadHead = readChunk->currentReadHead;
        }
    }

    if (!HasPackets())
    {
        // If we have read all the bytes from the current readChunk and this is the only chunk,
        // we can reset the chunk to avoid giving it back to the allocator at some point.
        readChunk->Reset();
    }
    amountOfPackets--;
}

bool ChunkedPacketQueue::HasPackets() const
{
    return readChunk != writeChunk || readChunk->currentReadHead != readChunk->amountOfByteInThisChunk;
}

bool ChunkedPacketQueue::IsCurrentlySendingSplitMessage() const
{
    if (isCurrentlySendingSplitMessage && !HasMoreToLookAhead())
    {
        // Implementation error! If this is happening, we are currently thinking that:
        //    a) We are in the middle of sending splits
        //    b) Have nothing more to look ahead
        // These two assumptions are in direct contradiction. They can never both be true,
        // because (simply put) if we have more to send in a split, then there must be more to send.
        // The most likely cause of this is that a split end wasn't properly queued.
        SIMEXCEPTION(IllegalStateException);
        GS->node.Reboot(SEC_TO_DS(60), RebootReason::IMPLEMENTATION_ERROR_SPLIT_WITH_NO_LOOK_AHEAD);
        logt("ERROR", "!!! FATAL !!! Split without look ahead");
        return false;
    }
    return isCurrentlySendingSplitMessage;
}

bool ChunkedPacketQueue::IsLookAheadAndReadSame() const
{
    return readChunk == lookAheadChunk && readChunk->currentReadHead == readChunk->currentLookAheadHead;
}

bool ChunkedPacketQueue::HasMoreToLookAhead() const
{
    return lookAheadChunk != writeChunk || lookAheadChunk->currentLookAheadHead != lookAheadChunk->amountOfByteInThisChunk;
}

u16 ChunkedPacketQueue::PeekLookAhead(u8* outData, u16 outDataSize) const
{
    if (!HasMoreToLookAhead())
    {
        SIMEXCEPTION(IllegalStateException);
        return 0;
    }

    return PeekPacketRaw(outData, outDataSize, lookAheadChunk, lookAheadChunk->currentLookAheadHead);
}

void ChunkedPacketQueue::IncrementLookAhead()
{
    if (!HasMoreToLookAhead())
    {
        SIMEXCEPTION(IllegalStateException);
        return;
    }
    const QueueEntryHeader* header = ((const QueueEntryHeader*)(lookAheadChunk->data.data() + lookAheadChunk->currentLookAheadHead));
    const u16 headerSize = header->isExtended ? sizeof(ExtendedQueueEntryHeader) : sizeof(QueueEntryHeader);
    isCurrentlySendingSplitMessage = header->isSplit == 1 ? true : false;
    const u16 size = header->size;
    const u16 sizeToJump = size + headerSize;
    const u16 oldReadHead = lookAheadChunk->currentLookAheadHead;
    lookAheadChunk->currentLookAheadHead += sizeToJump;
    lookAheadChunk->currentLookAheadHead = Utility::NextMultipleOf(lookAheadChunk->currentLookAheadHead, sizeof(u32));
    if (lookAheadChunk->currentLookAheadHead >= CONNECTION_QUEUE_MEMORY_CHUNK_SIZE && lookAheadChunk != writeChunk)
    {
        lookAheadChunk = lookAheadChunk->nextChunk;
        const u16 sizeRemovedFromFirstChunk = (CONNECTION_QUEUE_MEMORY_CHUNK_SIZE > oldReadHead ? CONNECTION_QUEUE_MEMORY_CHUNK_SIZE - oldReadHead : 0);
        if (sizeToJump > sizeRemovedFromFirstChunk)
        {
            // If the end of the popped message reaches into the next chunk
            const u16 sizeToPopFromSecondChunk = sizeToJump - sizeRemovedFromFirstChunk;
            lookAheadChunk->currentLookAheadHead += sizeToPopFromSecondChunk;
            lookAheadChunk->currentLookAheadHead = Utility::NextMultipleOf(lookAheadChunk->currentLookAheadHead, sizeof(u32));
        }
    }
}

void ChunkedPacketQueue::RollbackLookAhead()
{
    lookAheadChunk->currentLookAheadHead = lookAheadChunk->currentReadHead;
    auto currentChunk = readChunk;
    while (currentChunk != lookAheadChunk)
    {
        currentChunk->currentLookAheadHead = currentChunk->currentReadHead;
        currentChunk = currentChunk->nextChunk;
    }
    lookAheadChunk = readChunk;

    //We must reevaluate if the queue is currently sending a split packet as this might have changed after the rollback
    //E.g. The node was previously sending a long split message from queue-A where all packets were already queued, so it was done sending a split message
    //Then, queue-B was sending a split message and is now in the middle, so we have queue-A with currentlySendingSplitPacket=false and queue-B with true
    //Now, only two packets of the split message were ACKed by the SoftDevice and a RECONNECT is done
    //After the Rollback, it is essential that queue-A is starting sending instead of queue-B as it is in the middle of sending a split packet
    if (GetAmountOfPackets() > 0) {
        const QueueEntryHeader* header = ((const QueueEntryHeader*)(lookAheadChunk->data.data() + lookAheadChunk->currentLookAheadHead));
        isCurrentlySendingSplitMessage = header->isSplit == 1 ? true : false;
    }
}

bool ChunkedPacketQueue::IsRandomAccessIndexLookedAhead(u16 index) const
{
    const ChunkHeadPair pair = GetChunkHeadPairOfIndex(index);

    if (pair.chunk == lookAheadChunk)
    {
        return pair.head < lookAheadChunk->currentLookAheadHead;
    }

    ConnectionQueueMemoryChunk* currentChunk = readChunk;
    while (currentChunk != lookAheadChunk)
    {
        if (currentChunk == pair.chunk) return true;
        currentChunk = currentChunk->nextChunk;
    }

    return false;
}

u32 ChunkedPacketQueue::GetAmountOfPackets() const
{
    return amountOfPackets;
}

void ChunkedPacketQueue::Print() const
{
    trace("Amount of Packets: %u" EOL, GetAmountOfPackets());

    if (GetAmountOfPackets() > 0)
    {
        static_assert(SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED == 3, "The following helper only makes sense with this size. Adjust it if you change it.");
        static_assert(SIZEOF_CONN_PACKET_HEADER == 5,               "The following helper only makes sense with this size. Adjust it if you change it.");
        trace("     |SendPckd|ConnPacketHdr |" EOL);
    }

    for (u32 i = 0; i < GetAmountOfPackets(); i++)
    {
        u8 buffer[MAX_MESH_PACKET_SIZE];
        const u16 size = RandomAccessPeek(buffer, sizeof(buffer), i);
        if (size == 0)
        {
            trace("#%03u:|Failed to read" EOL, i);
        }
        else
        {
            char printBuffer[MAX_MESH_PACKET_SIZE];
            CheckedMemset(printBuffer, 0, sizeof(printBuffer));
            Logger::ConvertBufferToHexString(buffer, size, printBuffer, sizeof(printBuffer));
            static_assert(SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED == 3, "The following helper only makes sense with this size. Adjust it if you change it.");
            static_assert(SIZEOF_CONN_PACKET_HEADER == 5, "The following helper only makes sense with this size. Adjust it if you change it.");
            printBuffer[8] = '\0';
            trace("#%03u:|%s", i, printBuffer);
            printBuffer[8] = '|';
            printBuffer[23] = '\0';
            trace("%s", (printBuffer + 8));
            printBuffer[23] = '|';
            trace("%s" EOL, (printBuffer + 23));
        }

    }
}

DeliveryPriority ChunkedPacketQueue::GetPriority() const
{
    return prio;
}

void ChunkedPacketQueue::SetPriority(DeliveryPriority prio)
{
    this->prio = prio;
}

#ifdef SIM_ENABLED
void ChunkedPacketQueue::SimReset()
{
    auto currentChunk = readChunk;
    while (currentChunk)
    {
        auto nextChunk = currentChunk->nextChunk;
        GS->connectionQueueMemoryAllocator.Deallocate(currentChunk);
        currentChunk = nextChunk;
    }

    readChunk = GS->connectionQueueMemoryAllocator.Allocate(true);
    writeChunk = readChunk;
    lookAheadChunk = readChunk;
}
#endif
