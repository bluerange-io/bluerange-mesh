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

#include <TestRecordStorage.h>
#include <Config.h>
#include <GlobalState.h>
#include <RecordStorage.h>
#include <SystemTest.h>
#include <Utility.h>
#include <Logger.h>

extern "C" {
}

#ifdef SIM_ENABLED

TestRecordStorage::TestRecordStorage()
{

}


void TestRecordStorage::Start()
{
	Config->terminalMode = TerminalMode::TERMINAL_PROMPT_MODE;
	Logger::getInstance()->enableTag("NEWSTORAGE");

	startPage = (u8*)Conf::GetSettingsPageBaseAddress();
	numPages = RECORD_STORAGE_NUM_PAGES;

	//Initialize
	RecordStorage::getInstance();

	sim_commit_flash_operations();

	TestCleanup();
	TestSave();
	TestRandomSingleRecordUpdates();
	TestDeactivateRecord();
	TestGetNonExistentRecord();
	TestGetNonExistentRecordAfterStore();
	TestRandomMultiRecordUpdates();
	TestAsyncQueuing();
	TestFlashBusy();

	printf("sdf");
}

void TestRecordStorage::TestCleanup()
{

	logt("ERROR", "---- TEST CLEANUP ----");

	//###### Test with all empty pages
	logt("ERROR", "---- TEST CLEANUP EMPTY PAGES ----");
	memset(startPage, 0xff, numPages*PAGE_SIZE);

	GS->recordStorage->RepairPages();

	sim_commit_flash_operations();

	//Check if success
	for (int i = 0; i < numPages; i++) {
		RecordStoragePage* page = (RecordStoragePage*)(startPage + PAGE_SIZE * i);
		//First page must be swap page
		if (i == 0 && GS->recordStorage->GetPageState(page) != RecordStoragePageState::RECORD_STORAGE_PAGE_EMPTY) {
			logt("ERROR", "---- FAIL ----");
			throw new std::exception("TEST FAILED");
		}
		//other pages should be empty
		if (i > 0 && GS->recordStorage->GetPageState(page) != RecordStoragePageState::RECORD_STORAGE_PAGE_ACTIVE) {
			logt("ERROR", "---- FAIL ----");
			throw new std::exception("TEST FAILED");
		}
	}

	//###### Test with two corrupt pages
	logt("ERROR", "---- TEST CLEANUP CORRUPT PAGES ----");

	memset(startPage, 0xff, numPages*PAGE_SIZE);
	memset(startPage, 0x0101, 2);
	memset(startPage + PAGE_SIZE * 2, 0x11, 50);

	GS->recordStorage->RepairPages();

	sim_commit_flash_operations();

	//Check if success
	for (int i = 0; i < numPages; i++) {
		RecordStoragePage* page = (RecordStoragePage*)(startPage + PAGE_SIZE * i);
		//First page must be swap page
		if (i == 0 && GS->recordStorage->GetPageState(page) != RecordStoragePageState::RECORD_STORAGE_PAGE_EMPTY) {
			logt("ERROR", "---- FAIL ----");
			throw new std::exception("TEST FAILED");
		}
		//other pages should be empty
		if (i > 0 && GS->recordStorage->GetPageState(page) != RecordStoragePageState::RECORD_STORAGE_PAGE_ACTIVE) {
			logt("ERROR", "---- FAIL ----");
			throw new std::exception("TEST FAILED");
		}
	}
	
	//###### Test with active page
	logt("ERROR", "---- TEST CLEANUP WITH ACTIVE PAGE ----");

	memset(startPage, 0xff, numPages*PAGE_SIZE);
	RecordStoragePage* activePage = (RecordStoragePage*)startPage;
	activePage->magicNumber = RECORD_STORAGE_ACTIVE_PAGE_MAGIC_NUMBER;
	activePage->versionCounter = 1;
	
	GS->recordStorage->RepairPages();

	sim_commit_flash_operations();

	//Check if success
	for (int i = 1; i < numPages; i++) {
		RecordStoragePage* page = (RecordStoragePage*)(startPage + PAGE_SIZE * i);
		//First page must be active page
		if (i == 0 && GS->recordStorage->GetPageState(page) != RecordStoragePageState::RECORD_STORAGE_PAGE_ACTIVE) {
			logt("ERROR", "---- FAIL ----");
			throw new std::exception("TEST FAILED");
		}
		//second page must be swap page
		if (i == 1 && GS->recordStorage->GetPageState(page) != RecordStoragePageState::RECORD_STORAGE_PAGE_EMPTY) {
			logt("ERROR", "---- FAIL ----");
			throw new std::exception("TEST FAILED");
		}
		//other pages should be empty
		if (i > 1 && i < numPages && GS->recordStorage->GetPageState(page) != RecordStoragePageState::RECORD_STORAGE_PAGE_ACTIVE) {
			logt("ERROR", "---- FAIL ----");
			throw new std::exception("TEST FAILED");
		}
	}
}


void TestRecordStorage::TestSave()
{
	u32 cmp = 0;

	//Setup
	memset(startPage, 0xff, numPages*PAGE_SIZE);
	GS->recordStorage->RepairPages();

	sim_commit_flash_operations();

	logt("ERROR", "---- TEST SAVE ----");

	//Save record 1
	u8 data[] = { 1,2,3,4,5,6,7,8 };
	GS->recordStorage->SaveRecord(1, data, 8, NULL,0);

	sim_commit_flash_operations();

	sizedData dataB = GS->recordStorage->GetRecordData(1);

	cmp = memcmp(data, dataB.data, sizeof(data));
	if (dataB.length != sizeof(data) || cmp != 0) {
		logt("ERROR", "---- FAIL ----");
		throw new std::exception("TEST FAILED");
	}

	//Save record 2
	u8 data2[] = { 1,2,3,4,5,6,7,8 };
	GS->recordStorage->SaveRecord(2, data2, 8, NULL, 0);

	sim_commit_flash_operations();

	sizedData data2B = GS->recordStorage->GetRecordData(2);

	if (data2B.length != sizeof(data2)) {
		logt("ERROR", "---- FAIL ----");
		throw new std::exception("TEST FAILED");
	}
	if (memcmp(data2, data2B.data, sizeof(data2)) != 0) {
		logt("ERROR", "---- FAIL ----");
		throw new std::exception("TEST FAILED");
	}

	//Update record 1
	u8 data3[] = { 1,2,3,4 };
	GS->recordStorage->SaveRecord(1, data, 4, NULL, 0);

	sim_commit_flash_operations();

	sizedData data3B = GS->recordStorage->GetRecordData(1);

	cmp = memcmp(data3, data3B.data, sizeof(data3));
	if (data3B.length != sizeof(data3) || cmp != 0) {
		logt("ERROR", "---- FAIL ----");
		throw new std::exception("TEST FAILED");
	}

	printf("SUCCESS");
}

//TODO: Check what happens if we use a record size that is larger than a page

void TestRecordStorage::TestRandomSingleRecordUpdates()
{
	logt("ERROR", "---- TEST RANDOM SINGLE RECORD UPDATE ----");

	u32 cmp = 0;

	//Setup
	memset(startPage, 0xff, numPages*PAGE_SIZE);
	GS->recordStorage->RepairPages();

	sim_commit_flash_operations();

	//Save record 1
	u8 data[500];

	for (int i = 0; i < 2000; i++) 
	{
		memset(data, i, 500);

		u16 length = 4+(Utility::GetRandomInteger() % 100) / 4 * 4;

		GS->recordStorage->SaveRecord(8, data, length, NULL, 0);

		sim_commit_flash_operations();

		//Check if we can read back the correct record
		sizedData dataB = GS->recordStorage->GetRecordData(8);

		cmp = memcmp(data, dataB.data, length);
		if (dataB.length != length || cmp != 0) {
			logt("ERROR", "---- FAIL ----");
			throw new std::exception("TEST FAILED");
		}
	}
}

void TestRecordStorage::TestGetNonExistentRecord()
{
	logt("ERROR", "---- TEST GET NON EXISTENT RECORD ----");

	//Setup
	memset(startPage, 0xff, numPages*PAGE_SIZE);
	GS->recordStorage->RepairPages();

	sizedData dataB = GS->recordStorage->GetRecordData(7);

	if (dataB.length != 0) {
		logt("ERROR", "---- FAIL ----");
		throw new std::exception("TEST FAILED");
	}
}

void TestRecordStorage::TestGetNonExistentRecordAfterStore()
{
	logt("ERROR", "---- TEST GET NON EXISTENT RECORD AFTER STORE ----");

	//Setup
	memset(startPage, 0xff, numPages*PAGE_SIZE);
	GS->recordStorage->RepairPages();

	u8 data[] = { 1,2,3,4 };
	GS->recordStorage->SaveRecord(5, data, 4, NULL, 0);

	sizedData dataB = GS->recordStorage->GetRecordData(7);

	if (dataB.length != 0) {
		logt("ERROR", "---- FAIL ----");
		throw new std::exception("TEST FAILED");
	}
}

void TestRecordStorage::TestDeactivateRecord()
{
	logt("ERROR", "---- CLEANUP ----");

	//Setup
	memset(startPage, 0xff, numPages*PAGE_SIZE);
	GS->recordStorage->RepairPages();

	sim_commit_flash_operations();

	logt("ERROR", "---- TEST DEACTIVATE RECORD ----");

	RecordStoragePage* usedPage = (RecordStoragePage*)(startPage + PAGE_SIZE);

	//Save record
	u8 data[] = { 1,2,3,4 };
	GS->recordStorage->SaveRecord(5, data, 4, NULL, 0);

	//Update record
	u8 data2[] = { 1,2,1,2 };
	GS->recordStorage->SaveRecord(5, data2, 4, NULL, 0);

	//Deactivate record
	GS->recordStorage->DeactivateRecord(5, NULL, 0);

	//Committing done after all three steps are queued to test async
	sim_commit_flash_operations();

	//Record must still be valid
	RecordStorageRecord* record = GS->recordStorage->GetRecord(5);
	if (!GS->recordStorage->IsRecordValid(usedPage, record))
	{
		logt("ERROR", "---- FAIL ----");
		throw new std::exception("TEST FAILED");
	}

	//Record should not exist anymore
	sizedData dataB = GS->recordStorage->GetRecordData(5);
	if (dataB.length != 0) {
		logt("ERROR", "---- FAIL ----");
		throw new std::exception("TEST FAILED");
	}

	//Defragment the page
	GS->recordStorage->DefragmentPage(usedPage);

	sim_commit_flash_operations();

	//There should be no record on that page anymore
	RecordStorageRecord* firstRecord = (RecordStorageRecord*)usedPage->data;
	if (firstRecord->recordId != 0xFFFF) {
		logt("ERROR", "---- FAIL ----");
		throw new std::exception("TEST FAILED");
	}

	//Update record again
	u8 data3[] = { 1,2,1,2,3,4,3,4 };
	GS->recordStorage->SaveRecord(5, data, 8, NULL, 0);

	//Record should exist again
	sizedData dataC = GS->recordStorage->GetRecordData(5);
	if (dataC.length != 8) {
		logt("ERROR", "---- FAIL ----");
		throw new std::exception("TEST FAILED");
	}

}

void TestRecordStorage::TestFlashBusy()
{
	logt("ERROR", "---- CLEANUP ----");

	//Setup
	memset(startPage, 0xff, numPages*PAGE_SIZE);
	GS->recordStorage->RepairPages();

	sim_commit_flash_operations();

	logt("ERROR", "---- TEST SINGLE FLASH BUSY FAIL WILL WORK ----");

	RecordStoragePage* usedPage = (RecordStoragePage*)(startPage + PAGE_SIZE);

	//Store will use 1 write operation but will retry a few times
	u8 data[] = { 1,2,1,2,3,4,3,4 };
	GS->recordStorage->SaveRecord(5, data, 8, this, 1);

	u8 failData[] = { 1,0,0,0,0,0,0,0,0,0 };
	sim_commit_some_flash_operations(failData, 10);

	RecordStorageRecord* record = GS->recordStorage->GetRecord(5);
	if (!GS->recordStorage->IsRecordValid(usedPage, record))
	{
		logt("ERROR", "---- FAIL ----");
		throw new std::exception("TEST FAILED");
	}

	logt("ERROR", "---- MULTI FLASH BUSY FAIL NOT WRITE RECORD ----");

	GS->recordStorage->SaveRecord(65001, data, 8, this, 2);

	u8 failData2[] = { 1,1,1,1,1,1,0,0,0,0 };
	sim_commit_some_flash_operations(failData2, 10);



}

void TestRecordStorage::TestAsyncQueuing()
{
	logt("ERROR", "---- CLEANUP ----");

	//Setup
	memset(startPage, 0xff, numPages*PAGE_SIZE);
	GS->recordStorage->RepairPages(); 

	sim_commit_flash_operations();

	logt("ERROR", "---- TEST ASYNC QUEUING ----");

	u32 err = 0;

	//Deactivate non-existent record
	GS->recordStorage->DeactivateRecord(2, NULL, 0);

	u8 i = 1;
	u8 data[] = { 0,1,2,3,4,5,6,7,8,9,10,11 };
	do {
		data[0] = i;
		err = GS->recordStorage->SaveRecord(i, data, 12, NULL, 0);

		if (i == 4) {
			GS->recordStorage->DeactivateRecord(3, NULL, 0);
		}

		i++;
	} while (err == 0);

	//Last call should have returned busy
	if (err != RecordStorageResultCode::RECORD_STORAGE_STORE_BUSY) {
		logt("ERROR", "---- FAIL ----");
		throw new std::exception("TEST FAILED");
	}

	//We should be able to queue 5 write ops
	if (i < 5) {
		logt("ERROR", "---- FAIL ----");
		throw new std::exception("TEST FAILED");
	}

	logt("ERROR", "Queued %u items", i);

	//Committing done after all three steps are queued to test async
	sim_commit_flash_operations();

	//Record 2 must not be deactivated
	if (GS->recordStorage->GetRecord(2) == NULL) {
		logt("ERROR", "---- FAIL ----");
		throw new std::exception("TEST FAILED");
	}

	//Record 3 must be deactivated
	if (GS->recordStorage->GetRecord(3)->recordActive != false) {
		logt("ERROR", "---- FAIL ----");
		throw new std::exception("TEST FAILED");
	}

}

//Must be below 256 because of test limit when storing length in byte
#define MULTI_RECORD_TEST_NUM_RECORD_IDS 20
//Must be below 256 because of test limit when storing length in byte
#define MULTI_RECORD_TEST_RECORD_MAX_SIZE 40
u8 testBuffer[MULTI_RECORD_TEST_RECORD_MAX_SIZE * MULTI_RECORD_TEST_NUM_RECORD_IDS];
void TestRecordStorage::TestRandomMultiRecordUpdates()
{
	logt("ERROR", "---- TEST RANDOM MULTI RECORD UPDATE ----");

	u32 cmp = 0;

	//Setup
	memset(startPage, 0xff, numPages*PAGE_SIZE);
	GS->recordStorage->RepairPages();

	memset(testBuffer, 0x00, sizeof(testBuffer));

	//Save record 1
	u8 data[500];

	for (int i = 0; i < 2000; i++)
	{
		u16 length = 4+(Utility::GetRandomInteger() % (MULTI_RECORD_TEST_RECORD_MAX_SIZE - SIZEOF_RECORD_STORAGE_RECORD_HEADER)) / 4 * 4;
		u16 randomRecordId = (Utility::GetRandomInteger() % MULTI_RECORD_TEST_NUM_RECORD_IDS) + 1;

		//Generate record data
		memset(data, randomRecordId, 500);
		data[1] = (u8)Utility::GetRandomInteger(); //Store random integer to identify this record
		data[2] = (u8)length; //Store length in data to compare it later

		//Store the test record ourself in a big array
		memcpy(testBuffer + MULTI_RECORD_TEST_RECORD_MAX_SIZE * (randomRecordId-1), data, length);

		//Store it using the tested class
		GS->recordStorage->SaveRecord(randomRecordId, data, length, NULL, 0);


		sim_commit_flash_operations();

		//Check if we can read all records back in their latest version
		for (int i = 0; i < MULTI_RECORD_TEST_NUM_RECORD_IDS; i++) {
			u8* bufferedData = &(testBuffer[MULTI_RECORD_TEST_RECORD_MAX_SIZE * i]);
			u8 storedRecordId = bufferedData[0];
			u8 storedRecordRandomInt = bufferedData[1];
			u8 storedRecordLength = bufferedData[2];
			//If we have a non-zero value, this record had been stored in ram previously, try to read it back and compare
			if (storedRecordId != 0) {
				sizedData dataB = GS->recordStorage->GetRecordData(storedRecordId);

				//Record not found fail
				if (dataB.length == 0) {
					logt("ERROR", "---- FAIL ----");
					throw new std::exception("TEST FAILED");
				}
				//Record wrong length fail
				if (dataB.length != storedRecordLength) {
					logt("ERROR", "---- FAIL ----");
					throw new std::exception("TEST FAILED");
				}
				//Record data not matching fail
				u32 result = memcmp(bufferedData, dataB.data, dataB.length);
				if (result != 0) {
					logt("ERROR", "---- FAIL ----");
					throw new std::exception("TEST FAILED");
				}
			}
			
		}
	}
}

void TestRecordStorage::RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength)
{
	if (userType == 1) {
		if (resultCode != RecordStorageResultCode::RECORD_STORAGE_STORE_SUCCESS) {
			logt("ERROR", "---- FAIL ----");
			throw new std::exception("TEST FAILED");
		}
	}
	if (userType == 2) {
		if (resultCode != RecordStorageResultCode::RECORD_STORAGE_STORE_BUSY) {
			logt("ERROR", "---- FAIL ----");
			throw new std::exception("TEST FAILED");
		}
	}
}

#endif
