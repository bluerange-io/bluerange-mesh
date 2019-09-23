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

#include <Node.h>
#include <ScanController.h>
#include <Logger.h>
#include <Config.h>
#include <GlobalState.h>
#include "Utility.h"


/**
 * IMPORTANT: The ScanController must be informed if the scan state changes without its
 * knowledge, e.g. when callind sd_ble_gap_connect. Otherwise scanning will stop and it will
 * not know it has to restart scanning.
 */

ScanController::ScanController()
{
	//Define scanning Parameters
	currentScanParams.interval = 0;				// Scan interval.
	currentScanParams.window = 0;	// Scan window.
	currentScanParams.timeout = 0;					// Never stop scanning unless explicit asked to.

	scanStateOk = true;
	jobs.zeroData();
#if SDK == 15
	CheckedMemset(scanBuffer, 0, sizeof(scanBuffer));
#endif
}

void ScanController::TimerEventHandler(u16 passedTimeDs)
{
	for (u8 i = 0; i < jobs.length; i++)
	{
		if ((jobs[i].state == ScanJobState::ACTIVE) &&
			(jobs[i].timeout != 0))
		{
			//logt("SC", "Active scan job: %d", jobs[i].leftTimeoutDs);
			jobs[i].leftTimeoutDs -= passedTimeDs;
			if (jobs[i].leftTimeoutDs <= 0) RemoveJob(&jobs[i]);
		}
	}
	//To be absolutely sure that scanning is in the correct state, we call this function
	//within the timerHandler
	TryConfiguringScanState();
}

ScanController & ScanController::getInstance()
{
	return GS->scanController;
}

// Add new scanner job
// If the new job has higher duty cycle than current job it will be set as current.
ScanJob* ScanController::AddJob(ScanJob& job)
{
	if (job.state == ScanJobState::INACTIVE) return nullptr;
	if (job.type == ScanState::HIGH)
	{
		job.interval = Conf::getInstance().meshScanIntervalHigh;
		job.window = Conf::getInstance().meshScanWindowHigh;
		job.timeout = 0;
	}
	else if (job.type == ScanState::LOW)
	{
		job.interval = Conf::getInstance().meshScanIntervalLow;
		job.window = Conf::getInstance().meshScanWindowLow;
		job.timeout = 0;
	}
	else if (job.type == ScanState::CUSTOM)
	{
		// left empty on purpose
	}
	else
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
		return nullptr;
	}

	for (u8 i = 0; i < jobs.length; i++)
	{
		if (jobs[i].state == ScanJobState::ACTIVE) continue;
		job.leftTimeoutDs = job.timeout * 10;
		jobs[i] = job;
		RefreshJobs();
		return &jobs[i];
	}

	SIMEXCEPTION(OutOfMemoryException); //LCOV_EXCL_LINE assertion
	return nullptr;
}

// Checks duty cycle of given job and compares to current. If new one has higher duty cycle
// scannit will be restarted with new params.
void ScanController::RefreshJobs()
{
	u8 currentDutyCycle = currentScanParams.interval != 0 ? (currentScanParams.window * 100) / currentScanParams.interval : 0;
	u8 newDutyCycle = 0;
	ScanJob * p_job = nullptr;
	for (u8 i = 0; i < jobs.length; i++)
	{
		if (jobs[i].state == ScanJobState::ACTIVE)
		{
			u8 tempDutyCycle = (jobs[i].window * 100) / jobs[i].interval;
			if (tempDutyCycle > newDutyCycle)
			{
				newDutyCycle = tempDutyCycle;
				p_job = &jobs[i];
			}
		}
	}
	
	// no active jobs
	if ((newDutyCycle != currentDutyCycle) && (newDutyCycle == 0))
	{
		scanStateOk = false;
		CheckedMemset(&currentScanParams, 0, sizeof(currentScanParams));
		TryConfiguringScanState();
	}

	// new highest duty cycle
	if ((newDutyCycle != currentDutyCycle) && (p_job != nullptr))
	{
		scanStateOk = false;
		currentScanParams.window = p_job->window;
		currentScanParams.interval = p_job->interval;
		currentScanParams.timeout = 0;
		TryConfiguringScanState();
	}
}

void ScanController::RemoveJob(ScanJob * p_jobHandle)
{
	for (int i = 0; i < jobs.length; i++) {
		if (&(jobs[i]) == p_jobHandle && jobs[i].state != ScanJobState::INACTIVE)
		{
			p_jobHandle->state = ScanJobState::INACTIVE;
		}
	}
	RefreshJobs();
}

//This will call the HAL to enable the current scan state
void ScanController::TryConfiguringScanState()
{
	u32 err;
		if(!scanStateOk){
		//First, try stopping
		err = FruityHal::BleGapScanStop();
		if ((err == NRF_SUCCESS) || (err == NRF_ERROR_INVALID_STATE)) {
			if (currentScanParams.window == 0) {
				scanStateOk = true;
				return;
			}
		}
		else 
		{
			return;
		}
		//Next, try starting
#if SDK == 15
		err = FruityHal::BleGapScanStart(&currentScanParams, scanBuffer);
#else
		err = FruityHal::BleGapScanStart(&currentScanParams);
#endif
		if (err == NRF_SUCCESS) scanStateOk = true;
	}
}

void ScanController::ScanningHasStopped()
{
	scanStateOk = false;
}

//If a BLE event occurs, this handler will be called to do the work
bool ScanController::ScanEventHandler(const GapAdvertisementReportEvent& advertisementReportEvent) const
{
	//Check if packet is a valid mesh advertising packet
	const advPacketHeader* packetHeader = (const advPacketHeader*)advertisementReportEvent.getData();

	if (
			advertisementReportEvent.getDataLength() >= SIZEOF_ADV_PACKET_HEADER
			&& packetHeader->manufacturer.companyIdentifier == COMPANY_IDENTIFIER
			&& packetHeader->meshIdentifier == MESH_IDENTIFIER
			&& packetHeader->networkId == GS->node.configuration.networkId
		)
	{
		//Packet is valid and belongs to our network, forward to Node for further processing
		GS->node.GapAdvertisementMessageHandler(advertisementReportEvent);

	}

	return true;
}


//EOF
