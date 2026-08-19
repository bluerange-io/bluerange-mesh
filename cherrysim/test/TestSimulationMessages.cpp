////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// ** BlueRange Mesh – Community Edition (CE)
// ** Copyright (c) 2015-2021 MWAY DIGITAL GmbH, Germany
// ** Copyright (c) 2021-2026 BlueRange GmbH, Germany
// **
// ** This file is part of BlueRange Mesh Community Edition (formerly known as
// ** FruityMesh).
// **
// ** BlueRange Mesh Community Edition is free software: you can redistribute it
// ** and/or modify it under the terms of the GNU General Public License as
// ** published by the Free Software Foundation, either version 3 of the
// ** License, or (at your option) any later version.
// **
// ** BlueRange Mesh Community Edition is distributed in the hope that it will
// ** be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// ** See the GNU General Public License for more details.
// **
// ** You should have received a copy of the GNU General Public License along
// ** with this program. If not, see https://www.gnu.org/licenses/.
// **
// ** IMPORTANT:
// ** Any modification, extension, or derivative work of this file MUST also be
// ** licensed under the GNU General Public License v3 or later and the complete
// ** corresponding source code MUST be made available.
// **
// ** Commercial Use:
// ** If you wish to use this software without the obligations of the GPLv3
// ** (including source code disclosure), a commercial license for
// ** BlueRange Mesh OEM Edition is required.
// **
// ** License violations automatically terminate your rights under this license
// ** and may result in legal action under applicable law.
// ** For further information please use the contact form at:
// ** https://bluerange.io/en/contact
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "Utility.h"
#include "CherrySimTester.h"
#include "CherrySimUtils.h"
#include "Logger.h"

#include <string>
#include <vector>

TEST(TestSimulateMessages, TestMixedMessageTypes) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;



    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1 });
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 2 });
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);

    tester.sim->FindNodeById(1)->gs.logger.EnableTag("DEBUGMOD");
    tester.sim->FindNodeById(2)->gs.logger.EnableTag("DEBUGMOD");


    std::vector<SimulationMessage> messages;

    {
        tester.SendTerminalCommand(1, "action 2 status get_status");
        tester.SendTerminalCommand(1, "action 3 status get_status");

        messages = {
            SimulationMessage(1,"{\"nodeId\":2,\"type\":\"status\",\"module\":3", true),
            SimulationMessage(1,"this should not be found", false),
            SimulationMessage(1,"{\"nodeId\":3,\"type\":\"status\",\"module\":3", true)
        };
        tester.SimulateUntilMessagesReceived(10 * 1000,messages);
    }
    {
        tester.SendTerminalCommand(1, "action 2 status get_status");
        tester.SendTerminalCommand(1, "action 3 status get_status");

        Exceptions::ExceptionDisabler<MessageShouldNotOccurException> te;

        messages = {
            SimulationMessage(1,"\\{\"nodeId\":2,\"type\":\"status\",\"module\":3.*", true),
            SimulationMessage(1,"this should not be found", false),
            SimulationMessage(1,"\\{\"nodeId\":3,\"type\":\"status\",\"module\":3.*", false)
        };
        tester.SimulateUntilRegexMessagesReceived(10 * 1000, messages);
        ASSERT_TRUE(tester.sim->CheckExceptionWasThrown(typeid(MessageShouldNotOccurException)));
    }
    {
        tester.SendTerminalCommand(1, "action 2 status get_status");
        tester.SendTerminalCommand(1, "action 3 status get_status");

        Exceptions::ExceptionDisabler<TimeoutException> te;

        messages = {
            SimulationMessage(1,"\\{\"nodeId\":2,\"type\":\"status\",\"module\":3.*", true),
            SimulationMessage(1,"this should not be found", false),
            SimulationMessage(1,"\\{\"nodeId\":4,\"type\":\"status\",\"module\":3.*", true)
        };
        tester.SimulateUntilRegexMessagesReceived(10 * 1000, messages);
        ASSERT_TRUE(tester.sim->CheckExceptionWasThrown(typeid(TimeoutException)));
    }
}