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
#include "gtest/gtest.h"
#include "Utility.h"
#include "CherrySimTester.h"
#include "CherrySimUtils.h"
#include "Logger.h"
#include "IoModule.h"
#include "StatusReporterModule.h"
#include "VendorTemplateModule.h"

TEST(TestModule, TestCommands) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);

    tester.sim->FindNodeById(1)->gs.logger.EnableTag("MODULE");
    tester.sim->FindNodeById(2)->gs.logger.EnableTag("MODULE");

    //StatusReporterModule config is readable at the moment (IOT-4327)
    tester.SendTerminalCommand(1, "get_config 2 status");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"config\",\"module\":3,\"requestHandle\":0,\"config\":\"");

    tester.SendTerminalCommand(1, "set_active 2 io on");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"set_active_result\",\"module\":6,\"requestHandle\":0,\"code\":0"); // 0 = SUCCESS
    {
        NodeIndexSetter setter(1);
        ASSERT_TRUE(static_cast<IoModule*>(GS->node.GetModuleById(ModuleId::IO_MODULE))->configurationPointer->moduleActive == true);
    }
    tester.SendTerminalCommand(2, "reset");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "reboot");
    tester.SimulateUntilClusteringDone(100 * 1000);
    {
        NodeIndexSetter setter(1);
        ASSERT_TRUE(static_cast<IoModule*>(GS->node.GetModuleById(ModuleId::IO_MODULE))->configurationPointer->moduleActive == true);
    }

    tester.SendTerminalCommand(1, "set_active 2 io off");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"set_active_result\",\"module\":6,\"requestHandle\":0,\"code\":0"); // 0 = SUCCESS
    {
        NodeIndexSetter setter(1);
        ASSERT_TRUE(static_cast<IoModule*>(GS->node.GetModuleById(ModuleId::IO_MODULE))->configurationPointer->moduleActive == false);
    }

    tester.SendTerminalCommand(2, "reset");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "reboot");
    tester.SimulateUntilClusteringDone(100 * 1000);
    {
        NodeIndexSetter setter(1);
        ASSERT_TRUE(static_cast<IoModule*>(GS->node.GetModuleById(ModuleId::IO_MODULE))->configurationPointer->moduleActive == false);
    }
}

//This tests if all modules react correctly when trying to set them in/active over the mesh
TEST(TestModule, TestSetActive) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "github_dev_nrf52", 2 });
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);

    //Test setting the status reporter module to off by using its name
    tester.SendTerminalCommand(1, "set_active 2 status off");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"set_active_result\",\"module\":3,\"requestHandle\":0,\"code\":0}");

    {
        NodeIndexSetter setter(1);
        StatusReporterModule* mod = (StatusReporterModule*)GS->node.GetModuleById(ModuleId::STATUS_REPORTER_MODULE);
        ASSERT_NE(mod, nullptr);
        ASSERT_EQ(mod->configuration.moduleActive, 0);
    }

    //Test setting the status reporter module to on by using its moduleId
    tester.SendTerminalCommand(1, "set_active 2 3 on");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"set_active_result\",\"module\":3,\"requestHandle\":0,\"code\":0}");

    {
        NodeIndexSetter setter(1);
        StatusReporterModule* mod = (StatusReporterModule*)GS->node.GetModuleById(ModuleId::STATUS_REPORTER_MODULE);
        ASSERT_NE(mod, nullptr);
        ASSERT_EQ(mod->configuration.moduleActive, 1);
    }

    //Test setting a vendorModule to off by using its name
    {
        Exceptions::ExceptionDisabler<ErrorLoggedException> disabler;
        tester.SendTerminalCommand(1, "set_active 2 template off");
        //Should store that the module is now inactive
        tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"set_active_result\",\"module\":%s,\"requestHandle\":0,\"code\":0}", Utility::GetModuleIdString(VENDOR_TEMPLATE_MODULE_ID).data());
    }

    {
        NodeIndexSetter setter(1);
        VendorTemplateModule* mod = (VendorTemplateModule*)GS->node.GetModuleById(VENDOR_TEMPLATE_MODULE_ID);
        ASSERT_NE(mod, nullptr);
        ASSERT_EQ(mod->configuration.moduleActive, 0);
    }

    tester.SendTerminalCommand(2, "reset");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "reboot");
    tester.SimulateUntilClusteringDone(100 * 1000);

    //Check that the vendorModule stays off after a reset
    tester.SendTerminalCommand(2, "get_modules this");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"id\":\"0xABCD01F0\",\"version\":1,\"active\":0}");

    //Test setting a vendorModule to on by using its VendorModuleId
    {
        Exceptions::ExceptionDisabler<ErrorLoggedException> disabler;

        tester.SendTerminalCommand(1, "set_active 2 0x%08X on", VENDOR_TEMPLATE_MODULE_ID);
        //We should also get a positive response when using the module id
        tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"set_active_result\",\"module\":%s,\"requestHandle\":0,\"code\":0}", Utility::GetModuleIdString(VENDOR_TEMPLATE_MODULE_ID).data());
    }

    {
        NodeIndexSetter setter(1);
        VendorTemplateModule* mod = (VendorTemplateModule*)GS->node.GetModuleById(VENDOR_TEMPLATE_MODULE_ID);
        ASSERT_NE(mod, nullptr);
        ASSERT_EQ(mod->configuration.moduleActive, 1);
    }

    //We should not be able to modify the state of the node
    tester.SendTerminalCommand(1, "set_active 2 node off");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"set_active_result\",\"module\":%u,\"requestHandle\":0,\"code\":51}", (u32)ModuleId::NODE);

}

//Test to set the configuration for different modules over the mesh
TEST(TestModule, TestSetAndGetConfiguration) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "github_dev_nrf52", 2 });
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 60);

    StatusReporterModuleConfiguration statusConfig;
    statusConfig.moduleId = ModuleId::STATUS_REPORTER_MODULE;
    statusConfig.moduleVersion = STATUS_REPORTER_MODULE_CONFIG_VERSION;
    statusConfig.moduleActive = 1;
    statusConfig.reserved = 0;

    statusConfig.statusReportingIntervalDs = SEC_TO_DS(1);
    statusConfig.connectionReportingIntervalDs = 0;
    statusConfig.nearbyReportingIntervalDs = 0;
    statusConfig.deviceInfoReportingIntervalDs = 0;
    statusConfig.liveReportingState = LiveReportTypes::LEVEL_FATAL;

    static_assert(sizeof(StatusReporterModuleConfiguration) == 13, "Size changed from when test was written, change defaults");

    char statusConfigString[200];
    Logger::ConvertBufferToHexString((u8*)&statusConfig, sizeof(StatusReporterModuleConfiguration), statusConfigString, sizeof(statusConfigString));


    //Test setting the status reporter module to off by using its name
    tester.SendTerminalCommand(1, "set_config 2 status %s", statusConfigString);
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"set_config_result\",\"module\":3,\"requestHandle\":0,\"code\":0}");

    //Read the config back and check if it is the same
    tester.SendTerminalCommand(1, "get_config 2 status");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"config\",\"module\":3,\"requestHandle\":0,\"config\":\"03:02:01:00:00:00:0A:00:00:00:00:00:32\"}");

    VendorTemplateModuleConfiguration templateConfig;
    templateConfig.moduleId = VENDOR_TEMPLATE_MODULE_ID;
    templateConfig.moduleVersion = VENDOR_TEMPLATE_MODULE_CONFIG_VERSION;
    templateConfig.moduleActive = 1;
    templateConfig.reserved = 0;

    templateConfig.exampleValue = 123;

    static_assert(sizeof(VendorTemplateModuleConfiguration) == 9, "Size changed from when test was written, change defaults");

    char templateConfigString[200];
    Logger::ConvertBufferToHexString((u8*)&templateConfig, sizeof(VendorTemplateModuleConfiguration), templateConfigString, sizeof(templateConfigString));

    //Test if we can set the config of a vendor module
    {
        Exceptions::ExceptionDisabler<ErrorLoggedException> disabler;

        tester.SendTerminalCommand(1, "set_config 2 template %s", templateConfigString);
        //We await a positive feedback
        tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"set_config_result\",\"module\":%s,\"requestHandle\":0,\"code\":0}", Utility::GetModuleIdString(VENDOR_TEMPLATE_MODULE_ID).data());
    }

    tester.SendTerminalCommand(2, "reset");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "reboot");
    tester.SimulateUntilClusteringDone(100 * 1000);

    //Test if we can read the config back from the vendorModule
    {
        Exceptions::ExceptionDisabler<ErrorLoggedException> disabler;

        tester.SendTerminalCommand(1, "get_config 2 template");
        tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"config\",\"module\":%s,\"requestHandle\":0,\"config\":\"F0:01:CD:AB:01:01:00:00:7B\"}", Utility::GetModuleIdString(VENDOR_TEMPLATE_MODULE_ID).data());

    }

    //Make sure that the node config cannot be read
    tester.SendTerminalCommand(1, "get_config 2 node");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"get_config_error\",\"module\":%u,\"requestHandle\":0,\"code\":51}", (u32)ModuleId::NODE);

    //Make sure that the node config cannot be set
    tester.SendTerminalCommand(1, "set_config 2 node AA:BB:CC:DD:EE:FF");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"set_config_result\",\"module\":%u,\"requestHandle\":0,\"code\":51}", (u32)ModuleId::NODE);
}

TEST(TestModule, TestGetModuleList) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "github_dev_nrf52", 2 });
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);

    tester.SendTerminalCommand(1, "get_modules 2");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"module_list\",\"modules\":["
        "{\"id\":0,\"version\":2,\"active\":1},"
        "{\"id\":7,\"version\":2,\"active\":1},"
        "{\"id\":3,\"version\":2,\"active\":1},"
        "{\"id\":1,\"version\":1,\"active\":1},"
        "{\"id\":2,\"version\":2,\"active\":1},"
        "{\"id\":5,\"version\":1,\"active\":1},"
        "{\"id\":6,\"version\":1,\"active\":1},"
        "{\"id\":\"0xABCD01F0\",\"version\":1,\"active\":1},"
        "{\"id\":10,\"version\":2,\"active\":1}"
        "]}");
}

TEST(TestModule, TestConfigRemovalDuringUnenrollment) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "github_dev_nrf52", 2 });
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SendTerminalCommand(1, "action this enroll basic BBBBB 1 7 AA:BB:CC:DD:AA:BB:CC:DD:AA:BB:CC:DD:AA:BB:CC:DD");
    tester.SendTerminalCommand(2, "action this enroll basic BBBBC 2 7 AA:BB:CC:DD:AA:BB:CC:DD:AA:BB:CC:DD:AA:BB:CC:DD");

    tester.SimulateUntilClusteringDone(100 * 1000);

    //Disable TemplateModule
    tester.SendTerminalCommand(1, "set_active 2 template off");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"set_active_result\",\"module\":%s,\"requestHandle\":0,\"code\":0}", Utility::GetModuleIdString(VENDOR_TEMPLATE_MODULE_ID).data());

    //Disable StatusReporterModule
    tester.SendTerminalCommand(1, "set_active 2 status off");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"set_active_result\",\"module\":%u,\"requestHandle\":0,\"code\":0}", ModuleId::STATUS_REPORTER_MODULE);

    //Reset the node
    tester.SendTerminalCommand(2, "reset");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "reboot");
    tester.SimulateUntilClusteringDone(100 * 1000);

    //Make sure both are off
    tester.SendTerminalCommand(1, "get_modules 2");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"id\":\"0xABCD01F0\",\"version\":1,\"active\":0}");
    tester.SendTerminalCommand(1, "get_modules 2");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"id\":3,\"version\":2,\"active\":0}");

    //We send an unenrollment to the node
    tester.SendTerminalCommand(1, "action 2 enroll remove BBBBC");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "reboot");

    //As node 2 should have cleared all its settings now, both modules should be on again
    tester.SendTerminalCommand(2, "get_modules 2");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"id\":\"0xABCD01F0\",\"version\":1,\"active\":1}");
    tester.SendTerminalCommand(2, "get_modules 2");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"id\":3,\"version\":2,\"active\":1}");
}