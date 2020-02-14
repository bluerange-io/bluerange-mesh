////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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

 /*## RecordIds #############################################################*/
 // The modules use their moduleId as a recordId, records outside this range can be used
 // for other types of storage

 //Specific Record Ids
#define RECORD_STORAGE_RECORD_ID_UPDATE_STATUS 1000 //Stores the done status of an update
#define RECORD_STORAGE_RECORD_ID_UICR_REPLACEMENT 1001 //Can be used, if UICR can not be flashed, e.g. when updating another beacon with different firmware
#define RECORD_STORAGE_RECORD_ID_DEPRECATED 1002 //Was used to store fake positions for nodes to modify the incoming events


constexpr u16 RECORD_STORAGE_ACTIVE_PAGE_MAGIC_NUMBER = 0xAC71;

constexpr int RECORD_STORAGE_INVALIDATION_MASK = 0xFFFF0000;

class RecordStorageEventListener;


enum class RecordStorageOperationType : u8
{
	SAVE_RECORD,
	DEACTIVATE_RECORD
};

enum class RecordStorageSaveStage : u16
{
	FIRST_STAGE          = 0,
	DEFRAGMENT_IF_NEEDED = 0,
	SAVE                 = 1,
	CALLBACKS_AND_FINISH = 2,
};

enum class RecordStorageDeactivateStage : u16
{
	FIRST_STAGE          = 0,
	DEACTIVATE           = 0,
	CALLBACKS_AND_FINISH = 1,
};

enum class RepairStage : u8
{
	ERASE_CORRUPT_PAGES       = 0,
	CLEAR_SWAP_PAGE_IF_NEEDED = 1,
	ACTIVATE_PAGES            = 2,
	VALIDATE_PAGES            = 3,
	FINALIZE                  = 4,

	NO_REPAIR                 = 255,
};

enum class DefragmentationStage : u8
{
	MOVE_TO_SWAP_PAGE = 0,
	WRITE_PAGE_HEADER = 1,
	ERASE_OLD_PAGE    = 2,
	FINALIZE          = 4,

	NO_DEFRAGMENTATION = 255,
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
STATIC_ASSERT_SIZE(RecordStorageRecord, SIZEOF_RECORD_STORAGE_RECORD_HEADER + 1);

constexpr int SIZEOF_RECORD_STORAGE_PAGE_HEADER = 4;
typedef struct
{
		u16 magicNumber;
		u16 versionCounter;
		u8 data[1];

} RecordStoragePage;
STATIC_ASSERT_SIZE(RecordStoragePage, 5);

constexpr int SIZEOF_RECORD_STORAGE_OPERATION = 14;
typedef struct
{
	RecordStorageEventListener* callback;
	u32 userType;
	u8 type;
	FlashStorageError flashStorageErrorCode;
	u32 userDataLength;

}RecordStorageOperation;
STATIC_ASSERT_SIZE(RecordStorageOperation, SIZEOF_RECORD_STORAGE_OPERATION);

constexpr int SIZEOF_RECORD_STORAGE_SAVE_RECORD_OP = (SIZEOF_RECORD_STORAGE_OPERATION + 6);
typedef struct
{
	RecordStorageOperation op;
	RecordStorageSaveStage stage;
	u16 recordId;
	u16 dataLength;
	u8 data[1];

}SaveRecordOperation;
STATIC_ASSERT_SIZE(SaveRecordOperation, SIZEOF_RECORD_STORAGE_SAVE_RECORD_OP + 1);

constexpr int SIZEOF_RECORD_STORAGE_DEACTIVATE_RECORD_OP = (SIZEOF_RECORD_STORAGE_OPERATION + 6);
typedef struct
{
	RecordStorageOperation op;
	RecordStorageDeactivateStage stage;
	u16 recordId;
	u16 reserved;

}DeactivateRecordOperation;
STATIC_ASSERT_SIZE(DeactivateRecordOperation, SIZEOF_RECORD_STORAGE_DEACTIVATE_RECORD_OP);
#pragma pack(pop)

enum class RecordStorageResultCode : u8
{
	SUCCESS                  = 0,
	BUSY                     = 1,
	WRONG_ALIGNMENT          = 2,
	NO_SPACE                 = 3,
	RECORD_STORAGE_LOCK_DOWN = 4, //The best action for a module that receives this is to discard the write access completely.
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
		enum class FlashUserTypes : u32
		{
			DEFAULT   = 0, //FIXME: All usages of this value should be refactored to use a distinct user type instead.
			LOCK_DOWN = 1,
		};

		u8* startPage = nullptr;

		//A queue that stores high level operations
		u32 opBuffer[RECORD_STORAGE_QUEUE_SIZE / sizeof(u32)] = { 0 };
		PacketQueue opQueue;

		//Variables for repair
		RepairStage repairStage = RepairStage::NO_REPAIR;

		//Variables for defragmentation state
		RecordStoragePage* defragmentPage = nullptr;
		RecordStoragePage* defragmentSwapPage = nullptr;
		DefragmentationStage defragmentationStage = DefragmentationStage::NO_DEFRAGMENTATION;

		bool processQueueInProgress = false;

		//Stores a record
		void SaveRecordInternal(SaveRecordOperation& op);
		//Removes a record
		void DeactivateRecordInternal(DeactivateRecordOperation& op);
				
		void DefragmentPage(RecordStoragePage& pageToDefragment, bool force);
		void RepairPages();

		void ProcessQueue(bool force);

		void RecordOperationFinished(RecordStorageOperation& op, RecordStorageResultCode code);
		void ExecuteCallback(RecordStorageOperation& op, RecordStorageResultCode code);
		
		RecordStoragePage* GetSwapPage() const;
		RecordStoragePageState GetPageState(const RecordStoragePage& page) const;
		u8* GetFreeRecordSpace(u16 dataLength) const;
		u16 GetFreeSpaceOnPage(const RecordStoragePage& page) const;
		u16 GetFreeSpaceWhenDefragmented(const RecordStoragePage& page) const;

		//Helpers
		//Checks if a record is valid
		bool IsRecordValid(const RecordStoragePage& page, RecordStorageRecord const* record) const;
		//Looks through all pages and returns the page with the most space after defragmentation
		RecordStoragePage * FindPageToDefragment() const;
		RecordStoragePage& getPage(u32 index) const;

		bool isInit = false;

		bool recordStorageLockDown = false;
		RecordStorageEventListener *lockDownCallback = nullptr;
		u32 lockDownUserType = 0;
		constexpr static u8 LOCK_DOWN_RETRY_MAX = 10;
		u8 lockDownRetryCounter = 0;
		//Only the module that is responsible for the lockdown is allowed to open up the record storage again.
		ModuleId lockDownModuleId = ModuleId::INVALID_MODULE;

	public:
		RecordStorage();
		void Init();
		bool IsInit();

		//Initialize Storage class
		static RecordStorage& getInstance();

		//Stores a record (Operation is queued)
		RecordStorageResultCode SaveRecord(u16 recordId, u8* data, u16 dataLength, RecordStorageEventListener* callback, u32 userType, ModuleId lockDownModule = ModuleId::INVALID_MODULE);
		//Allows to cache some information until store completes
		RecordStorageResultCode SaveRecord(u16 recordId, u8* data, u16 dataLength, RecordStorageEventListener* callback, u32 userType, u8* userData, u16 userDataLength, ModuleId lockDownModule = ModuleId::INVALID_MODULE);
		//Removes a record (Operation is queued)
		RecordStorageResultCode DeactivateRecord(u16 recordId, RecordStorageEventListener * callback, u32 userType, ModuleId lockDownModule = ModuleId::INVALID_MODULE);
		//Retrieves a record
		RecordStorageRecord* GetRecord(u16 recordId) const;
		//Retrieves the data of a record
		SizedData GetRecordData(u16 recordId) const;
		//Resets all settings
		RecordStorageResultCode LockDownAndClearAllSettings(ModuleId responsibleModuleForLockDown, RecordStorageEventListener * callback, u32 userType);
		
		//Listener
		void FlashStorageItemExecuted(FlashStorageTaskItem* task, FlashStorageError errorCode) override;
		void FlashStorageQueueEmptyHandler();

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

