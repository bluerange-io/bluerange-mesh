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

#include <FlashStorage.h>
#include <GlobalState.h>
#include <Logger.h>
#include "Utility.h"

//TODO: NRF_BUSY and other errors of FruityHal::FlashWrite should be handled
//TODO: callback should use task pointer instead of struct
//TODO: Is it necessary to provide a validation method after saving / erasing or does the softdevice already handle this?
//TODO: if a transaction is started, first item will be executed even if not all transaction items could be saved
//TODO: Does only support items with about 220 bytes because of PacketQueue element length limitation
//TODO: WriteData is only able to write multiples of words

FlashStorage::FlashStorage()
    :taskQueue(taskBuffer, FLASH_STORAGE_QUEUE_SIZE)
{
}


FlashStorage & FlashStorage::GetInstance()
{
    return GS->flashStorage;
}

void FlashStorage::TimerEventHandler(u16 passedTimeDs)
{
    if(retryCallingSoftdevice){
        retryCallingSoftdevice = false;

        //Simulate flash error, as the softdevice was not ok with our call the last time, e.g. busy
        SystemEventHandler(FruityHal::SystemEvents::FLASH_OPERATION_ERROR);
    }
}

FlashStorageError FlashStorage::ErasePage(u16 page, FlashStorageEventListener* callback, u32 userType, u32 extraInfo)
{
    return ErasePages(page, 1, callback, userType, extraInfo);
}

FlashStorageError FlashStorage::ErasePages(u16 startPage, u16 numPages, FlashStorageEventListener* callback, u32 userType, u32 extraInfo)
{
    logt("FLASH", "Queue Erase Pages %u (%u)", startPage, numPages);

    if(startPage == 0){
        logt("FATAL", "WRONG PAGE ERASE");        // LCOV_EXCL_LINE assertion
        SIMEXCEPTION(IllegalArgumentException); // LCOV_EXCL_LINE assertion
        return FlashStorageError::WRONG_PARAM;  // LCOV_EXCL_LINE assertion
    }

    FlashStorageTaskItem task;
    CheckedMemset(&task, 0, sizeof(FlashStorageTaskItem));
    task.header.command = FlashStorageCommand::ERASE_PAGES;
    task.header.callback = callback;
    task.header.userType = userType;
    task.header.extraInfo = extraInfo;
    task.params.erasePages.startPage = startPage;
    task.params.erasePages.numPages = numPages;

    if (!taskQueue.Put((u8*)&task, SIZEOF_FLASH_STORAGE_TASK_ITEM_ERASE_PAGES))
    {
        return FlashStorageError::QUEUE_FULL;
    }

    ProcessQueue(false);

    return FlashStorageError::SUCCESS;
}

FlashStorageError FlashStorage::WriteData(u32* source, u32* destination, u16 length, FlashStorageEventListener* callback, u32 userType, u32 extraInfo)
{
    logt("FLASH", "Queue Write %u to %u (%u)", (u32)source, (u32)destination, length);

    FlashStorageTaskItem task;
    CheckedMemset(&task, 0, sizeof(FlashStorageTaskItem));
    task.header.command = FlashStorageCommand::WRITE_DATA;
    task.header.callback = callback;
    task.header.userType = userType;
    task.header.extraInfo = extraInfo;
    task.params.writeData.dataSource = source;
    task.params.writeData.dataDestination = destination;
    task.params.writeData.dataLength = length;

    if (!taskQueue.Put((u8*)&task, SIZEOF_FLASH_STORAGE_TASK_ITEM_WRITE_DATA))
    {
        return FlashStorageError::QUEUE_FULL;
    }

    ProcessQueue(false);

    return FlashStorageError::SUCCESS;
}

FlashStorageError FlashStorage::CacheAndWriteData(u32 const * source, u32* destination, u16 length, FlashStorageEventListener* callback, u32 userType, u32 extraInfo)
{
    logt("FLASH", "Queue CachedWrite %u to %u (%u)", (u32)source, (u32)destination, length);

    // Items that are bigger than the half size of the queue are not guaranteed to fit into an empty queue.
    if(length + SIZEOF_FLASH_STORAGE_TASK_ITEM_WRITE_CACHED_DATA > FLASH_STORAGE_QUEUE_SIZE / 2){
        SIMEXCEPTION(DataToCacheTooBigException);
        return FlashStorageError::WRONG_PARAM;
    }

    u8* buffer = taskQueue.Reserve(SIZEOF_FLASH_STORAGE_TASK_ITEM_WRITE_CACHED_DATA + length);

    if (buffer != nullptr)
    {
        //Write data into reserved space
        FlashStorageTaskItem* task = (FlashStorageTaskItem*)buffer;
        task->header.command = FlashStorageCommand::WRITE_AND_CACHE_DATA;
        task->header.callback = callback;
        task->header.userType = userType;
        task->header.extraInfo = extraInfo;
        task->params.writeCachedData.dataDestination = destination;
        task->params.writeCachedData.dataLength = length;
        CheckedMemcpy(task->params.writeCachedData.data, source, length);
    }
    else 
    {
        logt("ERROR", "aborted transaction");
        GS->logger.LogCustomError(CustomErrorTypes::FATAL_ABORTED_FLASH_TRANSACTION, 0);

        return FlashStorageError::QUEUE_FULL;
    }

    ProcessQueue(false);

    return FlashStorageError::SUCCESS;
}

//Aborts the transaction in progress because of a flash fail
void FlashStorage::AbortTransactionInProgress(FlashStorageError errorCode)
{
    //Finally, call the callback of the failing task
    if (currentTask->header.callback != nullptr) {
        currentTask->header.callback->FlashStorageItemExecuted(currentTask, errorCode);
    }

    RemoveExecutingTask();
}

void FlashStorage::RemoveExecutingTask()
{
    currentTask = nullptr;
    taskQueue.DiscardNext();
    if (taskQueue._numElements == 0) GS->recordStorage.FlashStorageQueueEmptyHandler();
}

void FlashStorage::OnCommandSuccessful()
{
    if (currentTask->header.callback != nullptr) currentTask->header.callback->FlashStorageItemExecuted(currentTask, FlashStorageError::SUCCESS);
    RemoveExecutingTask();
}

void FlashStorage::ProcessQueue(bool continueCurrentTask)
{
    //When starting flash operations, we want to make sure that we do not get interrupted by the Watchdog
    FruityHal::FeedWatchdog();

    ErrorType err = ErrorType::SUCCESS;

    //Do not execute next task if there is a task running or if there are no more tasks
    if((currentTask != nullptr && !continueCurrentTask) || taskQueue._numElements < 1) return;

    //Get one item from the queue and execute it
    SizedData data = taskQueue.PeekNext();
    currentTask = (FlashStorageTaskItem*)data.data;

    logt("FLASH", "processing command %u", (u32)currentTask->header.command);

    if(currentTask->header.command == FlashStorageCommand::ERASE_PAGES)
    {
        while(currentTask->params.erasePages.numPages > 0)
        {
            u16 pageNum = currentTask->params.erasePages.startPage + currentTask->params.erasePages.numPages - 1;

            if (pageNum == 0) {
                GS->logger.LogCustomError(CustomErrorTypes::FATAL_PROTECTED_PAGE_ERASE, 1);
                RemoveExecutingTask();
                SIMEXCEPTION(IllegalStateException);
                return;
            }

            //Erasing a flash page takes 22ms, reading a flash page takes 140 us, we will therefore do a read first
            //To see if we really must erase the page
            u32 buffer = 0xFFFFFFFF;
            for(u32 i=0; i< FruityHal::GetCodePageSize(); i+=sizeof(u32)){
                buffer = buffer & *(u32*)(FLASH_REGION_START_ADDRESS + pageNum * FruityHal::GetCodePageSize() + i);
            }

            //Flash page is already empty
            if(buffer == 0xFFFFFFFF){
                logt("FLASH", "page %u already erased", pageNum);
                currentTask->params.erasePages.numPages--;
                // => We continue with the loop and check the next page
                if(currentTask->params.erasePages.numPages == 0){
                    //Call systemEventHandler when we are done so that the task is cleaned up
                    SystemEventHandler(FruityHal::SystemEvents::FLASH_OPERATION_SUCCESS);
                    return;
                }
            } else {
                logt("FLASH", "erasing page %u", pageNum);
                err = FruityHal::FlashPageErase(pageNum);
                break;
            }
        }
    }
    else if (currentTask->header.command == FlashStorageCommand::WRITE_DATA) {
        FlashStorageTaskItemWriteData* params = &currentTask->params.writeData;

        logt("FLASH", "copy from %u to %u, length %u", (u32)params->dataSource, (u32)params->dataDestination, params->dataLength / 4);

        err = FruityHal::FlashWrite(params->dataDestination, params->dataSource, params->dataLength / 4); //FIXME: NRF_ERROR_BUSY and others not handeled
    }
    else if (currentTask->header.command == FlashStorageCommand::WRITE_AND_CACHE_DATA) {
        FlashStorageTaskItemWriteCachedData* params = &currentTask->params.writeCachedData;

        u8 padding = (4-params->dataLength%4)%4;

        logt("FLASH", "copy cached data to %u, length %u", (u32)params->dataDestination, params->dataLength);

        err = FruityHal::FlashWrite(params->dataDestination, (u32*)params->data, (params->dataLength+padding) / 4); //FIXME: NRF_ERROR_BUSY and others not handeled
    }
    else {
        logt("ERROR", "Wrong command %u", (u32)currentTask->header.command);
        GS->logger.LogCustomError(CustomErrorTypes::FATAL_WRONG_FLASH_STORAGE_COMMAND, (u16)currentTask->header.command);
        RemoveExecutingTask();
        SIMEXCEPTION(IllegalArgumentException);
    }

    //If the call did not return success, we have to retry later (from the timer handler)
    if(err != ErrorType::SUCCESS) {
        logt("ERROR", "Flash operation returned %u", (u32)err);
        retryCallingSoftdevice = true;
    }
}

void FlashStorage::SystemEventHandler(FruityHal::SystemEvents systemEvent)
{
    //This happens if another class requested a flash operation => Avoid that!
    if (currentTask == nullptr)
    {
        //jstodocurrent Add DebugLog. There is another MR on the way that already adds another DebugLog though. So better wait for
        //it to be merged to avoid unnecessary merge conflicts.
        SIMEXCEPTION(IllegalStateException);
        return;
    }

    if(systemEvent == FruityHal::SystemEvents::FLASH_OPERATION_ERROR)
    {
        logt("WARNING", "Flash operation error");
        GS->logger.LogCustomCount(CustomErrorTypes::COUNT_FLASH_OPERATION_ERROR);

        if(retryCount > 0)
        {
            //Decrement retry counter
            retryCount--;
            //Continue to process current task
            ProcessQueue(true);
            return;
        }
        else {
            //Reset currentTask and retryCount after the task was canceled
            retryCount = FLASH_STORAGE_RETRY_COUNT;

            //Abort transaction will now discard further tasks if the task belonged to a transaction
            AbortTransactionInProgress(FlashStorageError::FLASH_OPERATION_TIMED_OUT);
        }
    }
    else if(systemEvent == FruityHal::SystemEvents::FLASH_OPERATION_SUCCESS)
    {
        //Reset retryCount if something succeeded
        retryCount = FLASH_STORAGE_RETRY_COUNT;

        logt("FLASH", "Flash operation success");
        if(
                   currentTask->header.command == FlashStorageCommand::WRITE_DATA
                || currentTask->header.command == FlashStorageCommand::WRITE_AND_CACHE_DATA
        ){
            OnCommandSuccessful();
        }
        else if(currentTask->header.command == FlashStorageCommand::ERASE_PAGES){

            //We must still erase some pages
            if(currentTask->params.erasePages.numPages > 1){
                currentTask->params.erasePages.numPages--;
                ProcessQueue(true);
                return;
            }
            //All pages erased
            else
            {
                OnCommandSuccessful();
            }
        }
    }

    //Process either the next item or the current one again (if it has not been discarded)
    ProcessQueue(false);
}

u16 FlashStorage::GetNumberOfActiveTasks() const
{
    return taskQueue._numElements;
}
