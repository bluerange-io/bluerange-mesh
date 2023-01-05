////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2022 M-Way Solutions GmbH
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

#include <HelperFunctions.h>
#include <CherrySimTester.h>

TEST(TestIoModule, TestCommands) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    //testerConfig.verbose = false;
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
    //testerConfig.verbose = false;

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

#if defined(PROD_SINK_NRF52) && defined(PROD_ASSET_NRF52)
TEST(TestIoModule, TestLedOnAssetOverMeshAccessSerialConnectWithOrgaKey)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose               = true;
    SimConfiguration simConfig    = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId          = 0;
    simConfig.defaultNetworkId    = 0;
    simConfig.preDefinedPositions = {{0.1, 0.1}, {0.2, 0.1}, {0.3, 0.1}};
    simConfig.nodeConfigName.insert({"prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({"prod_mesh_nrf52", 1});
    simConfig.nodeConfigName.insert({"prod_asset_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateGivenNumberOfSteps(10);

    tester.SendTerminalCommand(
        1, "action this enroll basic BBBBB 1 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 "
           "22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 "
           "01:00:00:00:01:00:00:00:01:00:00:00:01:00:00:00 10 0 0");
    tester.SendTerminalCommand(
        2, "action this enroll basic BBBBC 2 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 "
           "22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 "
           "02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00 10 0 0");
    tester.SendTerminalCommand(
        3, "action this enroll basic BBBBD 33000 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 "
           "22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 "
           "03:00:00:00:03:00:00:00:03:00:00:00:03:00:00:00 10 0 0");

    tester.SimulateUntilMessageReceived(100 * 1000, 1, "clusterSize\":2"); // Wait until the nodes have clustered.

    RetryOrFail<TimeoutException>(
        32, [&] {
            // Initiate a connection from the sink on the mesh node to the asset tag using the organization key.
            tester.SendTerminalCommand(
                1, "action 2 ma serial_connect BBBBD 4 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 33000 20 13");
        }, [&] {
            tester.SimulateUntilMessageReceived(10 * 1000, 1,
                                                R"({"type":"serial_connect_response","module":10,"nodeId":2,)"
                                                R"("requestHandle":13,"code":0,"partnerId":33000})");
        });

    // Send a 'led on' from the sink to the asset tag using it's partner id.
    tester.SendTerminalCommand(1, "action 33000 io led on 55");
    // Wait for the response on the sink.
    tester.SimulateUntilMessageReceived(
        10 * 1000, 1, R"({"nodeId":33000,"type":"set_led_result","module":6,"requestHandle":55,"code":0})");
}
#endif
