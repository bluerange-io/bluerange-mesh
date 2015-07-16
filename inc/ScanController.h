/**

Copyright (c) 2014-2015 "M-Way Solutions GmbH"
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


public:

	static scanState scanningState; //The current state of scanning

	static void Initialize(void);
	static void SetScanState(scanState newState);

	static void SetScanDutyCycle(u16 interval, u16 window);

	static bool ScanEventHandler(ble_evt_t * p_ble_evt);


};

