#pragma once

#include <types.h>
#include <PacketQueue.h>

#ifdef SIM_ENABLED

class TestPacketQueue
{
public:
	TestPacketQueue();
	~TestPacketQueue();


	void Start();
	void TestRandomFill();
	void TestPeekLast();
	void TestDiscardLastRandom();
	bool CheckOuterBufferOk(u8* outerBuffer, u16 bufferSize);
	void PrintPacketQueueBuffer(PacketQueue* queue);
};

#endif
