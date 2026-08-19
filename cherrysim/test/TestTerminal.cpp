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
#include "Terminal.h"

TEST(TestTerminal, TestTokenizeLine) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;

    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    for (int i = 0; i < 2; i++)    //There are some direct memory accesses so we just repeat the test once.
    {
        NodeIndexSetter setter(0);
        char line[] = "This will be tokenized! Also with ~some special chars! This !is !one !token!";
        Terminal::GetInstance().TokenizeLine(line, sizeof(line));
        ASSERT_EQ(Terminal::GetInstance().GetCommandArgsPtr()[0], &(line[0 ])); ASSERT_STREQ(&(line[0 ]), "This");
        ASSERT_EQ(Terminal::GetInstance().GetCommandArgsPtr()[1], &(line[5 ])); ASSERT_STREQ(&(line[5 ]), "will");
        ASSERT_EQ(Terminal::GetInstance().GetCommandArgsPtr()[2], &(line[10])); ASSERT_STREQ(&(line[10]), "be");
        ASSERT_EQ(Terminal::GetInstance().GetCommandArgsPtr()[3], &(line[13])); ASSERT_STREQ(&(line[13]), "tokenized!");
        ASSERT_EQ(Terminal::GetInstance().GetCommandArgsPtr()[4], &(line[24])); ASSERT_STREQ(&(line[24]), "Also");
        ASSERT_EQ(Terminal::GetInstance().GetCommandArgsPtr()[5], &(line[29])); ASSERT_STREQ(&(line[29]), "with ~some");
        ASSERT_EQ(Terminal::GetInstance().GetCommandArgsPtr()[6], &(line[40])); ASSERT_STREQ(&(line[40]), "special");
        ASSERT_EQ(Terminal::GetInstance().GetCommandArgsPtr()[7], &(line[48])); ASSERT_STREQ(&(line[48]), "chars!");
        ASSERT_EQ(Terminal::GetInstance().GetCommandArgsPtr()[8], &(line[55])); ASSERT_STREQ(&(line[55]), "This !is !one !token!");


    }
}

TEST(TestTerminal, TestLogin) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;

    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    NodeIndexSetter setter(0);
    Terminal::GetInstance().EnableLogin();
    //make sure to use json mode because github release has a featureset redirect that does not use json
    tester.sim->FindNodeById(1)->gs.config.terminalMode = TerminalMode::JSON;

    tester.SimulateUntilClusteringDone(60 * 1000);

    {
        Exceptions::ExceptionDisabler<NotLoggedInException> brExceptionDisabler;
        tester.SendTerminalCommand(1, "status");
        tester.SimulateUntilMessageReceived(10 * 1000, 1, "\"type\":\"error\",\"code\":9"); // LOGIN_REQUIRED
    }

    {
        Exceptions::ExceptionDisabler<WrongCommandParameterException> brExceptionDisabler;
        tester.SendTerminalCommand(1, "login abc");
        tester.SimulateUntilMessageReceived(10 * 1000, 1, "\"type\":\"error\",\"code\":2"); // ARGUMENTS_WRONG
    }

    tester.SendTerminalCommand(1, "login 01:00:00:00:01:00:00:00:01:00:00:00:01:00:00:00");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "\"type\":\"error\",\"code\":0"); // SUCCESS

    // Should work now
    tester.SendTerminalCommand(1, "status");
    tester.SimulateUntilRegexMessageReceived(10 * 1000, 1, "Node BBBBB \\(nodeId: 1\\) vers: \\d+, NodeKey: 01:00:....:00:00");

    tester.SendTerminalCommand(1, "logout");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "\"type\":\"error\",\"code\":0"); // SUCCESS


    // Should not work anymore
    {
        Exceptions::ExceptionDisabler<NotLoggedInException> brExceptionDisabler;
        tester.SendTerminalCommand(1, "status");
        tester.SimulateUntilMessageReceived(10 * 1000, 1, "\"type\":\"error\",\"code\":9"); // LOGIN_REQUIRED
    }
}

#ifndef GITHUB_RELEASE
#ifdef PROD_SWITCH_NRF52832

TEST(TestTerminal, TestBoard72HasTerminalWhenNotEnrolled) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.defaultNetworkId = 0; // Not enrolled
    Exceptions::ExceptionDisabler<LicenseNotValidException> lnve;

    simConfig.nodeConfigName.insert({ "prod_switch_nrf52832", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.sim->nodes[0].uicr.CUSTOMER[1] = 72;
    tester.Start();

    // UART must be active before enrollment so the device can be commissioned
    ASSERT_EQ(tester.sim->nodes[0].gs.config.terminalMode, TerminalMode::JSON);
}

TEST(TestTerminal, TestBoard72DisablesTerminalWhenEnrolled) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    // defaultNetworkId = 10 (default) → node starts enrolled
    Exceptions::ExceptionDisabler<LicenseNotValidException> lnve;

    simConfig.nodeConfigName.insert({ "prod_switch_nrf52832", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.sim->nodes[0].uicr.CUSTOMER[1] = 72;
    tester.Start();

    // UART must be disabled once enrolled (BR-16968)
    ASSERT_EQ(tester.sim->nodes[0].gs.config.terminalMode, TerminalMode::DISABLED);
}

#endif //PROD_SWITCH_NRF52832

#ifdef PROD_BLIND_NRF52832
TEST(TestTerminal, TestBoard70HasTerminalWhenNotEnrolled) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.defaultNetworkId = 0; // Not enrolled
    Exceptions::ExceptionDisabler<LicenseNotValidException> lnve;

    simConfig.nodeConfigName.insert({ "prod_blind_nrf52832", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.sim->nodes[0].uicr.CUSTOMER[1] = 70;
    tester.Start();

    // UART must be active before enrollment so the device can be commissioned
    ASSERT_EQ(tester.sim->nodes[0].gs.config.terminalMode, TerminalMode::JSON);
}

TEST(TestTerminal, TestBoard70DisablesTerminalWhenEnrolled) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    // defaultNetworkId = 10 (default) → node starts enrolled
    Exceptions::ExceptionDisabler<LicenseNotValidException> lnve;

    simConfig.nodeConfigName.insert({ "prod_blind_nrf52832", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.sim->nodes[0].uicr.CUSTOMER[1] = 70;
    tester.Start();

    // UART must be disabled once enrolled (BR-16968)
    ASSERT_EQ(tester.sim->nodes[0].gs.config.terminalMode, TerminalMode::DISABLED);
}
#endif //PROD_BLIND_NRF52832
#endif //GITHUB_RELEASE

