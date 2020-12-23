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

/**
 *
 * The RecordStorage class enables easy access to flash storage with a simple API.
 * Internally it uses FlashStorage to queue flash operations and allows a configurable retry counter.
 * The user can either use SaveRecord or DeactivateRecord which will handle the operation in the background.
 * Both methods will call a listener after a flash operation has succeeded or failed.
 * This listener will be passed the userType and in some cases, it is even possible to store some
 * userData that will also be returned after the end of the operation.
 *
 * GetRecordData is synchronous and will return the requested data immediately if available.
 *
 * Each record is stored using a recordId and only the latest version of a Record is accessable.
 *
 * A configurable number of flash pages can be used for RecordStorage and only one Page is used
 * as a swap page. Only a single swap is done internally, so the swap page changes.
 * The swap page is always empty whereas the other pages are marked active with the page header that is:
 * Magic bytes 0xAC71 (2 byte), followed by the page version that is always incrementing (2byte)
 *
 * A power loss during an operation will trigger the repair routine upon reboot that will erase corrupt
 * pages and will erase the latest page if the swapping did not complete.
 *
 * Records are maintained in a way that is failsafe against power loss at any point.
 * A Record header consists of the recordId, a crc, its length and its version (always incrementing).
 *
 * All active pages will be used to store records until all pages are filled. Saving a recordId the second
 * time will increment the version counter and GetRecord will always deliver the newest version of a record.
 *
 * Once all pages are full, the page with the most possible free space is defragmented. Therefore,
 * all current and active records will be moved to the swap page. Afterwards this page is activated and
 * the old page is erased and becomes the new swap page.

 * The implementation does currently only support updating a record up to 65000 times and 65000 erase cycles of the settings pages

*/


#include <RecordStorage.h>
#include <Config.h>
#include <FlashStorage.h>
#include <Utility.h>
#include <Logger.h>
#include <GlobalState.h>
#include <FruityHal.h>

#define TO_PAGE(addr) (u32)(((((u32)(addr)) - FLASH_REGION_START_ADDRESS)/FruityHal::GetCodePageSize()))

RecordStorage::RecordStorage()
    : opQueue(opBuffer, RECORD_STORAGE_QUEUE_SIZE)
{
}

void RecordStorage::Init()
{
    startPage = (u8*)Utility::GetSettingsPageBaseAddress();
    RepairPages();
    isInit = true;
}

bool RecordStorage::IsInit()
{
    return isInit;
}

RecordStorage & RecordStorage::GetInstance()
{
    return GS->recordStorage;
}

/* ######################
# Public functions that allow write access
# A call will be queued first, and will then be executed
######################### */

RecordStorageResultCode RecordStorage::SaveRecord(u16 recordId, u8* data, u16 dataLength, RecordStorageEventListener* callback, u32 userType, ModuleIdWrapper lockDownModule)
{
    if (recordStorageLockDown && lockDownModule != lockDownModuleId)
    {
        SIMEXCEPTION(RecordStorageIsLockedDownException);
        return RecordStorageResultCode::RECORD_STORAGE_LOCK_DOWN;
    }
    return RecordStorage::SaveRecord(recordId, data, dataLength, callback, userType, nullptr, 0, lockDownModule);
}

RecordStorageResultCode RecordStorage::SaveRecord(u16 recordId, u8* data, u16 dataLength, RecordStorageEventListener* callback, u32 userType, u8* userData, u16 userDataLength, ModuleIdWrapper lockDownModule)
{
    if (recordStorageLockDown && lockDownModule != lockDownModuleId)
    {
        SIMEXCEPTION(RecordStorageIsLockedDownException);
        return RecordStorageResultCode::RECORD_STORAGE_LOCK_DOWN;
    }
    //Cache the operation to be processed later
    u8* buffer = opQueue.Reserve(SIZEOF_RECORD_STORAGE_SAVE_RECORD_OP + dataLength + userDataLength);

    if (buffer != nullptr) {
        SaveRecordOperation* op = (SaveRecordOperation*)buffer;
        op->op.type = (u8)RecordStorageOperationType::SAVE_RECORD;
        op->op.callback = callback;
        op->op.userType = userType;
        op->op.userDataLength = userDataLength;
        op->stage = RecordStorageSaveStage::FIRST_STAGE;
        op->recordId = recordId;
        op->dataLength = dataLength;
        CheckedMemcpy(op->data, data, dataLength);
        if (userData != nullptr) CheckedMemcpy(buffer + (SIZEOF_RECORD_STORAGE_SAVE_RECORD_OP + dataLength), userData, userDataLength);

        ProcessQueue(false);
        return RecordStorageResultCode::SUCCESS;
    }
    else {
        return RecordStorageResultCode::BUSY;
    }
}

RecordStorageResultCode RecordStorage::DeactivateRecord(u16 recordId, RecordStorageEventListener* callback, u32 userType, ModuleIdWrapper lockDownModule)
{
    if (recordStorageLockDown && lockDownModule != lockDownModuleId)
    {
        SIMEXCEPTION(RecordStorageIsLockedDownException);
        return RecordStorageResultCode::RECORD_STORAGE_LOCK_DOWN;
    }
    //Cache the operation to be processed later
    u8* buffer = opQueue.Reserve(SIZEOF_RECORD_STORAGE_DEACTIVATE_RECORD_OP);

    if (buffer != nullptr) {
        DeactivateRecordOperation* op = (DeactivateRecordOperation*)buffer;
        op->op.type = (u8)RecordStorageOperationType::DEACTIVATE_RECORD;
        op->op.callback = callback;
        op->op.userType = userType;
        op->recordId = recordId;
        op->stage = RecordStorageDeactivateStage::FIRST_STAGE;

        ProcessQueue(false);
        return RecordStorageResultCode::SUCCESS;
    }
    else {
        return RecordStorageResultCode::BUSY;
    }
}

/* ######################
# Public functions that allow write access
######################### */

//This function is called from the queue multiple times to do the actual store operations
void RecordStorage::SaveRecordInternal(SaveRecordOperation& op)
{
    //If any of the previous operations failed, call the callback with an error code
    if (op.op.flashStorageErrorCode != FlashStorageError::SUCCESS) {
        return RecordOperationFinished(op.op, RecordStorageResultCode::BUSY);
    }

    if (op.stage == RecordStorageSaveStage::DEFRAGMENT_IF_NEEDED) {
        logt("RS", "SaveRecord id %u, len %u", op.recordId, op.dataLength);

        u16 recordLength = op.dataLength + SIZEOF_RECORD_STORAGE_RECORD_HEADER;

        //First, check if we have enough space to simply save the record
        u8* freeSpace = GetFreeRecordSpace(recordLength);

        //If not, we must first defragment the page which has the most available space
        if (freeSpace == nullptr) {
            RecordStoragePage* pageToDefragment = FindPageToDefragment();
            if (pageToDefragment != nullptr) {
                op.stage = RecordStorageSaveStage::SAVE;
                return DefragmentPage(*pageToDefragment, false);
            }
        }

        op.stage = RecordStorageSaveStage::SAVE;
    }

    if (op.stage == RecordStorageSaveStage::SAVE) {

        //Data must be saved als multiple of 4 bytes, so we pad the data with 0xFF
        //userData needs no padding as it is not written to flash
        u8 padding = (4-op.dataLength%4)%4;

        u16 recordLength = op.dataLength + SIZEOF_RECORD_STORAGE_RECORD_HEADER + padding;

        //Afterwards, there must be enough free space, otherwise it is not possible to save this record
        u8* freeSpace = GetFreeRecordSpace(recordLength);

        if (freeSpace != nullptr) {

            //Check if we have an old record with the same id
            //If yes, save this one with a newer versionCounter
            RecordStorageRecord* oldRecord = GetRecord(op.recordId);

            //Currently, we only support updating a record up to 65000 times
            if (oldRecord != nullptr && oldRecord->versionCounter == UINT16_MAX) {
                return RecordOperationFinished(op.op, RecordStorageResultCode::NO_SPACE);
            }

            u16 recordVersion = oldRecord == nullptr ? 1 : oldRecord->versionCounter + 1;

            //Build the record in a buffer
            DYNAMIC_ARRAY(buffer, recordLength);
            CheckedMemset(buffer, 0xFF, recordLength);
            RecordStorageRecord* newRecord = (RecordStorageRecord*)buffer;
            newRecord->recordActive = 1;
            newRecord->padding = padding; //Padding must be stored so we can substract it later when retrieving the record
            newRecord->recordLength = recordLength;
            newRecord->recordId = op.recordId;
            newRecord->versionCounter = recordVersion;
            CheckedMemcpy(newRecord->data, op.data, op.dataLength);

            //Check if the old record matches the new record and do not write to flash in this case
            if(oldRecord != nullptr && oldRecord->recordActive && oldRecord->recordLength == newRecord->recordLength && oldRecord->padding == newRecord->padding){
                u32 cmp = memcmp(oldRecord->data, newRecord->data, op.dataLength);
                if(cmp == 0){
                    return RecordOperationFinished(op.op, RecordStorageResultCode::SUCCESS);
                }
            }

            //The crc is calculated over the record header and data, excluding the first two byte (crc and flags)
            newRecord->crc = Utility::CalculateCrc8(((u8*)newRecord) + 2, newRecord->recordLength - 2);
            op.stage = RecordStorageSaveStage::CALLBACKS_AND_FINISH;
            GS->flashStorage.CacheAndWriteData((u32*)newRecord, (u32*)freeSpace, recordLength, this, (u32)FlashUserTypes::DEFAULT);
            return;

        }
        else {
            logt("ERROR", "no space in RS");
            GS->logger.LogCustomError(CustomErrorTypes::FATAL_NO_RECORDSTORAGE_SPACE_LEFT, recordLength);

            for (u32 i = 0; i < RECORD_STORAGE_NUM_PAGES; i++) {
                RecordStoragePage& page = getPage(i);
                u16 freeSpaceAfterDefragment = GetFreeSpaceWhenDefragmented(page);
                logt("ERROR", "freeSpace in page %u: %u", ((u32)&page - (u32)FLASH_REGION_START_ADDRESS) / (u32)FruityHal::GetCodePageSize(), freeSpaceAfterDefragment);
            }

            return RecordOperationFinished(op.op, RecordStorageResultCode::NO_SPACE);
        }
    }
    
    if (op.stage == RecordStorageSaveStage::CALLBACKS_AND_FINISH)
    {
        return RecordOperationFinished(op.op, RecordStorageResultCode::SUCCESS);
    }
}

//Deactivating a record will set the deleted flag of the newest entry for this recordId, it will be deleted after a page is defragmented
void RecordStorage::DeactivateRecordInternal(DeactivateRecordOperation& op)
{
    //If any of the previous operations failed, call the callback with an error code
    if (op.op.flashStorageErrorCode != FlashStorageError::SUCCESS) {
        return RecordOperationFinished(op.op, RecordStorageResultCode::BUSY);
    }

    if (op.stage == RecordStorageDeactivateStage::DEACTIVATE) {
        logt("RS", "DeactivateRecord id %u", op.recordId);

        RecordStorageRecord* record = GetRecord(op.recordId);

        if (record == nullptr || record->recordActive == 0) {
            return RecordOperationFinished(op.op, RecordStorageResultCode::SUCCESS);
        }

        RecordStorageRecord newRecordHeader;
        CheckedMemset(&newRecordHeader, 0xFF, SIZEOF_RECORD_STORAGE_RECORD_HEADER);
        newRecordHeader.recordActive = 0;

        op.stage = RecordStorageDeactivateStage::CALLBACKS_AND_FINISH;
        GS->flashStorage.CacheAndWriteData((u32*)&newRecordHeader, (u32*)record, SIZEOF_RECORD_STORAGE_RECORD_HEADER, this, (u32)FlashUserTypes::DEFAULT);
        return;
    }
    
    if (op.stage == RecordStorageDeactivateStage::CALLBACKS_AND_FINISH)
    {
        return RecordOperationFinished(op.op, RecordStorageResultCode::SUCCESS);
    }
}

RecordStorageResultCode RecordStorage::LockDownAndClearAllSettings(ModuleIdWrapper responsibleModuleForShutDown, RecordStorageEventListener * callback, u32 userType)
{
    //Check if we already have locked down
    if (recordStorageLockDown)
    {
        return RecordStorageResultCode::RECORD_STORAGE_LOCK_DOWN;
    }
    while (opQueue._numElements > 0)
    {
        SizedData data = opQueue.PeekNext();
        RecordStorageOperation* op = (RecordStorageOperation*)data.data;
        ExecuteCallback(*op, RecordStorageResultCode::RECORD_STORAGE_LOCK_DOWN);
        opQueue.DiscardNext();
    }
    opQueue.Clean();
    lockDownCallback = callback;
    lockDownUserType = userType;
    lockDownModuleId = responsibleModuleForShutDown;
    FlashStorageError flashRetVal = GS->flashStorage.ErasePages(TO_PAGE(startPage), RECORD_STORAGE_NUM_PAGES, this, (u32)FlashUserTypes::LOCK_DOWN);
    if (flashRetVal == FlashStorageError::SUCCESS)
    {
        recordStorageLockDown = true;
        return RecordStorageResultCode::SUCCESS;
    }
    else
    {
        return RecordStorageResultCode::BUSY;
    }
}

/*#################################
# Repair and Defragment Pages
#################################*/


//This will heal corruption caused by power loss or write errors
//This will always produce one empty swap page with all other pages being active
//This method must be called again until it finishes
void RecordStorage::RepairPages()
{
    if (repairStage == RepairStage::NO_REPAIR) {
        repairStage = RepairStage::ERASE_CORRUPT_PAGES;
    }

    //If there are items in the flashStorage queue, we wait until we get called after the queue is empty
    //This allows us to ignore result codes from the queue because there will always be space
    if (GS->flashStorage.GetNumberOfActiveTasks() != 0){
        return;
    }
    
    if (repairStage == RepairStage::ERASE_CORRUPT_PAGES)
    {
        //Erase all corrupt pages
        for (u32 i = 0; i < RECORD_STORAGE_NUM_PAGES; i++) {
            RecordStoragePage& page = getPage(i);
            RecordStoragePageState pageState = GetPageState(page);

            if (pageState == RecordStoragePageState::CORRUPT) {
                GS->flashStorage.ErasePage(((u32)&page - FLASH_REGION_START_ADDRESS) / FruityHal::GetCodePageSize(), nullptr, (u32)FlashUserTypes::DEFAULT);
                return;
            }
        }

        repairStage = RepairStage::CLEAR_SWAP_PAGE_IF_NEEDED;
    }
    
    if (repairStage == RepairStage::CLEAR_SWAP_PAGE_IF_NEEDED)
    {
        //Check if there is now at least one swap page
        RecordStoragePage* swapPage = GetSwapPage();

        //If there is no swap page (all pages are active), we clear the page with the highest versionCounter
        //This is the last page that was saved, but the transaction had failed to clear
        //the other page at the end
        u16 maxVersionCounter = 0;
        if (swapPage == nullptr) {
            for (u32 i = 0; i < RECORD_STORAGE_NUM_PAGES; i++) {
                RecordStoragePage& page = getPage(i);
                if (page.versionCounter > maxVersionCounter) {
                    swapPage = &page;
                    maxVersionCounter = page.versionCounter;
                }
            }

            //Clear the swap page
            GS->flashStorage.ErasePage(((u32)swapPage - FLASH_REGION_START_ADDRESS) / FruityHal::GetCodePageSize(), nullptr, (u32)FlashUserTypes::DEFAULT);
            return;
        }

        repairStage = RepairStage::ACTIVATE_PAGES;
    }
    
    if (repairStage == RepairStage::ACTIVATE_PAGES)
    {
        RecordStoragePage* swapPage = GetSwapPage();

        //Determine max version counter
        u16 maxVersionCounter = 0;
        for (u32 i = 0; i < RECORD_STORAGE_NUM_PAGES; i++) {
            RecordStoragePage& page = getPage(i);
            if (GetPageState(page) == RecordStoragePageState::ACTIVE && page.versionCounter > maxVersionCounter) {
                maxVersionCounter = page.versionCounter;
            }
        }

        //Check that all pages except the swap page are active
        //Marks empty pages active with incrementing versionCounters
        for (u32 i = 0; i < RECORD_STORAGE_NUM_PAGES; i++) {
            RecordStoragePage& page = getPage(i);
            if (&page != swapPage && GetPageState(page) == RecordStoragePageState::EMPTY) {
                RecordStoragePage pageHeader;
                CheckedMemset(&pageHeader, 0, sizeof(pageHeader));
                pageHeader.magicNumber = RECORD_STORAGE_ACTIVE_PAGE_MAGIC_NUMBER;
                pageHeader.versionCounter = ++maxVersionCounter;

                GS->flashStorage.CacheAndWriteData((u32*)&pageHeader, (u32*)&page, SIZEOF_RECORD_STORAGE_PAGE_HEADER, nullptr, (u32)FlashUserTypes::DEFAULT);
                return;
            }
        }

        repairStage = RepairStage::VALIDATE_PAGES;
    }
    
    //TODO: untested
    //Check if, for all active pages, there is only free space after the last valid record
    //if not, defragment this page. There can only be one such page after a power loss
    if(repairStage == RepairStage::VALIDATE_PAGES)
    {
        for (u32 i = 0; i < RECORD_STORAGE_NUM_PAGES; i++) {
            RecordStoragePage& page = getPage(i);
            RecordStoragePageState pageState = GetPageState(page);

            if (pageState == RecordStoragePageState::ACTIVE) {
                RecordStorageRecord* record = (RecordStorageRecord*)page.data;

                //Iterate through all records to find the last valid one (record will then point to free space)
                while (IsRecordValid(page, record)) {
                    record = (RecordStorageRecord*)((u8*)record + record->recordLength);
                }

                //Now, we must check that the rest of the page is clean
                u32* pageData = (u32*)&page;
                u32 freeSpaceOffset = ((u32)record) - ((u32)pageData);
                for(u32 j=freeSpaceOffset; j<FruityHal::GetCodePageSize(); j+=sizeof(u32)){
                    if(pageData[j/4] != 0xFFFFFFFF){
                        repairStage = RepairStage::FINALIZE;
                        logt("RS", "Corruption after record detected, defragmenting");
                        DefragmentPage(page, true);
                        return;
                    }
                }
            }
        }

        repairStage = RepairStage::FINALIZE;
    }

    if (repairStage == RepairStage::FINALIZE)
    {
        repairStage = RepairStage::NO_REPAIR;

        //If this repair process was initiated from a lock down.
        if (recordStorageLockDown)
        {
            if (lockDownCallback != nullptr)
            {
                lockDownCallback->RecordStorageEventHandler(0, RecordStorageResultCode::SUCCESS, lockDownUserType, nullptr, 0);
            }

            if (GS->node.IsRebootScheduled() == false)
            {
                //This protects us against a callee to the factory reset that forgot to reboot the node.
                GS->node.Reboot(SEC_TO_DS(15 * 60), RebootReason::FACTORY_RESET_SUCCEEDED_FAILSAFE);
            }
        }

        //Call the listener manually because we did not queue another task
        ProcessQueue(true);
    }
}


//This will copy the contents of a page to the swap page while ignoring invalid records
//The implementation only queues one item at a time and will continue from there after being called again
//This allows us to keep queue size and ram usage to an absolute minimum
void RecordStorage::DefragmentPage(RecordStoragePage& pageToDefragment, bool force)
{
    logt("WARNING", "defr");

    if (defragmentationStage == DefragmentationStage::NO_DEFRAGMENTATION) {
        defragmentPage = &pageToDefragment;
        defragmentSwapPage = GetSwapPage();
        defragmentationStage = DefragmentationStage::MOVE_TO_SWAP_PAGE;

        if (!force && GetFreeSpaceOnPage(*defragmentPage) == GetFreeSpaceWhenDefragmented(*defragmentPage)) {
            logt("RS", "No defrag possible");
            defragmentationStage = DefragmentationStage::NO_DEFRAGMENTATION;
            return;
        }
        if (defragmentSwapPage == nullptr)
        {
            GS->logger.LogCustomError(CustomErrorTypes::FATAL_RECORD_STORAGE_COULD_NOT_FIND_SWAP_PAGE, 0);
            defragmentationStage = DefragmentationStage::NO_DEFRAGMENTATION;
            SIMEXCEPTION(IllegalStateException);
            return;
        }

        logt("RS", "Defragmenting Page %u (free %u, after %u)", ((u32)defragmentPage - (u32)FLASH_REGION_START_ADDRESS) / (u32)FruityHal::GetCodePageSize(), GetFreeSpaceOnPage(*defragmentPage), GetFreeSpaceWhenDefragmented(*defragmentPage));
    }

    //If there are items in the flashStorage queue, we wait until we get called after the queue is empty
    if (GS->flashStorage.GetNumberOfActiveTasks() != 0){
        logt("ERROR", "tasks %u", GS->flashStorage.GetNumberOfActiveTasks());
        return;
    }

    if (defragmentationStage == DefragmentationStage::MOVE_TO_SWAP_PAGE)
    {
        //Move records one by one to the swap page
        RecordStorageRecord* record = (RecordStorageRecord*)defragmentPage->data;
        RecordStorageRecord* swapRecord = (RecordStorageRecord*)defragmentSwapPage->data;
        RecordStorageRecord* freeSpacePtr = (RecordStorageRecord*)defragmentSwapPage->data;

        //This loop goes through all records, if it finds a record that needs to be moved (and wasn't already), it will move it
        while (IsRecordValid(*defragmentPage, record))
        {
            //Only copy record if it is not outdated (e.g. newer version on a different page)
            if (GetRecord(record->recordId) == record && record->recordActive)
            {
                //Check if record was already moved to swap page
                bool found = false;
                while (IsRecordValid(*defragmentSwapPage, swapRecord)) {
                    if (record->recordId == swapRecord->recordId) {
                        freeSpacePtr = (RecordStorageRecord*)((u8*)swapRecord + swapRecord->recordLength);
                        found = true;
                        break;
                    }
                    swapRecord = (RecordStorageRecord*)((u8*)swapRecord + swapRecord->recordLength);
                }
                //If the record was not found on the swap page, we must move it
                if (!found) {
                    logt("RS", "Moving record %u", (u32)record);
                    GS->flashStorage.CacheAndWriteData((u32*)record, (u32*)freeSpacePtr, record->recordLength, nullptr, (u32)FlashUserTypes::DEFAULT);
                    return;
                }
            }
            //Update reference to record
            record = (RecordStorageRecord*)((u8*)record + record->recordLength);
        }

        defragmentationStage = DefragmentationStage::WRITE_PAGE_HEADER;
    }
    
    if (defragmentationStage == DefragmentationStage::WRITE_PAGE_HEADER)
    {

        //Check the current versionCounter of all pages
        u16 maxVersionCounter = 0;
        for (u32 i = 0; i < RECORD_STORAGE_NUM_PAGES; i++) {
            RecordStoragePage& page = getPage(i);
            if (GetPageState(page) != RecordStoragePageState::ACTIVE) continue;
            if (page.versionCounter > maxVersionCounter) {
                maxVersionCounter = page.versionCounter;
            }
        }

        //Currently, we do not support more than 65000 erase cycles
        if (maxVersionCounter == UINT16_MAX) {
            GS->logger.LogCustomError(CustomErrorTypes::FATAL_RECORD_STORAGE_ERASE_CYCLES_HIGH, (u32)maxVersionCounter);
            SIMEXCEPTION(IllegalStateException);
            return;
        }
        else if (maxVersionCounter >= UINT16_MAX) {
            GS->logger.LogCustomError(CustomErrorTypes::WARN_RECORD_STORAGE_ERASE_CYCLES_HIGH, (u32)maxVersionCounter);
            SIMEXCEPTION(IllegalStateException);
        }

        //Next, write Active to this page with a version that is newer than all other pages
        RecordStoragePage pageHeader;
        CheckedMemset(&pageHeader, 0, sizeof(pageHeader));
        pageHeader.magicNumber = RECORD_STORAGE_ACTIVE_PAGE_MAGIC_NUMBER;
        pageHeader.versionCounter = maxVersionCounter + 1;

        GS->flashStorage.CacheAndWriteData((u32*)&pageHeader, (u32*)defragmentSwapPage, SIZEOF_RECORD_STORAGE_PAGE_HEADER, nullptr, (u32)FlashUserTypes::DEFAULT);
        
        defragmentationStage = DefragmentationStage::ERASE_OLD_PAGE;
    }
    else if (defragmentationStage == DefragmentationStage::ERASE_OLD_PAGE)
    {
        //Finally, erase the page that we just swapped
        GS->flashStorage.ErasePage(((u32)defragmentPage - FLASH_REGION_START_ADDRESS) / FruityHal::GetCodePageSize(), this, (u32)FlashUserTypes::DEFAULT);

        defragmentationStage = DefragmentationStage::FINALIZE;
    }
    else if (defragmentationStage == DefragmentationStage::FINALIZE)
    {
        defragmentationStage = DefragmentationStage::NO_DEFRAGMENTATION;

        //Call the listener manually because we did not queue another task
        ProcessQueue(true);
    }
}


/*##################################### 
# Various functions to read and helpers
##################################### */


//This will call the callback of the operation and will remove it from the queue
void RecordStorage::RecordOperationFinished(RecordStorageOperation& op, RecordStorageResultCode code)
{
    ExecuteCallback(op, code);

    //Clear operation from operation queue
    opQueue.DiscardNext();

    //Continue of tasks are available
    ProcessQueue(true);
}

void RecordStorage::ExecuteCallback(RecordStorageOperation & op, RecordStorageResultCode code)
{
    if (op.callback != nullptr) {
        if (op.type == (u8)RecordStorageOperationType::SAVE_RECORD)
        {
            SaveRecordOperation* sop = (SaveRecordOperation*)&op;
            if (op.userDataLength > 0)
            {
                // Make sure the user data is 4 byte aligned. That way the user can give
                // us any struct of alignment <= 4 without problems.
                DYNAMIC_ARRAY(buffer, op.userDataLength);
                CheckedMemcpy(buffer, ((u8*)&op) + SIZEOF_RECORD_STORAGE_SAVE_RECORD_OP + sop->dataLength, op.userDataLength)
                op.callback->RecordStorageEventHandler(sop->recordId, code, op.userType, buffer, op.userDataLength);
            }
            else
            {
                op.callback->RecordStorageEventHandler(sop->recordId, code, op.userType, nullptr, 0);
            }
        }
        else if (op.type == (u8)RecordStorageOperationType::DEACTIVATE_RECORD)
        {
            DeactivateRecordOperation* dop = (DeactivateRecordOperation*)&op;
            op.callback->RecordStorageEventHandler(dop->recordId, code, op.userType, ((u8*)&op) + SIZEOF_RECORD_STORAGE_DEACTIVATE_RECORD_OP, op.userDataLength);
        }
    }
}

RecordStoragePage* RecordStorage::FindPageToDefragment() const
{
    RecordStoragePage* pageToDefragment = nullptr;
    u32 maxFreeSpace = 0;

    //Go through all pages to find page with the most free space
    for (u32 i = 0; i< RECORD_STORAGE_NUM_PAGES; i++)
    {
        //Check if this page is active
        RecordStoragePage& page = getPage(i);
        if (GetPageState(page) != RecordStoragePageState::ACTIVE) continue;

        u16 freeSpace = GetFreeSpaceWhenDefragmented(page);

        if (freeSpace > maxFreeSpace) {
            maxFreeSpace = freeSpace;
            pageToDefragment = &page;
        }
    }
    return pageToDefragment;
}

RecordStoragePage& RecordStorage::getPage(u32 index) const
{
    if (index >= RECORD_STORAGE_NUM_PAGES)
    {
        SIMEXCEPTION(IllegalArgumentException);
    }
    if (startPage == nullptr)
    {
        SIMEXCEPTION(IllegalStateException);
    }
    return *(RecordStoragePage*)(startPage + FruityHal::GetCodePageSize() * index);
}

//Will return only the data of the record and will return nullptr if record has been deactivated
SizedData RecordStorage::GetRecordData(u16 recordId) const
{
    RecordStorageRecord* record = GetRecord(recordId);

    SizedData result;
    if (record == nullptr || !record->recordActive) {
        result.data = nullptr;
        result.length = 0;
    }
    else {
        result.data = record->data;
        result.length = record->recordLength - SIZEOF_RECORD_STORAGE_RECORD_HEADER - record->padding;
    }

    return result;
}

//Will return the latest version of a record if its structure is valid
//Will also return a record if it has been deactivated
RecordStorageRecord* RecordStorage::GetRecord(u16 recordId) const
{
    RecordStorageRecord* result = nullptr;

    //Go through all pages
    for(u32 i = 0; i< RECORD_STORAGE_NUM_PAGES; i++)
    {
        //Check if this page is active
        RecordStoragePage& page = getPage(i);
        if(GetPageState(page) != RecordStoragePageState::ACTIVE) continue;

        //Get first record
        RecordStorageRecord* record = (RecordStorageRecord*)page.data;

        //Iterate through all valid records
        while(IsRecordValid(page, record))
        {
            //If the record matches the requested recordId, remember it
            //The record with the biggest versionCounter is the valid one
            if(record->recordId == recordId){
                if(result != nullptr){
                    if(record->versionCounter > result->versionCounter){
                        result = record;
                    }
                } else {
                    result = record;
                }
            }

            record = (RecordStorageRecord*)((u8*)record + record->recordLength);
        }
    }

    return result;
}

//Returns a pointer to the free space, otherwise returns nullptr
u8* RecordStorage::GetFreeRecordSpace(u16 dataLength) const
{
    //Go through all pages
    for(u32 i = 0; i< RECORD_STORAGE_NUM_PAGES; i++)
    {
        //Check if this page is active
        RecordStoragePage& page = getPage(i);
        if(GetPageState(page) != RecordStoragePageState::ACTIVE) continue;

        //Get first record
        RecordStorageRecord* record = (RecordStorageRecord*)page.data;

        //Iterate through all valid records
        while(IsRecordValid(page, record))
        {
            record = (RecordStorageRecord*)((u8*)record + record->recordLength);
        }

        //Check if we have enough space left till the end of the page
        if(((u32)record - (u32)&page + dataLength) <= FruityHal::GetCodePageSize()){
            return (u8*)record;
        }
    }

    return nullptr;
}

u16 RecordStorage::GetFreeSpaceOnPage(const RecordStoragePage& page) const
{
    if (GetPageState(page) != RecordStoragePageState::ACTIVE) return 0;

    //Get first record
    const RecordStorageRecord* record = (const RecordStorageRecord*)page.data;
    //Iterate through all valid records until it jumps to the first invalid record
    while (IsRecordValid(page, record)) {
        record = (const RecordStorageRecord*)((const u8*)record + record->recordLength);
    }

    return (FruityHal::GetCodePageSize() - ((u32)record - (u32)&page));
}

//Calculates the free storage that would be available when defragmenting the page
u16 RecordStorage::GetFreeSpaceWhenDefragmented(const RecordStoragePage& page) const
{
    u32 usedSpace = SIZEOF_RECORD_STORAGE_PAGE_HEADER;

    //Should not happen, page is not active
    if(GetPageState(page) != RecordStoragePageState::ACTIVE) return 0;

    //Get first record
    const RecordStorageRecord * record = (const RecordStorageRecord *)page.data;

    while (IsRecordValid(page, record))
    {
        //Only copy record if it is not outdated (e.g. newer version on a different page)
        if (GetRecord(record->recordId) == record && record->recordActive)
        {
            usedSpace += record->recordLength;
        }

        record = (const RecordStorageRecord*)((const u8*)record + record->recordLength);
    }

    return (FruityHal::GetCodePageSize() - usedSpace);
}

RecordStoragePage* RecordStorage::GetSwapPage() const
{
    for(u32 i = 0; i< RECORD_STORAGE_NUM_PAGES; i++)
    {
        RecordStoragePage& page = getPage(i);
        if(GetPageState(page) == RecordStoragePageState::EMPTY){
            return &page;
        }
    }

    return nullptr;
}

//This will only check if a record is valid in terms of crc and basic check against corruption
//If a record is markes as deactivated, it is still valid
bool RecordStorage::IsRecordValid(const RecordStoragePage& page, RecordStorageRecord const * record) const
{
    //Check if length is within page boundaries
    if(record == nullptr || ((u32)record - (u32)&page) + record->recordLength > FruityHal::GetCodePageSize()){
        return false;
    }

    //Check if CRC is valid
    if(Utility::CalculateCrc8(((const u8*)record) + sizeof(u16), record->recordLength - 2) != record->crc){
        logt("ERROR", "crc %u not matching %u", record->crc, Utility::CalculateCrc8(((const u8*)record) + sizeof(u16), record->recordLength - 2));
        GS->logger.LogCustomError(CustomErrorTypes::FATAL_RECORD_CRC_WRONG, record->recordId);

        return false;
    }

    return true;
}

RecordStoragePageState RecordStorage::GetPageState(const RecordStoragePage& page) const
{
    if(page.magicNumber == RECORD_STORAGE_ACTIVE_PAGE_MAGIC_NUMBER){
        return RecordStoragePageState::ACTIVE;
    }

    for(u32 i=0; i<FruityHal::GetCodePageSize()/sizeof(u32); i++){
        if(((const u32*)&page)[i] != 0xFFFFFFFF){
            return RecordStoragePageState::CORRUPT;
        }
    }

    return RecordStoragePageState::EMPTY;
}

//This triggers the recordStorage queue processing if not already running
void RecordStorage::ProcessQueue(bool force)
{
    if (!processQueueInProgress || force) {
        FlashStorageItemExecuted(nullptr, FlashStorageError::SUCCESS);
    }
}

//This is the handler that is notified once a FlashStorage task has executed
void RecordStorage::FlashStorageItemExecuted(FlashStorageTaskItem* task, FlashStorageError errorCode)
{
    if (task == nullptr || task->header.userType == (u32)FlashUserTypes::DEFAULT)
    {
        processQueueInProgress = true;

        //TODO: Use errorCode

        //If either a repair or defrag is in Progress, do nothing, these are called from the QueueEmptyHandler
        if (repairStage != RepairStage::NO_REPAIR || defragmentationStage != DefragmentationStage::NO_DEFRAGMENTATION) {
            processQueueInProgress = false;
            return;
        }
        //Check if we have other high level operations to be executed
        else if (opQueue._numElements > 0)
        {
            SizedData data = opQueue.PeekNext();
            RecordStorageOperation* op = (RecordStorageOperation*)data.data;

            if (op->type == (u8)RecordStorageOperationType::SAVE_RECORD)
            {
                op->flashStorageErrorCode = errorCode;
                SaveRecordInternal(*(SaveRecordOperation*)op);
            }
            else if (op->type == (u8)RecordStorageOperationType::DEACTIVATE_RECORD)
            {
                op->flashStorageErrorCode = errorCode;
                DeactivateRecordInternal(*(DeactivateRecordOperation*)op);
            }
        }

        if (opQueue._numElements == 0) {
            processQueueInProgress = false;
        }
    }
    else if (task != nullptr && task->header.userType == (u32)FlashUserTypes::LOCK_DOWN)
    {
        //If we were not successful and havn't exceeded our retry counter, we try again. 
        if (errorCode != FlashStorageError::SUCCESS && lockDownRetryCounter < LOCK_DOWN_RETRY_MAX)
        {
            lockDownRetryCounter++;
            recordStorageLockDown = false; //Will be set to true in LockDownAndClearAllSettings again.
            if (LockDownAndClearAllSettings(lockDownModuleId, lockDownCallback, lockDownUserType) != RecordStorageResultCode::SUCCESS)
            {
                GS->node.Reboot(SEC_TO_DS(4), RebootReason::FACTORY_RESET_FAILED);
            }
        }
        //If we were not successful and have exceeded our retry counter, we inform the callback about the failure.
        else if (errorCode != FlashStorageError::SUCCESS)
        {
            if (lockDownCallback != nullptr)
            {
                lockDownCallback->RecordStorageEventHandler(0, RecordStorageResultCode::BUSY, lockDownUserType, nullptr, 0);
            }
            GS->node.Reboot(SEC_TO_DS(4), RebootReason::FACTORY_RESET_FAILED);
        }
        //If we were successful we inform the callback about success.
        else
        {
            RepairPages();
        }
    }
    else
    {
        SIMEXCEPTION(IllegalArgumentException);
    }
}

void RecordStorage::FlashStorageQueueEmptyHandler()
{
    //Repair and Defragment are only executed once the queue is empty to guarantee success
    if (repairStage != RepairStage::NO_REPAIR) {
        RepairPages();
        //Repair might have ended without queuing another task
        if(repairStage == RepairStage::NO_REPAIR && defragmentationStage != DefragmentationStage::NO_DEFRAGMENTATION){
            DefragmentPage(*defragmentPage, false);
        }
    }
    else if (defragmentationStage != DefragmentationStage::NO_DEFRAGMENTATION)
    {
        DefragmentPage(*defragmentPage, false);
    }
}
