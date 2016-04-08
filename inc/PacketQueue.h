/**

Copyright (c) 2014-2015 "M-Way Solutions GmbH"
FruityMesh - Bluetooth Low Energy mesh protocol [http://mwaysolutions.com/]

This file is part of FruityMesh

FruityMesh is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

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
	PacketQueue(u8* buffer, u16 bufferLength);
    bool Put(u8* data, u8 dataLength, bool reliable);
	sizedData PeekNext();
	void DiscardNext();
	void Clean(void);

	//private

	u8* bufferStart;
	u8* bufferEnd;
	u16 bufferLength;

	u8* readPointer;
	u8* writePointer;


	u16 _numElements;
};


