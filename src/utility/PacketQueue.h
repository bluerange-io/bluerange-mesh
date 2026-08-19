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
    bool Put(const u8* data, u16 dataLength);
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
