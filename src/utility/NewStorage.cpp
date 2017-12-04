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


#include <NewStorage.h>
#include <Logger.h>

extern "C"
{
#include <app_error.h>
}

//TODO: NRF_BUSY and other errors of sd_flash_write should be handled
//TODO: callback should use task pointe rinsted of struct
//TODO: Is it necessary to provide a validation method after saving / erasing or does the softdevice already handle this?
//TODO: if a transaction is started, first item will be executed even if not all transaction items could be saved

NewStorage::NewStorage()
{
	taskQueue = NULL;
	currentTask = NULL;
	emptyHandler = NULL;
	transactionCounter = 0;
	currentTransactionId = 0;

	//Initialize queue for queueing store and load tasks
	taskQueue = new PacketQueue(taskBuffer, NEW_STORAGE_QUEUE_SIZE);
	
	retryCount = NEW_STORAGE_RETRY_COUNT;

}

u32 NewStorage::ErasePage(u16 page, NewStorageEventListener* callback, u32 userType)
{
	logt("NEWSTORAGE", "Queue Erase Page %u", page);

	NewStorageTaskItem task;
	task.command = NewStorageCommand::ERASE_PAGE;
	task.transactionId = currentTransactionId;
	task.callback = callback;
	task.userType = userType;
	task.params.erasePage.page = page;

	if (!taskQueue->Put((u8*)&task, SIZEOF_NEW_STORAGE_TASK_ITEM_ERASE_PAGE))
	{
		AbortLastInsertedTransaction();
		return NewStorageError::QUEUE_FULL;
	}
	
	//Do not start processing until a transaction has been inserted fully
	if(currentTransactionId == 0) ProcessQueue(false);

	return NewStorageError::SUCCESS;
}

u32 NewStorage::ErasePages(u16 startPage, u16 numPages, NewStorageEventListener* callback, u32 userType)
{
	logt("NEWSTORAGE", "Queue Erase Pages %u (%u)", startPage, numPages);

	NewStorageTaskItem task;
	task.command = NewStorageCommand::ERASE_PAGES;
	task.transactionId = currentTransactionId;
	task.callback = callback;
	task.userType = userType;
	task.params.erasePages.startPage = startPage;
	task.params.erasePages.numPages = numPages;

	if (!taskQueue->Put((u8*)&task, SIZEOF_NEW_STORAGE_TASK_ITEM_ERASE_PAGES))
	{
		AbortLastInsertedTransaction();
		return NewStorageError::QUEUE_FULL;
	}

	//Do not start processing until a transaction has been inserted fully
	if (currentTransactionId == 0) ProcessQueue(false);

	return NewStorageError::SUCCESS;
}

u32 NewStorage::WriteData(u32* source, u32* destination, u16 length, NewStorageEventListener* callback, u32 userType)
{
	logt("NEWSTORAGE", "Queue Write %u to %u (%u)", source, destination, length);

	NewStorageTaskItem task;
	task.command = NewStorageCommand::WRITE_DATA;
	task.transactionId = currentTransactionId;
	task.callback = callback;
	task.userType = userType;
	task.params.writeData.dataSource = source;
	task.params.writeData.dataDestination = destination;
	task.params.writeData.dataLength = length;

	if (!taskQueue->Put((u8*)&task, SIZEOF_NEW_STORAGE_TASK_ITEM_WRITE_DATA))
	{
		AbortLastInsertedTransaction();
		return NewStorageError::QUEUE_FULL;
	}

	//Do not start processing until a transaction has been inserted fully
	if (currentTransactionId == 0) ProcessQueue(false);

	return NewStorageError::SUCCESS;
}

u32 NewStorage::CacheAndWriteData(u32* source, u32* destination, u16 length, NewStorageEventListener* callback, u32 userType)
{
	logt("NEWSTORAGE", "Queue CachedWrite %u to %u (%u)", source, destination, length);

	//First, reserve space in the queue if possible
	u8* buffer = taskQueue->Reserve(SIZEOF_NEW_STORAGE_TASK_ITEM_WRITE_CACHED_DATA + length);

	if (buffer != NULL)
	{
		//Write data into reserved space
		NewStorageTaskItem* task = (NewStorageTaskItem*)buffer;
		task->command = NewStorageCommand::WRITE_AND_CACHE_DATA;
		task->transactionId = currentTransactionId;
		task->callback = callback;
		task->userType = userType;
		task->params.writeCachedData.dataDestination = destination;
		task->params.writeCachedData.dataLength = length;
		memcpy(task->params.writeCachedData.data, source, length);
	}
	else 
	{
		AbortLastInsertedTransaction();
		return NewStorageError::QUEUE_FULL;
	}

	//Do not start processing until a transaction has been inserted fully
	if (currentTransactionId == 0) ProcessQueue(false);

	return NewStorageError::SUCCESS;
}


u32 NewStorage::StartTransaction()
{
	if (currentTransactionId != 0) return NewStorageError::TRANSACTION_IN_PROGRESS;

	currentTransactionId = ++transactionCounter;
	if (currentTransactionId == 0) currentTransactionId = 1;

	return NewStorageError::SUCCESS;
}

u32 NewStorage::EndTransaction(NewStorageEventListener* callback, u32 userType)
{
	if (currentTransactionId == 0) return NewStorageError::TRANSACTION_IN_PROGRESS;

	logt("NEWSTORAGE", "Queue EndTransaction");

	//Insert an end transaction taskitem so that we can register a callback at the end of our transaction
	NewStorageTaskItem task;
	task.command = NewStorageCommand::END_TRANSACTION;
	task.transactionId = currentTransactionId;
	task.callback = callback;
	task.userType = userType;

	//Clear the currently used transactionid
	currentTransactionId = 0;

	if (!taskQueue->Put((u8*)&task, SIZEOF_NEW_STORAGE_TASK_ITEM_END_TRANSACTION))
	{
		AbortLastInsertedTransaction();
		return NewStorageError::QUEUE_FULL;
	}

	//Process queue after the transaction end has been inserted
	ProcessQueue(false);

	return NewStorageError::SUCCESS;
}

u32 NewStorage::CacheDataInTask(u8* data, u16 dataLength, NewStorageEventListener* callback, u32 userType)
{
	logt("NEWSTORAGE", "Queue cache data");

	//First, reserve space in the queue if possible
	u16 taskLength = SIZEOF_NEW_STORAGE_TASK_ITEM_DATA_CACHE + dataLength;
	u8* buffer = taskQueue->Reserve((u8)taskLength);

	if (buffer != NULL)
	{
		//Write data into reserved space
		NewStorageTaskItem* task = (NewStorageTaskItem*)buffer;
		task->command = NewStorageCommand::CACHE_DATA;
		task->transactionId = currentTransactionId;
		task->callback = callback;
		task->userType = userType;
		task->params.dataCache.dataLength = dataLength;
		memcpy(task->params.dataCache.data, data, dataLength);
	}
	else
	{
		AbortLastInsertedTransaction();
		return NewStorageError::QUEUE_FULL;
	}

	//Do not start processing until a transaction has been inserted fully
	if (currentTransactionId == 0) ProcessQueue(false);

	return NewStorageError::SUCCESS;
}

//Aborts the transaction in progress because of a flash fail
void NewStorage::AbortTransactionInProgress(NewStorageTaskItem* task, u8 errorCode)
{
	AbortTransactionInProgress(task->transactionId, true);

	//Finally, call the callback of the failing task
	if (task->callback != NULL) {
		task->callback->NewStorageItemExecuted(task, errorCode);
	}
}

//Aborts the last transaction that was inserted into the queue
//used when the queue is full while inserting a transaction
//No callbacks are called because the transaction couldn't even start
void NewStorage::AbortLastInsertedTransaction()
{
	AbortTransactionInProgress(currentTransactionId, false);
}

void NewStorage::AbortTransactionInProgress(u16 transactionId, bool removeNext)
{
	if (transactionId != 0) {
		//Go through the next queue items and drop all those that belong to the same transaction
		while (true) {
			sizedData data;
			if (removeNext) data = taskQueue->PeekNext();
			else data = taskQueue->PeekLast();
			if (data.length != 0) {
				NewStorageTaskItem* task = (NewStorageTaskItem*)data.data;
				if (task->transactionId == transactionId) {
					//If the transaction has ended, we call the transaction end callback with the error
					if (task->command == NewStorageCommand::END_TRANSACTION && task->callback != NULL) {
						task->callback->NewStorageItemExecuted(task, NewStorageError::FLASH_OPERATION_TIMED_OUT);
					}
					if (removeNext) taskQueue->DiscardNext();
					else taskQueue->DiscardLast();

					if (taskQueue->_numElements == 0 && emptyHandler != NULL) emptyHandler->NewStorageQueueEmptyHandler();
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

void NewStorage::ProcessQueue(bool continueCurrentTask)
{
	//When starting flash operations, we want to make sure that we do not get interrupted by the Watchdog
	FruityHal::FeedWatchdog();

	u32 err = 0;

	//Do not execute next task if there is a task running or if there are no more tasks
	if((currentTask != NULL && !continueCurrentTask) || taskQueue->_numElements < 1) return;

	//Get one item from the queue and execute it
	sizedData data = taskQueue->PeekNext();
	currentTask = (NewStorageTaskItem*)data.data;

	logt("NEWSTORAGE", "processing command %u", currentTask->command);

	if(currentTask->command == NewStorageCommand::ERASE_PAGE)
	{
		err = sd_flash_page_erase(currentTask->params.erasePage.page);
		APP_ERROR_CHECK(err); //FIXME: NRF_ERROR_BUSY and others not handeled
	}
	else if(currentTask->command == NewStorageCommand::ERASE_PAGES)
	{
		u16 pageNum = currentTask->params.erasePages.startPage + currentTask->params.erasePages.numPages - 1;

		//Erasing a flash page takes 22ms, reading a flash page takes 140 us, we will therefore do a read first
		//To see if we really must erase the page
		u32 buffer = 0xFFFFFFFF;
		for(u32 i=0; i<NRF_FICR->CODEPAGESIZE; i+=4){
			buffer = buffer & *(u32*)(pageNum*NRF_FICR->CODEPAGESIZE+i);
		}

		//Flash page is already empty
		if(buffer == 0xFFFFFFFF){
			logt("NEWSTORAGE", "page %u already erased", pageNum);
			//Pretend that the flash operation has happened
			SystemEventHandler(NRF_EVT_FLASH_OPERATION_SUCCESS);
		} else {
			logt("NEWSTORAGE", "erasing page %u", pageNum);
			err = sd_flash_page_erase(pageNum);
			APP_ERROR_CHECK(err); //FIXME: NRF_ERROR_BUSY and others not handeled
		}
	}
	else if(currentTask->command == NewStorageCommand::WRITE_DATA){
		NewStorageTaskItemWriteData* params = &currentTask->params.writeData;

		logt("NEWSTORAGE", "copy from %u to %u, length %u", params->dataSource, params->dataDestination, params->dataLength/4);

		err = sd_flash_write(params->dataDestination, params->dataSource, params->dataLength / 4); //FIXME: NRF_ERROR_BUSY and others not handeled
		APP_ERROR_CHECK(err);
	}
	else if (currentTask->command == NewStorageCommand::WRITE_AND_CACHE_DATA) {
		NewStorageTaskItemWriteCachedData* params = &currentTask->params.writeCachedData;

		logt("NEWSTORAGE", "copy cached data to %u, length %u", params->dataDestination, params->dataLength);

		err = sd_flash_write(params->dataDestination, (u32*)params->data, params->dataLength / 4); //FIXME: NRF_ERROR_BUSY and others not handeled
		APP_ERROR_CHECK(err);
	}
	else if (currentTask->command == NewStorageCommand::END_TRANSACTION) {

		logt("NEWSTORAGE", "Transaction done");

		//We have to make a copy of the task and discard it before calling the callback to avoid problems in the simulator
		NewStorageTaskItem taskCopy = *currentTask;
		NewStorageTaskItemEndTransaction* params = &currentTask->params.endTransaction;
		taskQueue->DiscardNext();
		currentTask = NULL;

		if (taskCopy.callback != NULL) {
			taskCopy.callback->NewStorageItemExecuted(&taskCopy, NewStorageError::SUCCESS);
		}
		if (taskQueue->_numElements == 0 && emptyHandler != NULL) emptyHandler->NewStorageQueueEmptyHandler();
	}
	else if (currentTask->command == NewStorageCommand::CACHE_DATA) {

		logt("NEWSTORAGE", "Cached data");

		DYNAMIC_ARRAY(buffer, data.length);
		memcpy(buffer, data.data, data.length);
		NewStorageTaskItem* taskCopy = (NewStorageTaskItem*)buffer;

		taskQueue->DiscardNext();
		currentTask = NULL;

		if (taskCopy->callback != NULL) {
			taskCopy->callback->NewStorageItemExecuted(taskCopy, NewStorageError::SUCCESS);
		}
		if (taskQueue->_numElements == 0 && emptyHandler != NULL) emptyHandler->NewStorageQueueEmptyHandler();
	}
	else {
		logt("ERROR", "Wrong command %u", currentTask->command);
		taskQueue->DiscardNext();
		if (taskQueue->_numElements == 0 && emptyHandler != NULL) emptyHandler->NewStorageQueueEmptyHandler();
	}
}

void NewStorage::SetQueueEmptyHandler(NewStorageEventListener* callback)
{
	emptyHandler = callback;
}

void NewStorage::SystemEventHandler(u32 systemEvent)
{
	//This happens if another class requested a flash operation => Avoid that!
	if(currentTask == NULL) return;

	//Make a copy so we can clear it from our queue before calling a listener
	sizedData data = taskQueue->PeekNext();
	DYNAMIC_ARRAY(buffer, data.length);
	memcpy(buffer, data.data, data.length);
	NewStorageTaskItem* oldTaskReference = (NewStorageTaskItem*)data.data;
	NewStorageTaskItem* taskReference = (NewStorageTaskItem*)buffer;

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
			currentTask = NULL;
			retryCount = NEW_STORAGE_RETRY_COUNT;

			//Abort transaction will now discard further tasks if the task belonged to a transaction
			AbortTransactionInProgress(taskReference, NewStorageError::FLASH_OPERATION_TIMED_OUT);

			if (taskQueue->_numElements == 0 && emptyHandler != NULL) emptyHandler->NewStorageQueueEmptyHandler();
		}
	}
	else if(systemEvent == NRF_EVT_FLASH_OPERATION_SUCCESS)
	{
		//Reset retryCount if something succeeded
		retryCount = NEW_STORAGE_RETRY_COUNT;

		logt("NEWSTORAGE", "Flash operation success");
		if(
				taskReference->command == NewStorageCommand::ERASE_PAGE
				|| taskReference->command == NewStorageCommand::WRITE_DATA
				|| currentTask->command == NewStorageCommand::WRITE_AND_CACHE_DATA
		){
			//Erase page command successful
			taskQueue->DiscardNext();
			currentTask = NULL;
			if(taskReference->callback != NULL) taskReference->callback->NewStorageItemExecuted(taskReference, NewStorageError::SUCCESS);
			if (taskQueue->_numElements == 0 && emptyHandler != NULL) emptyHandler->NewStorageQueueEmptyHandler();
		}
		else if(taskReference->command == NewStorageCommand::ERASE_PAGES){

			//We must still erase some pages
			if(oldTaskReference->params.erasePages.numPages > 1){
				oldTaskReference->params.erasePages.numPages--;
				ProcessQueue(true);
				return;
			}
			//All pages erased
			else
			{
				logt("NEWSTORAGE", "done");
				taskQueue->DiscardNext();
				currentTask = NULL;
				if(taskReference->callback != NULL) taskReference->callback->NewStorageItemExecuted(taskReference, NewStorageError::SUCCESS);
				if (taskQueue->_numElements == 0 && emptyHandler != NULL) emptyHandler->NewStorageQueueEmptyHandler();
			}
		}
	}

	//Process either the next item or the current one again (if it has not been discarded)
	ProcessQueue(false);
}

u16 NewStorage::GetNumberOfActiveTasks()
{
	return taskQueue->_numElements;
}
