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
#include <CherrySimTester.h>

TEST(TestIoModule, TestCommands) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    testerConfig.verbose = false;
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 9 });
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    //Could modify the default test or sim config here,...
    tester.Start();

    tester.SimulateUntilClusteringDone(1000 * 1000);
    tester.SendTerminalCommand(2, "action this io led on");
    tester.SimulateUntilMessageReceived(500, 2, "set_led_result");


    tester.SendTerminalCommand(1, "action 2 io pinset 1 2");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":2,\"type\":\"set_pin_config_result\",\"module\":6");

}

TEST(TestIoModule, TestIdentifyCommands)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    testerConfig.verbose = false;

    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 2 });
    simConfig.SetToPerfectConditions();

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.Start();
    tester.SimulateUntilClusteringDone(60 * 1000);

    {
        NodeIndexSetter nodeIndexSetter{0};
        GS->logger.EnableTag("IOMOD");
    }

    // Turn identification ON on node 1 from node 2.
    tester.SendTerminalCommand(2, "action 1 io identify on");

    // Check that identification was turned on on node 1.
    tester.SimulateUntilRegexMessageReceived(60 * 1000, 1, "identification started by SET_IDENTIFICATION message");

    // Turn identification OFF on node 1 from node 2.
    tester.SendTerminalCommand(2, "action 1 io identify off");

    // Check that identification was turned off on node 1.
    tester.SimulateUntilRegexMessageReceived(60 * 1000, 1, "identification stopped by SET_IDENTIFICATION message");
}