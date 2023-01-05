////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2022 M-Way Solutions GmbH
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

#include <HelperFunctions.h>
#include <CherrySimTester.h>
#include <CherrySimUtils.h>
#include <Node.h>
#include <vector>
#include <cmath>
#include "MeshAccessModule.h"

#if IS_ACTIVE(CLC_MODULE)
TEST(TestEnrollmentModule, TestCommands) {
    //Configure a clc sink and a mesh clc beacon
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;

    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert( { "prod_clc_mesh_nrf52", 1 } );
    simConfig.SetToPerfectConditions();

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);

    //First, enroll the mesh node into a different network
    tester.SendTerminalCommandToAllNodes("action 0 enroll basic %s 123 456 11:22:33:44:55:66:77:88:11:22:33:44:55:66:77:88", tester.sim->FindUniqueNodeByTerminalId(2)->gs.config.GetSerialNumber());

    tester.SimulateForGivenTime(50000);

    //Check if successfully enrolled in different network
    ASSERT_TRUE(tester.sim->nodes[1].gs.node.configuration.enrollmentState == EnrollmentState::ENROLLED);
    ASSERT_TRUE(tester.sim->nodes[1].gs.node.configuration.nodeId == 123);

    //Next, press the button for about 12 seconds and check if the node entered to unenrolled state
    tester.SendButtonPress(2, 1, 123);

    tester.SimulateUntilMessageReceived(100 * 1000, 2, "Unenrollment successful");

    ASSERT_TRUE(tester.sim->nodes[1].gs.node.configuration.enrollmentState == EnrollmentState::NOT_ENROLLED);
    ASSERT_TRUE(tester.sim->nodes[1].gs.node.configuration.networkId == 0);
}
#endif //ACTIVATE_CLC_MODULE


TEST(TestEnrollmentModule, TestFactoryReset) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert( { "prod_sink_nrf52", 1 } );
    simConfig.asyncFlashCommitTimeProbability = UINT32_MAX;
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    //Store some dummy values
    tester.SendTerminalCommand(1, "saverec 1337 AA:BB:CC:DD:EE:FF");
    tester.SimulateGivenNumberOfSteps(1);

    //Check that the data is present...
    tester.SendTerminalCommand(1, "getrec 1337");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "AA:BB:CC:DD:EE:FF");

    //...even after a reboot
    tester.SendTerminalCommand(1, "reset");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "\"type\":\"reboot\"");
    tester.SendTerminalCommand(1, "getrec 1337");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "AA:BB:CC:DD:EE:FF");

    //Next, unenroll the beacon ...
    tester.SendTerminalCommand(1, "action this enroll remove BBBBB");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "Unenrollment successful");

    //... and make sure that the data is no longer there (factory reset)
    tester.SendTerminalCommand(1, "getrec 1337");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Record not found");
}


TEST(TestEnrollmentModule, TestFactoryResetWithImmortalRecords) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1 });
    simConfig.asyncFlashCommitTimeProbability = UINT32_MAX;
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    //Store some dummy values
    tester.SendTerminalCommand(1, "saverec 2001 11:BB:CC:DD:EE:FF");
    tester.SimulateGivenNumberOfSteps(1);
    tester.SendTerminalCommand(1, "saverec 2002 22:BB:CC:DD:EE:FF");
    tester.SimulateGivenNumberOfSteps(1);
    tester.SendTerminalCommand(1, "saverec 2003 33:BB:CC:DD:EE:FF");
    tester.SimulateGivenNumberOfSteps(1);
    tester.SendTerminalCommand(1, "saverec 2004 44:BB:CC:DD:EE:FF");
    tester.SimulateGivenNumberOfSteps(1);
    tester.SendTerminalCommand(1, "saverec 2005 55");
    tester.SimulateGivenNumberOfSteps(1);

    //Make two records immortal
    {
        NodeIndexSetter setter(0);

        GS->recordStorage.ImmortalizeRecord(2002, nullptr, 0);
        cherrySimInstance->SimCommitFlashOperations();
        GS->recordStorage.ImmortalizeRecord(2005, nullptr, 0);
        cherrySimInstance->SimCommitFlashOperations();

    }

    //Next, unenroll the beacon which triggers a factory reset
    tester.SendTerminalCommand(1, "action this enroll remove BBBBB");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "Unenrollment successful");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, R"("type":"reboot","reason":16,)");

    //Check that all records are gone but immortals are still present
    tester.SendTerminalCommand(1, "getrec 2001");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Record not found");
    tester.SendTerminalCommand(1, "getrec 2002");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "22:BB:CC:DD:EE:FF: (6)");
    tester.SendTerminalCommand(1, "getrec 2003");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Record not found");
    tester.SendTerminalCommand(1, "getrec 2004");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Record not found");
    tester.SendTerminalCommand(1, "getrec 2005");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "55: (1)");

    //Now, deactivate one immortal record
    tester.SendTerminalCommand(1, "delrec 2005");
    tester.SimulateGivenNumberOfSteps(1);

    //Enroll and unenroll to do a factory reset
    tester.SendTerminalCommand(1, "action this enroll basic BBBBB 123 456 AA:BB:CC:DD:AA:BB:CC:DD:AA:BB:CC:DD:AA:BB:CC:DD");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, R"("type":"reboot","reason":9,)");
    tester.SendTerminalCommand(1, "action this enroll remove BBBBB");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "Unenrollment successful");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, R"("type":"reboot","reason":16,)");

    //Make sure only one immortal is present
    tester.SendTerminalCommand(1, "getrec 2002");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "22:BB:CC:DD:EE:FF: (6)");
    tester.SendTerminalCommand(1, "getrec 2005");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Record not found");

    //Make sure the record is not available, also not in deactivated form
    {
        NodeIndexSetter setter(0);
        RecordStorageRecord* record = GS->recordStorage.GetRecord(2005);
        ASSERT_TRUE(record == nullptr);
    }

    //Deactivate the other one as well
    tester.SendTerminalCommand(1, "delrec 2002");
    tester.SimulateGivenNumberOfSteps(1);

    //Enroll and unenroll to do a factory reset again
    tester.SendTerminalCommand(1, "action this enroll basic BBBBB 123 456 AA:BB:CC:DD:AA:BB:CC:DD:AA:BB:CC:DD:AA:BB:CC:DD");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, R"("type":"reboot","reason":9,)");
    tester.SendTerminalCommand(1, "action this enroll remove BBBBB");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "Unenrollment successful");

    //Should be gone as well
    tester.SendTerminalCommand(1, "getrec 2002");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Record not found");

    //Make sure page is empty
    {
        NodeIndexSetter setter(0);
        bool hasImmortal = GS->recordStorage.HasImmortalRecords();
        ASSERT_TRUE(hasImmortal == false);
    }
}
TEST(TestEnrollmentModule, TestEnrollmentBasicNewMesh) {
    //Configure a clc sink and a mesh clc beacon
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.defaultNetworkId = 0;
    simConfig.preDefinedPositions = { {0.997185, 0.932557},{0.715971, 0.802758},{0.446135, 0.522125},{0.865020, 0.829147},{0.935539, 0.846311},{0.783314, 0.612539},{0.910448, 0.698930},{0.593066, 0.671654},{0.660636, 0.598495},{0.939128, 0.778389} };
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", 10} );
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();


    tester.SimulateGivenNumberOfSteps(10);
    //Enrollments some times fail, thus we have to retry some times.
    for (int retry = 0; retry < 4; retry++)
    {
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBB 1 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 01:00:00:00:01:00:00:00:01:00:00:00:01:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBG 5 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 05:00:00:00:05:00:00:00:05:00:00:00:05:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBM 10 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 0A:00:00:00:0A:00:00:00:0A:00:00:00:0A:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBF 4 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 04:00:00:00:04:00:00:00:04:00:00:00:04:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBJ 7 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 07:00:00:00:07:00:00:00:07:00:00:00:07:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBC 2 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBH 6 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 06:00:00:00:06:00:00:00:06:00:00:00:06:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBL 9 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 09:00:00:00:09:00:00:00:09:00:00:00:09:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBK 8 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 08:00:00:00:08:00:00:00:08:00:00:00:08:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBD 3 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 03:00:00:00:03:00:00:00:03:00:00:00:03:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
    }


    tester.SimulateUntilClusteringDone(1000 * 1000);
}



TEST(TestEnrollmentModule, TestEnrollmentBasicExistingMesh) {
    //Configure a clc sink and a mesh clc beacon
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.defaultNetworkId = 0;
    simConfig.preDefinedPositions = { {0.997185, 0.932557},{0.715971, 0.802758},{0.446135, 0.522125},{0.865020, 0.829147},{0.935539, 0.846311},{0.783314, 0.612539},{0.910448, 0.698930},{0.593066, 0.671654},{0.660636, 0.598495},{0.939128, 0.778389} };
    //testerConfig.verbose = false;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 9 });
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();


    tester.SimulateGivenNumberOfSteps(10);
    //Enrollments some times fail, thus we have to retry some times.
    for (int retry = 0; retry < 4; retry++)
    {
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBB 1 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 01:00:00:00:01:00:00:00:01:00:00:00:01:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBG 5 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 05:00:00:00:05:00:00:00:05:00:00:00:05:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBM 10 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 0A:00:00:00:0A:00:00:00:0A:00:00:00:0A:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBF 4 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 04:00:00:00:04:00:00:00:04:00:00:00:04:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBJ 7 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 07:00:00:00:07:00:00:00:07:00:00:00:07:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBC 2 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBH 6 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 06:00:00:00:06:00:00:00:06:00:00:00:06:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBL 9 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 09:00:00:00:09:00:00:00:09:00:00:00:09:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBK 8 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 08:00:00:00:08:00:00:00:08:00:00:00:08:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBD 3 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 03:00:00:00:03:00:00:00:03:00:00:00:03:00:00:00");
        tester.SimulateGivenNumberOfSteps(100);
    }

    tester.SimulateUntilClusteringDone(1000 * 1000);
}

TEST(TestEnrollmentModule, TestEnrollmentBasicExistingMeshLong) {
    //Configure a clc sink and a mesh clc beacon
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.defaultNetworkId = 0;
    simConfig.preDefinedPositions = { {0.997185, 0.932557},{0.715971, 0.802758},{0.446135, 0.522125},{0.865020, 0.829147},{0.935539, 0.846311},{0.783314, 0.612539},{0.910448, 0.698930},{0.593066, 0.671654},{0.660636, 0.598495},{0.939128, 0.778389} };
    //testerConfig.verbose = false;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 9 });
    simConfig.SetToPerfectConditions();

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();


    tester.SimulateGivenNumberOfSteps(10);
    //Enrollments some times fail, thus we have to retry some times.
    for (int retry = 0; retry < 4; retry++)
    {
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBB 1 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 01:00:00:00:01:00:00:00:01:00:00:00:01:00:00:00 10 0 0");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBG 5 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 05:00:00:00:05:00:00:00:05:00:00:00:05:00:00:00 10 0 0");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBM 10 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 0A:00:00:00:0A:00:00:00:0A:00:00:00:0A:00:00:00 10 0 0");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBF 4 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 04:00:00:00:04:00:00:00:04:00:00:00:04:00:00:00 10 0 0");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBJ 7 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 07:00:00:00:07:00:00:00:07:00:00:00:07:00:00:00 10 0 0");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBC 2 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00 10 0 0");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBH 6 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 06:00:00:00:06:00:00:00:06:00:00:00:06:00:00:00 10 0 0");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBL 9 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 09:00:00:00:09:00:00:00:09:00:00:00:09:00:00:00 10 0 0");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBK 8 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 08:00:00:00:08:00:00:00:08:00:00:00:08:00:00:00 10 0 0");
        tester.SimulateGivenNumberOfSteps(100);
        tester.SendTerminalCommand(1, "action 0 enroll basic BBBBD 3 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 03:00:00:00:03:00:00:00:03:00:00:00:03:00:00:00 10 0 0");
        tester.SimulateGivenNumberOfSteps(100);
    }


    tester.SimulateUntilClusteringDone(1000 * 1000);
}

TEST(TestEnrollmentModule, TestReceivingEnrollmentOverMeshResponses) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //Place nodes such that they are only reachable in a line.
    simConfig.preDefinedPositions = { {0.2, 0.5}, {0.4, 0.55}, {0.6, 0.5}, {0.8, 0.55}};

    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 3});
    simConfig.SetToPerfectConditions();
    //testerConfig.verbose = false;

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    //Change the default network id of node 3 so that only node 1, 2, and 3 are able to create a mesh.
    tester.sim->nodes[3].uicr.CUSTOMER[9] = 123;
    tester.Start();

    //Wait until nodes 1, 2 and 3 are connected
    tester.SimulateUntilClusteringDoneWithExpectedNumberOfClusters(1 * 60 * 1000, 2);

    RetryOrFail<TimeoutException>(
        32, [&] {
            //Request enrollment of the currently unenrolled node from node 1 on node 3
            tester.SendTerminalCommand(1, "action 3 enroll basic BBBBF 4 3678 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 04:00:00:00:04:00:00:00:04:00:00:00:04:00:00:00 10 0 1");
        },
        [&] {
            //Response is sent from the node that executed the enrollment
            tester.SimulateUntilMessageReceived(1 * 60 * 1000, 1, "{\"nodeId\":3,\"type\":\"enroll_response_serial\",\"module\":5,\"requestId\":1,\"serialNumber\":\"BBBBF\",\"code\":0}");
        });
}

TEST(TestEnrollmentModule, TestEnrollmentMultipleTimes) {

    for (u32 seed = 0; seed < 2; seed++) {
        printf("############## Seed %u #############", seed);

        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
        simConfig.seed = seed + 1;
        simConfig.defaultNetworkId = 0;
        simConfig.mapWidthInMeters = 5;
        simConfig.mapHeightInMeters = 5;
        //testerConfig.verbose = true;
        simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
        simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 9 });
        simConfig.SetToPerfectConditions();
        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
        tester.Start();

        std::vector<std::string> messages = {
            "action 0 enroll basic BBBBB 1 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 01:00:00:00:01:00:00:00:01:00:00:00:01:00:00:00 10 0 0",
            "action 0 enroll basic BBBBC 2 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00 10 0 0",
            "action 0 enroll basic BBBBD 3 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 03:00:00:00:03:00:00:00:03:00:00:00:03:00:00:00 10 0 0",
            "action 0 enroll basic BBBBF 4 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 04:00:00:00:04:00:00:00:04:00:00:00:04:00:00:00 10 0 0",
            "action 0 enroll basic BBBBG 5 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 05:00:00:00:05:00:00:00:05:00:00:00:05:00:00:00 10 0 0",
            "action 0 enroll basic BBBBH 6 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 06:00:00:00:06:00:00:00:06:00:00:00:06:00:00:00 10 0 0",
            "action 0 enroll basic BBBBJ 7 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 07:00:00:00:07:00:00:00:07:00:00:00:07:00:00:00 10 0 0",
            "action 0 enroll basic BBBBK 8 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 08:00:00:00:08:00:00:00:08:00:00:00:08:00:00:00 10 0 0",
            "action 0 enroll basic BBBBL 9 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 09:00:00:00:09:00:00:00:09:00:00:00:09:00:00:00 10 0 0",
            "action 0 enroll basic BBBBM 10 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 0A:00:00:00:0A:00:00:00:0A:00:00:00:0A:00:00:00 10 0 0",
        };

        for (size_t nodeIndex = 0; nodeIndex < messages.size(); nodeIndex++) {
            //Make sure that no enroll_response messages are floating around
            tester.SimulateForGivenTime(10 * 1000);
            for (int i = 0; i < 1000; i++) {
                tester.SendTerminalCommand(1, messages[nodeIndex].c_str());
                try {
                    Exceptions::DisableDebugBreakOnException ddboe;
                    tester.SimulateUntilMessageReceived(5 * 1000, 1, "enroll_response");
                    break;
                }
                catch (const TimeoutException &e) {

                }
            }
        }

        //Check that all nodes have the correct nodeId
        for (size_t i = 0; i < messages.size(); i++) {
            if (tester.sim->nodes[i].gs.node.configuration.nodeId != i + 1) {
                SIMEXCEPTION(IllegalStateException);
            }
        }

        tester.SimulateUntilClusteringDone(50 * 1000);
    }
}


//This test is disabled on Github because the github featureset set overwrites the networkid
#ifndef GITHUB_RELEASE
TEST(TestEnrollmentModule, TestRequestProposals) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = false;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 9 });
    //Place Node 0 and 1 in the middle, Node 2 far away, and the others in a circle around them.
    simConfig.preDefinedPositions.push_back({ 0.51, 0.5 });
    simConfig.preDefinedPositions.push_back({ 0.49, 0.5 });
    simConfig.preDefinedPositions.push_back({ 0.99, 0.5 });
    simConfig.SetToPerfectConditions();
    for (u32 i = 3; i < 10; i++)
    {
        double percentage = (double)i / (double)(10 - 3);
        simConfig.preDefinedPositions.push_back({
            std::sin(percentage * 3.14 * 2) * 0.1 + 0.5,
            std::cos(percentage * 3.14 * 2) * 0.1 + 0.5,
            });
    }

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    // This test relies on the propagation constant being 2.5
    tester.sim->propagationConstant = 2.5;

    //Set all the networkids except the middle ones to 0.
    for (u32 i = 2; i < tester.sim->GetTotalNodes(); i++)
    {
        tester.sim->nodes[i].uicr.CUSTOMER[9] = 0;
    }

    tester.Start();

    //Make sure node 0 and 1 are connected.
    tester.SimulateUntilClusteringDoneWithExpectedNumberOfClusters(1000 * 1000, 9);

    tester.SendTerminalCommand(1, "action 0 enroll request_proposals BBBBD BBBBF BBBBG");
    std::vector<SimulationMessage> messages = {
        //Messages that make sure that the request_proposals are set.
        SimulationMessage(1, "{\"nodeId\":1,\"type\":\"request_proposals\",\"serialNumbers\":[\"BBBBD\", \"BBBBF\", \"BBBBG\"], \"module\":5,\"requestHandle\":0}"),
        SimulationMessage(2, "{\"nodeId\":1,\"type\":\"request_proposals\",\"serialNumbers\":[\"BBBBD\", \"BBBBF\", \"BBBBG\"], \"module\":5,\"requestHandle\":0}"),

        //Messages that make sure that the responses are delivered
        SimulationMessage(1, "{\"nodeId\":1,\"type\":\"request_proposals_response\",\"serialNumber\":\"BBBBF\",\"module\":5,\"requestHandle\":0}"),
        SimulationMessage(1, "{\"nodeId\":1,\"type\":\"request_proposals_response\",\"serialNumber\":\"BBBBG\",\"module\":5,\"requestHandle\":0}"),
        SimulationMessage(1, "{\"nodeId\":2,\"type\":\"request_proposals_response\",\"serialNumber\":\"BBBBF\",\"module\":5,\"requestHandle\":0}"),
        SimulationMessage(1, "{\"nodeId\":2,\"type\":\"request_proposals_response\",\"serialNumber\":\"BBBBG\",\"module\":5,\"requestHandle\":0}"),
    };
    tester.SimulateUntilMessagesReceived(10 * 1000, messages);
    
    {
        //Send the previous command again and make sure that no request_proposals_responses about serialIndex 2 are deliverd (because it is too far away)
        Exceptions::ExceptionDisabler<TimeoutException> te;
        tester.SendTerminalCommand(1, "action 0 enroll request_proposals BBBBD BBBBF BBBBG");
        messages = {
            SimulationMessage(1, "\"type\":\"request_proposals_response\",\"serialNumber\":\"BBBBD\",\"module\":5,\"requestHandle\":0}"),
        };
        tester.SimulateUntilMessagesReceived(10 * 1000, messages);
        ASSERT_TRUE(tester.sim->CheckExceptionWasThrown(typeid(TimeoutException)));
    }

    //Test the command with the maximum number (11) of allowed indices.
    tester.SendTerminalCommand(1, "action 0 enroll request_proposals BBBBD BBBBF BBBBG BBBBH BBBBJ BBBBK BBBBL BBBBM BBBBN BBBBP BBBBQ");
    messages = {
        SimulationMessage(1, "{\"nodeId\":1,\"type\":\"request_proposals\",\"serialNumbers\":[\"BBBBD\", \"BBBBF\", \"BBBBG\", \"BBBBH\", \"BBBBJ\", \"BBBBK\", \"BBBBL\", \"BBBBM\", \"BBBBN\", \"BBBBP\", \"BBBBQ\"], \"module\":5,\"requestHandle\":0}"),
        SimulationMessage(2, "{\"nodeId\":1,\"type\":\"request_proposals\",\"serialNumbers\":[\"BBBBD\", \"BBBBF\", \"BBBBG\", \"BBBBH\", \"BBBBJ\", \"BBBBK\", \"BBBBL\", \"BBBBM\", \"BBBBN\", \"BBBBP\", \"BBBBQ\"], \"module\":5,\"requestHandle\":0}"),
    };
    tester.SimulateUntilMessagesReceived(10 * 1000, messages);
}
#endif

#if defined(PROD_SINK_USB_NRF52840)
//This test should make sure that we can configure a featureset to only use a temporary enrollment
//that is not persisted in flash
TEST(TestEnrollmentModule, TestTemporaryEnrollment) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;

    simConfig.nodeConfigName.insert({ "prod_sink_usb_nrf52840", 1 });
    simConfig.SetToPerfectConditions();
    //testerConfig.verbose = true;

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    //Set the default network id to 0
    tester.sim->nodes[0].uicr.CUSTOMER[9] = 0;

    tester.Start();

    // #####
    // ##### Make sure that we have an unenrolled node
    // #####

    //Make sure the node is not enrolled by default
    tester.SendTerminalCommand(1, "status");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Enrolled 0: networkId:0");

    // #####
    // ##### Enroll the node in a test network
    // ##### => Make sure it is enrolled after the reboot
    // #####

    //printf("\n\n\n\n############################ STEP 1 ##########################\n\n\n\n" EOL);

    //NodeKey AA...., UserBaseKey BB...., OrgaKey CC......
    tester.SendTerminalCommand(1, "action this enroll basic BBBBB 2 123 AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:00 BB:BB:BB:BB:BB:BB:BB:BB:BB:BB:BB:BB:BB:BB:BB:00 CC:CC:CC:CC:CC:CC:CC:CC:CC:CC:CC:CC:CC:CC:CC:00");

    //Give the node some time to enroll and reboot
    tester.SimulateUntilMessageReceived(10 * 1000, 1, R"({"nodeId":2,"type":"enroll_response_serial","module":5,"requestId":0,"serialNumber":"BBBBB","code":0})");

    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Reboot reason");

    //Make sure the node is now properly enrolled
    tester.SendTerminalCommand(1, "status");

    std::vector<SimulationMessage> messages = {
        SimulationMessage(1, "Node BBBBB (nodeId: 2)"),
        SimulationMessage(1, "Enrolled 1: networkId:123, deviceType:3, NetKey AA:AA:....:AA:00, UserBaseKey BB:BB:....:BB:00"),
    };
    tester.SimulateUntilMessagesReceived(10 * 1000, messages);

    //printf("\n\n\n\n############################ STEP 2 ##########################\n\n\n\n" EOL);

    // #####
    // ##### Use set_serial to assign a different serial number to the node (bridge mode)
    // ##### => Make sure Enrollment is still available
    // #####

    tester.SendTerminalCommand(1, "set_serial CCCCC");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Reboot reason was 19");

    tester.SendTerminalCommand(1, "status");

    messages = {
        SimulationMessage(1, "Node CCCCC (nodeId: 2)"),
        SimulationMessage(1, "Enrolled 1: networkId:123, deviceType:3, NetKey AA:AA:....:AA:00, UserBaseKey BB:BB:....:BB:00"),
    };
    tester.SimulateUntilMessagesReceived(10 * 1000, messages);

    //printf("\n\n\n\n############################ STEP 3 ##########################\n\n\n\n" EOL);

    // #####
    // ##### Use set_nodekey to assign a different node key to the node (bridge mode)
    // ##### => Make sure Enrollment is still available
    // #####

    tester.SendTerminalCommand(1, "set_node_key DD:DD:DD:DD:DD:DD:DD:DD:DD:DD:DD:DD:DD:DD:DD:00");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Node Key set");

    tester.SendTerminalCommand(1, "status");

    messages = {
        SimulationMessage(1, "Node CCCCC \\(nodeId: 2\\) vers: \\d+, NodeKey: DD:DD:....:DD:00"),
        SimulationMessage(1, "Enrolled 1: networkId:123, deviceType:3, NetKey AA:AA:....:AA:00, UserBaseKey BB:BB:....:BB:00"),
    };
    tester.SimulateUntilRegexMessagesReceived(10 * 1000, messages);

    //printf("\n\n\n\n############################ STEP 4 ##########################\n\n\n\n" EOL);

    // #####
    // ##### Do another soft reset
    // ##### => Make sure Enrollment, node key and serial number are still available
    // #####

    tester.SendTerminalCommand(1, "reset");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Reboot reason was 7");

    tester.SendTerminalCommand(1, "status");

    messages = {
        SimulationMessage(1, "Node CCCCC \\(nodeId: 2\\) vers: \\d+, NodeKey: DD:DD:....:DD:00"),
        SimulationMessage(1, "Enrolled 1: networkId:123, deviceType:3, NetKey AA:AA:....:AA:00, UserBaseKey BB:BB:....:BB:00"),
    };
    tester.SimulateUntilRegexMessagesReceived(10 * 1000, messages);

    //printf("\n\n\n\n############################ STEP 5 ##########################\n\n\n\n" EOL);

    // #####
    // ##### Simulate a power loss
    // ##### => Make sure Enrollment is gone
    // #####

    {
        NodeIndexSetter setter(0);
        tester.sim->ResetCurrentNode(RebootReason::UNKNOWN, false, true);
        tester.SimulateForGivenTime(1 * 1000);
    }

    //Check that the enrollment is gone after a power loss
    tester.SendTerminalCommand(1, "status");

    messages = {
        SimulationMessage(1, "Node BBBBB \\(nodeId: 1\\) vers: \\d+, NodeKey: 01:00:....:00:00"),
        SimulationMessage(1, "Enrolled 0: networkId:0, deviceType:3, NetKey 04:00:....:00:00, UserBaseKey FF:FF:....:FF:FF"),
    };
    tester.SimulateUntilRegexMessagesReceived(10 * 1000, messages);

    //printf("\n\n\n\n############################ STEP 6 ##########################\n\n\n\n" EOL);

    // #####
    // ##### Enroll Again
    // ##### => Make sure enrollment works again
    // #####

    //NodeKey AA...., UserBaseKey BB...., OrgaKey CC......
    tester.SendTerminalCommand(1, "action this enroll basic BBBBB 2 123 AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:00 BB:BB:BB:BB:BB:BB:BB:BB:BB:BB:BB:BB:BB:BB:BB:00 CC:CC:CC:CC:CC:CC:CC:CC:CC:CC:CC:CC:CC:CC:CC:00");

    //Give the node some time to enroll and reboot
    tester.SimulateForGivenTime(10 * 1000);

    //Make sure the node is now properly enrolled
    tester.SendTerminalCommand(1, "status");
    messages = {
        SimulationMessage(1, "Node BBBBB (nodeId: 2)"),
        SimulationMessage(1, "Enrolled 1: networkId:123, deviceType:3, NetKey AA:AA:....:AA:00, UserBaseKey BB:BB:....:BB:00"),
    };
    tester.SimulateUntilMessagesReceived(10 * 1000, messages);

    //printf("\n\n\n\n############################ STEP 7 ##########################\n\n\n\n" EOL);

    // #####
    // ##### Unenroll
    // ##### => Make sure enrollment is gone
    // #####

    tester.SendTerminalCommand(1, "action this enroll remove BBBBB");

    //Check that the unenrollment message is generated
    tester.SimulateUntilMessageReceived(10 * 1000, 1, R"({"nodeId":1,"type":"remove_enroll_response_serial","module":5,"requestId":0,"serialNumber":"BBBBB","code":0})");

    //Next, it must reboot
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Reboot reason was 16");

    //Make sure the node is now properly enrolled
    tester.SendTerminalCommand(1, "status");

    messages = {
        SimulationMessage(1, "Node BBBBB \\(nodeId: 1\\) vers: \\d+, NodeKey: 01:00:....:00:00"),
        SimulationMessage(1, "Enrolled 0: networkId:0, deviceType:3, NetKey 04:00:....:00:00, UserBaseKey FF:FF:....:FF:FF"),
    };
    tester.SimulateUntilRegexMessagesReceived(10 * 1000, messages);

}
#endif

#ifdef PROD_SINK_USB_NRF52840
//This test makes sure that a factory reset is triggered if there is a persistent
//enrollment on a node but its featureset specifies that it should not use persistence
//This is mostly relevant for migration
TEST(TestEnrollmentModule, TestFactoryResetForTemporaryEnrollment)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;

    simConfig.nodeConfigName.insert({ "prod_sink_usb_nrf52840", 1 });
    simConfig.SetToPerfectConditions();
    //testerConfig.verbose = true;

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    //Set the default network id to 0
    tester.sim->nodes[0].uicr.CUSTOMER[9] = 0;

    //Write an enrollment in the flash config
    NodeConfiguration config;
    CheckedMemset(&config, 0x00, sizeof(config));
    config.moduleId = ModuleId::NODE;
    config.moduleVersion = NODE_MODULE_CONFIG_VERSION;
    config.moduleActive = 1;
    config.enrollmentState = EnrollmentState::ENROLLED;
    config.nodeId = 1;
    config.networkId = 123;
    config.bleAddress.addr_type = FruityHal::BleGapAddrType::INVALID;
    STATIC_ASSERT_SIZE(config, 68); //Must be changed once the configuration changes
    NodeIndexSetter setter(0);
    tester.sim->WriteRecordToFlash((u16)ModuleId::NODE, (u8*)&config, sizeof(config));


    tester.Start();

    //First, make sure our stored enrollment was available
    tester.SendTerminalCommand(1, "status");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Enrolled 1: networkId:123");

    //Next, the node should reboot because of a FACTORY_RESET
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Reboot reason was 30");

    //After the reboot, the enrollment should be gone
    tester.SendTerminalCommand(1, "status");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Enrolled 0: networkId:0");
}
#endif