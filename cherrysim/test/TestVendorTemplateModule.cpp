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
#include "IoModule.h"
#include "VendorTemplateModule.h"

//This tests that the VendorTemplateModule works as intended
TEST(TestVendorTemplateModule, TestCommands)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "github_dev_nrf52", 1 });
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SendTerminalCommand(1, "action 0 template one 123");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Got command one message with 123");

    tester.SendTerminalCommand(1, "action 0 template two");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Got command two message");

}

TEST(TestVendorTemplateModule, TestRegisters) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert({ "github_dev_nrf52", 1 });
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);

    // First read a component that we haven't written to previously. Should be 0.
    //       0          1        2         3          4            5           6          7
    // component_act receiver moduleId actionType component registerAddress Payload RequestHandle
    tester.SendTerminalCommand(1, "component_act this 0xABCD01F0 read 0 20000 01 13");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":1,\"type\":\"component_sense\",\"module\":\"0xABCD01F0\",\"requestHandle\":13,\"actionType\":2,\"component\":\"0x0000\",\"register\":\"0x4E20\",\"payload\":\"AA==\"");

    // Set that register to some value (0x99 in this case)
    tester.SendTerminalCommand(1, "component_act this 0xABCD01F0 writeack 0 20000 99 13");
    // Wait for confirmation (Because it's a WRITE_ACK)
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":1,\"type\":\"component_sense\",\"module\":\"0xABCD01F0\",\"requestHandle\":13,\"actionType\":4,\"component\":\"0x0000\",\"register\":\"0x4E20\",\"payload\":\"AAA=\"");
    // And read it back
    tester.SendTerminalCommand(1, "component_act this 0xABCD01F0 read 0 20000 01 13");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":1,\"type\":\"component_sense\",\"module\":\"0xABCD01F0\",\"requestHandle\":13,\"actionType\":2,\"component\":\"0x0000\",\"register\":\"0x4E20\",\"payload\":\"mQ==\"");

    // Test the same as above, but with WRITE instead of WRITE_ACK and setting to 0x22 instead
    tester.SendTerminalCommand(1, "component_act this 0xABCD01F0 1 0 20000 22 13");
    tester.SimulateGivenNumberOfSteps(3);
    tester.SendTerminalCommand(1, "component_act this 0xABCD01F0 read 0 20000 01 13");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":1,\"type\":\"component_sense\",\"module\":\"0xABCD01F0\",\"requestHandle\":13,\"actionType\":2,\"component\":\"0x0000\",\"register\":\"0x4E20\",\"payload\":\"Ig==\"");

    // Test REGISTER_SOME_STRING_BASE
    tester.SendTerminalCommand(1, "component_act this 0xABCD01F0 1 0 20100 40:48:41:4C:4C:4F:00 13"); // String: @HALLO
    tester.SimulateGivenNumberOfSteps(3);
    tester.SendTerminalCommand(1, "component_act this 0xABCD01F0 read 0 20100 07 13");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":1,\"type\":\"component_sense\",\"module\":\"0xABCD01F0\",\"requestHandle\":13,\"actionType\":2,\"component\":\"0x0000\",\"register\":\"0x4E84\",\"payload\":\"QEhBTExPAA==\"");

    // Test REGISTER_CLAMPED_VALUE
    // The value is clamped in the range [100; 424242]
    // First set the value to something lower than the range...
    tester.SendTerminalCommand(1, "component_act this 0xABCD01F0 writeack 0 20200 00:00:00:00 13");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":1,\"type\":\"component_sense\",\"module\":\"0xABCD01F0\",\"requestHandle\":13,\"actionType\":4,\"component\":\"0x0000\",\"register\":\"0x4EE8\",\"payload\":\"AQA=\"");
    tester.SendTerminalCommand(1, "component_act this 0xABCD01F0 read 0 20200 04 13");
    // ...and observe that it was clamped to the lower bound (100).
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":1,\"type\":\"component_sense\",\"module\":\"0xABCD01F0\",\"requestHandle\":13,\"actionType\":2,\"component\":\"0x0000\",\"register\":\"0x4EE8\",\"payload\":\"ZAAAAA==\"");
    // Then set it to something within the range...
    tester.SendTerminalCommand(1, "component_act this 0xABCD01F0 writeack 0 20200 12:34:00:00 13"); // Is value 13330 in dec
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":1,\"type\":\"component_sense\",\"module\":\"0xABCD01F0\",\"requestHandle\":13,\"actionType\":4,\"component\":\"0x0000\",\"register\":\"0x4EE8\",\"payload\":\"AAA=\"");
    tester.SendTerminalCommand(1, "component_act this 0xABCD01F0 read 0 20200 04 13");
    // ...and observe that it remained unchanged.
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":1,\"type\":\"component_sense\",\"module\":\"0xABCD01F0\",\"requestHandle\":13,\"actionType\":2,\"component\":\"0x0000\",\"register\":\"0x4EE8\",\"payload\":\"EjQAAA==\"");
    // Finally set it to something too high...
    tester.SendTerminalCommand(1, "component_act this 0xABCD01F0 writeack 0 20200 12:34:FF:FF 13");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":1,\"type\":\"component_sense\",\"module\":\"0xABCD01F0\",\"requestHandle\":13,\"actionType\":4,\"component\":\"0x0000\",\"register\":\"0x4EE8\",\"payload\":\"AQA=\"");
    tester.SendTerminalCommand(1, "component_act this 0xABCD01F0 read 0 20200 04 13");
    // ...and observe that it was clamped down.
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":1,\"type\":\"component_sense\",\"module\":\"0xABCD01F0\",\"requestHandle\":13,\"actionType\":2,\"component\":\"0x0000\",\"register\":\"0x4EE8\",\"payload\":\"MnkGAA==\"");

    //Test that reading a register will call the read handler
    tester.SendTerminalCommand(1, "component_act this 0xABCD01F0 read 0 30000 02 13");
    std::vector<SimulationMessage> messages = {
        SimulationMessage(1, "Demo Register was read"),
        SimulationMessage(1, "{\"nodeId\":1,\"type\":\"component_sense\",\"module\":\"0xABCD01F0\",\"requestHandle\":13,\"actionType\":2,\"component\":\"0x0000\",\"register\":\"0x7530\",\"payload\":\"ewA=\"")
    };
    tester.SimulateUntilMessagesReceived(100 * 1000, messages);
}