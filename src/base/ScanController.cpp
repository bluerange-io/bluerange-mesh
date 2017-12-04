/**

Copyright (c) 2014-2017 "M-Way Solutions GmbH"
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

#include <Node.h>
#include <ScanController.h>
#include <ConnectionManager.h>
#include <Logger.h>
#include <Config.h>

extern "C"
{
#include <app_error.h>
}

ScanController::ScanController()
{
	//Define scanning Parameters
	currentScanParams.interval = (u16) Config->meshScanIntervalHigh;				// Scan interval.
	currentScanParams.window = (u16) Config->meshScanWindowHigh;	// Scan window.
	currentScanParams.timeout = 0;					// Never stop scanning unless explicit asked to.

	scanningState = SCAN_STATE_OFF;
}


//Start scanning with the specified scanning parameters
void ScanController::SetScanState(scanState newState)
{
	u32 err = 0;
	if (newState == scanningState) return;

	//Stop scanning to either leave it stopped or update it
	if (scanningState != SCAN_STATE_OFF)
	{
		err = FruityHal::BleGapScanStop();
		if(err != NRF_SUCCESS){
			//We'll just ignore NRF_ERROR_INVALID_STATE and hope that scanning is stopped
		}
		logt("C", "Scanning stopped");
	}

	if (newState == SCAN_STATE_HIGH)
	{
		currentScanParams.interval = Config->meshScanIntervalHigh;
		currentScanParams.window = Config->meshScanWindowHigh;
	}
	else if (newState == SCAN_STATE_LOW)
	{
		currentScanParams.interval = Config->meshScanIntervalLow;
		currentScanParams.window = Config->meshScanWindowLow;
	}

	//FIXME: Add Saveguard. Because if we are currently in connecting state, we can not scan

	if (newState != SCAN_STATE_OFF)
	{
		err = FruityHal::BleGapScanStart(&currentScanParams);
		if(err == NRF_SUCCESS){
			logt("C", "Scanning started");
		} else {
			//Ignore all errors, scanning could not be started
			newState = SCAN_STATE_OFF;
		}
	}

	scanningState = newState;
}

void ScanController::SetScanDutyCycle(u16 interval, u16 window){

	u32 err;
	if (scanningState != SCAN_STATE_OFF)
	{
		err = FruityHal::BleGapScanStop();
		if(err != NRF_SUCCESS){
			//We'll just ignore NRF_ERROR_INVALID_STATE and hope that scanning is stopped
		}
		logt("C", "Scanning stopped");
	}


	if(interval != 0){
		currentScanParams.interval = interval;
		currentScanParams.window = window;

		err = FruityHal::BleGapScanStart(&currentScanParams);
		if(err == NRF_SUCCESS){
			logt("C", "Scanning started");
		} else {
			//Ignore all errors, scanning could not be started
			scanningState = SCAN_STATE_OFF;
		}
	}
}

//If a BLE event occurs, this handler will be called to do the work
bool ScanController::ScanEventHandler(ble_evt_t * bleEvent)
{
	//u32 err = 0;

	//Depending on the type of the BLE event, we have to do different stuff
	switch (bleEvent->header.evt_id)
	{
		//########## Advertisement data coming in
		case BLE_GAP_EVT_ADV_REPORT:
		{
			//Check if packet is a valid mesh advertising packet
			advPacketHeader* packetHeader = (advPacketHeader*) bleEvent->evt.gap_evt.params.adv_report.data;

			if (
					bleEvent->evt.gap_evt.params.adv_report.dlen >= SIZEOF_ADV_PACKET_HEADER
					&& packetHeader->manufacturer.companyIdentifier == COMPANY_IDENTIFIER
					&& packetHeader->meshIdentifier == MESH_IDENTIFIER
					&& packetHeader->networkId == Node::getInstance()->persistentConfig.networkId
				)
			{
				//Packet is valid and belongs to our network, forward to Node for further processing
				Node::getInstance()->AdvertisementMessageHandler(bleEvent);

			}

			return true;
		}

		case BLE_GAP_EVT_CONNECTED:
		{
			//In case, a connection comes in, scanning might have been stopped before (valid for outgoing connections)
			SetScanState(SCAN_STATE_HIGH);

		}
			break;

			//########## Timout Event (when does this happen?)
		case BLE_GAP_EVT_TIMEOUT:
			if (bleEvent->evt.gap_evt.params.timeout.src == BLE_GAP_TIMEOUT_SRC_SCAN)
			{
				logt("SCAN", "Scan timed out.");
			}
			break;
		default:
			break;
	}
	return false;
}


//EOF
