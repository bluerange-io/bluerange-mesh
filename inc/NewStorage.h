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

/*
 * This Storage class should provide easy access to all storage functionality
 * */

#pragma once

#include <GlobalState.h>
#include <PacketQueue.h>

extern "C"{
#include <ble.h>
}

class NewStorageEventListener;


//Packed, because it might get misaligned in the queue (should be fixed)
//To dodge HardFaults, it has been packed so that the compiler will now have to deal with it.
#pragma pack(push)
#pragma pack(1)

#define SIZEOF_NEW_STORAGE_TASK_ITEM_HEADER 12

#define SIZEOF_NEW_STORAGE_TASK_ITEM_WRITE_DATA (SIZEOF_NEW_STORAGE_TASK_ITEM_HEADER + 10)
typedef struct
{
		u32* dataSource;
		u32* dataDestination;
		u16 dataLength;

} NewStorageTaskItemWriteData;

//We should pay attention that the data pointer is saved at a word aligned address so we can directly write to flash from this pointer
#define SIZEOF_NEW_STORAGE_TASK_ITEM_WRITE_CACHED_DATA (SIZEOF_NEW_STORAGE_TASK_ITEM_HEADER + 6)
typedef struct
{
	u32* dataDestination;
	u16 dataLength;
	u16 reserved;
	u8 data[1];

} NewStorageTaskItemWriteCachedData;

#define SIZEOF_NEW_STORAGE_TASK_ITEM_ERASE_PAGE (SIZEOF_NEW_STORAGE_TASK_ITEM_HEADER + 2)
typedef struct
{
		u16 page;

} NewStorageTaskItemErasePage;

#define SIZEOF_NEW_STORAGE_TASK_ITEM_ERASE_PAGES (SIZEOF_NEW_STORAGE_TASK_ITEM_HEADER + 4)
typedef struct
{
	u16 startPage;
	u16 numPages;

} NewStorageTaskItemErasePages;
#define SIZEOF_NEW_STORAGE_TASK_ITEM_END_TRANSACTION (SIZEOF_NEW_STORAGE_TASK_ITEM_HEADER + 0)
typedef struct
{


} NewStorageTaskItemEndTransaction;

//The data cache is useful in a multi-task transaction where data has to be cached for later and other tasks have to execute first
#define SIZEOF_NEW_STORAGE_TASK_ITEM_DATA_CACHE (SIZEOF_NEW_STORAGE_TASK_ITEM_HEADER + 4)
typedef struct
{
	u16 dataLength;
	u16 reserved;
	u8 data[1];

} NewStorageTaskItemDataCache;


//TODO: A compact version will only need 1 byte command, 1 byte transactionId, 
typedef struct
{
	u8 command;
	u8 reserved;
	u16 transactionId;
	NewStorageEventListener* callback;
	u32 userType;
	union
	{
		NewStorageTaskItemDataCache dataCache;
		NewStorageTaskItemEndTransaction endTransaction;
		NewStorageTaskItemWriteData writeData;
		NewStorageTaskItemWriteCachedData writeCachedData;
		NewStorageTaskItemErasePage erasePage;
		NewStorageTaskItemErasePages erasePages;
	} params;
} NewStorageTaskItem;
#pragma pack(pop)


enum NewStorageError {SUCCESS, QUEUE_FULL, FLASH_OPERATION_TIMED_OUT, TRANSACTION_IN_PROGRESS};
enum NewStorageCommand {NONE, WRITE_DATA, WRITE_AND_CACHE_DATA, ERASE_PAGE, ERASE_PAGES, END_TRANSACTION, CACHE_DATA};


#define NEW_STORAGE_RETRY_COUNT 3
#define NEW_STORAGE_QUEUE_SIZE 256

class NewStorage
{
	private:
		NewStorage();
				
		u8 taskBuffer[NEW_STORAGE_QUEUE_SIZE];
		PacketQueue* taskQueue;

		NewStorageTaskItem* currentTask;
		i8 retryCount;
		u16 transactionCounter;
		u16 currentTransactionId;

		NewStorageEventListener* emptyHandler;

		//Starts or continues to execute flash tasks
		void ProcessQueue(bool continueCurrentTask);
		
		//Drops all task items belonging to a transaction after there was one fail and finally, calls the callback
		void AbortTransactionInProgress(NewStorageTaskItem* task, u8 errorCode);

		void AbortLastInsertedTransaction();

		void AbortTransactionInProgress(u16 transactionId, bool removeNext);

	public:

		//Initialize Storage class
		static NewStorage* getInstance(){
			if(!GS->newStorage){
				GS->newStorage = new NewStorage();
			}
			return GS->newStorage;
		}

		//Erases a page and calls the callback
		u32 ErasePage(u16 page, NewStorageEventListener* callback, u32 userType);

		//Erases multiple pages and then calls teh callback
		u32 ErasePages(u16 startPage, u16 numPages, NewStorageEventListener* callback, u32 userType);

		//Writes some data that must stay at its source until the callback is called (destination page must be empty)
		u32 WriteData(u32* source, u32* destination, u16 length, NewStorageEventListener* callback, u32 userType);

		//Caches the data in an internal buffer before saving (destination page must be empty)
		u32 CacheAndWriteData(u32* source, u32* destination, u16 length, NewStorageEventListener* callback, u32 userType);

		//Starts a transaction (multiple task items that must only execute together or fail at one point)
		u32 StartTransaction();

		//Ends a set of task items that belong together
		u32 EndTransaction(NewStorageEventListener* callback, u32 userType);

		//Allows us to cache some data in the queue until we need it later
		u32 CacheDataInTask(u8 * data, u16 dataLength, NewStorageEventListener * callback, u32 userType);

		//Return the number of tasks
		u16 GetNumberOfActiveTasks();

		//Very basic method to set a single handler that is notified when the queue is empty
		void SetQueueEmptyHandler(NewStorageEventListener * callback);

		//This system event handler must be called by the implementation
		void SystemEventHandler(u32 sys_evt);
};

class NewStorageEventListener
{
	public:
		NewStorageEventListener(){};

	virtual ~NewStorageEventListener(){};

	//Struct is passed by value so that it can be dequeued before calling this handler
	//If we passed a reference, this handler would have to clear the item from the TaskQueue
	virtual void NewStorageItemExecuted(NewStorageTaskItem* task, u8 errorCode) = 0;

	//Must always be called after NewStorageItemExecuted!
	virtual void NewStorageQueueEmptyHandler() {};
	
};


