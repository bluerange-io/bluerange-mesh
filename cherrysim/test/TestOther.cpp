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
#include <fstream>
#include <CherrySimTester.h>
#include <Logger.h>
#include <Utility.h>
#include <string>
#include "ConnectionAllocator.h"
#include "StatusReporterModule.h"
#include "CherrySimUtils.h"
#include "RingIndexGenerator.h"
#include "json.hpp"
#include "SimpleQueue.h"
#include "DebugModule.h"


extern "C"{
#include <ccm_soft.h>
}


TEST(TestOther, BatteryTest)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 9});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SendTerminalCommand(1, "action this node discovery off");

    tester.SimulateForGivenTime(10000);

    //Log the battery usage
    for (u32 i = 0; i < tester.sim->GetTotalNodes(); i++) {
        u32 usageMicroAmpere = tester.sim->nodes[i].nanoAmperePerMsTotal / tester.sim->simState.simTimeMs;
        printf("Average Battery usage for node %d was %u uA" EOL, tester.sim->nodes[i].id, usageMicroAmpere);
    }
}

TEST(TestOther, TestSimpleQueue)
{
    SimpleQueue<u32, 4> queue;

    ASSERT_EQ(queue.GetAmountOfElements(), 0);
    ASSERT_EQ(queue.IsFull(), false);
    {
        Exceptions::DisableDebugBreakOnException disable;
        ASSERT_THROW(queue.Peek(), IllegalStateException);
    }
    ASSERT_EQ(queue.Pop(), false);

    ASSERT_TRUE(queue.Push(1));
    ASSERT_EQ(queue.GetAmountOfElements(), 1);
    ASSERT_EQ(queue.IsFull(), false);
    ASSERT_EQ(queue.Peek(), 1);
    ASSERT_EQ(queue.Pop(), true);
    ASSERT_EQ(queue.Pop(), false);

    ASSERT_TRUE(queue.Push(1));
    ASSERT_TRUE(queue.Push(2));
    ASSERT_EQ(queue.GetAmountOfElements(), 2);
    ASSERT_EQ(queue.IsFull(), false);
    ASSERT_EQ(queue.Peek(), 1);
    ASSERT_EQ(queue.Pop(), true);
    ASSERT_EQ(queue.Peek(), 2);
    ASSERT_EQ(queue.Pop(), true);
    ASSERT_EQ(queue.Pop(), false);

    ASSERT_TRUE(queue.Push(1));
    ASSERT_TRUE(queue.Push(2));
    ASSERT_TRUE(queue.Push(3));
    ASSERT_EQ(queue.GetAmountOfElements(), 3);
    ASSERT_EQ(queue.IsFull(), true);
    ASSERT_EQ(queue.Peek(), 1);
    ASSERT_EQ(queue.Pop(), true);
    ASSERT_EQ(queue.Peek(), 2);
    ASSERT_EQ(queue.Pop(), true);
    ASSERT_EQ(queue.Peek(), 3);
    ASSERT_EQ(queue.Pop(), true);
    ASSERT_EQ(queue.Pop(), false);

    ASSERT_TRUE(queue.Push(1));
    ASSERT_TRUE(queue.Push(2));
    ASSERT_TRUE(queue.Push(3));
    {
        Exceptions::DisableDebugBreakOnException disable;
        ASSERT_THROW(queue.Push(4), IllegalStateException);
    }
    ASSERT_EQ(queue.GetAmountOfElements(), 3);
    ASSERT_EQ(queue.IsFull(), true);
    ASSERT_EQ(queue.Peek(), 1);
    ASSERT_EQ(queue.Pop(), true);
    ASSERT_EQ(queue.Peek(), 2);
    ASSERT_EQ(queue.Pop(), true);
    ASSERT_EQ(queue.Peek(), 3);
    ASSERT_EQ(queue.Pop(), true);
    ASSERT_EQ(queue.Pop(), false);
}

TEST(TestOther, TestSimpleQueueRandomness)
{
    SimpleQueue<u32, 25> queue;
    std::queue<u32> comparisonQueue;
    MersenneTwister mt(1);

    for (u32 i = 0; i < 100000; i++)
    {
        ASSERT_EQ(queue.GetAmountOfElements(), comparisonQueue.size());
        ASSERT_EQ(queue.IsFull(), queue.GetAmountOfElements() == queue.length - 1);
        if (queue.GetAmountOfElements() > 0)
        {
            ASSERT_EQ(queue.Peek(), comparisonQueue.front());
        }

        // Flip coin if we push or pop
        if ((mt.NextPsrng(0xFFFFFFFF / 2) || queue.GetAmountOfElements() == 0) && queue.IsFull() == false)
        {
            // push
            const u32 element = mt.NextU32();
            queue.Push(element);
            comparisonQueue.push(element);
        }
        else
        {
            // pop
            queue.Pop();
            comparisonQueue.pop();
        }
    }
}

TEST(TestOther, TestRebootReason)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert( { "prod_sink_nrf52", 1 } );
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    //Make sure that the first reboot reason is unknown
    tester.SendTerminalCommand(1, "action this status get_rebootreason");
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "\\{\"type\":\"reboot_reason\",\"nodeId\":1,\"module\":3,\"reason\":0");

    //A local reset (7) should overwrite the reboot reason
    tester.SendTerminalCommand(1, "reset");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"reboot\",");
    tester.SendTerminalCommand(1, "action this status get_rebootreason");
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "\\{\"type\":\"reboot_reason\",\"nodeId\":1,\"module\":3,\"reason\":7");

    //After a successful boot, the node should no longer report RebootReason::UNKNOWN but RebootReason::UNKNOWN_BUT_BOOTED
    NodeIndexSetter setter(0);
    tester.sim->ResetCurrentNode(RebootReason::UNKNOWN, false);
    tester.SendTerminalCommand(1, "action this status get_rebootreason");
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "\\{\"type\":\"reboot_reason\",\"nodeId\":1,\"module\":3,\"reason\":22");
}

#ifndef GITHUB_RELEASE
TEST(TestOther, TestPositionSetting)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    //testerConfig.verbose = false;
    simConfig.mapWidthInMeters = 10;
    simConfig.mapHeightInMeters = 10;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 2});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    constexpr double absError = 0.1;

    tester.SendTerminalCommand(1, "sim set_position BBBBB 1337 52 12");
    tester.SimulateGivenNumberOfSteps(1);
    ASSERT_NEAR(tester.sim->nodes[0].x, 1337.f / simConfig.mapWidthInMeters,     absError);
    ASSERT_NEAR(tester.sim->nodes[0].y,   52.f / simConfig.mapHeightInMeters,    absError);
    ASSERT_NEAR(tester.sim->nodes[0].z,   12.f / simConfig.mapElevationInMeters, absError);

    tester.SendTerminalCommand(1, "sim set_position_norm BBBBB 1337 52 12");
    tester.SimulateGivenNumberOfSteps(1);
    ASSERT_NEAR(tester.sim->nodes[0].x, 1337, absError);
    ASSERT_NEAR(tester.sim->nodes[0].y,   52, absError);
    ASSERT_NEAR(tester.sim->nodes[0].z,   12, absError);

    tester.SendTerminalCommand(1, "sim add_position_norm BBBBB 12 13 14");
    tester.SimulateGivenNumberOfSteps(1);
    ASSERT_NEAR(tester.sim->nodes[0].x, 1349, absError);
    ASSERT_NEAR(tester.sim->nodes[0].y,   65, absError);
    ASSERT_NEAR(tester.sim->nodes[0].z,   26, absError);

    tester.SendTerminalCommand(1, "sim set_position BBBBC 13 14 2");
    tester.SimulateGivenNumberOfSteps(1);
    ASSERT_NEAR(tester.sim->nodes[1].x, 13.f / simConfig.mapWidthInMeters,     absError);
    ASSERT_NEAR(tester.sim->nodes[1].y, 14.f / simConfig.mapHeightInMeters,    absError);
    ASSERT_NEAR(tester.sim->nodes[1].z,  2.f / simConfig.mapElevationInMeters, absError);

    tester.SendTerminalCommand(1, "sim set_position BBBBD 100 200 111");
    tester.SimulateGivenNumberOfSteps(1);
    ASSERT_NEAR(tester.sim->nodes[2].x, 100.f / simConfig.mapWidthInMeters,     absError);
    ASSERT_NEAR(tester.sim->nodes[2].y, 200.f / simConfig.mapHeightInMeters,    absError);
    ASSERT_NEAR(tester.sim->nodes[2].z, 111.f / simConfig.mapElevationInMeters, absError);

    tester.SendTerminalCommand(1, "set_serial ZZZZZ");
    tester.SimulateGivenNumberOfSteps(1);
    tester.SendTerminalCommand(1, "sim set_position ZZZZZ -42.2 17.5 0.123");
    tester.SimulateGivenNumberOfSteps(1);
    ASSERT_NEAR(tester.sim->nodes[0].x, -42.2   / simConfig.mapWidthInMeters,     absError);
    ASSERT_NEAR(tester.sim->nodes[0].y,  17.5   / simConfig.mapHeightInMeters,    absError);
    ASSERT_NEAR(tester.sim->nodes[0].z,   0.123 / simConfig.mapElevationInMeters, absError);

    tester.SendTerminalCommand(1, "sim add_position BBBBC 5 6 -0.2");
    tester.SimulateGivenNumberOfSteps(1);
    ASSERT_NEAR(tester.sim->nodes[1].x, 18.f / simConfig.mapWidthInMeters,     absError);
    ASSERT_NEAR(tester.sim->nodes[1].y, 20.f / simConfig.mapHeightInMeters,    absError);
    ASSERT_NEAR(tester.sim->nodes[1].z,  1.8 / simConfig.mapElevationInMeters, absError);
}
#endif //!GITHUB_RELEASE

TEST(TestOther, TestMersenneTwister)
{
    //This test only makes sense for the seedOffset of 0. In other cases, this test just passes.
    if (MersenneTwister::seedOffset != 0) return;

    MersenneTwister mt(1337);
    ASSERT_EQ(mt.NextU32(),  925434190);
    ASSERT_EQ(mt.NextU32(), 2254002994);
    ASSERT_EQ(mt.NextU32(), 1395119812);
    ASSERT_EQ(mt.NextU32(), 2371922542);
    ASSERT_EQ(mt.NextU32(), 3640162417);
    ASSERT_EQ(mt.NextU32(), 2749074956);
    ASSERT_EQ(mt.NextU32(), 1787397407);
    ASSERT_EQ(mt.NextU32(), 4225313503);
    ASSERT_EQ(mt.NextU32(), 3241982240);
    ASSERT_EQ(mt.NextU32(), 1472171253);
    ASSERT_EQ(mt.NextU32(), 2121405432);
    ASSERT_EQ(mt.NextU32(), 1377883891);
    ASSERT_EQ(mt.NextU32(), 1980689950);
    ASSERT_EQ(mt.NextU32(), 3770806467);
    ASSERT_EQ(mt.NextU32(),  942187188);
    ASSERT_EQ(mt.NextU32(), 2388923659);
}

//This test should check if two different configurations can be applied to two nodes using the simulator
TEST(TestOther, ConfigurationTest)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.Start();


    tester.SimulateUntilClusteringDone(100 * 1000);
}

TEST(TestOther, TestJsonConfigSerialization)
{
    // This sure seems evil, so let me explain. The basic struggle is the question:
    // "What if someone changes the SimConfiguration but forgets to add the translation
    // of the new values to the to_json and from_json functions?". In such a case the
    // serialization of SimConfiguration might silently break, destroying e.g. the
    // replay feature. This test makes sure that this does not happen by interpreting a
    // memory region previously filled with garbage. Note that the object itself must not
    // be created as that would call the construction of each value, silently leading to
    // some probably valid values. CAREFUL THOUGH! If you add some member that needs
    // propper construction/destruction you have to call placement new and the destructor!

    static_assert(std::is_polymorphic<SimConfiguration>::value == false, "SimConfiguration must not be polymorphic as it does not get a valid v-table in this test.");

    constexpr u32 garbageMagicNumber = 0xDEADDEAD;
    alignas(SimConfiguration) u32 memoryArea[(sizeof(SimConfiguration) + 1) / sizeof(u32)];
    for (size_t i = 0; i < sizeof(memoryArea) / sizeof(*memoryArea); i++)
    {
        memoryArea[i] = garbageMagicNumber;
    }

    SimConfiguration *simConfig = reinterpret_cast<SimConfiguration*>(memoryArea);
    new (&simConfig->nodeConfigName)std::map<std::string, int>;
    simConfig->nodeConfigName.insert({ "prod_mesh_nrf52",1 });
    simConfig->seed = 3;
    simConfig->mapWidthInMeters = 4;
    simConfig->mapHeightInMeters = 5;
    simConfig->mapElevationInMeters = 6;
    simConfig->simTickDurationMs = 7;
    simConfig->terminalId = 8;
    simConfig->simOtherDelay = 9;
    simConfig->playDelay = 10;
    simConfig->interruptProbability = 11;
    simConfig->connectionTimeoutProbabilityPerSec = 12;
    simConfig->sdBleGapAdvDataSetFailProbability = 13;
    simConfig->sdBusyProbability = 14;
    simConfig->sdBusyProbabilityUnlikely = 15;
    simConfig->simulateAsyncFlash = true;
    simConfig->asyncFlashCommitTimeProbability = 16;
    simConfig->importFromJson = true;
    simConfig->realTime = true;
    simConfig->receptionProbabilityVeryClose = 11;
    simConfig->receptionProbabilityClose = 12;
    simConfig->receptionProbabilityFar = 13;
    simConfig->receptionProbabilityVeryFar = 14;
    new (&simConfig->siteJsonPath) std::string;
    simConfig->siteJsonPath = "aaa";
    new (&simConfig->devicesJsonPath) std::string;
    simConfig->devicesJsonPath = "bbb";
    new (&simConfig->replayPath) std::string;
    simConfig->replayPath = "path";
    simConfig->logReplayCommands = true;
    simConfig->useLogAccumulator = true;
    simConfig->defaultNetworkId = 19;
    new (&simConfig->preDefinedPositions)std::vector<std::pair<double, double>>;
    simConfig->preDefinedPositions = { {0.1, 0.2},{0.3, 0.4} };
    simConfig->rssiNoise = true;
    simConfig->simulateWatchdog = true;
    simConfig->simulateJittering = true;
    simConfig->verbose = true;
    simConfig->fastLaneToSimTimeMs = 123;
    simConfig->enableClusteringValidityCheck = true;
    simConfig->enableSimStatistics = true;
    new (&simConfig->storeFlashToFile) std::string;
    simConfig->storeFlashToFile = "eee";
    simConfig->verboseCommands = true;
    simConfig->defaultBleStackType = BleStackType::NRF_SD_132_ANY;

    for (size_t i = 0; i < sizeof(memoryArea) / sizeof(*memoryArea); i++)
    {
#define myOffsetOf(x, y) ((size_t)((char*)(&(x->y)) - (char*)((x))))
#define IsInSTLRange(x) (byteIndex >= myOffsetOf(simConfig, x) && byteIndex < myOffsetOf(simConfig, x) + sizeof(SimConfiguration::x))
        const size_t byteIndex = i * sizeof(u32);
        //The byte is ignored if it's in the range of an STL type as those are allowed to have uninitialized memory.

        if(IsInSTLRange(siteJsonPath)
            || IsInSTLRange(devicesJsonPath)
            || IsInSTLRange(replayPath)
            || IsInSTLRange(preDefinedPositions)
            || IsInSTLRange(nodeConfigName)
            || IsInSTLRange(storeFlashToFile)) continue;
#undef IsInSTLRange
        ASSERT_NE(memoryArea[i], garbageMagicNumber);
    }

    nlohmann::json j = *simConfig;
    SimConfiguration copy = j.get<SimConfiguration>();
    std::map<std::string, int> nodeConfigCompare;
    nodeConfigCompare.insert({ "prod_mesh_nrf52",1 });
    ASSERT_EQ(copy.nodeConfigName, nodeConfigCompare);
    ASSERT_EQ(copy.seed, 3);
    ASSERT_EQ(copy.mapWidthInMeters, 4);
    ASSERT_EQ(copy.mapHeightInMeters, 5);
    ASSERT_EQ(copy.mapElevationInMeters, 6);
    ASSERT_EQ(copy.simTickDurationMs, 7);
    ASSERT_EQ(copy.terminalId, 8);
    ASSERT_EQ(copy.simOtherDelay, 9);
    ASSERT_EQ(copy.playDelay, 10);
    ASSERT_EQ(copy.interruptProbability, 11);
    ASSERT_EQ(copy.connectionTimeoutProbabilityPerSec, 12);
    ASSERT_EQ(copy.sdBleGapAdvDataSetFailProbability, 13);
    ASSERT_EQ(copy.sdBusyProbability, 14);
    ASSERT_EQ(copy.sdBusyProbabilityUnlikely, 15);
    ASSERT_EQ(copy.simulateAsyncFlash, true);
    ASSERT_EQ(copy.asyncFlashCommitTimeProbability, 16);
    ASSERT_EQ(copy.importFromJson, true);
    ASSERT_EQ(copy.realTime, true);
    ASSERT_EQ(simConfig->receptionProbabilityVeryClose, 11);
    ASSERT_EQ(simConfig->receptionProbabilityClose, 12);
    ASSERT_EQ(simConfig->receptionProbabilityFar, 13);
    ASSERT_EQ(simConfig->receptionProbabilityVeryFar, 14);
    ASSERT_EQ(copy.siteJsonPath, "aaa");
    ASSERT_EQ(copy.devicesJsonPath, "bbb");
    ASSERT_EQ(copy.replayPath, "path");
    ASSERT_EQ(copy.logReplayCommands, true);
    ASSERT_EQ(copy.useLogAccumulator, true);
    ASSERT_EQ(copy.defaultNetworkId, 19);
    ASSERT_EQ(copy.preDefinedPositions.size(), 2);
    ASSERT_NEAR(copy.preDefinedPositions[0].first, 0.1, 0.01);
    ASSERT_NEAR(copy.preDefinedPositions[0].second, 0.2, 0.01);
    ASSERT_NEAR(copy.preDefinedPositions[1].first, 0.3, 0.01);
    ASSERT_NEAR(copy.preDefinedPositions[1].second, 0.4, 0.01);
    ASSERT_EQ(copy.rssiNoise, true);
    ASSERT_EQ(copy.simulateWatchdog, true);
    ASSERT_EQ(copy.simulateJittering, true);
    ASSERT_EQ(copy.verbose, true);
    ASSERT_EQ(simConfig->fastLaneToSimTimeMs, 123);
    ASSERT_EQ(copy.enableClusteringValidityCheck, true);
    ASSERT_EQ(copy.enableSimStatistics, true);
    ASSERT_EQ(copy.storeFlashToFile, "eee");
    ASSERT_EQ(copy.verboseCommands, true);
    ASSERT_EQ(copy.defaultBleStackType, BleStackType::NRF_SD_132_ANY);

    simConfig->storeFlashToFile.~basic_string();
    simConfig->nodeConfigName.~map();
    simConfig->preDefinedPositions.~vector();
    simConfig->devicesJsonPath.~basic_string();
    simConfig->replayPath.~basic_string();
    simConfig->siteJsonPath.~basic_string();
}


#if defined(PROD_SINK_NRF52)
TEST(TestOther, TestReplay)
{
    std::string logAccumulatorDemonstrator = "";
    std::string logAccumulatorReplay = "";

    constexpr u32 initialSleep = 1000;
    constexpr u32 getStatusSleep = 1234;
    constexpr u32 statusSleep = 91;
    constexpr u32 enrollmentSleep = 10 * 1000;
    constexpr u32 enrollmentRepeats = 5;

    constexpr u32 totalSleep = initialSleep + getStatusSleep + statusSleep + enrollmentSleep * enrollmentRepeats;

    {
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
        simConfig.nodeConfigName.insert({ "prod_sink_nrf52",1 });
        simConfig.nodeConfigName.insert({ "prod_mesh_nrf52",9 });
        simConfig.logReplayCommands = true;
        simConfig.useLogAccumulator = true;
        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
        tester.Start();

        tester.SimulateForGivenTime(initialSleep);
        tester.SendTerminalCommand(1, "action 0 status get_status");
        tester.SimulateForGivenTime(getStatusSleep);
        tester.SendTerminalCommand(7, "status");
        tester.SimulateForGivenTime(statusSleep);
        for (u32 i = 0; i < enrollmentRepeats; i++)
        {
            tester.SendTerminalCommand(1, "action 0 enroll basic BBBBG 5 118 ED:24:56:91:4E:48:C1:E1:7B:7B:D9:22:17:AE:59:EF FE:47:59:4D:FA:06:61:49:52:28:FD:5B:84:CA:DB:F5 43:BF:7F:7C:7B:AB:B2:C8:C5:3B:22:EB:F3:49:3B:01 05:00:00:00:05:00:00:00:05:00:00:00:05:00:00:00 5 0");
            tester.SimulateForGivenTime(enrollmentSleep);
        }

        logAccumulatorDemonstrator = tester.sim->logAccumulator;
    }

    //Sanity checks for the log accumulator.
    ASSERT_TRUE(logAccumulatorDemonstrator.size() > 200);
    ASSERT_TRUE(logAccumulatorDemonstrator.find("Node BBBBJ (nodeId: 7)") != std::string::npos); //Part of the status message

    //Check that all command executions are available
    ASSERT_TRUE(logAccumulatorDemonstrator.find("[!]COMMAND EXECUTION START:[!]index:0,time:1000,cmd:action 0 status get_status CRC: 1730502495[!]COMMAND EXECUTION END[!]") != std::string::npos);
    ASSERT_TRUE(logAccumulatorDemonstrator.find("[!]COMMAND EXECUTION START:[!]index:6,time:2250,cmd:status[!]COMMAND EXECUTION END[!]") != std::string::npos);
    ASSERT_TRUE(logAccumulatorDemonstrator.find("[!]COMMAND EXECUTION START:[!]index:0,time:2350,cmd:action 0 enroll basic BBBBG 5 118 ED:24:56:91:4E:48:C1:E1:7B:7B:D9:22:17:AE:59:EF FE:47:59:4D:FA:06:61:49:52:28:FD:5B:84:CA:DB:F5 43:BF:7F:7C:7B:AB:B2:C8:C5:3B:22:EB:F3:49:3B:01 05:00:00:00:05:00:00:00:05:00:00:00:05:00:00:00 5 0 CRC: 2568303097[!]COMMAND EXECUTION END[!]") != std::string::npos);
    ASSERT_TRUE(logAccumulatorDemonstrator.find("[!]COMMAND EXECUTION START:[!]index:0,time:12350,cmd:action 0 enroll basic BBBBG 5 118 ED:24:56:91:4E:48:C1:E1:7B:7B:D9:22:17:AE:59:EF FE:47:59:4D:FA:06:61:49:52:28:FD:5B:84:CA:DB:F5 43:BF:7F:7C:7B:AB:B2:C8:C5:3B:22:EB:F3:49:3B:01 05:00:00:00:05:00:00:00:05:00:00:00:05:00:00:00 5 0 CRC: 2568303097[!]COMMAND EXECUTION END[!]") != std::string::npos);
    ASSERT_TRUE(logAccumulatorDemonstrator.find("[!]COMMAND EXECUTION START:[!]index:0,time:22350,cmd:action 0 enroll basic BBBBG 5 118 ED:24:56:91:4E:48:C1:E1:7B:7B:D9:22:17:AE:59:EF FE:47:59:4D:FA:06:61:49:52:28:FD:5B:84:CA:DB:F5 43:BF:7F:7C:7B:AB:B2:C8:C5:3B:22:EB:F3:49:3B:01 05:00:00:00:05:00:00:00:05:00:00:00:05:00:00:00 5 0 CRC: 2568303097[!]COMMAND EXECUTION END[!]") != std::string::npos);
    ASSERT_TRUE(logAccumulatorDemonstrator.find("[!]COMMAND EXECUTION START:[!]index:0,time:32350,cmd:action 0 enroll basic BBBBG 5 118 ED:24:56:91:4E:48:C1:E1:7B:7B:D9:22:17:AE:59:EF FE:47:59:4D:FA:06:61:49:52:28:FD:5B:84:CA:DB:F5 43:BF:7F:7C:7B:AB:B2:C8:C5:3B:22:EB:F3:49:3B:01 05:00:00:00:05:00:00:00:05:00:00:00:05:00:00:00 5 0 CRC: 2568303097[!]COMMAND EXECUTION END[!]") != std::string::npos);
    ASSERT_TRUE(logAccumulatorDemonstrator.find("[!]COMMAND EXECUTION START:[!]index:0,time:42350,cmd:action 0 enroll basic BBBBG 5 118 ED:24:56:91:4E:48:C1:E1:7B:7B:D9:22:17:AE:59:EF FE:47:59:4D:FA:06:61:49:52:28:FD:5B:84:CA:DB:F5 43:BF:7F:7C:7B:AB:B2:C8:C5:3B:22:EB:F3:49:3B:01 05:00:00:00:05:00:00:00:05:00:00:00:05:00:00:00 5 0 CRC: 2568303097[!]COMMAND EXECUTION END[!]") != std::string::npos);

    const std::string replayPath = "TestReplay.log";
    std::ofstream out(replayPath);
    out << logAccumulatorDemonstrator;
    out.close();

    {
        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
        simConfig.replayPath = replayPath;
        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
        tester.Start();

        tester.SimulateForGivenTime(totalSleep);
        logAccumulatorReplay = tester.sim->logAccumulator;
    }

    //Unfortunately we can't check for equality as the logs contain pointer that change
    //every time a new simulation is started. So instead we have to check that the length
    //is roughly the same and additionally we check that the replay commands are present as well.
    ASSERT_NEAR(logAccumulatorDemonstrator.size(), logAccumulatorReplay.size(), 100);
    ASSERT_TRUE(logAccumulatorReplay.find("[!]COMMAND EXECUTION START:[!]index:0,time:1000,cmd:action 0 status get_status CRC: 1730502495[!]COMMAND EXECUTION END[!]") != std::string::npos);
    ASSERT_TRUE(logAccumulatorReplay.find("[!]COMMAND EXECUTION START:[!]index:6,time:2250,cmd:status[!]COMMAND EXECUTION END[!]") != std::string::npos);
    ASSERT_TRUE(logAccumulatorReplay.find("[!]COMMAND EXECUTION START:[!]index:0,time:2350,cmd:action 0 enroll basic BBBBG 5 118 ED:24:56:91:4E:48:C1:E1:7B:7B:D9:22:17:AE:59:EF FE:47:59:4D:FA:06:61:49:52:28:FD:5B:84:CA:DB:F5 43:BF:7F:7C:7B:AB:B2:C8:C5:3B:22:EB:F3:49:3B:01 05:00:00:00:05:00:00:00:05:00:00:00:05:00:00:00 5 0 CRC: 2568303097[!]COMMAND EXECUTION END[!]") != std::string::npos);
    ASSERT_TRUE(logAccumulatorReplay.find("[!]COMMAND EXECUTION START:[!]index:0,time:12350,cmd:action 0 enroll basic BBBBG 5 118 ED:24:56:91:4E:48:C1:E1:7B:7B:D9:22:17:AE:59:EF FE:47:59:4D:FA:06:61:49:52:28:FD:5B:84:CA:DB:F5 43:BF:7F:7C:7B:AB:B2:C8:C5:3B:22:EB:F3:49:3B:01 05:00:00:00:05:00:00:00:05:00:00:00:05:00:00:00 5 0 CRC: 2568303097[!]COMMAND EXECUTION END[!]") != std::string::npos);
    ASSERT_TRUE(logAccumulatorReplay.find("[!]COMMAND EXECUTION START:[!]index:0,time:22350,cmd:action 0 enroll basic BBBBG 5 118 ED:24:56:91:4E:48:C1:E1:7B:7B:D9:22:17:AE:59:EF FE:47:59:4D:FA:06:61:49:52:28:FD:5B:84:CA:DB:F5 43:BF:7F:7C:7B:AB:B2:C8:C5:3B:22:EB:F3:49:3B:01 05:00:00:00:05:00:00:00:05:00:00:00:05:00:00:00 5 0 CRC: 2568303097[!]COMMAND EXECUTION END[!]") != std::string::npos);
    ASSERT_TRUE(logAccumulatorReplay.find("[!]COMMAND EXECUTION START:[!]index:0,time:32350,cmd:action 0 enroll basic BBBBG 5 118 ED:24:56:91:4E:48:C1:E1:7B:7B:D9:22:17:AE:59:EF FE:47:59:4D:FA:06:61:49:52:28:FD:5B:84:CA:DB:F5 43:BF:7F:7C:7B:AB:B2:C8:C5:3B:22:EB:F3:49:3B:01 05:00:00:00:05:00:00:00:05:00:00:00:05:00:00:00 5 0 CRC: 2568303097[!]COMMAND EXECUTION END[!]") != std::string::npos);
    ASSERT_TRUE(logAccumulatorReplay.find("[!]COMMAND EXECUTION START:[!]index:0,time:42350,cmd:action 0 enroll basic BBBBG 5 118 ED:24:56:91:4E:48:C1:E1:7B:7B:D9:22:17:AE:59:EF FE:47:59:4D:FA:06:61:49:52:28:FD:5B:84:CA:DB:F5 43:BF:7F:7C:7B:AB:B2:C8:C5:3B:22:EB:F3:49:3B:01 05:00:00:00:05:00:00:00:05:00:00:00:05:00:00:00 5 0 CRC: 2568303097[!]COMMAND EXECUTION END[!]") != std::string::npos);
}
#endif //PROD_SINK_NRF52


TEST(TestOther, TestSimCommandCrc)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 1;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1 });
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();
    tester.SimulateGivenNumberOfSteps(1);
    tester.SendTerminalCommand(1, "sim animation create geofence-move-inside CRC: 2393378599"); //Correct CRC
    tester.SimulateGivenNumberOfSteps(1); //Test that no exception occures.
    tester.SendTerminalCommand(1, "sim animation create geofence-move-inside-2 CRC: 1337"); //Incorrect CRC
    {
        Exceptions::DisableDebugBreakOnException disabler;
        ASSERT_THROW(tester.SimulateGivenNumberOfSteps(1), CRCInvalidException);
    }
}

//This test should check if two different configurations can be applied to two nodes using the simulator
TEST(TestOther, SinkInMesh)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 1;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 9});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.Start();

    tester.SimulateUntilClusteringDone(1000 * 1000);

    //TODO: check that configurations were used
}

//Not sure what this test is doing, could ressurect it, but maybe not worth the effort
TEST(TestOther, TestEncryption) {
    //Boot up a simulator for our Logger
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert( { "prod_sink_nrf52", 1 } );
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    uint8_t cleartext[16];
    uint8_t key[16];
    uint8_t ciphertext[20];

    CheckedMemset(cleartext, 0x11, 16);
    CheckedMemset(key, 0x77, 16);


    uint8_t encnonce[13];
    CheckedMemset(encnonce, 0x01, 13);

    uint8_t decnonce[13];
    CheckedMemset(decnonce, 0x01, 13);

    printf("Encrypt normal AES\n");

    StackBaseSetter sbs;

    NodeIndexSetter setter(0);
    Utility::Aes128BlockEncrypt((Aes128Block*)cleartext, (Aes128Block*)key, (Aes128Block*)ciphertext);

    ccm_soft_data_t ccme;
    ccme.a_len = 0;
    ccme.mic_len = 4;
    ccme.m_len = 16;
    ccme.p_a = nullptr;
    ccme.p_key = key;
    ccme.p_m = cleartext;
    ccme.p_mic = &(ciphertext[15]);
    ccme.p_nonce = encnonce;
    ccme.p_out = ciphertext;


    printf("Encrypt AES CCM\n");

    ccm_soft_encrypt(&ccme);

    printf("Decrypt AES CCM\n");



    ccm_soft_data_t ccmd;
    ccmd.a_len = 0;
    ccmd.mic_len = 4;
    ccmd.m_len = 16;
    ccmd.p_a = nullptr;
    ccmd.p_key = key;
    ccmd.p_m = ciphertext;
    ccmd.p_mic = &(ciphertext[15]);
    ccmd.p_nonce = decnonce;
    ccmd.p_out = cleartext;

    CheckedMemset(cleartext, 0x00, 16);

    bool result = false;
    ccm_soft_decrypt(&ccmd, &result);
    printf("mic ok %u\n", result);


    printf("Encrypt same message again CCM\n");

    ccm_soft_encrypt(&ccme);
}


#if IS_ACTIVE(CLC_MODULE)
TEST(TestOther, TestConnectionAllocator) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert( { "prod_sink_nrf52", 1 } );
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.Start();

    MersenneTwister mt;
    mt.SetSeed(1);

    std::vector<BaseConnection*> conns;
    NodeIndexSetter setter(0);

    for (int i = 0; i < 10000; i++) 
    {
        if ((mt.NextU32(0, 1) && conns.size() > 0) || conns.size() == TOTAL_NUM_CONNECTIONS) { //dealloc
            int index = mt.NextU32(0, conns.size() - 1);
            ConnectionAllocator::GetInstance().Deallocate(conns[index]);
            conns.erase(conns.begin() + index);
        }
        else { //alloc
            FruityHal::BleGapAddr addr;
            addr.addr_type = FruityHal::BleGapAddrType::PUBLIC;
            addr.addr = {};
            int type = mt.NextU32(0, 3);
            if (type == 0) 
            {
                conns.push_back(ConnectionAllocator::GetInstance().AllocateClcAppConnection(0, ConnectionDirection::DIRECTION_IN, &addr));
            }
            else if (type == 1) 
            {
                conns.push_back(ConnectionAllocator::GetInstance().AllocateMeshAccessConnection(0, ConnectionDirection::DIRECTION_IN, &addr, FmKeyId::ZERO, MeshAccessTunnelType::INVALID, 0));
            }
            else if (type == 2) 
            {
                conns.push_back(ConnectionAllocator::GetInstance().AllocateMeshConnection(0, ConnectionDirection::DIRECTION_IN, &addr, 0));
            }
            else 
            {
                conns.push_back(ConnectionAllocator::GetInstance().AllocateResolverConnection(0, ConnectionDirection::DIRECTION_IN, &addr));
            }
        }
    }
}
#endif //IS_ACTIVE(CLC_MODULE)

//Tests the implementation of CherrySimTester::SimulateUntilMessagesReceived.
TEST(TestOther, TestMultiMessageSimulation) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    //Multiple copies of the same message should also happen multiple times. So, SimulateUntilMessagesReceived
    //only returnes if node 1 and 2, both send live_report twice.
    std::vector<SimulationMessage> msgs = {
        SimulationMessage(1, "Handshake starting"),
        SimulationMessage(2, "Handshake starting"),
        SimulationMessage(2, "Handshake done"),
    };

    tester.SimulateUntilMessagesReceived(10 * 1000, msgs);

    for (unsigned i = 0; i < msgs.size(); i++) {
        std::string completeMsg = msgs[i].GetCompleteMessage();
        if (completeMsg.find("### Handshake starting ###") == std::string::npos && completeMsg.find("Handshake done") == std::string::npos) {
            FAIL() << "Did not receive complete live report message.";
        }
    }
}

//This test should make sure that Gattc timeout events are reported properly through the error log and live reports
TEST(TestOther, TestGattcEvtTimeoutReporting) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(10 * 1000);

    //Find the mesh connection to the other node
    NodeIndexSetter setter(0);
    BaseConnections conns = tester.sim->nodes[0].gs.cm.GetConnectionsOfType(ConnectionType::FRUITYMESH, ConnectionDirection::INVALID);
    MeshConnection* conn = nullptr;
    for (int i = 0; i < conns.count; i++)
    {
        conn = (MeshConnection*)conns.handles[i].GetConnection();
        break;
    }
    if (conn == nullptr)
    {
        SIMEXCEPTIONFORCE(IllegalStateException);
        FAIL() << "Conn was nullptr!\n";
        return;
    }

    //Simulate that one node receives a BLE_GATTC_EVT_TIMEOUT on its connection
    //ATTENTION: Does not simulate the event properly as the connection should be invalid
    //after this event and no more data can be sent.
    simBleEvent s;
    CheckedMemset(&s, 0, sizeof(s));
    s.globalId = tester.sim->simState.globalEventIdCounter++;
    s.bleEvent.header.evt_id = BLE_GATTC_EVT_TIMEOUT;
    s.bleEvent.header.evt_len = s.globalId;
    s.bleEvent.evt.gattc_evt.conn_handle = conn->connectionHandle;
    //s.bleEvent.evt.gattc_evt.gatt_status = ?
    s.bleEvent.evt.gattc_evt.params.timeout.src = BLE_GATT_TIMEOUT_SRC_PROTOCOL;
    tester.sim->nodes[0].eventQueue.push_back(s);

    //Wait until the live report about the mesh disconnect with the proper disconnect reason is received
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"live_report\",\"nodeId\":1,\"module\":3,\"code\":51,\"extra\":2,\"extra2\":31}");

    //Ask for the error log
    tester.SendTerminalCommand(1, "action this status get_errors");

    //Check if the error log contains the proper entry
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"error_log_entry\",\"nodeId\":1,\"module\":3,\"errType\":2,\"code\":1");
}

TEST(TestOther, TestTimeSync) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
//    testerConfig.verbose = true;
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
    auto start = tester.sim->currentNode->gs.timeManager.GetTime();
    tester.SimulateForGivenTime(10 * 1000);

    //... and check that the times have been updated correctly
    auto end = tester.sim->currentNode->gs.timeManager.GetTime();
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
        tester.SimulateUntilMessageReceived(100 * 1000, i, "Time is currently approx. 1970 years, 1 days, 02h");
    }
}

TEST(TestOther, TestTimeSyncDuration_long) {
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
        u32 time = tester.sim->nodes[i].gs.timeManager.GetTime();
        if (time < minTime) minTime = time;
        if (time > maxTime) maxTime = time;
    }

    i32 timeDiff = maxTime - minTime;

    printf("Maximum time difference was: %d\n", timeDiff);

    ASSERT_TRUE(timeDiff <= 1);     //We allow 1 second off
}

TEST(TestOther, TestRestrainedKeyGeneration) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert( { "prod_sink_nrf52", 1 } );
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();
    tester.SimulateGivenNumberOfSteps(1);
    char restrainedKeyHexBuffer[1024];
    u8 restrainedKeyBuffer[1024];
    StackBaseSetter sbs;
    NodeIndexSetter setter(0);

    tester.SendTerminalCommand(1, "set_node_key 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF");
    tester.SimulateGivenNumberOfSteps(1);
    Conf::GetInstance().GetRestrainedKey(restrainedKeyBuffer);
    Logger::ConvertBufferToHexString(restrainedKeyBuffer, 16, restrainedKeyHexBuffer, sizeof(restrainedKeyHexBuffer));
    ASSERT_STREQ("2A:FC:35:99:4C:86:11:48:58:4C:C6:D9:EE:D4:A2:B6", restrainedKeyHexBuffer);

    tester.SendTerminalCommand(1, "set_node_key FF:EE:DD:CC:BB:AA:99:88:77:66:55:44:33:22:11:00");
    tester.SimulateGivenNumberOfSteps(1);
    Conf::GetInstance().GetRestrainedKey(restrainedKeyBuffer);
    Logger::ConvertBufferToHexString(restrainedKeyBuffer, 16, restrainedKeyHexBuffer, sizeof(restrainedKeyHexBuffer));
    ASSERT_STREQ("9E:63:8B:94:65:85:91:99:A9:74:7D:A7:40:7C:DD:B3", restrainedKeyHexBuffer);

    tester.SendTerminalCommand(1, "set_node_key DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF");
    tester.SimulateGivenNumberOfSteps(1);
    Conf::GetInstance().GetRestrainedKey(restrainedKeyBuffer);
    Logger::ConvertBufferToHexString(restrainedKeyBuffer, 16, restrainedKeyHexBuffer, sizeof(restrainedKeyHexBuffer));
    ASSERT_STREQ("3C:58:54:FC:29:96:00:59:B7:80:6B:4C:78:49:8B:27", restrainedKeyHexBuffer);

    tester.SendTerminalCommand(1, "set_node_key 00:01:02:03:04:05:06:07:08:09:0A:0B:0C:0D:0E:0F");
    tester.SimulateGivenNumberOfSteps(1);
    Conf::GetInstance().GetRestrainedKey(restrainedKeyBuffer);
    Logger::ConvertBufferToHexString(restrainedKeyBuffer, 16, restrainedKeyHexBuffer, sizeof(restrainedKeyHexBuffer));
    ASSERT_STREQ("60:AB:54:BB:F5:1C:3F:77:FA:BC:80:4C:E0:F4:78:58", restrainedKeyHexBuffer);
}

TEST(TestOther, TestTimeout) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1 });
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    {
        Exceptions::DisableDebugBreakOnException disable;
        ASSERT_THROW(tester.SimulateUntilMessageReceived(10 * 1000, 1, "I will never be printed!"), TimeoutException);
        ASSERT_THROW(tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "I will never be printed!"), TimeoutException);
        {
            std::vector<SimulationMessage> sm = { SimulationMessage(1, "I will never be printed!") };
            ASSERT_THROW(tester.SimulateUntilMessagesReceived(10 * 1000, sm), TimeoutException);
        }
        {
            std::vector<SimulationMessage> sm = { SimulationMessage(1, "I will never be printed!") };
            ASSERT_THROW(tester.SimulateUntilRegexMessagesReceived(10 * 1000, sm), TimeoutException);
        }
    }
}


TEST(TestOther, TestLegacyUicrSerialNumberSupport) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert( { "prod_sink_nrf52", 1 } );
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    const char* serialNumber = "BRTCR";

    tester.sim->nodes[0].uicr.CUSTOMER[12] = EMPTY_WORD;
    CheckedMemcpy(tester.sim->nodes[0].uicr.CUSTOMER + 2, serialNumber, 6);

    tester.Start();

    tester.SendTerminalCommand(1, "status");
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "Node BRTCR");

}

#ifndef GITHUB_RELEASE
TEST(TestOther, TestBulkMode) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.SetToPerfectConditions();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_pcbridge_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    CheckedMemset(tester.sim->nodes[0].uicr.CUSTOMER, 0xFF, sizeof(tester.sim->nodes[1].uicr.CUSTOMER));    //invalidate UICR

    tester.Start();

    tester.SimulateForGivenTime(10 * 1000); //Simulate a little to calculate battery usage.

    u32 usageMicroAmpere = tester.sim->nodes[0].nanoAmperePerMsTotal / tester.sim->simState.simTimeMs;
    if (usageMicroAmpere > 100)
    {
        FAIL() << "Bulk Mode should consume very low energy, but consumed: " << usageMicroAmpere;
    }
    tester.SendTerminalCommand(1, "set_node_key 02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00");
    tester.SimulateGivenNumberOfSteps(1);

    tester.SendTerminalCommand(2, "action this ma connect 00:00:00:01:00:00 1 02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00"); //1 = FmKeyId::NODE

    tester.SimulateForGivenTime(10 * 1000);
    u32 dummyVal = 1337;
    tester.SendTerminalCommand(2, "action 2002 bulk get_memory %u 4", (u32)(&dummyVal));
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 2, "\\{\"nodeId\":2002,\"type\":\"get_memory_result\",\"addr\":\\d+,\"data\":\"39:05:00:00\"\\}");

    u8 data[1024];
    for (size_t i = 0; i < sizeof(data); i++)
    {
        data[i] = i % 256;
    }
    u32 expectedValue = Utility::CalculateCrc32(data, sizeof(data));
    tester.SendTerminalCommand(2, "action 2002 bulk get_crc32 %u %u", (u32)(data), (u32)(sizeof(data)));
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":2002,\"type\":\"get_crc32_result\",\"crc32\":%u}", expectedValue);

    tester.SendTerminalCommand(2, "action 2002 bulk get_uicr_custom");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":2002,\"type\":\"get_uicr_custom_result\",\"data\":\"FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF\"}");
    
    tester.SendTerminalCommand(2, "action 2002 bulk set_uicr_custom AA:BB:CC:DD");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":2002,\"type\":\"set_uicr_custom_result\",\"code\":0}");
    tester.SimulateForGivenTime(20 * 1000); //Give the node time to reboot.

    ASSERT_EQ(tester.sim->nodes[0].restartCounter, 2);
    ASSERT_EQ(((u8*)(tester.sim->nodes[0].uicr.CUSTOMER))[0], 0xAA);
    ASSERT_EQ(((u8*)(tester.sim->nodes[0].uicr.CUSTOMER))[1], 0xBB);
    ASSERT_EQ(((u8*)(tester.sim->nodes[0].uicr.CUSTOMER))[2], 0xCC);
    ASSERT_EQ(((u8*)(tester.sim->nodes[0].uicr.CUSTOMER))[3], 0xDD);
    ASSERT_EQ(((u8*)(tester.sim->nodes[0].uicr.CUSTOMER))[4], 0xFF);
}
#endif //GITHUB_RELEASE

#if defined(DEV_AUTOMATED_TESTS_MASTER_NRF52)
TEST(TestOther, TestWatchdog) {
    Exceptions::ExceptionDisabler<WatchdogTriggeredException> wtDisabler;
    Exceptions::ExceptionDisabler<SafeBootTriggeredException> stDisabler;
    const char* usedFeatureset = "dev_automated_tests_master_nrf52"; //Has a very small watchdog time which is convenient for this test and its duration.
    constexpr unsigned long long starvationTimeNormalMode = 60UL * 2UL * 1000UL;
    constexpr unsigned long long starvationTimeSafeBoot = 20UL * 1000UL;
    {
        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        //testerConfig.verbose = true;
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
        simConfig.terminalId = 0;
        simConfig.simulateWatchdog = true;
        simConfig.nodeConfigName.insert( { usedFeatureset, 1 } );
        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

        tester.Start();

        tester.SimulateForGivenTime(1000);

        ASSERT_EQ(tester.sim->nodes[0].restartCounter, 1);

        tester.SimulateForGivenTime(starvationTimeNormalMode - 2000); //After this the node will be one second away from starvation.
        ASSERT_EQ(tester.sim->nodes[0].restartCounter, 1);

        tester.SimulateForGivenTime(2000); //Starve it and give it some time to reboot.
        ASSERT_EQ(tester.sim->nodes[0].restartCounter, 2);

        NodeIndexSetter setter(0);
        ASSERT_EQ(*GS->rebootMagicNumberPtr, REBOOT_MAGIC_NUMBER); // Check whether it is in safeBoot mode

        tester.SimulateForGivenTime(starvationTimeSafeBoot); //Starve it in safe boot mode and hence shorter watchdog delays
        ASSERT_EQ(*GS->rebootMagicNumberPtr, 0);
        ASSERT_EQ(tester.sim->nodes[0].restartCounter, 3);

        for (int i = 0; i < 30; i++) {
            //Lets keep feeding it. The node should not reset in this time.
            tester.SendTerminalCommand(1, "action this status keep_alive");
            tester.SimulateForGivenTime(starvationTimeNormalMode / 10);
            ASSERT_EQ(tester.sim->nodes[0].restartCounter, 3);
        }
    }


    {
        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        //testerConfig.verbose = true;
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
        simConfig.terminalId = 0;
        simConfig.simulateWatchdog = false;
        simConfig.nodeConfigName.insert( { usedFeatureset, 1 } );
        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

        tester.Start();

        tester.SimulateForGivenTime(starvationTimeNormalMode * 2);

        ASSERT_EQ(tester.sim->nodes[0].restartCounter, 1); //watchdogs are disabled, nodes should not starve at all.
    }
}
#endif //DEV_AUTOMATED_TESTS_MASTER_NRF52


#ifndef GITHUB_RELEASE
#if IS_ACTIVE(CLC_MODULE)
extern void SetBoard_3(BoardConfiguration *c);
extern void SetBoard_4(BoardConfiguration *c);
extern void SetBoard_9(BoardConfiguration *c);
extern void SetBoard_10(BoardConfiguration *c);
extern void SetBoard_11(BoardConfiguration *c);
extern void SetBoard_12(BoardConfiguration *c);
extern void SetBoard_13(BoardConfiguration *c);
extern void SetBoard_14(BoardConfiguration *c);
extern void SetBoard_16(BoardConfiguration *c);
extern void SetBoard_17(BoardConfiguration *c);
extern void SetBoard_18(BoardConfiguration *c);
extern void SetBoard_19(BoardConfiguration *c);
extern void SetBoard_20(BoardConfiguration *c);
extern void SetBoard_21(BoardConfiguration *c);
extern void SetBoard_22(BoardConfiguration *c);
extern void SetBoard_23(BoardConfiguration* c);
extern void SetBoard_24(BoardConfiguration* c);

TEST(TestOther, TestBoards) {
    //Executes all setBoard configs to make sure none of them crashes anything.
    //Does NOT check state!

    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.simulateWatchdog = true;
    simConfig.nodeConfigName.insert( { "prod_sink_nrf52", 1 } );
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    NodeIndexSetter setter(0);

    BoardConfiguration c;

    c.boardType = 3;
    SetBoard_3(&c);

    c.boardType = 4;
    SetBoard_4(&c);

    c.boardType = 9;
    SetBoard_9(&c);

    u8 buffer[1000] = {};
    NRF_UICR->CUSTOMER[1] = (u32)buffer;
    c.boardType = 10;
    SetBoard_10(&c);

    c.boardType = 11;
    SetBoard_11(&c);

    c.boardType = 12;
    SetBoard_12(&c);

    c.boardType = 13;
    SetBoard_13(&c);

    c.boardType = 14;
    SetBoard_14(&c);

    c.boardType = 16;
    SetBoard_16(&c);

    c.boardType = 17;
    SetBoard_17(&c);

    c.boardType = 18;
    SetBoard_18(&c);

    c.boardType = 19;
    SetBoard_19(&c);

    c.boardType = 20;
    SetBoard_20(&c);

    c.boardType = 21;
    SetBoard_21(&c);

    c.boardType = 22;
    SetBoard_22(&c);

    c.boardType = 23;
    SetBoard_23(&c);

    c.boardType = 24;
    SetBoard_24(&c);
}
#endif // IS_ACTIVE(CLC_MODULE)
#endif //GITHUB_RELEASE

TEST(TestOther, TestRingIndexGenerator) {
    Exceptions::DisableDebugBreakOnException ddboe;

    {
        RingIndexGenerator rig(0, 3);
        ASSERT_EQ(rig.HasNext(), true);
        ASSERT_EQ(rig.Next(), 0);
        ASSERT_EQ(rig.HasNext(), true);
        ASSERT_EQ(rig.Next(), 1);
        ASSERT_EQ(rig.HasNext(), true);
        ASSERT_EQ(rig.Next(), 2);
        ASSERT_EQ(rig.HasNext(), false);
        ASSERT_THROW(rig.Next(), IllegalStateException);
    }

    {
        RingIndexGenerator rig(1, 3);
        ASSERT_EQ(rig.HasNext(), true);
        ASSERT_EQ(rig.Next(), 1);
        ASSERT_EQ(rig.HasNext(), true);
        ASSERT_EQ(rig.Next(), 2);
        ASSERT_EQ(rig.HasNext(), true);
        ASSERT_EQ(rig.Next(), 0);
        ASSERT_EQ(rig.HasNext(), false);
        ASSERT_THROW(rig.Next(), IllegalStateException);
    }

    {
        RingIndexGenerator rig(2, 3);
        ASSERT_EQ(rig.HasNext(), true);
        ASSERT_EQ(rig.Next(), 2);
        ASSERT_EQ(rig.HasNext(), true);
        ASSERT_EQ(rig.Next(), 0);
        ASSERT_EQ(rig.HasNext(), true);
        ASSERT_EQ(rig.Next(), 1);
        ASSERT_EQ(rig.HasNext(), false);
        ASSERT_THROW(rig.Next(), IllegalStateException);
    }

    {
        RingIndexGenerator rig(3, 3);
        ASSERT_EQ(rig.HasNext(), true);
        ASSERT_EQ(rig.Next(), 0);
        ASSERT_EQ(rig.HasNext(), true);
        ASSERT_EQ(rig.Next(), 1);
        ASSERT_EQ(rig.HasNext(), true);
        ASSERT_EQ(rig.Next(), 2);
        ASSERT_EQ(rig.HasNext(), false);
        ASSERT_THROW(rig.Next(), IllegalStateException);
    }
}

TEST(TestOther, TestStringConversions) {
    ASSERT_EQ(Utility::StringToI8(  "0"), 0);
    ASSERT_EQ(Utility::StringToI8( "00"), 0);
    ASSERT_EQ(Utility::StringToI8("000"), 0);
    ASSERT_EQ(Utility::StringToI8(  "1"), 1);
    ASSERT_EQ(Utility::StringToI8( "01"), 1);
    ASSERT_EQ(Utility::StringToI8("001"), 1);
    ASSERT_EQ(Utility::StringToI8(  "13"), 13);
    ASSERT_EQ(Utility::StringToI8( "013"), 13);
    ASSERT_EQ(Utility::StringToI8("0013"), 13);
    ASSERT_EQ(Utility::StringToI8(  "-0"), 0);
    ASSERT_EQ(Utility::StringToI8( "-00"), 0);
    ASSERT_EQ(Utility::StringToI8("-000"), 0);
    ASSERT_EQ(Utility::StringToI8(  "-1"), -1);
    ASSERT_EQ(Utility::StringToI8( "-01"), -1);
    ASSERT_EQ(Utility::StringToI8("-001"), -1);
    ASSERT_EQ(Utility::StringToI8(  "-13"), -13);
    ASSERT_EQ(Utility::StringToI8( "-013"), -13);
    ASSERT_EQ(Utility::StringToI8("-0013"), -13);

    ASSERT_EQ(Utility::StringToU8(  "0"), 0);
    ASSERT_EQ(Utility::StringToU8( "00"), 0);
    ASSERT_EQ(Utility::StringToU8("000"), 0);
    ASSERT_EQ(Utility::StringToU8(  "1"), 1);
    ASSERT_EQ(Utility::StringToU8( "01"), 1);
    ASSERT_EQ(Utility::StringToU8("001"), 1);
    ASSERT_EQ(Utility::StringToU8(  "13"), 13);
    ASSERT_EQ(Utility::StringToU8( "013"), 13);
    ASSERT_EQ(Utility::StringToU8("0013"), 13);

    ASSERT_EQ(Utility::StringToI8("127"), 127);
    ASSERT_EQ(Utility::StringToI16("127"), 127);
    ASSERT_EQ(Utility::StringToI32("127"), 127);
    {
        Exceptions::ExceptionDisabler<NumberStringNotInRangeException> Nsnir;
        ASSERT_EQ(Utility::StringToI8("128"), 0);
    }
    ASSERT_EQ(Utility::StringToI16("128"), 128);
    ASSERT_EQ(Utility::StringToI32("128"), 128);
    ASSERT_EQ(Utility::StringToI8("-128"), -128);
    ASSERT_EQ(Utility::StringToI16("-128"), -128);
    ASSERT_EQ(Utility::StringToI32("-128"), -128);
    {
        Exceptions::ExceptionDisabler<NumberStringNotInRangeException> Nsnir;
        ASSERT_EQ(Utility::StringToI8("-129"), 0);
    }
    ASSERT_EQ(Utility::StringToI16("-129"), -129);
    ASSERT_EQ(Utility::StringToI32("-129"), -129);

    ASSERT_EQ(Utility::StringToU8("255"), 255);
    ASSERT_EQ(Utility::StringToU16("255"), 255);
    ASSERT_EQ(Utility::StringToU32("255"), 255);
    {
        Exceptions::ExceptionDisabler<NumberStringNotInRangeException> Nsnir;
        ASSERT_EQ(Utility::StringToU8("256"), 0);
    }
    ASSERT_EQ(Utility::StringToU16("256"), 256);
    ASSERT_EQ(Utility::StringToU32("256"), 256);
    {
        Exceptions::ExceptionDisabler<NumberStringNotInRangeException> Nsnir;
        ASSERT_EQ(Utility::StringToU8("70000"), 0);
        ASSERT_EQ(Utility::StringToU16("70000"), 0);
    }
    ASSERT_EQ(Utility::StringToU32("70000"), 70000);


    ASSERT_EQ(Utility::StringToU32("0x0A"), 10);

    {
        Exceptions::ExceptionDisabler<NotANumberStringException> Nanse;
        Exceptions::ExceptionDisabler<NumberStringNotInRangeException> Nsnir;
        bool didError = false;
        ASSERT_EQ(Utility::StringToU8("255", &didError), 255);
        ASSERT_FALSE(didError);
        ASSERT_EQ(Utility::StringToU16("255", &didError), 255);
        ASSERT_FALSE(didError);
        ASSERT_EQ(Utility::StringToU32("255", &didError), 255);
        ASSERT_FALSE(didError);
        ASSERT_EQ(Utility::StringToU8("256", &didError), 0);
        ASSERT_TRUE(didError);
        didError = false;
        ASSERT_EQ(Utility::StringToU16("256", &didError), 256);
        ASSERT_FALSE(didError);
        ASSERT_EQ(Utility::StringToU32("256", &didError), 256);
        ASSERT_FALSE(didError);
        ASSERT_EQ(Utility::StringToU8("70000", &didError), 0);
        ASSERT_TRUE(didError);
        didError = false;
        ASSERT_EQ(Utility::StringToU16("70000", &didError), 0);
        ASSERT_TRUE(didError);
        didError = false;
        ASSERT_EQ(Utility::StringToU32("70000", &didError), 70000);
        ASSERT_FALSE(didError);
    }
}

TEST(TestOther, TestNoOfReceivedMsgs) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1 });
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1 });
    simConfig.terminalId = 0;
    simConfig.SetToPerfectConditions();
   // testerConfig.verbose = true;

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.Start();

    tester.SimulateUntilClusteringDone(50 * 1000);

    tester.SendTerminalCommand(1, "action 2 status get_errors");
    tester.SendTerminalCommand(1, "action 2 status get_device_info");
    tester.SendTerminalCommand(1, "action 2 status get_device_info");
    tester.SendTerminalCommand(1, "action 2 status get_errors");

    //number of received messages (3)
    //2 get_devic_info msgs 
    //1 get_errors msg
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "\\{\"type\":\"error_log_entry\",\"nodeId\":2,\"module\":3,\"errType\":2,\"code\":81,\"extra\":3,\"time\":\\d+");
}

#ifndef GITHUB_RELEASE
TEST(TestOther, TestSimulatorFlashToFileStorage) {
    const char* testFilePath = "TestFlashStorageFile.bin";
    //Make sure the testfile does not exist anymore.
    remove(testFilePath);


    //First iteration will create the safe file and enroll all the beacons, second iteration will load it and make sure that they are still enrolled.
    for(int i = 0; i<2; i++)
    {
        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        //testerConfig.verbose = true;
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
        simConfig.terminalId = 0;
        simConfig.defaultNetworkId = 0;
        simConfig.storeFlashToFile = testFilePath;
        simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
        simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 2});
        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
        tester.Start();
        
        if (i == 0)
        {
            {
                //Make sure that the nodes are unable to cluster
                Exceptions::DisableDebugBreakOnException ddboe;
                ASSERT_THROW(tester.SimulateUntilClusteringDone(10 * 1000), TimeoutException);
            }

            std::vector<std::string> messages = {
                "action 0 enroll basic BBBBB 1 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 01:00:00:00:01:00:00:00:01:00:00:00:01:00:00:00 10 0 0",
                "action 0 enroll basic BBBBC 2 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00 10 0 0",
                "action 0 enroll basic BBBBD 3 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 03:00:00:00:03:00:00:00:03:00:00:00:03:00:00:00 10 0 0",
            };

            ASSERT_EQ(messages.size(), tester.sim->GetTotalNodes());

            for (size_t nodeIndex = 0; nodeIndex < messages.size(); nodeIndex++) {
                for (int i = 0; i < 10; i++) {
                    tester.SendTerminalCommand(nodeIndex + 1, messages[nodeIndex].c_str());
                    tester.SimulateGivenNumberOfSteps(10);
                }
            }

            tester.SimulateUntilClusteringDone(10 * 1000);

        }
        else if (i == 1)
        {
            tester.SimulateUntilClusteringDone(10 * 1000);
        }
    }

}

TEST(TestOther, TestConnectionSupervisionTimeoutWillDisconnect) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    // testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});

    simConfig.mapWidthInMeters = 100;
    simConfig.mapHeightInMeters = 100;
    simConfig.preDefinedPositions = {{0, 0},{0, 0.1}};

    // We want to check if with variable noise when device is at distance limit it will disconnect within few seconds
    // as expected because of connection supervision timeout
    simConfig.rssiNoise = true;

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();
    tester.sim->FindNodeById(1)->gs.logger.EnableTag("C");
    tester.sim->FindNodeById(2)->gs.logger.EnableTag("C");

    tester.SimulateUntilClusteringDone(100 * 1000);

    // With following settings static RSSI is around -89.95dbm which is just above reception level (0.3 probability).
    // With variable noise it should casue connection timeout
    tester.sim->SetPosition(1, 0, 0.25, 0);
    tester.SimulateUntilMessageReceived(1000 * 1000, 2, "Disconnected device %d", FruityHal::BleHciError::CONNECTION_TIMEOUT);
}

TEST(TestOther, TestConnectionSupervisionTimeoutWontDisconnect) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    // testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});

    simConfig.mapWidthInMeters = 100;
    simConfig.mapHeightInMeters = 100;
    simConfig.preDefinedPositions = {{0, 0},{0, 0.1}};

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();
    tester.sim->FindNodeById(1)->gs.logger.EnableTag("C");
    tester.sim->FindNodeById(2)->gs.logger.EnableTag("C");

    tester.SimulateUntilClusteringDone(10 * 1000);

    // With following settings static RSSI is around -89.95dbm which is just above reception level (0.3 probability).
    // With no variable noise we never get reception probability to 0 so there is almost no chance all packets will be lost with connection timeout
    tester.sim->SetPosition(1, 0, 0.25, 0);
    {
        Exceptions::DisableDebugBreakOnException ddboe;
        ASSERT_THROW(tester.SimulateUntilMessageReceived(1000 * 1000, 2, "Disconnected device %d", FruityHal::BleHciError::CONNECTION_TIMEOUT), TimeoutException);
    }
}
#endif //GITHUB_RELEASE

TEST(TestOther, TestDataSentSplit) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    // testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert( { "prod_sink_nrf52", 1 } );
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();
    tester.SimulateUntilClusteringDone(10 * 1000);
    
    alignas(4) u8 buffer[MAX_MESH_PACKET_SIZE];

    // Put a message header in the beginning of buffer
    ConnPacketHeader* header = (ConnPacketHeader*)(buffer);
    header->messageType = MessageType::DATA_1;
    header->sender = 1;
    header->receiver = 0;

    // Put some data after header
    u8* counter = (u8*)(buffer + SIZEOF_CONN_PACKET_HEADER);
    for (uint8_t i = 0; i < MAX_MESH_PACKET_SIZE - SIZEOF_CONN_PACKET_HEADER; i++)
    {
        *counter = i;
        counter++;
    }

    char bufferHex[500];
    uint8_t len;

    // Test non split packet
    len = 10;
    {
        NodeIndexSetter setter(1);
        tester.sim->currentNode->gs.cm.SendMeshMessage(buffer, len);
    }

    Logger::ConvertBufferToBase64String(buffer, len, bufferHex, sizeof(bufferHex));
    tester.SimulateUntilMessageReceived(100 * 1000, 2, "DataSentHandler: %s", bufferHex);
    
    // Test mid-size message
    len = MAX_MESH_PACKET_SIZE / 3;
    {
        NodeIndexSetter setter(1);
        tester.sim->currentNode->gs.cm.SendMeshMessage(buffer, len);
    }

    Logger::ConvertBufferToBase64String(buffer, len, bufferHex, sizeof(bufferHex));
    tester.SimulateUntilMessageReceived(100 * 1000, 2, "DataSentHandler: %s", bufferHex);

    // Test max-size message
    len = MAX_MESH_PACKET_SIZE;
    {
        NodeIndexSetter setter(1);
        tester.sim->currentNode->gs.cm.SendMeshMessage(buffer, len);
    }

    Logger::ConvertBufferToBase64String(buffer, len, bufferHex, sizeof(bufferHex));
    tester.SimulateUntilMessageReceived(100 * 1000, 2, "DataSentHandler: %s", bufferHex);

    // Test non split packet again to make sure it will return proper data after split packet was handled
    len = 10;
    {
        NodeIndexSetter setter(1);
        tester.sim->currentNode->gs.cm.SendMeshMessage(buffer, len);
    }

    Logger::ConvertBufferToBase64String(buffer, len, bufferHex, sizeof(bufferHex));
    tester.SimulateUntilMessageReceived(100 * 1000, 2, "DataSentHandler: %s", bufferHex);
}

// Can be enabled when BR-453 is resolved
TEST(TestOther, DISABLED_TestNoPacketsDropped) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    // testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.simTickDurationMs = 15;
    simConfig.SetToPerfectConditions();
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1 });
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 15 });
    simConfig.nodeConfigName.insert({ "prod_asset_nrf52", 5 });

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();
    tester.SimulateUntilClusteringDone(50 * 1000);

    // tester.SendTerminalCommand(1, "action 0 status get_errors");
    sim_print_statistics();
    sim_clear_statistics();

    for (uint32_t i = 0; i < 5; i++)
    {
        tester.SimulateForGivenTime(60 * 1000);
        tester.SendTerminalCommand(1, "action 0 status get_errors");
    }
    tester.SimulateForGivenTime(100 * 1000);
    int errors = sim_get_statistics(Logger::GetErrorLogCustomError(CustomErrorTypes::COUNT_DROPPED_PACKETS));
    ASSERT_EQ(errors, 0);
    sim_print_statistics();
}

TEST(TestOther, TestThroughput) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    // testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.SetToPerfectConditions();
    simConfig.simTickDurationMs = 15;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1 });
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1 });

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(50 * 1000);
    tester.sim->FindNodeById(1)->gs.logger.DisableTag("CONN");
    tester.sim->FindNodeById(2)->gs.logger.DisableTag("CONN");

    tester.SendTerminalCommand(1, "action this debug flood 2 2 10000");

    // Throughput has to be at least as high as now = 3900 byte/s
    tester.SimulateUntilRegexMessageReceived(40 * 1000, 2, "Counted \\d+ flood payload bytes in \\d+ ms = \\d+ byte/s");
    {
        NodeIndexSetter setter(1);
        DebugModule* test = (DebugModule*)tester.sim->FindNodeById(2)->gs.node.GetModuleById(ModuleId::DEBUG_MODULE);
        ASSERT_TRUE(test->GetThroughputTestResult() >= 3900);
    }
}
