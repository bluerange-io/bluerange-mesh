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
#include <Node.h>
#include <StatusReporterModule.h>
#include <TestStatistics.h>

//This test checks a normal clustering for the number of packets sent and makes sure that these are
//not exceeded and that no unnecessary packets are sent
TEST(TestStatistics, TestNumberClusteringMessagesSent) {
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();

	simConfig.numNodes = 10;
	simConfig.enableSimStatistics = true;

	//testerConfig.verbose = true;

	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
	tester.Start();

	tester.SimulateUntilClusteringDone(60 * 1000);

	//Calculate the statistic for all messages routed by all nodes summed up
	PacketStat stat[PACKET_STAT_SIZE];
	for (u32 i = 0; i < simConfig.numNodes; i++) {
		for (u32 j = 0; j < PACKET_STAT_SIZE; j++) {
			tester.sim->AddPacketToStats(stat, tester.sim->nodes[i].routedPackets + j);
		}
	}

	//We check for all known message types with some min and max values
	checkAndClearStat(stat, MessageType::CLUSTER_WELCOME, 10, 50);
	checkAndClearStat(stat, MessageType::CLUSTER_ACK_1, 10, 50);
	checkAndClearStat(stat, MessageType::CLUSTER_ACK_2, 10, 50);
	checkAndClearStat(stat, MessageType::CLUSTER_INFO_UPDATE, 10, 200);

	checkAndClearStat(stat, MessageType::MODULE_GENERAL, 10, 400, ModuleId::STATUS_REPORTER_MODULE, (u8)StatusReporterModule::StatusModuleGeneralMessages::LIVE_REPORT);

	//After checking for all expected messages, the stat should be empty
	checkStatEmpty(stat);
}

//#################################### Helpers for Statistic Tests #######################################

//Helper function that checks a given message type for a maximum count and clears it if it was ok
void checkAndClearStat(PacketStat* stat, MessageType mt, u32 minCount /*= 0*/, u32 maxCount /*= UINT32_MAX*/, ModuleId moduleId /*= ModuleId::INVALID_MODULE*/, u8 actionType /*= 0*/)
{
	for (u32 i = 0; i < PACKET_STAT_SIZE; i++) {
		PacketStat* entry = stat + i;
		if (entry->messageType == mt) {
			if (moduleId == ModuleId::INVALID_MODULE || (moduleId == entry->moduleId && actionType == entry->actionType)) {
				if (entry->count < minCount) SIMEXCEPTION(IllegalStateException);
				if (entry->count > maxCount) SIMEXCEPTION(IllegalStateException);
				entry->messageType = MessageType::INVALID;
			}
		}
	}
}

//Useful for clearing a statistic e.g. after clustering to only check newly sent packets after some action
void clearStat(PacketStat* stat)
{
	for (u32 i = 0; i < PACKET_STAT_SIZE; i++) {
		PacketStat* entry = stat + i;
		entry->messageType = MessageType::INVALID;
	}
}

//After checking and clearing all stat entries we can check if it is empty with this function
void checkStatEmpty(PacketStat* stat)
{
	for (u32 i = 0; i < PACKET_STAT_SIZE; i++) {
		PacketStat* entry = stat + i;
		if (entry->messageType != MessageType::INVALID) SIMEXCEPTION(IllegalStateException);
	}
}