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

#include <FmTypes.h>

/*
 * The packet queue implements a circular buffer for sending packets of varying
 * sizes.
 */
class PacketQueue
{
private: 


public:
    //really public
    PacketQueue();
    PacketQueue(u32* buffer, u16 bufferLength);
    u8* Reserve(u16 dataLength);
    bool Put(u8* data, u16 dataLength);
    SizedData PeekNext() const;
    SizedData PeekNext(u8 pos) const;
    void DiscardNext();
    SizedData PeekLast();
    void DiscardLast();
    void Clean(void);

    void Print() const;

    u8 packetSendPosition = 0; //Is used to note the position in messages that consist of multiple parts
    u8 packetSentRemaining = 0; //Is used to check how many have not yet been sent of the ones that have been queued
    u8 packetFailedToQueueCounter = 0; //Used to store the number of time the packet failed to send

    //private
    u8* bufferStart = nullptr;
    u8* bufferEnd = nullptr;
    u16 bufferLength = 0;

    u8* readPointer = nullptr;
    u8* writePointer = nullptr;

    u16 _numElements = 0;
    
    u16 numUnsentElements = 0; //Used for marking some packets as already sent (queued in the softdevice)
};


