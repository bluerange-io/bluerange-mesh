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
#pragma once

#include "FmTypes.h"
constexpr static u32 ticksPerSecond = 32768; // The amount of ticks it takes for one second to pass. Platform dependent!

class TimePoint {
private:
    u32 unixTime;
    u32 additionalTicks;
public:
    TimePoint(u32 unixTime, u32 additionalTicks);
    TimePoint();

    //The difference between two TimePoints, in ticks 
    i32 operator-(const TimePoint& other);
    TimePoint& operator=(const TimePoint& other) = default;

    u32 GetAdditionalTicks() const;
};


/*
 * The TimeManager is responsible for synchronizing times beetween different
 * nodes in the network.
 */
class TimeManager {
private:
    u32 syncTime = 0; // The sync time is a timestamp that describes since when the time is synced and progragated via the mesh.
                      // Note: This is NOT the timestamp when the node was synced but the mesh!
    u32 timeSinceSyncTime = 0;
    u32 additionalTicks = 0; // Time that was impossible to store in timeSinceSyncTime, because it is too short (less than one 
                             // second). Unit is platform dependent!

    u32 counter = 0; // Gets incremented every time new data is available. Makes sure that we are always able to set the time, even backwards.

    i32 offset = 0; // Time Offset in minutes, can be used for time zones

    bool waitingForCorrection = false;
    bool timeCorrectionReceived = false;

public:
    TimeManager();

    u32 GetTime();
    TimePoint GetTimePoint();
    void SetTime(u32 syncTime, u32 timeSinceSyncTime, i16 offset, u32 additionalTicks = 0);
    void SetTime(const TimeSyncInitial& timeSyncIntitialMessage);
    void SetTime(const TimeSyncInterNetwork& timeSyncInterNetwork);
    bool IsTimeSynced() const;
    bool IsTimeCorrected() const;

    void AddTicks(u32 ticks);
    void AddCorrection(u32 ticks);
    void ProcessTicks();
    
    void HandleUpdateTimestampMessages(ConnPacketHeader const * packetHeader, u16 dataLength);

    //Trivial implementation for converting the timestamp in human readable format
    //This does not pay respect to any leap seconds, gap years, whatever
    void convertTimestampToString(char* buffer);

    TimeSyncInitial GetTimeSyncIntialMessage(NodeId receiver) const;
    TimeSyncInterNetwork GetTimeSyncInterNetworkMessage(NodeId receiver) const;
};
