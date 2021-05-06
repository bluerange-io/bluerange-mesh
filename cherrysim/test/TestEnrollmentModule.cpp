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
#include <vector>
#include <cmath>
#include "MeshAccessModule.h"

#if IS_ACTIVE(CLC_MODULE)
TEST(TestEnrollmentModule, TestCommands) {
    //Configure a clc sink and a mesh clc beacon
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();

    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert( { "prod_clc_mesh_nrf52", 1 } );
    simConfig.SetToPerfectConditions();
    testerConfig.verbose = false;

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);

    //First, enroll the mesh node into a different network
    tester.SendTerminalCommand(0, "action 0 enroll basic %s 123 456 11:22:33:44:55:66:77:88:11:22:33:44:55:66:77:88", tester.sim->nodes[1].gs.config.GetSerialNumber());

    tester.SimulateForGivenTime(50000);

    //Check if successfully enrolled in different network
    ASSERT_TRUE(tester.sim->nodes[1].gs.node.configuration.enrollmentState == EnrollmentState::ENROLLED);
    ASSERT_TRUE(tester.sim->nodes[1].gs.node.configuration.nodeId == 123);

    //Next, press the button for two seconds and check if the node entered to unenrolled state
    tester.SendButtonPress(1, 1, 23);

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
    testerConfig.verbose = false;
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
    testerConfig.verbose = false;
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
    testerConfig.verbose = false;

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    //Change the default network id of node 3 so that only node 1, 2, and 3 are able to create a mesh.
    tester.sim->nodes[3].uicr.CUSTOMER[9] = 123;
    tester.Start();

    //Wait until nodes 1, 2 and 3 are connected
    tester.SimulateForGivenTime(15 * 1000);

    //Connect node 1 via a mesh access connection to node 2
    tester.SendTerminalCommand(1, "action 3 enroll basic BBBBF 4 3678 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 04:00:00:00:04:00:00:00:04:00:00:00:04:00:00:00 10 0 1");
    
    //Response is sent from the node that executed the enrollment
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":3,\"type\":\"enroll_response_serial\",\"module\":5,\"requestId\":1,\"serialNumber\":\"BBBBF\",\"code\":0}");
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
    testerConfig.verbose = false;
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
        Exceptions::DisableDebugBreakOnException ddboe;
        tester.SendTerminalCommand(1, "action 0 enroll request_proposals BBBBD BBBBF BBBBG");
        messages = {
            SimulationMessage(1, "\"type\":\"request_proposals_response\",\"serialNumber\":\"BBBBD\",\"module\":5,\"requestHandle\":0}"),
        };
        ASSERT_THROW(tester.SimulateUntilMessagesReceived(10 * 1000, messages), TimeoutException);
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
