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

#include <TestBattery.h>
#include <Logger.h>
#include <AdvertisingController.h>
#include <ScanController.h>

extern "C"{
#include <app_error.h>
#include <nrf_soc.h>
}

Node* TestBattery::node;
ConnectionManager* TestBattery::cm;

u16 deactivateDiscoveryAfterMs = 0;



TestBattery::TestBattery()
{

}

void TestBattery::TimerHandler(){
	if(deactivateDiscoveryAfterMs != 0 && deactivateDiscoveryAfterMs < node->appTimerMs){

		node->ChangeState(discoveryState::DISCOVERY_OFF);
		node->DisableStateMachine(true);
		node->currentLedMode = ledMode::LED_MODE_OFF;

		LedRed->Off();
		LedGreen->Off();
		LedBlue->Off();
	}
}

void TestBattery::prepareTesting()
{

	Logger::getInstance().logEverything = true;



}

void TestBattery::startTesting()
{
	u32 err;

	//Enable or disable DC/DC
	err = sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
	APP_ERROR_CHECK(err); //OK

	//Set power off mode
	err = sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
	APP_ERROR_CHECK(err); //OK

	//Switch off everything possible
	node = Node::getInstance();
	cm = ConnectionManager::getInstance();

	node->currentLedMode = ledMode::LED_MODE_OFF;

	//deactiveate node
	node->ChangeState(discoveryState::DISCOVERY_OFF);
	node->DisableStateMachine(true);


}

void TestBattery::advertiseAt100ms()
{
	Config->meshAdvertisingIntervalHigh = MSEC_TO_UNITS(25, UNIT_0_625_MS);
	AdvertisingController::SetAdvertisingState(advState::ADV_STATE_HIGH);
}

void TestBattery::advertiseAt2000ms()
{
	Config->meshAdvertisingIntervalHigh = MSEC_TO_UNITS(2000, UNIT_0_625_MS);
	AdvertisingController::SetAdvertisingState(advState::ADV_STATE_HIGH);
}

void TestBattery::advertiseAt5000ms()
{
	Config->meshAdvertisingIntervalHigh = MSEC_TO_UNITS(5000, UNIT_0_625_MS);
	AdvertisingController::SetAdvertisingState(advState::ADV_STATE_HIGH);
}

void TestBattery::scanAt50Percent()
{
	Config->meshScanIntervalHigh = MSEC_TO_UNITS(1000, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
	Config->meshScanWindowHigh = MSEC_TO_UNITS(500, UNIT_0_625_MS);

	ScanController::SetScanState(scanState::SCAN_STATE_HIGH);
}

void TestBattery::scanAt100Percent()
{
	Config->meshScanIntervalHigh = MSEC_TO_UNITS(1000, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
	Config->meshScanWindowHigh = MSEC_TO_UNITS(980, UNIT_0_625_MS);

	ScanController::SetScanState(scanState::SCAN_STATE_HIGH);
}

void TestBattery::meshWith100MsConnAndHighDiscovery()
{
	Config->meshMinConnectionInterval = Config->meshMaxConnectionInterval = MSEC_TO_UNITS(100, UNIT_1_25_MS);
	Config->meshConnectionSupervisionTimeout = MSEC_TO_UNITS(6000, UNIT_10_MS);

	Config->meshAdvertisingIntervalHigh = MSEC_TO_UNITS(100, UNIT_0_625_MS);	//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
	Config->meshScanIntervalHigh = MSEC_TO_UNITS(40, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
	Config->meshScanWindowHigh = MSEC_TO_UNITS(20, UNIT_0_625_MS);

	Config->meshStateTimeoutHigh = 3 * 1000;

	Config->meshStateTimeoutBackOff = 1 * 1000;
	Config->meshStateTimeoutBackOffVariance = 1 * 1000;

	node->DisableStateMachine(false);
	node->ChangeState(discoveryState::DISCOVERY_HIGH);
}

void TestBattery::meshWith100msConnAndLowDiscovery()
{
	Config->meshMinConnectionInterval = Config->meshMaxConnectionInterval = MSEC_TO_UNITS(100, UNIT_1_25_MS);
	Config->meshConnectionSupervisionTimeout = MSEC_TO_UNITS(6000, UNIT_10_MS);

	Config->meshAdvertisingIntervalHigh = MSEC_TO_UNITS(1000, UNIT_0_625_MS);	//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
	Config->meshScanIntervalHigh = MSEC_TO_UNITS(2000, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
	Config->meshScanWindowHigh = MSEC_TO_UNITS(10, UNIT_0_625_MS);

	Config->meshStateTimeoutHigh = 8 * 1000;

	Config->meshStateTimeoutBackOff = 5 * 1000;
	Config->meshStateTimeoutBackOffVariance = 5 * 1000;

	node->DisableStateMachine(false);
	node->ChangeState(discoveryState::DISCOVERY_HIGH);
}

void TestBattery::meshWith30msConnAndDiscoveryOff()
{
	Config->meshMinConnectionInterval = Config->meshMaxConnectionInterval = MSEC_TO_UNITS(30, UNIT_1_25_MS);
		Config->meshConnectionSupervisionTimeout = MSEC_TO_UNITS(6000, UNIT_10_MS);

		Config->meshAdvertisingIntervalHigh = MSEC_TO_UNITS(100, UNIT_0_625_MS);	//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
		Config->meshScanIntervalHigh = MSEC_TO_UNITS(40, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
		Config->meshScanWindowHigh = MSEC_TO_UNITS(20, UNIT_0_625_MS);

		Config->meshStateTimeoutHigh = 3 * 1000;

		Config->meshStateTimeoutBackOff = 1 * 1000;
		Config->meshStateTimeoutBackOffVariance = 1 * 1000;

		node->DisableStateMachine(false);
		node->ChangeState(discoveryState::DISCOVERY_HIGH);

		//Disable discovery after 20 seconds
		node->currentLedMode = ledMode::LED_MODE_CONNECTIONS;
		deactivateDiscoveryAfterMs = 20 * 1000;
}

void TestBattery::meshWith100msConnAndDiscoveryOff()
{
	Config->meshMinConnectionInterval = Config->meshMaxConnectionInterval = MSEC_TO_UNITS(100, UNIT_1_25_MS);
	Config->meshConnectionSupervisionTimeout = MSEC_TO_UNITS(6000, UNIT_10_MS);

	Config->meshAdvertisingIntervalHigh = MSEC_TO_UNITS(100, UNIT_0_625_MS);	//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
	Config->meshScanIntervalHigh = MSEC_TO_UNITS(40, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
	Config->meshScanWindowHigh = MSEC_TO_UNITS(20, UNIT_0_625_MS);

	Config->meshStateTimeoutHigh = 3 * 1000;

	Config->meshStateTimeoutBackOff = 1 * 1000;
	Config->meshStateTimeoutBackOffVariance = 1 * 1000;

	node->DisableStateMachine(false);
	node->ChangeState(discoveryState::DISCOVERY_HIGH);

	//Disable discovery after 20 seconds
	node->currentLedMode = ledMode::LED_MODE_CONNECTIONS;
	deactivateDiscoveryAfterMs = 20 * 1000;
}


void TestBattery::meshWith500msConnAndDiscoveryOff()
{
	Config->meshMinConnectionInterval = Config->meshMaxConnectionInterval = MSEC_TO_UNITS(500, UNIT_1_25_MS);
	Config->meshConnectionSupervisionTimeout = MSEC_TO_UNITS(6000, UNIT_10_MS);

	Config->meshAdvertisingIntervalHigh = MSEC_TO_UNITS(100, UNIT_0_625_MS);	//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
	Config->meshScanIntervalHigh = MSEC_TO_UNITS(40, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
	Config->meshScanWindowHigh = MSEC_TO_UNITS(20, UNIT_0_625_MS);

	Config->meshStateTimeoutHigh = 3 * 1000;

	Config->meshStateTimeoutBackOff = 1 * 1000;
	Config->meshStateTimeoutBackOffVariance = 1 * 1000;

	node->DisableStateMachine(false);
	node->ChangeState(discoveryState::DISCOVERY_HIGH);

	//Disable discovery after 20 seconds
	node->currentLedMode = ledMode::LED_MODE_CONNECTIONS;
	deactivateDiscoveryAfterMs = 20 * 1000;
}

