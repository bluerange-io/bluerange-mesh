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
		 */

		//If magic number exists, fill Config with valid data from UICR
		if(NRF_UICR->CUSTOMER[0] == 0xF07700){
			if(instance->boardType != EMPTY_WORD) instance->boardType = NRF_UICR->CUSTOMER[1];
			if(NRF_UICR->CUSTOMER[2] != EMPTY_WORD){
				memcpy((u8*)instance->serialNumber, (u8*)(NRF_UICR->CUSTOMER + 2), 5);
				instance->serialNumber[5] = '\0';
			} else {
				generateRandomSerial();
			}
			/*if(!isEmpty((u8*)(NRF_UICR->CUSTOMER + 2), 16)){
				memcpy(&instance->meshNetworkKey, (u8*)(NRF_UICR->CUSTOMER + 2), 16);
			}*/
			if(NRF_UICR->CUSTOMER[9] != EMPTY_WORD) Config->meshNetworkIdentifier = (u16)NRF_UICR->CUSTOMER[9];
			if(NRF_UICR->CUSTOMER[10] != EMPTY_WORD) Config->defaultNodeId = (u16)NRF_UICR->CUSTOMER[10];
		}
	}

	return instance;
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
bool Conf::isEmpty(u8* mem, u16 numBytes){
	for(u16 i=0; i<numBytes; i++){
		if(mem[i] != 0xFF) return false;
	}
	return true;
}
