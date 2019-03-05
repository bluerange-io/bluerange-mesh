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

/*
 * The Advertising Controller is responsible for wrapping all advertising
 * functionality and the necessary softdevice calls in one class.
 */

#pragma once

#include <types.h>
#include <GlobalState.h>
#include <FruityHal.h>
#include "SimpleArray.h"

extern "C"{
#include <ble.h>
#include <ble_gap.h>
}

enum class AdvJobTypes : u8{
	INVALID,
	SCHEDULED, //Automatically scheduled with other jobs
	IMMEDIATE  //Will be executed immediately until done

};

typedef struct AdvJob {
	AdvJobTypes type;
	//For Scheduler
	u8 slots; //Number of slots this advertising message will get (1-10), 0 = Invalid
	u8 delay; //Number of slots that this message will be delayed
	u16 advertisingInterval; //In units of 0.625ms
	u8 advertisingChannelMask;

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


#define ADVERTISING_CONTROLLER_MAX_NUM_JOBS 4

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

	SimpleArray<AdvJob, ADVERTISING_CONTROLLER_MAX_NUM_JOBS> jobs;

	enum class AdvertisingState : u8{
		DISABLED,
		ENABLED
	};

	enum class AdvertisingStateAction : u8{
		OK,
		DISABLE,
		RESTART,
	};

	AdvertisingState advertisingState;
	AdvertisingStateAction advertisingStateAction;

	fh_ble_gap_adv_params_t currentAdvertisingParams;
	u8 currentNumJobs;


	AdvJob* currentActiveJob;
	AdvJob* jobToSet;

	static AdvertisingController& getInstance(){
		if(!GS->advertisingController){
			GS->advertisingController = new AdvertisingController();
		}
		return *(GS->advertisingController);
	}


	void Initialize();

	//Job Scheduling
	void InitJobScheduling();
	AdvJob* AddJob(const AdvJob& job);
	void RefreshJob(const AdvJob* jobHandle);
	void RemoveJob(AdvJob* jobHandle);
	AdvJob* DetermineCurrentAdvertisingJob();
	void DetermineAndSetAdvertisingJob();

	void DetermineCurrentAdvertisingInterval();

	//Change Advertising with Softdevice
	void SetAdvertisingData(AdvJob* job);
	void SetAdvertisingState(AdvJob* job);

	u16 GetLowestAdvertisingInterval();
	void RestartAdvertising();

	void DeleteAllJobs();


	void TimerEventHandler(u16 passedTimeDs);

	bool AdvertiseEventHandler(const ble_evt_t &bleEvent);


};

