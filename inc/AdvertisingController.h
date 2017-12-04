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
 * The Advertising Controller is responsible for wrapping all advertising
 * functionality and the necessary softdevice calls in one class.
 */

#pragma once

#include <types.h>
#include <GlobalState.h>
#include <adv_packets.h>

extern "C"{
#include <ble.h>
#include <ble_gap.h>
}

enum AdvJobTypes {
	ADV_JOB_TYPE_INVALID,
	ADV_JOB_TYPE_SCHEDULED, //Automatically scheduled with other jobs
	ADV_JOB_TYPE_IMMEDIATE //Will be executed immediately until done

};

typedef struct AdvJob {
	AdvJobTypes type;
	//For Scheduler
	u8 slots; //Number of slots this advertising message will get (1-10), 0 = Invalid
	u8 delay; //Number of slots that this message will be delayed
	u16 advertisingInterval; //In units of 0.625ms

	//Internal Scheduling
	u8 currentSlots;
	u8 currentDelay;

	//Advertising Data
	i8 advertisingType; //BLE_GAP_ADV_TYPES
	u8 advData[31];
	u8 advDataLength;
	u8 scanData[31];
	u8 scanDataLength;

} AdvJob;


#define ADVERTISING_CONTROLLER_MAX_NUM_JOBS 3

class AdvertisingController
{
private:
	AdvertisingController();

	u32 sumSlots;
	u16 currentAdvertisingInterval;

	//The address that should be used for advertising, the Least Significant Byte
	//May be changed by the advertiser to account for different advertising services
	fh_ble_gap_addr_t baseGapAddress;

public:

	AdvJob jobs[ADVERTISING_CONTROLLER_MAX_NUM_JOBS];

	enum AdvertisingState {
		ADVERTISING_STATE_DISABLED,
		ADVERTISING_STATE_ENABLED
	};

	enum AdvertisingStateAction {
		ADVERTISING_STATE_ACTION_OK,
		ADVERTISING_STATE_ACTION_SHOULD_DISABLE,
		ADVERTISING_STATE_ACTION_SHOULD_RESTART,
	};

	AdvertisingState advertisingState;
	AdvertisingStateAction advertisingStateAction;

	fh_ble_gap_adv_params_t currentAdvertisingParams;
	u8 currentNumJobs;


	AdvJob* jobToSet;

	static AdvertisingController* getInstance(){
		if(!GS->advertisingController){
			GS->advertisingController = new AdvertisingController();
		}
		return GS->advertisingController;
	}


	void Initialize();

	//Job Scheduling
	void InitJobScheduling();
	AdvJob* AddJob(AdvJob* job);
	void RefreshJob(AdvJob* jobHandle);
	void RemoveJob(AdvJob* jobHandle);
	AdvJob* DetermineCurrentAdvertisingJob();

	//Change Advertising with Softdevice
	void SetAdvertisingData();
	void SetAdvertisingState(AdvJob* job);


	void TimerHandler(u16 passedTimeDs);

	void Test();

	bool AdvertiseEventHandler(ble_evt_t* bleEvent);


};

