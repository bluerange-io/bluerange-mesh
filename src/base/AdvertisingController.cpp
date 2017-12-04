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

#include <AdvertisingController.h>
#include <ConnectionManager.h>
#include <Logger.h>
#include <Config.h>
#include <Utility.h>

extern "C"{
#include <nrf_error.h>
#include <app_error.h>
}




/*
TODO:
- should allow to set intervals either HIGH/LOW or custom
- should have callback after sending an advertising packet??

 */



AdvertisingController::AdvertisingController()
{
	advertisingState = AdvertisingState::ADVERTISING_STATE_DISABLED; //The current state of advertising
	memset(&currentAdvertisingParams, 0x00, sizeof(currentAdvertisingParams));

	memset(jobs, 0x00, sizeof(jobs));
	sumSlots = 0;
	currentNumJobs = 0;
	currentAdvertisingInterval = UINT16_MAX;
	jobToSet = NULL;
}

void  AdvertisingController::Test()
{
	Logger::getInstance()->enableTag("ADV");

	AdvJob job = {
		AdvJobTypes::ADV_JOB_TYPE_SCHEDULED,
		5, //Slots
		0, //Delay
		MSEC_TO_UNITS(100, UNIT_0_625_MS), //AdvInterval
		0, //CurrentSlots
		0, //CurrentDelay
		BLE_GAP_ADV_TYPE_ADV_IND, //Advertising Mode
		{ 0 }, //AdvData
		0, //AdvDataLength
		{ 0 }, //ScanData
		0 //ScanDataLength
	};
	AddJob(&job);

	AdvJob job2 = {
		AdvJobTypes::ADV_JOB_TYPE_SCHEDULED,
		2, //Slots
		0, //Delay
		MSEC_TO_UNITS(100, UNIT_0_625_MS), //AdvInterval
		0, //CurrentSlots
		0, //CurrentDelay
		BLE_GAP_ADV_TYPE_ADV_IND, //Advertising Mode
		{ 0 }, //AdvData
		0, //AdvDataLength
		{ 0 }, //ScanData
		0 //ScanDataLength
	};
	AdvJob* handle = AddJob(&job2);

	bool job2Active = true;

	while (true) {
		if (!job2Active && Utility::GetRandomInteger() % 1000 > 700) {
			handle = AdvertisingController::getInstance()->AddJob(&job2);
			job2Active = true;
		}
		else if (job2Active && Utility::GetRandomInteger() % 1000 > 700) {
			AdvertisingController::getInstance()->RemoveJob(handle);
			job2Active = false;
		}
		if (Utility::GetRandomInteger() % 1000 > 500) AdvertisingController::getInstance()->TimerHandler(2);
	}

}

void AdvertisingController::Initialize()
{
	memset(&currentAdvertisingParams, 0x00, sizeof(currentAdvertisingParams));
	currentAdvertisingParams.type = BLE_GAP_ADV_TYPE_ADV_IND; //Connectable
	currentAdvertisingParams.interval = MSEC_TO_UNITS(100, UNIT_0_625_MS); // Advertising interval between 0x0020 and 0x4000 in 0.625 ms units (20ms to 10.24s), see @ref BLE_GAP_ADV_INTERVALS
	currentAdvertisingParams.timeout = 0;
	currentAdvertisingParams.channel_mask.ch_37_off = Config->advertiseOnChannel37 ? 0 : 1;
	currentAdvertisingParams.channel_mask.ch_38_off = Config->advertiseOnChannel38 ? 0 : 1;
	currentAdvertisingParams.channel_mask.ch_39_off = Config->advertiseOnChannel39 ? 0 : 1;

	//Read used GAP address, will always succeed
	FruityHal::BleGapAddressGet(&baseGapAddress);
}

/**
 * The Advertising Job Scheduler accepts a number of jobs with slots and delay
 * Each job is give its number of slots uniformly distributed over a cycle where all jobs
 * are processed. After all jobs have been processed, a new cycle is started. A delay can
 * span multiple cycles and enables advertising e.g. each hour for 10 slots.
 * Afterwards, the delay is reloaded
 */

AdvJob* AdvertisingController::AddJob(AdvJob* job){
	if (job->type == AdvJobTypes::ADV_JOB_TYPE_INVALID) return NULL;

	for(int i=0; i<ADVERTISING_CONTROLLER_MAX_NUM_JOBS; i++){
		if(jobs[i].type == AdvJobTypes::ADV_JOB_TYPE_INVALID){
			currentNumJobs++;
			jobs[i] = *job;

			if(jobs[i].type == AdvJobTypes::ADV_JOB_TYPE_IMMEDIATE){
				jobs[i].currentSlots = jobs[i].slots;
			}
			logt("ADV", "Adding job %u", i);
			RefreshJob(&(jobs[i]));
			return &(jobs[i]);
		}
	}
	return NULL;
}

//Must be called after a jobHandle was used to modify a job currently in use
void AdvertisingController::RefreshJob(AdvJob* jobHandle){
	logt("ADV", "Refreshing job");
	//Update advertising interval if necessary, reschedule current cycle
	if(jobHandle->advertisingInterval < currentAdvertisingInterval){
		currentAdvertisingInterval = jobHandle->advertisingInterval;
		advertisingStateAction = AdvertisingStateAction::ADVERTISING_STATE_ACTION_SHOULD_RESTART;
	}
}

void AdvertisingController::RemoveJob(AdvJob* jobHandle)
{
	for (int i = 0; i < ADVERTISING_CONTROLLER_MAX_NUM_JOBS; i++) {
		if (&(jobs[i]) == jobHandle && jobs[i].type != AdvJobTypes::ADV_JOB_TYPE_INVALID) {
			logt("ADV", "Removing job %u", i);
			currentNumJobs--;

			//Remove the remaining slots from our currently active scheduling
			if (jobHandle->type == AdvJobTypes::ADV_JOB_TYPE_SCHEDULED) {
				sumSlots -= jobHandle->currentSlots;
			}

			jobHandle->type = AdvJobTypes::ADV_JOB_TYPE_INVALID;

			//Update Advertising interval
			u16 newInterval = UINT16_MAX;
			for (int i = 0; i<ADVERTISING_CONTROLLER_MAX_NUM_JOBS; i++) {
				if (jobs[i].type == AdvJobTypes::ADV_JOB_TYPE_SCHEDULED && jobs[i].advertisingInterval < newInterval) {
					newInterval = jobs[i].advertisingInterval;
				}
			}
			currentAdvertisingInterval = newInterval;

			if (currentNumJobs == 0) {
				advertisingStateAction = AdvertisingStateAction::ADVERTISING_STATE_ACTION_SHOULD_DISABLE;
			}
			else {
				advertisingStateAction = AdvertisingStateAction::ADVERTISING_STATE_ACTION_SHOULD_RESTART;
			}
		}
	}	
}

void AdvertisingController::TimerHandler(u16 passedTimeDs)
{
	//Find the job that should advertise
	AdvJob* job = DetermineCurrentAdvertisingJob();

	logt("ADV", "Timer advState %u, numJobs %u, sumSlots %u", advertisingState, currentNumJobs, sumSlots);

	//Set the selected advertising job to the SoftDevice
	SetAdvertisingData();

	jobToSet = job;

	SetAdvertisingState(job);
}

//Must be called once upfront before using Advertising Job Scheduler
//Is automatically called by the scheduler as soon as a cycle has finished
void AdvertisingController::InitJobScheduling(){
	logt("ADV", "Resetting Scheduling" SEP);
	sumSlots = 0;
	for(int i=0; i<ADVERTISING_CONTROLLER_MAX_NUM_JOBS; i++){
		if (jobs[i].type == AdvJobTypes::ADV_JOB_TYPE_SCHEDULED) {
			//Refill slots, but only if there are none left (happens if a delay is set)
			if (jobs[i].currentSlots == 0) {
				jobs[i].currentSlots = jobs[i].slots;
				jobs[i].currentDelay = jobs[i].delay;
			}
			//Count the max number of slots (Only the ones that aren't delayed)
			if (jobs[i].currentDelay == 0) {
				sumSlots += jobs[i].currentSlots;
			}
		}
	}
	logt("ADV", "sumSlots %u" SEP, sumSlots);
}

AdvJob* AdvertisingController::DetermineCurrentAdvertisingJob()
{
	logt("ADV", "###### DETERMINE ADV JOB");
	//Some logging
	for (int i = 0; i < ADVERTISING_CONTROLLER_MAX_NUM_JOBS; i++) {
		if (jobs[i].type != AdvJobTypes::ADV_JOB_TYPE_INVALID) {
			logt("ADV", "job %u: slots: %u, currentSlots: %u", i, jobs[i].slots, jobs[i].currentSlots);
		}
	}


	if(currentNumJobs == 0){
		return NULL;
	}

	//Check if we reached the end of this scheduling cycle
	if (sumSlots == 0) {
		InitJobScheduling();
	}

	AdvJob* selectedJob = NULL;

	//Generate a random number between 0 and sumSlots
	//TODO: Don't use hardware random
	i32 rnd = sumSlots == 0 ? 0 : Utility::GetRandomInteger() % sumSlots;

	logt("ADV", "Rnd: %u, sumCurrentSlots %u", rnd, sumSlots);

	//Go through list to pick the right job by random
	//This loop must run to the end to
	for(int i=0; i<ADVERTISING_CONTROLLER_MAX_NUM_JOBS; i++){
		//If we have an immediate job, select it
		if(jobs[i].type == AdvJobTypes::ADV_JOB_TYPE_IMMEDIATE){
			//An immediate job does not count against sumSlots and delay is not considered
			jobs[i].currentSlots--;
			//Clear job if done
			if(jobs[i].currentSlots == 0){
				RemoveJob(&(jobs[i]));
			}
			//Select job
			selectedJob = &(jobs[i]);
		}
		//Job is checked only if it exists and if it still has available slots
		if(jobs[i].type == AdvJobTypes::ADV_JOB_TYPE_SCHEDULED && jobs[i].currentSlots != 0){
			if(jobs[i].currentDelay > 0){
				jobs[i].currentDelay--; //Decrement delay for all jobs
				if(jobs[i].currentDelay == 0){
					//If a delay of 0 was reached, we must account for this job in the current scheduling cycle
					sumSlots += jobs[i].currentSlots;
				} else {
					//Job still delayed
					continue;
				}
			}
			//This job should advertise its message
			if(selectedJob == NULL && rnd >= 0 && rnd < jobs[i].slots){
				jobs[i].currentSlots--;
				sumSlots--;

				selectedJob = &(jobs[i]);

				logt("ADV", "Advertising job %u selected", i);
			}
			//Jump to next job, this one wasn't selected
			else
			{
			}
			//Decrement rnd, as soon as it is below 0, it can not trigger a job anymore
			rnd -= jobs[i].currentSlots;
		}
	}

	return selectedJob;
}

//Will set the advertising data in the softdevice
void AdvertisingController::SetAdvertisingData()
{
	u32 err;

	if(jobToSet == NULL) return;

	//Save a reference as this function is called asynchronously
	AdvJob* job = jobToSet;
	jobToSet = NULL;

	err = sd_ble_gap_adv_data_set(
			job->advData,
			job->advDataLength,
			job->scanData,
			job->scanDataLength
		);

	if(err != NRF_SUCCESS){
		char buffer[100];
		Logger::getInstance()->convertBufferToHexString(job->advData, job->advDataLength, buffer, 100);

		logt("ERROR", "Setting Adv data err %u: %s (%u)", err, buffer, job->advDataLength);
	}
}

void AdvertisingController::SetAdvertisingState(AdvJob* job)
{
	u32 err;

	//Nothing to do
	if(advertisingStateAction == AdvertisingStateAction::ADVERTISING_STATE_ACTION_OK){
		return;
	}

	//Nothing to do
	if(
		advertisingStateAction == AdvertisingStateAction::ADVERTISING_STATE_ACTION_SHOULD_DISABLE
		&& advertisingState == AdvertisingState::ADVERTISING_STATE_DISABLED
	){
		advertisingStateAction = AdvertisingStateAction::ADVERTISING_STATE_ACTION_OK;
		return;
	}

	//Determine new advertising parameters
	//TODO: Check if our job needs special settings
	BaseConnections connections = GS->cm->GetBaseConnections(ConnectionDirection::CONNECTION_DIRECTION_IN);
	if(connections.count < Config->totalInConnections){
		currentAdvertisingParams.type = BLE_GAP_ADV_TYPE_ADV_IND; //Connectable
	} else {
		currentAdvertisingParams.type = BLE_GAP_ADV_TYPE_ADV_NONCONN_IND; // Non-Connectable
	}

	currentAdvertisingParams.interval = currentAdvertisingInterval;

	logt("ADV", "iv %u", currentAdvertisingParams.interval);

	//FIXME: What do we do if no job was given?
	if(job == NULL){
		logt("ADV", "Job was null");
		return;
	}

	//Try to stop the advertiser if it should be stopped or restartet
	if(advertisingStateAction == AdvertisingStateAction::ADVERTISING_STATE_ACTION_SHOULD_DISABLE)
	{
		err = sd_ble_gap_adv_stop();

		if(err == NRF_SUCCESS){
			advertisingStateAction = AdvertisingStateAction::ADVERTISING_STATE_ACTION_OK;
			advertisingState = AdvertisingState::ADVERTISING_STATE_DISABLED;
			logt("ADV", "Adv Stopped %u", err);
		}
	}

	//Start or restart with different advertising parameters
	else if(advertisingStateAction == AdvertisingStateAction::ADVERTISING_STATE_ACTION_SHOULD_RESTART)
	{
		//If advertising is still enabled, we have to stop it first
		if(advertisingState == AdvertisingState::ADVERTISING_STATE_ENABLED){
			err = sd_ble_gap_adv_stop();

			if(err == NRF_SUCCESS){
				advertisingState = AdvertisingState::ADVERTISING_STATE_DISABLED;
				logt("ADV", "Adv Stopped %u", err);
			}
		}

		//We can only restart advertising if stopping worked
		if(advertisingState == AdvertisingState::ADVERTISING_STATE_DISABLED){
			err = FruityHal::BleGapAdvStart(&currentAdvertisingParams);

			if(err == NRF_SUCCESS){
				logt("ADV", "Advertising enabled");
				advertisingStateAction = AdvertisingStateAction::ADVERTISING_STATE_ACTION_OK;
				advertisingState = AdvertisingState::ADVERTISING_STATE_ENABLED;
			} else {
				logt("ERROR", "Error restarting advertisement %u", err);
			}
		}
	}
}

bool AdvertisingController::AdvertiseEventHandler(ble_evt_t* bleEvent)
{
	u32 err = 0;
	switch (bleEvent->header.evt_id)
	{
		case BLE_GAP_EVT_CONNECTED: {
			if(bleEvent->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_PERIPH){
				//If a peripheral connection got connected, we can only advertise non-connectable, reschedule
				//Also, we must restart because the advertising was stopped automatically
				if(advertisingState == AdvertisingState::ADVERTISING_STATE_ENABLED){
					advertisingState = AdvertisingState::ADVERTISING_STATE_DISABLED;
					advertisingStateAction = AdvertisingStateAction::ADVERTISING_STATE_ACTION_SHOULD_RESTART;
				}
			}
		}
		break;

		case BLE_GAP_EVT_DISCONNECTED: {
			//TODO: Should check if this was a peripheral connection
			//If a peripheral connection is lost, we can restart advertising in connectable mode
			if(advertisingState == AdvertisingState::ADVERTISING_STATE_ENABLED){
				advertisingStateAction = AdvertisingStateAction::ADVERTISING_STATE_ACTION_SHOULD_RESTART;
			}
		}
		break;
	}
	return false;
}
/* EOF */
