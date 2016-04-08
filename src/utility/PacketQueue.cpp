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

#include <PacketQueue.h>
#include <Logger.h>

extern "C"
{
#include <nrf_error.h>
#include <cstring>
}

PacketQueue::PacketQueue(u8* buffer, u16 bufferLength)
{
	this->_numElements = 0;
	this->bufferStart = buffer;
	this->bufferEnd = buffer + bufferLength - 1; //FIXME: workaround to avoid 1byte overflow of the packet queue
	this->bufferLength = bufferLength - 1; // s.o.

	this->readPointer = this->bufferStart;
	this->writePointer = this->bufferStart;

	writePointer[0] = 0;

	memset(buffer, 0, bufferLength);
}

//IF READ AND WRITE ARE EQUAL, THE QUEUE IS EMPTY

//Put does only allow data sizes up to 200 byte per element
bool PacketQueue::Put(u8* data, u8 dataLength, bool reliable)
{
	//Keep one byte for (un)reliable flag, one byte for sizeField and one byte to not let read and write pointers overlap
	u8 elementSize = dataLength + 1 + 1 + 1;

	//If the writePointer is ahead (or at the same point) of the read pointer && bufferSpace
	//at the end is not enough && dataSize at the beginning is enough
	if (writePointer >= readPointer && bufferEnd - writePointer <= elementSize && readPointer - bufferStart >= elementSize)
	{
		writePointer = bufferStart;
		writePointer[0] = 0;
	}

	//Check if Buffer can hold the item
	else if (readPointer <= writePointer && writePointer + elementSize >= bufferEnd)
	{
		return false;
	}
	else if (readPointer > writePointer && writePointer + elementSize >= readPointer)
	{
		return false;
	}

	this->writePointer[0] = dataLength+1; //+1 for the reliable flag
	this->writePointer[1] = reliable ? 1 : 0;
	memcpy(this->writePointer + 2, data, dataLength);

	this->writePointer += dataLength + 2;
	//Set length to 0 for next datafield
	this->writePointer[0] = 0;

	_numElements++;

	return true;
}

sizedData PacketQueue::PeekNext()
{
	sizedData data;
	//If queue has been fully read, return empty data
	if (_numElements == 0)
	{
		data.length = 0;

		return data;
	}

	//Check if we reached the end and wrap
	if (readPointer[0] == 0 && writePointer < readPointer)
		readPointer = bufferStart;

	data.length = (this->readPointer[0]);
	data.data = this->readPointer + 1;

	return data;
}

void PacketQueue::DiscardNext()
{
	if(_numElements < 1) return;
	this->readPointer += (this->readPointer[0]+1);
	_numElements--;
}

void PacketQueue::Clean(void)
{
	_numElements = 0;
	this->readPointer = this->bufferStart;
	this->writePointer = this->bufferStart;
	this->writePointer[0] = 0;
}

/* EOF */
