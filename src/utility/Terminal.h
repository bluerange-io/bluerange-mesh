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
#define ACTIVATE_UART 0
#endif

#define TERMARGS(commandArgsIndex, compareTo)     (strcmp(commandArgs[commandArgsIndex], compareTo)==0)


constexpr int MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS = 20;
constexpr int MAX_TERMINAL_JSON_LISTENER_CALLBACKS = 1;
constexpr int TERMINAL_READ_BUFFER_LENGTH = 250;
constexpr int MAX_NUM_TERM_ARGS = 15;

enum class TerminalCommandHandlerReturnType : u8
{
	//The command...
	UNKNOWN              = 0, //...is unknown
	SUCCESS              = 1, //...was successfully interpreted and executed
	WRONG_ARGUMENT       = 2, //...exists but the given arguments were malformed
	NOT_ENOUGH_ARGUMENTS = 3, //...exists but the amount of arguments was too low
	WARN_DEPRECATED      = 4, //...was successfully interpreted and executed but is marked deprecated and will potentially be removed in the future.
};

class TerminalCommandListener
{
public:
	TerminalCommandListener() {};
	virtual ~TerminalCommandListener() {};

#ifdef TERMINAL_ENABLED
	//This method can be implemented by any subclass and will be notified when
	//a command is entered via uart.
	virtual TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) /*nonconst*/ = 0;
#endif

};

class TerminalJsonListener
{
public:
	TerminalJsonListener() {};
	virtual ~TerminalJsonListener() {};

#ifdef TERMINAL_ENABLED
	//This method can be implemented by any subclass and will be notified when
	//a message is written to the Terminal.
	virtual void TerminalJsonHandler(const char* json) /*nonconst*/ = 0;
#endif

};


class Terminal
{
		friend class DebugModule;

private:
	const char* commandArgsPtr[MAX_NUM_TERM_ARGS];
	
	//CommandListeners
	u8 registeredCommandCallbacksNum = 0;
	TerminalCommandListener* registeredCommandCallbacks[MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS] = { 0 };

	u8 registeredJsonCallbacksNum = 0;
	TerminalJsonListener* registeredJsonCallbacks[MAX_TERMINAL_JSON_LISTENER_CALLBACKS] = { 0 };
	bool currentlyExecutingJsonCallbacks = false;	//Avoids endless recursion, where outputCallbacks themselves want to print something.

	u8 readBufferOffset = 0;
	char readBuffer[TERMINAL_READ_BUFFER_LENGTH];

	void WriteStdioLineToReadBuffer();


	//Will be false after a timeout and true after input is received
	bool uartActive = false;

	bool crcChecksEnabled = false;

public:
	static Terminal& getInstance();

	//After the terminal has been initialized (all transports), this is true
	bool terminalIsInitialized = false;

	bool lineToReadAvailable = false;

	//###### General ######
	//Checks if a line is available or reads a line if input is detected
	void CheckAndProcessLine();
	void ProcessLine(char* line);
	i32 TokenizeLine(char* line, u16 lineLength);

	//Register a class that will be notified when the activation string is entered
	void AddTerminalCommandListener(TerminalCommandListener* callback);
	void AddTerminalJsonListener(TerminalJsonListener* callback);

	//###### Log Transport ######
	//Must be called before using the Terminal
	Terminal();
	void Init();
	void PutString(const char* buffer);
	void PutChar(const char character);

	void OnJsonLogged(const char* json);

	const char** getCommandArgsPtr();
	u8 getAmountOfRegisteredCommandListeners();
	TerminalCommandListener** getRegisteredCommandListeners();
	u8 getReadBufferOffset();
	char* getReadBuffer();

	void EnableCrcChecks();
	bool IsCrcChecksEnabled();

	//##### UART ######
#if IS_ACTIVE(UART)
private:
	void UartEnable(bool promptAndEchoMode);
	void UartCheckAndProcessLine();
	//Read - blocking (non-interrupt based)
	void UartReadLineBlocking();
	//Write (always blocking)
	void UartPutStringBlockingWithTimeout(const char* message);
	void UartPutCharBlockingWithTimeout(const char character);
	//Read - Interrupt driven
public:
	void UartInterruptHandler();
private:
	void UartHandleInterruptRX(char byte);
#endif


	//###### Segger RTT ######
#if IS_ACTIVE(SEGGER_RTT)
private:
	void SeggerRttInit();
	void SeggerRttCheckAndProcessLine();
public:
	void SeggerRttPutString(const char* message);
	void SeggerRttPutChar(const char character);

#endif

	//###### Stdio ######
#if IS_ACTIVE(STDIO)
private:
	void StdioInit();
	void StdioCheckAndProcessLine();
public:
	bool PutIntoReadBuffer(const char* message);
	void StdioPutString(const char* message);

#endif

	//###### Virtual Com Port ######
#if IS_ACTIVE(VIRTUAL_COM_PORT)
public:
	void VirtualComCheckAndProcessLine();
	static void VirtualComPortEventHandler(bool portOpened);
#endif
};

//#if IS_ACTIVE(SEGGER_RTT)
//void SeggerRttPrintf_c(const char* message, ...);
//#endif

	//###### Other ######
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


