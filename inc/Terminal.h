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

#if defined(USE_SEGGER_RTT) || defined(USE_UART)
#define TERMINAL_ENABLED
#endif

#define MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS 20

class TerminalCommandListener
{
private:

public:
	TerminalCommandListener();
	virtual ~TerminalCommandListener();

#ifdef TERMINAL_ENABLED
	//This method can be implemented by any subclass and will be notified when
	//a command is entered via uart.
	virtual bool TerminalCommandHandler(string commandName, vector<string> commandArgs) = 0;
#endif

};


class Terminal
{
private:

	static string commandName;
	static vector<string> commandArgs;

	static SimplePushStack* registeredCallbacks;

	static u8 readBufferOffset;
	static char readBuffer[];

	//After the terminal has been initialized (all transports), this is true
	static bool terminalIsInitialized;

	//Will be false after a timeout and true after input is received
	static bool uartActive;


public:
	static bool lineToReadAvailable;

	//###### General ######
	//Checks if a line is available or reads a line if input is detected
	static void CheckAndProcessLine();
	static void ProcessLine(char* line);

	//Register a class that will be notified when the activation string is entered
	static void AddTerminalCommandListener(TerminalCommandListener* callback);

	//###### Log Transport ######
	//Must be called before using the Terminal
	static void Init();
	static void PutString(const char* buffer);
	static void PutChar(const char character);

private:
	//##### General #####

	//##### UART ######
#ifdef USE_UART
	static void UartEnable(bool promptAndEchoMode);
	static void UartDisable();
	static void UartCheckAndProcessLine();
	static void UartHandleError(u32 error);
	//Read - blocking (non-interrupt based)
	static bool UartCheckInputAvailable();
	static void UartReadLineBlocking();
	static char UartReadCharBlocking();
	//Write (always blocking)
	static void UartPutStringBlockingWithTimeout(const char* message);
	static void UartPutCharBlockingWithTimeout(const char character);
	//Read - Interrupt driven
public: static void UartInterruptHandler(); private:
	static void UartHandleInterruptRX(char byte);
	static void UartEnableReadInterrupt();
#endif


	//###### Segger RTT ######
#ifdef USE_SEGGER_RTT
	static void SeggerRttInit();
	static void SeggerRttCheckAndProcessLine();
public: static void SeggerRttPrintf(const char* message, ...);
public: static void SeggerRttPutString(const char* message);
public: static void SeggerRttPutChar(const char character);

#endif
};

#ifdef TERMINAL_ENABLED
	//Some sort of logging is used
	#define log_transport_init() Terminal::Init(Terminal::promptAndEchoMode);
	#define log_transport_putstring(message) Terminal::PutString(message)
	#define log_transport_put(character) Terminal::PutChar(character)
#else
	//logging is completely disabled
	#define log_transport_init() do{}while(0)
	#define log_transport_putstring(message) do{}while(0)
	#define log_transport_put(character) do{}while(0)
#endif


