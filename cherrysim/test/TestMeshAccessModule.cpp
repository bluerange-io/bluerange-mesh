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
	simConfig.numNodes = 2;
	simConfig.terminalId = 0;
	//testerConfig.verbose = true;

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
	testerConfig.verbose = false;

	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.numNodes = 3;
	simConfig.terminalId = 0;
	simConfig.preDefinedPositions = { {0.5, 0.5},{0.6, 0.5},{0.7, 0.5} };

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
	simConfig.numNodes = 2;
	simConfig.terminalId = 0;
	//testerConfig.verbose = true;

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
	simConfig.numNodes = 2;
	simConfig.terminalId = 0;
	//testerConfig.verbose = true;

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
	simConfig.numNodes = 2;
	simConfig.terminalId = 0;
	testerConfig.verbose = true;

	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

	tester.sim->nodes[1].uicr.CUSTOMER[9] = 123; // Change default network id of node 2

	tester.Start();
	tester.SendTerminalCommand(2, "set_node_key 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF");
	tester.SimulateGivenNumberOfSteps(1);

	//Wait for establishing mesh access connection                          5 = FmKeyId::RESTRAINED
	tester.SendTerminalCommand(1, "action this ma connect 00:00:00:02:00:00 5 2A:FC:35:99:4C:86:11:48:58:4C:C6:D9:EE:D4:A2:B6");

	tester.SimulateUntilMessageReceived(10 * 1000, 1, "Received remote mesh data");
}
