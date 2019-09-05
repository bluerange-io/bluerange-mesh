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
#pragma once

#include <types.h>
#include <FlashStorage.h>

/**
 * The RecordStorage is able to manage multiple records in the flash. It is possible to create new
 * records, update records and delete records
 */

constexpr int RECORD_STORAGE_ACTIVE_PAGE_MAGIC_NUMBER = 0xAC71;

constexpr int RECORD_STORAGE_INVALIDATION_MASK = 0xFFFF0000;

class RecordStorageEventListener;


enum class RecordStorageOperationType : u8
{
	SAVE_RECORD,
	DEACTIVATE_RECORD
};

#pragma pack(push)
#pragma pack(1)
constexpr int SIZEOF_RECORD_STORAGE_RECORD_HEADER = 8;
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
STATIC_ASSERT_SIZE(RecordStorageRecord, 9);

constexpr int SIZEOF_RECORD_STORAGE_PAGE_HEADER = 4;
typedef struct
{
		u16 magicNumber;
		u16 versionCounter;
		u8 data[1];

} RecordStoragePage;
STATIC_ASSERT_SIZE(RecordStoragePage, 5);
typedef union
{
	struct {
		u16 functionId;
		u16 stage;
	} data;
	u32 toInt;

} RecordStorageUT;

constexpr int SIZEOF_RECORD_STORAGE_OPERATION = 16;
typedef struct
{
	RecordStorageEventListener* callback;
	u32 userType;
	u8 type;
	FlashStorageError flashStorageErrorCode;
	u16 stage;
	u32 userDataLength;

}RecordStorageOperation;
STATIC_ASSERT_SIZE(RecordStorageOperation, 16);
constexpr int SIZEOF_RECORD_STORAGE_SAVE_RECORD_OP = (SIZEOF_RECORD_STORAGE_OPERATION + 4);
typedef struct
{
	RecordStorageOperation op;
	u16 recordId;
	u16 dataLength;
	u8 data[1];

}SaveRecordOperation;
STATIC_ASSERT_SIZE(SaveRecordOperation, 21);

constexpr int SIZEOF_RECORD_STORAGE_DEACTIVATE_RECORD_OP = (SIZEOF_RECORD_STORAGE_OPERATION + 4);
typedef struct
{
	RecordStorageOperation op;
	u16 recordId;
	u16 reserved;

}DeactivateRecordOperation;
STATIC_ASSERT_SIZE(DeactivateRecordOperation, 20);
#pragma pack(pop)

enum class RecordStorageResultCode : u8
{
	SUCCESS,
	BUSY,
	WRONG_ALIGNMENT,
	NO_SPACE
};

enum class RecordStoragePageState : u8
{
	EMPTY,
	CORRUPT,
	ACTIVE,
};

class RecordStorageEventListener;

constexpr int RECORD_STORAGE_QUEUE_SIZE = 256;


class RecordStorage : public FlashStorageEventListener
{
	friend class TestRecordStorage;

	private:
		u8* startPage;
		u8 numPages;

		//A queue that stores high level operations
		u32 opBuffer[RECORD_STORAGE_QUEUE_SIZE/4];
		PacketQueue opQueue;

		//Variables for repair
		bool repairInProgress = false;
		u32 repairStep = 0;

		//Variables for defragmentation state
		bool defragmentInProgress = false;
		RecordStoragePage* defragmentPage = nullptr;
		RecordStoragePage* defragmentSwapPage = nullptr;
		u32 defragmentStep = 0;
		u32 defragRecordCounter = 0;

		bool processQueueInProgress = false;

		//Stores a record
		void SaveRecordInt(SaveRecordOperation* op);
		//Removes a record
		void DeactivateRecordInt(DeactivateRecordOperation* op);
				
		void DefragmentPage(RecordStoragePage* pageToDefragment, bool force);
		void RepairPages();

		void ProcessQueue(bool force);

		void RecordOperationFinished(RecordStorageOperation* op, RecordStorageResultCode code);
		
		RecordStoragePage* GetSwapPage() const;
		RecordStoragePageState GetPageState(RecordStoragePage* page) const;
		u8* GetFreeRecordSpace(u16 dataLength) const;
		u16 GetFreeSpaceOnPage(RecordStoragePage * page) const;
		u16 GetFreeSpaceWhenDefragmented(RecordStoragePage* page) const;

		//Helpers
		//Checks if a record is valid
		bool IsRecordValid(RecordStoragePage* page, RecordStorageRecord* record) const;
		//Looks through all pages and returns the page with the most space after defragmentation
		RecordStoragePage * FindPageToDefragment() const;

		bool isInit = false;
		
	public:
		RecordStorage();
		void Init();
		bool IsInit();

		//Initialize Storage class
		static RecordStorage& getInstance();

		//Must only be called upon start
		void InitialRepair();

		//Stores a record (Operation is queued)
		RecordStorageResultCode SaveRecord(u16 recordId, u8* data, u16 dataLength, RecordStorageEventListener* callback, u32 userType);
		//Allows to cache some information until store completes
		RecordStorageResultCode SaveRecord(u16 recordId, u8* data, u16 dataLength, RecordStorageEventListener* callback, u32 userType, u8* userData, u16 userDataLength);
		//Removes a record (Operation is queued)
		RecordStorageResultCode DeactivateRecord(u16 recordId, RecordStorageEventListener * callback, u32 userType);
		//Retrieves a record
		RecordStorageRecord* GetRecord(u16 recordId) const;
		//Retrieves the data of a record
		SizedData GetRecordData(u16 recordId) const;
		//Resets all settings
		void ClearAllSettings();
		
		//Listener
		void FlashStorageItemExecuted(FlashStorageTaskItem* task, FlashStorageError errorCode) override;
		void FlashStorageQueueEmptyHandler() override;

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

