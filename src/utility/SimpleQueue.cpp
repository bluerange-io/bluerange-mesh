#include <SimpleQueue.h>
#include <Logger.h>

extern "C"{
#include <nrf_error.h>
#include <cstring>
}

//TODO: This queue might hardfault because of unaligned memory access, must be fixed

SimpleQueue::SimpleQueue(u8* buffer, u32 bufferLength)
{
    this->_numElements = 0;
    this->bufferStart = buffer;
    this->bufferEnd = buffer + bufferLength;
    this->bufferLength = bufferLength;

    this->readPointer = this->bufferStart;
    this->writePointer = this->bufferStart;


    ((u32*)writePointer)[0] = 0;

}

//IF READ AND WRITE ARE EQUAL, THE QUEUE IS EMPTY

bool SimpleQueue::Put(u8* data, u32 dataLength)
{
	//Keep two byte for sizeField and one byte to not let read and write pointers overlap
	u32 elementSize = dataLength + 4 + 1;
	
	//If the writePointer is ahead (or at the same point) of the read pointer && bufferSpace
	//at the end is not enough && dataSize at the beginning is enough
	if(writePointer >= readPointer && (u32)(bufferEnd - writePointer) <= elementSize && (u32)(readPointer - bufferStart) >= elementSize){
		writePointer = bufferStart;
		memset(writePointer, 0, 4);
	}
	
	//Check if Buffer can hold the item
	else if(readPointer <= writePointer && writePointer + elementSize >= bufferEnd){

	    return false;
	}
	else if(readPointer > writePointer && writePointer + elementSize >= readPointer){
	    return false;
	}
	
	memcpy(writePointer, &dataLength, 4);
	memcpy(writePointer + 4, data, dataLength);
	
	writePointer += dataLength + 4; //+two for size field
	//Set length to 0 for next datafield
	memset(writePointer, 0, 4);
	
	_numElements++;

	return true;
}



sizedData SimpleQueue::PeekNext(void)
{
   sizedData data;
	//If queue has been fully read, return empty data
	if(_numElements == 0){
		data.length = 0;
		return data;
	}

	//Check if we reached the end and wrap
	u32 size;
	memcpy(&size, readPointer, 4);
	if(size == 0 && writePointer < readPointer){
		memcpy(&data.length, bufferStart, 4);
		data.data = bufferStart + 4;

		return data;
	}
	else
	{
		memcpy(&data.length, readPointer, 4);
		data.data = this->readPointer + 4;

		return data;
	}

}


sizedData SimpleQueue::GetNext(void)
{
   sizedData data;
	//If queue has been fully read, return empty data
	if(_numElements == 0){		
		data.length = 0;
		return data;
	}
	
	//Check if we reached the end and wrap
	u32 size;
	memcpy(&size, readPointer, 4);
	if(size == 0 && writePointer < readPointer) readPointer = bufferStart;
	
	memcpy(&data.length, readPointer, 4);
	data.data = this->readPointer + 4;
	
	this->readPointer += data.length + 4;

	_numElements--;
	return data;
}



void SimpleQueue::DiscardNext(void)
{
	//If queue has been fully read, return empty data
	if(_numElements == 0){
		return;
	}

	//Check if we reached the end and wrap
	u32 size;
	memcpy(&size, readPointer, 4);
	if(size == 0 && writePointer < readPointer) readPointer = bufferStart;


	memcpy(&size, readPointer, 4);
	this->readPointer += size + 4;

	_numElements--;
}



void SimpleQueue::Clean(void)
{
	_numElements = 0;
	this->readPointer = this->bufferStart;
	this->writePointer = this->bufferStart;
	memset(&writePointer, 0, 4);
}

/* EOF */
