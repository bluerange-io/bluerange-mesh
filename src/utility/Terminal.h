////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2022 M-Way Solutions GmbH
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

#pragma once


#include <Config.h>
#include <Boardconfig.h>

#include <FmTypes.h>
#ifdef SIM_ENABLED
#include <string>
#include <queue>

struct TerminalCommandQueueEntry
{
    std::string terminalCommand = "";
    bool skipCrcCheck = false;
};

#endif

//UART does not work with the SIM
#ifdef SIM_ENABLED
#define ACTIVATE_UART 0
#endif

#define TERMARGS(commandArgsIndex, compareTo)     (strcmp(commandArgs[commandArgsIndex], compareTo)==0)


constexpr int MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS = 20;
constexpr int MAX_TERMINAL_JSON_LISTENER_CALLBACKS = 1;
constexpr int TERMINAL_READ_BUFFER_LENGTH = 300;
constexpr int MAX_NUM_TERM_ARGS = 15;

enum class TerminalCommandHandlerReturnType : u8
{
    //The command...
    UNKNOWN              = 0, //...is unknown
    SUCCESS              = 1, //...was successfully interpreted and executed
    WRONG_ARGUMENT       = 2, //...exists but the given arguments were malformed
    NOT_ENOUGH_ARGUMENTS = 3, //...exists but the amount of arguments was too low
    WARN_DEPRECATED      = 4, //...was successfully interpreted and executed but is marked deprecated and will potentially be removed in the future.
    INTERNAL_ERROR       = 5, //An internal error occurred that potentially requires the attention of a firmware developer.
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

/*
 * The Terminal is used for UART input and output and allows easy debugging
 * and function execution, it can be disabled for nodes that do not need
 * this capability.
 */
class Terminal
{
        friend class DebugModule;

private:
    const char* commandArgsPtr[MAX_NUM_TERM_ARGS];

    u8 registeredJsonCallbacksNum = 0;
    TerminalJsonListener* registeredJsonCallbacks[MAX_TERMINAL_JSON_LISTENER_CALLBACKS] = {};
    bool currentlyExecutingJsonCallbacks = false;    //Avoids endless recursion, where outputCallbacks themselves want to print something.

    u32 readBufferOffset = 0;
    char readBuffer[TERMINAL_READ_BUFFER_LENGTH];

#ifdef SIM_ENABLED
    std::queue<TerminalCommandQueueEntry> terminalCommandQueue;
    std::string ReadStdioLine();
#endif


    //Will be false after a timeout and true after input is received
    bool uartActive = false;

    bool crcChecksEnabled = false;

    bool receivedProcessableLine = false;

    void ProcessTerminalCommandHandlerReturnType(TerminalCommandHandlerReturnType handled, i32 commandArgsSize);

public:
    static Terminal& GetInstance();

    //After the terminal has been initialized (all transports), this is true
    bool terminalIsInitialized = false;

    //Will be set to true once a full line was received during an interrupt
    //Will then be reset by the event looper once the line was fully processed
    volatile bool lineToReadAvailable = false;

    //###### General ######
    //Checks if a line is available or reads a line if input is detected
    void CheckAndProcessLine();
    void ProcessLine(char* line);
    i32 TokenizeLine(char* line, u16 lineLength);

    //Register a class that will be notified when the activation string is entered
    void AddTerminalJsonListener(TerminalJsonListener* callback);

    //###### Log Transport ######
    //Must be called before using the Terminal
    Terminal();
    void Init();
    void PutString(const char* buffer);
    void PutChar(const char character);

    void OnJsonLogged(const char* json);

    const char** GetCommandArgsPtr();
    u8 GetReadBufferOffset();
    char* GetReadBuffer();

    void EnableCrcChecks();
#ifdef SIM_ENABLED
    void DisableCrcChecks();
#endif
    bool IsCrcChecksEnabled();

#ifdef SIM_ENABLED
    //Used to improve the performance to only execute some calls in the simulator
    //if the mentioned terminal is active
    bool IsTermActive();
#endif

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

    //###### App UART ######
#if IS_ACTIVE(APP_UART)
private:
    void AppUartCheckAndProcessLine();
    void AppUartPutString(const char* message);
#endif

    //###### Stdio ######
#if IS_ACTIVE(STDIO)
public:
    static bool stdioActive;

private:
    bool TryProcessSimulatorCommand(const std::string &command);

    void LogReplayCommand(const std::string &command);

private:
    void StdioInit();
    void StdioCheckAndProcessLine();

public:
    void PutIntoTerminalCommandQueue(std::string &message, bool skipCrc);
    bool GetNextTerminalQueueEntry(TerminalCommandQueueEntry &out);
    void StdioPutString(const char* message);

#endif

    //###### Socket Term ######
    //The SocketTerm implements TCP socket based communication for the CherrySim
    //This makes it possible to connect to multiple nodes at the same time
#if IS_ACTIVE(SOCKET_TERM)
private:
    void SocketTermCheckAndProcessLine();

#endif

    //###### Virtual Com Port ######
#if IS_ACTIVE(VIRTUAL_COM_PORT)
public:
    void VirtualComCheckAndProcessLine();
    static void VirtualComPortEventHandler(bool portOpened);
#endif
};

//A helper macro to add debug logs in both c++ and c code from anywhere using Segger RTT
#if ACTIVATE_SEGGER_RTT == 1
extern "C" {
extern void SeggerRttPrintf_c(const char* message, ...);
}
#define log_rtt(...) SeggerRttPrintf_c(__VA_ARGS__)
#else
#define log_rtt(...) do {} while(0)
#endif

    //###### Other ######
#ifdef TERMINAL_ENABLED
    //Some sort of logging is used
    #define log_transport_init() Terminal::GetInstance().Init(Terminal::promptAndEchoMode);
    #define log_transport_putstring(message) Terminal::GetInstance().PutString(message)
    #define log_transport_put(character) Terminal::GetInstance().PutChar(character)
#else
    //logging is completely disabled
    #define log_transport_init() do{}while(0)
    #define log_transport_putstring(message) do{}while(0)
    #define log_transport_put(character) do{}while(0)
#endif


