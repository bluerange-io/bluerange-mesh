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
#include "DebugModule.h"
#include "StatusReporterModule.h"
#include "BeaconingModule.h"
#include "ScanningModule.h"
#include "EnrollmentModule.h"
#include "IoModule.h"
#include "MeshAccessModule.h"
#include "GlobalState.h"

extern void SetBoard_18(BoardConfiguration* c);
extern void SetBoard_24(BoardConfiguration* c);

// This is an example featureset for a gateway (sink). It is meant for USB dongle (PCA10059),
// but can also be tested with nrf52840 dev kit (pca10056)
// Logging is disabled but UART communication is enabled
// It also has a default enrollment hardcoded so that all mesh nodes are
// in the same mesh network after flashing

void SetBoardConfiguration_github_sink_usb_nrf52840(BoardConfiguration* c)
{
    SetBoard_18(c);
    SetBoard_24(c);
}

void SetFeaturesetConfiguration_github_sink_usb_nrf52840(ModuleConfiguration* config, void* module)
{
    if (config->moduleId == ModuleId::CONFIG)
    {
        Conf::GetInstance().defaultLedMode = LedMode::CONNECTIONS;
        Conf::GetInstance().terminalMode = TerminalMode::JSON;
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

void SetFeaturesetConfigurationVendor_github_sink_usb_nrf52840(VendorModuleConfiguration* config, void* module)
{

}

u32 InitializeModules_github_sink_usb_nrf52840(bool createModule)
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

DeviceType GetDeviceType_github_sink_usb_nrf52840()
{
    return DeviceType::SINK;
}

Chipset GetChipset_github_sink_usb_nrf52840()
{
    return Chipset::CHIP_NRF52840;
}

FeatureSetGroup GetFeatureSetGroup_github_sink_usb_nrf52840()
{
    return FeatureSetGroup::NRF52840_SINK_USB;
}

u32 GetWatchdogTimeout_github_sink_usb_nrf52840()
{
    return 0; //Watchdog disabled by default, activate if desired
}

u32 GetWatchdogTimeoutSafeBoot_github_sink_usb_nrf52840()
{
    return 0; //Safe Boot Mode disabled by default, activate if desired
}

