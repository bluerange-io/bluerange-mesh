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

