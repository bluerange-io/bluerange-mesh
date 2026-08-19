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

#pragma once


#include "Config.h"
#include <FmTypes.h>
#include <array>

enum class ScanJobState : u8{
    INVALID,
    ACTIVE,
};

enum class ScanJobTimeMode : u8 {
    ENDLESS,
    TIMED,
};

typedef struct ScanJob
{
    ScanJobTimeMode timeMode;
    i32             timeLeftDs;
    u16             interval;
    u16             window;
    ScanJobState    state;
    ScanState       type;
}ScanJob;

//Forward declaration
class DebugModule;

/*
 * The ScanController wraps SoftDevice calls around scanning/observing and
 * provides an interface to control this behaviour.
 * It also includes a job manager where all scan jobs are managed.
 */
class ScanController
{
    friend DebugModule;

private:
    FruityHal::BleGapScanParams currentScanParams;
    bool scanStateOk = true;
    std::array<ScanJob, SCAN_CONTROLLER_MAX_NUM_JOBS> jobs{};

    void TryConfiguringScanState();

public:
    ScanController();
    static ScanController& GetInstance();

    //Job Scheduling
    ScanJob* AddJob(ScanJob& job);
    void RefreshJobs();
    void RemoveJob(ScanJob * p_jobHandle);
    //Helper for a common use, where an old job should be removed (if set), and
    //a new one should be created with a given ScanState and ScanJobState.
    void UpdateJobPointer(ScanJob **outUpdatePtr, ScanState type, ScanJobState state);

    void TimerEventHandler(u16 passedTimeDs);

    bool ScanEventHandler(const FruityHal::GapAdvertisementReportEvent& advertisementReportEvent) const;

    //Must be called if scanning was stopped by any external procedure
    void ScanningHasStopped();

    const std::array<ScanJob, SCAN_CONTROLLER_MAX_NUM_JOBS>& GetJobs() const;

#ifdef SIM_ENABLED
    int GetAmountOfJobs();
    ScanJob* GetJob(int index);
#endif //SIM_ENABLED
};

