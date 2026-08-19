////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// ** BlueRange Mesh – Community Edition (CE)
// ** Copyright (c) 2015-2021 MWAY DIGITAL GmbH, Germany
// ** Copyright (c) 2021-2026 BlueRange GmbH, Germany
// **
// ** This file is part of BlueRange Mesh Community Edition (formerly known as
// ** FruityMesh).
// **
// ** BlueRange Mesh Community Edition is free software: you can redistribute it
// ** and/or modify it under the terms of the GNU General Public License as
// ** published by the Free Software Foundation, either version 3 of the
// ** License, or (at your option) any later version.
// **
// ** BlueRange Mesh Community Edition is distributed in the hope that it will
// ** be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// ** See the GNU General Public License for more details.
// **
// ** You should have received a copy of the GNU General Public License along
// ** with this program. If not, see https://www.gnu.org/licenses/.
// **
// ** IMPORTANT:
// ** Any modification, extension, or derivative work of this file MUST also be
// ** licensed under the GNU General Public License v3 or later and the complete
// ** corresponding source code MUST be made available.
// **
// ** Commercial Use:
// ** If you wish to use this software without the obligations of the GPLv3
// ** (including source code disclosure), a commercial license for
// ** BlueRange Mesh OEM Edition is required.
// **
// ** License violations automatically terminate your rights under this license
// ** and may result in legal action under applicable law.
// ** For further information please use the contact form at:
// ** https://bluerange.io/en/contact
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////
#include "Config.h"
#include "Node.h"
#include "Utility.h"
#include "DebugModule.h"
#include "StatusReporterModule.h"
#include "BeaconingModule.h"
#include "ScanningModule.h"
#include "EnrollmentModule.h"
#include "IoModule.h"
#include "MeshAccessModule.h"
#include "GlobalState.h"

// This is an example featureset for a mesh node based on the nRF52832
// Logging is disabled
// It also has a default enrollment hardcoded so that all mesh nodes are
// in the same mesh network after flashing

void SetBoardConfiguration_github_mesh_nrf52832_sdk17(BoardConfiguration* c)
{
    //Additional boards can be put in here to be selected at runtime
    //BoardConfiguration* c = (BoardConfiguration*)config;
    //e.g. setBoard_123(c);
}

void SetFeaturesetConfiguration_github_mesh_nrf52832_sdk17(ModuleConfiguration* config, void* module)
{
    if (config->moduleId == ModuleId::CONFIG)
    {
        Conf::GetInstance().defaultLedMode = LedMode::CONNECTIONS;
        Conf::GetInstance().terminalMode = TerminalMode::PROMPT;
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

void SetFeaturesetConfigurationVendor_github_mesh_nrf52832_sdk17(VendorModuleConfiguration* config, void* module)
{

}

u32 InitializeModules_github_mesh_nrf52832_sdk17(bool createModule)
{
    u32 size = 0;
    size += GS->InitializeModule<DebugModule>(createModule);
    size += GS->InitializeModule<StatusReporterModule>(createModule);
    size += GS->InitializeModule<BeaconingModule>(createModule);
    size += GS->InitializeModule<ScanningModule>(createModule);
    size += GS->InitializeModule<EnrollmentModule>(createModule);
    size += GS->InitializeModule<IoModule>(createModule);
    size += GS->InitializeModule<MeshAccessModule>(createModule);
    return size;
}

DeviceType GetDeviceType_github_mesh_nrf52832_sdk17()
{
    return DeviceType::STATIC;
}

Chipset GetChipset_github_mesh_nrf52832_sdk17()
{
    return Chipset::CHIP_NRF52;
}

FeatureSetGroup GetFeatureSetGroup_github_mesh_nrf52832_sdk17()
{
    return FeatureSetGroup::NRF52_MESH_GITHUB_SDK17;
}

u32 GetWatchdogTimeout_github_mesh_nrf52832_sdk17()
{
    return 0; //Watchdog disabled by default, activate if desired
}

u32 GetWatchdogTimeoutSafeBoot_github_mesh_nrf52832_sdk17()
{
    return 0; //Safe Boot Mode disabled by default, activate if desired
}
LicenseState GetLicenseState_github_mesh_nrf52832_sdk17()
{
    return LicenseState::VALID_BUT_NOT_AVAILABLE;
}
