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

#include "TimeManager.h"
#include "mini-printf.h"
#include "GlobalState.h"
#include "Node.h"
#include "Utility.h"

/*
Known Limitation: Starting a timesync on multiple nodes around the same time with different times will
not sync the same time to all nodes. Doing another sync on one node will however correct this.
The reason is that both nodes will generate the same counter value and therefore, there will be no winner.
*/

TimeManager::TimeManager()
{
    syncTime = 0;
    timeSinceSyncTime = 0;
}

u32 TimeManager::GetUtcTime()
{
    ProcessTicks();
    u32 unixTime = syncTime + timeSinceSyncTime;

    return unixTime;
}

u32 TimeManager::GetLocalTime()
{
    ProcessTicks();
    u32 unixTime = syncTime + timeSinceSyncTime;
    i32 offsetSeconds = offset * 60;
    if (offsetSeconds < 0 && unixTime < static_cast<u32>(-offsetSeconds))
    {
        //Edge case.
        return unixTime;
    }
    else
    {
        return static_cast<u32>(unixTime + offsetSeconds);
    }
}

i16 TimeManager::GetOffset()
{
    return offset;
}

bool TimeManager::IsTimeMaster()
{
    return isTimeMaster;
}

TimePoint TimeManager::GetLocalTimePoint()
{
    ProcessTicks();
    return TimePoint(GetLocalTime(), additionalTicks);
}

void TimeManager::SetMasterTime(u32 syncTimeDs, u32 timeSinceSyncTimeDs, i16 offset, u32 additionalTicks)
{
    this->syncTime = syncTimeDs;
    this->timeSinceSyncTime = timeSinceSyncTimeDs;
    this->additionalTicks = additionalTicks;
    this->offset = offset;
    this->counter++;
    this->waitingForCorrection = false;
    this->timeCorrectionReceived = true;

    this->isTimeMaster = true;

    //We inform the connection manager so that it resends the time sync messages.
    logt("TSYNC", "Received time by command! NodeId: %u", (u32)GS->node.configuration.nodeId);
    GS->cm.ResetTimeSync();
}

void TimeManager::SetTime(const TimeSyncInitial & timeSyncInitialMessage)
{
    if (timeSyncInitialMessage.counter > this->counter)
    {
        this->syncTime = timeSyncInitialMessage.syncTimeStamp;
        this->timeSinceSyncTime = timeSyncInitialMessage.timeSincSyncTimeStamp;
        this->additionalTicks = timeSyncInitialMessage.additionalTicks;
        this->offset = timeSyncInitialMessage.offset;
        this->counter = timeSyncInitialMessage.counter; //THIS is the main difference to SetTime(u32,u32,u32)!
        this->waitingForCorrection = true;
        this->timeCorrectionReceived = false;

        this->isTimeMaster = false;

        //We inform the connection manager so that it resends the time sync messages.
        logt("TSYNC", "Received time by mesh! NodeId: %u, Partner: %u", (u32)GS->node.configuration.nodeId, (u32)timeSyncInitialMessage.header.header.sender);
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

        this->isTimeMaster = false;

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

        if(timeSyncedListener)
        {
            timeSyncedListener->TimeSyncedHandler();
        }

        logt("TSYNC", "Time synced and corrected");
    }
}

void TimeManager::ProcessTicks()
{
    u32 seconds = additionalTicks / ticksPerSecond;
    timeSinceSyncTime += seconds;

    additionalTicks -= seconds * ticksPerSecond;
}

void TimeManager::HandleUpdateTimestampMessages(ConnPacketHeader const * packetHeader, MessageLength dataLength)
{
    if (packetHeader->messageType == MessageType::UPDATE_TIMESTAMP)
    {
        //Set our time to the received timestamp
        connPacketUpdateTimestamp const * packet = (connPacketUpdateTimestamp const *)packetHeader;
        if (dataLength >= offsetof(connPacketUpdateTimestamp, offset) + sizeof(packet->offset))
        {
            SetMasterTime(packet->timestampSec, 0, packet->offset);
        }
        else
        {
            SetMasterTime(packet->timestampSec, 0, 0);
        }
    }
}

void TimeManager::ConvertTimeToString(char* buffer, u16 bufferSize)
{
    ProcessTicks();
    TimeManager::ConvertTimeToString(GetUtcTime(), GetOffset(), additionalTicks, buffer, bufferSize);
}

void TimeManager::ConvertTimeToString(u32 unixTimestamp, i16 offset, u32 ticks, char* buffer, u16 bufferSize)
{
    u32 localTime = unixTimestamp;
    i32 offsetSeconds = offset * 60;
    if (offsetSeconds < 0 && localTime < static_cast<u32>(-offsetSeconds))
    {
        //Edge case.
        snprintf(buffer, bufferSize, "Negative Offset (%d) smaller than timestamp (%u)", offset, unixTimestamp);
        return;
    }
    else
    {
        localTime = unixTimestamp + offsetSeconds;
    }


    u32 remainingSeconds = localTime;

    u32 yearDivider = 60 * 60 * 24 * 365;
    u16 years = remainingSeconds / yearDivider + 1970;
    remainingSeconds = remainingSeconds % yearDivider;

    u32 gapDays = (years - 1970) / 4 - 1;
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

    snprintf(buffer, bufferSize, "approx. %u years, %u days, %02uh:%02um:%02us,%u ticks (offset %d)", years, days, hours, minutes, seconds, ticks, offset);
}

TimeSyncInitial TimeManager::GetTimeSyncInitialMessage(NodeId receiver) const
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

void TimeManager::AddTimeSyncedListener(TimeSyncedListener* listener)
{
    if (timeSyncedListener)
    {
        // The current implementation only allows a single Listener. If more
        // is needed, change timeSyncedListener to be an array instead.
        SIMEXCEPTION(IllegalStateException);
    }
    timeSyncedListener = listener;
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
