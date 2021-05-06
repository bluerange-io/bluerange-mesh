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
#include <RuuviWeatherModule.h>
#include <initializer_list>

namespace
{
    void SetupRuuviTagSimConfiguration(SimConfiguration &simConfig, std::size_t nodeCount)
    {
        simConfig.terminalId = 0;
        simConfig.nodeConfigName.insert({ "prod_ruuvi_weather_nrf52", nodeCount });
        simConfig.SetToPerfectConditions();
    }

    void SetupRuuviTagCherrySimTester(CherrySimTester &tester, std::initializer_list<std::size_t> nodeIndices)
    {
        for (const auto nodeIndex : nodeIndices) {
            auto & node = tester.sim->nodes[nodeIndex];

            node.gs.logger.EnableTag("RUUVI");
            node.gs.logger.EnableTag("TIMESLOT");

            // Set the board id to that of the Ruuvi Tag so that the BME280
            // sensor has pins assigned (otherwise the sensor will not be
            // read and no broadcasts are generated).
            node.uicr.CUSTOMER[1] = 12;
        }
    }

    constexpr int GetClusteringTimeout()
    {
        return 60 * 1000;
    }
}

TEST(TestRuuviWeatherModule, TestSensorDataIsBroadcast) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    testerConfig.verbose = false;

    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    SetupRuuviTagSimConfiguration(simConfig, 4);

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    SetupRuuviTagCherrySimTester(tester, {0, 1, 2, 3});

    tester.Start();
    tester.SimulateUntilClusteringDone(GetClusteringTimeout());

    std::vector<SimulationMessage> messages = {
        // sensor data broadcasts from each node
        SimulationMessage{1, "sensor data sent to node 0"},
        SimulationMessage{2, "sensor data sent to node 0"},
        SimulationMessage{3, "sensor data sent to node 0"},
        SimulationMessage{4, "sensor data sent to node 0"},
        // sensor data received from each node on each node
        SimulationMessage{1, "sensor data received from node 1"},
        SimulationMessage{1, "sensor data received from node 2"},
        SimulationMessage{1, "sensor data received from node 3"},
        SimulationMessage{1, "sensor data received from node 4"},
        SimulationMessage{2, "sensor data received from node 1"},
        SimulationMessage{2, "sensor data received from node 2"},
        SimulationMessage{2, "sensor data received from node 3"},
        SimulationMessage{2, "sensor data received from node 4"},
        SimulationMessage{3, "sensor data received from node 1"},
        SimulationMessage{3, "sensor data received from node 2"},
        SimulationMessage{3, "sensor data received from node 3"},
        SimulationMessage{3, "sensor data received from node 4"},
        SimulationMessage{4, "sensor data received from node 1"},
        SimulationMessage{4, "sensor data received from node 2"},
        SimulationMessage{4, "sensor data received from node 3"},
        SimulationMessage{4, "sensor data received from node 4"},
    };

    tester.SimulateUntilMessagesReceived(100 * 1000, messages);
}

TEST(TestRuuviWeatherModule, TestActionConfigureAdvertiser) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    testerConfig.verbose = false;

    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    SetupRuuviTagSimConfiguration(simConfig, 2);

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    SetupRuuviTagCherrySimTester(tester, {0, 1});

    tester.Start();
    tester.SimulateUntilClusteringDone(GetClusteringTimeout());

    tester.SendTerminalCommand(1, "action 1 ruuvi_weather advertiser enable");
    tester.SimulateUntilMessageReceived(1000, 1, "advertiser enabled by node 1");

    tester.SendTerminalCommand(1, "action 1 ruuvi_weather advertiser disable");
    tester.SimulateUntilMessageReceived(1000, 1, "advertiser disabled by node 1");

    tester.SendTerminalCommand(1, "action 1 ruuvi_weather advertiser txPower -55");
    tester.SimulateUntilMessageReceived(1000, 1, "advertiser transmission power set to -40 by node 1");

    tester.SendTerminalCommand(1, "action 2 ruuvi_weather advertiser enable");
    tester.SimulateUntilMessageReceived(1000, 2, "advertiser enabled by node 1");

    tester.SendTerminalCommand(1, "action 2 ruuvi_weather advertiser disable");
    tester.SimulateUntilMessageReceived(1000, 2, "advertiser disabled by node 1");

    tester.SendTerminalCommand(1, "action 2 ruuvi_weather advertiser txPower 55");
    tester.SimulateUntilMessageReceived(1000, 2, "advertiser transmission power set to 4 by node 1");
}

TEST(TestRuuviWeatherModule, TestDataIsAdvertisedUsingTimeslots) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    testerConfig.verbose = false;

    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    SetupRuuviTagSimConfiguration(simConfig, 3);

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    SetupRuuviTagCherrySimTester(tester, {0, 1, 2});

    tester.Start();
    tester.SimulateGivenNumberOfSteps(10);

    tester.sim->nodes[0].gs.logger.EnableTag("FH");
    tester.sim->nodes[0].gs.logger.EnableTag("SIM");

    tester.SendTerminalCommand(2, "action this ruuvi_weather advertiser disable");
    tester.SendTerminalCommand(3, "action this ruuvi_weather advertiser disable");

    tester.SimulateUntilClusteringDone(GetClusteringTimeout());

    auto messages = std::vector<SimulationMessage>{
        {1, "sensor data received from node 1"},
        {1, "timeslot session successfully opened"},
    };
    tester.SimulateUntilMessagesReceived(30 * 1000, messages);

    messages = std::vector<SimulationMessage>{
        {1, "handling radio signal START"},
        {1, "radio task DISABLE triggered"},
        {1, "handling radio signal RADIO due to radio event DISABLED"},
    };
    tester.SimulateUntilMessagesReceived(1000, messages);

    messages = std::vector<SimulationMessage>{
        {1, "radio task TXEN triggered"},
        {1, "handling radio signal RADIO due to radio event DISABLED"},
    };
    tester.SimulateUntilMessagesReceived(1000, messages);

    messages = std::vector<SimulationMessage>{
        {1, "radio task TXEN triggered"},
        {1, "handling radio signal RADIO due to radio event DISABLED"},
    };
    tester.SimulateUntilMessagesReceived(1000, messages);

    messages = std::vector<SimulationMessage>{
        {1, "radio task TXEN triggered"},
        {1, "handling radio signal RADIO due to radio event DISABLED"},
    };
    tester.SimulateUntilMessagesReceived(1000, messages);

    tester.SimulateUntilMessageReceived(1000, 1, "timeslot is over");
}

