////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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

void SetBoardConfiguration_github_nrf52(BoardConfiguration* c)
{
    //Additional boards can be put in here to be selected at runtime
    //BoardConfiguration* c = (BoardConfiguration*)config;
    //e.g. setBoard_123(c);
}

void SetFeaturesetConfiguration_github_nrf52(ModuleConfiguration* config, void* module)
{
    if (config->moduleId == ModuleId::CONFIG)
    {
        Conf::GetInstance().defaultLedMode = LedMode::CONNECTIONS;
        Conf::GetInstance().terminalMode = TerminalMode::PROMPT;
    }
    else if (config->moduleId == ModuleId::NODE)
    {
        //Specifies a default enrollment for the github configuration
        //This enrollment will be overwritten as soon as the node is either enrolled or the enrollment removed
        NodeConfiguration* c = (NodeConfiguration*) config;
        c->enrollmentState = EnrollmentState::ENROLLED;
        c->networkId = 11;
        CheckedMemset(c->networkKey, 0x00, 16);
    }
}

void SetFeaturesetConfigurationVendor_github_nrf52(VendorModuleConfiguration* config, void* module)
{
    if (config->moduleId == VENDOR_TEMPLATE_MODULE_ID)
    {
        logt("TMOD", "Setting template module configuration for featureset");
    }
}

u32 InitializeModules_github_nrf52(bool createModule)
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

DeviceType GetDeviceType_github_nrf52()
{
    return DeviceType::STATIC;
}

Chipset GetChipset_github_nrf52()
{
    return Chipset::CHIP_NRF52;
}

FeatureSetGroup GetFeatureSetGroup_github_nrf52()
{
    return FeatureSetGroup::NRF52_MESH;
}

u32 GetWatchdogTimeout_github_nrf52()
{
    return 32768UL * 60 * 60 * 2;
}

u32 GetWatchdogTimeoutSafeBoot_github_nrf52()
{
    return 32768UL * 20UL;
}
