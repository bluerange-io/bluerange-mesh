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

#include <AdvertisingController.h>
#include <Logger.h>
#include <Config.h>
#include <Utility.h>
#include <GlobalState.h>

/*
TODO:
- should allow to set intervals either HIGH/LOW or custom
- should have callback after sending an advertising packet??

 */



AdvertisingController::AdvertisingController()
{
	advertisingState = AdvertisingState::DISABLED; //The current state of advertising
	advertisingStateAction = AdvertisingStateAction::OK;
	CheckedMemset(&currentAdvertisingParams, 0x00, sizeof(currentAdvertisingParams));

	jobs.zeroData();
	sumSlots = 0;
	currentNumJobs = 0;
	currentAdvertisingInterval = UINT16_MAX;
	jobToSet = nullptr;
	currentActiveJob = nullptr;
#if SDK == 15
	advData.zeroData();
	currentSlotUsed = 0;
	handle = 0xFF; //BLE_GAP_ADV_SET_HANDLE_NOT_SET
#endif
}

AdvertisingController & AdvertisingController::getInstance()
{
	return GS->advertisingController;
}

void AdvertisingController::Initialize()
{
	CheckedMemset(&currentAdvertisingParams, 0x00, sizeof(currentAdvertisingParams));
	currentAdvertisingParams.type = GapAdvType::ADV_IND;
	currentAdvertisingParams.interval = MSEC_TO_UNITS(100, UNIT_0_625_MS); // Advertising interval between 0x0020 and 0x4000 in 0.625 ms units (20ms to 10.24s), see @ref BLE_GAP_ADV_INTERVALS
	currentAdvertisingParams.timeout = 0;
	currentAdvertisingParams.channel_mask.ch_37_off = Conf::advertiseOnChannel37 ? 0 : 1;
	currentAdvertisingParams.channel_mask.ch_38_off = Conf::advertiseOnChannel38 ? 0 : 1;
	currentAdvertisingParams.channel_mask.ch_39_off = Conf::advertiseOnChannel39 ? 0 : 1;

	//Read used GAP address, will always succeed
	FruityHal::BleGapAddressGet(&baseGapAddress);
}

void AdvertisingController::Deactivate()
{
	isActive = false;
#if (SDK == 15)
	FruityHal::BleGapAdvStop(handle);
#else
	FruityHal::BleGapAdvStop();
#endif
}

/**
 * The Advertising Job Scheduler accepts a number of jobs with slots and delay
 * Each job is give its number of slots uniformly distributed over a cycle where all jobs
 * are processed. After all jobs have been processed, a new cycle is started. A delay can
 * span multiple cycles and enables advertising e.g. each hour for 10 slots.
 * Afterwards, the delay is reloaded
 */

AdvJob* AdvertisingController::AddJob(const AdvJob& job){
	if (job.type == AdvJobTypes::INVALID) return nullptr;

	for(int i=0; i< jobs.length; i++){
		if(jobs[i].type == AdvJobTypes::INVALID){
			currentNumJobs++;
			jobs[i] = job;

			if(jobs[i].type == AdvJobTypes::IMMEDIATE){
				jobs[i].currentSlots = jobs[i].slots;
			}
			logt("ADV", "Adding job %u", i);
			RefreshJob(&(jobs[i]));
			return &(jobs[i]);
		}
	}

	advertisingStateAction = AdvertisingStateAction::RESTART;

	return nullptr;
}

//Must be called after a jobHandle was used to modify a job currently in use
void AdvertisingController::RefreshJob(const AdvJob* jobHandle){
	if(jobHandle == nullptr) return;

	logt("ADV", "Refreshing job");
	//Reset current active job if it was the same one so that it gets sent to the softdevice
	if(jobHandle == currentActiveJob) currentActiveJob = nullptr;

	//Update advertising interval if necessary, reschedule current cycle
	if(
		GetLowestAdvertisingInterval() != currentAdvertisingInterval
		|| jobHandle->advertisingChannelMask != *(u8*)&currentAdvertisingParams.channel_mask
		)
	{
		currentAdvertisingInterval = GetLowestAdvertisingInterval();
		advertisingStateAction = AdvertisingStateAction::RESTART;
	}
}

u16 AdvertisingController::GetLowestAdvertisingInterval()
{
	u16 newInterval = UINT16_MAX;
	for (int i = 0; i< jobs.length; i++) {
		if (jobs[i].type == AdvJobTypes::SCHEDULED && jobs[i].advertisingInterval < newInterval) {
			newInterval = jobs[i].advertisingInterval;
		}
	}

	return newInterval;
}

void AdvertisingController::RemoveJob(AdvJob* jobHandle)
{
	if(jobHandle == nullptr) return;

	for (int i = 0; i < jobs.length; i++) {
		if (&(jobs[i]) == jobHandle && jobs[i].type != AdvJobTypes::INVALID) {
			logt("ADV", "Removing job %u", i);
			currentNumJobs--;

			//Remove the remaining slots from our currently active scheduling
			if (jobHandle->type == AdvJobTypes::SCHEDULED) {
				sumSlots -= jobHandle->currentSlots;
			}

			jobHandle->type = AdvJobTypes::INVALID;

			//Update Advertising interval
			currentAdvertisingInterval = GetLowestAdvertisingInterval();

			if (currentNumJobs == 0) {
				advertisingStateAction = AdvertisingStateAction::DISABLE;
			}
			else {
				advertisingStateAction = AdvertisingStateAction::RESTART;
			}
		}
	}	

	//In case the job is currently running, stop it
	if(currentActiveJob == jobHandle){
		DetermineAndSetAdvertisingJob();
	}
}

void AdvertisingController::TimerEventHandler(u16 passedTimeDs)
{
	if(SHOULD_IV_TRIGGER(GS->appTimerDs, passedTimeDs, 4)){
		DetermineAndSetAdvertisingJob();
	}
}

void AdvertisingController::DetermineAndSetAdvertisingJob()
{
	if (!isActive) return;

	//Find the job that should advertise
	jobToSet = DetermineCurrentAdvertisingJob();

	//Set the selected advertising job to the SoftDevice
	if(jobToSet != currentActiveJob || advertisingStateAction != AdvertisingStateAction::OK){
		SetAdvertisingData(jobToSet);
	}

	//Must be called every time as this will restart advertising if necessary
	SetAdvertisingState(jobToSet);
}

//Must be called once upfront before using Advertising Job Scheduler
//Is automatically called by the scheduler as soon as a cycle has finished
void AdvertisingController::InitJobScheduling(){
	logt("ADVS", "Resetting Scheduling" SEP);
	sumSlots = 0;
	for(int i=0; i< jobs.length; i++){
		if (jobs[i].type == AdvJobTypes::SCHEDULED) {
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
	logt("ADVS", "sumSlots %u" SEP, sumSlots);
}

AdvJob* AdvertisingController::DetermineCurrentAdvertisingJob()
{
	logt("ADVS", "###### DETERMINE ADV JOB");
	//Some logging
	for (int i = 0; i < jobs.length; i++) {
		if (jobs[i].type != AdvJobTypes::INVALID) {
			logt("ADVS", "job %u: slots: %u, currentSlots: %u", i, jobs[i].slots, jobs[i].currentSlots);
		}
	}


	if(currentNumJobs == 0){
		return nullptr;
	}

	//Check if we reached the end of this scheduling cycle
	if (sumSlots == 0) {
		InitJobScheduling();
	}

	AdvJob* selectedJob = nullptr;

	//Generate a random number between 0 and sumSlots
	//TODO: Don't use hardware random
	i32 rnd = sumSlots == 0 ? 0 : Utility::GetRandomInteger() % sumSlots;

	logt("ADVS", "Rnd: %d, sumCurrentSlots %u", rnd, sumSlots);

	//Go through list to pick the right job by random
	//This loop must run to the end to
	for(int i=0; i< jobs.length; i++){
		//If we have an immediate job, select it
		if(jobs[i].type == AdvJobTypes::IMMEDIATE){
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
		if(jobs[i].type == AdvJobTypes::SCHEDULED && jobs[i].currentSlots != 0){
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
			if(selectedJob == nullptr && rnd >= 0 && rnd < jobs[i].currentSlots){
				jobs[i].currentSlots--;
				sumSlots--;

				selectedJob = &(jobs[i]);

				logt("ADVS", "Advertising job %u selected", i);
			}
			//Jump to next job, this one wasn't selected
			else
			{
			}
			//Decrement rnd, as soon as it is below 0, it can not trigger a job anymore
			rnd -= jobs[i].currentSlots;
		}
	}

	if(selectedJob == nullptr){
		logt("ADVS", "No advertising job selected");
	}

	logt("ADVS", "Timer advState %u, numJobs %u, sumSlots %u", (u32)advertisingState, currentNumJobs, sumSlots);

	return selectedJob;
}

//Will set the advertising data in the softdevice
void AdvertisingController::SetAdvertisingData(AdvJob* job)
{
	u32 err;

	if(job == nullptr) return;

	//Stop advertising before changing the data if the channel mask changed
	//because we cannot change advertising data while advertising
	if(job->advertisingChannelMask != *(u8*)&currentAdvertisingParams.channel_mask){
#if (SDK == 15)
		err = FruityHal::BleGapAdvStop(handle);
#else
		err = FruityHal::BleGapAdvStop();
#endif
		if(err == FruityHal::SUCCESS){
			advertisingState = AdvertisingState::DISABLED;
		}
	}
#if (SDK == 15)
	advData[currentSlotUsed].inUse = false;
	currentSlotUsed++;
	currentSlotUsed %= 2;
	advData[currentSlotUsed].inUse = true;
	memcpy(advData[currentSlotUsed].advData, job->advData, job->advDataLength);
	advData[currentSlotUsed].advDataLength = job->advDataLength;
	memcpy(advData[currentSlotUsed].scanData, job->scanData, job->scanDataLength);
	advData[currentSlotUsed].scanDataLength = job->scanDataLength;
	
	fh_ble_gap_adv_params_t * p_advParams = nullptr;
	if (advertisingState == AdvertisingState::DISABLED){
		p_advParams = &currentAdvertisingParams;
	}
	err = FruityHal::BleGapAdvDataSet(
			&handle,
			p_advParams,
			advData[currentSlotUsed].advData,
			advData[currentSlotUsed].advDataLength,
			advData[currentSlotUsed].scanData,
			advData[currentSlotUsed].scanDataLength
		);
#else
	err = FruityHal::BleGapAdvDataSet(
			job->advData,
			job->advDataLength,
			job->scanData,
			job->scanDataLength
		);
#endif
	logt("ADV", "Adv Data Set %u", err);

	if(err != FruityHal::SUCCESS){
		char buffer[100];
		Logger::convertBufferToHexString(job->advData, job->advDataLength, buffer, sizeof(buffer));

		logt("ERROR", "Setting Adv data err %u: %s (%u)", err, buffer, job->advDataLength);
	} else {
		currentActiveJob = job;
	}
}

void AdvertisingController::SetAdvertisingState(AdvJob* job)
{
	u32 err;

	//Stop advertising if no job was given
	if(job == nullptr && advertisingState != AdvertisingState::DISABLED){
		advertisingStateAction = AdvertisingStateAction::DISABLE;
	}
	//Restart advertising if job is given again
	if(job != nullptr && advertisingState == AdvertisingState::DISABLED){
		advertisingStateAction = AdvertisingStateAction::RESTART;
	}

	//Nothing to do
	if(advertisingStateAction == AdvertisingStateAction::OK){
		return;
	}

	//Nothing to do
	if(
		advertisingStateAction == AdvertisingStateAction::DISABLE
		&& advertisingState == AdvertisingState::DISABLED
	){
		advertisingStateAction = AdvertisingStateAction::OK;
		return;
	}

	//Determine new advertising parameters
	currentAdvertisingParams.interval = currentAdvertisingInterval;
	if (job != nullptr) {
		*((u8*)&currentAdvertisingParams.channel_mask) = job->advertisingChannelMask;
	}

	//TODO: Check if our job needs special settings

	BaseConnections connections = GS->cm.GetBaseConnections(ConnectionDirection::DIRECTION_IN);
	u8 connectedConnections = 0;
	for(int i=0; i<connections.count; i++){
		BaseConnection* conn = GS->cm.allConnections[connections.connectionIndizes[i]];
		if (conn != nullptr) {
			if (
				conn->connectionState == ConnectionState::CONNECTED
				|| conn->connectionState == ConnectionState::HANDSHAKING
				|| conn->connectionState == ConnectionState::HANDSHAKE_DONE
				) {
				connectedConnections++;
			}
		}
	}

	if(connectedConnections < Conf::totalInConnections){
		currentAdvertisingParams.type = GapAdvType::ADV_IND;
	} else {
		currentAdvertisingParams.type = GapAdvType::ADV_NONCONN_IND; // Non-Connectable
		//Non connectable advertising must not be faster than 100ms
		if(currentAdvertisingParams.interval < MSEC_TO_UNITS(100, UNIT_0_625_MS)){
			currentAdvertisingParams.interval = MSEC_TO_UNITS(100, UNIT_0_625_MS);
		}
	}

	logt("ADV", "iv %u", currentAdvertisingParams.interval);

	//Try to stop the advertiser if it should be stopped or restartet
	//TODO: What if stop always returns an error because it was already stopped?
	if(advertisingStateAction == AdvertisingStateAction::DISABLE)
	{
#if (SDK == 15)
		err = FruityHal::BleGapAdvStop(handle);
#else
		err = FruityHal::BleGapAdvStop();
#endif

		if(err == FruityHal::SUCCESS){
			advertisingStateAction = AdvertisingStateAction::OK;
			advertisingState = AdvertisingState::DISABLED;
			logt("ADV", "Adv Stopped %u", err);
		}
	}

	//Start or restart with different advertising parameters
	else if(advertisingStateAction == AdvertisingStateAction::RESTART)
	{
		//If advertising is still enabled, we have to stop it first
		if(advertisingState == AdvertisingState::ENABLED){
#if (SDK == 15)
			err = FruityHal::BleGapAdvStop(handle);
#else
			err = FruityHal::BleGapAdvStop();
#endif

			//Probably worked
			advertisingState = AdvertisingState::DISABLED;
			logt("ADV", "Adv Stopped %u", err);
		}

		//We can only restart advertising if stopping worked
		if(advertisingState == AdvertisingState::DISABLED){
#if (SDK == 15)
			err = FruityHal::BleGapAdvStart(handle);
#else
			err = FruityHal::BleGapAdvStart(&currentAdvertisingParams);
#endif
			if(err == FruityHal::SUCCESS){
				logt("ADV", "Advertising enabled");
				advertisingStateAction = AdvertisingStateAction::OK;
				advertisingState = AdvertisingState::ENABLED;
			} else {
				logt("ERROR", "Error restarting advertisement %u", err);
				return;
			}
#if (SDK == 15)
			err = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, handle, Conf::defaultDBmTX);
			if (err != FruityHal::SUCCESS) {
				logt("ERROR","error code = %u", err);
			}
#endif
		}
	}
}

void AdvertisingController::RestartAdvertising()
{
	advertisingStateAction = AdvertisingStateAction::RESTART;
}

void AdvertisingController::GapConnectedEventHandler(const GapConnectedEvent & connectedEvent)
{
	if (connectedEvent.getRole() == GapRole::PERIPHERAL) {
		//If a peripheral connection got connected, we can only advertise non-connectable, reschedule
		//Also, we must restart because the advertising was stopped automatically
		if (advertisingState == AdvertisingState::ENABLED) {
			advertisingState = AdvertisingState::DISABLED;
			if(currentNumJobs > 0) RestartAdvertising();
		}
	}
}

void AdvertisingController::GapDisconnectedEventHandler(const GapDisconnectedEvent & disconnectedEvent)
{
	//TODO: Should check if this was a peripheral connection
	//If a peripheral connection is lost, we can restart advertising in connectable mode
	if (advertisingState == AdvertisingState::ENABLED) {
		if (currentNumJobs > 0) RestartAdvertising();
	}
}
/* EOF */
