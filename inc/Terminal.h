/**

Copyright (c) 2014-2015 "M-Way Solutions GmbH"
FruityMesh - Bluetooth Low Energy mesh protocol [http://mwaysolutions.com/]

This file is part of FruityMesh

FruityMesh is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

/*
 * The Terminal is used for UART input and output and allows easy debugging
 * and function execution, it can be disabled for nodes that do not need
 * this capability.
 */

#pragma once

#include <string>
#include <vector>

#include <Config.h>

#include <types.h>
#include <SimplePushStack.h>

using namespace std;


#define MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS 20


class TerminalCommandListener
{
private:

public:
	TerminalCommandListener();
	virtual ~TerminalCommandListener();

#ifdef ENABLE_TERMINAL
	//This method can be implemented by any subclass and will be notified when
	//a command is entered via uart.
	virtual bool TerminalCommandHandler(string commandName, vector<string> commandArgs) = 0;
#endif

};


/*
#ifndef ENABLE_TERMINAL
#define TerminalCommandHandler(...){} TerminalCommandHandler(...){  }
#endif
*/

class Terminal
{
private:


	static string commandName;
	static vector<string> commandArgs;



	static SimplePushStack* registeredCallbacks;

	//Used to Read a line into the buffer
	static void ReadlineUART(char* readBuffer, u8 readBufferLength, u8 offset);

public:
	//Can be set to true or false
	static bool terminalIsInitialized;

	//Whether to behave as user terminal or as an interface to another program
	static bool promptAndEchoMode;

	//Must be called before using the Terminal
	static void Init();

	//Called every once in a while to check for UART input
	static void PollUART();

	static void ReadFromUARTNonBlocking();

	//Register a class that will be notified when the activation string is entered
	static void AddTerminalCommandListener(TerminalCommandListener* callback);


};

