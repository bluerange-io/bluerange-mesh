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

#include <PacketQueue.h>
#include "GlobalState.h"
#include <Logger.h>
#include <cstring>
#include "Utility.h"

PacketQueue::PacketQueue()
{
    //Leave uninit
}

//Data will be 4-byte aligned if all inputs are 4 byte aligned
PacketQueue::PacketQueue(u32* buffer, u16 bufferLength)
    :bufferStart((u8*)buffer),
    bufferEnd((u8*)buffer + bufferLength - 1), //FIXME: workaround to avoid 1byte overflow of the packet queue
    bufferLength(bufferLength - 1) // s.o.
{
    this->readPointer = this->bufferStart;
    this->writePointer = this->bufferStart;

    ((u16*)writePointer)[0] = 0;

    CheckedMemset(buffer, 0, bufferLength);
}

//IF READ AND WRITE ARE EQUAL, THE QUEUE IS EMPTY
//Put does only allow data sizes up to 250 byte per element
bool PacketQueue::Put(u8* data, u16 dataLength)
{
    u8* dest = Reserve(dataLength);

    if(dest != nullptr){
        CheckedMemcpy(dest, data, dataLength);
        return true;
    } else {
        return false;
    }
}

u8* PacketQueue::Reserve(u16 dataLength)
{
    if (dataLength == 0) return nullptr;

    if(dataLength + 10 > bufferLength){
        logt("ERROR", "Too big");                                                                    //LCOV_EXCL_LINE assertion
        GS->logger.LogCustomError(CustomErrorTypes::FATAL_PACKETQUEUE_PACKET_TOO_BIG, dataLength);    //LCOV_EXCL_LINE assertion
        SIMEXCEPTION(IllegalArgumentException);                                                        //LCOV_EXCL_LINE assertion
        return nullptr;                                                                                //LCOV_EXCL_LINE assertion
    }

    //Padding makes sure that we only save 4-byte aligned data
    u8 padding = (4-dataLength%4)%4;

    //Keep 4 byte for sizeField and one byte to not let read and write pointers overlap
    u16 elementSize = dataLength + 4 + 1 + padding;

    //If the writePointer is ahead (or at the same point) of the read pointer && bufferSpace
    //at the end is not enough && dataSize at the beginning is enough
    if (writePointer >= readPointer && bufferEnd - writePointer <= elementSize && readPointer - bufferStart >= elementSize)
    {
        writePointer = bufferStart;
        ((u16*)writePointer)[0] = 0;
    }

    //Check if Buffer can hold the item
    else if (readPointer <= writePointer && writePointer + elementSize >= bufferEnd)
    {
        logt("PQ", "No space for %u bytes", dataLength);
        return nullptr;
    }
    else if (readPointer > writePointer && writePointer + elementSize >= readPointer)
    {
        logt("PQ", "No space for %u bytes", dataLength);
        return nullptr;
    }

    ((u16*)writePointer)[0] = dataLength;
    u8* dataPointer = this->writePointer + 4; //jump over length field

    //logt("ERROR", "Put %u at %u", dataLength, dataPointer);

    //Move write Pointer to next field
    this->writePointer += dataLength + 4 + padding; //4 byte length

    //Set length to 0 for next datafield
    ((u16*)writePointer)[0] = 0;

    _numElements++;

    logt("PQ", "Reserve %u bytes, now %u elements", dataLength, _numElements);

    CheckedMemset(dataPointer, 0, dataLength);

    return dataPointer;
}

SizedData PacketQueue::PeekNext() const
{
    return PeekNext(0);
}

SizedData PacketQueue::PeekNext(u8 pos) const
{
    SizedData data;
    //If queue has been fully read, return empty data
    if (pos >= _numElements){
        data.length = 0;
        data.data = nullptr;
        return data;
    }

    u8* virtualReadPointer = readPointer;

    for(int i=0; i<=pos; i++){
        //Check if we reached the end and wrap
        if (((u16*)virtualReadPointer)[0] == 0 && writePointer < virtualReadPointer) {
            virtualReadPointer = bufferStart;
        }

        if(i>0){
            //Padding makes sure that we only save 4-byte aligned data
            u8 padding = (4-((u16*)virtualReadPointer)[0]%4)%4;
            virtualReadPointer += ((u16*)virtualReadPointer)[0] + 4 + padding; //4 byte length

            //Check if we reached the end of entries and the next entry following is an empty one, if yes place read pointer at start
            if (((u16*)virtualReadPointer)[0] == 0) {
                virtualReadPointer = bufferStart;
            }
        }
    }

    data.length = ((u16*)virtualReadPointer)[0];
    data.data = virtualReadPointer + 4; // 4 byte added for length field
    
    return data;
}

void PacketQueue::DiscardNext()
{
    if (_numElements == 0) return;

    //Check if we reached the end and wrap
    if (((u16*)readPointer)[0] == 0 && writePointer < readPointer) {
        readPointer = bufferStart;
    }
    
    //Padding makes sure that we only save 4-byte aligned data
    u8 padding = (4-((u16*)readPointer)[0]%4)%4;
    this->readPointer += ((u16*)readPointer)[0] + 4 + padding; //4 byte length
    _numElements--;

    //Reset the pointers to buffer start if the queue is empty
    if (readPointer == writePointer) {
        readPointer = writePointer = bufferStart;
    }

    //Check if we reached the end of entries and the next entry following is an empty one, if yes place read pointer at start
    if (((u16*)readPointer)[0] == 0) {
        readPointer = bufferStart;
    }

    logt("PQ", "DiscardNext, now %u elements", _numElements);
}

//Iterates over all items in order to find the last element
SizedData PacketQueue::PeekLast()
{
    SizedData data = { nullptr, 0 };

    //Return 0 length data in case we have no elements
    if (_numElements == 0){
        return data;
    }

    u16 virtualElementsLeft = _numElements;
    u8* virtualReadPointer = readPointer;

    //Iterate over the queue using a copy of read pointer and numElements until reaching the last element
    while (virtualElementsLeft > 0)
    {
        //Check if we reached the end and wrap
        if (((u16*)virtualReadPointer)[0] == 0 && writePointer < virtualReadPointer)
            virtualReadPointer = bufferStart;

        //Copy data reference into sizeData
        data.length = ((u16*)virtualReadPointer)[0];
        data.data = virtualReadPointer + 4; // 4 byte added for length field

        //Do a virtual discard
        //Padding makes sure that we only save 4-byte aligned data
        u8 padding = (4-((u16*)virtualReadPointer)[0]%4)%4;

        virtualReadPointer += (((u16*)virtualReadPointer)[0] + 4 + padding); //4 byte length
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
    SizedData lastElement = PeekLast();

    //Discard this element (No wrapping necessary because writePointer is always at the end of an element if it exists)
    u8 padding = (4-lastElement.length.GetRaw()%4)%4;
    this->writePointer -= lastElement.length.GetRaw() + 4 + padding; //4 byte length
    _numElements--;

    ((u16*)writePointer)[0] = 0;

    //Reset the packetQueue
    if (readPointer == writePointer) {
        readPointer = writePointer = bufferStart;
    }
    //If our writePointer is at buffer start, we have to find the correct position at the end of the buffer
    else if (writePointer == bufferStart) {
        lastElement = PeekLast();
        padding = (4-lastElement.length.GetRaw()%4)%4;

        writePointer = lastElement.data + lastElement.length + padding; //put writePointe to the end of the last element
        ((u16*)writePointer)[0] = 0;
    }


    logt("PQ", "DiscardLast, now %u elements", _numElements);
}

void PacketQueue::Clean(void)
{
    _numElements = 0;
    readPointer = this->bufferStart;
    writePointer = this->bufferStart;
    ((u16*)writePointer)[0] = 0;

    logt("PQ", "Clean");
}

//Allows us to print the contents of the packet queue
void PacketQueue::Print() const
{
    logt("PQ", "Printing Queue: ");
    u8* tmpReadPointer = readPointer;
    for(u32 i=0; i<_numElements; i++){
        //Check if we reached the end and wrap
        if (((u16*)tmpReadPointer)[0] == 0 && writePointer < tmpReadPointer)
            tmpReadPointer = bufferStart;

        SizedData data;
        data.length = ((u16*)tmpReadPointer)[0];
        data.data = tmpReadPointer + 4; // 4 byte added for length field

        trace("%u: ", i);
        for(u32 j=0; j<data.length; j++){
            trace("%02x:", data.data[j]);
        }
        trace(EOL);
    }
}

/* EOF */
