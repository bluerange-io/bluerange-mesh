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

//This test should make sure that the LightLightness state is correctly bound to the OnOff State
TEST(TestSigLightModels, TestLightLightnessOnOffBinding)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "dev_sig_mesh", 2 });
    //testerConfig.verbose = true;
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1999);

    for(int i=0; i<2; i++){
        NodeIndexSetter setter(i);
        GS->logger.EnableTag("SIG");
        GS->logger.EnableTag("SIGMODEL");
    }

    //Hint: The configured publish addresses do not exist in our setup but are
    //still logged out as json which makes it good for testing without affecting other functionality

    //First, configure a publish address for the 3rd elements OnOff Server on node 2 so that it reports its status changes to node 1
    //0x1234 SenderAddress, 0x0020 ReceiverAddress (ConfigurationServer), Opcode 0x03 CONFIG_MODEL_PUBLICATION_SET
    //Parameters: 0x0022 ElementAddress, 0x0001 PublishAddress, 0x0000 AppKeyIndex + CredentialFlag + RFU
    //            0x00 PublishTTL, 0x00 PublishPeriod, 0x00 PublishRetransmitCount + PublishRetransmitIntervalSteps
    //            0x1000 ModelIdentifier (GENERIC_ONOFF_SERVER)
    tester.SendTerminalCommand(1, "sigmesh 0x1234 0x0020 0x03 22:00:01:00:00:00:00:00:00:00:10");

    //Next, configure a publish address (0x0002) for the 3rd elements Light Lightness Server (0x1300) on node 2 as well
    tester.SendTerminalCommand(1, "sigmesh 0x1234 0x0020 0x03 22:00:02:00:00:00:00:00:00:00:13");

    //Next, configure a publish address (0x0003) for the 3rd elements Generic Level Server (0x1002) on node 2 as well
    tester.SendTerminalCommand(1, "sigmesh 0x1234 0x0020 0x03 22:00:03:00:00:00:00:00:00:02:10");

    tester.SimulateForGivenTime(10 * 1000);

    //Make sure the publication address was changed
    tester.SendTerminalCommand(2, "sigprint");

    {
        std::vector<SimulationMessage> messages = {
            SimulationMessage(2, "Model id 0x1300 (LIGHT_LIGHTNESS_SERVER), (publishAddr 0x0002)"),
            SimulationMessage(2, "> Model id 0x1002 (GENERIC_LEVEL_SERVER) (publishAddr 0x0003)"),
            SimulationMessage(2, "> Model id 0x1000 (GENERIC_ONOFF_SERVER) (publishAddr 0x0001)"),
        };
        tester.SimulateUntilMessagesReceived(10 * 1000, messages);
    }
    
    //Generate a state change in the Generic OnOff Model (part of the LightLightness Server) on the 3rd Element from within node 2 from a random sender address
    tester.SendTerminalCommand(2, "sigmesh 0x1234 0x0022 0x8202 01:00");

    //Wait until node 1 has received the change
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"sigmesh\",\"nodeId\":2,\"senderAddress\":\"0x0022\",\"receiverAddress\":\"0x0001\",\"opcode\":\"0x8204\",\"payload\":\"AQAA\"}");

    //Generate a GENERIC_LEVEL_SET message for the 3rd Element from within node 2 from a random sender address
    //Level: 0x1234, TID: 0x00
    tester.SendTerminalCommand(2, "sigmesh 0x1234 0x0022 0x8206 34:12:00");

    //See Mesh Model Spec 7.4.2.6
    {
        std::vector<SimulationMessage> messages = {
            //First, the Level Status should be reported to the requester
            SimulationMessage(2, "{\"type\":\"sigmesh\",\"nodeId\":2,\"senderAddress\":\"0x0022\",\"receiverAddress\":\"0x1234\",\"opcode\":\"0x8208\",\"payload\":\"NBIAAAA=\"}"),
            //Next, the LightLightness Status should be published to configured publish address reports the Light Lightness Actual State
            SimulationMessage(2, "{\"type\":\"sigmesh\",\"nodeId\":2,\"senderAddress\":\"0x0022\",\"receiverAddress\":\"0x0002\",\"opcode\":\"0x824E\",\"payload\":\"NJIAAAA=\"}"),
        };
        tester.SimulateUntilMessagesReceived(10 * 1000, messages);
    }
}

#endif //GITHUB_RELEASE
