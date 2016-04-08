#include <NewStorage.h>
#include <Logger.h>

extern "C"
{
#include <app_error.h>
}

u8 NewStorage::taskBuffer[] = {};
SimpleQueue* NewStorage::taskQueue = NULL;
NewStorageTaskItem* NewStorage::currentTask = NULL;
i8 NewStorage::retryCount = 0;
u32 NewStorage::dataBuffer[] = {};
bool NewStorage::dataBufferInUse = false;


void NewStorage::Init()
{
	Logger::getInstance().enableTag("NEWSTORAGE");

	//Initialize queue for queueing store and load tasks
	taskQueue = new SimpleQueue(taskBuffer, NEW_STORAGE_TASK_QUEUE_LENGTH*sizeof(NewStorageTaskItem));

	//TODO: Could implement dataBuffer as circular buffer to support multiple queued writes at once
	memset(dataBuffer, 0, NEW_STORAGE_DATA_BUFFER_SIZE);

	retryCount = NEW_STORAGE_RETRY_COUNT;

}

void NewStorage::ErasePage(u16 page, NewStorageEventListener* callback, u32 userType)
{
	NewStorageTaskItem task;
	task.command = NewStorageCommand::ERASE_PAGE;
	task.callback = callback;
	task.userType = userType;
	task.params.erasePage.page = page;

	if(taskQueue->_numElements >= NEW_STORAGE_TASK_QUEUE_LENGTH){
		if(callback != NULL) callback->NewStorageItemExecuted(task, NewStorageError::QUEUE_FULL);
		return;
	}

	taskQueue->Put((u8*)&task, sizeof(NewStorageTaskItem));
	ProcessQueue(false);
}

void NewStorage::ErasePages(u16 startPage, u16 numPages, NewStorageEventListener* callback, u32 userType)
{
	NewStorageTaskItem task;
	task.command = NewStorageCommand::ERASE_PAGES;
	task.callback = callback;
	task.userType = userType;
	task.params.erasePages.startPage = startPage;
	task.params.erasePages.numPages = numPages;

	if(taskQueue->_numElements >= NEW_STORAGE_TASK_QUEUE_LENGTH){
		if(callback != NULL) callback->NewStorageItemExecuted(task, NewStorageError::QUEUE_FULL);
		return;
	}

	taskQueue->Put((u8*)&task, sizeof(NewStorageTaskItem));
	ProcessQueue(false);
}

void NewStorage::WriteData(u32* source, u32* destination, u16 length, NewStorageEventListener* callback, u32 userType)
{
	NewStorageTaskItem task;
	task.command = NewStorageCommand::WRITE_DATA;
	task.callback = callback;
	task.userType = userType;
	task.params.writeData.dataSource = source;
	task.params.writeData.dataDestination = destination;
	task.params.writeData.dataLength = length;

	if(taskQueue->_numElements >= NEW_STORAGE_TASK_QUEUE_LENGTH){
		if(callback != NULL) callback->NewStorageItemExecuted(task, NewStorageError::QUEUE_FULL);
		return;
	}

	taskQueue->Put((u8*)&task, sizeof(NewStorageTaskItem));
	ProcessQueue(false);
}

void NewStorage::CacheAndWriteData(u32* source, u32* destination, u16 length, NewStorageEventListener* callback, u32 userType)
{
	logt("NEWSTORAGE", "cache and write data");

	NewStorageTaskItem task;
	task.command = NewStorageCommand::WRITE_AND_CACHE_DATA;
	task.callback = callback;
	task.userType = userType;
	task.params.writeData.dataSource = dataBuffer;
	task.params.writeData.dataDestination = destination;
	task.params.writeData.dataLength = length;

	if(taskQueue->_numElements >= NEW_STORAGE_TASK_QUEUE_LENGTH){
		if(callback != NULL) callback->NewStorageItemExecuted(task, NewStorageError::QUEUE_FULL);
		return;
	} else if(dataBufferInUse){
		if(callback != NULL) callback->NewStorageItemExecuted(task, NewStorageError::DATA_BUFFER_IN_USE);
		return;
	}

	dataBufferInUse = true;
	memcpy(dataBuffer, source, length);

	taskQueue->Put((u8*)&task, sizeof(NewStorageTaskItem));
	ProcessQueue(false);
}



void NewStorage::ProcessQueue(bool continueCurrentTask)
{
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
		u16 pageNum = currentTask->params.erasePages.startPage + currentTask->params.erasePages.numPages;

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
	else if(
			currentTask->command == NewStorageCommand::WRITE_DATA
			|| currentTask->command == NewStorageCommand::WRITE_AND_CACHE_DATA
	){
		NewStorageTaskItemWriteData* params = &currentTask->params.writeData;

		logt("NEWSTORAGE", "copy from %u to %u, length %u", params->dataSource, params->dataDestination, params->dataLength/4);

		err = sd_flash_write(params->dataDestination, params->dataSource, params->dataLength / 4); //FIXME: NRF_ERROR_BUSY and others not handeled
		APP_ERROR_CHECK(err);
	}
}


void NewStorage::SystemEventHandler(u32 systemEvent)
{
	//This happens if another class requested a flash operation => Avoid that!
	if(currentTask == NULL) return;

	NewStorageTaskItem* taskReference = currentTask;

	if(systemEvent == NRF_EVT_FLASH_OPERATION_ERROR)
	{
		logt("ERROR", "Flash operation error");
		if(retryCount > 0)
		{
			retryCount--;
			//Continue to process current task
			ProcessQueue(true);
			return;
		}
		else {
			//Reset retryCount if the task was canceled
			retryCount = NEW_STORAGE_RETRY_COUNT;

			taskQueue->DiscardNext();
			currentTask = NULL;
			if(taskReference->callback != NULL) taskReference->callback->NewStorageItemExecuted(*taskReference, NewStorageError::FLASH_OPERATION_TIMED_OUT);
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
		){
			//Erase page command successful
			taskQueue->DiscardNext();
			currentTask = NULL;
			if(taskReference->callback != NULL) taskReference->callback->NewStorageItemExecuted(*taskReference, NewStorageError::SUCCESS);
		}
		else if(taskReference->command == NewStorageCommand::ERASE_PAGES){

			//We must still erase some pages
			if(taskReference->params.erasePages.numPages > 0){
				taskReference->params.erasePages.numPages--;
				ProcessQueue(true);
				return;
			}
			//All pages erased
			else
			{
				logt("NEWSTORAGE", "done");
				taskQueue->DiscardNext();
				currentTask = NULL;
				if(taskReference->callback != NULL) taskReference->callback->NewStorageItemExecuted(*taskReference, NewStorageError::SUCCESS);
			}
		}
		else if(currentTask->command == NewStorageCommand::WRITE_AND_CACHE_DATA)
		{
			dataBufferInUse = false;
			taskQueue->DiscardNext();
			currentTask = NULL;
			if(taskReference->callback != NULL) taskReference->callback->NewStorageItemExecuted(*taskReference, NewStorageError::SUCCESS);
		}
	}

	//Process either the next item or the current one again (if it has not been discarded)
	ProcessQueue(false);
}

u8 NewStorage::GetNumberOfActiveTasks()
{
	return taskQueue->_numElements;
}
