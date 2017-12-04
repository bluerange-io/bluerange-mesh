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

#include <Config.h>
#include <Boardconfig.h>
#include <GlobalState.h>
#include <Logger.h>
#include <RecordStorage.h>

extern "C"{
}

//Config.cpp initializes variables defined in Config.h with values from UICR

//Put the firmware version in a special section right after the initialization vector
#ifndef SIM_ENABLED
uint32_t fruityMeshVersion __attribute__((section(".Version"), used)) = FM_VERSION;
#else
uint32_t fruityMeshVersion = FM_VERSION;
#endif

bool Conf::loadConfigFromFlash;

Conf::Conf()
{

}

//Uses the testDevice array and copies the configured values to the node settings
#ifdef ENABLE_TEST_DEVICES
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
#endif

void Conf::Initialize(bool loadConfigFromFlash)
{
	Conf::loadConfigFromFlash = loadConfigFromFlash;

	//First, fill with default Settings
	LoadDefaults();

	//Overwrite with settings from the settings page
	if(loadConfigFromFlash){
		LoadSettingsFromFlash(moduleID::CONFIG_ID, (ModuleConfiguration*)&configuration, sizeof(ConfigConfiguration));
	}

	//If there is UICR data available, we overwrite parts of the defaults
	if(NRF_UICR->CUSTOMER[0] == 0xF07700){
		LoadUicr();
	}

	//In case we don't have UICR data, we might find data in the testDevices section
#ifdef ENABLE_TEST_DEVICES
	else if (this->getTestDevice() != NULL){
		LoadTestDevices();
	}
#endif


}

void Conf::LoadDefaults(){
	configuration.moduleId = moduleID::CONFIG_ID;
	configuration.moduleVersion = 1;
	configuration.moduleActive = 1;
	configuration.reserved = sizeof(ConfigConfiguration);

	configuration.breakpointToggleActive = false;
	configuration.debugMode = false;
	configuration.advertiseDebugPackets = false;
	configuration.defaultLedMode = ledMode::LED_MODE_CONNECTIONS;
	if(Boardconfig->boardType == 7){
		configuration.terminalMode = TERMINAL_DISABLED;
	} else {
		configuration.terminalMode = TERMINAL_JSON_MODE;
	}
	configuration.mainTimerTickDs = 2;
	configuration.meshMinConnectionInterval = (u16) MSEC_TO_UNITS(10, UNIT_1_25_MS);
	configuration.meshMaxConnectionInterval = (u16) MSEC_TO_UNITS(10, UNIT_1_25_MS);
	configuration.meshPeripheralSlaveLatency = 0;
	configuration.meshConnectionSupervisionTimeout = (u16) MSEC_TO_UNITS(6000, UNIT_10_MS);
	configuration.meshExtendedConnectionTimeoutSec = 0;

	configuration.meshAdvertisingIntervalHigh = (u16) MSEC_TO_UNITS(100, UNIT_0_625_MS);
	configuration.meshScanIntervalHigh = (u16) MSEC_TO_UNITS(20, UNIT_0_625_MS);
	configuration.meshScanWindowHigh = (u16) MSEC_TO_UNITS(3, UNIT_0_625_MS);

	configuration.meshAdvertisingIntervalLow = (u16) MSEC_TO_UNITS(100, UNIT_0_625_MS);
	configuration.meshScanIntervalLow = (u16) MSEC_TO_UNITS(20, UNIT_0_625_MS);
	configuration.meshScanWindowLow = (u16) MSEC_TO_UNITS(3, UNIT_0_625_MS);

	configuration.meshConnectingScanInterval = (u16) MSEC_TO_UNITS(20, UNIT_0_625_MS);
	configuration.meshConnectingScanWindow = (u16) MSEC_TO_UNITS(4, UNIT_0_625_MS);
	configuration.meshConnectingScanTimeout = 1;
	configuration.meshHandshakeTimeoutDs = SEC_TO_DS(4);
	configuration.meshStateTimeoutHighDs = SEC_TO_DS(2);
	configuration.meshStateTimeoutLowDs = SEC_TO_DS(10);
	configuration.meshStateTimeoutBackOffDs = SEC_TO_DS(0);
	configuration.meshStateTimeoutBackOffVarianceDs = SEC_TO_DS(1);
	configuration.discoveryHighToLowTransitionDuration = 10;
	configuration.advertiseOnChannel37 = 1;
	configuration.advertiseOnChannel38 = 1;
	configuration.advertiseOnChannel39 = 1;
	configuration.meshMaxInConnections = MESH_IN_CONNECTIONS;
	configuration.meshMaxOutConnections = MESH_OUT_CONNECTIONS;
	configuration.meshMaxConnections = configuration.meshMaxInConnections + configuration.meshMaxOutConnections;
	configuration.totalInConnections = MESH_IN_CONNECTIONS;
	configuration.totalOutConnections = MESH_OUT_CONNECTIONS;
	configuration.gapEventLength = 3;
	configuration.enableRadioNotificationHandler = false;
	configuration.enableConnectionRSSIMeasurement = true;

	configuration.defaultDBmTX = 4;

	configuration.deviceConfigOrigin = deviceConfigOrigins::RANDOM_CONFIG;
	Conf::generateRandomSerial();
	configuration.manufacturerId = 0x024D; //M-Way Solutions
	configuration.encryptionEnabled = true;
	configuration.meshNetworkKey[0] = 0x30;
	configuration.meshNetworkKey[1] = 0x28;
	configuration.meshNetworkKey[2] = 0x30;
	configuration.meshNetworkKey[3] = 0x6A;
	configuration.meshNetworkKey[4] = 0x63;
	configuration.meshNetworkKey[5] = 0x23;
	configuration.meshNetworkKey[6] = 0x3C;
	configuration.meshNetworkKey[7] = 0x78;
	configuration.meshNetworkKey[8] = 0x4F;
	configuration.meshNetworkKey[9] = 0x4C;
	configuration.meshNetworkKey[10] = 0x4D;
	configuration.meshNetworkKey[11] = 0x58;
	configuration.meshNetworkKey[12] = 0x63;
	configuration.meshNetworkKey[13] = 0x76;
	configuration.meshNetworkKey[14] = 0xA3;
	configuration.meshNetworkKey[15] = 0x15;
	configuration.meshNetworkIdentifier = 121;
	configuration.deviceType = deviceTypes::DEVICE_TYPE_STATIC;
	memset(&configuration.staticAccessAddress, 0xFF, sizeof(configuration.staticAccessAddress));
}

void Conf::LoadUicr(){
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
	configuration.deviceConfigOrigin = deviceConfigOrigins::UICR_CONFIG;

	if(NRF_UICR->CUSTOMER[1] != EMPTY_WORD) Boardconfig->boardType = NRF_UICR->CUSTOMER[1];
	if(NRF_UICR->CUSTOMER[2] != EMPTY_WORD){
		memcpy((u8*)configuration.serialNumber, (u8*)(NRF_UICR->CUSTOMER + 2), 5);
		configuration.serialNumber[5] = '\0';
	}
	if(!isEmpty((u32*)(NRF_UICR->CUSTOMER + 4), 4)){
		memcpy(&(configuration.meshNetworkKey), (u8*)(NRF_UICR->CUSTOMER + 4), 16);
	}
	if(NRF_UICR->CUSTOMER[8] != EMPTY_WORD) configuration.manufacturerId = (u16)NRF_UICR->CUSTOMER[8];
	if(NRF_UICR->CUSTOMER[9] != EMPTY_WORD) configuration.meshNetworkIdentifier = (u16)NRF_UICR->CUSTOMER[9];
	if(NRF_UICR->CUSTOMER[10] != EMPTY_WORD) configuration.defaultNodeId = (u16)NRF_UICR->CUSTOMER[10];
	if(NRF_UICR->CUSTOMER[11] != EMPTY_WORD) configuration.deviceType = (deviceTypes)NRF_UICR->CUSTOMER[11];
	if(NRF_UICR->CUSTOMER[12] != EMPTY_WORD) configuration.serialNumberIndex = (u32)NRF_UICR->CUSTOMER[12];
}

#ifdef ENABLE_TEST_DEVICES
void Conf::LoadTestDevices(){
	if (this->getTestDevice() != NULL){
		this->deviceConfigOrigin = deviceConfigOrigins::TESTDEVICE_CONFIG;

		testDevice* testDevice = this->getTestDevice();

		generateRandomSerial();

		this->defaultNodeId = testDevice->id;
		this->deviceType = testDevice->deviceType;

		memcpy(&this->staticAccessAddress, &testDevices->addr, sizeof(ble_gap_addr_t));
	}
}
#endif

int int_pow(int base, int exponent){
    int result = 1;
    while (exponent){
        if (exponent & 1) result *= base;
        exponent /= 2;
        base *= base;
    }
    return result;
}

u32 Conf::GetSettingsPageBaseAddress()
{
	bool bootloaderAvailable = (BOOTLOADER_UICR_ADDRESS != 0xFFFFFFFF);
	u32 bootloaderAddress = bootloaderAvailable ? BOOTLOADER_UICR_ADDRESS : FLASH_SIZE;
	u32 appSettingsAddress = bootloaderAddress - (RECORD_STORAGE_NUM_PAGES)* PAGE_SIZE;

	return (appSettingsAddress + FLASH_REGION_START_ADDRESS);
}

bool Conf::LoadSettingsFromFlash(moduleID moduleId, ModuleConfiguration* configurationPointer, u16 configurationLength)
{
	if(!Conf::loadConfigFromFlash){
		return false;
	}

	if (loadConfigFromFlash) {
		sizedData configData = GS->recordStorage->GetRecordData(moduleId);

		if (configData.length != 0) {
			memcpy((u8*)configurationPointer, configData.data, configurationLength);

			logt("CONFIG", "Config for moduleId %u loaded, version %d, len %u", moduleId, configurationPointer->moduleVersion, configData.length);
			return true;
		}
		else {
			logt("CONFIG", "No valid config for moduleId %d loaded", moduleId);
		}
	}
	return false;
}

bool Conf::SaveModuleSettingsToFlash(moduleID moduleId, ModuleConfiguration* configurationPointer, u16 configurationLength, RecordStorageEventListener* listener, u32 userType, u8* userData, u16 userDataLength)
{
	u32 err = GS->recordStorage->SaveRecord(moduleId, (u8*)configurationPointer, configurationLength, listener, userType, userData, userDataLength);

	return err;
}

void Conf::generateRandomSerial(){
	//Generate a random serial number
	//(removed vocals to prevent bad words, removed 0 because it could be mistaken for an o)
	const char* alphabet = "BCDFGHJKLMNPQRSTVWXYZ123456789"; //30 chars

	//This takes 5bit wide chunks from the device id to generate a serial number
	//in tests, 10k serial numbers had 4 duplicates
	u32 index = 0;
	for(int i=0; i<NODE_SERIAL_NUMBER_LENGTH; i++){
		u8 fiveBitChunk = (NRF_FICR->DEVICEID[0] & 0x1F << (i*5)) >> (i*5);
		configuration.serialNumber[NODE_SERIAL_NUMBER_LENGTH-i-1] = alphabet[fiveBitChunk % 30];
		index += int_pow(30, i)*(fiveBitChunk % 30);
	}
	configuration.serialNumber[NODE_SERIAL_NUMBER_LENGTH] = '\0';
	configuration.serialNumberIndex = index;
	configuration.defaultNodeId = index + 50;
}

//Tests if a memory region in flash storage is empty (0xFF)
bool Conf::isEmpty(u32* mem, u8 numWords){
	for(u8 i=0; i<numWords; i++){
		if(*(mem+i) != 0xFFFFFFFF) return false;
	}
	return true;
}

#ifdef ENABLE_TEST_DEVICES
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
#endif
