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

#include <Config.h>
#include <Boardconfig.h>
#include <GlobalState.h>
#include <Logger.h>
#include <RecordStorage.h>
#include <Utility.h>

extern "C"{
}


#define CONFIG_CONFIG_VERSION 2

//Config.cpp initializes variables defined in Config.h with values from UICR

//Put the firmware version in a special section right after the initialization vector
#ifndef SIM_ENABLED
uint32_t fruityMeshVersion __attribute__((section(".Version"), used)) = FM_VERSION;
uint32_t appMagicNumber __attribute__((section(".AppMagicNumber"), used)) = APP_ID_MAGIC_NUMBER;
#else
uint32_t fruityMeshVersion = FM_VERSION;
#endif

bool Conf::loadConfigFromFlash;

Conf::Conf()
{
	//If firmware groupids are defined, we save them in our config
	memset(fwGroupIds, 0x00, sizeof(fwGroupIds));
#ifdef SET_FW_GROUPID_CHIPSET
	fwGroupIds[0] = SET_FW_GROUPID_CHIPSET;
#endif
#ifdef SET_FW_GROUPID_FEATURESET
	fwGroupIds[1] = SET_FW_GROUPID_FEATURESET;
#endif

}



#define _____________INITIALIZING_______________

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
	return nullptr;
}
#endif

void Conf::Initialize(bool loadConfigFromFlash)
{
	Conf::loadConfigFromFlash = loadConfigFromFlash;

	//First, fill with default Settings from the codebase
	LoadDefaults();

	//Overwrite with settings from the settings page if they exist
	if(loadConfigFromFlash){
		Utility::LoadSettingsFromFlashWithId(moduleID::CONFIG_ID, (ModuleConfiguration*)&configuration, sizeof(ConfigConfiguration));
	}

	//Check if the config is incompatible
	if(configuration.moduleVersion < CONFIG_CONFIG_VERSION){
		LoadDefaults();
	}

	//If there is UICR data available, we use it to fill uninitialized parts of the config
	LoadUicr();

	//In case we don't have UICR data, we might find data in the testDevices section
#ifdef ENABLE_TEST_DEVICES
	else if (this->getTestDevice() != nullptr){
		LoadTestDevices();
	}
#endif

	//Fix some settings
#ifdef NRF51
	if(Config->totalInConnections > 1) Config->totalInConnections = 1;
#endif

	SET_FEATURESET_CONFIGURATION(&configuration);

}

void Conf::LoadDefaults(){
	configuration.moduleId = moduleID::CONFIG_ID;
	configuration.moduleVersion = 2;
	configuration.moduleActive = true;
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
	configuration.meshMinConnectionInterval = (u16) MSEC_TO_UNITS(10, UNIT_1_25_MS);
	configuration.meshMaxConnectionInterval = (u16) MSEC_TO_UNITS(10, UNIT_1_25_MS);
	configuration.meshPeripheralSlaveLatency = 0;
	configuration.meshConnectionSupervisionTimeout = (u16) MSEC_TO_UNITS(6000, UNIT_10_MS);
	configuration.meshExtendedConnectionTimeoutSec = 10;

	configuration.meshAdvertisingIntervalHigh = (u16) MSEC_TO_UNITS(100, UNIT_0_625_MS);
	configuration.meshScanIntervalHigh = (u16) MSEC_TO_UNITS(20, UNIT_0_625_MS);
	configuration.meshScanWindowHigh = (u16) MSEC_TO_UNITS(3, UNIT_0_625_MS);

	configuration.meshAdvertisingIntervalLow = (u16) MSEC_TO_UNITS(100, UNIT_0_625_MS);
	configuration.meshScanIntervalLow = (u16) MSEC_TO_UNITS(20, UNIT_0_625_MS);
	configuration.meshScanWindowLow = (u16) MSEC_TO_UNITS(3, UNIT_0_625_MS);

	configuration.meshConnectingScanInterval = (u16) MSEC_TO_UNITS(20, UNIT_0_625_MS);
	configuration.meshConnectingScanWindow = (u16) MSEC_TO_UNITS(4, UNIT_0_625_MS);
	configuration.meshConnectingScanTimeout = 3;
	configuration.meshHandshakeTimeoutDs = SEC_TO_DS(4);
	configuration.deprecated_meshStateTimeoutHighDs = SEC_TO_DS(0);
	configuration.deprecated_meshStateTimeoutLowDs = SEC_TO_DS(10);
	configuration.deprecated_meshStateTimeoutBackOffDs = SEC_TO_DS(0);
	configuration.deprecated_meshStateTimeoutBackOffVarianceDs = SEC_TO_DS(1);
	configuration.deprecated_discoveryHighToLowTransitionDuration = 10;
	configuration.advertiseOnChannel37 = 1;
	configuration.advertiseOnChannel38 = 1;
	configuration.advertiseOnChannel39 = 1;
	configuration.meshMaxInConnections = MESH_IN_CONNECTIONS;
	configuration.meshMaxOutConnections = MESH_OUT_CONNECTIONS;
	configuration.meshMaxConnections = configuration.meshMaxInConnections + configuration.meshMaxOutConnections;

#if defined(NRF51) || defined(SIM_ENABLED)
	configuration.totalInConnections = 1;
#elif NRF52
	//Allow two incoming connections by default
	configuration.totalInConnections = 2;
#endif
	configuration.totalOutConnections = MESH_OUT_CONNECTIONS;
	configuration.gapEventLength = 3;
	configuration.enableRadioNotificationHandler = false;
	configuration.enableConnectionRSSIMeasurement = true;

	configuration.defaultDBmTX = 4;
	configuration.encryptionEnabled = true;

	configuration.numNodesForDecision = 4;
	configuration.maxTimeUntilDecisionDs = SEC_TO_DS(2);
	configuration.highToLowDiscoveryTimeSec = 0;

	//Set defaults for stuff that is loaded from UICR in case that no UICR data is present
	manufacturerId = 0x024D; //M-Way Solutions
	Conf::generateRandomSerialAndNodeId();
	memset(nodeKey, 0x00, 16);
	defaultNetworkId = 0;
	memset(defaultNetworkKey, 0x00, 16);
	memset(defaultUserBaseKey, 0x00, 16);
	deviceType = deviceTypes::DEVICE_TYPE_STATIC;
	memset(&staticAccessAddress, 0xFF, sizeof(staticAccessAddress));
}

void Conf::LoadUicr(){
	u32* uicrData = getUicrDataPtr();

	//If UICR data is available, we fill various variables with the data
	if(uicrData != nullptr){
		/* If we write data to NRF_UICR->CUSTOMER, it will be used by fruitymesh
		 * [0] MAGIC_NUMBER, must be set to 0xF07700 when UICR data is available
		 * [1] BOARD_TYPE, accepts an integer that defines the hardware board that fruitymesh should be running on
		 * [2] SERIAL_NUMBER, the given serial number (2 words)
		 * [4] NODE_KEY, randomly generated (4 words)
		 * [8] MANUFACTURER_ID, set to manufacturer id according to the BLE company identifiers: https://www.bluetooth.org/en-us/specification/assigned-numbers/company-identifiers
		 * [9] DEFAULT_NETWORK_ID, network id if preenrollment should be used
		 * [10] DEFAULT_NODE_ID, node id to be used if not enrolled
		 * [11] DEVICE_TYPE, type of device (sink, mobile, etc,..)
		 * [12] SERIAL_NUMBER_INDEX, unique index that represents the serial number
		 * [13] NETWORK_KEY, default network key if preenrollment should be used (4 words)
		 * [17] ...
		 */

		//If magic number exists, fill Config with valid data from UICR
		deviceConfigOrigin = deviceConfigOrigins::UICR_CONFIG;

		//=> uicrData[1] was already read in the BoardConfig class

		if(uicrData[2] != EMPTY_WORD){
			memcpy((u8*)serialNumber, (u8*)(uicrData + 2), 5);
			serialNumber[5] = '\0';
		}
		if(!isEmpty((u8*)(uicrData + 4), 16)){
			memcpy(&(nodeKey), (u8*)(uicrData + 4), 16);
		}
		if(uicrData[8] != EMPTY_WORD) manufacturerId = (u16)uicrData[8];
		if(uicrData[9] != EMPTY_WORD) defaultNetworkId = (u16)uicrData[9];
		if(uicrData[10] != EMPTY_WORD) defaultNodeId = (u16)uicrData[10];
		if(uicrData[11] != EMPTY_WORD) deviceType = (deviceTypes)uicrData[11];
		if(uicrData[12] != EMPTY_WORD) serialNumberIndex = (u32)uicrData[12];

		//If no network key is present in UICR but a node key is present, use the node key for both (to migrate settings for old nodes)
		if(isEmpty((u8*)(uicrData + 13), 16) && !isEmpty((u8*)nodeKey, 16)){
			memcpy(defaultNetworkKey, nodeKey, 16);
		} else {
			//Otherwise, we use the default network key
			memcpy(defaultNetworkKey, (u8*)(uicrData + 13), 16);
		}

		//Hacky migration for fixing meshing problems
		if(configuration.meshConnectingScanTimeout < 3) configuration.meshConnectingScanTimeout = 3;

	}
}

#ifdef ENABLE_TEST_DEVICES
void Conf::LoadTestDevices(){
	if (this->getTestDevice() != nullptr){
		this->deviceConfigOrigin = deviceConfigOrigins::TESTDEVICE_CONFIG;

		testDevice* testDevice = this->getTestDevice();

		generateRandomSerialAndNodeId();

		this->defaultNodeId = testDevice->id;
		this->deviceType = testDevice->deviceType;

		memcpy(&this->staticAccessAddress, &testDevices->addr, sizeof(ble_gap_addr_t));
	}
}
#endif


#define _____________HELPERS_______________

u32* getUicrDataPtr()
{
	//We are using a magic number to determine if the UICR data present was put there by fruitydeploy
	if (NRF_UICR->CUSTOMER[0] == UICR_SETTINGS_MAGIC_WORD) {
		return (u32*)NRF_UICR->CUSTOMER;
	}
	else {
		//On some devices, we are not able to store data in UICR as they are flashed by a 3rd party
		//and we are only updating to fruitymesh. We have a dedicated record for these instances
		//which is used the same as if the data were stored in UICR
		sizedData data = GS->recordStorage->GetRecordData(RECORD_STORAGE_RECORD_ID_UICR_REPLACEMENT);
		if (data.length >= 16 * 4 && ((u32*)data.data)[0] == UICR_SETTINGS_MAGIC_WORD) {
			return (u32*)data.data;
		}
	}

	return nullptr;
}

int int_pow(int base, int exponent){
    int result = 1;
    while (exponent){
        if (exponent & 1) result *= base;
        exponent /= 2;
        base *= base;
    }
    return result;
}

void Conf::generateRandomSerialAndNodeId(){
	//Generate a random serial number
	//(removed vocals to prevent bad words, removed 0 because it could be mistaken for an o)
	const char* alphabet = "BCDFGHJKLMNPQRSTVWXYZ123456789"; //30 chars

	//This takes 5bit wide chunks from the device id to generate a serial number
	//in tests, 10k serial numbers had 4 duplicates
	u32 index = 0;
	for(int i=0; i<NODE_SERIAL_NUMBER_LENGTH; i++){
		u8 fiveBitChunk = (NRF_FICR->DEVICEID[0] & 0x1F << (i*5)) >> (i*5);
		serialNumber[NODE_SERIAL_NUMBER_LENGTH-i-1] = alphabet[fiveBitChunk % 30];
		index += int_pow(30, i)*(fiveBitChunk % 30);
	}
	serialNumber[NODE_SERIAL_NUMBER_LENGTH] = '\0';
	serialNumberIndex = index;
	defaultNodeId = (index + 50) % (NODE_ID_GROUP_BASE-1); //nodeId must stay within valid range
}

//Tests if a memory region in flash storage is empty (0xFF)
bool Conf::isEmpty(const u8* mem, u16 numBytes) const{
	for(u32 i=0; i<numBytes; i++){
		if(mem[i] != 0xFF) return false;
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
	- NodeId: Enter the desired NodeId here (the last 3 digits of the segger id for example)
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
NodeId Conf::testColourIDs[NUM_TEST_COLOUR_IDS] = {
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
