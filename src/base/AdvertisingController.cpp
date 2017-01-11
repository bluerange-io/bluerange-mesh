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

#include <AdvertisingController.h>
#include <ConnectionManager.h>
#include <Logger.h>
#include <Config.h>

extern "C"{
#include <nrf_error.h>
#include <app_error.h>
}

//

/*
TODO:
- should allow to set intervals either HIGH/LOW or custom
- should have callback after sending an advertising packet??

 */


advState AdvertisingController::advertisingState = ADV_STATE_OFF; //The current state of advertising


ble_gap_adv_params_t AdvertisingController::currentAdvertisingParams;
u8 AdvertisingController::currentAdvertisementPacket[40] = { 0 };
u8 AdvertisingController::currentScanResponsePacket[40] = { 0 };
advPacketHeader* AdvertisingController::header = NULL;
scanPacketHeader* AdvertisingController::scanHeader = NULL;
u8 AdvertisingController::currentAdvertisementPacketLength = 0;
bool AdvertisingController::advertisingPacketAwaitingUpdate = false;


AdvertisingController::AdvertisingController()
{

}

void AdvertisingController::Initialize(u16 networkIdentifier)
{
	u32 err = 0;

	advertisingPacketAwaitingUpdate = false;
	currentAdvertisementPacketLength = 0;

	//Define default Advertisement params
	currentAdvertisingParams.type = BLE_GAP_ADV_TYPE_ADV_SCAN_IND; //Connectable
	currentAdvertisingParams.p_peer_addr = NULL; //Unidirected, not directed
	currentAdvertisingParams.fp = BLE_GAP_ADV_FP_ANY; //Filter policy
	currentAdvertisingParams.p_whitelist = NULL;
	currentAdvertisingParams.interval = 0x0200; // Advertising interval between 0x0020 and 0x4000 in 0.625 ms units (20ms to 10.24s), see @ref BLE_GAP_ADV_INTERVALS
	currentAdvertisingParams.timeout = 0;
	currentAdvertisingParams.channel_mask.ch_37_off = Config->advertiseOnChannel37 ? 0 : 1;
	currentAdvertisingParams.channel_mask.ch_38_off = Config->advertiseOnChannel38 ? 0 : 1;
	currentAdvertisingParams.channel_mask.ch_39_off = Config->advertiseOnChannel39 ? 0 : 1;

	//Set state
	advertisingState = ADV_STATE_OFF;

	//----------------- Prefill advertisement header
	header = (advPacketHeader*) currentAdvertisementPacket;
	header->flags.len = SIZEOF_ADV_STRUCTURE_FLAGS-1; //minus length field itself
	header->flags.type = BLE_GAP_AD_TYPE_FLAGS;
	header->flags.flags = BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;

	// LENGTH will be set later //

	header->manufacturer.type = BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA;
	header->manufacturer.companyIdentifier = COMPANY_IDENTIFIER;

	//The specific instance of a mesh network
	header->meshIdentifier = MESH_IDENTIFIER;
	header->networkId = networkIdentifier;

	//---------------- Prefill scan response header
	u16 size = 0;


	scanHeader = (scanPacketHeader*) currentScanResponsePacket;

	scanHeader->name.len = 3;
	scanHeader->name.type = BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME;
	scanHeader->name.name[0] = 'F';
	scanHeader->name.name[1] = 'M';

	scanHeader->manufacturer.len = 3;
	scanHeader->manufacturer.type = BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA;
	scanHeader->manufacturer.companyIdentifier = COMPANY_IDENTIFIER;

}

void AdvertisingController::SetConnectable()
{
	currentAdvertisingParams.type = BLE_GAP_ADV_TYPE_ADV_IND;
}
void AdvertisingController::SetNonConnectable()
{
	currentAdvertisingParams.type = BLE_GAP_ADV_TYPE_ADV_NONCONN_IND;
}

u32 AdvertisingController::UpdateAdvertisingData(u8 messageType, sizedData* payload, bool connectable)
{
	u32 err = NRF_SUCCESS;

	//Check if we  are switching to connectable / non-connectable
	bool mustRestartAdvertising = false;
	if (connectable && currentAdvertisingParams.type != BLE_GAP_ADV_TYPE_ADV_IND)
	{

		logt("ADV", "Switching to connectable Advertising");
		currentAdvertisingParams.type = BLE_GAP_ADV_TYPE_ADV_IND;
		mustRestartAdvertising = true;
	}
	else if (!connectable && currentAdvertisingParams.type != BLE_GAP_ADV_TYPE_ADV_SCAN_IND)
	{
		logt("ADV", "Switching to non-connectable Advertising");
		currentAdvertisingParams.type = BLE_GAP_ADV_TYPE_ADV_SCAN_IND;
		mustRestartAdvertising = true;
	}

	currentAdvertisementPacketLength = SIZEOF_ADV_PACKET_HEADER + payload->length;

	//Set the data payload
	header->messageType = messageType;
	header->manufacturer.len = payload->length + SIZEOF_ADV_PACKET_STUFF_AFTER_MANUFACTURER;
	memcpy((u8*) header + SIZEOF_ADV_PACKET_HEADER, payload->data, payload->length);

	err = sd_ble_gap_adv_data_set(currentAdvertisementPacket, SIZEOF_ADV_PACKET_HEADER + payload->length, NULL, 0);
	if(err != NRF_SUCCESS){
		advertisingPacketAwaitingUpdate = true;
		return err;
	}

	//Now we check if we need to restart advertising
	if (mustRestartAdvertising)
	{
		advState backup = advertisingState;
		SetAdvertisingState(ADV_STATE_OFF);
		SetAdvertisingState(backup);
	}

	return NRF_SUCCESS;
}

u32 AdvertisingController::SetScanResponse(sizedData* payload){
	u32 err;

	scanHeader->manufacturer.len = 3 + payload->length;

	memcpy(currentScanResponsePacket + SIZEOF_SCAN_PACKET_HEADER, payload->data, payload->length);

	err = sd_ble_gap_adv_data_set(NULL, 0, currentScanResponsePacket, SIZEOF_SCAN_PACKET_HEADER + payload->length);

	return err;
}

bool AdvertisingController::SetScanResponseData(Node* node, string dataString){
	//Send data over all connections
	connPacketData1 data;
	data.header.messageType = MESSAGE_TYPE_DATA_2;
	data.header.sender = node->persistentConfig.nodeId;
	data.header.receiver = 0;

	if(dataString.empty() || dataString.length() > 20){
		uart_error(Logger::ARGUMENTS_WRONG);
		return false;
	} else {
		data.payload.length = dataString.length();
		memcpy(data.payload.data, dataString.c_str(), data.payload.length);

		ConnectionManager::getInstance()->SendMessageOverConnections(NULL, (u8*) &data, SIZEOF_CONN_PACKET_DATA_2, true);

		//Update the node's scan response as well
		node->UpdateScanResponsePacket((u8*)dataString.c_str(), data.payload.length);

		uart_error(Logger::NO_ERROR);

		return true;
	}
}

void AdvertisingController::SetAdvertisingState(advState newState)
{
	if (newState == advertisingState)
		return;

	u32 err;

	//Stop if it should be stopped or because it is currently running
	if (advertisingState != ADV_STATE_OFF)
	{
		err = sd_ble_gap_adv_stop();
		if(err == NRF_SUCCESS){
			logt("ADV", "Advertising stopped");
		} else {
			//Was probably stopped before
		}
	}

	if (newState == ADV_STATE_HIGH)
		currentAdvertisingParams.interval = Config->meshAdvertisingIntervalHigh;
	else if (newState == ADV_STATE_LOW)
		currentAdvertisingParams.interval = Config->meshAdvertisingIntervalLow;

	//Check if the advertisement packet did not get updated before
	if(advertisingPacketAwaitingUpdate){
		err = sd_ble_gap_adv_data_set(currentAdvertisementPacket, currentAdvertisementPacketLength, NULL, 0);
		if(err == NRF_SUCCESS){
			advertisingPacketAwaitingUpdate = false;
		} else {
			//Just don't set it if it is wrong
		}

	}

	//Start advertising if needed
	if (newState != ADV_STATE_OFF)
	{
		err = sd_ble_gap_adv_start(&currentAdvertisingParams);
		if(err == NRF_SUCCESS){
			logt("ADV", "Advertising started");
		} else {
			//Could not be started, ignore
			newState = ADV_STATE_OFF;
		}
	}

	advertisingState = newState;
}

//If Advertising was interrupted, restart in previous state
void AdvertisingController::AdvertisingInterruptedBecauseOfIncomingConnectionHandler(void)
{
	logt("ADV", "advertising interrupted");
	advertisingState = ADV_STATE_OFF;
	currentAdvertisingParams.type = BLE_GAP_ADV_TYPE_ADV_SCAN_IND;

}


//If a BLE event occurs, this handler will be called to do the work
//BLE events can either come from GAP, GATT client, GATT server or L2CAP
//Returns true or false, weather the event has been handled
bool AdvertisingController::AdvertiseEventHandler(ble_evt_t* bleEvent)
{
	u32 err = 0;

	//Depending on the type of the BLE event, we have to do different stuff
	//Further Events: http://developer.nordicsemi.com/nRF51_SDK/doc/7.1.0/s120/html/a00746.html#gada486dd3c0cce897b23a887bed284fef
	switch (bleEvent->header.evt_id)
	{
	//************************** GAP *****************************
	//########## Connection with other device
	case BLE_GAP_EVT_CONNECTED:
	{
		break;
	}
	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
	{
		break;
	}
		//########## 
	case BLE_GAP_EVT_AUTH_STATUS:
	{
		break;
	}
		//########## 
	case BLE_GAP_EVT_SEC_INFO_REQUEST:
	{
		break;
	}
		//########## 
	case BLE_GAP_EVT_TIMEOUT:
	{
		break;
	}

		//************************** GATT server *****************************

		//************************** GATT client *****************************

	default:
		break;
	}
	return false;
}
/* EOF */
