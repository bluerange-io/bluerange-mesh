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
#include "VendorTemplateModule.h"
#include "GlobalState.h"

// This is an example featureset for the nrf52833
// It has logging activated and is perfect for playing around with FruityMesh
// It also has a default enrollment hardcoded so that all mesh nodes are
// in the same mesh network after flashing

void SetBoardConfiguration_github_dev_nrf52833(BoardConfiguration* c)
{
    //Additional boards can be put in here to be selected at runtime
    //BoardConfiguration* c = (BoardConfiguration*)config;
    //e.g. setBoard_123(c);
}

void SetFeaturesetConfiguration_github_dev_nrf52833(ModuleConfiguration* config, void* module)
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

void SetFeaturesetConfigurationVendor_github_dev_nrf52833(VendorModuleConfiguration* config, void* module)
{
    if (config->moduleId == VENDOR_TEMPLATE_MODULE_ID)
    {
        logt("TMOD", "Setting template module configuration for featureset");
    }
}

u32 InitializeModules_github_dev_nrf52833(bool createModule)
{
    u32 size = 0;
    size += GS->InitializeModule<DebugModule>(createModule);
    size += GS->InitializeModule<StatusReporterModule>(createModule);
    size += GS->InitializeModule<BeaconingModule>(createModule);
    size += GS->InitializeModule<ScanningModule>(createModule);
    size += GS->InitializeModule<EnrollmentModule>(createModule);
    size += GS->InitializeModule<IoModule>(createModule);

    //Each Vendor module needs a RecordStorage id if it wants to store a persistent configuration
    //see the section for VendorModules in RecordStorage.h for more info
    size += GS->InitializeModule<VendorTemplateModule>(createModule, RECORD_STORAGE_RECORD_ID_VENDOR_MODULE_CONFIG_BASE + 0);

    size += GS->InitializeModule<MeshAccessModule>(createModule);
    return size;
}

DeviceType GetDeviceType_github_dev_nrf52833()
{
    return DeviceType::STATIC;
}

Chipset GetChipset_github_dev_nrf52833()
{
    return Chipset::CHIP_NRF52833;
}

FeatureSetGroup GetFeatureSetGroup_github_dev_nrf52833()
{
    return FeatureSetGroup::NRF52833_DEV_GITHUB;
}

u32 GetWatchdogTimeout_github_dev_nrf52833()
{
    return 0; //Watchdog disabled by default, activate if desired
}

u32 GetWatchdogTimeoutSafeBoot_github_dev_nrf52833()
{
    return 0; //Safe Boot Mode disabled by default, activate if desired
}
