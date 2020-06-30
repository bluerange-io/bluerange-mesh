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
	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
	tester.Start();

	tester.sim->findNodeById(1)->gs.logger.enableTag("MACONN");
	tester.sim->findNodeById(2)->gs.logger.enableTag("MACONN");
	tester.sim->findNodeById(1)->gs.logger.enableTag("MAMOD");
	tester.sim->findNodeById(2)->gs.logger.enableTag("MAMOD");

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

	tester.sim->findNodeById(1)->gs.logger.enableTag("MAMOD");
	tester.sim->findNodeById(2)->gs.logger.enableTag("MAMOD");

	tester.SendTerminalCommand(1, "malog"); // enable advertisment log reading
	tester.SendTerminalCommand(2, "malog");

	//Test if we receive the advertisement packets, once with sink 0, once with sink 1.
	tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "Serial BBBBC, Addr 00:00:00:02:00:00, networkId \\d+, enrolled \\d+, sink 0, connectable \\d+, rssi ");
	tester.SimulateUntilRegexMessageReceived(10 * 1000, 2, "Serial BBBBB, Addr 00:00:00:01:00:00, networkId \\d+, enrolled \\d+, sink 1, connectable \\d+, rssi ");
}
#endif //GITHUB_RELEASE

TEST(TestMeshAccessModule, TestUnsecureNoneKeyConnection) {
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.terminalId = 0;
	//testerConfig.verbose = true;
	simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
	simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);


	tester.sim->nodes[1].uicr.CUSTOMER[9] = 123; // Change default network id of node 2

	tester.Start();

	tester.SendTerminalCommand(2, "action this enroll remove BBBBC");
	tester.SimulateForGivenTime(10 * 1000); //An enrollment reboot takes 4 seconds to start. We give the node additional 6 seconds to start up again.

	//Enable unsecure connections.
	tester.sim->setNode(0);
	static_cast<MeshAccessModule*>(tester.sim->currentNode->gs.node.GetModuleById(ModuleId::MESH_ACCESS_MODULE))->allowUnenrolledUnsecureConnections = true;
	tester.sim->setNode(1); 
	static_cast<MeshAccessModule*>(tester.sim->currentNode->gs.node.GetModuleById(ModuleId::MESH_ACCESS_MODULE))->allowUnenrolledUnsecureConnections = true;

	//Wait for establishing mesh access connection
	tester.SendTerminalCommand(1, "action this ma connect 00:00:00:02:00:00 0"); //0 = FmKeyId::ZERO

	tester.SimulateUntilMessageReceived(10 * 1000, 1, "Received remote mesh data");




	//Disable unsecure connections.
	tester.sim->setNode(0);
	static_cast<MeshAccessModule*>(tester.sim->currentNode->gs.node.GetModuleById(ModuleId::MESH_ACCESS_MODULE))->allowUnenrolledUnsecureConnections = false;
	tester.sim->setNode(1);
	static_cast<MeshAccessModule*>(tester.sim->currentNode->gs.node.GetModuleById(ModuleId::MESH_ACCESS_MODULE))->allowUnenrolledUnsecureConnections = false;

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
	//testerConfig.verbose = true;
	simConfig.preDefinedPositions = { {0.1, 0.5}, {0.3, 0.55}, {0.5, 0.5} };
	for (u32 i = 0; i < amountOfNodesInOtherNetwork; i++)
	{
		simConfig.preDefinedPositions.push_back({ 0.7, 0.0 + i * 0.2 });
	}
	for (u32 i = 0; i < amountOfNodesInOtherNetwork; i++)
	{
		simConfig.preDefinedPositions.push_back({ 0.9, 0.0 + i * 0.2 });
	}
	simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
	simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", amountOfNodesInOwnNetwork - 1 });
	simConfig.nodeConfigName.insert({ "prod_asset_nrf52", amountOfNodesInOtherNetwork });
	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

	for (u32 i = amountOfNodesInOwnNetwork; i < numNodes; i++)
	{
		tester.sim->nodes[i].uicr.CUSTOMER[9] = 123; // Change default network id of node 4
	}

	tester.Start();
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
	tester.SendTerminalCommand(1, "action 6 ma serial_connect BBBBQ 1 0D:00:00:00:0D:00:00:00:0D:00:00:00:0D:00:00:00 33012 20 13");
	tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"serial_connect_response\",\"module\":10,\"nodeId\":6,\"requestHandle\":13,\"code\":0,\"partnerId\":33012}");
	tester.SimulateUntilMessageReceived(100 * 1000, 6, "Removing ma conn due to SCHEDULED_REMOVE");
	tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":6,\"type\":\"ma_conn_state\",\"module\":10,\"requestHandle\":0,\"partnerId\":33012,\"state\":0}");

	// Test that not just scheduled removals but also other disconnect reasons e.g. a reset generate a ma_conn_state message.
	tester.SendTerminalCommand(1, "action 4 ma serial_connect BBBBN 2 FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF 33010 20 12");
	tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"serial_connect_response\",\"module\":10,\"nodeId\":4,\"requestHandle\":12,\"code\":0,\"partnerId\":33010}");
	tester.SendTerminalCommand(11, "reset");
	tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":4,\"type\":\"ma_conn_state\",\"module\":10,\"requestHandle\":0,\"partnerId\":33010,\"state\":0}");
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
	tester.SendTerminalCommand(3, "action 0 enroll basic BBBBD 3 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 03:00:00:00:03:00:00:00:03:00:00:00:03:00:00:00 10 0 0");
	
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
