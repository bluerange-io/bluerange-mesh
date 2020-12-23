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
#include "gtest/gtest.h"
#include <FmTypes.h>
#include <CherrySimTester.h>
#include <PacketQueue.h>
#include <Utility.h>

#ifdef SIM_ENABLED

class TestPacketQueue : public ::testing::Test {
private:
    CherrySimTester* tester;
public:
    TestPacketQueue() {
        //We have to boot up a simulator for this test because the PacketQueue uses the Logger
        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
        simConfig.nodeConfigName.insert( { "prod_sink_nrf52", 1 } );
        tester = new CherrySimTester(testerConfig, simConfig);
        tester->Start();
    }

    ~TestPacketQueue() {
        delete tester;
    }
};

bool CheckOuterBufferOk(u8* outerBuffer, u16 bufferSize);
void PrintPacketQueueBuffer(PacketQueue* queue);

TEST_F(TestPacketQueue, TestPeekNext) {
    //printf("------ Testing packet queue peeknext ------");
    StackBaseSetter sbs;
    NodeIndexSetter setter(0);
    const int bufferSize = 600;

    //Create some buffer space
    u8 buffer[bufferSize + 200];
    CheckedMemset(buffer, 0, bufferSize + 200);

    //Create an inner buffer used by the packetQueue (outer buffer is used for overflow checking only and should not be touched by the queue)
    u32* innerBuffer = (u32*)(buffer + 100);

    //Initialize the queue
    PacketQueue* queue = new PacketQueue(innerBuffer, bufferSize);

    //Reserve some space for saving tmpData
    u8 data[100];
    CheckedMemset(data, 0x00, 100);

    //Fill 10 items
    for (int i = 0; i < 10; i++) {
        data[0] = i;

        u8* dest = queue->Reserve(20);

        if (dest != nullptr) {
            CheckedMemcpy(dest, data, 20);
            //printf("Memory dest %u", dest);
        }
        else {
            FAIL() << "Buffer too small";
        }
    }

    //printf("----- Now reading -----");

    //Read Data
    for (int i = 0; i < 10; i++) {
        SizedData d = queue->PeekNext(i);

        if (d.data[0] != i) {
            FAIL() << "Wrong entry read";
        }
    }

    //Read one more than available
    SizedData d = queue->PeekNext(10);

    if (d.length != 0) {
        FAIL() << "Item should not exist";
    }

    queue->Clean();

    //Step 2: Fill randomly with variable size data and always read all items back using peekNext

    //printf("------ Testing packet queue randomfill with peeknext ------");

    u8 fillCounter = 0;
    u8 readCounter = 0;
    int rnd;


    for (int j = 0; j < 1000; j++) {
        //Fill some items
        for (int i = 0; i < 100; i++){
            rnd = Utility::GetRandomInteger() % 50 + 1;
            data[0] = fillCounter;

            u8* dest = queue->Reserve(rnd);

            if (dest != nullptr) {
                CheckedMemcpy(dest, data, rnd);
                fillCounter++;
                //logt("ERROR", "Added item, with size %u at %u, fillCounter now at %u", rnd, dest, fillCounter);
            }
            else {
                break;
            }
        }

        //Peek all items
        for (int i = 0; i < queue->_numElements; i++) {
            SizedData d = queue->PeekNext(i);

            //logt("ERROR", "Read item at %u, with size %u, and data %u", d.data, d.length, d.data[0]);

            if (d.data[0] != (u8)(readCounter + i)) {
                FAIL() << "FAIL: Wrong entry read";
            }
        }

        //Remove some items
        rnd = Utility::GetRandomInteger() % queue->_numElements;

        for (int i = 0; i < queue->_numElements; i++)
        {
            queue->DiscardNext();
            readCounter++;
            //logt("ERROR", "Removing item, readCounter now at %u", readCounter);
        }
    }

    delete queue;

    //printf("------ DONE ------");
}

TEST_F(TestPacketQueue, TestRandomFill) {
    //printf("------ Testing packet queue ------");
    StackBaseSetter sbs;
    NodeIndexSetter setter(0);

    const int bufferSize = 600;

    //Create some buffer space
    u8 buffer[bufferSize + 200];
    CheckedMemset(buffer, 0, bufferSize + 200);

    //Create an inner buffer used by the packetQueue (outer buffer is used for overflow checking only and should not be touched by the queue)
    u32* innerBuffer = (u32*)(buffer + 100);

    //Initialize the queue
    PacketQueue* queue = new PacketQueue(innerBuffer, bufferSize);

    //Generate some testData 1,2,3,4... that can be filled in the queue
    u8 testData[200];
    for (int i = 0; i<200; i += 1) {
        testData[i] = i + 1;
    }

    int i = 1;
    while (i<10000) {
        //Generate random data of random length
        u32 rand = Utility::GetRandomInteger() % 100 + 1;
        rand += 3;
        //if (i % 100 == 0) trace("%u,", rand);
        CheckedMemset(testData, rand, rand);

        //Fill queue a few times with this data
        u32 j = 0;
        for (j = 0; j<rand; j++) {
            if (!queue->Put(testData, rand)) {
                //logt("ERROR", "queue full");
                break;
            }
        }

        //Read queue a few times
        for (; j>0; j--) {
            SizedData readData = queue->PeekNext();
            queue->DiscardNext();

            if (readData.length.IsZero()) {
                printf("size is 0");
            }

            if (((u32)readData.length.GetRaw()) != rand) {
                FAIL() << "Wrong size read (" << readData.length.GetRaw() << " instead of " << rand << ")";
            }

            //Check data
            if (memcmp(testData, readData.data, rand) != 0) {
                FAIL() << "FAIL: wrong data";
            }
        }

        //Check bounds for corruption
        if (buffer[99] != 0 || buffer[bufferSize + 100] != 0) {
            FAIL() << "overflow";
        }

        i++;
    }

    delete queue;
}


TEST_F(TestPacketQueue, TestPeekLast) {
    //printf("------ Testingpeek last ------");
    StackBaseSetter sbs;
    NodeIndexSetter setter(0);
    const int bufferSize = 200;

    //Create some buffer space
    u8 buffer[bufferSize + 200];
    CheckedMemset(buffer, 0, bufferSize + 200);

    //Create an inner buffer used by the packetQueue (outer buffer is used for overflow checking only and should not be touched by the queue)
    u32* innerBuffer = (u32*)(buffer + 100);

    u32 rand;
    u8 testData[200];
    CheckedMemset(testData, 0, sizeof(testData));

    PacketQueue* queue = new PacketQueue(innerBuffer, bufferSize);


    //Test if data is returned by peekNext and peekLast
    //printf("------ Testing data returned peekNext and peekLast ------");

    testData[0] = 1;
    queue->Put(testData, 7);

    SizedData dataA = queue->PeekNext();
    SizedData dataB = queue->PeekLast();

    if (dataA.length != dataB.length) {
        FAIL() << "WRONG size";
    }
    if (dataA.data != dataB.data) {
        FAIL() << "WRONG data";
    }

    //Test if correct data is returned by peek last after a second element was added
    //printf("------ Testing data returned peekNext and peekLast second element ------");

    testData[0] = 2;
    queue->Put(testData, 8);

    dataA = queue->PeekLast();
    dataB = queue->PeekNext();

    if (dataA.length != 8) {
        FAIL() << "WRONG size";
    }
    if (dataA.data[0] != 2) {
        FAIL() << "WRONG data not 2";
    }
    if (dataB.length != 7) {
        FAIL() << "WRONG data not 7";
    }
    if (dataB.data[0] != 1) {
        FAIL() << "WRONG data not 1";
    }

    //Test if correct data is returned by peek last after a third element was added
    //printf("------ Testing data returned peekNext and peekLast third element ------");

    testData[0] = 3;
    queue->Put(testData, 5);

    dataA = queue->PeekLast();

    if (dataA.length != 5) {
        FAIL() << "WRONG size";
    }
    if (dataA.data[0] != 3) {
        FAIL() << "WRONG data";
    }

    //Next, fill queue randomly with data, clean first
    //printf("------ Testing data returned peekNext and peekLast random fill ------");
    queue->Clean();

    for (int i = 0; i < 100; i++) {
        rand = Utility::GetRandomInteger() % 50 + 1;

        //Fill and discard some elements to get to a random state
        for (u32 j = 0; j < rand; j++) {
            if (Utility::GetRandomInteger() % 4 != 0) {
                queue->Put(testData, rand);
                if (!CheckOuterBufferOk(buffer, bufferSize)) {
                    FAIL() << "Overflow";
                }
            }
            else {
                queue->DiscardNext();
                if (!CheckOuterBufferOk(buffer, bufferSize)) {
                    FAIL() << "Overflow";
                }
            }
            //printf("%u : %u\n",i, j);
            //PrintPacketQueueBuffer(queue);
        }
        
        //Add the last element (discard others if we have not enough space)
        u32 lastElementRand = Utility::GetRandomInteger() % 50 + 2;
        testData[0] = 1;
        testData[1] = 2;
        while (!queue->Put(testData, lastElementRand)) {
            queue->DiscardNext();
        }

        //Peek the last element and check if it is correct
        SizedData data = queue->PeekLast();

        if (data.length != lastElementRand) {
            FAIL() << "WRONG size";
        }
        if (data.data[0] != 1 || data.data[1] != 2) {
            FAIL() << "Wrong data";
        }
    }

    delete queue;
}

TEST_F(TestPacketQueue, TestDiscardLastRandom) {
    // Next, fill queue randomly with data, while using discardLast
    //printf("------ Testing data returned peekNext and peekLast random fill ------");
    StackBaseSetter sbs;
    NodeIndexSetter setter(0);
    const int bufferSize = 200;

    //Create some buffer space
    u8 buffer[bufferSize + 200];
    CheckedMemset(buffer, 0, bufferSize + 200);

    //Create an inner buffer used by the packetQueue (outer buffer is used for overflow checking only and should not be touched by the queue)
    u32* innerBuffer = (u32*)(buffer + 100);

    u32 rand;
    u8 testData[200];
    CheckedMemset(testData, 0, sizeof(testData));

    PacketQueue* queue = new PacketQueue(innerBuffer, bufferSize);

    const u32 testIterations = 10000;
    u32 elementCounter = 0;
    u8 removedElementCounters[testIterations];
    u16 elementSizes[testIterations];
    CheckedMemset(removedElementCounters, 0x00, sizeof(removedElementCounters));

    for (u32 i = 0; i < testIterations; i++) {
        rand = Utility::GetRandomInteger() % 20 + 8;
        u32 rand2 = Utility::GetRandomInteger() % 3;

        //Sometimes, we add elements to the queue
        if (rand2 == 0) {
            //Put an element in the queue and increment the elementCounter
            ((u32*)testData)[0] = elementCounter;
            ((u32*)testData)[1] = rand;
            bool putResult = queue->Put(testData, rand);
            //Increment elementCounter if put was successful
            if (putResult) {
                elementSizes[elementCounter] = rand;
                //logt("ERROR", "%u: Element %u added (%u bytes), hasEl %u", i, elementCounter, rand, queue->_numElements);
                elementCounter++;
            }
        }
        //Sometimes we peek and discard the last element
        else if (rand2 == 1) {
            SizedData data = queue->PeekLast();
            if (data.length != 0) {
                u32 elementId = ((u32*)data.data)[0];
                u32 elementSize = ((u32*)data.data)[1];

                //logt("ERROR", "%u: Element %u removed by discardLast (%u bytes), leftEl %u", i, elementId, elementSize, queue->_numElements-1);
                queue->DiscardLast();

                if (removedElementCounters[elementId] != 0) {
                    PrintPacketQueueBuffer(queue);
                    FAIL() << "Element removed twice";
                }
                if (data.length != elementSize) {
                    PrintPacketQueueBuffer(queue);
                    FAIL() << "Element has wrong size";
                }

                if (!CheckOuterBufferOk(buffer, bufferSize)) {
                    PrintPacketQueueBuffer(queue);
                    FAIL() << "Overflow";
                }

                removedElementCounters[elementId] = 2;
            }
        }
        //Sometimes we peek and discard the first element
        else if (rand2 == 2) {
            SizedData data = queue->PeekNext();
            if (data.length != 0) {
                u32 elementId = ((u32*)data.data)[0];
                u32 elementSize = ((u32*)data.data)[1];

                //logt("ERROR", "%u: Element %u removed by discardNext (%u bytes), leftEl %u", i, elementId, elementSize, queue->_numElements-1);
                queue->DiscardNext();

                if (removedElementCounters[elementId] != 0) {
                    PrintPacketQueueBuffer(queue);
                    FAIL() << "Element removed twice";
                }
                if (data.length != elementSize) {
                    PrintPacketQueueBuffer(queue);
                    FAIL() << "Element has wrong size";
                }

                if (!CheckOuterBufferOk(buffer, bufferSize)) {
                    PrintPacketQueueBuffer(queue);
                    FAIL() << "Overflow";
                }
                removedElementCounters[elementId] = 1;
            }
        }

        //PrintPacketQueueBuffer(queue);
    }

    //At the end, we remove all remaining elements
    while (queue->_numElements > 0) {
        SizedData data = queue->PeekLast();
        u32 elementId = ((u32*)data.data)[0];
        u32 elementSize = ((u32*)data.data)[1];

        if (removedElementCounters[elementId] != 0) {
            FAIL() << "Element removed twice";
        }
        if (data.length != elementSize) {
            FAIL() << "Element has wrong size";
        }

        removedElementCounters[elementId] = 2;
        queue->DiscardLast();

        if (!CheckOuterBufferOk(buffer, bufferSize)) {
            FAIL() << "Overflow";
        }
    }

    //Finaly, all removed elements will be checked
    int cntLast = 0;
    int cntFirst = 0;
    for (u32 i = 0; i < elementCounter; i++) {
        if (removedElementCounters[i] == 0) {
            FAIL() << "Element" << i << " has not been removed";
        }
        else if (removedElementCounters[i] == 2) {
            cntLast++;
        }
        else if (removedElementCounters[i] == 1) {
            cntFirst++;
        }
    }
    //printf("Removed by DiscardLast %u, removed by DiscardNext %u", cntLast, cntFirst);

    delete queue;
}

bool CheckOuterBufferOk(u8* outerBuffer, u16 bufferSize)
{
    for (int i = 0; i < 100; i++) {
        if (outerBuffer[i] != 0) {
            return false;
        }
    }
    for (int i = 0; i < 100; i++) {
        if (outerBuffer[100+bufferSize+i] != 0) {
            return false;
        }
    }
    return true;
}

void PrintPacketQueueBuffer(PacketQueue* queue)
{
    u8* buffer = queue->bufferStart;
    u16 bufferLength = queue->bufferLength;

    for (int i = 0; i < bufferLength; i++) {
        if (buffer + i == queue->readPointer && queue->readPointer == queue->writePointer) printf("B");
        else if (buffer + i == queue->readPointer) printf("R");
        else if (buffer + i == queue->writePointer) printf("W");
        else if (queue->readPointer < queue->writePointer && buffer + i > queue->readPointer && buffer + i < queue->writePointer) printf("#");
        else if (queue->readPointer > queue->writePointer && (buffer + i > queue->readPointer || buffer + i < queue->writePointer)) printf("#");
        else printf("-");
    }

    printf(EOL);
}

#endif
