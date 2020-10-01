////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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

#include "TimeManager.h"
#include "mini-printf.h"
#include "GlobalState.h"
#include "Node.h"
#include "Utility.h"

TimeManager::TimeManager()
{
    syncTime = 0;
    timeSinceSyncTime = 0;
}

u32 TimeManager::GetTime()
{
    ProcessTicks();
    u32 unixTime = syncTime + timeSinceSyncTime;
    i32 offsetSeconds = offset * 60;
#if IS_INACTIVE(CLC_GW_SAVE_SPACE)
    if (offsetSeconds < 0 && unixTime < static_cast<u32>(-offsetSeconds))
    {
        //Edge case.
        return unixTime;
    }
    else
#endif
    {
        return static_cast<u32>(unixTime + offsetSeconds);
    }
}

TimePoint TimeManager::GetTimePoint()
{
    ProcessTicks();
    return TimePoint(GetTime(), additionalTicks);
}

void TimeManager::SetTime(u32 syncTimeDs, u32 timeSinceSyncTimeDs, i16 offset, u32 additionalTicks)
{
    this->syncTime = syncTimeDs;
    this->timeSinceSyncTime = timeSinceSyncTimeDs;
    this->additionalTicks = additionalTicks;
    this->offset = offset;
    this->counter++;
    this->waitingForCorrection = false;
    this->timeCorrectionReceived = true;

    //We inform the connection manager so that it resends the time sync messages.
    logt("TSYNC", "Received time by command! NodeId: %u", (u32)GS->node.configuration.nodeId);
    GS->cm.ResetTimeSync();
}

void TimeManager::SetTime(const TimeSyncInitial & timeSyncIntitialMessage)
{
    if (timeSyncIntitialMessage.counter > this->counter)
    {
        this->syncTime = timeSyncIntitialMessage.syncTimeStamp;
        this->timeSinceSyncTime = timeSyncIntitialMessage.timeSincSyncTimeStamp;
        this->additionalTicks = timeSyncIntitialMessage.additionalTicks;
        this->offset = timeSyncIntitialMessage.offset;
        this->counter = timeSyncIntitialMessage.counter; //THIS is the main difference to SetTime(u32,u32,u32)!
        this->waitingForCorrection = true;
        this->timeCorrectionReceived = false;

        //We inform the connection manager so that it resends the time sync messages.
        logt("TSYNC", "Received time by mesh! NodeId: %u, Partner: %u", (u32)GS->node.configuration.nodeId, (u32)timeSyncIntitialMessage.header.header.sender);
        GS->cm.ResetTimeSync();
    }
}

void TimeManager::SetTime(const TimeSyncInterNetwork& timeSyncInterNetwork)
{
    if (this->counter == 0 || GET_DEVICE_TYPE() == DeviceType::ASSET)
    {
        this->syncTime = timeSyncInterNetwork.syncTimeStamp;
        this->timeSinceSyncTime = timeSyncInterNetwork.timeSincSyncTimeStamp;
        this->additionalTicks = timeSyncInterNetwork.additionalTicks;
        this->offset = timeSyncInterNetwork.offset;
        this->counter++;
        this->waitingForCorrection = false;
        this->timeCorrectionReceived = false;

        //We inform the connection manager so that it resends the time sync messages.
        logt("TSYNC", "Received time by inter mesh! NodeId: %u, Partner: %u", (u32)GS->node.configuration.nodeId, (u32)timeSyncInterNetwork.header.header.sender);
        GS->cm.ResetTimeSync();
    }
}

bool TimeManager::IsTimeSynced() const
{
    return syncTime != 0;
}

bool TimeManager::IsTimeCorrected() const
{
    return (timeCorrectionReceived || !waitingForCorrection) && IsTimeSynced();
}

void TimeManager::AddTicks(u32 ticks)
{
    additionalTicks += ticks;
}

void TimeManager::AddCorrection(u32 ticks)
{
    if (waitingForCorrection)
    {
        AddTicks(ticks);
        this->waitingForCorrection = false;
        this->timeCorrectionReceived = true;

        logt("TSYNC", "Time synced and corrected");
    }
}

void TimeManager::ProcessTicks()
{
    u32 seconds = additionalTicks / ticksPerSecond;
    timeSinceSyncTime += seconds;

    additionalTicks -= seconds * ticksPerSecond;
}

void TimeManager::HandleUpdateTimestampMessages(ConnPacketHeader const * packetHeader, u16 dataLength)
{
    if (packetHeader->messageType == MessageType::UPDATE_TIMESTAMP)
    {
        //Set our time to the received timestamp
        connPacketUpdateTimestamp const * packet = (connPacketUpdateTimestamp const *)packetHeader;
        if (dataLength >= offsetof(connPacketUpdateTimestamp, offset) + sizeof(packet->offset))
        {
            SetTime(packet->timestampSec, 0, packet->offset);
        }
        else
        {
            SetTime(packet->timestampSec, 0, 0);
        }
    }
}

void TimeManager::convertTimestampToString(char * buffer)
{
    ProcessTicks();
    u32 gapDays;
    u32 remainingSeconds = GetTime();

    u32 yearDivider = 60 * 60 * 24 * 365;
    u16 years = remainingSeconds / yearDivider + 1970;
    remainingSeconds = remainingSeconds % yearDivider;

    gapDays = (years - 1970) / 4 - 1;
    u32 dayDivider = 60 * 60 * 24;
    u16 days = remainingSeconds / dayDivider;
    days -= gapDays;
    remainingSeconds = remainingSeconds % dayDivider;

    u32 hourDivider = 60 * 60;
    u16 hours = remainingSeconds / hourDivider;
    remainingSeconds = remainingSeconds % hourDivider;

    u32 minuteDivider = 60;
    u16 minutes = remainingSeconds / minuteDivider;
    remainingSeconds = remainingSeconds % minuteDivider;

    u32 seconds = remainingSeconds;

    snprintf(buffer, 80, "approx. %u years, %u days, %02uh:%02um:%02us,%u ticks", years, days, hours, minutes, seconds, this->additionalTicks);
}

TimeSyncInitial TimeManager::GetTimeSyncIntialMessage(NodeId receiver) const
{
    TimeSyncInitial retVal;
    CheckedMemset(&retVal, 0, sizeof(retVal));
    
    retVal.header.header.messageType = MessageType::TIME_SYNC;
    retVal.header.header.receiver = receiver;
    retVal.header.header.sender = GS->node.configuration.nodeId;
    retVal.header.type = TimeSyncType::INITIAL;

    retVal.syncTimeStamp = syncTime;
    retVal.timeSincSyncTimeStamp = timeSinceSyncTime;
    retVal.additionalTicks = additionalTicks;
    retVal.offset = offset;
    retVal.counter = counter;
    
    return retVal;
}

TimeSyncInterNetwork TimeManager::GetTimeSyncInterNetworkMessage(NodeId receiver) const
{
    TimeSyncInterNetwork retVal;
    CheckedMemset(&retVal, 0, sizeof(retVal));

    retVal.header.header.messageType = MessageType::TIME_SYNC;
    retVal.header.header.receiver = receiver;
    retVal.header.header.sender = GS->node.configuration.nodeId;
    retVal.header.type = TimeSyncType::INTER_NETWORK;

    retVal.syncTimeStamp = syncTime;
    retVal.timeSincSyncTimeStamp = timeSinceSyncTime;
    retVal.additionalTicks = additionalTicks;
    retVal.offset = offset;

    return retVal;
}

TimePoint::TimePoint(u32 unixTime, u32 additionalTicks)
    :unixTime(unixTime), additionalTicks(additionalTicks)
{
}

TimePoint::TimePoint()
    : unixTime(0), additionalTicks(0)
{
}

i32 TimePoint::operator-(const TimePoint & other)
{
    const i32 secondDifference = this->unixTime - other.unixTime;
    const i32 ticksDifference = this->additionalTicks - other.additionalTicks;
    
    return ticksDifference + secondDifference * ticksPerSecond;
}

u32 TimePoint::GetAdditionalTicks() const
{
    return additionalTicks;
}
