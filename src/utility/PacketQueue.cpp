/**

Copyright (c) 2014-2017 "M-Way Solutions GmbH"
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

//Data will be 4-byte aligned if all inputs are 4 byte aligned
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
//Put does only allow data sizes up to 250 byte per element
bool PacketQueue::Put(u8* data, u8 dataLength)
{
	u8* dest = Reserve(dataLength);

	if(dest != NULL){
		memcpy(dest, data, dataLength);
		return true;
	} else {
		return false;
	}
}

u8* PacketQueue::Reserve(u8 dataLength)
{
	if (dataLength == 0 || dataLength > 245) return NULL;

	//Padding makes sure that we only save 4-byte aligned data
	u8 padding = (4-dataLength%4)%4;

	//Keep 4 byte for sizeField and one byte to not let read and write pointers overlap
	u8 elementSize = dataLength + 4 + 1 + padding;

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
		logt("PQ", "No space for %u bytes", dataLength);
		return NULL;
	}
	else if (readPointer > writePointer && writePointer + elementSize >= readPointer)
	{
		logt("PQ", "No space for %u bytes", dataLength);
		return NULL;
	}

	this->writePointer[0] = dataLength;
	u8* dataPointer = this->writePointer + 4; //jump over length field

	//logt("ERROR", "Put %u at %u", dataLength, dataPointer);

	//Move write Pointer to next field
	this->writePointer += dataLength + 4 + padding; //4 byte length

	//Set length to 0 for next datafield
	this->writePointer[0] = 0;

	_numElements++;

	logt("PQ", "Reserve %u bytes, now %u elements", dataLength, _numElements);

	return dataPointer;
}

sizedData PacketQueue::PeekNext()
{
	sizedData data;
	//If queue has been fully read, return empty data
	if (_numElements == 0){
		data.length = 0;
		return data;
	}

	//Check if we reached the end and wrap
	if (readPointer[0] == 0 && writePointer < readPointer)
		readPointer = bufferStart;

	data.length = (this->readPointer[0]);
	data.data = this->readPointer + 4; // 4 byte added for length field
	
	return data;
}

void PacketQueue::DiscardNext()
{
	if (_numElements == 0) return;

	//Check if we reached the end and wrap
	if (readPointer[0] == 0 && writePointer < readPointer) {
		readPointer = bufferStart;
	}
	
	//Padding makes sure that we only save 4-byte aligned data
	u8 padding = (4-this->readPointer[0]%4)%4;
	this->readPointer += (this->readPointer[0] + 4 + padding); //4 byte length
	_numElements--;

	//Reset the pointers to buffer start if the queue is empty
	if (readPointer == writePointer) {
		readPointer = writePointer = bufferStart;
	}

	//Check if we reached the end of entries and the next entry following is an empty one, if yes place read pointer at start
	if (readPointer[0] == 0) {
		readPointer = bufferStart;
	}

	logt("PQ", "DiscardNext, now %u elements", _numElements);
}

//Iterates over all items in order to find the last element
sizedData PacketQueue::PeekLast()
{
	sizedData data;

	//Return 0 length data in case we have no elements
	if (_numElements == 0){
		data.data = NULL;
		data.length = 0;
		return data;
	}

	u16 virtualElementsLeft = _numElements;
	u8* virtualReadPointer = readPointer;

	//Iterate over the queue using a copy of read pointer and numElements until reaching the last element
	while (virtualElementsLeft > 0)
	{
		//Check if we reached the end and wrap
		if (virtualReadPointer[0] == 0 && writePointer < virtualReadPointer)
			virtualReadPointer = bufferStart;

		//Copy data reference into sizeData
		data.length = (virtualReadPointer[0]);
		data.data = virtualReadPointer + 4; // 4 byte added for length field

		//Do a virtual discard
		//Padding makes sure that we only save 4-byte aligned data
		u8 padding = (4-virtualReadPointer[0]%4)%4;

		virtualReadPointer += (virtualReadPointer[0] + 4 + padding); //4 byte length
		virtualElementsLeft--;
	}

	return data;
}

void PacketQueue::DiscardLast()
{
	if (_numElements == 0) {
		return;
	}

	//Look for the last element
	sizedData lastElement = PeekLast();

	//Discard this element (No wrapping necessary because writePointer is always at the end of an element if it exists)
	u8 padding = (4-lastElement.length%4)%4;
	this->writePointer -= lastElement.length + 4 + padding; //4 byte length
	_numElements--;

	writePointer[0] = 0;

	//Reset the packetQueue
	if (readPointer == writePointer) {
		readPointer = writePointer = bufferStart;
	}
	//If our writePointer is at buffer start, we have to find the correct position at the end of the buffer
	else if (writePointer == bufferStart) {
		lastElement = PeekLast();
		padding = (4-lastElement.length%4)%4;

		writePointer = lastElement.data + lastElement.length + padding; //put writePointe to the end of the last element
		writePointer[0] = 0;
	}


	logt("PQ", "DiscardLast, now %u elements", _numElements);
}

void PacketQueue::Clean(void)
{
	_numElements = 0;
	this->readPointer = this->bufferStart;
	this->writePointer = this->bufferStart;
	this->writePointer[0] = 0;

	logt("PQ", "Clean");
}

/* EOF */
