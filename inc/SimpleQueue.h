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
 * This Queue can work with variable length data, it is only slightly different
 * from the packet queue, but has been copied because the packet queue
 * has to work with reliable / unreliable packets which offsets the storage
 */



#pragma once

#include <types.h>

extern "C" {
}

class SimpleQueue
{
private: 


public:
//private.....
	  u8* bufferStart;
	  u8* bufferEnd;
		u16 bufferLength;

		u8* readPointer;
		u8* writePointer;
//really public
    SimpleQueue(u8* buffer, u32 bufferLength);
    bool Put(u8* data, u32 dataLength);
	sizedData GetNext();
	sizedData PeekNext();
	void DiscardNext();
	void Clean(void);

	u16 _numElements;
};

