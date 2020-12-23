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

#pragma once

#include <PacketQueue.h>
#include <FruityHal.h>

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
    WRITE_DATA,    //While unsafe by design, the command is still required for massive writes, e.g. the Bootloadersettings.
    WRITE_AND_CACHE_DATA,
    ERASE_PAGES,
};

//Packed, because it might get misaligned in the queue (should be fixed)
//To dodge HardFaults, it has been packed so that the compiler will now have to deal with it.
#pragma pack(push)
#pragma pack(1)

constexpr int SIZEOF_FLASH_STORAGE_TASK_ITEM_HEADER = 16;
struct FlashStorageTaskItemHeader
{
    FlashStorageCommand command;
    u8 reserved[3];
    FlashStorageEventListener* callback;
    u32 userType;
    u32 extraInfo;
};
STATIC_ASSERT_SIZE(FlashStorageTaskItemHeader, SIZEOF_FLASH_STORAGE_TASK_ITEM_HEADER);

constexpr int SIZEOF_FLASH_STORAGE_TASK_ITEM_WRITE_DATA = (SIZEOF_FLASH_STORAGE_TASK_ITEM_HEADER + 10);
struct FlashStorageTaskItemWriteData
{
    u32* dataSource;
    u32* dataDestination;
    u16 dataLength;
};
STATIC_ASSERT_SIZE(FlashStorageTaskItemWriteData, 10);

constexpr int SIZEOF_FLASH_STORAGE_TASK_ITEM_WRITE_CACHED_DATA = (SIZEOF_FLASH_STORAGE_TASK_ITEM_HEADER + 8);
struct FlashStorageTaskItemWriteCachedData
{
    u32* dataDestination;
    u16 dataLength;
    u16 reserved;
    u8 data[1];
};
//We should pay attention that the data pointer is saved at a word aligned address so we can directly write to flash from this pointer
static_assert(offsetof(FlashStorageTaskItemWriteCachedData, data) % sizeof(u32) == 0, "Payload offset must be word aligned.");
STATIC_ASSERT_SIZE(FlashStorageTaskItemWriteCachedData, 9);

constexpr int SIZEOF_FLASH_STORAGE_TASK_ITEM_ERASE_PAGES = (SIZEOF_FLASH_STORAGE_TASK_ITEM_HEADER + 4);
struct FlashStorageTaskItemErasePages
{
    u16 startPage;
    u16 numPages;
};
STATIC_ASSERT_SIZE(FlashStorageTaskItemErasePages, 4);


//TODO: A compact version will only need 1 byte command, 1 byte transactionId, 
struct FlashStorageTaskItem
{
    FlashStorageTaskItemHeader header;
    union
    {
        FlashStorageTaskItemWriteData writeData;
        FlashStorageTaskItemWriteCachedData writeCachedData;
        FlashStorageTaskItemErasePages erasePages;
    } params;
};
#pragma pack(pop)


constexpr int FLASH_STORAGE_RETRY_COUNT = 10;
constexpr int FLASH_STORAGE_QUEUE_SIZE = 2048;

/*
 * This Storage class provides easy access to all storage operations
 * in persistent flash memory. It offers the possibilities to either store
 * data from some memory location but can also buffer the data until it is
 * stored for easier usage (asynchronously).
 */
class FlashStorage
{
    private:
                
        u32 taskBuffer[FLASH_STORAGE_QUEUE_SIZE / sizeof(u32)] = {};
        PacketQueue taskQueue;

        FlashStorageTaskItem* currentTask = nullptr;
        i8 retryCount = 0;
        bool retryCallingSoftdevice = false;

        //Starts or continues to execute flash tasks
        void ProcessQueue(bool continueCurrentTask);
        
        //Drops all task items belonging to a transaction after there was one fail and finally, calls the callback
        void AbortTransactionInProgress(FlashStorageError errorCode);

        void RemoveExecutingTask();
        void OnCommandSuccessful();

    public:
        FlashStorage();

        //Initialize Storage class
        static FlashStorage& GetInstance();

        void TimerEventHandler(u16 passedTimeDs);

        //Erases a page and calls the callback
        FlashStorageError ErasePage(u16 page, FlashStorageEventListener* callback, u32 userType, u32 extraInfo = 0);

        //Erases multiple pages and then calls teh callback
        FlashStorageError ErasePages(u16 startPage, u16 numPages, FlashStorageEventListener* callback, u32 userType, u32 extraInfo = 0);

        //Writes some data that must stay at its source until the callback is called (destination page must be empty)
        FlashStorageError WriteData(u32* source, u32* destination, u16 length, FlashStorageEventListener* callback, u32 userType, u32 extraInfo = 0);

        //Caches the data in an internal buffer before saving (destination page must be empty)
        FlashStorageError CacheAndWriteData(u32 const * source, u32* destination, u16 length, FlashStorageEventListener* callback, u32 userType, u32 extraInfo = 0);

        //Return the number of tasks
        u16 GetNumberOfActiveTasks() const;

        //This system event handler must be called by the implementation
        void SystemEventHandler(FruityHal::SystemEvents sys_evt);
};

class FlashStorageEventListener
{
public:
    FlashStorageEventListener() {};

    virtual ~FlashStorageEventListener() {};

    //Struct is passed by value so that it can be dequeued before calling this handler
    //If we passed a reference, this handler would have to clear the item from the TaskQueue
    virtual void FlashStorageItemExecuted(FlashStorageTaskItem* task, FlashStorageError errorCode) = 0;
};


