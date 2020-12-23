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
#include "DebugModule.h"
#include <string>
#include "Exceptions.h"

TEST(TestRawData, TestRawDataLight) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.Start();
    tester.SimulateUntilClusteringDone(100 * 1000);

    tester.SendTerminalCommand(1, "raw_data_light 2 0 1 abcdeQ==");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_light\",\"module\":0,\"protocol\":1,\"payload\":\"abcdeQ==\",\"requestHandle\":0}");
    tester.SendTerminalCommand(1, "raw_data_light 2 0 1 abcdeQ== 3");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_light\",\"module\":0,\"protocol\":1,\"payload\":\"abcdeQ==\",\"requestHandle\":3}");

    //Test sending raw_data_light for a vendor module
    tester.SendTerminalCommand(1, "raw_data_light 2 0xABCD01F0 1 abcdeQ==");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_light\",\"module\":\"0xABCD01F0\",\"protocol\":1,\"payload\":\"abcdeQ==\",\"requestHandle\":0}");

    {
        std::string buffer = "";
        for (int i = 0; i < 40; i++)
        {
            buffer += "aaaa";

            std::string command = "raw_data_light 2 0 42 ";
            tester.SendTerminalCommand(1, (command + buffer).c_str());
            tester.SimulateUntilMessageReceived(10 * 1000, 2, buffer.c_str());
        }
    }
}

TEST(TestRawData, TestSimpleTransmissions) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.Start();
    tester.SimulateUntilClusteringDone(100 * 1000);

    tester.SendTerminalCommand(1, "raw_data_start 2 0 128 2");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_start\",\"module\":0,\"numChunks\":128,\"protocol\":2,\"fmKeyId\":0,\"requestHandle\":0}");
    tester.SendTerminalCommand(1, "raw_data_start 2 1 128 2 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_start\",\"module\":1,\"numChunks\":128,\"protocol\":2,\"fmKeyId\":0,\"requestHandle\":12}");
    tester.SendTerminalCommand(1, "raw_data_start 2 0xABCD01F0 128 2 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_start\",\"module\":\"0xABCD01F0\",\"numChunks\":128,\"protocol\":2,\"fmKeyId\":0,\"requestHandle\":12}");

    tester.SendTerminalCommand(1, "raw_data_start_received 2 0");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_start_received\",\"module\":0,\"requestHandle\":0}");
    tester.SendTerminalCommand(1, "raw_data_start_received 2 1 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_start_received\",\"module\":1,\"requestHandle\":12}");
    tester.SendTerminalCommand(1, "raw_data_start_received 2 0xABCD01F0 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_start_received\",\"module\":\"0xABCD01F0\",\"requestHandle\":12}");

    tester.SendTerminalCommand(1, "raw_data_chunk 2 0 42 abcdeQ==");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_chunk\",\"module\":0,\"chunkId\":42,\"payload\":\"abcdeQ==\",\"requestHandle\":0}");
    tester.SendTerminalCommand(1, "raw_data_chunk 2 1 42 abcdeQ== 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_chunk\",\"module\":1,\"chunkId\":42,\"payload\":\"abcdeQ==\",\"requestHandle\":12}");
    tester.SendTerminalCommand(1, "raw_data_chunk 2 0xABCD01F0 42 abcdeQ== 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_chunk\",\"module\":\"0xABCD01F0\",\"chunkId\":42,\"payload\":\"abcdeQ==\",\"requestHandle\":12}");

    tester.SendTerminalCommand(1, "raw_data_report 2 0 -");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_report\",\"module\":0,\"missing\":[],\"requestHandle\":0}");
    tester.SendTerminalCommand(1, "raw_data_report 2 0xABCD01F0 -");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_report\",\"module\":\"0xABCD01F0\",\"missing\":[],\"requestHandle\":0}");
    tester.SendTerminalCommand(1, "raw_data_report 2 1 11");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_report\",\"module\":1,\"missing\":[11],\"requestHandle\":0}");
    tester.SendTerminalCommand(1, "raw_data_report 2 0 11,31");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_report\",\"module\":0,\"missing\":[11,31],\"requestHandle\":0}");
    tester.SendTerminalCommand(1, "raw_data_report 2 0 11,31,66");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_report\",\"module\":0,\"missing\":[11,31,66],\"requestHandle\":0}");
    tester.SendTerminalCommand(1, "raw_data_report 2 0 - 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_report\",\"module\":0,\"missing\":[],\"requestHandle\":12}");
    tester.SendTerminalCommand(1, "raw_data_report 2 0 11 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_report\",\"module\":0,\"missing\":[11],\"requestHandle\":12}");
    tester.SendTerminalCommand(1, "raw_data_report 2 0 11,31 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_report\",\"module\":0,\"missing\":[11,31],\"requestHandle\":12}");
    tester.SendTerminalCommand(1, "raw_data_report 2 1 11,31,66 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_report\",\"module\":1,\"missing\":[11,31,66],\"requestHandle\":12}");
    tester.SendTerminalCommand(1, "raw_data_report 2 0xABCD01F0 11,31,66 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_report\",\"module\":\"0xABCD01F0\",\"missing\":[11,31,66],\"requestHandle\":12}");

    tester.SendTerminalCommand(1, "raw_data_report_desired 2 13");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_report_desired\",\"module\":13,\"requestHandle\":0}");
    tester.SendTerminalCommand(1, "raw_data_report_desired 2 0xABCD11F0");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_report_desired\",\"module\":\"0xABCD11F0\",\"requestHandle\":0}");
    tester.SendTerminalCommand(1, "raw_data_report_desired 2 11 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_report_desired\",\"module\":11,\"requestHandle\":12}");
    tester.SendTerminalCommand(1, "raw_data_report_desired 2 0xABCD01F0 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_report_desired\",\"module\":\"0xABCD01F0\",\"requestHandle\":12}");

    tester.SendTerminalCommand(1, "raw_data_error 2 0 1 1");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_error\",\"module\":0,\"error\":1,\"destination\":1,\"requestHandle\":0}");
    tester.SendTerminalCommand(1, "raw_data_error 2 1 1 3 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_error\",\"module\":1,\"error\":1,\"destination\":3,\"requestHandle\":12}");
    tester.SendTerminalCommand(1, "raw_data_error 2 0xABCD01F0 1 3 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":1,\"type\":\"raw_data_error\",\"module\":\"0xABCD01F0\",\"error\":1,\"destination\":3,\"requestHandle\":12}");
}

TEST(TestRawData, TestSimpleTransmissionsViaMeshAccess) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.sim->nodes[0].uicr.CUSTOMER[9] = 100; // Change default network id of node 1
    tester.sim->nodes[1].uicr.CUSTOMER[9] = 123; // Change default network id of node 2

    tester.Start();

    //Wait for establishing mesh access connection
    tester.SendTerminalCommand(1, "action this ma connect 00:00:00:02:00:00 2");
    tester.SimulateForGivenTime(300000);

    tester.SendTerminalCommand(1, "raw_data_start 2001 0 128 2 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":2002,\"type\":\"raw_data_start\",\"module\":0,\"numChunks\":128,\"protocol\":2,\"fmKeyId\":0,\"requestHandle\":12}");

    tester.SendTerminalCommand(1, "raw_data_start_received 2001 0 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":2002,\"type\":\"raw_data_start_received\",\"module\":0,\"requestHandle\":12}");

    tester.SendTerminalCommand(1, "raw_data_chunk 2001 0 42 abcdeQ== 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":2002,\"type\":\"raw_data_chunk\",\"module\":0,\"chunkId\":42,\"payload\":\"abcdeQ==\",\"requestHandle\":12}");

    tester.SendTerminalCommand(1, "raw_data_report 2001 0 -");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":2002,\"type\":\"raw_data_report\",\"module\":0,\"missing\":[],\"requestHandle\":0}");

    tester.SendTerminalCommand(1, "raw_data_error 2001 0 1 3 12");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":2002,\"type\":\"raw_data_error\",\"module\":0,\"error\":1,\"destination\":3,\"requestHandle\":12}");
}

TEST(TestRawData, TestSimpleTransmissionsViaMeshAccessFail) {
    Exceptions::DisableDebugBreakOnException disable;
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.sim->nodes[0].uicr.CUSTOMER[9] = 100; // Change default network id of node 1
    tester.sim->nodes[1].uicr.CUSTOMER[9] = 123; // Change default network id of node 2

    tester.Start();

    //Wait for establishing mesh access connection
    tester.SendTerminalCommand(1, "action this ma connect 00:00:00:02:00:00 3");
    tester.SimulateForGivenTime(300000);

    tester.SendTerminalCommand(1, "raw_data_start 2001 0 128 2 12");
    ASSERT_THROW(tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":2002,\"type\":\"raw_data_start\",\"module\":0,\"numChunks\":128,\"protocol\":2,\"fmKeyId\":0,\"requestHandle\":12}"), TimeoutException);

    tester.SendTerminalCommand(1, "raw_data_start_received 2001 0 12");
    ASSERT_THROW(tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":2002,\"type\":\"raw_data_start_received\",\"module\":0,\"requestHandle\":12}"), TimeoutException);

    tester.SendTerminalCommand(1, "raw_data_chunk 2001 0 42 abcdeQ== 12");
    ASSERT_THROW(tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":2002,\"type\":\"raw_data_chunk\",\"module\":0,\"chunkId\":42,\"payload\":\"abcdeQ==\",\"requestHandle\":12}"), TimeoutException);

    tester.SendTerminalCommand(1, "raw_data_report 2001 0 -");
    ASSERT_THROW(tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":2002,\"type\":\"raw_data_report\",\"module\":0,\"missing\":[],\"requestHandle\":0}"), TimeoutException);

    tester.SendTerminalCommand(1, "raw_data_error 2001 0 1 3 12");
    ASSERT_THROW(tester.SimulateUntilMessageReceived(10 * 1000, 2, "{\"nodeId\":2002,\"type\":\"raw_data_error\",\"module\":0,\"error\":1,\"destination\":3,\"requestHandle\":12}"), TimeoutException);
}

TEST(TestRawData, TestTransmissionsOfAllPossibleSizes) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.Start();
    tester.SimulateUntilClusteringDone(100 * 1000);

    {
        std::string buffer = "";
        for (int i = 0; i < 40; i++)
        {
            buffer += "aaaa";

            std::string command = "raw_data_chunk 2 0 42 ";
            tester.SendTerminalCommand(1, (command + buffer).c_str());
            tester.SimulateUntilMessageReceived(10 * 1000, 2, buffer.c_str());
        }
    }
}

TEST(TestRawData, TestRandomTransmissions) {
    const int numNodes = 10;

    for (int repeat = 0; repeat < 3; repeat++) {
        CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
        //testerConfig.verbose = true;
        SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
        simConfig.sdBusyProbability = 0;
        simConfig.seed = repeat + 1;
        simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
        simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", numNodes - 1});
        simConfig.SetToPerfectConditions();
        CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

        tester.Start();
        tester.SimulateUntilClusteringDone(1000 * 1000);
        tester.SimulateForGivenTime(10 * 1000); //Give the mesh some additional time to clear the vital and other queues
        u8 payloadBuffer[120];

        for (int transmission = 0; transmission < 50; transmission++) {
            int sender   = tester.sim->simState.rnd.NextU32(1, numNodes);
            int receiver = tester.sim->simState.rnd.NextU32(1, numNodes);

            std::string command = "";
            command = "raw_data_chunk " + std::to_string(receiver) + " 0 42 ";
            int length = tester.sim->simState.rnd.NextU32(1, 60);
            
            for (int i = 0; i < length; i++) {
                payloadBuffer[i] = tester.sim->simState.rnd.NextU32(0, 255);
            }

            char base64Buffer[1024];
            Logger::ConvertBufferToBase64String(payloadBuffer, length, base64Buffer, sizeof(base64Buffer));

            tester.SendTerminalCommand(sender, (command + base64Buffer).c_str());
            tester.SimulateUntilMessageReceived(10 * 1000, receiver, base64Buffer);
        }
    }
}
