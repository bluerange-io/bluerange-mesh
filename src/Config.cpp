////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2021 M-Way Solutions GmbH
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
#include <GlobalState.h>
#include <Logger.h>
#include <RecordStorage.h>
#include <Utility.h>

#ifdef SIM_ENABLED
#include "CherrySim.h"
#endif

#define CONFIG_CONFIG_VERSION 2

//Config.cpp initializes variables defined in Config.h with values from UICR

//Put the firmware version in a special section right after the initialization vector
#ifndef SIM_ENABLED
u32 fruityMeshVersion __attribute__((section(".Version"), used)) = FM_VERSION;
u32 appMagicNumber __attribute__((section(".AppMagicNumber"), used)) = APP_ID_MAGIC_NUMBER;
#else
u32 fruityMeshVersion = FM_VERSION;
#endif

Conf::Conf()
{
    CheckedMemset(_serialNumber, 0, sizeof(_serialNumber));
    CheckedMemset(fwGroupIds, 0, sizeof(fwGroupIds));
    CheckedMemset(defaultNetworkKey, 0, sizeof(defaultNetworkKey));
    CheckedMemset(defaultUserBaseKey, 0, sizeof(defaultUserBaseKey));
    CheckedMemset(&staticAccessAddress, 0, sizeof(staticAccessAddress));

    terminalMode = TerminalMode::DISABLED;
}



#define _____________INITIALIZING_______________

void Conf::RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8 * userData, u16 userDataLength)
{
    if (userType == (u32)RecordTypeConf::SET_SERIAL)
    {
        if (resultCode == RecordStorageResultCode::SUCCESS)
        {
            GS->node.Reboot(SEC_TO_DS(1), RebootReason::SET_SERIAL_SUCCESS);
        }
        else
        {
            //Rebooting in this case is the safest bet. The initialization sequence will just restart by the other side.
            GS->node.Reboot(SEC_TO_DS(1), RebootReason::SET_SERIAL_FAILED);
        }
    }
}

void Conf::Initialize(bool safeBootEnabled)
{
    this->safeBootEnabled = safeBootEnabled;

    fwGroupIds[0] = (NodeId)GET_CHIPSET();
    fwGroupIds[1] = (NodeId)GET_FEATURE_SET_GROUP();

    //First, fill with default Settings from the codebase
    LoadDefaults();

    //If there is UICR data available, we use it to fill uninitialized parts of the config
    LoadDeviceConfiguration();

    //Overwrite with settings from flash if sth. was stored previously
    if (!safeBootEnabled) {
        SizedData configData = GS->recordStorage.GetRecordData((u16)ModuleId::CONFIG);
        ModuleConfiguration* newConfig = (ModuleConfiguration*)configData.data;

        if (
            configData.length >= SIZEOF_MODULE_CONFIGURATION_HEADER
            && configData.length <= sizeof(ConfigConfiguration)
            && newConfig->moduleVersion == configuration.moduleVersion
            && newConfig->moduleId == configuration.moduleId
        ){
            CheckedMemcpy((ModuleConfiguration*)&configuration, configData.data, configData.length.GetRaw());

            logt("CONFIG", "Config loaded from flash");
        }
    }

    SET_FEATURESET_CONFIGURATION(&configuration, this);
}

void Conf::LoadDefaults(){
    configuration.moduleId = ModuleId::CONFIG;
    configuration.moduleVersion = 4;
    configuration.moduleActive = true;
    configuration.reserved = sizeof(ConfigConfiguration);
    configuration.isSerialNumberIndexOverwritten = false;
    configuration.overwrittenSerialNumberIndex = 0;

    CheckedMemset(configuration.preferredPartnerIds, 0, sizeof(configuration.preferredPartnerIds));
    configuration.preferredConnectionMode = PreferredConnectionMode::PENALTY;
    configuration.amountOfPreferredPartnerIds = 0;

    terminalMode = TerminalMode::JSON;

    enableSinkRouting = true;
    //Check if the BLE stack supports the number of connections and correct if not
#ifdef SIM_ENABLED
    totalInConnections = 3;
    meshMaxInConnections = 2;
#endif

    meshMinConnectionInterval = (u16)MSEC_TO_UNITS(15, CONFIG_UNIT_1_25_MS);
    meshMaxConnectionInterval = (u16)MSEC_TO_UNITS(15, CONFIG_UNIT_1_25_MS);

#if IS_ACTIVE(CONN_PARAM_UPDATE)
    // Initialize the long term connection intervals to the same values as the
    // 'normal' intervals.
    meshMinLongTermConnectionInterval = meshMinConnectionInterval;
    meshMaxLongTermConnectionInterval = meshMaxConnectionInterval;
#endif

    meshScanIntervalHigh = 120; //FIXME_HAL: 120 units = 75ms (0.625ms steps)
    meshScanWindowHigh = 12; //FIXME_HAL: 12 units = 7.5ms (0.625ms steps)

    meshScanIntervalLow = (u16)MSEC_TO_UNITS(250, CONFIG_UNIT_0_625_MS);
    meshScanWindowLow = (u16)MSEC_TO_UNITS(3, CONFIG_UNIT_0_625_MS);

    highDiscoveryTimeoutSec = 0;

    //Set defaults for stuff that is loaded from UICR in case that no UICR data is present
    //This is just for testing, production nodes should always use UICR data
    //For more info, check the UICR section in the Specification chapter of our online documentation
    manufacturerId = MANUFACTURER_ID;
    Conf::GenerateRandomSerialAndNodeId(); //Generates a random serial number depending on the unique chipId for testing
    CheckedMemset(configuration.nodeKey, 0x11, 16); //NodeKey is set to a default of 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11
    defaultNetworkId = 0; //A node is unenrolled by default
    CheckedMemset(defaultNetworkKey, 0xFF, 16); //NetworkKey is set to a default of FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF
    CheckedMemset(defaultUserBaseKey, 0xFF, 16); //UserBaseKey is set to a default of FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF
    CheckedMemset(&staticAccessAddress.addr, 0xFF, 6); //The BLE Address is empty by default, so each chip uses its preprogrammed address
    staticAccessAddress.addr_type = FruityHal::BleGapAddrType::INVALID;
}

void Conf::LoadDeviceConfiguration(){
    DeviceConfiguration config;
    ErrorType err = FruityHal::GetDeviceConfiguration(config);

    //If Deviceconfiguration data is available, we fill various variables with the data
    if (err == ErrorType::SUCCESS) {
        //If magic number exists, fill Config with valid data from UICR
        deviceConfigOrigin = DeviceConfigOrigins::UICR_CONFIG;

        if(!IsEmpty((u8*)config.nodeKey, 16)){
            CheckedMemcpy(configuration.nodeKey, (u8*)config.nodeKey, 16);
        }
        if(config.manufacturerId != EMPTY_WORD) manufacturerId = (u16)config.manufacturerId;
        if(config.defaultNetworkId != EMPTY_WORD) defaultNetworkId = (u16)config.defaultNetworkId;
        if(config.defaultNodeId != EMPTY_WORD) defaultNodeId = (u16)config.defaultNodeId;
        // if(config.deviceType != EMPTY_WORD) deviceType = (deviceTypes)config.deviceType; //deprectated as of 02.07.2019
        if(config.serialNumberIndex != EMPTY_WORD) serialNumberIndex = (u32)config.serialNumberIndex;
        else if (config.serialNumber[0] != EMPTY_WORD) {
            //Legacy uicr serial number support. Might be removed some day.
            //If you want to remove it, check if any flashed device exist 
            //and is still in use, that was not flashed with DeviceConfiguration.serialNumberIndex.
            //If AND ONLY IF this is not the case, you can savely remove it.
            //WARNING: To not introduce any compatibility issues with older hardware, this does only support the old 5-character serial numbers
            //The UICR must not contain a serialNumber (should be FFFFFF...) if the serialNumber has more than 5 characters
            char serialNumber[6];
            CheckedMemcpy((u8*)serialNumber, (u8*)config.serialNumber, 5);
            serialNumber[5] = '\0';
            serialNumberIndex = Utility::GetIndexForSerial(serialNumber);
        }

        // If no network key is present in UICR but a node key is present, use the node key for both (to migrate settings for old nodes)
        if(IsEmpty((u8*)config.networkKey, 16) && !IsEmpty(configuration.nodeKey, 16)){
            CheckedMemcpy(defaultNetworkKey, configuration.nodeKey, 16);
        } else {
            //Otherwise, we use the default network key
            CheckedMemcpy(defaultNetworkKey, (u8*)config.networkKey, 16);
        }
    }
}

u32 Conf::GetFruityMeshVersion() const
{
#ifdef SIM_ENABLED
    if (cherrySimInstance->currentNode->fakeDfuVersion != 0 && cherrySimInstance->currentNode->fakeDfuVersionArmed == true) {
        return cherrySimInstance->currentNode->fakeDfuVersion;
    }
#endif
    return fruityMeshVersion;
}


#define _____________HELPERS_______________

Conf & Conf::GetInstance()
{
    return GS->config;
}

void Conf::LoadSettingsFromFlash(Module* module, u16 recordId, u8* configurationPointer, u16 configurationLength)
{
    if (module == nullptr || recordId == RECORD_STORAGE_RECORD_ID_INVALID) return;

    if (!safeBootEnabled) {
        SizedData configData = GS->recordStorage.GetRecordData((u16)recordId);

        ModuleConfiguration* moduleConfig = (ModuleConfiguration*)configData.data;
        VendorModuleConfiguration* vendorModuleConfig = (VendorModuleConfiguration*)configData.data;

        //Check if configuration exists and is valid then copy the configuration and call the handler
        if (
            //Configuration for core modules must have a minimum size and must not be bigger than the current configuration and the config version must match
            (
                !Utility::IsVendorModuleId(module->moduleId)
                && configData.length >= SIZEOF_MODULE_CONFIGURATION_HEADER
                && configData.length <= configurationLength
                && moduleConfig->moduleVersion == ((ModuleConfiguration*)configurationPointer)->moduleVersion
                && moduleConfig->moduleId == ((ModuleConfiguration*)configurationPointer)->moduleId
            )
            //Same applies to vendor modules
            || (
                Utility::IsVendorModuleId(module->vendorModuleId)
                && configData.length >= SIZEOF_VENDOR_MODULE_CONFIGURATION_HEADER
                && configData.length <= configurationLength
                && vendorModuleConfig->moduleVersion == ((VendorModuleConfiguration*)configurationPointer)->moduleVersion
                && vendorModuleConfig->moduleId == ((VendorModuleConfiguration*)configurationPointer)->moduleId
            )
        ){
            CheckedMemcpy(configurationPointer, configData.data, configData.length.GetRaw());

            logt("CONFIG", "Config for module %s loaded from record %u", Utility::GetModuleIdString(module->vendorModuleId, false).data(), recordId);

            module->ConfigurationLoadedHandler(nullptr, 0);
        }
        //If the configuration has a different version, we call the migration if it exists
        else if(
            (!Utility::IsVendorModuleId(module->moduleId) && configData.length >= SIZEOF_MODULE_CONFIGURATION_HEADER)
            || (Utility::IsVendorModuleId(module->vendorModuleId) && configData.length >= SIZEOF_VENDOR_MODULE_CONFIGURATION_HEADER)
        ){
            logt("CONFIG", "Flash config for module %s has mismatching version", Utility::GetModuleIdString(module->vendorModuleId, false).data());

            module->ConfigurationLoadedHandler(configData.data, configData.length.GetRaw());
        }
        else {
            logt("CONFIG", "No flash config for module %s found, using defaults", Utility::GetModuleIdString(module->vendorModuleId, false).data());

            module->ConfigurationLoadedHandler(nullptr, 0);
        }
    } else {
        module->ConfigurationLoadedHandler(nullptr, 0);
    }
}

uint32_t UintPow(uint32_t base, uint32_t exponent){
    uint32_t result = 1;
    while (exponent){
        if (exponent & 1) result *= base;
        exponent /= 2;
        base *= base;
    }
    return result;
}

void Conf::GenerateRandomSerialAndNodeId(){
    //Generate a random serial number for testing from the open source testing range (FMBBB - FM999)
    serialNumberIndex = (FruityHal::GetDeviceId() % (SERIAL_NUMBER_FM_TESTING_RANGE_END - SERIAL_NUMBER_FM_TESTING_RANGE_START)) + SERIAL_NUMBER_FM_TESTING_RANGE_START;

    defaultNodeId = serialNumberIndex % NODE_ID_DEVICE_BASE_SIZE; //nodeId must stay within valid range
}

//Tests if a memory region in flash storage is empty (0xFF)
bool Conf::IsEmpty(const u8* mem, u16 numBytes) const{
    for(u32 i=0; i<numBytes; i++){
        if(mem[i] != 0xFF) return false;
    }
    return true;
}

u32 Conf::GetSerialNumberIndex() const
{
    if (configuration.isSerialNumberIndexOverwritten) {
        return configuration.overwrittenSerialNumberIndex;
    }
    else {
        return serialNumberIndex;
    }
}

const char * Conf::GetSerialNumber() const
{
    Utility::GenerateBeaconSerialForIndex(GetSerialNumberIndex(), _serialNumber);
    return _serialNumber;
}

void Conf::SetSerialNumberIndex(u32 serialNumber)
{
    if (serialNumber == INVALID_SERIAL_NUMBER_INDEX)
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
    }

    //Already has this serial number
    if (serialNumber == configuration.overwrittenSerialNumberIndex && configuration.isSerialNumberIndexOverwritten) return;

    configuration.overwrittenSerialNumberIndex = serialNumber;
    configuration.isSerialNumberIndexOverwritten = true;

    RecordStorageResultCode err = SaveConfigToFlash(this, (u32)RecordTypeConf::SET_SERIAL, nullptr, 0);
    if (err != RecordStorageResultCode::SUCCESS)
    {
        //Rebooting in this case is the safest bet. The initialization sequence will just restart by the other side.
        GS->node.Reboot(SEC_TO_DS(1), RebootReason::SET_SERIAL_FAILED);
    }
}

const u8 * Conf::GetNodeKey() const
{
    return configuration.nodeKey;
}

void Conf::GetRestrainedKey(u8* buffer) const
{
    Aes128Block key;
    CheckedMemcpy(key.data, GetNodeKey(), 16);

    Aes128Block messageBlock;
    CheckedMemcpy(messageBlock.data, RESTRAINED_KEY_CLEAR_TEXT, 16);

    Aes128Block restrainedKeyBlock;
    Utility::Aes128BlockEncrypt(&messageBlock, &key, &restrainedKeyBlock);

    CheckedMemcpy(buffer, restrainedKeyBlock.data, 16);
}

void Conf::SetNodeKey(const u8 * key)
{
    CheckedMemcpy(configuration.nodeKey, key, 16);

    SaveConfigToFlash(nullptr, 0, nullptr, 0);
}

RecordStorageResultCode Conf::SaveConfigToFlash(RecordStorageEventListener* listener, u32 userType, u8* userData, u16 userDataLength)
{
    return GS->recordStorage.SaveRecord((u16)ModuleId::CONFIG, (u8*)&configuration, sizeof(ConfigConfiguration), listener, userType, userData, userDataLength);
}
