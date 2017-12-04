/**

Copyright (c) 2014-2017 "M-Way Solutions GmbH"
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
#include <Boardconfig.h>

#include <types.h>

//UART does not work with the SIM
#ifdef SIM_ENABLED
#undef USE_UART
#endif


#if defined(USE_SEGGER_RTT) || defined(USE_UART) || defined(USE_STDIO)
#define TERMINAL_ENABLED
#endif

#define MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS 20
#define READ_BUFFER_LENGTH 250

class TerminalCommandListener
{
private:

public:
	TerminalCommandListener();
	virtual ~TerminalCommandListener();

#ifdef TERMINAL_ENABLED
	//This method can be implemented by any subclass and will be notified when
	//a command is entered via uart.
	virtual bool TerminalCommandHandler(std::string commandName, std::vector<std::string> commandArgs) = 0;
#endif

};


class Terminal
{
		friend class DebugModule;

private:
	std::string commandName;
	std::vector<std::string> commandArgs;

	u8 registeredCallbacksNum;
	TerminalCommandListener* registeredCallbacks[MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS];

	u8 readBufferOffset;
	char readBuffer[READ_BUFFER_LENGTH];


	//Will be false after a timeout and true after input is received
	bool uartActive;


public:
	static Terminal* getInstance(){
		if(!GS->terminal){
			GS->terminal = new Terminal();
		}
		return GS->terminal;
	}

	//After the terminal has been initialized (all transports), this is true
	bool terminalIsInitialized;

	bool lineToReadAvailable;

	//###### General ######
	//Checks if a line is available or reads a line if input is detected
	void CheckAndProcessLine();
	void ProcessLine(char* line);

	//Register a class that will be notified when the activation string is entered
	void AddTerminalCommandListener(TerminalCommandListener* callback);

	//###### Log Transport ######
	//Must be called before using the Terminal
	Terminal();
	void Init();
	void PutString(const char* buffer);
	void PutChar(const char character);

	//##### General #####

	//##### UART ######
#ifdef USE_UART
private:
	void UartEnable(bool promptAndEchoMode);
	void UartDisable();
	void UartCheckAndProcessLine();
	void UartHandleError(u32 error);
	//Read - blocking (non-interrupt based)
	bool UartCheckInputAvailable();
	void UartReadLineBlocking();
	char UartReadCharBlocking();
	//Write (always blocking)
	void UartPutStringBlockingWithTimeout(const char* message);
	void UartPutCharBlockingWithTimeout(const char character);
	//Read - Interrupt driven
public:
	void UartInterruptHandler();
private:
	void UartHandleInterruptRX(char byte);
	void UartEnableReadInterrupt();
#endif


	//###### Segger RTT ######
#ifdef USE_SEGGER_RTT
private:
	void SeggerRttInit();
	void SeggerRttCheckAndProcessLine();
public:
	void SeggerRttPrintf(const char* message, ...);
	void SeggerRttPutString(const char* message);
	void SeggerRttPutChar(const char character);

#endif

	//###### Stdio ######
#ifdef USE_STDIO
private:
	void StdioInit();
	void StdioCheckAndProcessLine();
public:
	void StdioPutString(const char* message);
	void StdioPutChar(const char character);

#endif
};

#ifdef TERMINAL_ENABLED
	//Some sort of logging is used
	#define log_transport_init() Terminal::getInstance()->Init(Terminal::promptAndEchoMode);
	#define log_transport_putstring(message) Terminal::getInstance()->PutString(message)
	#define log_transport_put(character) Terminal::getInstance()->PutChar(character)
#else
	//logging is completely disabled
	#define log_transport_init() do{}while(0)
	#define log_transport_putstring(message) do{}while(0)
	#define log_transport_put(character) do{}while(0)
#endif


