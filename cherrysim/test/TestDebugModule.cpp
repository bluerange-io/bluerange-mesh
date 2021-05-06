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
#include "DebugModule.h"
#include "Node.h"

TEST(TestDebugModule, TestCommands) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", 2 } );
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();
    tester.SimulateUntilClusteringDone(10 * 1000);

    tester.sim->FindNodeById(1)->gs.logger.EnableTag("DEBUGMOD");
    tester.sim->FindNodeById(1)->gs.logger.EnableTag("WATCHDOG");
    tester.sim->FindNodeById(1)->gs.logger.EnableTag("PQ");
    tester.sim->FindNodeById(1)->gs.logger.EnableTag("CPQ");
    tester.sim->FindNodeById(2)->gs.logger.EnableTag("DEBUGMOD");
    tester.sim->FindNodeById(1)->gs.logger.EnableTag("DEBUG");

    tester.SendTerminalCommand(1, "action 2 debug send_max_message");
    tester.SimulateUntilMessageReceived(10* 1000, 1, "{\"nodeId\":2,\"type\":\"send_max_message_response\", \"correctValues\":192, \"expectedCorrectValues\":192}");

    tester.SendTerminalCommand(1, "data 2");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "IN <= 1 ################## Got Data packet");

    tester.sim->FindNodeById(2)->gs.logger.EnableTag("DEBUGMOD");
    tester.sim->FindNodeById(2)->gs.logger.EnableTag("DEBUG");

    tester.SimulateUntilClusteringDone(100 * 1000);

    tester.SendTerminalCommand(1, "action 2 debug get_buffer");
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "\\{\"buf\":\"advT \\d+,rssi \\d+,time \\d+,last \\d+,node 2,cid \\d+,csiz \\d+, in \\d+, out \\d+, devT \\d+, ack \\d+\"\\}");

    tester.SendTerminalCommand(1, "action 2 debug reset_connection_loss_counter");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "Resetting connection loss counter");

    tester.SendTerminalCommand(1, "action 2 debug get_stats");
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "\\{\"nodeId\":2,\"type\":\"debug_stats\", \"conLoss\":0,\"dropped\":\\d+,\"sentRel\":\\d+,\"sentUnr\":\\d+\\}");

    {
        Exceptions::DisableDebugBreakOnException disable;
        bool exceptionCaught = false;
        try {
            tester.SendTerminalCommand(1, "action 2 debug hardfault");
            tester.SimulateForGivenTime(10 * 1000);
        }
        catch (...) {
            exceptionCaught = true;
        }
        ASSERT_TRUE(exceptionCaught);
    }

    //Set node 1 to listen for flood of node 2
    tester.SendTerminalCommand(1, "action this debug flood 2 3 0");
    tester.SimulateGivenNumberOfSteps(1);
    //Send node 2 the request to flood us (node 1) with 10 packets per 10 seconds with a timeout of 100 sec
    tester.SendTerminalCommand(1, "action 2 debug flood 1 1 10 100");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Resetting flood counter.");
    tester.SimulateForGivenTime(100 * 1000);

    NodeIndexSetter setter(0);
    ASSERT_GT(static_cast<DebugModule*>(tester.sim->FindNodeById(1)->gs.node.GetModuleById(ModuleId::DEBUG_MODULE))->GetPacketsIn(), (u32)95);

    tester.SendTerminalCommand(1, "action 2 debug ping 1 r");
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "p \\d+ ms");

    tester.SendTerminalCommand(1, "action 2 debug pingpong 1 r");
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "\\{\"type\":\"pingpong_response\",\"passedTime\":\\d+");

    tester.SendTerminalCommand(1, "heap");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"stack\":");

    tester.SendTerminalCommand(1, "readblock 1");
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "[0-9A-Za-z/=:]{20}");

    tester.SendTerminalCommand(1, "readblock 1 2");
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "[0-9A-Za-z/=:]{20}");

    tester.SendTerminalCommand(1, "memorymap");
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "[01]{250}");

    tester.SendTerminalCommand(1, "log_error 1337 42");
    tester.SimulateGivenNumberOfSteps(1);

    tester.SendTerminalCommand(1, "saverec 13 DE:AD:BE:EF");
    tester.SimulateGivenNumberOfSteps(1);
    tester.SendTerminalCommand(1, "getrec 13");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "DE:AD:BE:EF:");
    tester.SendTerminalCommand(1, "delrec 13");
    tester.SimulateGivenNumberOfSteps(1);

    tester.SendTerminalCommand(1, "send b");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "IN <= 0 ################## Got Data packet 0:0:7");

    tester.SendTerminalCommand(1, "feed");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Watchdogs fed.");

    tester.SendTerminalCommand(1, "lping 1 r");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "lp 1(0): 0 ms");

    tester.SendTerminalCommand(1, "filltx");
    tester.SimulateGivenNumberOfSteps(1);

    tester.SendTerminalCommand(1, "getpending");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "cm pending ");

    tester.SendTerminalCommand(1, "writedata 1336 DE:AD:BE:EF");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Queue CachedWrite ");

    tester.SendTerminalCommand(1, "floodstat");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Flooding has");

    tester.SendTerminalCommand(1, "printqueue");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Amount of Packets");

}

TEST(TestDebugModule, TestReadMemory) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", 2 } );
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    //Fill the flash of node 2 with some recognizable information
    for (int i = 0; i < 256; i++) {
        tester.sim->nodes[1].flash[i] = i;
    }

    tester.SimulateUntilClusteringDone(10 * 1000);

    //Query a readback of parts of the flash memory and check if we get the correct data back
    tester.SendTerminalCommand(1, "action 2 debug readmem 0 5");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"memory\",\"address\":0,\"data\":\"00:01:02:03:04\"}");

    tester.SendTerminalCommand(1, "action 2 debug readmem 10 32");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"memory\",\"address\":10,\"data\":\"0A:0B:0C:0D:0E:0F:10:11:12:13:14:15:16:17:18:19:1A:1B:1C:1D:1E:1F:20:21:22:23:24:25:26:27:28:29\"}");

    //We just make sure that it does not crash once we request too much
    tester.SendTerminalCommand(1, "action 2 debug readmem 10 400");
    tester.SimulateGivenNumberOfSteps(50);
}

TEST(TestDebugModule, TestQueueFlood) {
    // This test makes sure that data sent in high prio is being sent more than data in the medium queue. The same is checked for medium <=> low.
    // To check this, a lot of small messages are queued into random queues via the DebugModule. Random queues are chosen because if we would
    // iterate over all queues in the same order all the time, the first queue might get all the messages all the time, leaving none to the other
    // queues.
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 2 });
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);

    DebugModule* mod = nullptr;
    {
        NodeIndexSetter set(1);
        // Determining the address of the DebugModule of NodeIndex 1. We do this
        // before the loop so that we don't have to determine it over and over again.
        mod = (DebugModule*)GS->node.GetModuleById(ModuleId::DEBUG_MODULE);
        ASSERT_TRUE(mod != nullptr);
    }

    for (u32 i = 0; i < 10000; i++)
    {
        NodeIndexSetter set(1);
        mod->SendQueueFloodMessages();
        tester.SimulateGivenNumberOfSteps(1);
    }


    NodeIndexSetter set(0);
    // Now mod is no longer the address of the DebugModule of NodeIndex 1, but of NodeIndex 0.
    mod = (DebugModule*)GS->node.GetModuleById(ModuleId::DEBUG_MODULE);
    if (mod == nullptr) SIMEXCEPTIONFORCE(IllegalStateException);
    const u32 high   = mod->GetQueueFloodCounterHigh();
    const u32 medium = mod->GetQueueFloodCounterMedium();
    const u32 low    = mod->GetQueueFloodCounterLow();

    // Make sure that the high queue sent at least 50% more than the medium queue.
    // Do the same with medium <=> low.
    ASSERT_TRUE(high > medium * 3 / 2);
    ASSERT_TRUE(medium > low * 3 / 2);

    // Also make sure that the low prio queue was able to send at least some minimum.
    ASSERT_TRUE(low > 100);
}
