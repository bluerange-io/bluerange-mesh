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
#include "gtest/gtest.h"
#include "Utility.h"
#include "CherrySimTester.h"
#include "CherrySimUtils.h"
#include "Logger.h"
#include "IoModule.h"

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

    tester.sim->findNodeById(1)->gs.logger.enableTag("MODULE");
    tester.sim->findNodeById(2)->gs.logger.enableTag("MODULE");

    //tester.SendTerminalCommand(1, "set_config 2 TODO TODO");//TODO
    //tester.SimulateUntilMessageReceived(10 * 1000, 1, "RSSI measurement started for connection");
    tester.SendTerminalCommand(1, "get_config 2 node");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"config\",\"module\":0,\"config\":\"");

    tester.SendTerminalCommand(1, "set_active 2 io on");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"set_active_result\",\"module\":6,\"requestHandle\":0,\"code\":0"); // 0 = SUCCESS
    {
        NodeIndexSetter setter(1);
        ASSERT_TRUE(static_cast<IoModule*>(GS->node.GetModuleById(ModuleId::IO_MODULE))->configurationPointer->moduleActive == true);
    }
    tester.SendTerminalCommand(2, "reset");
    tester.SimulateForGivenTime(20 * 1000);
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
    tester.SimulateForGivenTime(20 * 1000);
    tester.SimulateUntilClusteringDone(100 * 1000);
    {
        NodeIndexSetter setter(1);
        ASSERT_TRUE(static_cast<IoModule*>(GS->node.GetModuleById(ModuleId::IO_MODULE))->configurationPointer->moduleActive == false);
    }
}
