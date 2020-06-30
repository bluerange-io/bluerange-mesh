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
#include "Utility.h"
#include "CherrySimTester.h"
#include "CherrySimUtils.h"
#include <Node.h>

//TODO: Once this feature is ready, we should enable it in the Github release
#ifndef GITHUB_RELEASE

TEST(TestSigAccessLayer, TestGenericModels)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "dev_sig_mesh", 1 });
    testerConfig.verbose = true;
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.sim->findNodeById(1)->gs.logger.enableTag("SIG");
    tester.sim->findNodeById(1)->gs.logger.enableTag("SIGMODEL");

    //Send a sig message (0x23) from nodeid 0x0001 to broadcast 0x0000: senderSigAddress 0x1234, receiverSigAddress 0x0001,
    //opcode "Generic OnOff Set" 0x8202, state 0x01, transaction identifier 0x00
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:02:82:01:00");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Got state change for element 0 from model 1000 and state 1 with new value 1");

    //Same as above but sends a generic level set message with a level of 0x7788 and transaction id of 0
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:06:82:88:77:00");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Got state change for element 0 from model 1300 and state 2 with new value 30600");

    //Same as above but sends a light lightness set message with a level of 0x7711 and transaction id of 0
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:4C:82:11:77:00");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Got state change for element 0 from model 1300 and state 5 with new value 33356");

    //Sends a generic level set message to the second element with level 0x7788 and transaction id of 0
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:02:00:06:82:88:77:00");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "received generic level set: level 30600, TID 0");

    //Asks the primary element for the composition data
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:08:80:00");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Sending SIG message: 23:01:00:00:00  01:00:34:12  02:00:11:11:22:22:33:33:00:00:00:00:00:00:04:00:00:00:00:10:02:10:00:13:00:00:02:01:00:10:02:10:78:56:34:12");

    //Set publication address (opcode 0x03) of first element to publish address 0x7777 for model 0x1002
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:03:01:00:77:77:00:00:00:00:00:02:10");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Setting publication address of elementIndex 0, modelid 0x1002 to 0x7777");

    //Set publication address (opcode 0x03) of second element to publish address 0x1111 for model 0x12345678
    //Must still be sent to element address 1 as this is the configuration server
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:03:02:00:11:11:00:00:00:00:00:78:56:34:12");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Setting publication address of elementIndex 1, modelid 0x12345678 to 0x1111");

    //Request the publish status (opcode 0x8018) for element 1, model 0x1002
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:18:80:01:00:02:10");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Sending SIG message: 23:01:00:00:00  01:00:34:12  00:01:00:77:77:00:00:00:00:00:02:10");

    //Add a subscription to address 0xC001 for element 1, model 0x1000
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:1B:80:01:00:01:C0:00:10");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Adding subscription for elementIndex 0, modelid 0x1000 with address 0xC001");

    //Add a subscription to address 0xC002 for element 1, model 0x1002 => will be added to same model as above as the lists are shared
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:1B:80:01:00:02:C0:02:10");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Adding subscription for elementIndex 0, modelid 0x1300 with address 0xC002");
}

TEST(TestSigAccessLayer, TestConfigCompositionData)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "dev_sig_mesh", 1 });
    //testerConfig.verbose = true;
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.sim->findNodeById(1)->gs.logger.enableTag("SIG");
    tester.sim->findNodeById(1)->gs.logger.enableTag("SIGMODEL");

    //Asks the primary element for the composition data
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:08:80:00");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Sending SIG message: 23:01:00:00:00  01:00:34:12  02:00:11:11:22:22:33:33:00:00:00:00:00:00:04:00:00:00:00:10:02:10:00:13:00:00:02:01:00:10:02:10:78:56:34:12");
}

TEST(TestSigAccessLayer, TestConfigModelPublication)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "dev_sig_mesh", 1 });
    testerConfig.verbose = true;
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.sim->findNodeById(1)->gs.logger.enableTag("SIG");
    tester.sim->findNodeById(1)->gs.logger.enableTag("SIGMODEL");

    //Set publication address (opcode 0x03) of first element to publish address 0x7777 for model 0x1002
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:03:01:00:77:77:00:00:00:00:00:02:10");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Setting publication address of elementIndex 0, modelid 0x1002 to 0x7777");

    //Set publication address (opcode 0x03) of second element to publish address 0x1111 for model 0x12345678
    //Must still be sent to element address 1 as this is the configuration server
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:03:02:00:11:11:00:00:00:00:00:78:56:34:12");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Setting publication address of elementIndex 1, modelid 0x12345678 to 0x1111");

    //Request the publish status (opcode 0x8018) for element 1, model 0x1002
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:18:80:01:00:02:10");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Sending SIG message: 23:01:00:00:00  01:00:34:12  00:01:00:77:77:00:00:00:00:00:02:10");
}

TEST(TestSigAccessLayer, TestConfigModelSubscription)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "dev_sig_mesh", 1 });
    testerConfig.verbose = true;
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.sim->findNodeById(1)->gs.logger.enableTag("SIG");
    tester.sim->findNodeById(1)->gs.logger.enableTag("SIGMODEL");

    //Add a subscription to address 0xC001 for element 1, model 0x1000
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:1B:80:01:00:01:C0:00:10");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Adding subscription for elementIndex 0, modelid 0x1000 with address 0xC001");
    //The publish address should now be registered
    tester.SendTerminalCommand(1, "sigprint");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Model id 0x1000, (publishAddr 0x0000) (subscriptions 0xC001");

    //Add a subscription to address 0xC002 for element 1, model 0x1002 => will be added to same model as above as the lists are shared
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:1B:80:01:00:02:C0:02:10");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Adding subscription for elementIndex 0, modelid 0x1300 with address 0xC002");
    //The subscription should be registered for the root model
    tester.SendTerminalCommand(1, "sigprint");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Model id 0x1300, (publishAddr 0x0000) (subscriptions 0xC002");

    //Add same subscription again, should not be added twice
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:01:00:1B:80:01:00:02:C0:02:10");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Adding subscription for elementIndex 0, modelid 0x1300 with address 0xC002");
    tester.SendTerminalCommand(1, "sigprint");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Model id 0x1300, (publishAddr 0x0000) (subscriptions 0xC002, )");
}

#endif //GITHUB_RELEASE
