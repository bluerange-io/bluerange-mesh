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

#include <array>
#include "FmTypes.h"
#include "ChunkedPacketQueue.h"

struct QueuePriorityPair
{
    ChunkedPacketQueue* queue;
    DeliveryPriority priority;
};

struct QueuePriorityPairConst
{
    const ChunkedPacketQueue* queue;
    DeliveryPriority priority;
};

constexpr u32 AMOUNT_OF_PRIORITY_DROPLETS_UNTIL_OVERFLOW = 2;
static_assert(AMOUNT_OF_PRIORITY_DROPLETS_UNTIL_OVERFLOW > 0, "Must be at least 1, else we always overflow and never send.");

class ChunkedPriorityPacketQueue
{
    //See Quality of Service documentation.
private:
    std::array<ChunkedPacketQueue, AMOUNT_OF_SEND_QUEUE_PRIORITIES> queues = {};
    std::array<u32,                AMOUNT_OF_SEND_QUEUE_PRIORITIES> priorityDroplets = {};

    QueuePriorityPair GetSplitQueue();
    QueuePriorityPairConst GetSplitQueue() const;

public:
    ChunkedPriorityPacketQueue();

    bool SplitAndAddMessage(DeliveryPriority prio, u8* data, u16 size, u16 payloadSizePerSplit, u32* messageHandle);
    u32 GetAmountOfPackets() const;
    bool IsCurrentlySendingSplitMessage() const;
    QueuePriorityPair GetSendQueue();
    ChunkedPacketQueue* GetQueueByPriority(DeliveryPriority prio);
    void RollbackLookAhead();
};


