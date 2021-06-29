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
#include <CherrySimUtils.h>
#include <ConnectionManager.h>
#include <cmath>
#include <algorithm>
#include <regex>
#include "DebugModule.h"


//This test fixture is used to run a parametrized test based on the chosen BLE Stack
typedef struct FeaturesetAndBleStack {
    BleStackType bleStack;
    std::string featuresetName;
} FeaturesetAndBleStack;

class MultiStackFixture : public ::testing::TestWithParam<FeaturesetAndBleStack> {};

FeaturesetAndBleStack prod_mesh_nrf52 = { BleStackType::NRF_SD_132_ANY, "prod_mesh_nrf52" };



void DoClusteringTestImportedFromJson(const std::string &site, const std::string &device, u32 clusteringIterations, int maxClusteringTimeMs, u32 maxMedianClusteringTimeMs, FeaturesetAndBleStack config)
{
    int clusteringTimeTotalMs = 0;

    printf("Test with %s and stack %u" EOL, config.featuresetName.c_str(), (u32)config.bleStack);
    std::vector<u32> clusteringTimesMs;

    for (u32 i = 0; i < clusteringIterations; i++) {
        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
        simConfig.seed = i + 1;
        simConfig.importFromJson = true;
        simConfig.siteJsonPath = site;
        simConfig.devicesJsonPath = device;
        simConfig.terminalId = -1;
        //testerConfig.verbose = true;
        printf("ClusterTest Iteration %u, seed %u" EOL, i, simConfig.seed);

        simConfig.defaultBleStackType = config.bleStack;

        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
        tester.Start();
        tester.SimulateUntilClusteringDone(maxClusteringTimeMs);
        clusteringTimeTotalMs += tester.sim->simState.simTimeMs;
        clusteringTimesMs.push_back(tester.sim->simState.simTimeMs);
    }

    std::sort(clusteringTimesMs.begin(), clusteringTimesMs.end());
    u32 clusteringTimeMedianMs = clusteringTimesMs[clusteringTimesMs.size() / 2];
    if (clusteringTimeMedianMs > maxMedianClusteringTimeMs)
    {
        //The median took too long!
        SIMEXCEPTION(IllegalStateException);
    }

    printf("Average clustering time %u seconds" EOL, clusteringTimeTotalMs / clusteringIterations / 1000);
}

TEST_P(MultiStackFixture, TestBasicClustering) {
    const int maxClusteringTimeMs = 1000 * 1000;
    const int clusteringIterations = 5;
    int clusteringTimeTotalMs = 0;

    FeaturesetAndBleStack config = GetParam();
    printf("Test with %s and stack %u" EOL, config.featuresetName.c_str(), (u32)config.bleStack);

    for (u32 i = 0; i < clusteringIterations; i++) {
        printf("ClusterTest Iteration %u" EOL, i);
        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
        simConfig.seed = i + 1;
        //simConfig.verbose = true;
        //Run with the parametrized config
        simConfig.nodeConfigName.insert({ config.featuresetName.c_str(), 10 });
        simConfig.defaultBleStackType = config.bleStack;
        simConfig.terminalId = -1;

        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
        tester.Start();
        tester.SimulateUntilClusteringDone(maxClusteringTimeMs);
        clusteringTimeTotalMs += tester.sim->simState.simTimeMs;

        printf("Time %u", tester.sim->simState.simTimeMs);
    }

    printf("Average clustering time %d seconds" EOL, clusteringTimeTotalMs / clusteringIterations / 1000);
}

//Tests that the exemplary devices.json and site.json for the github release still work
TEST_P(MultiStackFixture, TestGithubExample) {
    std::string site = CherrySimUtils::GetNormalizedPath() + "/test/res/github_example/site.json";
    std::string device = CherrySimUtils::GetNormalizedPath() + "/test/res/github_example/devices.json";
    // We do not care for the exact clustering time as we only want to test that the jsons are still valid
    DoClusteringTestImportedFromJson(site, device, 1, 60 * 1000, 100000, GetParam());
}

/*Nodes in this test are spread horizontally and can be reached by more than one other nodes*/
TEST_P(MultiStackFixture, TestHorizontalSpreadNetwork) {
    std::string site = CherrySimUtils::GetNormalizedPath() + "/test/res/horizontalspreadnetwork/site.json";
    std::string device = CherrySimUtils::GetNormalizedPath() + "/test/res/horizontalspreadnetwork/devices.json";
    constexpr u32 maxRecordedClusteringMedianMs = 39650; //The maximum median recorded over 1000 different seed offsets
    DoClusteringTestImportedFromJson(site, device, 5, 600 * 1000, maxRecordedClusteringMedianMs * 2, GetParam());
}

/*Nodes in the network are densely arranged (short range) and can reach each other*/
TEST_P(MultiStackFixture, TestDenseNetwork) {
    std::string site = CherrySimUtils::GetNormalizedPath() + "/test/res/densenetwork/site.json";
    std::string device = CherrySimUtils::GetNormalizedPath() + "/test/res/densenetwork/devices.json";
    constexpr u32 maxRecordedClusteringMedianMs = 43350; //The maximum median recorded over 1000 different seed offsets
    DoClusteringTestImportedFromJson(site, device, 5, 600 * 1000, maxRecordedClusteringMedianMs * 2, GetParam());
}

/*Nodes in the network are arranged according to start topology*/
TEST_P(MultiStackFixture, TestStarNetwork) {
    std::string site = CherrySimUtils::GetNormalizedPath() + "/test/res/starnetwork/site.json";
    std::string device = CherrySimUtils::GetNormalizedPath() + "/test/res/starnetwork/devices.json";
    constexpr u32 maxRecordedClusteringMedianMs = 33450; //The maximum median recorded over 1000 different seed offsets
    DoClusteringTestImportedFromJson(site, device, 5, 90 * 1000, maxRecordedClusteringMedianMs * 2, GetParam());
}

/*Nodes in the network are arranged in line where each node can only reach one other node*/
TEST_P(MultiStackFixture, TestRowNetwork) {
    std::string site = CherrySimUtils::GetNormalizedPath() + "/test/res/rownetwork/site.json";
    std::string device = CherrySimUtils::GetNormalizedPath() + "/test/res/rownetwork/devices.json";
    constexpr u32 maxRecordedClusteringMedianMs = 44150; //The maximum median recorded over 1000 different seed offsets
    DoClusteringTestImportedFromJson(site, device, 5, 100 * 1000, maxRecordedClusteringMedianMs * 2, GetParam());
}

/*Nodes in the network are sparsely arranged and can only reach one or two other nodes */
TEST_P(MultiStackFixture, TestSparseNetwork) {
    std::string site = CherrySimUtils::GetNormalizedPath() + "/test/res/sparsenetwork/site.json";
    std::string device = CherrySimUtils::GetNormalizedPath() + "/test/res/sparsenetwork/devices.json";
    constexpr u32 maxRecordedClusteringMedianMs = 46350; //The maximum median recorded over 1000 different seed offsets
    DoClusteringTestImportedFromJson(site, device, 5, 500 * 1000, maxRecordedClusteringMedianMs * 2, GetParam());
}

/*Nodes are densely arranged on left and right side of the network but has only one node in the middle to connect both sides*/
TEST_P(MultiStackFixture, TestSinglePointFailureNetwork) {
    std::string site = CherrySimUtils::GetNormalizedPath() + "/test/res/singlepointfailure/site.json";
    std::string device = CherrySimUtils::GetNormalizedPath() + "/test/res/singlepointfailure/devices.json";
    constexpr u32 maxRecordedClusteringMedianMs = 48500; //The maximum median recorded over 1000 different seed offsets
    DoClusteringTestImportedFromJson(site, device, 5, 1000 * 1000, maxRecordedClusteringMedianMs * 2, GetParam());
}

TEST(TestClustering, TestClusteringWithManySdBusy) {
    int clusteringTimeTotalMs = 0;
    const int maxClusteringTimeMs = 10000 * 1000; //Yes, this massive timeout is necessary. It was tested with a lot of seeds, this timeout is the smallest power of ten that did not fail.
    const int clusteringIterations = 5;

    for (u32 i = 0; i < clusteringIterations; i++) {
        printf("ClusterTest Iteration %u" EOL, i);
        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
        simConfig.seed = i + 1;
        simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 10 });
        simConfig.sdBusyProbability = UINT32_MAX / 2;
        simConfig.terminalId = -1;
        //testerConfig.verbose = true;

        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
        tester.Start();
        tester.SimulateUntilClusteringDone(maxClusteringTimeMs);
        clusteringTimeTotalMs += tester.sim->simState.simTimeMs;
    }

    printf("Average clustering time %d seconds" EOL, clusteringTimeTotalMs / clusteringIterations / 1000);
}

TEST(TestClustering, TestMessagesInOrder) {
    // This test makes sure that messages from the same priority (in this case the priority of raw_data_light)
    // are always received in the order in which they were sent, without a message overtaking a previous message.
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    //FIXME: IOT-4648 - seed had to be changed to workaround this rare, but still valid issue
    simConfig.seed = 2;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1 });
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 10 });

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();
    tester.SimulateUntilClusteringDone(1000 * 1000);

    u32 failCounter = 0;
    u32 rawDataLightCounter = 1;
    u32 biggestReceivedVal = 0;

    for (int repeats = 0; repeats < 1000; repeats++)
    {
        for (u32 k = 0; k < 512; k++)
        {
            const u32 currentCounter = rawDataLightCounter;
            rawDataLightCounter++;
            char buffer[128] = {};
            Logger::ConvertBufferToHexString((u8*)&currentCounter, sizeof(currentCounter), buffer, sizeof(buffer));
            tester.SendTerminalCommand(1, (std::string("raw_data_light 10 0 0 ") + buffer).c_str());
        }
        const std::string regexString = "\\{\"nodeId\":1,\"type\":\"raw_data_light\",\"module\":0,\"protocol\":0,\"payload\":\"([A-Za-z0-9+/=]*?)\",\"requestHandle\":0\\}";
        std::vector<SimulationMessage> msgs = { SimulationMessage(10, regexString) };
        try
        {
            Exceptions::DisableDebugBreakOnException disabler;
            tester.SimulateUntilRegexMessagesReceived(10 * 1000, msgs);
            std::string completedMessage = msgs[0].GetCompleteMessage();

            std::regex reg(regexString);
            std::smatch matches;
            std::regex_search(completedMessage, matches, reg);
            u32 val = 0;
            bool didError = false;
            Logger::ParseEncodedStringToBuffer(matches[1].str().c_str(), (u8*)&val, sizeof(val), &didError);
            if (didError)
            {
                SIMEXCEPTION(IllegalStateException);
            }
            if (val <= biggestReceivedVal)
            {
                // A message has overtaken a previous message!
                SIMEXCEPTION(IllegalStateException);
            }
            biggestReceivedVal = val;
        }
        catch (TimeoutException& e)
        {
            // Messages may get dropped and thus lead to time outs. This is fine however, as long as these timeouts don't happen too often.
            failCounter++;
            if (failCounter > 100) SIMEXCEPTION(IllegalStateException);
        }
        
    }

}

TEST_P(MultiStackFixture, TestBasicClusteringWithNodeReset) {
    const int maxClusteringTimeMs = 1500 * 1000; //May require an emergency disconnect some times, thus such a high value.
    const int clusteringIterations = 5;
    const int resetTimesPerIteration = 3;

    FeaturesetAndBleStack config = GetParam();
    printf("Test with %s and stack %u" EOL, config.featuresetName.c_str(), (u32)config.bleStack);

    for (u32 i = 0; i < clusteringIterations; i++) {
        printf("ClusterTest Iteration %u" EOL, i);
        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
        simConfig.connectionTimeoutProbabilityPerSec = 0; //TODO: do this test with and without timeouts
        simConfig.seed = i + 1;

        //testerConfig.verbose = true;

        //Run with the parametrized config
        simConfig.defaultBleStackType = config.bleStack;

        simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", 20} );

        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
        tester.Start();

        tester.SimulateUntilClusteringDone(maxClusteringTimeMs);

        for (u32 j = 0; j < resetTimesPerIteration; j++)
        {
            u32 numNodesToReset = (u32)(PSRNGINT(0, (tester.sim->GetTotalNodes() / 5)) + 1);

            auto nodeIdsToReset = CherrySimUtils::GenerateRandomNumbers(1, tester.sim->GetTotalNodes(), numNodesToReset);

            for (auto const nodeId : nodeIdsToReset) {
                tester.SendTerminalCommand(nodeId, "reset");
            }

            //We simulate for some time so that the reset command is processed before waiting for clustering
            tester.SimulateForGivenTime(1 * 1000);
            tester.SimulateUntilClusteringDone(maxClusteringTimeMs);
        }
    }
}

//Tests different clustering scenarios
TEST(TestClustering, TestBasicClusteringWithNodeReset_scheduled) {
    int maxClusteringTimeMs = 250 * 1000;
    const int clusteringIterations = 30;
    const int resetTimesPerIteration = 3;

    //Exceptions::DisableDebugBreakOnException disabler;

    u32 seed = (u32)time(NULL);
    
    for (u32 i = 0; i < clusteringIterations; i++) {
        seed++;
        u32 numNodes = seed % 200 + 2;

        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();

        //The following block can be commented in for debugging a specific scenario
        //numNodes = 197;
        //seed = 123;
        //k = 1;
        //maxClusteringTimeMs = 0;
        //testerConfig.verbose = true;
        //simConfig.terminalId = 0;
        //simConfig.enableClusteringValidityCheck = true;

        simConfig.mapWidthInMeters = 400;
        simConfig.mapHeightInMeters = 300;
        simConfig.seed = seed;
        simConfig.simulateJittering = true;

        
        simConfig.defaultBleStackType = prod_mesh_nrf52.bleStack;

        printf("%u: Starting clustering with %u nodes, seed %u, featureset %s" EOL, i, numNodes, simConfig.seed, "prod_mesh_nrf52");
        simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", numNodes } );


        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
        tester.Start();

        tester.SimulateUntilClusteringDone(maxClusteringTimeMs);
        int lastClusterDoneTime = tester.sim->simState.simTimeMs;

        printf("Clustering for %s done in %u seconds" EOL, "prod_mesh_nrf52", tester.sim->simState.simTimeMs / 1000);

        for (u32 j = 0; j < resetTimesPerIteration; j++)
        {
            u32 numNodesToReset = (u32)(PSRNGINT(0, (numNodes / 5)) + 1);

            auto nodeIdsToReset = CherrySimUtils::GenerateRandomNumbers(1, numNodes, numNodesToReset);

            for (auto const nodeId : nodeIdsToReset) {
                tester.SendTerminalCommand(nodeId, "reset");
            }

            //We simulate for some time so that the reset command is processed before waiting for clustering
            tester.SimulateForGivenTime(1 * 1000);

            //try {
                tester.SimulateUntilClusteringDone(maxClusteringTimeMs);
            //}
            //catch (TimeoutException e) {
            //    printf("EXCEPTION TimeoutException" EOL);
            //}

            printf("Clustering after reset after %u seconds" EOL, (tester.sim->simState.simTimeMs - lastClusterDoneTime) / 1000);
            lastClusterDoneTime = tester.sim->simState.simTimeMs;
        }
    }
}

//FIXME: This will corrently result in an error
// Tests reestablishing against errors, first we simulate connection timeouts for a period of 60 seconds
// where some nodes will need to reestablish their connection, then we stop simulating timeouts and check
//If the cluster is still valid
TEST(TestClustering, TestBasicClusteringWithNodeResetAndConnectionTimeouts_scheduled) {
    int maxClusteringTimeMs = 500 * 1000;
    const int clusteringIterations = 30;
    const int resetTimesPerIteration = 3;

    //Exceptions::DisableDebugBreakOnException disabler;

    u32 seed = (u32)time(NULL);

    for (u32 i = 0; i < clusteringIterations; i++) {
        seed++;
        u32 numNodes = seed % 50 + 4;

        printf("ClusterTest Iteration %u" EOL, i);
        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();

        //The following block can be commented in for debugging a specific scenario
        //numNodes = 50;
        //seed = 1562245796;
        //maxClusteringTimeMs = 0;
        //testerConfig.verbose = true;
        //simConfig.terminalId = 0;
        //simConfig.enableClusteringValidityCheck = true;

        simConfig.mapWidthInMeters = 400;
        simConfig.mapHeightInMeters = 300;
        simConfig.connectionTimeoutProbabilityPerSec = 0.0005 * UINT32_MAX;
        simConfig.seed = seed;
        simConfig.simulateJittering = true;

        simConfig.defaultBleStackType = prod_mesh_nrf52.bleStack;

        printf("Starting clustering with %u nodes, seed %u, featureset %s" EOL, numNodes, simConfig.seed, "prod_mesh_nrf52");

        simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", numNodes } );

        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
        tester.Start();

        tester.SimulateForGivenTime(60 * 1000);

        //Disable timouts after some time so that we can check against clustering timeouts without them solveing themselves after some time
        tester.sim->simConfig.connectionTimeoutProbabilityPerSec = 0;

        tester.SimulateUntilClusteringDone(maxClusteringTimeMs);
        int lastClusterDoneTime = tester.sim->simState.simTimeMs;

        printf("Clustering for %s done in %u seconds" EOL, "prod_mesh_nrf52", tester.sim->simState.simTimeMs / 1000);

        for (u32 j = 0; j < resetTimesPerIteration; j++)
        {
            u32 numNodesToReset = (u32)(PSRNGINT(0, std::ceil(numNodes / 5.0)) + 1);

            auto nodeIdsToReset = CherrySimUtils::GenerateRandomNumbers(1, numNodes, numNodesToReset);

            for (auto const nodeId : nodeIdsToReset) {
                tester.SendTerminalCommand(nodeId, "reset");
            }

            //We simulate for some time so that the reset command is processed before waiting for clustering
            tester.SimulateForGivenTime(1 * 1000);

            //try {
            tester.SimulateUntilClusteringDone(maxClusteringTimeMs);
            //}
            //catch (TimeoutException e) {
            //    printf("EXCEPTION TimeoutException" EOL);
            //}

            printf("Clustering after reset after %u seconds" EOL, (tester.sim->simState.simTimeMs - lastClusterDoneTime) / 1000);
            lastClusterDoneTime = tester.sim->simState.simTimeMs;
        }
    }
}

//Test if meshing works if we put load on the network
TEST(TestClustering, TestMeshingUnderLoad) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", 50 } );
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    //Instruct all nodes to flood the network using FLOOD_MODE_UNRELIABLE_SPLIT
    for (u32 i = 0; i < tester.sim->GetTotalNodes(); i++) {
        tester.SendTerminalCommand(i+1, "action this debug flood 0 4 10000 50000");
    }
    tester.SimulateUntilClusteringDone(1000 * 1000);

    printf("Clustering under load took %u seconds", tester.sim->simState.simTimeMs / 1000);
}

TEST(TestClustering, SimulateLongevity_long) {
    u32 numIterations = 10;

    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", 50 } );
    simConfig.connectionTimeoutProbabilityPerSec = 0.00001 * UINT32_MAX;
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);

    for (u32 i = 0; i < numIterations; i++) {
        tester.SimulateForGivenTime(20 * 1000);
        tester.SendTerminalCommand(1, "action 0 status get_device_info");
        tester.SimulateForGivenTime(20 * 1000);
        tester.SendTerminalCommand(1, "action 0 status get_errors");
    }

    //TODO: Should check how many disconnects we have, best to have no disconnects, then check
    //if there are any dropped messages
}

i32 determineHopsToSink(NodeEntry* node, std::vector<NodeEntry*>& visitedNodes)
{
    NodeIndexSetter setter(node->index);
    visitedNodes.push_back(node);
    if (GET_DEVICE_TYPE() == DeviceType::SINK) {
        return 0;
    }

    for (int i = 0; i < cherrySimInstance->currentNode->state.configuredTotalConnectionCount; i++) {
        SoftdeviceConnection* c = &(node->state.connections[i]);
        //Although FruityMesh connections can never run in a circle, SoftdeviceConnections can! Thus we have to store all previously visited nodes to avoid stack overflows.
        if (c->connectionActive && std::find(visitedNodes.begin(), visitedNodes.end(), c->partner) == visitedNodes.end()) {
            BaseConnection* conn = GS->cm.GetConnectionFromHandle(c->connectionHandle).GetConnection();
            if (conn && conn->connectionType == ConnectionType::FRUITYMESH && conn->connectionState >= ConnectionState::HANDSHAKE_DONE && conn->connectionState <= ConnectionState::REESTABLISHING_HANDSHAKE)
            {
                i32 tmp = determineHopsToSink(c->partner, visitedNodes);
                if (tmp >= 0) {
                    return tmp + 1;
                }
            }
        }
    }
    return -1;
}

i32 determineHopsToSink(NodeEntry* node)
{
    std::vector<NodeEntry*> visitedNodes;
    return determineHopsToSink(node, visitedNodes);
}

TEST_P(MultiStackFixture, TestSinkDetectionWithSingleSink)
{
    FeaturesetAndBleStack config = GetParam();
    printf("Test with %s and stack %u" EOL, config.featuresetName.c_str(), (u32)config.bleStack);

    for(int seed=50; seed< 55; seed++) {
        printf("Seed is %d" EOL, seed);

        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        //testerConfig.verbose = true;
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
        simConfig.terminalId = -1;
        simConfig.seed = seed;

        //Run with the parametrized config
        simConfig.defaultBleStackType = config.bleStack;

        simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
        simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 19 });

        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

        tester.Start();

        tester.sim->simConfig.terminalId = 0;
        tester.SendTerminalCommand(0, "debug flash");
        tester.SimulateGivenNumberOfSteps(1);
        tester.SendTerminalCommand(0, "debug maconn");
        tester.SimulateGivenNumberOfSteps(1);
        tester.SendTerminalCommand(0, "debug sec");
        tester.SimulateGivenNumberOfSteps(1);
        tester.SendTerminalCommand(0, "debug conn");
        tester.SimulateGivenNumberOfSteps(1);
        tester.SendTerminalCommand(0, "debug node");
        tester.SimulateGivenNumberOfSteps(1);
        tester.SendTerminalCommand(0, "debug rconn");
        tester.SimulateGivenNumberOfSteps(1);
        tester.SendTerminalCommand(0, "debug statusmod");
        tester.SimulateGivenNumberOfSteps(1);
        
        //Disable terminal again
        tester.sim->simConfig.terminalId = -1;

        tester.SimulateUntilClusteringDone(1000*1000);

        // Give some additional time after clustering as a sink update packet
        // (clusterUpdateInfo) might still be on the way
        tester.SimulateForGivenTime(5000);

        //Check if all nodes have correctly calculated their hops to the sink
        for (u32 i = 0; i < tester.sim->GetTotalNodes(); i++) {
            NodeIndexSetter setter(i);
            NodeEntry* node = &(tester.sim->nodes[i]);

            //Determine the number of hops to the sink according to the simulator connections
            i32 hopsToSink = determineHopsToSink(node);

            //printf("Node %u has %d hops to sink " EOL, node->id, hopsToSink);

            //Next, check if the number of hops that each node has saved for its connections matches
            MeshConnections conns = GS->cm.GetMeshConnections(ConnectionDirection::INVALID);
            bool hopsFound = false;
            for (int j = 0; j < conns.count; j++) {
                MeshConnection* conn = conns.handles[j].GetConnection();

                //printf("   %d" EOL, conn->hopsToSink);

                if (!hopsFound) {
                    if (hopsToSink == conn->hopsToSink) hopsFound = true;
                    else if (conn->hopsToSink != -1) {
                        FAIL() << "Hops to sink not correct for node " << node->id;
                    }
                }
                else {
                    if (conn->hopsToSink != -1) {
                        FAIL() << "Another connection already had hops to sink";
                    }
                }
                
            }
            if (GET_DEVICE_TYPE() == DeviceType::SINK) {
                //The sink will not have the number of hops saved as all its connections point to non-sinks
                //It will therefore have -1 on all connections, so it is correct if we do not find the hops
                if (hopsFound) {
                    FAIL() << "Sink must not have hops to itself";
                }
            }
            else if (!hopsFound) {
                FAIL() << "Wrong hop amount to sink for node " << node->id;
            }
        }
    }
}

//This test was written to make sure that the mesh can still cluster if the vital queue is flooded
//with other packets.
TEST(TestClustering, TestVitalPrioQueueFull) {
    for (int seed = 0; seed < 3; seed++) {
        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        //testerConfig.verbose = true;
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
        simConfig.terminalId = 0;
        simConfig.seed = seed + 1;

        simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", 20 } );
        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

        tester.Start();

        //Give the simulation some time so that we are in a half done clustering state.
        tester.SimulateGivenNumberOfSteps(30);

        NodeIndexSetter setter(0);
        DebugModule* mod = (DebugModule*)GS->node.GetModuleById(ModuleId::DEBUG_MODULE);
        ASSERT_TRUE(mod != nullptr);
        tester.SimulateUntilClusteringDone(1000 * 1000 /*Such a high value because for some RNG seeds an emergency disconnect might be required which takes quite a bit of time.*/,
            [&]() {
                NodeIndexSetter setter(0);
                mod->SendQueueFloodMessage(DeliveryPriority::VITAL);
                mod->SendQueueFloodMessage(DeliveryPriority::VITAL);
                mod->SendQueueFloodMessage(DeliveryPriority::VITAL);
                mod->SendQueueFloodMessage(DeliveryPriority::VITAL);
            });
    }
    //We check that we indeed generated enough packets to fill up the queue. If this test fails, it might mean that message transmission was optimized
    //and we are not properly flooding with enough packets in the lines above.
    ASSERT_TRUE(sim_get_statistics("vitalPrioQueueFull") > 3);
}

TEST(TestClustering, TestInfluceOfNodeWithWrongNetworkKey) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();

    simConfig.connectionTimeoutProbabilityPerSec = 0.001 * UINT32_MAX;

    //testerConfig.verbose = true;
    testerConfig.terminalFilter = 1;
    simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", 5 } );
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    Exceptions::ExceptionDisabler<ErrorLoggedException> ele; //Due to the wrong network key, some absolutely sane errors might be logged. These however should not fail our test.

    //Enroll node 2 in same network with different networkkey
    tester.SendTerminalCommand(3, "action this enroll basic BBBBD 2 %u 11:99:99:99:99:99:99:99:99:99:99:99:99:99:99:99 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 03:00:00:00:03:00:00:00:03:00:00:00:03:00:00:00 10 0", simConfig.defaultNetworkId);
    tester.SimulateUntilMessageReceived(10 * 1000, 3, "reboot");
    tester.SendTerminalCommand(3, "action this enroll basic BBBBD 2 %u 11:99:99:99:99:99:99:99:99:99:99:99:99:99:99:99 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 03:00:00:00:03:00:00:00:03:00:00:00:03:00:00:00 10 0", simConfig.defaultNetworkId);
    tester.SimulateUntilMessageReceived(100 * 1000, 3, "enroll_response");

    //Simulate for 200 seconds and see if any exception occurs
    //Reset some nodes every once in a while (connection loss is also included)
    while (tester.sim->simState.simTimeMs < 200 * 1000) {
        tester.SimulateForGivenTime(10 * 1000);

        u32 numNodesToReset = (u32)(PSRNGINT(0, (tester.sim->GetTotalNodes() / 2)));

        auto nodeIdsToReset = CherrySimUtils::GenerateRandomNumbers(1, tester.sim->GetTotalNodes(), numNodesToReset);

        for (auto const nodeId : nodeIdsToReset) {
            tester.SendTerminalCommand(nodeId, "reset");
        }
    }
}

TEST(TestClustering, TestEmergencyDisconnect) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    u32 numNodes = 10;
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    
    //Place Node 0 in the middle and the others in a circle around it.
    simConfig.preDefinedPositions.push_back({ 0.5, 0.5 });
    for (u32 i = 1; i < numNodes; i++)
    {
        double percentage = (double)i / (double)(numNodes - 1);
        simConfig.preDefinedPositions.push_back({
            std::sin(percentage * 3.14 * 2) * 0.1 + 0.5,
            std::cos(percentage * 3.14 * 2) * 0.1 + 0.5,
        });
    }
    simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", numNodes } );
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    //All the nodes can connect to Node 0, but none of them can connect to each other.
    for (u32 i = 1; i < numNodes; i++)
    {
        for (u32 k = 1; k < numNodes; k++)
        {
            tester.sim->nodes[i].impossibleConnection.push_back(k);
        }
    }

    //Make sure that emergency disconnects happen regularly.
    for (u32 i = 0; i < 3; i++)
    {
        tester.SimulateUntilMessageReceived(200 * 1000, 1, "Emergency disconnect");
    }
}

TEST(TestClustering, TestFakedJoinMeAffectOnClustering) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();

    simConfig.SetToPerfectConditions();
    // testerConfig.verbose = true;
    // testerConfig.terminalFilter = 1;
    simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", 5 } );
    // Place nodes close to each other so that faked JOIN_ME packets would affect all the nodes. 
    simConfig.preDefinedPositions = { {0.5, 0.5}, {0.55, 0.5}, {0.5, 0.55}, {0.55, 0.55}, {0.5, 0.45} };
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    Exceptions::ExceptionDisabler<ErrorLoggedException> ele; //Due to the wrong network key, some absolutely sane errors might be logged. These however should not fail our test.

    //Enroll node 2 in same network with different networkkey
    tester.SendTerminalCommand(3, "action this enroll basic BBBBD 2 %u 11:99:99:99:99:99:99:99:99:99:99:99:99:99:99:99 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 03:00:00:00:03:00:00:00:03:00:00:00:03:00:00:00 10 0", simConfig.defaultNetworkId);
    tester.SimulateUntilMessageReceived(10 * 1000, 3, "reboot");
    tester.SendTerminalCommand(3, "action this enroll basic BBBBD 2 %u 11:99:99:99:99:99:99:99:99:99:99:99:99:99:99:99 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 03:00:00:00:03:00:00:00:03:00:00:00:03:00:00:00 10 0", simConfig.defaultNetworkId);
    tester.SimulateUntilMessageReceived(100 * 1000, 3, "enroll_response");

    u16 nodeId = 3;
    u16 clusterSize = 1;
    u32 clusterId = simConfig.defaultNetworkId;
    u32 failCounter = 0;
    bool clusteringDone = false;
    tester.SendTerminalCommand(3, "action this adv add 02:01:06:1B:FF:4D:02:F0:0A:00:01:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:19:00:04:01:FF:FF:01:00:00:00:00:00 10", 
                                                                (u8)(nodeId & 0xFF), (u8)((nodeId >> 8) & 0xFF),
                                                                (u8)(clusterId & 0xFF), (u8)((clusterId >> 8) & 0xFF), (u8)((clusterId >> 16) & 0xFF), (u8)((clusterId >> 24) & 0xFF),
                                                                (u8)(clusterSize & 0xFF), (u8)((clusterSize >> 8) & 0xFF));

    // TEST #1
    // Track node 4 cluster information and make node 3 advertise fake JOIN_ME data
    
    // Simulation time for this subtest
    u32 timeoutSec = tester.sim->simState.simTimeMs + 100 * 1000;

    // Expected clustering time
    u32 clusteringTimeoutSec = tester.sim->simState.simTimeMs + 20 * 1000;
    
    // reset all nodes to cause reclustering
    tester.SendTerminalCommand(0, "reset");

    failCounter = 0;
    while (tester.sim->simState.simTimeMs < timeoutSec) {
        if (tester.sim->simState.simTimeMs >= clusteringTimeoutSec){
            uint8_t clusteredNodesCounter = 0;
            // Check if clustering was disturbed by fake JOIN_ME packet.
            for (int i = 0; i < 5; i++)
            {
                NodeIndexSetter setter(i);
                if (i == 2)
                {
                    ASSERT_EQ(tester.sim->currentNode->gs.node.GetClusterSize(), 1);
                }
                else
                {
                    if (clusteringDone)
                    {
                        ASSERT_EQ(tester.sim->currentNode->gs.node.GetClusterSize(), 4);
                    }
                    else
                    {
                        if (tester.sim->currentNode->gs.node.GetClusterSize() == 4) clusteredNodesCounter++;
                    }
                }
            }
            if (clusteredNodesCounter == 4) clusteringDone = true;
        }

        // Set up fake JOIN_ME packet for node 3
        {
            nodeId = 4;
            NodeIndexSetter setter(3);
            clusterSize = tester.sim->currentNode->gs.node.GetClusterSize();
            clusterId = tester.sim->currentNode->gs.node.clusterId;
        }

        tester.SendTerminalCommand(3, "action this adv set 0 02:01:06:1B:FF:4D:02:F0:0A:00:01:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:19:00:04:01:FF:FF:01:00:00:00:00:00 10", 
                                                                (u8)(nodeId & 0xFF), (u8)((nodeId >> 8) & 0xFF),
                                                                (u8)(clusterId & 0xFF), (u8)((clusterId >> 8) & 0xFF), (u8)((clusterId >> 16) & 0xFF), (u8)((clusterId >> 24) & 0xFF),
                                                                (u8)(clusterSize & 0xFF), (u8)((clusterSize >> 8) & 0xFF));
        tester.SimulateForGivenTime(5 * 1000);

        // Check if all the nodes are getting fake JOIN_ME packets
        for (int i = 1; i < 6; i++)
        {
            if (i == 3) continue;
            tester.SendTerminalCommand(i, "bufferstat");
            try
            {
                Exceptions::DisableDebugBreakOnException disabler;
                tester.SimulateUntilMessageReceived(500, i, "=> %d, clstId:%u, clstSize:%d", nodeId, clusterId, clusterSize);
            }
            catch (TimeoutException& e)
            {
                // Not all the nodes may receive fake JOIN_ME packets everytime, we accept that sometimes nodes are not getting those. 
                failCounter++;
                if (failCounter > 100) SIMEXCEPTION(IllegalStateException);
            }
        }
    }
    ASSERT_TRUE(clusteringDone);

    // TEST #2
    // Fake being a bigger cluster
    
    // Simulation time for this subtest
    timeoutSec = tester.sim->simState.simTimeMs + 100 * 1000;

    // Expected clustering time
    clusteringTimeoutSec = tester.sim->simState.simTimeMs + 20 * 1000;

    // reset all nodes to cause reclustering
    tester.SendTerminalCommand(0, "reset");

    // Set up fake JOIN_ME packet for node 3
    nodeId = 3;
    clusterSize = 10;
    clusterId = 0x01ABCDEF;
    tester.SendTerminalCommand(3, "action this adv set 0 02:01:06:1B:FF:4D:02:F0:0A:00:01:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:19:00:04:01:FF:FF:01:00:00:00:00:00 10", 
                                                            (u8)(nodeId & 0xFF), (u8)((nodeId >> 8) & 0xFF),
                                                            (u8)(clusterId & 0xFF), (u8)((clusterId >> 8) & 0xFF), (u8)((clusterId >> 16) & 0xFF), (u8)((clusterId >> 24) & 0xFF),
                                                            (u8)(clusterSize & 0xFF), (u8)((clusterSize >> 8) & 0xFF));

    failCounter = 0;
    clusteringDone = false;
    while (tester.sim->simState.simTimeMs < timeoutSec) {
        if (tester.sim->simState.simTimeMs >= clusteringTimeoutSec){
            uint8_t clusteredNodesCounter = 0;
            // Check if clustering was disturbed by fake JOIN_ME packet.
            for (int i = 0; i < 5; i++)
            {
                NodeIndexSetter setter(i);
                if (i == 2)
                {
                    ASSERT_EQ(tester.sim->currentNode->gs.node.GetClusterSize(), 1);
                }
                else
                {
                    if (clusteringDone)
                    {
                        ASSERT_EQ(tester.sim->currentNode->gs.node.GetClusterSize(), 4);
                    }
                    else
                    {
                        if (tester.sim->currentNode->gs.node.GetClusterSize() == 4) clusteredNodesCounter++;
                    }
                }
            }
            if (clusteredNodesCounter == 4) clusteringDone = true;
        }
        tester.SimulateForGivenTime(5 * 1000);

        // Check if all the nodes are getting fake JOIN_ME packets
        for (int i = 1; i < 6; i++)
        {
            if (i == 3) continue;
            tester.SendTerminalCommand(i, "bufferstat");
            try
            {
                Exceptions::DisableDebugBreakOnException disabler;
                tester.SimulateUntilMessageReceived(500, i, "=> %d, clstId:%u, clstSize:%d", nodeId, clusterId, clusterSize);
            }
            catch (TimeoutException& e)
            {
                // Not all the nodes may receive fake JOIN_ME packets everytime, we accept that sometimes nodes are not getting those. 
                failCounter++;
                if (failCounter > 100) SIMEXCEPTION(IllegalStateException);
            }
        }
    }
    ASSERT_TRUE(clusteringDone);
    

    // TEST #3
    // Follow other nodes cluster information and advertise the same with different node id
    
    // Simulation time for this subtest
    timeoutSec = tester.sim->simState.simTimeMs + 100 * 1000;

    // Expected clustering time
    clusteringTimeoutSec = tester.sim->simState.simTimeMs + 30 * 1000;
    
    // reset all nodes to cause reclustering
    tester.SendTerminalCommand(0, "reset");

    static uint8_t currentFollowedNodeId = 0;
    failCounter = 0;
    clusteringDone = false;
    while (tester.sim->simState.simTimeMs < timeoutSec) {
        if (tester.sim->simState.simTimeMs >= clusteringTimeoutSec){
            uint8_t clusteredNodesCounter = 0;
            // Check if clustering was disturbed by fake JOIN_ME packet.
            for (int i = 0; i < 5; i++)
            {
                NodeIndexSetter setter(i);
                if (i == 2)
                {
                    ASSERT_EQ(tester.sim->currentNode->gs.node.GetClusterSize(), 1);
                }
                else
                {
                    if (clusteringDone)
                    {
                        ASSERT_EQ(tester.sim->currentNode->gs.node.GetClusterSize(), 4);
                    }
                    else
                    {
                        if (tester.sim->currentNode->gs.node.GetClusterSize() == 4) clusteredNodesCounter++;
                    }
                }
            }
            if (clusteredNodesCounter == 4) clusteringDone = true;
        }

        // Set up fake JOIN_ME packet for node 3
        {
            NodeIndexSetter setter(currentFollowedNodeId);
            nodeId = currentFollowedNodeId + 1;
            clusterSize = tester.sim->currentNode->gs.node.GetClusterSize();
            clusterId = tester.sim->currentNode->gs.node.clusterId;

            currentFollowedNodeId++;
            // Skip attacking node
            if (currentFollowedNodeId == 3) currentFollowedNodeId++;
            currentFollowedNodeId %= 5;
        }

        tester.SendTerminalCommand(3, "action this adv set 0 02:01:06:1B:FF:4D:02:F0:0A:00:01:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:19:00:04:01:FF:FF:01:00:00:00:00:00 10", 
                                                                (u8)(nodeId & 0xFF), (u8)((nodeId >> 8) & 0xFF),
                                                                (u8)(clusterId & 0xFF), (u8)((clusterId >> 8) & 0xFF), (u8)((clusterId >> 16) & 0xFF), (u8)((clusterId >> 24) & 0xFF),
                                                                (u8)(clusterSize & 0xFF), (u8)((clusterSize >> 8) & 0xFF));
        tester.SimulateForGivenTime(5 * 1000);

        // Check if all the nodes are getting fake JOIN_ME packets
        for (int i = 1; i < 6; i++)
        {
            if (i == 3) continue;
            tester.SendTerminalCommand(i, "bufferstat");
            try
            {
                Exceptions::DisableDebugBreakOnException disabler;
                tester.SimulateUntilMessageReceived(500, i, "=> %d, clstId:%u, clstSize:%d", nodeId, clusterId, clusterSize);
            }
            catch (TimeoutException& e)
            {
                // Not all the nodes may receive fake JOIN_ME packets everytime, we accept that sometimes nodes are not getting those. 
                failCounter++;
                if (failCounter > 100) SIMEXCEPTION(IllegalStateException);
            }
        }
    }
    ASSERT_TRUE(clusteringDone);
    

    // TEST #4
    // Change JOIN_ME packet very often to generate a lot of traffic.
    
    // Simulation time for this subtest
    timeoutSec = tester.sim->simState.simTimeMs + 100 * 1000;

    // Expected clustering time
    clusteringTimeoutSec = tester.sim->simState.simTimeMs + 30 * 1000;
    
    // reset all nodes to cause reclustering
    tester.SendTerminalCommand(0, "reset");

    failCounter = 0;
    clusteringDone = false;
    while (tester.sim->simState.simTimeMs < timeoutSec) {
        if (tester.sim->simState.simTimeMs >= clusteringTimeoutSec){
            uint8_t clusteredNodesCounter = 0;
            // Check if clustering was disturbed by fake JOIN_ME packet.
            for (int i = 0; i < 5; i++)
            {
                NodeIndexSetter setter(i);
                if (i == 2)
                {
                    ASSERT_EQ(tester.sim->currentNode->gs.node.GetClusterSize(), 1);
                }
                else
                {
                    if (clusteringDone)
                    {
                        ASSERT_EQ(tester.sim->currentNode->gs.node.GetClusterSize(), 4);
                    }
                    else
                    {
                        if (tester.sim->currentNode->gs.node.GetClusterSize() == 4) clusteredNodesCounter++;
                    }
                }
            }
            if (clusteredNodesCounter == 4) clusteringDone = true;
        }

        // Set up fake JOIN_ME packet for node 3
        {
            nodeId += 1;
            clusterSize += 1;
            clusterId += 10;
        }

        {
            NodeIndexSetter setter(2);
            tester.sim->currentNode->gs.node.configuration.nodeId = nodeId;
        }

        tester.SendTerminalCommand(3, "action this adv set 0 02:01:06:1B:FF:4D:02:F0:0A:00:01:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:19:00:04:01:FF:FF:01:00:00:00:00:00 10", 
                                                                (u8)(nodeId & 0xFF), (u8)((nodeId >> 8) & 0xFF),
                                                                (u8)(clusterId & 0xFF), (u8)((clusterId >> 8) & 0xFF), (u8)((clusterId >> 16) & 0xFF), (u8)((clusterId >> 24) & 0xFF),
                                                                (u8)(clusterSize & 0xFF), (u8)((clusterSize >> 8) & 0xFF));

        // Shorter simulation steps to change JOIN_ME more often
        tester.SimulateForGivenTime(5 * 100);

        // Check if all the nodes are getting fake JOIN_ME packets
        for (int i = 1; i < 6; i++)
        {
            if (i == 3) continue;
            tester.SendTerminalCommand(i, "bufferstat");
            try
            {
                Exceptions::DisableDebugBreakOnException disabler;
                tester.SimulateUntilMessageReceived(500, i, "=> %d, clstId:%u, clstSize:%d", nodeId, clusterId, clusterSize);
            }
            catch (TimeoutException& e)
            {
                // Not all the nodes may receive fake JOIN_ME packets everytime, we accept that sometimes nodes are not getting those. 
                failCounter++;
                if (failCounter > 200) SIMEXCEPTION(IllegalStateException);
            }
        }
    }
    ASSERT_TRUE(clusteringDone);
}

//This tests that our implementation does not break if methods return busy that are very unlikely to return busy
//based on our tests in a live network
TEST(TestClustering, TestUnlikelySdBusy) {

    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 10 });
    simConfig.sdBusyProbabilityUnlikely = 0.2 * UINT32_MAX;
    simConfig.connectionTimeoutProbabilityPerSec = 0.001 * UINT32_MAX;
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    //Simulate for some time and expect that some errors will have been generated in that time
    tester.SimulateForGivenTime(30 * 1000);

    //We enhance our meshing conditions to make sure a mesh can be built
    simConfig.SetToPerfectConditions();

    //We should be able to build a functional mesh even with unlikely errors that happened before
    tester.SimulateUntilClusteringDone(100 * 1000);

    //Request the Error Log
    tester.SendTerminalCommand(1, "action 0 status get_errors");

    //This error log should contain at least one failed MTU upgrade which happens with the sdBusyProbabilityUnlikely
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "FATAL_MTU_UPGRADE_FAILED");
}


//TODO: Write a test that checks reestablishing while the mesh is flooded

//This executes all MultiStackFixture Tests with the S130 and S132 stacks
INSTANTIATE_TEST_SUITE_P(TestClustering, MultiStackFixture,
    ::testing::Values(prod_mesh_nrf52));
