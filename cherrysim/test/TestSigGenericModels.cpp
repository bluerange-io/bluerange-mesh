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
#include "Utility.h"
#include "CherrySimTester.h"
#include "CherrySimUtils.h"
#include <Node.h>

//TODO: Once this feature is ready, we should enable it in the Github release
#ifndef GITHUB_RELEASE

TEST(TestSigGenericModels, TestGenericModels)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "dev_sig_mesh", 1 });
    //testerConfig.verbose = true;
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.sim->FindNodeById(1)->gs.logger.EnableTag("SIG");
    tester.sim->FindNodeById(1)->gs.logger.EnableTag("SIGMODEL");

    //Send a sig message (0x23) from nodeid 0x0001 to broadcast 0x0000: senderSigAddress 0x1234, receiverSigAddress 0x0010,
    //opcode "Generic OnOff Set" 0x8202, state 0x01, transaction identifier 0x00
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:10:00:02:82:01:00");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Got state change for element 0 from model 1000 and state 1 with new value 1");

    //Same as above but sends a generic level set message to the third element with a level of 0x7788 and transaction id of 0
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:12:00:06:82:88:77:00");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Got state change for element 2 from model 1300 and state 2 with new value 30600");

    //Same as above but sends a light lightness set message with a level of 0x7711 and transaction id of 0
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:12:00:4C:82:11:77:00");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Got state change for element 2 from model 1300 and state 5 with new value 30481");

    //Sends a generic level set message to the second element with level 0x7788 and transaction id of 0
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:11:00:06:82:88:77:00");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "received generic level set: level 30600, TID 0");

    //Set publication address (opcode 0x03) of first element to publish address 0x7777 for model 0x1002
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:10:00:03:10:00:77:77:00:00:00:00:00:02:10");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Setting publication address of elementIndex 0, modelid 0x1002 to 0x7777");

    //Set publication address (opcode 0x03) of second element to publish address 0x1111 for model 0x12345678
    //Must still be sent to element address 1 as this is the configuration server
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:10:00:03:11:00:11:11:00:00:00:00:00:78:56:34:12");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Setting publication address of elementIndex 1, modelid 0x12345678 to 0x1111");

    //Request the publish status (opcode 0x8018) for element 1, model 0x1002
    tester.SendTerminalCommand(1, "rawsend 23:01:00:00:00:34:12:10:00:18:80:10:00:02:10");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Sending SIG message: 23:01:00:00:00  10:00:34:12  00:10:00:77:77:00:00:00:00:00:02:10");
}

TEST(TestSigGenericModels, TestGenericOnOffPublication)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "dev_sig_mesh", 2 });
    //testerConfig.verbose = true;
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1999);

    //First, configure a publish address for the first elements OnOff Server on node 2 so that it reports its status changes to node 1
    //Opcode 0x03 CONFIG_MODEL_PUBLICATION_SET
    //Parameters: 0x0020 ElementAddress, 0x0010 PublishAddress, 0x0000 AppKeyIndex + CredentialFlag + RFU
    //            0x00 PublishTTL, 0x00 PublishPeriod, 0x00 PublishRetransmitCount + PublishRetransmitIntervalSteps
    //            0x1000 ModelIdentifier (GENERIC_ONOFF_SERVER)
    tester.SendTerminalCommand(1, "sigmesh 0x0010 0x0020 0x03 20:00:10:00:00:00:00:00:00:00:10");

    tester.SimulateForGivenTime(10 * 1000);

    //Make sure the publication address was changed
    tester.SendTerminalCommand(2, "sigprint");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "Model id 0x1000 (GENERIC_ONOFF_SERVER), (publishAddr 0x0010)");
    
    //Generate a state change from within node 2 from a random sender address
    tester.SendTerminalCommand(2, "sigmesh 0x1234 0x0020 0x8202 01:00");

    //Wait until node 1 has received the change
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sigmesh\",\"nodeId\":2,\"senderAddress\":\"0x0020\",\"receiverAddress\":\"0x0010\",\"opcode\":\"0x8204\",\"payload\":\"AQAA\"}");

}

TEST(TestSigGenericModels, TestGenericOnOffStatus)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 1;
    simConfig.nodeConfigName.insert({ "dev_sig_mesh", 2 });
    //testerConfig.verbose = true;
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);

    tester.SendTerminalCommand(1, "sigprint");

    //Send a SET command (ON) to the generic on/off server of the first element on node 2
    tester.SendTerminalCommand(1, "sigmesh 0x0010 0x0020 0x8202 01:00");

    //Wait until status message is received with correct state
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sigmesh\",\"nodeId\":2,\"senderAddress\":\"0x0020\",\"receiverAddress\":\"0x0010\",\"opcode\":\"0x8204\",\"payload\":\"AQAA\"}");



    //Send a GET command to the generic on/off server of the first element on node 2
    tester.SendTerminalCommand(1, "sigmesh 0x0010 0x0020 0x8201");

    //Status should still be reported as ON
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sigmesh\",\"nodeId\":2,\"senderAddress\":\"0x0020\",\"receiverAddress\":\"0x0010\",\"opcode\":\"0x8204\",\"payload\":\"AQAA\"}");
}

#endif //GITHUB_RELEASE
