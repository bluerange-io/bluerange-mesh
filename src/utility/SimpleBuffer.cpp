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

#include <SimpleBuffer.h>

extern "C"
{
#include <cstring>
}

SimpleBuffer::SimpleBuffer(u8* buffer, u16 bufferLength, u16 itemLength)
{
	this->_numElements = 0;
	this->buffer = buffer;
	this->bufferLength = bufferLength;
	this->itemLength = itemLength;
	
	this->readPointer = this->buffer;
	this->writePointer = this->buffer;

	memset(buffer, 0x00, bufferLength);
}

//Put does only allow data sizes up to 128 byte per element
bool SimpleBuffer::Put(u8* data)
{
	//Check if Buffer can hold the item
	if(buffer + bufferLength - writePointer < itemLength) return false;
	
	memcpy(this->writePointer, data, itemLength);
	
	this->writePointer += itemLength;
	
	_numElements++;
	
	return true;
}

//Reserves a specific amount of space in the queue if available and returns the pointer to that space
u8* SimpleBuffer::Reserve(void)
{
	if(buffer + bufferLength - writePointer < itemLength) return NULL;
	else {
		this->writePointer += itemLength;
		
		_numElements++;
		
		return this->writePointer - itemLength;
	}
}

u8* SimpleBuffer::GetNext(void)
{
	
	//If buffer has been fully read, return empty data
	if(readPointer >= writePointer){		
		return NULL;
	}
	
	this->readPointer += itemLength;
	
	
  _numElements--;
	
	return this->readPointer;
}

u8* SimpleBuffer::PeekItemAt(u16 position){
	if(position >= _numElements) return NULL;
	
	return this->buffer + itemLength * position;
}


void SimpleBuffer::Clean(void)
{
   _numElements = 0;
	this->readPointer = this->buffer;
	this->writePointer = this->buffer;
}

/* EOF */
