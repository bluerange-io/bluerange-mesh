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
#include <Logger.h>
#include <Utility.h>
#include <string>
#include "ConnectionAllocator.h"
#include "StatusReporterModule.h"
#include "CherrySimUtils.h"
#include "RingIndexGenerator.h"


extern "C"{
#include <ccm_soft.h>
}


TEST(TestOther, BatteryTest)
{
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.numNodes = 10;
	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
	tester.Start();

	tester.SendTerminalCommand(1, "action this node discovery off");

	tester.SimulateForGivenTime(10000);

	//Log the battery usage
	for (u32 i = 0; i < simConfig.numNodes; i++) {
		u32 usageMicroAmpere = tester.sim->nodes[i].nanoAmperePerMsTotal / tester.sim->simState.simTimeMs;
		printf("Average Battery usage for node %d was %u uA" EOL, tester.sim->nodes[i].id, usageMicroAmpere);
	}
}

TEST(TestOther, TestRebootReason)
{
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	testerConfig.verbose = true;
	simConfig.numNodes = 1;
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
	tester.sim->setNode(0);
	tester.sim->resetCurrentNode(RebootReason::UNKNOWN, false);
	tester.SendTerminalCommand(1, "action this status get_rebootreason");
	tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "\\{\"type\":\"reboot_reason\",\"nodeId\":1,\"module\":3,\"reason\":22");
}

TEST(TestOther, TestPositionSetting)
{
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	testerConfig.verbose = true;
	simConfig.numNodes = 3;
	simConfig.mapWidthInMeters = 1;
	simConfig.mapHeightInMeters = 1;
	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
	tester.Start();

	constexpr double absError = 0.0001;

	tester.SendTerminalCommand(1, "sim set_position BBBBB 1337 52 12");
	tester.SimulateGivenNumberOfSteps(1);
	ASSERT_NEAR(tester.sim->nodes[0].x, 1337, absError);
	ASSERT_NEAR(tester.sim->nodes[0].y, 52, absError);
	ASSERT_NEAR(tester.sim->nodes[0].z, 12, absError);

	tester.SendTerminalCommand(1, "sim set_position BBBBC 13 14 2");
	tester.SimulateGivenNumberOfSteps(1);
	ASSERT_NEAR(tester.sim->nodes[1].x, 13, absError);
	ASSERT_NEAR(tester.sim->nodes[1].y, 14, absError);
	ASSERT_NEAR(tester.sim->nodes[1].z, 2, absError);

	tester.SendTerminalCommand(1, "sim set_position BBBBD 100 200 111");
	tester.SimulateGivenNumberOfSteps(1);
	ASSERT_NEAR(tester.sim->nodes[2].x, 100, absError);
	ASSERT_NEAR(tester.sim->nodes[2].y, 200, absError);
	ASSERT_NEAR(tester.sim->nodes[2].z, 111, absError);

	tester.SendTerminalCommand(1, "set_serial ZZZZZ");
	tester.SimulateGivenNumberOfSteps(1);
	tester.SendTerminalCommand(1, "sim set_position ZZZZZ -42.2 17.5 0.123");
	tester.SimulateGivenNumberOfSteps(1);
	ASSERT_NEAR(tester.sim->nodes[0].x, -42.2, absError);
	ASSERT_NEAR(tester.sim->nodes[0].y, 17.5, absError);
	ASSERT_NEAR(tester.sim->nodes[0].z, 0.123, absError);

	tester.SendTerminalCommand(1, "sim add_position BBBBC 5 6 -0.2");
	tester.SimulateGivenNumberOfSteps(1);
	ASSERT_NEAR(tester.sim->nodes[1].x, 18, absError);
	ASSERT_NEAR(tester.sim->nodes[1].y, 20, absError);
	ASSERT_NEAR(tester.sim->nodes[1].z, 1.8, absError);
}

TEST(TestOther, TestMersenneTwister)
{
	MersenneTwister mt(1337);
	ASSERT_EQ(mt.nextU32(), 1981032416);
	ASSERT_EQ(mt.nextU32(), 1498672866);
	ASSERT_EQ(mt.nextU32(), 17410945);
	ASSERT_EQ(mt.nextU32(), 2021186573);
	ASSERT_EQ(mt.nextU32(), 3884794617);
	ASSERT_EQ(mt.nextU32(), 2115046042);
	ASSERT_EQ(mt.nextU32(), 369580570);
	ASSERT_EQ(mt.nextU32(), 4020472140);
	ASSERT_EQ(mt.nextU32(), 1162855072);
	ASSERT_EQ(mt.nextU32(), 2578237934);
	ASSERT_EQ(mt.nextU32(), 3665080984);
	ASSERT_EQ(mt.nextU32(), 909454479);
	ASSERT_EQ(mt.nextU32(), 4186590157);
	ASSERT_EQ(mt.nextU32(), 802630325);
	ASSERT_EQ(mt.nextU32(), 2192268788);
	ASSERT_EQ(mt.nextU32(), 1555061911);
	
	constexpr double absError = 0.0000001;
	ASSERT_NEAR(mt.nextDouble(), 0.251894243120191207996327875662245787680149078369140625, absError);
	ASSERT_NEAR(mt.nextDouble(), 0.2523832712910099029812727167154662311077117919921875, absError);
	ASSERT_NEAR(mt.nextDouble(), 0.6441562533481410834923508446081541478633880615234375, absError);
	ASSERT_NEAR(mt.nextDouble(), 0.54784144473910367789670772253884933888912200927734375, absError);
	ASSERT_NEAR(mt.nextDouble(), 0.12091048926136234442640926545209367759525775909423828125, absError);
	ASSERT_NEAR(mt.nextDouble(), 0.88643486632184009810231373194255866110324859619140625, absError);
	ASSERT_NEAR(mt.nextDouble(), 0.1833344784526467485807899038263713009655475616455078125, absError);
	ASSERT_NEAR(mt.nextDouble(), 0.84808397871630358810790539791923947632312774658203125, absError);
	ASSERT_NEAR(mt.nextDouble(), 0.10864859868508032481049241368964430876076221466064453125, absError);
	ASSERT_NEAR(mt.nextDouble(), 0.9043797580302646021976897827698849141597747802734375, absError);
	ASSERT_NEAR(mt.nextDouble(), 0.61667418308012977856691350098117254674434661865234375, absError);
	ASSERT_NEAR(mt.nextDouble(), 0.9985854830589577790789235223201103508472442626953125, absError);
	ASSERT_NEAR(mt.nextDouble(), 0.355181997258956994034662102421862073242664337158203125, absError);
	ASSERT_NEAR(mt.nextDouble(), 0.58985742009008712560813592062913812696933746337890625, absError);
	ASSERT_NEAR(mt.nextDouble(), 0.032900537604675754443928070713809574954211711883544921875, absError);
	ASSERT_NEAR(mt.nextDouble(), 0.654886060313993656478714910917915403842926025390625, absError);

	ASSERT_NEAR(mt.nextNormal(0, 1), 0.77372467790764953843307694114628247916698455810546875, absError);
	ASSERT_NEAR(mt.nextNormal(0, 1), 0.260928499035595728994252340271486900746822357177734375, absError);
	ASSERT_NEAR(mt.nextNormal(0, 1), 0.84312650675754696738550819645752198994159698486328125, absError);
	ASSERT_NEAR(mt.nextNormal(0, 1), -1.059761249244417502524129304219968616962432861328125, absError);
	ASSERT_NEAR(mt.nextNormal(0, 1), 0.15475690410602893631875076607684604823589324951171875, absError);
	ASSERT_NEAR(mt.nextNormal(0, 1), 0.34254451851282119445585294670308940112590789794921875, absError);
	ASSERT_NEAR(mt.nextNormal(0, 1), 0.791886474092245773448439649655483663082122802734375, absError);
	ASSERT_NEAR(mt.nextNormal(0, 1), 0.0818000501182297445890156950554228387773036956787109375, absError);
	ASSERT_NEAR(mt.nextNormal(0, 1), -1.619811757831973952903581448481418192386627197265625, absError);
	ASSERT_NEAR(mt.nextNormal(0, 1), -0.23123517731102083416772074997425079345703125, absError);
	ASSERT_NEAR(mt.nextNormal(0, 1), -0.442988852581041536726758067743503488600254058837890625, absError);
	ASSERT_NEAR(mt.nextNormal(0, 1), 0.8911279896723136584313351704622618854045867919921875, absError);
	ASSERT_NEAR(mt.nextNormal(0, 1), 2.13873028741060178248289957991801202297210693359375, absError);
	ASSERT_NEAR(mt.nextNormal(0, 1), -0.35447922936990206022045413192245177924633026123046875, absError);
	ASSERT_NEAR(mt.nextNormal(0, 1), 0.68564254490865439439772899277159012854099273681640625, absError);
	ASSERT_NEAR(mt.nextNormal(0, 1), 0.93824658110089520501873039393103681504726409912109375, absError);
}

//This test should check if two different configurations can be applied to two nodes using the simulator
TEST(TestOther, ConfigurationTest)
{
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	testerConfig.verbose = false;
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.numNodes = 2;
	simConfig.terminalId = 0;
	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

	strcpy(tester.sim->nodes[0].nodeConfiguration, "prod_sink_nrf52");
	strcpy(tester.sim->nodes[1].nodeConfiguration, "prod_mesh_nrf52");

	tester.Start();


	tester.SimulateUntilClusteringDone(0);
}

//This test should check if two different configurations can be applied to two nodes using the simulator
TEST(TestOther, SinkInMesh)
{
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	//testerConfig.verbose = true;
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.numNodes = 10;
	simConfig.terminalId = 1;
	strcpy(simConfig.defaultNodeConfigName, "prod_mesh_nrf52");
	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

	strcpy(tester.sim->nodes[0].nodeConfiguration, "prod_sink_nrf52");

	tester.Start();

	tester.SimulateUntilClusteringDone(0);

	//TODO: check that configurations were used
}

//Not sure what this test is doing, could ressurect it, but maybe not worth the effort
TEST(TestOther, TestEncryption) {
	//Boot up a simulator for our Logger
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.numNodes = 1;
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

#ifndef GITHUB_RELEASE
TEST(TestOther, TestConnectionAllocator) {
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	testerConfig.verbose = false;
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.numNodes = 1;
	simConfig.terminalId = 0;
	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
	strcpy(tester.sim->nodes[0].nodeConfiguration, "prod_sink_nrf52");
	tester.Start();

	MersenneTwister mt;

	std::vector<BaseConnection*> conns;

	for (int i = 0; i < 10000; i++) 
	{
		if ((mt.nextU32(0, 1) && conns.size() > 0) || conns.size() == TOTAL_NUM_CONNECTIONS) { //dealloc
			int index = mt.nextU32(0, conns.size() - 1);
			ConnectionAllocator::getInstance().deallocate(conns[index]);
			conns.erase(conns.begin() + index);
		}
		else { //alloc
			FruityHal::BleGapAddr addr;
			addr.addr_type = FruityHal::BleGapAddrType::PUBLIC;
			CheckedMemset(addr.addr, 0, 6);
			int type = mt.nextU32(0, 3);
			if (type == 0) 
			{
				conns.push_back(ConnectionAllocator::getInstance().allocateClcAppConnection(0, ConnectionDirection::DIRECTION_IN, &addr));
			}
			else if (type == 1) 
			{
				conns.push_back(ConnectionAllocator::getInstance().allocateMeshAccessConnection(0, ConnectionDirection::DIRECTION_IN, &addr, FmKeyId::ZERO, MeshAccessTunnelType::INVALID));
			}
			else if (type == 2) 
			{
				conns.push_back(ConnectionAllocator::getInstance().allocateMeshConnection(0, ConnectionDirection::DIRECTION_IN, &addr, 0));
			}
			else 
			{
				conns.push_back(ConnectionAllocator::getInstance().allocateResolverConnection(0, ConnectionDirection::DIRECTION_IN, &addr));
			}
		}
	}
}
#endif //GITHUB_RELEASE

//Tests the implementation of CherrySimTester::SimulateUntilMessagesReceived.
TEST(TestOther, TestMultiMessageSimulation) {
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	testerConfig.verbose = true;
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.numNodes = 2;
	simConfig.terminalId = 0;
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
		std::string completeMsg = msgs[i].getCompleteMessage();
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
	simConfig.numNodes = 2;
	simConfig.terminalId = 0;
	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
	tester.Start();

	tester.SimulateUntilClusteringDone(10 * 1000);

	//Find the mesh connection to the other node
	BaseConnections conns = tester.sim->nodes[0].gs.cm.GetConnectionsOfType(ConnectionType::FRUITYMESH, ConnectionDirection::INVALID);
	MeshConnection* conn = nullptr;
	for (int i = 0; i < conns.count; i++)
	{
		conn = (MeshConnection*)tester.sim->nodes[0].gs.cm.allConnections[conns.connectionIndizes[i]];
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
//	testerConfig.verbose = true;
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.numNodes = 10;
	simConfig.terminalId = 0;
	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
	tester.Start();

	tester.SimulateUntilClusteringDone(100 * 1000);

	//Test that all connections are unsynced
	for (u32 i = 1; i <= simConfig.numNodes; i++) {
		tester.SendTerminalCommand(i, "status");
		tester.SimulateUntilMessageReceived(10 * 1000, i, "tSync:0");
	}

	//Set the time of node 1. This node will then start propagating the time through the mesh.
	tester.SendTerminalCommand(1, "settime 1560262597 0");
	tester.SimulateForGivenTime(60 * 1000);

	//Test that all connections are synced
	for (u32 i = 1; i <= simConfig.numNodes; i++) {
		tester.SendTerminalCommand(i, "status");
		tester.SimulateUntilMessageReceived(100, i, "tSync:2");
	}

	for (u32 i = 1; i <= simConfig.numNodes; i++) {
		tester.SendTerminalCommand(i, "gettime");
		tester.SimulateUntilMessageReceived(10 * 1000, i, "Time is currently approx. 2019 years");
	}

	//Check that time is running and simulate 10 seconds of passing time...
	tester.sim->setNode(0);
	auto start = tester.sim->currentNode->gs.timeManager.GetTime();
	tester.SimulateForGivenTime(10 * 1000);

	//... and check that the times have been updated correctly
	auto end = tester.sim->currentNode->gs.timeManager.GetTime();
	int diff = end - start;
	
	ASSERT_TRUE(diff >= 9 && diff <= 11);


	//Reset most of the nodes (not node 1)
	for (u32 i = 2; i <= simConfig.numNodes; i++)
	{
		tester.SendTerminalCommand(1, "action %d node reset", i);
		tester.SimulateGivenNumberOfSteps(1);
	}

	//Give the nodes time to reset
	tester.SimulateForGivenTime(15 * 1000);
	//Wait until they clustered
	tester.SimulateUntilClusteringDone(100 * 1000);
	//Wait until they have synced their time
	tester.SimulateForGivenTime(60 * 1000);

	//Make sure that the reset worked.
	for (u32 i = 2; i <= simConfig.numNodes; i++)
	{
		ASSERT_EQ(tester.sim->nodes[i - 1].restartCounter, 2);
	}

	//Check that the time has been correctly sent to each node again.
	for (u32 i = 1; i <= simConfig.numNodes; i++) {
		tester.SendTerminalCommand(i, "gettime");
		tester.SimulateUntilMessageReceived(10 * 1000, i, "Time is currently approx. 2019 years");
	}

	//Set the time again, to some higher value (also checks for year 2038 problem)
	tester.SendTerminalCommand(1, "settime 2960262597 0");
	tester.SimulateForGivenTime(60 * 1000);

	for (u32 i = 1; i <= simConfig.numNodes; i++) {
		tester.SendTerminalCommand(i, "gettime");
		tester.SimulateUntilMessageReceived(10 * 1000, i, "Time is currently approx. 2063 years");
	}

	//... and to some lower value
	tester.SendTerminalCommand(1, "settime 1260263424 0");

	tester.SimulateForGivenTime(60 * 1000);

	for (u32 i = 1; i <= simConfig.numNodes; i++) {
		tester.SendTerminalCommand(i, "gettime");
		tester.SimulateUntilMessageReceived(10 * 1000, i, "Time is currently approx. 2009 years");
	}

	// Test positive offset...
	tester.SendTerminalCommand(1, "settime 7200 60");
	tester.SimulateForGivenTime(60 * 1000);

	for (u32 i = 1; i <= simConfig.numNodes; i++) {
		tester.SendTerminalCommand(i, "gettime");
		tester.SimulateUntilMessageReceived(10 * 1000, i, "Time is currently approx. 1970 years, 1 days, 03h");
	}

	// ... and negative offset
	tester.SendTerminalCommand(1, "settime 7200 -60");
	tester.SimulateForGivenTime(60 * 1000);

	for (u32 i = 1; i <= simConfig.numNodes; i++) {
		tester.SendTerminalCommand(i, "gettime");
		tester.SimulateUntilMessageReceived(10 * 1000, i, "Time is currently approx. 1970 years, 1 days, 01h");
	}

	// Test edge case where the offset is larger than the time itself. In such a case the offset should be ignored.
	tester.SendTerminalCommand(1, "settime 7200 -10000");
	tester.SimulateForGivenTime(60 * 1000);

	for (u32 i = 1; i <= simConfig.numNodes; i++) {
		tester.SendTerminalCommand(i, "gettime");
		tester.SimulateUntilMessageReceived(10 * 1000, i, "Time is currently approx. 1970 years, 1 days, 02h");
	}
}

TEST(TestOther, TestTimeSyncDuration_long) {
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	//testerConfig.verbose = true;
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.numNodes = 50;
	simConfig.terminalId = 0;
	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
	tester.Start();

	tester.SimulateUntilClusteringDone(100 * 1000);

	tester.SendTerminalCommand(1, "settime 1337 0");
	tester.SimulateForGivenTime(150 * 1000);

	//Test that all connections are synced
	for (u32 i = 1; i <= simConfig.numNodes; i++) {
		tester.SendTerminalCommand(i, "status");
		tester.SimulateUntilMessageReceived(100, i, "tSync:2");
	}

	u32 minTime = 100000;
	u32 maxTime = 0;


	for (u32 i = 0; i < simConfig.numNodes; i++)
	{
		u32 time = tester.sim->nodes[i].gs.timeManager.GetTime();
		if (time < minTime) minTime = time;
		if (time > maxTime) maxTime = time;
	}

	i32 timeDiff = maxTime - minTime;

	printf("Maximum time difference was: %d\n", timeDiff);

	ASSERT_TRUE(timeDiff <= 1);	 //We allow 1 second off
}

TEST(TestOther, TestRestrainedKeyGeneration) {
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	//testerConfig.verbose = true;
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.numNodes = 1;
	simConfig.terminalId = 0;
	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
	tester.Start();
	tester.SimulateGivenNumberOfSteps(1);
	char restrainedKeyHexBuffer[1024];
	u8 restrainedKeyBuffer[1024];
	StackBaseSetter sbs;

	tester.SendTerminalCommand(1, "set_node_key 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF");
	tester.SimulateGivenNumberOfSteps(1);
	Conf::getInstance().GetRestrainedKey(restrainedKeyBuffer);
	Logger::convertBufferToHexString(restrainedKeyBuffer, 16, restrainedKeyHexBuffer, sizeof(restrainedKeyHexBuffer));
	ASSERT_STREQ("2A:FC:35:99:4C:86:11:48:58:4C:C6:D9:EE:D4:A2:B6", restrainedKeyHexBuffer);

	tester.SendTerminalCommand(1, "set_node_key FF:EE:DD:CC:BB:AA:99:88:77:66:55:44:33:22:11:00");
	tester.SimulateGivenNumberOfSteps(1);
	Conf::getInstance().GetRestrainedKey(restrainedKeyBuffer);
	Logger::convertBufferToHexString(restrainedKeyBuffer, 16, restrainedKeyHexBuffer, sizeof(restrainedKeyHexBuffer));
	ASSERT_STREQ("9E:63:8B:94:65:85:91:99:A9:74:7D:A7:40:7C:DD:B3", restrainedKeyHexBuffer);

	tester.SendTerminalCommand(1, "set_node_key DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF");
	tester.SimulateGivenNumberOfSteps(1);
	Conf::getInstance().GetRestrainedKey(restrainedKeyBuffer);
	Logger::convertBufferToHexString(restrainedKeyBuffer, 16, restrainedKeyHexBuffer, sizeof(restrainedKeyHexBuffer));
	ASSERT_STREQ("3C:58:54:FC:29:96:00:59:B7:80:6B:4C:78:49:8B:27", restrainedKeyHexBuffer);

	tester.SendTerminalCommand(1, "set_node_key 00:01:02:03:04:05:06:07:08:09:0A:0B:0C:0D:0E:0F");
	tester.SimulateGivenNumberOfSteps(1);
	Conf::getInstance().GetRestrainedKey(restrainedKeyBuffer);
	Logger::convertBufferToHexString(restrainedKeyBuffer, 16, restrainedKeyHexBuffer, sizeof(restrainedKeyHexBuffer));
	ASSERT_STREQ("60:AB:54:BB:F5:1C:3F:77:FA:BC:80:4C:E0:F4:78:58", restrainedKeyHexBuffer);
}

TEST(TestOther, TestTimeout) {
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	//testerConfig.verbose = true;
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.numNodes = 10;
	simConfig.terminalId = 0;
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
	simConfig.numNodes = 1;
	simConfig.terminalId = 0;
	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

	char* serialNumber = "BRTCR";

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
	simConfig.numNodes = 2;
	simConfig.terminalId = 0;
	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

	strcpy(tester.sim->nodes[0].nodeConfiguration, "prod_pcbridge_nrf52");
	strcpy(tester.sim->nodes[1].nodeConfiguration, "prod_mesh_nrf52");

	CheckedMemset(tester.sim->nodes[1].uicr.CUSTOMER, 0xFF, sizeof(tester.sim->nodes[1].uicr.CUSTOMER));	//invalidate UICR

	tester.Start();

	/*for (u32 i = 0; i < simConfig.numNodes; i++)
	{
		tester.sim->setNode(i);
		Logger::getInstance().enableAll();
	}*/

	tester.SimulateForGivenTime(10 * 1000); //Simulate a little to calculate battery usage.

	u32 usageMicroAmpere = tester.sim->nodes[1].nanoAmperePerMsTotal / tester.sim->simState.simTimeMs;
	if (usageMicroAmpere > 100)
	{
		FAIL() << "Bulk Mode should consume very low energy, but consumed: " << usageMicroAmpere;
	}
	tester.SendTerminalCommand(2, "set_node_key 02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00");
	tester.SimulateGivenNumberOfSteps(1);

	tester.SendTerminalCommand(1, "action this ma connect 00:00:00:02:00:00 1 02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00"); //1 = FmKeyId::NODE

	tester.SimulateForGivenTime(10 * 1000);
	u32 dummyVal = 1337;
	tester.SendTerminalCommand(1, "action 2001 bulk get_memory %u 4", (u32)(&dummyVal));
	tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "\\{\"nodeId\":2001,\"type\":\"get_memory_result\",\"addr\":\\d+,\"data\":\"39:05:00:00\"\\}");

	u8 data[1024];
	for (int i = 0; i < sizeof(data); i++)
	{
		data[i] = i % 256;
	}
	u32 expectedValue = Utility::CalculateCrc32(data, sizeof(data));
	tester.SendTerminalCommand(1, "action 2001 bulk get_crc32 %u %u", (u32)(data), (u32)(sizeof(data)));
	tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2001,\"type\":\"get_crc32_result\",\"crc32\":%u}", expectedValue);

	tester.SendTerminalCommand(1, "action 2001 bulk get_uicr_custom");
	tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2001,\"type\":\"get_uicr_custom_result\",\"data\":\"FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF\"}");
	
	tester.SendTerminalCommand(1, "action 2001 bulk set_uicr_custom AA:BB:CC:DD");
	tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2001,\"type\":\"set_uicr_custom_result\",\"code\":0}");
	tester.SimulateForGivenTime(20 * 1000); //Give the node time to reboot.

	ASSERT_EQ(tester.sim->nodes[1].restartCounter, 2);
	ASSERT_EQ(((u8*)(tester.sim->nodes[1].uicr.CUSTOMER))[0], 0xAA);
	ASSERT_EQ(((u8*)(tester.sim->nodes[1].uicr.CUSTOMER))[1], 0xBB);
	ASSERT_EQ(((u8*)(tester.sim->nodes[1].uicr.CUSTOMER))[2], 0xCC);
	ASSERT_EQ(((u8*)(tester.sim->nodes[1].uicr.CUSTOMER))[3], 0xDD);
	ASSERT_EQ(((u8*)(tester.sim->nodes[1].uicr.CUSTOMER))[4], 0xFF);
}
#endif //GITHUB_RELEASE

TEST(TestOther, TestWatchdog) {
	Exceptions::ExceptionDisabler<WatchdogTriggeredException> wtDisabler;
	Exceptions::ExceptionDisabler<SafeBootTriggeredException> stDisabler;
	constexpr unsigned long long starvationTimeNormalMode = FM_WATCHDOG_TIMEOUT / 32768UL * 1000UL;
	constexpr unsigned long long starvationTimeSafeBoot = FM_WATCHDOG_TIMEOUT_SAFE_BOOT / 32768UL * 1000UL;;
	{
		CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
		//testerConfig.verbose = true;
		SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
		simConfig.numNodes = 1;
		simConfig.terminalId = 0;
		simConfig.simulateWatchdog = true;
		CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

		tester.Start();

		tester.SimulateForGivenTime(1000);

		ASSERT_EQ(tester.sim->nodes[0].restartCounter, 1);

		tester.SimulateForGivenTime(starvationTimeNormalMode - 2000); //After this the node will be one second away from starvation.
		ASSERT_EQ(tester.sim->nodes[0].restartCounter, 1);

		tester.SimulateForGivenTime(2000); //Starve it and give it some time to reboot.
		ASSERT_EQ(tester.sim->nodes[0].restartCounter, 2);

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
		simConfig.numNodes = 1;
		simConfig.terminalId = 0;
		simConfig.simulateWatchdog = false;
		CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

		tester.Start();

		tester.SimulateForGivenTime(starvationTimeNormalMode * 2);

		ASSERT_EQ(tester.sim->nodes[0].restartCounter, 1); //watchdogs are disabled, nodes should not starve at all.
	}
}


#ifndef GITHUB_RELEASE
extern void setBoard_0(BoardConfiguration *c);
extern void setBoard_1(BoardConfiguration *c);
extern void setBoard_3(BoardConfiguration *c);
extern void setBoard_4(BoardConfiguration *c);
extern void setBoard_6(BoardConfiguration *c);
extern void setBoard_7(BoardConfiguration *c);
extern void setBoard_8(BoardConfiguration *c);
extern void setBoard_9(BoardConfiguration *c);
extern void setBoard_10(BoardConfiguration *c);
extern void setBoard_11(BoardConfiguration *c);
extern void setBoard_12(BoardConfiguration *c);
extern void setBoard_13(BoardConfiguration *c);
extern void setBoard_14(BoardConfiguration *c);
extern void setBoard_15(BoardConfiguration *c);
extern void setBoard_16(BoardConfiguration *c);
extern void setBoard_17(BoardConfiguration *c);
extern void setBoard_18(BoardConfiguration *c);
extern void setBoard_19(BoardConfiguration *c);
extern void setBoard_20(BoardConfiguration *c);
extern void setBoard_21(BoardConfiguration *c);
extern void setBoard_22(BoardConfiguration *c);
extern void setBoard_23(BoardConfiguration* c);
extern void setBoard_24(BoardConfiguration* c);

TEST(TestOther, TestBoards) {
	//Executes all setBoard configs to make sure none of them crashes anything.
	//Does NOT check state!

	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	//testerConfig.verbose = true;
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.numNodes = 1;
	simConfig.terminalId = 0;
	simConfig.simulateWatchdog = true;
	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
	tester.Start();

	BoardConfiguration c;
	
	c.boardType = 0;
	setBoard_0(&c);

	c.boardType = 1;
	setBoard_1(&c);

	c.boardType = 3;
	setBoard_3(&c);

	c.boardType = 4;
	setBoard_4(&c);

	c.boardType = 6;
	setBoard_6(&c);

	c.boardType = 7;
	setBoard_7(&c);

	c.boardType = 8;
	setBoard_8(&c);

	c.boardType = 9;
	setBoard_9(&c);

	u8 buffer[1000] = {};
	NRF_UICR->CUSTOMER[1] = (u32)buffer;
	c.boardType = 10;
	setBoard_10(&c);

	c.boardType = 11;
	setBoard_11(&c);

	c.boardType = 12;
	setBoard_12(&c);

	c.boardType = 13;
	setBoard_13(&c);

	c.boardType = 14;
	setBoard_14(&c);

	c.boardType = 15;
	setBoard_15(&c);

	c.boardType = 16;
	setBoard_16(&c);

	c.boardType = 17;
	setBoard_17(&c);

	c.boardType = 18;
	setBoard_18(&c);

	c.boardType = 19;
	setBoard_19(&c);

	c.boardType = 20;
	setBoard_20(&c);

	c.boardType = 21;
	setBoard_21(&c);

	c.boardType = 22;
	setBoard_22(&c);

	c.boardType = 23;
	setBoard_23(&c);

	c.boardType = 24;
	setBoard_24(&c);
}
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

TEST(TestOther, TestStringConvertions) {
	Exceptions::ExceptionDisabler<NotANumberStringException> Nanse;
	Exceptions::ExceptionDisabler<NumberStringNotInRangeException> Nsnir;

	ASSERT_EQ(Utility::StringToI8("127"), 127);
	ASSERT_EQ(Utility::StringToI16("127"), 127);
	ASSERT_EQ(Utility::StringToI32("127"), 127);
	ASSERT_EQ(Utility::StringToI8("128"), 0);
	ASSERT_EQ(Utility::StringToI16("128"), 128);
	ASSERT_EQ(Utility::StringToI32("128"), 128);
	ASSERT_EQ(Utility::StringToI8("-128"), -128);
	ASSERT_EQ(Utility::StringToI16("-128"), -128);
	ASSERT_EQ(Utility::StringToI32("-128"), -128);
	ASSERT_EQ(Utility::StringToI8("-129"), 0);
	ASSERT_EQ(Utility::StringToI16("-129"), -129);
	ASSERT_EQ(Utility::StringToI32("-129"), -129);

	ASSERT_EQ(Utility::StringToU8("255"), 255);
	ASSERT_EQ(Utility::StringToU16("255"), 255);
	ASSERT_EQ(Utility::StringToU32("255"), 255);
	ASSERT_EQ(Utility::StringToU8("256"), 0);
	ASSERT_EQ(Utility::StringToU16("256"), 256);
	ASSERT_EQ(Utility::StringToU32("256"), 256);
	ASSERT_EQ(Utility::StringToU8("70000"), 0);
	ASSERT_EQ(Utility::StringToU16("70000"), 0);
	ASSERT_EQ(Utility::StringToU32("70000"), 70000);

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
		simConfig.numNodes = 3;
		simConfig.terminalId = 0;
		simConfig.defaultNetworkId = 0;
		simConfig.storeFlashToFile = testFilePath;
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

			ASSERT_EQ(messages.size(), simConfig.numNodes);

			for (int nodeIndex = 0; nodeIndex < messages.size(); nodeIndex++) {
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
#endif //GITHUB_RELEASE
