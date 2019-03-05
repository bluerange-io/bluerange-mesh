////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2019 M-Way Solutions GmbH
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

/*
 * This Storage class should provide easy access to all storage functionality
 * */

#pragma once

#include <GlobalState.h>
#include <PacketQueue.h>

extern "C"{
#include <ble.h>
}

class FlashStorageEventListener;

enum class FlashStorageError : u8 {
	SUCCESS,
	QUEUE_FULL,
	FLASH_OPERATION_TIMED_OUT,
	TRANSACTION_IN_PROGRESS,
	WRONG_PARAM
};
enum class FlashStorageCommand : u8 {
	NONE,
	WRITE_DATA,
	WRITE_AND_CACHE_DATA,
	ERASE_PAGE,
	ERASE_PAGES,
	END_TRANSACTION,
	CACHE_DATA
};

//Packed, because it might get misaligned in the queue (should be fixed)
//To dodge HardFaults, it has been packed so that the compiler will now have to deal with it.
#pragma pack(push)
#pragma pack(1)

#define SIZEOF_FLASH_STORAGE_TASK_ITEM_HEADER 12
struct FlashStorageTaskItemHeader
{
	FlashStorageCommand command;
	u8 reserved;
	u16 transactionId;
	FlashStorageEventListener* callback;
	u32 userType;
};
STATIC_ASSERT_SIZE(FlashStorageTaskItemHeader, 12);

#define SIZEOF_FLASH_STORAGE_TASK_ITEM_WRITE_DATA (SIZEOF_FLASH_STORAGE_TASK_ITEM_HEADER + 10)
struct FlashStorageTaskItemWriteData
{
		u32* dataSource;
		u32* dataDestination;
		u16 dataLength;

};
STATIC_ASSERT_SIZE(FlashStorageTaskItemWriteData, 10);

//We should pay attention that the data pointer is saved at a word aligned address so we can directly write to flash from this pointer
#define SIZEOF_FLASH_STORAGE_TASK_ITEM_WRITE_CACHED_DATA (SIZEOF_FLASH_STORAGE_TASK_ITEM_HEADER + 8)
struct FlashStorageTaskItemWriteCachedData
{
	u32* dataDestination;
	u16 dataLength;
	u16 reserved;
	u8 data[1];

};
STATIC_ASSERT_SIZE(FlashStorageTaskItemWriteCachedData, 9);

#define SIZEOF_FLASH_STORAGE_TASK_ITEM_ERASE_PAGE (SIZEOF_FLASH_STORAGE_TASK_ITEM_HEADER + 2)
struct FlashStorageTaskItemErasePage
{
		u16 page;

};
STATIC_ASSERT_SIZE(FlashStorageTaskItemErasePage, 2);

#define SIZEOF_FLASH_STORAGE_TASK_ITEM_ERASE_PAGES (SIZEOF_FLASH_STORAGE_TASK_ITEM_HEADER + 4)
struct FlashStorageTaskItemErasePages
{
	u16 startPage;
	u16 numPages;

};
STATIC_ASSERT_SIZE(FlashStorageTaskItemErasePages, 4)

//The data cache is useful in a multi-task transaction where data has to be cached for later and other tasks have to execute first
#define SIZEOF_FLASH_STORAGE_TASK_ITEM_DATA_CACHE (SIZEOF_FLASH_STORAGE_TASK_ITEM_HEADER + 4)
struct FlashStorageTaskItemDataCache
{
	u16 dataLength;
	u16 reserved;
	u8 data[1];

};
STATIC_ASSERT_SIZE(FlashStorageTaskItemDataCache, 5);


//TODO: A compact version will only need 1 byte command, 1 byte transactionId, 
struct FlashStorageTaskItem
{
	FlashStorageTaskItemHeader header;
	union
	{
		FlashStorageTaskItemDataCache dataCache;
		FlashStorageTaskItemWriteData writeData;
		FlashStorageTaskItemWriteCachedData writeCachedData;
		FlashStorageTaskItemErasePage erasePage;
		FlashStorageTaskItemErasePages erasePages;
	} params;
};
#pragma pack(pop)





#define FLASH_STORAGE_RETRY_COUNT 3
#define FLASH_STORAGE_QUEUE_SIZE 256

class FlashStorage
{
	private:
		FlashStorage();
				
		u32 taskBuffer[FLASH_STORAGE_QUEUE_SIZE/sizeof(u32)];
		PacketQueue* taskQueue;

		FlashStorageTaskItem* currentTask;
		i8 retryCount;
		u16 transactionCounter;
		u16 currentTransactionId;
		bool retryCallingSoftdevice;

		FlashStorageEventListener* emptyHandler;

		//Starts or continues to execute flash tasks
		void ProcessQueue(bool continueCurrentTask);
		
		//Drops all task items belonging to a transaction after there was one fail and finally, calls the callback
		void AbortTransactionInProgress(FlashStorageTaskItem* task, FlashStorageError errorCode) const;

		void AbortLastInsertedTransaction() const;

		void AbortTransactionInProgress(u16 transactionId, bool removeNext) const;

	public:

		//Initialize Storage class
		static FlashStorage& getInstance(){
			if(!GS->flashStorage){
				GS->flashStorage = new FlashStorage();
			}
			return *(GS->flashStorage);
		}

		void TimerEventHandler(u16 passedTimeDs);

		//Erases a page and calls the callback
		FlashStorageError ErasePage(u16 page, FlashStorageEventListener* callback, u32 userType);

		//Erases multiple pages and then calls teh callback
		FlashStorageError ErasePages(u16 startPage, u16 numPages, FlashStorageEventListener* callback, u32 userType);

		//Writes some data that must stay at its source until the callback is called (destination page must be empty)
		FlashStorageError WriteData(u32* source, u32* destination, u16 length, FlashStorageEventListener* callback, u32 userType);

		//Caches the data in an internal buffer before saving (destination page must be empty)
		FlashStorageError CacheAndWriteData(u32* source, u32* destination, u16 length, FlashStorageEventListener* callback, u32 userType);

		//Starts a transaction (multiple task items that must only execute together or fail at one point)
		FlashStorageError StartTransaction();

		//Ends a set of task items that belong together
		FlashStorageError EndTransaction(FlashStorageEventListener* callback, u32 userType);

		//Allows us to cache some data in the queue until we need it later
		FlashStorageError CacheDataInTask(u8 * data, u16 dataLength, FlashStorageEventListener * callback, u32 userType);

		//Return the number of tasks
		u16 GetNumberOfActiveTasks() const;

		//Very basic method to set a single handler that is notified when the queue is empty
		void SetQueueEmptyHandler(FlashStorageEventListener * callback);

		//This system event handler must be called by the implementation
		void SystemEventHandler(u32 sys_evt);
};

class FlashStorageEventListener
{
	public:
		FlashStorageEventListener(){};

	virtual ~FlashStorageEventListener(){};

	//Struct is passed by value so that it can be dequeued before calling this handler
	//If we passed a reference, this handler would have to clear the item from the TaskQueue
	virtual void FlashStorageItemExecuted(FlashStorageTaskItem* task, FlashStorageError errorCode) = 0;

	//Must always be called after FlashStorageItemExecuted!
	virtual void FlashStorageQueueEmptyHandler() {};
	
};


