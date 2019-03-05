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

#include <FlashStorage.h>
#include <Logger.h>

extern "C"
{
#include <app_error.h>
}

//TODO: NRF_BUSY and other errors of sd_flash_write should be handled
//TODO: callback should use task pointe rinsted of struct
//TODO: Is it necessary to provide a validation method after saving / erasing or does the softdevice already handle this?
//TODO: if a transaction is started, first item will be executed even if not all transaction items could be saved
//TODO: Does only support items with about 220 bytes because of PacketQueue element length limitation
//TODO: WriteData is only able to write multiples of words

FlashStorage::FlashStorage()
{
	taskQueue = nullptr;
	currentTask = nullptr;
	emptyHandler = nullptr;
	transactionCounter = 0;
	currentTransactionId = 0;
	retryCallingSoftdevice = false;

	//Initialize queue for queueing store and load tasks
	taskQueue = new PacketQueue(taskBuffer, FLASH_STORAGE_QUEUE_SIZE);
	
	retryCount = FLASH_STORAGE_RETRY_COUNT;
}


void FlashStorage::TimerEventHandler(u16 passedTimeDs)
{
	if(retryCallingSoftdevice){
		retryCallingSoftdevice = false;

		//Simulate flash error, as the softdevice was not ok with our call the last time, e.g. busy
		SystemEventHandler(NRF_EVT_FLASH_OPERATION_ERROR);
	}
}

FlashStorageError FlashStorage::ErasePage(u16 page, FlashStorageEventListener* callback, u32 userType)
{
	logt("FLASH", "Queue Erase Page %u", page);
	      

	if(page == 0){
		logt("FATAL", "WRONG PAGE ERASE");
		return FlashStorageError::WRONG_PARAM;
	}

	FlashStorageTaskItem task;
	task.header.command = FlashStorageCommand::ERASE_PAGE;
	task.header.transactionId = currentTransactionId;
	task.header.callback = callback;
	task.header.userType = userType;
	task.params.erasePage.page = page;

	if (!taskQueue->Put((u8*)&task, SIZEOF_FLASH_STORAGE_TASK_ITEM_ERASE_PAGE))
	{
		AbortLastInsertedTransaction();
		return FlashStorageError::QUEUE_FULL;
	}
	
	//Do not start processing until a transaction has been inserted fully
	if(currentTransactionId == 0) ProcessQueue(false);

	return FlashStorageError::SUCCESS;
}

FlashStorageError FlashStorage::ErasePages(u16 startPage, u16 numPages, FlashStorageEventListener* callback, u32 userType)
{
	logt("FLASH", "Queue Erase Pages %u (%u)", startPage, numPages);

	if(startPage == 0){
		logt("FATAL", "WRONG PAGE ERASE");
		return FlashStorageError::WRONG_PARAM;
	}

	FlashStorageTaskItem task;
	task.header.command = FlashStorageCommand::ERASE_PAGES;
	task.header.transactionId = currentTransactionId;
	task.header.callback = callback;
	task.header.userType = userType;
	task.params.erasePages.startPage = startPage;
	task.params.erasePages.numPages = numPages;

	if (!taskQueue->Put((u8*)&task, SIZEOF_FLASH_STORAGE_TASK_ITEM_ERASE_PAGES))
	{
		AbortLastInsertedTransaction();
		return FlashStorageError::QUEUE_FULL;
	}

	//Do not start processing until a transaction has been inserted fully
	if (currentTransactionId == 0) ProcessQueue(false);

	return FlashStorageError::SUCCESS;
}

FlashStorageError FlashStorage::WriteData(u32* source, u32* destination, u16 length, FlashStorageEventListener* callback, u32 userType)
{
	logt("FLASH", "Queue Write %u to %u (%u)", source, destination, length);

	FlashStorageTaskItem task;
	task.header.command = FlashStorageCommand::WRITE_DATA;
	task.header.transactionId = currentTransactionId;
	task.header.callback = callback;
	task.header.userType = userType;
	task.params.writeData.dataSource = source;
	task.params.writeData.dataDestination = destination;
	task.params.writeData.dataLength = length;

	if (!taskQueue->Put((u8*)&task, SIZEOF_FLASH_STORAGE_TASK_ITEM_WRITE_DATA))
	{
		AbortLastInsertedTransaction();
		return FlashStorageError::QUEUE_FULL;
	}

	//Do not start processing until a transaction has been inserted fully
	if (currentTransactionId == 0) ProcessQueue(false);

	return FlashStorageError::SUCCESS;
}

FlashStorageError FlashStorage::CacheAndWriteData(u32* source, u32* destination, u16 length, FlashStorageEventListener* callback, u32 userType)
{
	logt("FLASH", "Queue CachedWrite %u to %u (%u)", source, destination, length);

	if(length + SIZEOF_FLASH_STORAGE_TASK_ITEM_WRITE_CACHED_DATA > 250){
		return FlashStorageError::QUEUE_FULL;
	}

	//First, reserve space in the queue if possible
	u8 padding = (4-length%4)%4;

	u8* buffer = taskQueue->Reserve(SIZEOF_FLASH_STORAGE_TASK_ITEM_WRITE_CACHED_DATA + length + padding);

	logt("WARNING", "buffer %u", buffer);

	if (buffer != nullptr)
	{
		//Write data into reserved space
		FlashStorageTaskItem* task = (FlashStorageTaskItem*)buffer;
		task->header.command = FlashStorageCommand::WRITE_AND_CACHE_DATA;
		task->header.transactionId = currentTransactionId;
		task->header.callback = callback;
		task->header.userType = userType;
		task->params.writeCachedData.dataDestination = destination;
		task->params.writeCachedData.dataLength = length;
		memcpy(task->params.writeCachedData.data, source, length);
		memset(task->params.writeCachedData.data + length, 0xFF, padding);
	}
	else 
	{
		AbortLastInsertedTransaction();
		logt("ERROR", "aborted transaction");
		return FlashStorageError::QUEUE_FULL;
	}

	//Do not start processing until a transaction has been inserted fully
	if (currentTransactionId == 0) ProcessQueue(false);

	return FlashStorageError::SUCCESS;
}


FlashStorageError FlashStorage::StartTransaction()
{
	if (currentTransactionId != 0) return FlashStorageError::TRANSACTION_IN_PROGRESS;

	currentTransactionId = ++transactionCounter;
	if (currentTransactionId == 0) currentTransactionId = 1;

	return FlashStorageError::SUCCESS;
}

FlashStorageError FlashStorage::EndTransaction(FlashStorageEventListener* callback, u32 userType)
{
	if (currentTransactionId == 0) return FlashStorageError::TRANSACTION_IN_PROGRESS;

	logt("FLASH", "Queue EndTransaction");

	//Insert an end transaction taskitem so that we can register a callback at the end of our transaction
	FlashStorageTaskItemHeader task;
	task.command = FlashStorageCommand::END_TRANSACTION;
	task.transactionId = currentTransactionId;
	task.callback = callback;
	task.userType = userType;

	//Clear the currently used transactionid
	currentTransactionId = 0;

	if (!taskQueue->Put((u8*)&task, SIZEOF_FLASH_STORAGE_TASK_ITEM_HEADER))
	{
		AbortLastInsertedTransaction();
		return FlashStorageError::QUEUE_FULL;
	}

	//Process queue after the transaction end has been inserted
	ProcessQueue(false);

	return FlashStorageError::SUCCESS;
}

FlashStorageError FlashStorage::CacheDataInTask(u8* data, u16 dataLength, FlashStorageEventListener* callback, u32 userType)
{
	logt("FLASH", "Queue cache data");

	//First, reserve space in the queue if possible
	u16 taskLength = SIZEOF_FLASH_STORAGE_TASK_ITEM_DATA_CACHE + dataLength;
	u8* buffer = taskQueue->Reserve((u8)taskLength);

	if (buffer != nullptr)
	{
		//Write data into reserved space
		FlashStorageTaskItem* task = (FlashStorageTaskItem*)buffer;
		task->header.command = FlashStorageCommand::CACHE_DATA;
		task->header.transactionId = currentTransactionId;
		task->header.callback = callback;
		task->header.userType = userType;
		task->params.dataCache.dataLength = dataLength;
		memcpy(task->params.dataCache.data, data, dataLength);
	}
	else
	{
		AbortLastInsertedTransaction();
		return FlashStorageError::QUEUE_FULL;
	}

	//Do not start processing until a transaction has been inserted fully
	if (currentTransactionId == 0) ProcessQueue(false);

	return FlashStorageError::SUCCESS;
}

//Aborts the transaction in progress because of a flash fail
void FlashStorage::AbortTransactionInProgress(FlashStorageTaskItem* task, FlashStorageError errorCode) const
{
	AbortTransactionInProgress(task->header.transactionId, true);

	//Finally, call the callback of the failing task
	if (task->header.callback != nullptr) {
		task->header.callback->FlashStorageItemExecuted(task, errorCode);
	}
}

//Aborts the last transaction that was inserted into the queue
//used when the queue is full while inserting a transaction
//No callbacks are called because the transaction couldn't even start
void FlashStorage::AbortLastInsertedTransaction() const
{
	AbortTransactionInProgress(currentTransactionId, false);
}

void FlashStorage::AbortTransactionInProgress(u16 transactionId, bool removeNext) const
{
	if (transactionId != 0) {
		//Go through the next queue items and drop all those that belong to the same transaction
		while (true) {
			sizedData data;
			if (removeNext) data = taskQueue->PeekNext();
			else data = taskQueue->PeekLast();
			if (data.length != 0) {
				FlashStorageTaskItem* task = (FlashStorageTaskItem*)data.data;
				if (task->header.transactionId == transactionId) {
					//If the transaction has ended, we call the transaction end callback with the error
					if (task->header.command == FlashStorageCommand::END_TRANSACTION && task->header.callback != nullptr) {
						task->header.callback->FlashStorageItemExecuted(task, FlashStorageError::FLASH_OPERATION_TIMED_OUT);
					}
					if (removeNext) taskQueue->DiscardNext();
					else taskQueue->DiscardLast();

					if (taskQueue->_numElements == 0 && emptyHandler != nullptr) emptyHandler->FlashStorageQueueEmptyHandler();
				}
				else {
					break;
				}
			}
			else {
				break;
			}
		}
	}
}

void FlashStorage::ProcessQueue(bool continueCurrentTask)
{
	//When starting flash operations, we want to make sure that we do not get interrupted by the Watchdog
	FruityHal::FeedWatchdog();

	u32 err = 0xFFFFFFFFUL;

	//Do not execute next task if there is a task running or if there are no more tasks
	if((currentTask != nullptr && !continueCurrentTask) || taskQueue->_numElements < 1) return;

	//Get one item from the queue and execute it
	sizedData data = taskQueue->PeekNext();
	currentTask = (FlashStorageTaskItem*)data.data;

	logt("FLASH", "processing command %u", currentTask->header.command);

	if(currentTask->header.command == FlashStorageCommand::ERASE_PAGE)
	{
		if(currentTask->params.erasePage.page == 0){
			err = NRF_ERROR_FORBIDDEN;
			GS->logger->logError(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::FATAL_PROTECTED_PAGE_ERASE, 0);
		} else {
			err = sd_flash_page_erase(currentTask->params.erasePage.page);
		}
	}
	else if(currentTask->header.command == FlashStorageCommand::ERASE_PAGES)
	{
		while(currentTask->params.erasePages.numPages > 0)
		{
			u16 pageNum = currentTask->params.erasePages.startPage + currentTask->params.erasePages.numPages - 1;

			//Erasing a flash page takes 22ms, reading a flash page takes 140 us, we will therefore do a read first
			//To see if we really must erase the page
			u32 buffer = 0xFFFFFFFF;
			for(u32 i=0; i<NRF_FICR->CODEPAGESIZE; i+=4){
				buffer = buffer & *(u32*)(FLASH_REGION_START_ADDRESS + pageNum * NRF_FICR->CODEPAGESIZE + i);
			}

			//Flash page is already empty
			if(buffer == 0xFFFFFFFF){
				logt("FLASH", "page %u already erased", pageNum);
				currentTask->params.erasePages.numPages--;
				// => We continue with the loop and check the next page
				if(currentTask->params.erasePages.numPages == 0){
					//Call systemEventHandler when we are done so that the task is cleaned up
					SystemEventHandler(NRF_EVT_FLASH_OPERATION_SUCCESS);
					return;
				}

			} else {
				logt("FLASH", "erasing page %u", pageNum);
				if(pageNum == 0){
					err = NRF_ERROR_FORBIDDEN;
					GS->logger->logError(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::FATAL_PROTECTED_PAGE_ERASE, 1);
				} else {
					err = sd_flash_page_erase(pageNum);
					break;
				}
			}
		}
	}
	else if(currentTask->header.command == FlashStorageCommand::WRITE_DATA){
		FlashStorageTaskItemWriteData* params = &currentTask->params.writeData;

		logt("FLASH", "copy from %u to %u, length %u", params->dataSource, params->dataDestination, params->dataLength/4);

		err = sd_flash_write(params->dataDestination, params->dataSource, params->dataLength / 4); //FIXME: NRF_ERROR_BUSY and others not handeled
	}
	else if (currentTask->header.command == FlashStorageCommand::WRITE_AND_CACHE_DATA) {
		FlashStorageTaskItemWriteCachedData* params = &currentTask->params.writeCachedData;

		u8 padding = (4-params->dataLength%4)%4;

		logt("FLASH", "copy cached data to %u, length %u", params->dataDestination, params->dataLength);

		err = sd_flash_write(params->dataDestination, (u32*)params->data, (params->dataLength+padding) / 4); //FIXME: NRF_ERROR_BUSY and others not handeled
	}
	else if (currentTask->header.command == FlashStorageCommand::END_TRANSACTION) {

		logt("FLASH", "Transaction done");

		//We have to make a copy of the task and discard it before calling the callback to avoid problems in the simulator
		FlashStorageTaskItem taskCopy = *currentTask;
		taskQueue->DiscardNext();
		currentTask = nullptr;

		if (taskCopy.header.callback != nullptr) {
			taskCopy.header.callback->FlashStorageItemExecuted(&taskCopy, FlashStorageError::SUCCESS);
		}
		if (taskQueue->_numElements == 0 && emptyHandler != nullptr) emptyHandler->FlashStorageQueueEmptyHandler();
	}
	else if (currentTask->header.command == FlashStorageCommand::CACHE_DATA) {

		logt("FLASH", "Cached data");

		DYNAMIC_ARRAY(buffer, data.length);
		memcpy(buffer, data.data, data.length);
		FlashStorageTaskItem* taskCopy = (FlashStorageTaskItem*)buffer;

		taskQueue->DiscardNext();
		currentTask = nullptr;

		if (taskCopy->header.callback != nullptr) {
			taskCopy->header.callback->FlashStorageItemExecuted(taskCopy, FlashStorageError::SUCCESS);
		}
		if (taskQueue->_numElements == 0 && emptyHandler != nullptr) emptyHandler->FlashStorageQueueEmptyHandler();
	}
	else {
		logt("ERROR", "Wrong command %u", currentTask->header.command);
		taskQueue->DiscardNext();
		if (taskQueue->_numElements == 0 && emptyHandler != nullptr) emptyHandler->FlashStorageQueueEmptyHandler();
	}

	//SoftDevice either responded with SUCCESS or we did not query the SoftDevice at all
	if(err == NRF_SUCCESS || err == 0xFFFFFFFFUL){
		//do nothing
	}
	//If the call did not return success, we have to retry later (from the timer handler)
	else {
		logt("ERROR", "Flash operation returned %u", err);
		retryCallingSoftdevice = true;
	}
}

void FlashStorage::SetQueueEmptyHandler(FlashStorageEventListener* callback)
{
	emptyHandler = callback;
}

void FlashStorage::SystemEventHandler(u32 systemEvent)
{
	//This happens if another class requested a flash operation => Avoid that!
	if(currentTask == nullptr) return;

	//Make a copy so we can clear it from our queue before calling a listener
	sizedData data = taskQueue->PeekNext();
	DYNAMIC_ARRAY(buffer, data.length);
	memcpy(buffer, data.data, data.length);
	FlashStorageTaskItem* oldTaskReference = (FlashStorageTaskItem*)data.data;
	FlashStorageTaskItem* taskReference = (FlashStorageTaskItem*)buffer;

	if(systemEvent == NRF_EVT_FLASH_OPERATION_ERROR)
	{
		logt("ERROR", "Flash operation error");
		if(retryCount > 0)
		{
			//Decrement retry counter
			retryCount--;
			//Continue to process current task
			ProcessQueue(true);
			return;
		}
		else {
			//Discard task
			taskQueue->DiscardNext();

			//Reset currentTask and retryCount after the task was canceled
			currentTask = nullptr;
			retryCount = FLASH_STORAGE_RETRY_COUNT;

			//Abort transaction will now discard further tasks if the task belonged to a transaction
			AbortTransactionInProgress(taskReference, FlashStorageError::FLASH_OPERATION_TIMED_OUT);

			if (taskQueue->_numElements == 0 && emptyHandler != nullptr) emptyHandler->FlashStorageQueueEmptyHandler();
		}
	}
	else if(systemEvent == NRF_EVT_FLASH_OPERATION_SUCCESS)
	{
		//Reset retryCount if something succeeded
		retryCount = FLASH_STORAGE_RETRY_COUNT;

		logt("FLASH", "Flash operation success");
		if(
				taskReference->header.command == FlashStorageCommand::ERASE_PAGE
				|| taskReference->header.command == FlashStorageCommand::WRITE_DATA
				|| currentTask->header.command == FlashStorageCommand::WRITE_AND_CACHE_DATA
		){
			//Erase page command successful
			taskQueue->DiscardNext();
			currentTask = nullptr;
			if(taskReference->header.callback != nullptr) taskReference->header.callback->FlashStorageItemExecuted(taskReference, FlashStorageError::SUCCESS);
			if (taskQueue->_numElements == 0 && emptyHandler != nullptr) emptyHandler->FlashStorageQueueEmptyHandler();
		}
		else if(taskReference->header.command == FlashStorageCommand::ERASE_PAGES){

			//We must still erase some pages
			if(oldTaskReference->params.erasePages.numPages > 1){
				oldTaskReference->params.erasePages.numPages--;
				ProcessQueue(true);
				return;
			}
			//All pages erased
			else
			{
				logt("FLASH", "done");
				taskQueue->DiscardNext();
				currentTask = nullptr;
				if(taskReference->header.callback != nullptr) taskReference->header.callback->FlashStorageItemExecuted(taskReference, FlashStorageError::SUCCESS);
				if (taskQueue->_numElements == 0 && emptyHandler != nullptr) emptyHandler->FlashStorageQueueEmptyHandler();
			}
		}
	}

	//Process either the next item or the current one again (if it has not been discarded)
	ProcessQueue(false);
}

u16 FlashStorage::GetNumberOfActiveTasks() const
{
	return taskQueue->_numElements;
}
