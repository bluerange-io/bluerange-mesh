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
 * This class will test the battery
 */

#pragma once

#include <Node.h>
#include <ConnectionManager.h>

class TestBattery
{
	private:
		static Node* node;
		static ConnectionManager* cm;

	public:
		TestBattery();

		static void prepareTesting();
		static void startTesting();
		static void advertiseAt100ms();
		static void advertiseAt2000ms();
		static void advertiseAt5000ms();
		static void scanAt50Percent();
		static void scanAt100Percent();
		static void meshWith100MsConnAndHighDiscovery();
		static void meshWith100msConnAndLowDiscovery();
		static void meshWith30msConnAndDiscoveryOff();
		static void meshWith100msConnAndDiscoveryOff();
		static void meshWith500msConnAndDiscoveryOff();

		static void TimerHandler();
};

