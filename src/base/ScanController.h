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

extern "C"{
#include <ble.h>
#include <ble_gap.h>
}


class ScanController
{
private:
	fh_ble_gap_scan_params_t currentScanParams;
	scanState scanningState; //The current state of scanning
	bool scanStateOk;

	ScanController();

	void TryConfiguringScanState();

public:
	static ScanController& getInstance(){
		if(!GS->scanController){
			GS->scanController = new ScanController();
		}
		return *(GS->scanController);
	}

	void TimerEventHandler(u16 passedTimeDs);

	void SetScanState(scanState newState);

	void SetScanDutyCycle(u16 interval, u16 window);

	bool ScanEventHandler(ble_evt_t &p_ble_evt) const;

	//Must be called if scanning was stopped by any external procedure
	void ScanningHasStopped();


};

