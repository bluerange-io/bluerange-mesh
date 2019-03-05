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

#include <TestBattery.h>

#ifdef ACITVATE_TEST_BATTERY

#include <Logger.h>
#include <AdvertisingController.h>
#include <ScanController.h>
#include <IoModule.h>

extern "C"{
#include <app_error.h>
#include <nrf_soc.h>
}

Node* TestBattery::node;
ConnectionManager* TestBattery::cm;

u16 deactivateDiscoveryAfterDs = 0;



TestBattery::TestBattery()
{

}

void TestBattery::TimerHandler(){
	if(deactivateDiscoveryAfterDs != 0 && deactivateDiscoveryAfterDs < node->appTimerDs){

		node->ChangeState(discoveryState::DISCOVERY_OFF);
		node->DisableStateMachine(true);
		((IoModule*)node->GetModuleById(moduleID::IO_MODULE_ID))->currentLedMode = ledMode::LED_MODE_OFF;

		GS->ledRed->Off();
		GS->ledGreen->Off();
		GS->ledBlue->Off();
	}
}

void TestBattery::prepareTesting()
{

	GS->logger->logEverything = true;



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
	node = GS->node;
	cm = GS->cm;

	((IoModule*)node->GetModuleById(moduleID::IO_MODULE_ID))->currentLedMode = ledMode::LED_MODE_OFF;

	//deactiveate node
	node->ChangeState(discoveryState::DISCOVERY_OFF);
	node->DisableStateMachine(true);


}

void TestBattery::advertiseAt100ms()
{
	Config->meshAdvertisingIntervalHigh = MSEC_TO_UNITS(25, UNIT_0_625_MS);
	//TODO: ADVREF GS->advertisingController->SetAdvertisingState(advState::ADV_STATE_HIGH);
}

void TestBattery::advertiseAt2000ms()
{
	Config->meshAdvertisingIntervalHigh = MSEC_TO_UNITS(2000, UNIT_0_625_MS);
	//TODO: ADVREF GS->advertisingController->SetAdvertisingState(advState::ADV_STATE_HIGH);
}

void TestBattery::advertiseAt5000ms()
{
	Config->meshAdvertisingIntervalHigh = MSEC_TO_UNITS(5000, UNIT_0_625_MS);
	//TODO: ADVREF GS->advertisingController->SetAdvertisingState(advState::ADV_STATE_HIGH);
}

void TestBattery::scanAt50Percent()
{
	Config->meshScanIntervalHigh = MSEC_TO_UNITS(1000, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
	Config->meshScanWindowHigh = MSEC_TO_UNITS(500, UNIT_0_625_MS);

	GS->scanController->SetScanState(scanState::SCAN_STATE_HIGH);
}

void TestBattery::scanAt100Percent()
{
	Config->meshScanIntervalHigh = MSEC_TO_UNITS(1000, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
	Config->meshScanWindowHigh = MSEC_TO_UNITS(980, UNIT_0_625_MS);

	GS->scanController->SetScanState(scanState::SCAN_STATE_HIGH);
}

void TestBattery::meshWith100MsConnAndHighDiscovery()
{
	Config->meshMinConnectionInterval = Config->meshMaxConnectionInterval = MSEC_TO_UNITS(100, UNIT_1_25_MS);
	Config->meshConnectionSupervisionTimeout = MSEC_TO_UNITS(6000, UNIT_10_MS);

	Config->meshAdvertisingIntervalHigh = MSEC_TO_UNITS(100, UNIT_0_625_MS);	//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
	Config->meshScanIntervalHigh = MSEC_TO_UNITS(40, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
	Config->meshScanWindowHigh = MSEC_TO_UNITS(20, UNIT_0_625_MS);

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

		node->DisableStateMachine(false);
		node->ChangeState(discoveryState::DISCOVERY_HIGH);

		//Disable discovery after 20 seconds
		((IoModule*)node->GetModuleById(moduleID::IO_MODULE_ID))->currentLedMode = ledMode::LED_MODE_CONNECTIONS;
		deactivateDiscoveryAfterDs = SEC_TO_DS(20);
}

void TestBattery::meshWith100msConnAndDiscoveryOff()
{
	Config->meshMinConnectionInterval = Config->meshMaxConnectionInterval = MSEC_TO_UNITS(100, UNIT_1_25_MS);
	Config->meshConnectionSupervisionTimeout = MSEC_TO_UNITS(6000, UNIT_10_MS);

	Config->meshAdvertisingIntervalHigh = MSEC_TO_UNITS(100, UNIT_0_625_MS);	//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
	Config->meshScanIntervalHigh = MSEC_TO_UNITS(40, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
	Config->meshScanWindowHigh = MSEC_TO_UNITS(20, UNIT_0_625_MS);

	node->DisableStateMachine(false);
	node->ChangeState(discoveryState::DISCOVERY_HIGH);

	//Disable discovery after 20 seconds
	((IoModule*)node->GetModuleById(moduleID::IO_MODULE_ID))->currentLedMode = ledMode::LED_MODE_CONNECTIONS;
	deactivateDiscoveryAfterDs = SEC_TO_DS(20);
}


void TestBattery::meshWith500msConnAndDiscoveryOff()
{
	Config->meshMinConnectionInterval = Config->meshMaxConnectionInterval = MSEC_TO_UNITS(500, UNIT_1_25_MS);
	Config->meshConnectionSupervisionTimeout = MSEC_TO_UNITS(6000, UNIT_10_MS);

	Config->meshAdvertisingIntervalHigh = MSEC_TO_UNITS(100, UNIT_0_625_MS);	//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
	Config->meshScanIntervalHigh = MSEC_TO_UNITS(40, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
	Config->meshScanWindowHigh = MSEC_TO_UNITS(20, UNIT_0_625_MS);

	node->DisableStateMachine(false);
	node->ChangeState(discoveryState::DISCOVERY_HIGH);

	//Disable discovery after 20 seconds
	((IoModule*)node->GetModuleById(moduleID::IO_MODULE_ID))->currentLedMode = ledMode::LED_MODE_CONNECTIONS;
	deactivateDiscoveryAfterDs = SEC_TO_DS(20);
}

#endif
