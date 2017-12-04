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

#pragma once

#include <types.h>
#include <GlobalState.h>
#include <NewStorage.h>

extern "C"{

}

/**
 * The RecordStorage is able to manage multiple records in the flash. It is possible to create new
 * records, update records and delete records
 */

#define RECORD_STORAGE_ACTIVE_PAGE_MAGIC_NUMBER 0xAC71

#define RECORD_STORAGE_INVALIDATION_MASK 0xFFFF0000

class RecordStorageEventListener;


enum RecordStorageOperationType
{
	RS_STATE_SAVE_RECORD,
	RS_STATE_DEACTIVATE_RECORD
};

#pragma pack(push)
#pragma pack(1)
#define SIZEOF_RECORD_STORAGE_RECORD_HEADER 8
typedef struct
{
		u8 crc;
		u8 reserved : 5;
		u8 padding : 2;
		u8 recordActive : 1;
		u16 recordLength;
		u16 recordId;
		u16 versionCounter;
		u8 data[1];

} RecordStorageRecord;

#define SIZEOF_RECORD_STORAGE_PAGE_HEADER 4
typedef struct
{
		u16 magicNumber;
		u16 versionCounter;
		u8 data[1];

} RecordStoragePage;
typedef union
{
	struct {
		u16 functionId;
		u16 stage;
	} data;
	u32 toInt;

} RecordStorageUT;

#define SIZEOF_RECORD_STORAGE_OPERATION 16
typedef struct
{
	RecordStorageEventListener* callback;
	u32 userType;
	u8 type;
	u8 newStorageErrorCode;
	u16 stage;
	u32 userDataLength;

}RecordStorageOperation;
#define SIZEOF_RECORD_STORAGE_SAVE_RECORD_OP (SIZEOF_RECORD_STORAGE_OPERATION + 4)
typedef struct
{
	RecordStorageOperation op;
	u16 recordId;
	u16 dataLength;
	u8 data[1];

}SaveRecordOperation;

#define SIZEOF_RECORD_STORAGE_DEACTIVATE_RECORD_OP (SIZEOF_RECORD_STORAGE_OPERATION + 4)
typedef struct
{
	RecordStorageOperation op;
	u16 recordId;
	u16 reserved;

}DeactivateRecordOperation;
#pragma pack(pop)

enum RecordStorageResultCode
{
	RECORD_STORAGE_STORE_SUCCESS,
	RECORD_STORAGE_STORE_BUSY,
	RECORD_STORAGE_STORE_WRONG_ALIGNMENT,
	RECORD_STORAGE_STORE_NO_SPACE
};

enum RecordStoragePageState
{
	RECORD_STORAGE_PAGE_EMPTY,
	RECORD_STORAGE_PAGE_CORRUPT,
	RECORD_STORAGE_PAGE_ACTIVE,
};

class RecordStorageEventListener;

#define RECORD_STORAGE_QUEUE_SIZE 256


class RecordStorage : public NewStorageEventListener
{
	friend class TestRecordStorage;

	private:
		RecordStorage();
		u8* startPage;
		u8 numPages;

		//A queue that stores high level operations
		u8 opBuffer[RECORD_STORAGE_QUEUE_SIZE];
		PacketQueue* opQueue;

		//Variables for repair
		bool repairInProgress = false;
		u32 repairStep = 0;

		//Variables for defragmentation state
		bool defragmentInProgress = false;
		RecordStoragePage* defragmentPage = NULL;
		RecordStoragePage* defragmentSwapPage = NULL;
		u32 defragmentStep = 0;
		u32 defragRecordCounter = 0;

		//Stores a record
		void SaveRecordInt(SaveRecordOperation* op);
		//Removes a record
		void DeactivateRecordInt(DeactivateRecordOperation* op);
				
		void DefragmentPage(RecordStoragePage* pageToDefragment, bool force);
		void RepairPages();

		void ProcessQueue(bool force);

		void RecordOperationFinished(RecordStorageOperation* op, RecordStorageResultCode code);
		
		RecordStoragePage* GetSwapPage();
		RecordStoragePageState GetPageState(RecordStoragePage* page);
		u8* GetFreeRecordSpace(u16 dataLength);
		u16 GetFreeSpaceOnPage(RecordStoragePage * page);
		u16 GetFreeSpaceWhenDefragmented(RecordStoragePage* page);

		//Helpers
		//Checks if a record is valid
		bool IsRecordValid(RecordStoragePage* page, RecordStorageRecord* record);
		//Looks through all pages and returns the page with the most space after defragmentation
		RecordStoragePage * FindPageToDefragment();

		
	public:

		//Initialize Storage class
		static RecordStorage* getInstance(){
			if(!GS->recordStorage){
				GS->recordStorage = new RecordStorage();
			}
			return GS->recordStorage;
		}

		//Must only be called upon start
		void InitialRepair();

		//Stores a record (Operation is queued)
		u32 SaveRecord(u16 recordId, u8* data, u16 dataLength, RecordStorageEventListener* callback, u32 userType);
		//Allows to cache some information until store completes
		u32 SaveRecord(u16 recordId, u8* data, u16 dataLength, RecordStorageEventListener* callback, u32 userType, u8* userData, u16 userDataLength);
		//Removes a record (Operation is queued)
		u32 DeactivateRecord(u16 recordId, RecordStorageEventListener * callback, u32 userType);
		//Retrieves a record
		RecordStorageRecord* GetRecord(u16 recordId);
		//Retrieves the data of a record
		sizedData GetRecordData(u16 recordId);

		//Stuff
		void PrintPages();
		
		//Listener
		void NewStorageItemExecuted(NewStorageTaskItem* task, u8 errorCode);
		void NewStorageQueueEmptyHandler();

};

class RecordStorageEventListener
{
	public:
		RecordStorageEventListener(){};

	virtual ~RecordStorageEventListener(){};

	//Struct is passed by value so that it can be dequeued before calling this handler
	//If we passed a reference, this handler would have to clear the item from the TaskQueue
	virtual void RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength) = 0;

};

