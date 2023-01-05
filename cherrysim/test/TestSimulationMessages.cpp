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