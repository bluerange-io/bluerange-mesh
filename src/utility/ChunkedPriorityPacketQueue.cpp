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

#include "ChunkedPriorityPacketQueue.h"
#include "BaseConnection.h"
#include "Utility.h"

QueuePriorityPair ChunkedPriorityPacketQueue::GetSplitQueue()
{
    QueuePriorityPair retVal;
    CheckedMemset(&retVal, 0, sizeof(retVal));
    retVal.priority = DeliveryPriority::INVALID;
    retVal.queue = nullptr;
    for (u32 i = 0; i < queues.size(); i++)
    {
        if (queues[i].IsCurrentlySendingSplitMessage())
        {
            retVal.priority = (DeliveryPriority)i;
            retVal.queue = &queues[i];
            break;
        }
    }
    return retVal;
}

QueuePriorityPairConst ChunkedPriorityPacketQueue::GetSplitQueue() const
{
    QueuePriorityPairConst retVal;
    CheckedMemset(&retVal, 0, sizeof(retVal));
    retVal.priority = DeliveryPriority::INVALID;
    retVal.queue = nullptr;
    for (u32 i = 0; i < queues.size(); i++)
    {
        if (queues[i].IsCurrentlySendingSplitMessage())
        {
            retVal.priority = (DeliveryPriority)i;
            retVal.queue = &queues[i];
            break;
        }
    }
    return retVal;
}

ChunkedPriorityPacketQueue::ChunkedPriorityPacketQueue()
{
    for (u32 i = 0; i < queues.size(); i++)
    {
        queues[i].SetPriority((DeliveryPriority)i);
    }
}

bool ChunkedPriorityPacketQueue::SplitAndAddMessage(DeliveryPriority prio, u8* data, u16 size, u16 payloadSizePerSplit, u32* messageHandle)
{
    if ((u32)prio >= AMOUNT_OF_SEND_QUEUE_PRIORITIES)
    {
        SIMEXCEPTION(IllegalArgumentException);
    }

    constexpr u32 MAX_VITAL_SIZE = 20 + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED;

    if (prio == DeliveryPriority::VITAL && size <= MAX_VITAL_SIZE)
    {
        return queues[(u32)DeliveryPriority::VITAL].AddMessage(data, size, messageHandle, false);
    }
    else
    {
        if (prio == DeliveryPriority::VITAL)
        {
            // A vital message had to be queued with a lower queue because it exceeded the allowed max size
            // This max size exists because the vital queue is never allowed to split.
            //TODO: Currently we do not allow message splitting in Vital Queue.
            //      The current implementation could maybe already allow this,
            //      if this check here is removed and everything is queued with
            //      SplitAndAddMessage. This however needs to be checked.
            SIMEXCEPTION(IllegalArgumentException);
            prio = DeliveryPriority::HIGH;
            logt("FATAL", "Vital queue message had to be queued with high prio queue because it was too large!");
        }
        return queues[(u32)prio].SplitAndAddMessage(data, size, payloadSizePerSplit, messageHandle);
    }
}

u32 ChunkedPriorityPacketQueue::GetAmountOfPackets() const
{
    u32 retVal = 0;
    for (u32 i = 0; i < queues.size(); i++)
    {
        retVal += queues[i].GetAmountOfPackets();
    }
    return retVal;
}

bool ChunkedPriorityPacketQueue::IsCurrentlySendingSplitMessage() const
{
    return GetSplitQueue().queue != nullptr;
}

QueuePriorityPair ChunkedPriorityPacketQueue::GetSendQueue()
{
    // If we have a queue that is currently sending a split, it trumps
    // all priority levels.
    QueuePriorityPair retVal = GetSplitQueue();
    if (retVal.queue) return retVal;

    // Next we check if the vital queue has some data. It bypasses
    // priority droplets.
    if (queues[(u32)DeliveryPriority::VITAL].HasMoreToLookAhead())
    {
        retVal.priority = DeliveryPriority::VITAL;
        retVal.queue = &queues[(u32)DeliveryPriority::VITAL];
        return retVal;
    }

    //We have to iterate twice in case every queue has a priority droplet overflow.
    //In such a case, all droplets are removed and we start again from the top.
    for (u32 repeat = 0; repeat < 2; repeat++)
    {
        for (u32 i = 1; i < queues.size(); i++)
        {
            if (queues[i].HasMoreToLookAhead())
            {
                priorityDroplets[i]++;
                if (priorityDroplets[i] > AMOUNT_OF_PRIORITY_DROPLETS_UNTIL_OVERFLOW)
                {
                    priorityDroplets[i] = 0;
                }
                else
                {
                    retVal.priority = (DeliveryPriority)i;
                    retVal.queue = &queues[i];
                    return retVal;
                }
            }
        }
    }
    return retVal;
}

ChunkedPacketQueue* ChunkedPriorityPacketQueue::GetQueueByPriority(DeliveryPriority prio)
{
    if ((u32)prio >= AMOUNT_OF_SEND_QUEUE_PRIORITIES)
    {
        SIMEXCEPTION(IllegalArgumentException);
    }
    return &queues[(u32)prio];
}

void ChunkedPriorityPacketQueue::RollbackLookAhead()
{
    for (u32 i = 0; i < queues.size(); i++)
    {
        queues[i].RollbackLookAhead();
    }
}
