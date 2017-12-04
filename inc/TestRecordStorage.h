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
/*
 * This class will test the battery
 */


#pragma once



#ifdef SIM_ENABLED

#include <types.h>
#include <RecordStorage.h>

class TestRecordStorage : public RecordStorageEventListener
{
	private:
		u8* startPage;
		u16 numPages;

	public:
		TestRecordStorage();

		void Start();
		void TestCleanup();
		void TestSave();
		void TestRandomSingleRecordUpdates();
		void TestGetNonExistentRecord();
		void TestGetNonExistentRecordAfterStore();
		void TestDeactivateRecord();
		void TestFlashBusy();
		void TestAsyncQueuing();
		void TestRandomMultiRecordUpdates();

		void RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength);
};

#endif
