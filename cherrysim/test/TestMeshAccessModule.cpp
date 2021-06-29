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
#include <string>
#include "Node.h"
#include "MeshAccessModule.h"

TEST(TestMeshAccessModule, TestCommands) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1 });
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.sim->FindNodeById(1)->gs.logger.EnableTag("MACONN");
    tester.sim->FindNodeById(2)->gs.logger.EnableTag("MACONN");
    tester.sim->FindNodeById(1)->gs.logger.EnableTag("MAMOD");
    tester.sim->FindNodeById(2)->gs.logger.EnableTag("MAMOD");

    std::string command = "maconn 00:00:00:02:00:00";
    tester.SendTerminalCommand(1, command.c_str());
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Trying to connect");

    command = "action 1 ma disconnect 00:00:00:02:00:00";
    tester.SendTerminalCommand(1, command.c_str());
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Received disconnect task");
    tester.SimulateGivenNumberOfSteps(1);

    command = "action 1 ma connect 00:00:00:02:00:00";
    tester.SendTerminalCommand(1, command.c_str());
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Received connect task");
}

#ifndef GITHUB_RELEASE
TEST(TestMeshAccessModule, TestReceivingClusterUpdate)
{
    //Create a mesh with 1 sink, 1 node and a third node not part of the mesh
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = false;

    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.preDefinedPositions = { {0.5, 0.5},{0.6, 0.5},{0.7, 0.5} };
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 2 });
    simConfig.SetToPerfectConditions();

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.sim->nodes[2].uicr.CUSTOMER[9] = 123; // Change default network id of node 3 so it will not connet to the cluster

    tester.Start();

    //Wait until 1 and 2 are in a mesh
    tester.SimulateForGivenTime(10000);

    //Tell node 2 to connect to node 3 using a mesh access connection
    tester.SendTerminalCommand(2, "action this ma connect 00:00:00:03:00:00 2");

    // We should initially get a message that gives us info about the cluster, size 2 and 1 hop to sink
    tester.SimulateUntilMessageReceived(5000, 3, "Received ClusterInfoUpdate over MACONN with size:2 and hops:1");
    
    //Send a reset command to node 1 to generate a change in the cluster
    tester.SendTerminalCommand(1, "reset");

    //We should not get another update that the cluster is now only 1 node and no sink
    tester.SimulateUntilMessageReceived(5000, 3, "Received ClusterInfoUpdate over MACONN with size:1 and hops:-1");

    //We should not get another update that the cluster is now only 2 nodes and 1 hop to sink again
    tester.SimulateUntilMessageReceived(5000, 3, "Received ClusterInfoUpdate over MACONN with size:2 and hops:1");
}
#endif //GITHUB_RELEASE

#ifndef GITHUB_RELEASE
TEST(TestMeshAccessModule, TestAdvertisement) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.sim->FindNodeById(1)->gs.logger.EnableTag("MAMOD");
    tester.sim->FindNodeById(2)->gs.logger.EnableTag("MAMOD");

    tester.SendTerminalCommand(1, "malog"); // enable advertisment log reading
    tester.SendTerminalCommand(2, "malog");

    //Test if we receive the advertisement packets, once with sink 0, once with sink 1.
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "Serial BBBBC, Addr 00:00:00:02:00:00, networkId \\d+, enrolled \\d+, sink 0, deviceType 1, connectable \\d+, rssi ");
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 2, "Serial BBBBB, Addr 00:00:00:01:00:00, networkId \\d+, enrolled \\d+, sink 1, deviceType 3, connectable \\d+, rssi ");
}
#endif //GITHUB_RELEASE

#ifndef GITHUB_RELEASE
TEST(TestMeshAccessModule, TestAdvertisementLegacy)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1 });
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.sim->FindNodeById(1)->gs.logger.EnableTag("MAMOD");
    
    tester.SendTerminalCommand(1, "malog"); // enable advertisment log reading
    tester.SimulateGivenNumberOfSteps(1);
    
    alignas(ble_evt_t) u8 buffer[sizeof(ble_evt_hdr_t) + sizeof(ble_gap_evt_t) - (SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA - SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA_LEGACY)];
    CheckedMemset(buffer, 0, sizeof(buffer));
    ble_evt_t& evt = *(ble_evt_t*)buffer;
    AdvPacketServiceAndDataHeader* packet = (AdvPacketServiceAndDataHeader*)evt.evt.gap_evt.params.adv_report.data;
    advStructureMeshAccessServiceData* maPacket = (advStructureMeshAccessServiceData*)&packet->data;
    evt.header.evt_id = BLE_GAP_EVT_ADV_REPORT;
    evt.evt.gap_evt.params.adv_report.dlen = SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA_LEGACY;
    evt.evt.gap_evt.params.adv_report.rssi = -45;
    packet->flags.len = SIZEOF_ADV_STRUCTURE_FLAGS - 1;
    packet->uuid.len = SIZEOF_ADV_STRUCTURE_UUID16 - 1;
    packet->data.uuid.type = (u8)BleGapAdType::TYPE_SERVICE_DATA;
    packet->data.uuid.uuid = MESH_SERVICE_DATA_SERVICE_UUID16;
    packet->data.messageType = ServiceDataMessageType::MESH_ACCESS;
    maPacket->networkId = 1337; //Mesh network id
    maPacket->isEnrolled = 1; // Flag if this beacon is enrolled
    maPacket->isSink = 0;
    maPacket->isZeroKeyConnectable = 1;
    maPacket->IsConnectable = 0;
    maPacket->interestedInConnection = 1;
    maPacket->reserved = 0;
    maPacket->serialIndex = 829; //SerialNumber index of the beacon

    NodeIndexSetter setter(0);
    FruityHal::DispatchBleEvents(&evt);
    tester.SimulateGivenNumberOfSteps(1);
    // Just execute and check that no illegal memory is accessed via sanitizer.
    // TODO: If we'd have a function to queue artificial advertisements and locations we could
    //       queue the legacy packet in there and then wait for the same type of message that
    //       TestAdvertisement is waiting for.
}
#endif //GITHUB_RELEASE

TEST(TestMeshAccessModule, TestUnsecureNoneKeyConnection) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);


    tester.sim->nodes[1].uicr.CUSTOMER[9] = 123; // Change default network id of node 2

    tester.Start();

    tester.SendTerminalCommand(2, "action this enroll remove BBBBC");
    tester.SimulateForGivenTime(10 * 1000); //An enrollment reboot takes 4 seconds to start. We give the node additional 6 seconds to start up again.

    //Enable unsecure connections.
    {
        NodeIndexSetter setter(0);
        static_cast<MeshAccessModule*>(tester.sim->currentNode->gs.node.GetModuleById(ModuleId::MESH_ACCESS_MODULE))->allowUnenrolledUnsecureConnections = true;
    }
    {
        NodeIndexSetter setter(1);
        static_cast<MeshAccessModule*>(tester.sim->currentNode->gs.node.GetModuleById(ModuleId::MESH_ACCESS_MODULE))->allowUnenrolledUnsecureConnections = true;
    }

    //Wait for establishing mesh access connection
    tester.SendTerminalCommand(1, "action this ma connect 00:00:00:02:00:00 0"); //0 = FmKeyId::ZERO

    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Received remote mesh data");




    //Disable unsecure connections.
    {
        NodeIndexSetter setter(0);
        static_cast<MeshAccessModule*>(tester.sim->currentNode->gs.node.GetModuleById(ModuleId::MESH_ACCESS_MODULE))->allowUnenrolledUnsecureConnections = false;
    }
    {
        NodeIndexSetter setter(1);
        static_cast<MeshAccessModule*>(tester.sim->currentNode->gs.node.GetModuleById(ModuleId::MESH_ACCESS_MODULE))->allowUnenrolledUnsecureConnections = false;
    }

    //Wait for establishing mesh access connection
    tester.SendTerminalCommand(1, "action this ma connect 00:00:00:02:00:00 0"); //0 = FmKeyId::ZERO

    {
        Exceptions::DisableDebugBreakOnException disable;
        ASSERT_THROW(tester.SimulateUntilMessageReceived(10 * 1000, 1, "Received remote mesh data"), TimeoutException);
    }
    
}

TEST(TestMeshAccessModule, TestRestrainedAccess) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.SetToPerfectConditions();
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.sim->nodes[1].uicr.CUSTOMER[9] = 123; // Change default network id of node 2

    tester.Start();
    tester.SendTerminalCommand(2, "set_node_key 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF");
    tester.SimulateGivenNumberOfSteps(1);

    //Wait for establishing mesh access connection                          5 = FmKeyId::RESTRAINED
    tester.SendTerminalCommand(1, "action this ma connect 00:00:00:02:00:00 5 2A:FC:35:99:4C:86:11:48:58:4C:C6:D9:EE:D4:A2:B6");

    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Received remote mesh data");
}

#ifndef GITHUB_RELEASE
TEST(TestMeshAccessModule, TestSerialConnect) {
    constexpr u32 amountOfNodesInOtherNetwork = 7;
    constexpr u32 amountOfNodesInOwnNetwork = 3 + amountOfNodesInOtherNetwork;

    // This test builds up a mesh that roughly looks like this:
    //
    //       x    o
    //       |    
    // x-x-x-x    o
    //       |    
    //      ...  ...
    //       |    
    //       x    o
    //
    // Where xs nodes of a network, os are assets and - and | are connections.
    // The left most x is the communication beacon, the distance makes sure that it can't
    // directly communicate with the o network. For this it uses the xs of its network that
    // are close to the o network.

    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    u32 numNodes = amountOfNodesInOtherNetwork + amountOfNodesInOwnNetwork;
    simConfig.terminalId = 0;
    simConfig.mapWidthInMeters = 150;
    simConfig.mapHeightInMeters = 150;
    //testerConfig.verbose = true;
    simConfig.preDefinedPositions = { {0.3, 0.5}, {0.4, 0.5}, {0.5, 0.5} };
    for (u32 i = 0; i < amountOfNodesInOtherNetwork; i++)
    {
        simConfig.preDefinedPositions.push_back({ 0.6, 0.3 + i * 0.1 });
    }
    for (u32 i = 0; i < amountOfNodesInOtherNetwork; i++)
    {
        simConfig.preDefinedPositions.push_back({ 0.7, 0.3 + i * 0.1 });
    }
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", amountOfNodesInOwnNetwork - 1 });
    simConfig.nodeConfigName.insert({ "prod_asset_nrf52", amountOfNodesInOtherNetwork });
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    for (u32 i = amountOfNodesInOwnNetwork; i < numNodes; i++)
    {
        tester.sim->nodes[i].uicr.CUSTOMER[9] = 123; // Change default network id of node 4
    }

    tester.Start();

    for (u32 i = 0; i < numNodes; i++)
    {
        NodeIndexSetter setter(i);
        GS->logger.DisableTag("ASMOD");
    }

    tester.SimulateUntilMessageReceived(100 * 1000, 1, "clusterSize\":%u", amountOfNodesInOwnNetwork); //Simulate a little to let the first 4 nodes connect to each other.
    
    // Make sure that the network key (2) works without specifying it (FF:...:FF)
    // We don't need to specify it as by default in the simulator all nodes have
    // the same network key.
    tester.SendTerminalCommand(1, "action 4 ma serial_connect BBBBN 2 FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF 33010 20 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"serial_connect_response\",\"module\":10,\"nodeId\":4,\"requestHandle\":12,\"code\":0,\"partnerId\":33010}");
    //Test that messages are possible to be sent and received through the network key connection (others are mostly blacklisted at the moment)
    tester.SendTerminalCommand(1, "action 33010 status get_status");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":33010,\"type\":\"status\"");
    tester.SimulateUntilMessageReceived(100 * 1000, 4, "Removing ma conn due to SCHEDULED_REMOVE");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":4,\"type\":\"ma_conn_state\",\"module\":10,\"requestHandle\":0,\"partnerId\":33010,\"state\":0}");

    // The same applies for the organization key (4).
    tester.SendTerminalCommand(1, "action 5 ma serial_connect BBBBP 4 FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF 33011 20 13");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"serial_connect_response\",\"module\":10,\"nodeId\":5,\"requestHandle\":13,\"code\":0,\"partnerId\":33011}");
    // Component act must be sendable through MA with orga key.
    tester.SendTerminalCommand(1, "component_act 33011 3 1 0xABCD 0x1234 01 13");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":33011,\"type\":\"component_sense\",\"module\":3,\"requestHandle\":13,\"actionType\":2,\"component\":\"0xABCD\",\"register\":\"0x1234\",\"payload\":");
    // The same applies to capabilities
    tester.SendTerminalCommand(1, "request_capability 33011");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "\"type\":\"capability_entry\"");
    tester.SimulateUntilMessageReceived(100 * 1000, 5, "Removing ma conn due to SCHEDULED_REMOVE");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":5,\"type\":\"ma_conn_state\",\"module\":10,\"requestHandle\":0,\"partnerId\":33011,\"state\":0}");

    // The node key (1) however must be given.
    tester.SendTerminalCommand(1, "action 4 ma serial_connect BBBBN 1 0B:00:00:00:0B:00:00:00:0B:00:00:00:0B:00:00:00 33012 20 13");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"type\":\"serial_connect_response\",\"module\":10,\"nodeId\":4,\"requestHandle\":13,\"code\":0,\"partnerId\":33012}");
    tester.SimulateUntilMessageReceived(100 * 1000, 4, "Removing ma conn due to SCHEDULED_REMOVE");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":4,\"type\":\"ma_conn_state\",\"module\":10,\"requestHandle\":0,\"partnerId\":33012,\"state\":0}");

    // Test that not just scheduled removals but also other disconnect reasons e.g. a reset generate a ma_conn_state message.
    tester.SendTerminalCommand(1, "action 4 ma serial_connect BBBBN 2 FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF 33010 20 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"serial_connect_response\",\"module\":10,\"nodeId\":4,\"requestHandle\":12,\"code\":0,\"partnerId\":33010}");
    tester.SendTerminalCommand(11, "reset");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":4,\"type\":\"ma_conn_state\",\"module\":10,\"requestHandle\":0,\"partnerId\":33010,\"state\":0}");
}

TEST(TestMeshAccessModule, TestDiscoveryAlwaysBusy) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.preDefinedPositions = { {0.45, 0.5}, {0.55, 0.5}, {0.5, 0.5} };
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1 });
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1 });
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.sim->nodes[0].uicr.CUSTOMER[9] = 123; // Change default network id of node 0
    
    tester.Start();

    tester.SimulateForGivenTime(1000); //Give nodes a bit time to boot up

    for (u32 i = 0; i < tester.sim->GetTotalNodes(); i++)
    {
        NodeIndexSetter setter(i);
        GS->logger.EnableTag("MACONN");
        tester.sim->currentNode->discoveryAlwaysBusy = true;
    }
    
    // Use network key (2) without specifying it (FF:...:FF)
    // We don't need to specify it as by default in the simulator all nodes have
    // the same network key. See TestMeshAccessModule.TestSerialConnect for
    // the validity of this statement.
    tester.SendTerminalCommand(1, "action this ma serial_connect BBBBC 2 FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF 33010 100 12");
    std::vector<SimulationMessage> messages = {
        SimulationMessage(1, "Deleted Connection, type 5, discR: 0, appDiscR: 1"), // For the central,    the type is MeshAccessConnection(5)
        SimulationMessage(2, "Deleted Connection, type 4, discR: 0, appDiscR: 1")  // For the peripheral, the type is ResolverConnection(4)
    };
    tester.SimulateUntilMessagesReceived(10 * 1000, messages);
}

TEST(TestMeshAccessModule, TestInfoRetrievalOverOrgaKey) {
    //The gateway retrieves get_device_info and get_status
    //messages from the assets through a mesh access connection.
    //This test makes sure that this is possible.
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.defaultNetworkId = 0;
    simConfig.preDefinedPositions = { {0.1, 0.5}, {0.3, 0.55}, {0.5, 0.5} };
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_asset_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SendTerminalCommand(1, "action 0 enroll basic BBBBB 1 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 01:00:00:00:01:00:00:00:01:00:00:00:01:00:00:00 10 0 0");
    tester.SendTerminalCommand(2, "action 0 enroll basic BBBBC 2 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00 10 0 0");
    tester.SendTerminalCommand(3, "action 0 enroll basic BBBBD 33000 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 03:00:00:00:03:00:00:00:03:00:00:00:03:00:00:00 10 0 0");
    
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "clusterSize\":2"); //Wait until the nodes have clustered.

    //Connect using the orga key.
    tester.SendTerminalCommand(1, "action 2 ma serial_connect BBBBD 4 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 33011 20 13");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"serial_connect_response\",\"module\":10,\"nodeId\":2,\"requestHandle\":13,\"code\":0,\"partnerId\":33011}");

    //Retriev the information using explicit nodeId
    tester.SendTerminalCommand(1, "action 33011 status get_device_info");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":33011,\"type\":\"device_info\"");
    tester.SendTerminalCommand(1, "action 33011 status get_status");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":33011,\"type\":\"status\"");

    //Retriev the information using broadcast
    tester.SendTerminalCommand(1, "action 0 status get_device_info");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":33011,\"type\":\"device_info\"");
    tester.SendTerminalCommand(1, "action 0 status get_status");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":33011,\"type\":\"status\"");

    //Make sure that the connection cleans up even after usage.
    tester.SimulateUntilMessageReceived(100 * 1000, 2, "Removing ma conn due to SCHEDULED_REMOVE");
}
#endif //!GITHUB_RELEASE

TEST(TestMeshAccessModule, TestConnectWithNetworkKey)
{
    //Set up a test with two nodes that are close together
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.preDefinedPositions = { {0.5, 0.5},{0.6, 0.6} };
    simConfig.nodeConfigName.insert({ "github_dev_nrf52", 2 });
    simConfig.SetToPerfectConditions();

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    //Enable some logging to be able to better understand the MeshAccessConnection
    tester.sim->FindNodeById(1)->gs.logger.EnableTag("MACONN");
    tester.sim->FindNodeById(2)->gs.logger.EnableTag("MACONN");
    tester.sim->FindNodeById(1)->gs.logger.EnableTag("MAMOD");
    tester.sim->FindNodeById(2)->gs.logger.EnableTag("MAMOD");
    tester.sim->FindNodeById(1)->gs.logger.EnableTag("CONN_DATA");
    tester.sim->FindNodeById(2)->gs.logger.EnableTag("CONN_DATA");

    // Change default network id of node 2 so it will not automatically connect to node 1
    tester.sim->nodes[1].uicr.CUSTOMER[9] = 123;

    tester.Start();

    //Tell node 1 to connect to node 2 using a mesh access connection and the network key (2)
    //NetworkKey is not given here as both nodes have the same networkKey stored because of the test setup
    tester.SendTerminalCommand(1, "action this ma connect 00:00:00:02:00:00 2");

    // We should initially get a message that gives us info about the cluster, size 2 and 1 hop to sink
    tester.SimulateUntilMessageReceived(5000, 2, "-- TX Handshake Done");
}

TEST(TestMeshAccessModule, TestActionViaNetworkKeyRemoteMeshOnNonPartnerNode)
{
    //Set up a test with two nodes that are close together
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.preDefinedPositions = { {0.5, 0.5},{0.6, 0.6},{0.7, 0.7} };
    simConfig.nodeConfigName.insert({ "github_dev_nrf52", 3 });
    simConfig.SetToPerfectConditions();

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    //Enable some logging to be able to better understand the MeshAccessConnection
    for (int nodeId : std::array<int, 3>{1, 2, 3})
    {
        tester.sim->FindNodeById(nodeId)->gs.logger.EnableTag("MACONN");
        tester.sim->FindNodeById(nodeId)->gs.logger.EnableTag("MAMOD");
        tester.sim->FindNodeById(nodeId)->gs.logger.EnableTag("CONN_DATA");
    }

    // The nodes are connected as such:
    // (1) --access--> (2) <==mesh==> (3)

    tester.Start();

    tester.SendTerminalCommand(1, "action 0 enroll basic BBBBB 1 123 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 01:00:00:00:01:00:00:00:01:00:00:00:01:00:00:00 10 0 0");
    tester.SendTerminalCommand(2, "action 0 enroll basic BBBBC 2 456 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 44:44:44:44:44:44:44:44:44:44:44:44:44:44:44:44 02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00 10 0 0");
    tester.SendTerminalCommand(3, "action 0 enroll basic BBBBD 3 456 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 44:44:44:44:44:44:44:44:44:44:44:44:44:44:44:44 03:00:00:00:03:00:00:00:03:00:00:00:03:00:00:00 10 0 0");

    tester.SimulateUntilClusteringDoneWithExpectedNumberOfClusters(60000, 2);

    // Tell node 1 to connect to node 2 using a mesh access connection and the
    // network key (2). Use MESH_ACCESS_TUNNEL_TYPE_REMOTE_MESH (1) to access
    // nodes in the remote mesh network.
    tester.SendTerminalCommand(1, "action this ma connect 00:00:00:02:00:00 2 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 1");

    // Simulate until the mesh access connection has been successfully
    // established.
    tester.SimulateUntilRegexMessageReceived(5000, 1, R"("nodeId":1,"type":"ma_conn_state","module":10,"requestHandle":0,"partnerId":[^,]*,"state":4)");

    // Find the virtual partner id of the partner of node 1.
    NodeId virtualPartnerId = 0;
    {
        NodeIndexSetter nodeIndexSetter{0};
        ASSERT_EQ(
            tester.sim->currentNode->gs.node.configuration.nodeId,
            static_cast<NodeId>(1)
        );
        auto meshAccessConnections =
            tester.sim->FindNodeById(1)->gs.cm.GetMeshAccessConnections(ConnectionDirection::DIRECTION_OUT);
        ASSERT_EQ(meshAccessConnections.count, 1);
        virtualPartnerId = meshAccessConnections.handles[0].GetVirtualPartnerId();
    }

    // Find the virtual partner id of node 1 as seen by node 2.
    NodeId remoteVirtualPartnerId = 0;
    {
        NodeIndexSetter nodeIndexSetter{1};
        ASSERT_EQ(
            tester.sim->currentNode->gs.node.configuration.nodeId,
            static_cast<NodeId>(2)
        );
        auto meshAccessConnections =
            tester.sim->FindNodeById(2)->gs.cm.GetMeshAccessConnections(ConnectionDirection::DIRECTION_IN);
        ASSERT_EQ(meshAccessConnections.count, 1);
        remoteVirtualPartnerId = meshAccessConnections.handles[0].GetVirtualPartnerId();
    }

    // Request the device info of the partner node and verify it is received
    // with the virtual node id of the partner.
    tester.SendTerminalCommand(1, "action %u status get_device_info", virtualPartnerId);
    tester.SimulateUntilRegexMessageReceived(5000, 1, R"("nodeId":%u,.*"type":"device_info")", virtualPartnerId);

    // Request the device info of the partner node using it's node id in the
    // remote mesh and verify it is received with it's virtual node id of the partner.
    tester.SendTerminalCommand(1, "action 2 status get_device_info");
    tester.SimulateUntilRegexMessageReceived(5000, 1, R"("nodeId":%u,.*"type":"device_info")", virtualPartnerId);

    // Request the device info of the node connected to the partner node and
    // verify it is received with it's remote node id.
    tester.SendTerminalCommand(1, "action 3 status get_device_info");
    tester.SimulateUntilRegexMessageReceived(5000, 1, R"("nodeId":3,.*"type":"device_info")");

    // Request the connections of the node connected to the partner node and
    // verify it is received.
    tester.SendTerminalCommand(1, "action 3 status get_connections");
    tester.SimulateUntilRegexMessageReceived(5000, 1, R"("type":"connections",.*"nodeId":3[^0-9])");

    {   // Request the connections via broadcast and verify they are all received.
        tester.SendTerminalCommand(1, "action 0 status get_connections");
        auto messages = std::vector<SimulationMessage>{
            {1, R"("type":"connections",.*"nodeId":1[^0-9])"},
            {1, R"("type":"connections",.*"nodeId":)" + std::to_string(virtualPartnerId) + "[^0-9]"},
            {1, R"("type":"connections",.*"nodeId":3[^0-9])"},
        };
        tester.SimulateUntilRegexMessagesReceived(10000, messages);
    }

    {   // Request the connection via the virtual node id on the remote
        // partner node and make sure the answer is received.
        tester.SendTerminalCommand(2, "action %u status get_connections", remoteVirtualPartnerId);
        tester.SimulateUntilRegexMessageReceived(10000, 2, R"("type":"connections",.*"nodeId":%u[^0-9])", remoteVirtualPartnerId);
    }

    {   // Request the connection via the virtual node id on the remote
        // (non-partner) node and make sure the answer is received.
        tester.SendTerminalCommand(3, "action %u status get_connections", remoteVirtualPartnerId);
        tester.SimulateUntilRegexMessageReceived(10000, 3, R"("type":"connections",.*"nodeId":%u[^0-9])", remoteVirtualPartnerId);
    }

    {   // Request the connection via broadcast on the remote (non-partner) node
        // and make sure all answers are received.
        tester.SendTerminalCommand(3, "action 0 status get_connections");
        auto messages = std::vector<SimulationMessage>{
            {3, R"("type":"connections",.*"nodeId":3[^0-9])"},
            {3, R"("type":"connections",.*"nodeId":2[^0-9])"},
            {3, R"("type":"connections",.*"nodeId":)" + std::to_string(remoteVirtualPartnerId) + "[^0-9]"},
        };
        tester.SimulateUntilRegexMessagesReceived(10000, messages);
    }
}

TEST(TestMeshAccessModule, TestActionViaNodeKeyRemoteMeshOnNonPartnerNode)
{
    //Set up a test with two nodes that are close together
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.preDefinedPositions = { {0.5, 0.5},{0.6, 0.6},{0.7, 0.7} };
    simConfig.nodeConfigName.insert({ "github_dev_nrf52", 3 });
    simConfig.SetToPerfectConditions();

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    //Enable some logging to be able to better understand the MeshAccessConnection
    for (int nodeId : std::array<int, 3>{1, 2, 3})
    {
        tester.sim->FindNodeById(nodeId)->gs.logger.EnableTag("MACONN");
        tester.sim->FindNodeById(nodeId)->gs.logger.EnableTag("MAMOD");
        tester.sim->FindNodeById(nodeId)->gs.logger.EnableTag("CONN_DATA");
    }
    // Change default network id of node 1 such that node 2 and 3 form their
    // own small mesh.
    tester.sim->nodes[0].uicr.CUSTOMER[9] = 123;
    tester.sim->nodes[1].uicr.CUSTOMER[9] = 456;
    tester.sim->nodes[2].uicr.CUSTOMER[9] = 456;

    // The nodes are connected as such:
    // (1) --access--> (2) <==mesh==> (3)

    tester.Start();

    tester.SendTerminalCommand(1, "action 0 enroll basic BBBBB 1 123 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 01:00:00:00:01:00:00:00:01:00:00:00:01:00:00:00 10 0 0");
    tester.SendTerminalCommand(2, "action 0 enroll basic BBBBC 2 456 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 44:44:44:44:44:44:44:44:44:44:44:44:44:44:44:44 02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00 10 0 0");
    tester.SendTerminalCommand(3, "action 0 enroll basic BBBBD 3 456 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 44:44:44:44:44:44:44:44:44:44:44:44:44:44:44:44 03:00:00:00:03:00:00:00:03:00:00:00:03:00:00:00 10 0 0");

    tester.SimulateUntilClusteringDoneWithExpectedNumberOfClusters(60000, 2);

    // Tell node 1 to connect to node 2 using a mesh access connection and the
    // node key (1). Use MESH_ACCESS_TUNNEL_TYPE_REMOTE_MESH (1) to access
    // nodes in the remote mesh network.
    tester.SendTerminalCommand(1, "action this ma connect 00:00:00:02:00:00 1 02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00 1");

    // Simulate until the mesh access connection has been successfully
    // established.
    tester.SimulateUntilRegexMessageReceived(5000, 1, R"("nodeId":1,"type":"ma_conn_state","module":10,"requestHandle":0,"partnerId":[^,]*,"state":4)");

    // Find the virtual partner id of the partner of node 1.
    NodeId virtualPartnerId = 0;
    {
        NodeIndexSetter nodeIndexSetter{0};
        ASSERT_EQ(
            tester.sim->currentNode->gs.node.configuration.nodeId,
            static_cast<NodeId>(1)
        );
        auto meshAccessConnections =
            tester.sim->FindNodeById(1)->gs.cm.GetMeshAccessConnections(ConnectionDirection::DIRECTION_OUT);
        ASSERT_EQ(meshAccessConnections.count, 1);
        virtualPartnerId = meshAccessConnections.handles[0].GetVirtualPartnerId();
    }

    // Find the virtual partner id of node 1 as seen by node 2.
    NodeId remoteVirtualPartnerId = 0;
    {
        NodeIndexSetter nodeIndexSetter{1};
        ASSERT_EQ(
            tester.sim->currentNode->gs.node.configuration.nodeId,
            static_cast<NodeId>(2)
        );
        auto meshAccessConnections =
            tester.sim->FindNodeById(2)->gs.cm.GetMeshAccessConnections(ConnectionDirection::DIRECTION_IN);
        ASSERT_EQ(meshAccessConnections.count, 1);
        remoteVirtualPartnerId = meshAccessConnections.handles[0].GetVirtualPartnerId();
    }

    // Request the device info of the partner node and verify it is received
    // with the virtual node id of the partner.
    tester.SendTerminalCommand(1, "action %u status get_device_info", virtualPartnerId);
    tester.SimulateUntilRegexMessageReceived(5000, 1, R"("nodeId":%u,.*"type":"device_info")", virtualPartnerId);

    {   Exceptions::DisableDebugBreakOnException disableDebugBreakOnException;
        // Request the device info of the partner node using it's non-virtual
        // node id and verify it is not received.
        tester.SendTerminalCommand(1, "action 2 status get_device_info");
        ASSERT_THROW(
            tester.SimulateUntilRegexMessageReceived(5000, 1, R"("nodeId":[^,]*,.*"type":"device_info")"),
            TimeoutException
        );
    }

    {   Exceptions::DisableDebugBreakOnException disableDebugBreakOnException;
        // Request the device info of the non-partner remote node and verify it
        // is not received.
        tester.SendTerminalCommand(1, "action 3 status get_device_info");
        ASSERT_THROW(
            tester.SimulateUntilRegexMessageReceived(5000, 1, R"("nodeId":[^,]*,.*"type":"device_info")"),
            TimeoutException
        );
    }

    {   // Request the device info via broadcast and verify it is received with
        // the virtual node id of the partner and the node itself.
        tester.SendTerminalCommand(1, "action 0 status get_device_info");
        auto messages = std::vector<SimulationMessage>{
            {1, R"("nodeId":1,.*"type":"device_info")"},
            {1, R"("nodeId":)" + std::to_string(virtualPartnerId) + R"(,.*"type":"device_info")"},
        };
        tester.SimulateUntilRegexMessagesReceived(5000, messages);
    }

    tester.SimulateForGivenTime(10000);

    {   Exceptions::DisableDebugBreakOnException disableDebugBreakOnException;
        // Request the device info via broadcast and verify no response is
        // received from the non-partner remote node.
        tester.SendTerminalCommand(1, "action 0 status get_device_info");
        ASSERT_THROW(
            tester.SimulateUntilRegexMessageReceived(5000, 1, R"("nodeId":3,.*"type":"device_info")"),
            TimeoutException
        );
    }
}

TEST(TestMeshAccessModule, TestMeshAccessConnectionUsingInvalidKeyData)
{
    //Set up a test with two nodes that are close together
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.preDefinedPositions = { {0.5, 0.5},{0.6, 0.6} };
    simConfig.nodeConfigName.insert({ "github_dev_nrf52", 2 });
    simConfig.SetToPerfectConditions();

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    //Enable some logging to be able to better understand the MeshAccessConnection
    for (int nodeId : std::array{1, 2})
    {
        tester.sim->FindNodeById(nodeId)->gs.logger.EnableTag("MACONN");
        tester.sim->FindNodeById(nodeId)->gs.logger.EnableTag("MAMOD");
        tester.sim->FindNodeById(nodeId)->gs.logger.EnableTag("CONN_DATA");
    }

    // The nodes are connected as such:
    // (1) --access--> (2)

    tester.Start();

    tester.SendTerminalCommand(1, "action 0 enroll basic BBBBB 1 123 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 01:00:00:00:01:00:00:00:01:00:00:00:01:00:00:00 10 0 0");
    tester.SendTerminalCommand(2, "action 0 enroll basic BBBBC 2 456 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 44:44:44:44:44:44:44:44:44:44:44:44:44:44:44:44 02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00 10 0 0");

    tester.SimulateForGivenTime(60000);

    tester.SimulateUntilClusteringDoneWithExpectedNumberOfClusters(10000, 2);

    // Tell node 1 to connect to node 2 using a mesh access connection and the
    // node key (1). Use MESH_ACCESS_TUNNEL_TYPE_REMOTE_MESH (1) to access
    // nodes in the remote mesh network. Deliberately use the wrong node key.
    tester.SendTerminalCommand(1, "action this ma connect 00:00:00:02:00:00 1 0F:00:00:00:0F:00:00:00:0F:00:00:00:0F:00:00:00 1");

    // Make sure the mesh access connection is not established (i.e. state is
    // DISCONNECTED).
    tester.SimulateUntilRegexMessageReceived(5000, 1, R"("nodeId":1,"type":"ma_conn_state","module":10,"requestHandle":0,"partnerId":[^,]*,"state":0)");
}
