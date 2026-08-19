////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// ** BlueRange Mesh – Community Edition (CE)
// ** Copyright (c) 2015-2021 MWAY DIGITAL GmbH, Germany
// ** Copyright (c) 2021-2026 BlueRange GmbH, Germany
// **
// ** This file is part of BlueRange Mesh Community Edition (formerly known as
// ** FruityMesh).
// **
// ** BlueRange Mesh Community Edition is free software: you can redistribute it
// ** and/or modify it under the terms of the GNU General Public License as
// ** published by the Free Software Foundation, either version 3 of the
// ** License, or (at your option) any later version.
// **
// ** BlueRange Mesh Community Edition is distributed in the hope that it will
// ** be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// ** See the GNU General Public License for more details.
// **
// ** You should have received a copy of the GNU General Public License along
// ** with this program. If not, see https://www.gnu.org/licenses/.
// **
// ** IMPORTANT:
// ** Any modification, extension, or derivative work of this file MUST also be
// ** licensed under the GNU General Public License v3 or later and the complete
// ** corresponding source code MUST be made available.
// **
// ** Commercial Use:
// ** If you wish to use this software without the obligations of the GPLv3
// ** (including source code disclosure), a commercial license for
// ** BlueRange Mesh OEM Edition is required.
// **
// ** License violations automatically terminate your rights under this license
// ** and may result in legal action under applicable law.
// ** For further information please use the contact form at:
// ** https://bluerange.io/en/contact
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
    CheckedMemset(&currentScanParams, 0, sizeof(currentScanParams));
}

void ScanController::TimerEventHandler(u16 passedTimeDs)
{
    for (u8 i = 0; i < jobs.size(); i++)
    {
        if ((jobs[i].state == ScanJobState::ACTIVE) &&
            (jobs[i].timeMode == ScanJobTimeMode::TIMED))
        {
            jobs[i].timeLeftDs -= passedTimeDs;
            if (jobs[i].timeLeftDs <= 0)
            {
                logt("SC", "Job timed out with id %u", i);
                RemoveJob(&jobs[i]);
            }
        }
    }
    //To be absolutely sure that scanning is in the correct state, we call this function
    //within the timerHandler
    TryConfiguringScanState();
}

ScanController & ScanController::GetInstance()
{
    return GS->scanController;
}

// Add new scanner job
// If the new job has higher duty cycle than current job it will be set as current.
ScanJob* ScanController::AddJob(ScanJob& job)
{
    if (job.state == ScanJobState::INVALID) return nullptr;
    if (job.type == ScanState::HIGH)
    {
        job.interval = Conf::GetInstance().meshScanIntervalHigh;
        job.window = Conf::GetInstance().meshScanWindowHigh;
        job.timeMode = ScanJobTimeMode::ENDLESS;
    }
    else if (job.type == ScanState::LOW)
    {
        job.interval = Conf::GetInstance().meshScanIntervalLow;
        job.window = Conf::GetInstance().meshScanWindowLow;
        job.timeMode = ScanJobTimeMode::ENDLESS;
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

    for (u8 i = 0; i < jobs.size(); i++)
    {
        if (jobs[i].state != ScanJobState::INVALID) continue;
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
    for (u8 i = 0; i < jobs.size(); i++)
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
    for (u32 i = 0; i < jobs.size(); i++) {
        if (&(jobs[i]) == p_jobHandle && jobs[i].state != ScanJobState::INVALID)
        {
            p_jobHandle->state = ScanJobState::INVALID;
        }
    }
    RefreshJobs();
}

void ScanController::UpdateJobPointer(ScanJob **outUpdatePtr, ScanState type, ScanJobState state)
{
    GS->scanController.RemoveJob(*outUpdatePtr);
    ScanJob scanJob = ScanJob();
    scanJob.type = type;
    scanJob.state = state;
    *outUpdatePtr = GS->scanController.AddJob(scanJob);
}

//This will call the HAL to enable the current scan state
void ScanController::TryConfiguringScanState()
{
    ErrorType err;
    if(!scanStateOk){
        //First, try stopping
        err = FruityHal::BleGapScanStop();
        if ((err == ErrorType::SUCCESS) || (err == ErrorType::INVALID_STATE)) {
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
        err = FruityHal::BleGapScanStart(currentScanParams);
        if (err == ErrorType::SUCCESS) scanStateOk = true;
    }
}

void ScanController::ScanningHasStopped()
{
    scanStateOk = false;
}

const std::array<ScanJob, SCAN_CONTROLLER_MAX_NUM_JOBS>& ScanController::GetJobs() const
{
    return jobs;
}

#ifdef SIM_ENABLED
int ScanController::GetAmountOfJobs()
{
    return jobs.size();
}

ScanJob * ScanController::GetJob(int index)
{
    return jobs.data() + index;
}
#endif //SIM_ENABLED

//If a BLE event occurs, this handler will be called to do the work
bool ScanController::ScanEventHandler(const FruityHal::GapAdvertisementReportEvent& advertisementReportEvent) const
{
    //Check if packet is a valid mesh advertising packet
    const AdvPacketHeader* packetHeader = (const AdvPacketHeader*)advertisementReportEvent.GetData();

    if (
            advertisementReportEvent.GetDataLength() >= SIZEOF_ADV_PACKET_HEADER
            && packetHeader->manufacturer.companyIdentifier == MESH_COMPANY_IDENTIFIER
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
