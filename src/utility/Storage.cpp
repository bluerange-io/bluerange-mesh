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

#include <Storage.h>
#include <Logger.h>
#include <Module.h>

extern "C"{
#include <app_error.h>
#include <stdlib.h>
#include <cstring>
}


//Use this storage class to save module configurations and either implement a callback queue
//or even better a save and load queue with callback handlers


Storage::Storage()
{
	u32 err;

	//Register with Terminal
	Terminal::AddTerminalCommandListener(this);

	//Initialize queue for queueing store and load tasks
	taskQueue = new SimpleQueue(taskBuffer, TASK_BUFFER_LENGTH);

	//Initialize pstorage library
	pstorage_module_param_t param;
	u8 num_blocks = STORAGE_BLOCK_NUMBER;

	bufferedOperationInProgress = false;

	param.block_size  = STORAGE_BLOCK_SIZE; //Multiple of 4 and divisor of page size (pagesize is usually 1024) in case total size is bigger than page size
	param.block_count = num_blocks;
	param.cb          = PstorageEventHandler;

	err = pstorage_init();
	APP_ERROR_CHECK(err);

	//Register a number of blocks
	if (pstorage_register(&param, &handle) != NRF_SUCCESS){
		logt("STORAGE", "Could not register storage");
	}
	for (u32 i = 0; i < num_blocks; ++i){
		pstorage_block_identifier_get(&handle, i, &block_handles[i]);
	}
}

void Storage::PstorageEventHandler(pstorage_handle_t* handle, u8 opCode, u32 result, u8* data, u32 dataLength)
{
	//logt("STORAGE", "Event: %u, result:%d, len:%d", opCode, result, dataLength);
	if(result != NRF_SUCCESS) logt("STORAGE", "%s", Logger::getInstance().getPstorageStatusErrorString(opCode));


	//if it 's a clear, we do the write
	if(opCode == PSTORAGE_CLEAR_OP_CODE && result == NRF_SUCCESS){

		taskitem* task = (taskitem*)Storage::getInstance().taskQueue->PeekNext().data;

		pstorage_store(&Storage::getInstance().block_handles[task->storageBlock], task->data, task->dataLength, 0);

	}

	//If its a write or a read, we drop the last item and execute the next one
	else if((opCode == PSTORAGE_STORE_OP_CODE || opCode == PSTORAGE_LOAD_OP_CODE) && result == NRF_SUCCESS)
	{
		//Remove item from queue because it was successful
		taskitem* task = (taskitem*)Storage::getInstance().taskQueue->GetNext().data;

		//Notify callback
		if(task->operation == OPERATION_READ){ task->callback->ConfigurationLoadedHandler(); }
		else if(task->operation == OPERATION_WRITE){};

		Storage::getInstance().bufferedOperationInProgress = false;
		Storage::getInstance().ProcessQueue();
	}
	else if(result != NRF_SUCCESS){
		Storage::getInstance().bufferedOperationInProgress = false;
		Storage::getInstance().ProcessQueue();
	}
}


//TODO: read and write can only process data that is a multiple of 4 bytes
//This involves problems when the data buffer has a different size
bool Storage::BufferedRead(u8* data, u32 block, u32 len)
{
	//logt("STORAGE", "Reading len:%u from block:%u", len, block);

	pstorage_load(data, &block_handles[block], len, 0);

	return true;
}

bool Storage::BufferedWrite(u8* data, u32 block,u32 len)
{

	logt("STORAGE", "Writing len:%u to block:%u", len, block);

	//Call clear first before writing to the flash
	//Clear will generate an event that is handeled in the PstorabeEventHandler
	u32 err = pstorage_clear(&block_handles[block], 128);
	APP_ERROR_CHECK(err);

	return true;
}

void Storage::QueuedRead(u8* data, u16 dataLength, u32 blockId, StorageEventListener* callback)
{
	taskitem task;
	task.data = data;
	task.dataLength = dataLength;
	task.storageBlock = blockId;
	task.callback = callback;
	task.operation = operation::OPERATION_READ;

	taskQueue->Put((u8*)&task, sizeof(taskitem));

	ProcessQueue();
}

void Storage::QueuedWrite(u8* data, u16 dataLength, u32 blockId, StorageEventListener* callback)
{
	taskitem task;
	task.data = data;
	task.dataLength = dataLength;
	task.storageBlock = blockId;
	task.callback = callback;
	task.operation = operation::OPERATION_WRITE;

	taskQueue->Put((u8*)&task, sizeof(taskitem));

	ProcessQueue();
}
/*
void Storage::QueuedErasePage(u16 page, StorageEventListener* callback)
{
	taskitem task;
	task.data = NULL;
	task.dataLength = 0;
	task.storageBlock = blockId;
	task.callback = callback;
	task.operation = operation::OPERATION_ERASE_PAGE;

	taskQueue->Put((u8*)&task, sizeof(taskitem));

	ProcessQueue();
}*/

void Storage::ProcessQueue()
{
	if(bufferedOperationInProgress || taskQueue->_numElements < 1){
		return;
	} else {
		bufferedOperationInProgress = true;
	}

	//Get one item from the queue and execute it
	sizedData data = taskQueue->PeekNext();
	taskitem* task = (taskitem*)data.data;

	if(task->operation == operation::OPERATION_READ) BufferedRead(task->data, task->storageBlock, task->dataLength);
	else if(task->operation == operation::OPERATION_WRITE) BufferedWrite(task->data, task->storageBlock, task->dataLength);
}

bool Storage::TerminalCommandHandler(string commandName, vector<string> commandArgs){

	if(commandName == "save"){
		int slotNum = atoi(commandArgs[0].c_str());
		int len = strlen(commandArgs[1].c_str());
		if(len % 4 != 0) len = len +4 - (len%4);

		char data[len];
		strcpy(data, commandArgs[1].c_str());

		BufferedWrite((unsigned char*)data, slotNum, len);

		logt("STORAGE", "len: %d is saved in %d", len, slotNum);

	} else if (commandName == "load"){
		int slotNum = atoi(commandArgs[0].c_str());
		int len = atoi(commandArgs[1].c_str());
		if(len % 4 != 0) len = len +4 - (len%4);

		u8 dest_data[len];
		BufferedRead(dest_data, slotNum, len);

		logt("STORAGE", "%s (%d) has been loaded from %d", dest_data, len, slotNum);

	} else {
		return false;
	}
	return true;

}
