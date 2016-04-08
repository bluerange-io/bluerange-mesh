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
 * This Storage class should provide easy access to all storage functionality
 * */

#pragma once


#include <Module.h>

extern "C"{
#include <ble.h>
#include <ble_dfu.h>
}

class NewStorageEventListener;


//Packed, because it might get misaligned in the queue (should be fixed)
//To dodge HardFaults, it has been packed so that the compiler will now have to deal with it.
#pragma pack(push)
#pragma pack(1)
typedef struct
{
		u32* dataSource;
		u32* dataDestination;
		u16 dataLength;

} NewStorageTaskItemWriteData;

typedef struct
{
		u16 page;

} NewStorageTaskItemErasePage;

typedef struct
{
		u16 startPage;
		u16 numPages;

} NewStorageTaskItemErasePages;

typedef struct
{
	u8 command;
	NewStorageEventListener* callback;
	u32 userType;
	union
	{
		  NewStorageTaskItemWriteData writeData;
		  NewStorageTaskItemErasePage erasePage;
		  NewStorageTaskItemErasePages erasePages;
	} params;
} NewStorageTaskItem;
#pragma pack(pop)


enum NewStorageError {SUCCESS, QUEUE_FULL, DATA_BUFFER_IN_USE, FLASH_OPERATION_TIMED_OUT};
enum NewStorageCommand {NONE, WRITE_DATA, WRITE_AND_CACHE_DATA, ERASE_PAGE, ERASE_PAGES};




#define NEW_STORAGE_TASK_QUEUE_LENGTH 10
#define NEW_STORAGE_RETRY_COUNT 3
#define NEW_STORAGE_DATA_BUFFER_SIZE 256

class NewStorage : public TerminalCommandListener
{
	private:
		NewStorage();

		static u8 taskBuffer[NEW_STORAGE_TASK_QUEUE_LENGTH*sizeof(NewStorageTaskItem)];
		static SimpleQueue* taskQueue;

		static NewStorageTaskItem* currentTask;
		static i8 retryCount;

#pragma pack(push)
#pragma pack(4)
		static u32 dataBuffer[NEW_STORAGE_DATA_BUFFER_SIZE];
#pragma(pop)
		static bool dataBufferInUse;


		static void ProcessQueue(bool continueCurrentTask);


	public:

		//Initialize Storage class
		static void Init();

		//Erases a page and calls the callback
		static void ErasePage(u16 page, NewStorageEventListener* callback, u32 userType);

		//Erases multiple pages and then calls teh callback
		static void ErasePages(u16 startPage, u16 numPages, NewStorageEventListener* callback, u32 userType);

		//Writes some data that must stay at its source until the callback is called (destination page must be empty)
		static void WriteData(u32* source, u32* destination, u16 length, NewStorageEventListener* callback, u32 userType);

		//Caches the data in an internal buffer before saving (destination page must be empty)
		static void CacheAndWriteData(u32* source, u32* destination, u16 length, NewStorageEventListener* callback, u32 userType);

		//This system event handler must be called by the implementation
		static void SystemEventHandler(uint32_t sys_evt);

		static u8 GetNumberOfActiveTasks();
};

class NewStorageEventListener
{
	public:
		NewStorageEventListener(){};

	virtual ~NewStorageEventListener(){};

	//Struct is passed by value so that it can be dequeued before calling this handler
	//If we passed a reference, this handler would have to clear the item from the TaskQueue
	virtual void NewStorageItemExecuted(NewStorageTaskItem task, u8 errorCode) = 0;

};


