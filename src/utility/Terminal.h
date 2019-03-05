////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2019 M-Way Solutions GmbH
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

/*
 * The Terminal is used for UART input and output and allows easy debugging
 * and function execution, it can be disabled for nodes that do not need
 * this capability.
 */

#pragma once


#include <Config.h>
#include <Boardconfig.h>

#include <types.h>

//UART does not work with the SIM
#ifdef SIM_ENABLED
#undef USE_UART
#endif

#define TERMARGS(commandArgsIndex, compareTo)     (strcmp(commandArgs[commandArgsIndex], compareTo)==0)


#define MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS 20
#define READ_BUFFER_LENGTH 250
#define MAX_NUM_TERM_ARGS      12

class TerminalCommandListener
{
public:
	TerminalCommandListener() {};
	virtual ~TerminalCommandListener() {};

#ifdef TERMINAL_ENABLED
	//This method can be implemented by any subclass and will be notified when
	//a command is entered via uart.
	virtual bool TerminalCommandHandler(char* commandArgs[], u8 commandArgsSize) /*nonconst*/ = 0;
#endif

};


class Terminal
{
		friend class DebugModule;

private:
	char* commandArgsPtr[MAX_NUM_TERM_ARGS];
	u8 commandArgsSize;
	
	//CommandListeners
	u8 registeredCallbacksNum;
	TerminalCommandListener* registeredCallbacks[MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS];

	u8 readBufferOffset;
	char readBuffer[READ_BUFFER_LENGTH];


	//Will be false after a timeout and true after input is received
	bool uartActive;


public:
	static Terminal& getInstance(){
		if(!GS->terminal){
			GS->terminal = new Terminal();
		}
		return *(GS->terminal);
	}
	~Terminal() {
		GS->terminal = nullptr;
	}

	//After the terminal has been initialized (all transports), this is true
	bool terminalIsInitialized;

	bool lineToReadAvailable;

	//###### General ######
	//Checks if a line is available or reads a line if input is detected
	void CheckAndProcessLine();
	void ProcessLine(char* line);
	bool TokenizeLine(char* line, u16 lineLength);

	//Register a class that will be notified when the activation string is entered
	void AddTerminalCommandListener(TerminalCommandListener* callback);

	//###### Log Transport ######
	//Must be called before using the Terminal
	Terminal();
	void Init();
	void PutString(const char* buffer);
	void PutChar(const char character);

	char** getCommandArgsPtr();
	u8 getAmountOfRegisteredCommandListeners();
	TerminalCommandListener** getRegisteredCommandListeners();
	u8 getReadBufferOffset();
	char* getReadBuffer();


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
	bool PutIntoReadBuffer(const char* message);
	void StdioPutString(const char* message);
	void StdioPutChar(const char character);

#endif
};

#ifdef TERMINAL_ENABLED
	//Some sort of logging is used
	#define log_transport_init() Terminal::getInstance().Init(Terminal::promptAndEchoMode);
	#define log_transport_putstring(message) Terminal::getInstance().PutString(message)
	#define log_transport_put(character) Terminal::getInstance().PutChar(character)
#else
	//logging is completely disabled
	#define log_transport_init() do{}while(0)
	#define log_transport_putstring(message) do{}while(0)
	#define log_transport_put(character) do{}while(0)
#endif


