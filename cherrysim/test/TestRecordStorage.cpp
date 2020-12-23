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
#include "gtest/gtest.h"
#include <Config.h>
#include <GlobalState.h>
#include <RecordStorage.h>
#include <SystemTest.h>
#include <CherrySim.h>
#include <Utility.h>
#include <Logger.h>
#include <CherrySimTester.h>

class TestRecordStorage : public ::testing::Test, public RecordStorageEventListener
{
private:
    CherrySimTester* tester = nullptr;
public:
    u8* startPage;
    static constexpr u16 numPages = RECORD_STORAGE_NUM_PAGES;

    void SetUp() override
    {
        //We have to boot up a simulator for this test because the PacketQueue uses the Logger
        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
        simConfig.nodeConfigName.insert( { "prod_sink_nrf52", 1 } );
        tester = new CherrySimTester(testerConfig, simConfig);
        tester->sim->nodes[0].nodeConfiguration = "prod_sink_nrf52";
        tester->Start();
        NodeIndexSetter setter(0);

        startPage = (u8*)Utility::GetSettingsPageBaseAddress();
    }

    void TearDown() override
    {
        delete tester;
    }

    //We redirect some calls to private functions of RecordStorage
    RecordStoragePageState GetPageState(RecordStoragePage* page) {
        return GS->recordStorage.GetPageState(*page);
    }
    void RepairPages() {
        GS->recordStorage.RepairPages();
    }
    bool IsRecordValid(RecordStoragePage* page, RecordStorageRecord* record) {
        return GS->recordStorage.IsRecordValid(*page, record);
    }
    void DefragmentPage(RecordStoragePage* pageToDefragment, bool force) {
        GS->recordStorage.DefragmentPage(*pageToDefragment, false);
    }

    void RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength) override
    {
        if (userType == 1) {
            if (resultCode != RecordStorageResultCode::SUCCESS) {
                logt("ERROR", "---- FAIL ----");                    //LCOV_EXCL_LINE assertion
                SIMEXCEPTION(IllegalStateException); //TEST FAILED    //LCOV_EXCL_LINE assertion
            }
        }
        if (userType == 2) {
            if (resultCode != RecordStorageResultCode::BUSY) {
                logt("ERROR", "---- FAIL ----");                   //LCOV_EXCL_LINE assertion
                SIMEXCEPTION(IllegalStateException); //TEST FAILED //LCOV_EXCL_LINE assertion
            }
        }
    }
};


TEST_F(TestRecordStorage, TestCleanup) {
    NodeIndexSetter setter(0);
    logt("WARNING", "---- TEST CLEANUP ----");

    //###### Test with all empty pages
    logt("WARNING", "---- TEST CLEANUP EMPTY PAGES ----");
    CheckedMemset(startPage, 0xff, numPages*FruityHal::GetCodePageSize());

    RepairPages();

    cherrySimInstance->SimCommitFlashOperations();

    //Check if success
    for (int i = 0; i < numPages; i++) {
        RecordStoragePage* page = (RecordStoragePage*)(startPage + FruityHal::GetCodePageSize() * i);
        //First page must be swap page
        if (i == 0 && GetPageState(page) != RecordStoragePageState::EMPTY) {
            FAIL() << "FIrst page not an empty swap page"; //LCOV_EXCL_LINE assertion
        }
        //other pages should be active
        if (i > 0 && GetPageState(page) != RecordStoragePageState::ACTIVE) {
            FAIL() << "Second page not active"; //LCOV_EXCL_LINE assertion
        }
    }

    //###### Test with two corrupt pages
    logt("WARNING", "---- TEST CLEANUP CORRUPT PAGES ----");

    CheckedMemset(startPage, 0xff, numPages*FruityHal::GetCodePageSize());
    CheckedMemset(startPage, 0x01, 2);
    CheckedMemset(startPage + FruityHal::GetCodePageSize() * 2, 0x11, 50);

    RepairPages();

    cherrySimInstance->SimCommitFlashOperations();

    //Check if success
    for (int i = 0; i < numPages; i++) {
        RecordStoragePage* page = (RecordStoragePage*)(startPage + FruityHal::GetCodePageSize() * i);
        //First page must be swap page
        if (i == 0 && GetPageState(page) != RecordStoragePageState::EMPTY) {
            FAIL() << "First page not empty swap page"; //LCOV_EXCL_LINE assertion
        }
        //other pages should be empty
        if (i > 0 && GetPageState(page) != RecordStoragePageState::ACTIVE) {
            FAIL() << "Second page not active"; //LCOV_EXCL_LINE assertion
        }
    }
    
    //###### Test with active page
    logt("WARNING", "---- TEST CLEANUP WITH ACTIVE PAGE ----");

    CheckedMemset(startPage, 0xff, numPages*FruityHal::GetCodePageSize());
    RecordStoragePage* activePage = (RecordStoragePage*)startPage;
    activePage->magicNumber = RECORD_STORAGE_ACTIVE_PAGE_MAGIC_NUMBER;
    activePage->versionCounter = 1;
    
    RepairPages();

    cherrySimInstance->SimCommitFlashOperations();

    //Check if success
    for (int i = 1; i < numPages; i++) {
        RecordStoragePage* page = (RecordStoragePage*)(startPage + FruityHal::GetCodePageSize() * i);
        //First page must be active page
        if (i == 0 && GetPageState(page) != RecordStoragePageState::ACTIVE) {
            FAIL() << "First page not active"; //LCOV_EXCL_LINE assertion
        }
        //second page must be swap page
        if (i == 1 && GetPageState(page) != RecordStoragePageState::EMPTY) {
            FAIL() << "Second page not empty"; //LCOV_EXCL_LINE assertion
        }
        //other pages should be empty
        if (i > 1 && i < numPages && GetPageState(page) != RecordStoragePageState::ACTIVE) {
            FAIL() << "Other pages not active"; //LCOV_EXCL_LINE assertion
        }
    }
}

TEST_F(TestRecordStorage, TestSave) {
    NodeIndexSetter setter(0);
    u32 cmp = 0;

    //Setup
    CheckedMemset(startPage, 0xff, numPages*FruityHal::GetCodePageSize());
    RepairPages();

    cherrySimInstance->SimCommitFlashOperations();

    logt("WARNING", "---- TEST SAVE ----");

    //Save record 1
    u8 data[] = { 1,2,3,4,5,6,7,8 };
    GS->recordStorage.SaveRecord(1, data, 8, nullptr,0);

    cherrySimInstance->SimCommitFlashOperations();

    SizedData dataB = GS->recordStorage.GetRecordData(1);

    cmp = memcmp(data, dataB.data, sizeof(data));
    if (dataB.length != sizeof(data) || cmp != 0) {
        FAIL() << "Stored record does not match saved record"; //LCOV_EXCL_LINE assertion
    }

    //Save record 2
    u8 data2[] = { 1,2,3,4,5,6,7,8 };
    GS->recordStorage.SaveRecord(2, data2, 8, nullptr, 0);

    cherrySimInstance->SimCommitFlashOperations();

    SizedData data2B = GS->recordStorage.GetRecordData(2);

    if (data2B.length != sizeof(data2)) {
        FAIL() << "Stored record length does not match saved record"; //LCOV_EXCL_LINE assertion
    }
    if (memcmp(data2, data2B.data, sizeof(data2)) != 0) {
        FAIL() << "Stored record does not match saved record"; //LCOV_EXCL_LINE assertion
    }

    //Update record 1
    u8 data3[] = { 1,2,3,4 };
    GS->recordStorage.SaveRecord(1, data, 4, nullptr, 0);

    cherrySimInstance->SimCommitFlashOperations();

    SizedData data3B = GS->recordStorage.GetRecordData(1);

    cmp = memcmp(data3, data3B.data, sizeof(data3));
    if (data3B.length != sizeof(data3) || cmp != 0) {
        FAIL() << "Updated record does not match"; //LCOV_EXCL_LINE assertion
    }
}

//TODO: Check what happens if we use a record size that is larger than a page

TEST_F(TestRecordStorage, TestRandomSingleRecordUpdates) {
    NodeIndexSetter setter(0);
    logt("WARNING", "---- TEST RANDOM SINGLE RECORD UPDATE ----");

    u32 cmp = 0;

    //Setup
    CheckedMemset(startPage, 0xff, numPages*FruityHal::GetCodePageSize());
    RepairPages();

    cherrySimInstance->SimCommitFlashOperations();

    //Save record 1
    u8 data[500];

    for (int i = 0; i < 2000; i++) 
    {
        CheckedMemset(data, i, 500);

        u16 length = 4+(Utility::GetRandomInteger() % 100) / 4 * 4;

        GS->recordStorage.SaveRecord(8, data, length, nullptr, 0);

        cherrySimInstance->SimCommitFlashOperations();

        //Check if we can read back the correct record
        SizedData dataB = GS->recordStorage.GetRecordData(8);

        cmp = memcmp(data, dataB.data, length);
        if (dataB.length != length || cmp != 0) {
            FAIL() << "Record data wrong after iteration " << i; //LCOV_EXCL_LINE assertion
        }
    }
}

TEST_F(TestRecordStorage, TestGetNonExistentRecord) {
    NodeIndexSetter setter(0);
    logt("WARNING", "---- TEST GET NON EXISTENT RECORD ----");

    //Setup
    CheckedMemset(startPage, 0xff, numPages*FruityHal::GetCodePageSize());
    RepairPages();

    SizedData dataB = GS->recordStorage.GetRecordData(7);

    if (dataB.length != 0) {
        FAIL() << "Non existent rerd must return a length of 0"; //LCOV_EXCL_LINE assertion
    }
}

TEST_F(TestRecordStorage, TestGetNonExistentRecordAfterStore) {
    NodeIndexSetter setter(0);
    logt("WARNING", "---- TEST GET NON EXISTENT RECORD AFTER STORE ----");

    //Setup
    CheckedMemset(startPage, 0xff, numPages*FruityHal::GetCodePageSize());
    RepairPages();

    u8 data[] = { 1,2,3,4 };
    GS->recordStorage.SaveRecord(5, data, 4, nullptr, 0);

    SizedData dataB = GS->recordStorage.GetRecordData(7);

    if (dataB.length != 0) {
        FAIL() << "Returned wrong record"; //LCOV_EXCL_LINE assertion
    }
}

TEST_F(TestRecordStorage, TestDeactivateRecord) {
    NodeIndexSetter setter(0);
    logt("WARNING", "---- CLEANUP ----");

    //Setup
    CheckedMemset(startPage, 0xff, numPages*FruityHal::GetCodePageSize());
    RepairPages();

    cherrySimInstance->SimCommitFlashOperations();

    logt("WARNING", "---- TEST DEACTIVATE RECORD ----");

    RecordStoragePage* usedPage = (RecordStoragePage*)(startPage + FruityHal::GetCodePageSize());

    //Save record
    u8 data[] = { 1,2,3,4 };
    GS->recordStorage.SaveRecord(5, data, sizeof(data), nullptr, 0);

    //Update record
    u8 data2[] = { 1,2,1,2 };
    GS->recordStorage.SaveRecord(5, data2, sizeof(data2), nullptr, 0);

    //Deactivate record
    GS->recordStorage.DeactivateRecord(5, nullptr, 0);

    //Committing done after all three steps are queued to test async
    cherrySimInstance->SimCommitFlashOperations();

    //Record must still be valid
    RecordStorageRecord* record = GS->recordStorage.GetRecord(5);
    if (!IsRecordValid(usedPage, record))
    {
        FAIL() << "Deactivated record corrupted"; //LCOV_EXCL_LINE assertion
    }

    //Record should not exist anymore
    SizedData dataB = GS->recordStorage.GetRecordData(5);
    if (dataB.length != 0) {
        FAIL() << "Record not deactivated properly"; //LCOV_EXCL_LINE assertion
    }

    //Defragment the page
    DefragmentPage(usedPage, false);

    cherrySimInstance->SimCommitFlashOperations();

    //There should be no record on that page anymore
    RecordStorageRecord* firstRecord = (RecordStorageRecord*)usedPage->data;
    if (firstRecord->recordId != 0xFFFF) {
        FAIL() << "Record should have been cleared by defragmentation"; //LCOV_EXCL_LINE assertion
    }

    //Update record again
    u8 data3[] = { 1,2,1,2,3,4,3,4 };
    GS->recordStorage.SaveRecord(5, data3, sizeof(data3), nullptr, 0);

    //Record should exist again
    SizedData dataC = GS->recordStorage.GetRecordData(5);
    if (dataC.length != 8) {
        FAIL() << "Record should exist again"; //LCOV_EXCL_LINE assertion
    }

}

TEST_F(TestRecordStorage, TestFlashBusy) {
    NodeIndexSetter setter(0);
    logt("WARNING", "---- CLEANUP ----");

    //Setup
    CheckedMemset(startPage, 0xff, numPages*FruityHal::GetCodePageSize());
    RepairPages();

    cherrySimInstance->SimCommitFlashOperations();

    logt("WARNING", "---- TEST SINGLE FLASH BUSY FAIL WILL WORK ----");

    RecordStoragePage* usedPage = (RecordStoragePage*)(startPage + FruityHal::GetCodePageSize());

    //Store will use 1 write operation but will retry a few times
    u8 data[] = { 1,2,1,2,3,4,3,4 };
    GS->recordStorage.SaveRecord(5, data, 8, this, 1);

    u8 failData[] = { 1,0,0,0,0,0,0,0,0,0 };
    cherrySimInstance->SimCommitSomeFlashOperations(failData, 10);

    RecordStorageRecord* record = GS->recordStorage.GetRecord(5);
    if (!IsRecordValid(usedPage, record))
    {
        FAIL() << "Single flash fail should have saved record"; //LCOV_EXCL_LINE assertion
    }

    logt("WARNING", "---- MULTI FLASH BUSY FAIL NOT WRITE RECORD ----");

    GS->recordStorage.SaveRecord(6, data, 8, this, 2);

    u8 failData2[] = { 1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0 };
    cherrySimInstance->SimCommitSomeFlashOperations(failData2, sizeof(failData2));

    RecordStorageRecord* record2 = GS->recordStorage.GetRecord(6);
    if (!IsRecordValid(usedPage, record2))
    {
        FAIL() << "Multiple flash fail shouldn't have written record"; //LCOV_EXCL_LINE assertion
    }

}

TEST_F(TestRecordStorage, TestAsyncQueuing) {
    NodeIndexSetter setter(0);
    logt("WARNING", "---- CLEANUP ----");

    //Setup
    CheckedMemset(startPage, 0xff, numPages*FruityHal::GetCodePageSize());
    RepairPages(); 

    cherrySimInstance->SimCommitFlashOperations();

    logt("WARNING", "---- TEST ASYNC QUEUING ----");

    RecordStorageResultCode err = RecordStorageResultCode::SUCCESS;

    //Deactivate non-existent record
    GS->recordStorage.DeactivateRecord(2, nullptr, 0);

    u8 i = 1;
    u8 data[] = { 0,1,2,3,4,5,6,7,8,9,10,11 };
    while (err == RecordStorageResultCode::SUCCESS) {
        data[0] = i;
        err = GS->recordStorage.SaveRecord(i, data, 12, nullptr, 0);

        if (i == 4) {
            GS->recordStorage.DeactivateRecord(3, nullptr, 0);
        }

        i++;
    }

    //Last call should have returned busy
    if (err != RecordStorageResultCode::BUSY) {
        FAIL() << "Flash not committed yet, should be busy"; //LCOV_EXCL_LINE assertion
    }

    //We should be able to queue 5 write ops
    if (i < 5) {
        FAIL() << "At least 5 write operations should be queueable"; //LCOV_EXCL_LINE assertion
    }

    logt("WARNING", "Queued %u items", i);

    //Committing done after all three steps are queued to test async
    cherrySimInstance->SimCommitFlashOperations();

    //Record 2 must not be deactivated
    if (GS->recordStorage.GetRecord(2) == nullptr) {
        FAIL() << "Record 2 should exist"; //LCOV_EXCL_LINE assertion
    }

    //Record 3 must be deactivated
    if (GS->recordStorage.GetRecord(3)->recordActive != 0) {
        FAIL() << "Record 3 should be inactive"; //LCOV_EXCL_LINE assertion
    }

}

//Must be below 256 because of test limit when storing length in byte
#define MULTI_RECORD_TEST_NUM_RECORD_IDS 20
//Must be below 256 because of test limit when storing length in byte
#define MULTI_RECORD_TEST_RECORD_MAX_SIZE 40
u8 testBuffer[MULTI_RECORD_TEST_RECORD_MAX_SIZE * MULTI_RECORD_TEST_NUM_RECORD_IDS];

TEST_F(TestRecordStorage, TestRandomMultiRecordUpdates) {
    NodeIndexSetter setter(0);
    logt("WARNING", "---- TEST RANDOM MULTI RECORD UPDATE ----");

    //Setup
    CheckedMemset(startPage, 0xff, numPages*FruityHal::GetCodePageSize());
    RepairPages();

    CheckedMemset(testBuffer, 0x00, sizeof(testBuffer));

    //Save record 1
    u8 data[500];

    for (int i = 0; i < 2000; i++)
    {
        u16 length = 4+(Utility::GetRandomInteger() % (MULTI_RECORD_TEST_RECORD_MAX_SIZE - SIZEOF_RECORD_STORAGE_RECORD_HEADER)) / 4 * 4;
        u16 randomRecordId = (Utility::GetRandomInteger() % MULTI_RECORD_TEST_NUM_RECORD_IDS) + 1;

        //Generate record data
        CheckedMemset(data, randomRecordId, 500);
        data[1] = (u8)Utility::GetRandomInteger(); //Store random integer to identify this record
        data[2] = (u8)length; //Store length in data to compare it later

        //Store the test record ourself in a big array
        CheckedMemcpy(testBuffer + MULTI_RECORD_TEST_RECORD_MAX_SIZE * (randomRecordId-1), data, length);

        //Store it using the tested class
        GS->recordStorage.SaveRecord(randomRecordId, data, length, nullptr, 0);


        cherrySimInstance->SimCommitFlashOperations();

        //Check if we can read all records back in their latest version
        for (int i = 0; i < MULTI_RECORD_TEST_NUM_RECORD_IDS; i++) {
            u8* bufferedData = &(testBuffer[MULTI_RECORD_TEST_RECORD_MAX_SIZE * i]);
            u8 storedRecordId = bufferedData[0];
            u8 storedRecordLength = bufferedData[2];
            //If we have a non-zero value, this record had been stored in ram previously, try to read it back and compare
            if (storedRecordId != 0) {
                SizedData dataB = GS->recordStorage.GetRecordData(storedRecordId);

                //Record not found fail
                if (dataB.length.IsZero()) {
                    FAIL() << "Record not found in iteration " << i; //LCOV_EXCL_LINE assertion
                }
                //Record wrong length fail
                if (dataB.length != storedRecordLength) {
                    FAIL() << "Record had wrong length in iteration " << i; //LCOV_EXCL_LINE assertion
                }
                //Record data not matching fail
                u32 result = memcmp(bufferedData, dataB.data, dataB.length.GetRaw());
                if (result != 0) {
                    FAIL() << "Record data corrupt in iteration " << i; //LCOV_EXCL_LINE assertion
                }
            }
            
        }
    }
}
