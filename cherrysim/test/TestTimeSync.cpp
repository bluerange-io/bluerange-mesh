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
#include <fstream>
#include <CherrySimTester.h>
#include <Logger.h>
#include <Utility.h>
#include <string>


TEST(TestTimeSync, TestTimeSync) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 9});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(1000 * 1000);

    //Test that all connections are unsynced
    for (u32 i = 1; i <= tester.sim->GetTotalNodes(); i++) {
        tester.SendTerminalCommand(i, "status");
        tester.SimulateUntilMessageReceived(100 * 1000, i, "tSync:0");
    }

    //Set the time of node 1. This node will then start propagating the time through the mesh.
    tester.SendTerminalCommand(1, "settime 1560262597 0");
    tester.SimulateForGivenTime(60 * 1000);

    //Test that all connections are synced
    for (u32 i = 1; i <= tester.sim->GetTotalNodes(); i++) {
        tester.SendTerminalCommand(i, "status");
        tester.SimulateUntilMessageReceived(1000, i, "tSync:2");
    }

    for (u32 i = 1; i <= tester.sim->GetTotalNodes(); i++) {
        tester.SendTerminalCommand(i, "gettime");
        tester.SimulateUntilMessageReceived(100 * 1000, i, "Time is currently approx. 2019 years");
    }

    //Check that time is running and simulate 10 seconds of passing time...
    NodeIndexSetter setter(0);
    auto start = tester.sim->currentNode->gs.timeManager.GetLocalTime();
    tester.SimulateForGivenTime(10 * 1000);

    //... and check that the times have been updated correctly
    auto end = tester.sim->currentNode->gs.timeManager.GetLocalTime();
    int diff = end - start;
    
    ASSERT_TRUE(diff >= 9 && diff <= 11);


    //Reset most of the nodes (not node 1)
    for (u32 i = 2; i <= tester.sim->GetTotalNodes(); i++)
    {
        tester.SendTerminalCommand(1, "action %d node reset", i);
        tester.SimulateGivenNumberOfSteps(1);
    }

    //Give the nodes time to reset
    tester.SimulateForGivenTime(15 * 1000);
    //Wait until they clustered
    tester.SimulateUntilClusteringDone(1000 * 1000);
    //Wait until they have synced their time
    tester.SimulateForGivenTime(60 * 1000);

    //Make sure that the reset worked.
    for (u32 i = 2; i <= tester.sim->GetTotalNodes(); i++)
    {
        ASSERT_EQ(tester.sim->nodes[i - 1].restartCounter, 2);
    }

    //Check that the time has been correctly sent to each node again.
    for (u32 i = 1; i <= tester.sim->GetTotalNodes(); i++) {
        tester.SendTerminalCommand(i, "gettime");
        tester.SimulateUntilMessageReceived(100 * 1000, i, "Time is currently approx. 2019 years");
    }

    //Set the time again, to some higher value (also checks for year 2038 problem)
    tester.SendTerminalCommand(1, "settime 2960262597 0");
    tester.SimulateForGivenTime(60 * 1000);

    for (u32 i = 1; i <= tester.sim->GetTotalNodes(); i++) {
        tester.SendTerminalCommand(i, "gettime");
        tester.SimulateUntilMessageReceived(100 * 1000, i, "Time is currently approx. 2063 years");
    }

    //... and to some lower value
    tester.SendTerminalCommand(1, "settime 1260263424 0");

    tester.SimulateForGivenTime(60 * 1000);

    for (u32 i = 1; i <= tester.sim->GetTotalNodes(); i++) {
        tester.SendTerminalCommand(i, "gettime");
        tester.SimulateUntilMessageReceived(100 * 1000, i, "Time is currently approx. 2009 years");
    }

    // Test positive offset...
    tester.SendTerminalCommand(1, "settime 7200 60");
    tester.SimulateForGivenTime(60 * 1000);

    for (u32 i = 1; i <= tester.sim->GetTotalNodes(); i++) {
        tester.SendTerminalCommand(i, "gettime");
        tester.SimulateUntilMessageReceived(100 * 1000, i, "Time is currently approx. 1970 years, 1 days, 03h");
    }

    // ... and negative offset
    tester.SendTerminalCommand(1, "settime 7200 -60");
    tester.SimulateForGivenTime(60 * 1000);

    for (u32 i = 1; i <= tester.sim->GetTotalNodes(); i++) {
        tester.SendTerminalCommand(i, "gettime");
        tester.SimulateUntilMessageReceived(100 * 1000, i, "Time is currently approx. 1970 years, 1 days, 01h");
    }

    // Test edge case where the offset is larger than the time itself. In such a case the offset should be ignored.
    tester.SendTerminalCommand(1, "settime 7200 -10000");
    tester.SimulateForGivenTime(60 * 1000);

    for (u32 i = 1; i <= tester.sim->GetTotalNodes(); i++) {
        tester.SendTerminalCommand(i, "gettime");
        tester.SimulateUntilMessageReceived(100 * 1000, i, "Negative Offset (-10000) smaller than timestamp (72");
    }
}

TEST(TestTimeSync, TestTimeSyncDuration_long) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 49});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);

    tester.SendTerminalCommand(1, "settime 1337 0");
    tester.SimulateForGivenTime(150 * 1000);

    //Test that all connections are synced
    for (u32 i = 1; i <= tester.sim->GetTotalNodes(); i++) {
        tester.SendTerminalCommand(i, "status");
        tester.SimulateUntilMessageReceived(100, i, "tSync:2");
    }

    u32 minTime = 100000;
    u32 maxTime = 0;


    for (u32 i = 0; i < tester.sim->GetTotalNodes(); i++)
    {
        u32 time = tester.sim->nodes[i].gs.timeManager.GetLocalTime();
        if (time < minTime) minTime = time;
        if (time > maxTime) maxTime = time;
    }

    i32 timeDiff = maxTime - minTime;

    printf("Maximum time difference was: %d\n", timeDiff);

    ASSERT_TRUE(timeDiff <= 1);     //We allow 1 second off
}

