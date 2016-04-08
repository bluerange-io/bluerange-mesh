/**
 OS_LICENSE_PLACEHOLDER
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




scanState ScanController::scanningState = SCAN_STATE_OFF; //The current state of scanning

//The currently used parameters for scanning
ble_gap_scan_params_t currentScanParams;

void ScanController::Initialize(void)
{

	//Define scanning Parameters
	currentScanParams.active = 0;					// Active scanning set.
	currentScanParams.selective = 0;				// Selective scanning not set.
	currentScanParams.p_whitelist = NULL;				// White-list not set.
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
		err = sd_ble_gap_scan_stop();
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
		err = sd_ble_gap_scan_start(&currentScanParams);
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
		err = sd_ble_gap_scan_stop();
		if(err != NRF_SUCCESS){
			//We'll just ignore NRF_ERROR_INVALID_STATE and hope that scanning is stopped
		}
		logt("C", "Scanning stopped");
	}


	if(interval != 0){
		currentScanParams.interval = interval;
		currentScanParams.window = window;

		err = sd_ble_gap_scan_start(&currentScanParams);
		if(err == NRF_SUCCESS){
			logt("C", "Scanning started");
		} else {
			//Ignore all errors, scanning could not be started
			scanningState = SCAN_STATE_OFF;
		}
	}
}

//BLE addresses, 6 Byte (48bit), can be either random, public, etc...
ble_gap_addr_t blePeripheralAdresses[1] = { BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x25, 0xED, 0xA4, 0x6B, 0xC6, 0xE7 } /*0xD91220800D00*/};

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
