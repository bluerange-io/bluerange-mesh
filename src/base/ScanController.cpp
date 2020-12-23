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
