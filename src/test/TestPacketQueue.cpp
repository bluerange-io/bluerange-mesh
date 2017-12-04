#include <TestPacketQueue.h>
#include <PacketQueue.h>
#include <Logger.h>
#include <Utility.h>

#ifdef SIM_ENABLED

TestPacketQueue::TestPacketQueue()
{
}

void TestPacketQueue::Start()
{
	Config->terminalMode = TerminalMode::TERMINAL_PROMPT_MODE;
	//Logger::getInstance()->enableTag("PQ");

	TestRandomFill();
	TestPeekLast();
	TestDiscardLastRandom();
}

void TestPacketQueue::TestRandomFill()
{
	logt("ERROR", "------ Testing packet queue ------");

	const int bufferSize = 600;

	//Create some buffer space
	u8 buffer[bufferSize + 200];
	memset(buffer, 0, bufferSize + 200);

	//Create an inner buffer used by the packetQueue (outer buffer is used for overflow checking only and should not be touched by the queue)
	u8* innerBuffer = buffer + 100;

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
		memset(testData, rand, rand);

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
			sizedData readData = queue->PeekNext();
			queue->DiscardNext();

			if (readData.length == 0) {
				logt("ERROR", "size is 0");
			}

			if (((u32)readData.length) != rand) {
				logt("ERROR", "FAIL: Wrong size read (%u instead of %u)", readData.length, rand);
				return;
			}

			//Check data
			if (memcmp(testData, readData.data, rand) != 0) {
				logt("ERROR", "FAIL: wrong data");
				return;
			}
		}

		//Check bounds for corruption
		if (buffer[99] != 0 || buffer[bufferSize + 100] != 0) {
			logt("ERROR", "Fail: overflow");
			return;
		}

		i++;
	}
}


void TestPacketQueue::TestPeekLast()
{
	logt("ERROR", "------ Testingpeek last ------");

	const int bufferSize = 200;

	//Create some buffer space
	u8 buffer[bufferSize + 200];
	memset(buffer, 0, bufferSize + 200);

	//Create an inner buffer used by the packetQueue (outer buffer is used for overflow checking only and should not be touched by the queue)
	u8* innerBuffer = buffer + 100;

	u32 rand;
	u8 testData[200];
	memset(testData, 0, sizeof(testData));

	PacketQueue* queue = new PacketQueue(innerBuffer, bufferSize);


	//Test if data is returned by peekNext and peekLast
	logt("ERROR", "------ Testing data returned peekNext and peekLast ------");

	testData[0] = 1;
	queue->Put(testData, 7);

	sizedData dataA = queue->PeekNext();
	sizedData dataB = queue->PeekLast();

	if (dataA.length != dataB.length) {
		logt("ERROR", "Fail: WRONG size");
		return;
	}
	if (dataA.data != dataB.data) {
		logt("ERROR", "Fail: WRONG data");
		return;
	}

	//Test if correct data is returned by peek last after a second element was added
	logt("ERROR", "------ Testing data returned peekNext and peekLast second element ------");

	testData[0] = 2;
	queue->Put(testData, 8);

	dataA = queue->PeekLast();
	dataB = queue->PeekNext();

	if (dataA.length != 8) {
		logt("ERROR", "Fail: WRONG size");
		return;
	}
	if (dataA.data[0] != 2) {
		logt("ERROR", "Fail: WRONG data");
		return;
	}
	if (dataB.length != 7) {
		logt("ERROR", "Fail: WRONG data");
		return;
	}
	if (dataB.data[0] != 1) {
		logt("ERROR", "Fail: WRONG data");
		return;
	}

	//Test if correct data is returned by peek last after a third element was added
	logt("ERROR", "------ Testing data returned peekNext and peekLast third element ------");

	testData[0] = 3;
	queue->Put(testData, 5);

	dataA = queue->PeekLast();

	if (dataA.length != 5) {
		logt("ERROR", "Fail: WRONG size");
		return;
	}
	if (dataA.data[0] != 3) {
		logt("ERROR", "Fail: WRONG data");
		return;
	}

	//Next, fill queue randomly with data, clean first
	logt("ERROR", "------ Testing data returned peekNext and peekLast random fill ------");
	queue->Clean();

	for (int i = 0; i < 100; i++) {
		rand = Utility::GetRandomInteger() % 50 + 1;

		//Fill and discard some elements to get to a random state
		for (u32 j = 0; j < rand; j++) {
			if (Utility::GetRandomInteger() % 4 != 0) {
				queue->Put(testData, rand);
				if (!CheckOuterBufferOk(buffer, bufferSize)) {
					logt("ERROR", "Fail: Overflow");
					return;
				}
			}
			else {
				queue->DiscardNext();
				if (!CheckOuterBufferOk(buffer, bufferSize)) {
					logt("ERROR", "Fail: Overflow");
					return;
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
		sizedData data = queue->PeekLast();

		if (data.length != lastElementRand) {
			logt("ERROR", "Fail: WRONG size");
			return;
		}
		if (data.data[0] != 1 || data.data[1] != 2) {
			logt("ERROR", "Fail: Wrong data");
			return;
		}
	}
}

void TestPacketQueue::TestDiscardLastRandom()
{
	// Next, fill queue randomly with data, while using discardLast
	logt("ERROR", "------ Testing data returned peekNext and peekLast random fill ------");

	const int bufferSize = 200;

	//Create some buffer space
	u8 buffer[bufferSize + 200];
	memset(buffer, 0, bufferSize + 200);

	//Create an inner buffer used by the packetQueue (outer buffer is used for overflow checking only and should not be touched by the queue)
	u8* innerBuffer = buffer + 100;

	u32 rand;
	u8 testData[200];
	memset(testData, 0, sizeof(testData));

	PacketQueue* queue = new PacketQueue(innerBuffer, bufferSize);

	const u32 testIterations = 10000;
	u32 elementCounter = 0;
	u8 removedElementCounters[testIterations];
	u16 elementSizes[testIterations];
	memset(removedElementCounters, 0x00, sizeof(removedElementCounters));

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
			sizedData data = queue->PeekLast();
			if (data.length != 0) {
				u32 elementId = ((u32*)data.data)[0];
				u32 elementSize = ((u32*)data.data)[1];

				//logt("ERROR", "%u: Element %u removed by discardLast (%u bytes), leftEl %u", i, elementId, elementSize, queue->_numElements-1);
				queue->DiscardLast();

				if (removedElementCounters[elementId] != 0) {
					PrintPacketQueueBuffer(queue);
					logt("ERROR", "Fail: Element removed twice");
					return;
				}
				if (data.length != elementSize) {
					PrintPacketQueueBuffer(queue);
					logt("ERROR", "Fail: Element has wrong size");
					return;
				}

				if (!CheckOuterBufferOk(buffer, bufferSize)) {
					PrintPacketQueueBuffer(queue);
					logt("ERROR", "Fail: Overflow");
					return;
				}

				removedElementCounters[elementId] = 2;
			}
		}
		//Sometimes we peek and discard the first element
		else if (rand2 == 2) {
			sizedData data = queue->PeekNext();
			if (data.length != 0) {
				u32 elementId = ((u32*)data.data)[0];
				u32 elementSize = ((u32*)data.data)[1];

				//logt("ERROR", "%u: Element %u removed by discardNext (%u bytes), leftEl %u", i, elementId, elementSize, queue->_numElements-1);
				queue->DiscardNext();

				if (removedElementCounters[elementId] != 0) {
					PrintPacketQueueBuffer(queue);
					logt("ERROR", "Fail: Element removed twice");
					return;
				}
				if (data.length != elementSize) {
					PrintPacketQueueBuffer(queue);
					logt("ERROR", "Fail: Element has wrong size");
					return;
				}

				if (!CheckOuterBufferOk(buffer, bufferSize)) {
					PrintPacketQueueBuffer(queue);
					logt("ERROR", "Fail: Overflow");
					return;
				}
				removedElementCounters[elementId] = 1;
			}
		}

		//PrintPacketQueueBuffer(queue);
	}

	//At the end, we remove all remaining elements
	while (queue->_numElements > 0) {
		sizedData data = queue->PeekLast();
		u32 elementId = ((u32*)data.data)[0];
		u32 elementSize = ((u32*)data.data)[1];

		if (removedElementCounters[elementId] != 0) {
			logt("ERROR", "Fail: Element removed twice");
			return;
		}
		if (data.length != elementSize) {
			logt("ERROR", "Fail: Element has wrong size");
			return;
		}

		removedElementCounters[elementId] = 2;
		queue->DiscardLast();

		if (!CheckOuterBufferOk(buffer, bufferSize)) {
			logt("ERROR", "Fail: Overflow");
			return;
		}
	}

	//Finaly, all removed elements will be checked
	int cntLast = 0;
	int cntFirst = 0;
	for (u32 i = 0; i < elementCounter; i++) {
		if (removedElementCounters[i] == 0) {
			logt("ERROR", "Fail: Element %u has not been removed", i);
			return;
		}
		else if (removedElementCounters[i] == 2) {
			cntLast++;
		}
		else if (removedElementCounters[i] == 1) {
			cntFirst++;
		}
	}
	logt("ERROR", "Removed by DiscardLast %u, removed by DiscardNext %u", cntLast, cntFirst);
}

bool TestPacketQueue::CheckOuterBufferOk(u8* outerBuffer, u16 bufferSize)
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

void TestPacketQueue::PrintPacketQueueBuffer(PacketQueue* queue)
{
	u8* buffer = queue->bufferStart;
	u16 bufferLength = queue->bufferLength;

	for (int i = 0; i < bufferLength; i++) {
		if (buffer + i == queue->readPointer && queue->readPointer == queue->writePointer) trace("B");
		else if (buffer + i == queue->readPointer) trace("R");
		else if (buffer + i == queue->writePointer) trace("W");
		else if (queue->readPointer < queue->writePointer && buffer + i > queue->readPointer && buffer + i < queue->writePointer) trace("#");
		else if (queue->readPointer > queue->writePointer && (buffer + i > queue->readPointer || buffer + i < queue->writePointer)) trace("#");
		else trace("-");
	}

	trace(EOL);
}

TestPacketQueue::~TestPacketQueue()
{
}

#endif
