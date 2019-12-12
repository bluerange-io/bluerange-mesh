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
 * The ScanController wraps SoftDevice calls around scanning/observing and
 * provides an interface to control this behaviour.
 */

#pragma once


#include <types.h>
#include <FruityHalNrf.h>

#define SCAN_CONTROLLER_JOBS_MAX	4

enum class ScanJobState : u8{
	INVALID,
	ACTIVE,
};

typedef struct ScanJob
{
	i32				timeout;
	i32				leftTimeoutDs;
	u16				interval;
	u16				window;
	ScanJobState	state;
	ScanState		type;
}ScanJob;

class ScanController
{
private:
	FruityHal::BleGapScanParams currentScanParams;
	bool scanStateOk = true;
	SimpleArray<ScanJob, SCAN_CONTROLLER_JOBS_MAX> jobs;
	ScanJob* currentActiveJob = nullptr;


	void TryConfiguringScanState();

public:
	ScanController();
	static ScanController& getInstance();

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


};

