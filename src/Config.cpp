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

#include <Config.h>

//Config.cpp initializes variables defined in Config.h with values from UICR
Conf* Conf::instance;

Conf* Conf::getInstance(){
	if(!instance){
		instance = new Conf();

		/* If we write data to NRF_UICR->CUSTOMER, it will be used by fruitymesh
		 * [0] MAGIC_NUMBER, must be set to 0xF07700 when UICR data is available
		 * [1] BOARD_TYPE, accepts an integer that defines the hardware board that fruitymesh should be running on
		 * [2] SERIAL_NUMBER, the given serial number (2 words)
		 * [4] NETWORK_KEY, random network key (4 words)
		 * [8] MANUFACTURER_ID, set to manufacturer id according to the BLE company identifiers: https://www.bluetooth.org/en-us/specification/assigned-numbers/company-identifiers
		 * [9] DEFAULT_NETWORK_ID, network id to be used if not enrolled
		 * [10] DEFAULT_NODE_ID, node id to be used if not enrolled
		 * [11] DEVICE_TYPE, type of device (sink, mobile, etc,..)
		 * [12] SERIAL_NUMBER_INDEX, unique index that represents the serial number
		 */

		//If magic number exists, fill Config with valid data from UICR
		if(NRF_UICR->CUSTOMER[0] == 0xF07700){
			instance->deviceConfigOrigin = deviceConfigOrigins::UICR_CONFIG;

			if(NRF_UICR->CUSTOMER[1] != EMPTY_WORD) instance->boardType = NRF_UICR->CUSTOMER[1];
			if(NRF_UICR->CUSTOMER[2] != EMPTY_WORD){
				memcpy((u8*)instance->serialNumber, (u8*)(NRF_UICR->CUSTOMER + 2), 5);
				instance->serialNumber[5] = '\0';
			} else {
				generateRandomSerial();
			}
			if(!isEmpty((u32*)(NRF_UICR->CUSTOMER + 4), 4)){
				memcpy(&instance->meshNetworkKey, (u8*)(NRF_UICR->CUSTOMER + 4), 16);
			}
			if(NRF_UICR->CUSTOMER[8] != EMPTY_WORD) instance->manufacturerId = (u16)NRF_UICR->CUSTOMER[8];
			if(NRF_UICR->CUSTOMER[9] != EMPTY_WORD) instance->meshNetworkIdentifier = (u16)NRF_UICR->CUSTOMER[9];
			if(NRF_UICR->CUSTOMER[10] != EMPTY_WORD) instance->defaultNodeId = (u16)NRF_UICR->CUSTOMER[10];
			if(NRF_UICR->CUSTOMER[11] != EMPTY_WORD) instance->deviceType = (deviceTypes)NRF_UICR->CUSTOMER[11];
			if(NRF_UICR->CUSTOMER[12] != EMPTY_WORD) instance->serialNumberIndex = (u32)NRF_UICR->CUSTOMER[12];

		//If no UICR data is available, we try to find a device in the testDevice array
		} else if (instance->getTestDevice() != NULL){
			instance->deviceConfigOrigin = deviceConfigOrigins::TESTDEVICE_CONFIG;

			testDevice* testDevice = instance->getTestDevice();

			generateRandomSerial();

			instance->defaultNodeId = testDevice->id;
			instance->deviceType = testDevice->deviceType;

			memcpy(&instance->staticAccessAddress, &testDevices->addr, sizeof(ble_gap_addr_t));

		//No device specific config was found, generate random values
		} else {
			instance->deviceConfigOrigin = deviceConfigOrigins::RANDOM_CONFIG;


			instance->defaultNodeId = (nodeID)NRF_FICR->DEVICEID[1] % 15000 + 1;
			generateRandomSerial();
		}
	}

	return instance;
}

//Uses the testDevice array and copies the configured values to the node settings
Conf::testDevice* Conf::getTestDevice()
{
	u8 found = 0;

	//Find our testDevice
	for(u32 i=0; i<NUM_TEST_DEVICES; i++){
		if(testDevices[i].chipID == NRF_FICR->DEVICEID[1])
		{
			return &(testDevices[i]);
		}
	}
	return NULL;
}


void Conf::generateRandomSerial(){
	//Generate a random serial number
	//(removed vocals to prevent bad words, removed 0 because it could be mistaken for an o)
	const char* alphabet = "BCDFGHJKLMNPQRSTVWXYZ123456789"; //30 chars

	//This takes 5bit wide chunks from the device id to generate a serial number
	//in tests, 10k serial numbers had 4 duplicates
	for(int i=0; i<SERIAL_NUMBER_LENGTH; i++){
		u8 fiveBitChunk = (NRF_FICR->DEVICEID[0] & 0x1F << (i*5)) >> (i*5);
		instance->serialNumber[i] = alphabet[fiveBitChunk % 30];
	}
	instance->serialNumber[SERIAL_NUMBER_LENGTH] = '\0';
}

//Tests if a memory region in flash storage is empty (0xFF)
bool Conf::isEmpty(u32* mem, u8 numWords){
	for(u8 i=0; i<numWords; i++){
		if(*(mem+i) != 0xFFFFFFFF) return false;
	}
	return true;
}

/*
IDs for development devices:
	Use this section to map the nRF chip id to some of your desired values
	This makes it easy to deploy the same firmware to a number of nodes and have them use Fixed settings

Parameters:
	- chipID: Boot the device with this firmware, enter "status" in the terminal and copy the chipID that is read from the NRF_FICR->DEVICEID[1] register (chipIdB)
	- nodeID: Enter the desired nodeID here (the last 3 digits of the segger id for example)
	- deviceType: whether the node is a data endpoint, moving around or static
	- string representation of the node id for the terminal
	- desired BLE access address: Must comply to the spec (only modify the first byte for starters)
*/
Conf::testDevice Conf::testDevices[NUM_TEST_DEVICES] = {

		{ 1650159794, 45, DEVICE_TYPE_SINK, {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x45, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 2267790660, 72, DEVICE_TYPE_SINK, {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x72, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 931144702, 458, DEVICE_TYPE_STATIC, {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x58, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 1952379473, 635, DEVICE_TYPE_STATIC, {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x35, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 3505517882, 847, DEVICE_TYPE_STATIC, {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x47, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 0xFFFF, 667, DEVICE_TYPE_STATIC, {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x67, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 1812994605, 304, DEVICE_TYPE_STATIC, {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x04, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 449693942, 493, DEVICE_TYPE_STATIC, {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x93, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 306206226, 309, DEVICE_TYPE_STATIC, {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x09, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 1040859205, 880, DEVICE_TYPE_STATIC, {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x80, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } }

	};

//Insert node ids here and they will receive a colored led when clustering
//after clustering, all nodes should have the color of the node that generated the cluster
nodeID Conf::testColourIDs[NUM_TEST_COLOUR_IDS] = {
		45,
		880,
		304,
		4290,
		9115,
		309,

		14980,
		2807,
		583,
		6574,
		12583,
		6388

};
