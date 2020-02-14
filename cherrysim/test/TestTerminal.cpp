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
#include "Terminal.h"

TEST(TestTerminal, TestTokenizeLine) {
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.numNodes = 2;
	simConfig.terminalId = 0;
	//testerConfig.verbose = true;

	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
	tester.Start();

	for (int i = 0; i < 2; i++)	//There are some direct memory accesses so we just repeat the test once.
	{
		char line[] = "This will be tokenized! Also with ~some special chars! This !is !one !token!";
		Terminal::getInstance().TokenizeLine(line, sizeof(line));
		ASSERT_EQ(Terminal::getInstance().getCommandArgsPtr()[0], &(line[0 ])); ASSERT_STREQ(&(line[0 ]), "This");
		ASSERT_EQ(Terminal::getInstance().getCommandArgsPtr()[1], &(line[5 ])); ASSERT_STREQ(&(line[5 ]), "will");
		ASSERT_EQ(Terminal::getInstance().getCommandArgsPtr()[2], &(line[10])); ASSERT_STREQ(&(line[10]), "be");
		ASSERT_EQ(Terminal::getInstance().getCommandArgsPtr()[3], &(line[13])); ASSERT_STREQ(&(line[13]), "tokenized!");
		ASSERT_EQ(Terminal::getInstance().getCommandArgsPtr()[4], &(line[24])); ASSERT_STREQ(&(line[24]), "Also");
		ASSERT_EQ(Terminal::getInstance().getCommandArgsPtr()[5], &(line[29])); ASSERT_STREQ(&(line[29]), "with ~some");
		ASSERT_EQ(Terminal::getInstance().getCommandArgsPtr()[6], &(line[40])); ASSERT_STREQ(&(line[40]), "special");
		ASSERT_EQ(Terminal::getInstance().getCommandArgsPtr()[7], &(line[48])); ASSERT_STREQ(&(line[48]), "chars!");
		ASSERT_EQ(Terminal::getInstance().getCommandArgsPtr()[8], &(line[55])); ASSERT_STREQ(&(line[55]), "This !is !one !token!");


	}
}

TEST(TestTerminal, TestAddTerminalCommandListener) {
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.numNodes = 2;
	simConfig.terminalId = 0;
	//testerConfig.verbose = true;

	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
	tester.Start();

	struct TerminalCommandListenerImplementation : public TerminalCommandListener{
		TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override { return TerminalCommandHandlerReturnType::UNKNOWN; /*Dummy impl*/ }
	};

	TerminalCommandListenerImplementation impls[MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS];
	ASSERT_LT(Terminal::getInstance().getAmountOfRegisteredCommandListeners(), MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS); //If this fails, there were not enough slots to test the function.

	for (int i = Terminal::getInstance().getAmountOfRegisteredCommandListeners(); i < MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS; i++)
	{
		Terminal::getInstance().AddTerminalCommandListener(&(impls[i]));
		ASSERT_EQ(Terminal::getInstance().getAmountOfRegisteredCommandListeners(), i + 1);
	}


	for (int i = Terminal::getInstance().getAmountOfRegisteredCommandListeners(); i < MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS; i++)
	{
		ASSERT_EQ(Terminal::getInstance().getRegisteredCommandListeners()[i], &(impls[i]));
	}

}

TEST(TestTerminal, TestPutIntoReadBuffer) {
	CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
	SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
	simConfig.numNodes = 2;
	simConfig.terminalId = 0;
	//testerConfig.verbose = true;

	CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
	tester.Start();

	ASSERT_TRUE(Terminal::getInstance().PutIntoReadBuffer("Hello World!"));
	ASSERT_EQ(Terminal::getInstance().getReadBufferOffset(), 13);
	ASSERT_STREQ(Terminal::getInstance().getReadBuffer(), "Hello World!");

}

