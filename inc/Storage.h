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
 * The Storage module can save all the module's configuration variables to a
 * persistent place in memory.
 */

#pragma once

#include <Terminal.h>
#include <SimpleQueue.h>

extern "C"{
#include <pstorage.h>
}

#define TASK_BUFFER_LENGTH sizeof(taskitem)*10


class StorageEventListener
{
	public:
		StorageEventListener(){};
	virtual ~StorageEventListener(){};

	//Called when the configuration has been loaded
	virtual void ConfigurationLoadedHandler() = 0;
};

//The storage class has two different operation modes, a buffered read/write
//and a queued read/write that is used for storing and loading configurations

class Storage : public TerminalCommandListener
{
	private:
		Storage();

		//Struct and queue are used for queued write/load operations
		typedef struct
		{
				u16 operation;
				u16 dataLength;
				u32 storageBlock;
				u8* data;
				StorageEventListener* callback;

		} taskitem;

		enum operation{OPERATION_READ, OPERATION_WRITE};

		u8 taskBuffer[TASK_BUFFER_LENGTH];
		SimpleQueue* taskQueue;

		//This is the handle that is used to access the storage blocks
		pstorage_handle_t handle;
		pstorage_handle_t block_handles[10];

		//These variables are used by the buffered read/write functions to store
		//temporary copies of the input data
		/*static u32 dataBlock;
		static u8*dataStorage;
		static u32 dataLen;*/

		bool bufferedOperationInProgress;

		//The buffered read/write functions copy the given data to a local buffer that is allocated
		//during runtime, which means that the source
		bool BufferedRead(u8* data, u32 block, u32 len);
		bool BufferedWrite(u8* data, u32 block, u32 len);

		//This function is called from the pstorage library
		static void PstorageEventHandler(pstorage_handle_t* handle, u8 opCode, u32 result, u8* data, u32 dataLength);

		//This function is called when pstorage finished writing/reading sth or when
		//a new item is added to the queue
		void ProcessQueue();

	public:
		static Storage& getInstance(){
			static Storage instance;
			return instance;
		}



		//The queued read and Write functions queue multiple read/write operations
		//But the data is not copied and should therefore not be inconsistent at times
		void QueuedRead(u8* data, u16 dataLength, u32 blockId, StorageEventListener* callback);
		void QueuedWrite(u8* data, u16 dataLength, u32 blockId, StorageEventListener* callback);


		//Terminal
		bool TerminalCommandHandler(string commandName, vector<string> commandArgs);
};


