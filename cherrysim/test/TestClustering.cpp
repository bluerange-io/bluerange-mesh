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
#include <CherrySimTester.h>
#include <CherrySimUtils.h>
#include <ConnectionManager.h>
#include <cmath>
#include <algorithm>


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
    DoClusteringTestImportedFromJson(site, device, 5, 60 * 1000, maxRecordedClusteringMedianMs * 2, GetParam());
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
        simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", 10 } );
        simConfig.sdBusyProbability = UINT32_MAX / 2;
        simConfig.connectionTimeoutProbabilityPerSec = 0;
        simConfig.terminalId = -1;
        //testerConfig.verbose = true;

        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
        tester.Start();
        tester.SimulateUntilClusteringDone(maxClusteringTimeMs);
        clusteringTimeTotalMs += tester.sim->simState.simTimeMs;
    }

    printf("Average clustering time %d seconds" EOL, clusteringTimeTotalMs / clusteringIterations / 1000);
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
        simConfig.connectionTimeoutProbabilityPerSec = 0.0005;
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
    testerConfig.verbose = false;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", 50 } );
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    //Instruct all nodes to flood the network using FLOOD_MODE_UNRELIABLE_SPLIT
    for (u32 i = 0; i < tester.sim->GetTotalNodes(); i++) {
        tester.SendTerminalCommand(i+1, "action this debug flood 0 4 200");
    }
    tester.SimulateUntilClusteringDone(100 * 1000);

    printf("Clustering under load took %u seconds", tester.sim->simState.simTimeMs / 1000);
}

TEST(TestClustering, SimulateLongevity_long) {
    u32 numIterations = 10;

    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", 50 } );
    simConfig.connectionTimeoutProbabilityPerSec = 0.00001;
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

extern std::map<std::string, int> simStatCounts;
TEST(TestClustering, TestHighPrioQueueFull) {
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

        for (int i = 0; i < 1000; i++) {
            tester.SendTerminalCommand(0, "update_iv 0 100");
            tester.SimulateGivenNumberOfSteps(1);
        }

        tester.SimulateUntilClusteringDone(1000 * 1000); //Such a high value because for some RNG seeds an emergency disconnect might be required which takes quite a bit of time.
    }

    ASSERT_TRUE(simStatCounts.find("highPrioQueueFull") != simStatCounts.end());
    ASSERT_TRUE(simStatCounts["highPrioQueueFull"] > 3);
}

TEST(TestClustering, TestInfluceOfNodeWithWrongNetworkKey) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();

    simConfig.connectionTimeoutProbabilityPerSec = 0.001;

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
    testerConfig.verbose = false;
    
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


//TODO: Write a test that checks reestablishing while the mesh is flooded

//This executes all MultiStackFixture Tests with the S130 and S132 stacks
INSTANTIATE_TEST_SUITE_P(TestClustering, MultiStackFixture,
    ::testing::Values(prod_mesh_nrf52));