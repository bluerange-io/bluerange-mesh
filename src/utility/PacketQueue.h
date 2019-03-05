////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2019 M-Way Solutions GmbH
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

/*
 * The packet queue implements a circular buffer for sending packets of varying
 * sizes.
 */

#pragma once

#include <types.h>

extern "C" {
#include <nrf_soc.h>
}

class PacketQueue
{
private: 


public:
	//really public
	PacketQueue(u32* buffer, u16 bufferLength);
	u8* Reserve(u16 dataLength);
    bool Put(u8* data, u16 dataLength);
	sizedData PeekNext() const;
	sizedData PeekNext(u8 pos) const;
	void DiscardNext();
	sizedData PeekLast();
	void DiscardLast();
	void Clean(void);

	void Print() const;

	u8 packetSendPosition; //Is used to note the position in messages that consist of multiple parts
	u8 packetSentRemaining; //Is used to check how many have not yet been sent of the ones that have been queued
	u8 packetFailedToQueueCounter; //Used to store the number of time the packet failed to send

	//private
	u8* bufferStart;
	u8* bufferEnd;
	u16 bufferLength;

	u8* readPointer;
	u8* writePointer;

	u16 _numElements;
	
	u16 numUnsentElements; //Used for marking some packets as already sent (queued in the softdevice)
};


