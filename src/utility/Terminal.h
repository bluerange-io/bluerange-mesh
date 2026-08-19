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
    LOGIN_REQUIRED       = 6, //...is not processed because a login is required
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

    //If isLoginRequired, user must log in via node key to unlock terminal functionality
    bool isLoggedIn = false;

    void ProcessTerminalCommandHandlerReturnType(TerminalCommandHandlerReturnType handled, i32 commandArgsSize);

public:
    static Terminal& GetInstance();

    //After the terminal has been initialized (all transports), this is true
    bool terminalIsInitialized = false;

    //Will be set to true once a full line was received during an interrupt
    //Will then be reset by the event looper once the line was fully processed
    volatile bool lineToReadAvailable = false;

    //Set true if terminal commands can only be sent after successful login
    bool isLoginRequired = false;

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

    /**
     * @brief Applies the given terminal mode
     *
     * This will for example enable or disable the UART and set it to the
     * correct mode, depending on the given terminal mode.
     *
     * @param[in] mode The terminal mode to apply
     *
     * @warning Currently only affects UART; other transports are unaffected and
     *          UART cannot be disabled via this call.
     */
    void ApplyTerminalMode(TerminalMode mode);

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

    void EnableLogin();
    bool IsLocked() const;
    bool Login(const char* password);
    void Logout();

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


