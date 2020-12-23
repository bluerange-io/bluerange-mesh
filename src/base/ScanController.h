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

#pragma once


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
    std::array<ScanJob, 4> jobs{};

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

#ifdef SIM_ENABLED
    int GetAmountOfJobs();
    ScanJob* GetJob(int index);
#endif //SIM_ENABLED
};

