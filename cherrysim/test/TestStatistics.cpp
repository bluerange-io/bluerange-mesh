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
#include <Node.h>
#include <StatusReporterModule.h>
#include <TestStatistics.h>

//This test checks a normal clustering for the number of packets sent and makes sure that these are
//not exceeded and that no unnecessary packets are sent
TEST(TestStatistics, TestNumberClusteringMessagesSent) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();

    simConfig.enableSimStatistics = true;

    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 9});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    //TODO: This should actually wait until clustering was done, but depending on the seed it might result in an emergency
    //disconnect. Because an emergency disconnect will send some other packets and will also send encrypted packets through
    //a meshAccessConnection, it is not simple to remove these packets from the statistic
    //Therefore a shortcut has been taken to only simulate for some time so that an emergency disconnect will not happen
    tester.SimulateForGivenTime(30 * 1000);

    //Calculate the statistic for all messages routed by all nodes summed up
    PacketStat stat[PACKET_STAT_SIZE];
    for (u32 i = 0; i < tester.sim->GetTotalNodes(); i++) {
        for (u32 j = 0; j < PACKET_STAT_SIZE; j++) {
            tester.sim->AddPacketToStats(stat, tester.sim->nodes[i].routedPackets + j);
        }
    }

    //We check for all known message types with some min and max values
    CheckAndClearStat(stat, MessageType::CLUSTER_WELCOME, ModuleId::INVALID_MODULE, 10, 100); //This check surpasses 50 cases. See IOT-3997
    CheckAndClearStat(stat, MessageType::CLUSTER_ACK_1, ModuleId::INVALID_MODULE, 10, 50);
    CheckAndClearStat(stat, MessageType::CLUSTER_ACK_2, ModuleId::INVALID_MODULE, 10, 50);
    CheckAndClearStat(stat, MessageType::CLUSTER_INFO_UPDATE, ModuleId::INVALID_MODULE, 10, 200);

    //This check surpassed 1000 cases. See IOT-3997
    CheckAndClearStat(stat, MessageType::MODULE_GENERAL, ModuleId::STATUS_REPORTER_MODULE, 10, 2000, (u8)StatusReporterModule::StatusModuleGeneralMessages::LIVE_REPORT);

    CheckAndClearStat(stat, MessageType::MODULE_TRIGGER_ACTION, ModuleId::INVALID_MODULE, 10, 50);
    CheckAndClearStat(stat, MessageType::MODULE_ACTION_RESPONSE, ModuleId::INVALID_MODULE, 10, 50);

    //After checking for all expected messages, the stat should be empty
    checkStatEmpty(stat);
}

//#################################### Helpers for Statistic Tests #######################################

void CheckAndClearStat(PacketStat* stat, MessageType mt, ModuleId moduleId, u32 minCount, u32 maxCount, u8 actionType, u8 requestHandle)
{
    CheckAndClearStat(stat, mt, Utility::GetWrappedModuleId(moduleId), minCount, maxCount, actionType, requestHandle);
}

//Helper function that checks a given message type with its request handle for a maximum count and clears the message type for statistics it if it was ok
//Used for VendorModuleId & WrappedModuleIdU32
void CheckAndClearStat(PacketStat* stat, MessageType mt, ModuleIdWrapper moduleId, u32 minCount, u32 maxCount, u8 actionType, u8 requestHandle)
{
    for (u32 i = 0; i < PACKET_STAT_SIZE; i++) {
        PacketStat* entry = stat + i;
        if (entry->messageType == mt) {
            if (moduleId == INVALID_WRAPPED_MODULE_ID || (moduleId == entry->moduleId && actionType == entry->actionType)) {
                if (entry->count < minCount && entry->requestHandle == requestHandle) SIMEXCEPTION(IllegalStateException);
                if (entry->count > maxCount && entry->requestHandle == requestHandle) SIMEXCEPTION(IllegalStateException);
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
