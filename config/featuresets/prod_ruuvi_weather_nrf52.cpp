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
#include <Logger.h>
#include <DebugModule.h>
#include <StatusReporterModule.h>
#include <BeaconingModule.h>
#include <ScanningModule.h>
#include <EnrollmentModule.h>
#include <IoModule.h>
#include <RuuviWeatherModule.h>
#include <MeshAccessModule.h>
#include <GlobalState.h>

// WARNING: If you flash this featureset on a nRF52-DK without modification
//          it will not work correctly! To use it on a nRF52-DK please remove
//          `c->boardType = 12` as it hardcodes the board type and uncomment
//          the declaration of `SetBoard_4` and the call to `SetBoard_4` in
//          `SetBoardConfiguration_prod_ruuvi_weather_nrf52`.

//extern void SetBoard_4(BoardConfiguration* c);
extern void SetBoard_12(BoardConfiguration* c);

void SetBoardConfiguration_prod_ruuvi_weather_nrf52(BoardConfiguration* c)
{
    // This sets the board type to 12 (RuuviTag). In general the boardType is
    // programmed into a field of the UICR and it is not recommended to
    // hard-code it into the firmware like this.
    c->boardType = 12;

    // Board type 4 is the nRF52-DK. The DK does not produce sensors data, but
    // incoming data is advertised.

    //SetBoard_4(c);
    SetBoard_12(c);
}

void SetFeaturesetConfiguration_prod_ruuvi_weather_nrf52(ModuleConfiguration* config, void* module)
{
    if (config->moduleId == ModuleId::CONFIG)
    {
        Conf::GetInstance().defaultLedMode = LedMode::OFF;
        Conf::GetInstance().terminalMode = TerminalMode::DISABLED;
    }
    else if (config->moduleId == ModuleId::NODE)
    {
        //Specifies a default enrollment for the github configuration
        //This is just for illustration purpose so that all nodes are enrolled and connect to each other after flashing
        //For production, all nodes should have a unique nodeKey in the UICR and should be unenrolled
        //They can then be enrolled by the user e.g. by using a smartphone application
        //More info is available as part of the documentation in the Specification and the UICR chapter
        NodeConfiguration* c = (NodeConfiguration*) config;
        //Default state will be that the node is already enrolled
        c->enrollmentState = EnrollmentState::ENROLLED;
        //Enroll the node by default in networkId 11
        c->networkId = 11;
        //Set a default network key of 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22
        CheckedMemcpy(c->networkKey, "\x22\x22\x22\x22\x22\x22\x22\x22\x22\x22\x22\x22\x22\x22\x22\x22", 16);

        //Info: The default node key and other keys are set in Conf::LoadDefaults()
    }
}

void SetFeaturesetConfigurationVendor_prod_ruuvi_weather_nrf52(VendorModuleConfiguration* config, void* module)
{
    
}

u32 InitializeModules_prod_ruuvi_weather_nrf52(bool createModule)
{
    u32 size = 0;
    size += GS->InitializeModule<DebugModule>(createModule);
    size += GS->InitializeModule<StatusReporterModule>(createModule);
    size += GS->InitializeModule<BeaconingModule>(createModule);
    size += GS->InitializeModule<ScanningModule>(createModule);
    size += GS->InitializeModule<EnrollmentModule>(createModule);
    size += GS->InitializeModule<IoModule>(createModule);
    size += GS->InitializeModule<RuuviWeatherModule>(createModule, RECORD_STORAGE_RECORD_ID_VENDOR_MODULE_CONFIG_BASE + 0);
    size += GS->InitializeModule<MeshAccessModule>(createModule);
    return size;
}

DeviceType GetDeviceType_prod_ruuvi_weather_nrf52()
{
    return DeviceType::STATIC;
}

Chipset GetChipset_prod_ruuvi_weather_nrf52()
{
    return Chipset::CHIP_NRF52;
}

FeatureSetGroup GetFeatureSetGroup_prod_ruuvi_weather_nrf52()
{
    return FeatureSetGroup::NRF52_RV_WEATHER_MESH;
}

u32 GetWatchdogTimeout_prod_ruuvi_weather_nrf52()
{
    return 32768UL * 10; //set to 0 if the watchdog should be disabled
}

u32 GetWatchdogTimeoutSafeBoot_prod_ruuvi_weather_nrf52()
{
    return 0;
}
