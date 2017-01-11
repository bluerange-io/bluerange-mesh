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

#include <Logger.h>
#include <Terminal.h>
#include <Config.h>
#include <Utility.h>

extern "C"
{
#include "nrf.h"

#ifdef USE_UART
#include "app_util_platform.h"
#include "nrf_uart.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#endif
#ifdef USE_SEGGER_RTT
#include "SEGGER_RTT.h"
#endif
}


#ifdef TERMINAL_ENABLED

#define READ_BUFFER_LENGTH 250

bool Terminal::terminalIsInitialized = false;
SimplePushStack* Terminal::registeredCallbacks;
string Terminal::commandName;
vector<string> Terminal::commandArgs;

//UART
bool Terminal::uartActive = false;
bool Terminal::lineToReadAvailable = false;
u8 Terminal::readBufferOffset = 0;
char Terminal::readBuffer[READ_BUFFER_LENGTH];
#endif

// ######################### GENERAL
#define ________________GENERAL___________________

//Initialize the mhTerminal
void Terminal::Init()
{
#ifdef TERMINAL_ENABLED
	registeredCallbacks = new SimplePushStack(MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS);

#ifdef USE_UART
	UartEnable(Config->terminalPromptMode);
#endif
#ifdef USE_SEGGER_RTT
	SeggerRttInit();
#endif

	terminalIsInitialized = true;

	char versionString[15];
	Utility::GetVersionStringFromInt(Config->firmwareVersion, versionString);

	if (Config->terminalPromptMode)
	{
		//Send Escape sequence
		log_transport_put(27); //ESC
		log_transport_putstring("[2J"); //Clear Screen
		log_transport_put(27); //ESC
		log_transport_putstring("[H"); //Cursor to Home

		//Send App start header
		log_transport_putstring("--------------------------------------------------" EOL);
		log_transport_putstring("Terminal started, compile date: ");
		log_transport_putstring(__DATE__);
		log_transport_putstring("  ");
		log_transport_putstring(__TIME__);
		log_transport_putstring(", version: ");
		log_transport_putstring(versionString);

#ifdef NRF52
		log_transport_putstring(", nRF52");
#else
		log_transport_putstring(", nRF51");
#endif

		if(Config->deviceConfigOrigin == Conf::deviceConfigOrigins::RANDOM_CONFIG){
			log_transport_putstring(", RANDOM Config");
		} else if(Config->deviceConfigOrigin == Conf::deviceConfigOrigins::UICR_CONFIG){
			log_transport_putstring(", UICR Config");
		} else if(Config->deviceConfigOrigin == Conf::deviceConfigOrigins::TESTDEVICE_CONFIG){
			log_transport_putstring(", TESTDEVICE Config");
		}


		log_transport_putstring(EOL "--------------------------------------------------" EOL);
	} else {
		uart("NODE", "{\"type\":\"reboot\"}" SEP);
	}
#endif
}

void Terminal::PutString(const char* buffer)
{
	if(!terminalIsInitialized) return;

#ifdef USE_UART
	UartPutStringBlockingWithTimeout(buffer);
#endif
#ifdef USE_SEGGER_RTT
	Terminal::SeggerRttPutString(buffer);
#endif
}

void Terminal::PutChar(const char character)
{
	if(!terminalIsInitialized) return;

#ifdef USE_UART
	UartPutCharBlockingWithTimeout(character);
#endif
#ifdef USE_SEGGER_RTT
	SeggerRttPutChar(character);
#endif
}

// Checks all transports if a line is available (or retrieves a line)
// Then processes it
void Terminal::CheckAndProcessLine()
{
	if(!terminalIsInitialized) return;

#ifdef USE_UART
	UartCheckAndProcessLine();
#endif
#ifdef USE_SEGGER_RTT
	SeggerRttCheckAndProcessLine();
#endif
}

//Processes a line (give to all handlers and print response)
void Terminal::ProcessLine(char* line)
{
#ifdef TERMINAL_ENABLED
	//Log the input
	//logt("ERROR", "input:%s", line);

	//Clear previous command
	commandName.clear();
	commandArgs.clear();

	//Tokenize input string into vector
	char* token = strtok(line, " ");
	if (token != NULL)
		commandName.assign(token);

	while (token != NULL)
	{
		token = strtok(NULL, " ");
		if (token != NULL)
			commandArgs.push_back(string(token));
	}

	//Call all callbacks
	int handled = 0;

	for(u32 i=0; i<registeredCallbacks->size(); i++){
		handled += ((TerminalCommandListener*)registeredCallbacks->GetItemAt(i))->TerminalCommandHandler(commandName, commandArgs);
	}

	//Output result
	if (handled == 0){
		if(Config->terminalPromptMode){
			log_transport_putstring("Command not found" EOL);
		} else {
			uart_error(Logger::COMMAND_NOT_FOUND);
		}
	} else if(!Config->terminalPromptMode){
		uart_error(Logger::NO_ERROR);
	}
#endif
}

// ############################### UART
#define ________________UART___________________
#ifdef USE_UART


void Terminal::UartDisable()
{
	//Disable UART interrupt
	NVIC_DisableIRQ(UART0_IRQn);

	//Disable all UART Events
	nrf_uart_int_disable(NRF_UART0, NRF_UART_INT_MASK_RXDRDY |
									NRF_UART_INT_MASK_TXDRDY |
									NRF_UART_INT_MASK_ERROR  |
									NRF_UART_INT_MASK_RXTO);
	//Clear all pending events
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_CTS);
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_NCTS);
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXDRDY);
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_TXDRDY);
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_ERROR);
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXTO);

	//Disable UART
	NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Disabled;

	//Reset all Pinx to default state
	nrf_uart_txrx_pins_disconnect(NRF_UART0);
	nrf_uart_hwfc_pins_disconnect(NRF_UART0);

	nrf_gpio_cfg_default(Config->uartTXPin);
	nrf_gpio_cfg_default(Config->uartRXPin);

	if (NRF_UART0->PSELRTS != NRF_UART_PSEL_DISCONNECTED) nrf_gpio_cfg_default(Config->uartRTSPin);
	if (NRF_UART0->PSELCTS != NRF_UART_PSEL_DISCONNECTED) nrf_gpio_cfg_default(Config->uartCTSPin);
}

void Terminal::UartEnable(bool promptAndEchoMode)
{
	u32 err = 0;

	if(!Config->uartRXPin == -1) return;

	//Disable UART if it was active before
	UartDisable();

	//Delay to fix successive stop or startterm commands
	nrf_delay_us(10000);

	//Set variables
	Config->terminalPromptMode = promptAndEchoMode;

	readBufferOffset = 0;
	lineToReadAvailable = false;

	//Configure pins
	nrf_gpio_pin_set(Config->uartTXPin);
	nrf_gpio_cfg_output(Config->uartTXPin);
	nrf_gpio_cfg_input(Config->uartRXPin, NRF_GPIO_PIN_NOPULL);

	nrf_uart_baudrate_set(NRF_UART0, NRF_UART_BAUDRATE_38400);
	nrf_uart_configure(NRF_UART0, NRF_UART_PARITY_EXCLUDED, NRF_UART_HWFC_ENABLED);
	nrf_uart_txrx_pins_set(NRF_UART0, Config->uartTXPin, Config->uartRXPin);

	//Configure RTS/CTS
	nrf_gpio_cfg_input(Config->uartCTSPin, NRF_GPIO_PIN_NOPULL);
	nrf_gpio_pin_set(Config->uartRTSPin);
	nrf_gpio_cfg_output(Config->uartRTSPin);
	nrf_uart_hwfc_pins_set(NRF_UART0, Config->uartRTSPin, Config->uartCTSPin);

	//Enable Interrupts + timeout events
	if(!promptAndEchoMode){
		nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXTO);
		nrf_uart_int_enable(NRF_UART0, NRF_UART_INT_MASK_RXTO);

		NVIC_SetPriority(UART0_IRQn, APP_IRQ_PRIORITY_LOW);
		NVIC_ClearPendingIRQ(UART0_IRQn);
		NVIC_EnableIRQ(UART0_IRQn);
	}

	//Enable UART
	nrf_uart_enable(NRF_UART0);

	//Enable Receiver
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_ERROR);
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXDRDY);
	nrf_uart_task_trigger(NRF_UART0, NRF_UART_TASK_STARTRX);

	//Enable Transmitter
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_TXDRDY);
	nrf_uart_task_trigger(NRF_UART0, NRF_UART_TASK_STARTTX);

	//Start receiving RX events
	if(!promptAndEchoMode){
		UartEnableReadInterrupt();
	}

	uartActive = true;
}

//Checks whether a character is waiting on the input line
void Terminal::UartCheckAndProcessLine(){
	//Check if a line is available
	if(Config->terminalPromptMode && UartCheckInputAvailable()){
		UartReadLineBlocking();
	}

	//Check if a line is available either through blocking or interrupt mode
	if(!lineToReadAvailable) return;

	//Set uart active if input was received
	uartActive = true;

	//Some special stuff
	if (strcmp(readBuffer, "cls") == 0)
	{
		//Send Escape sequence
		UartPutCharBlockingWithTimeout(27); //ESC
		UartPutStringBlockingWithTimeout("[2J"); //Clear Screen
		UartPutCharBlockingWithTimeout(27); //ESC
		UartPutStringBlockingWithTimeout("[H"); //Cursor to Home
	}
	else if(strcmp(readBuffer, "startterm") == 0){
		UartEnable(true);
		return;
	}
	else if(strcmp(readBuffer, "stopterm") == 0){
		UartEnable(false);
		return;
	}
	else
	{
		ProcessLine(readBuffer);
	}

	//Reset buffer
	readBufferOffset = 0;
	lineToReadAvailable = false;

	//Re-enable Read interrupt after line was processed
	if(!Config->terminalPromptMode){
		UartEnableReadInterrupt();
	}
}

void Terminal::UartHandleError(u32 error)
{
	//Errorsource is given, but has to be cleared to be handled
	NRF_UART0->ERRORSRC = error;

	//SeggerRttPrintf("ERROR %d, ", error);

	readBufferOffset = 0;

	//FIXME: maybe we need some better error handling here
}


//############################ UART_BLOCKING_READ
#define ___________UART_BLOCKING_READ______________

bool Terminal::UartCheckInputAvailable()
{
	if(NRF_UART0->EVENTS_RXDRDY == 1) uartActive = true;
	//SeggerRttPrintf("[%d]", uartActive);
	return NRF_UART0->EVENTS_RXDRDY == 1;
}

// Reads a String from UART (until the user has pressed ENTER)
// and provides a nice terminal emulation
// ATTENTION: If no system events are fired, this function will never execute as
// a non-interrupt driven UART will not generate an event
void Terminal::UartReadLineBlocking()
{
	if (!terminalIsInitialized)
		return;

	UartPutStringBlockingWithTimeout("mhTerm: ");

	u8 byteBuffer = 0;

	//Read in an infinite loop until \r is recognized
	while (true)
	{
		//Read a byte from UART
		byteBuffer = UartReadCharBlocking();

		//BACKSPACE
		if (byteBuffer == 127)
		{
			if (readBufferOffset > 0)
			{
				//Output Backspace
				UartPutCharBlockingWithTimeout(byteBuffer);

				readBuffer[readBufferOffset - 1] = 0;
				readBufferOffset--;
			}
		}
		//ALL OTHER CHARACTERS
		else
		{
			//Display entered character in terminal
			UartPutCharBlockingWithTimeout(byteBuffer);

			if (byteBuffer == '\r' || readBufferOffset >= READ_BUFFER_LENGTH - 1)
			{
				readBuffer[readBufferOffset] = '\0';
				UartPutStringBlockingWithTimeout(EOL);
				if(readBufferOffset > 0) lineToReadAvailable = true;
				break;
			}
			else
			{
				memcpy(readBuffer + readBufferOffset, &byteBuffer, sizeof(u8));
			}

			readBufferOffset++;
		}
	}
}

char Terminal::UartReadCharBlocking()
{
	int i=0;
	while (NRF_UART0->EVENTS_RXDRDY != 1){
		if(NRF_UART0->EVENTS_ERROR){
			UartHandleError(NRF_UART0->ERRORSRC);
		}
		// Info: No timeout neede here, as we are waiting for user input
	}
	NRF_UART0->EVENTS_RXDRDY = 0;
	return NRF_UART0->RXD;
}


//############################ UART_BLOCKING_WRITE
#define ___________UART_BLOCKING_WRITE______________

void Terminal::UartPutStringBlockingWithTimeout(const char* message)
{
	//SeggerRttPrintf("TX <");
	if(!uartActive) return;

	uint_fast8_t i  = 0;
	uint8_t byte = message[i++];

	while (byte != '\0')
	{
		NRF_UART0->TXD = byte;
		byte = message[i++];

		int i=0;
		while (NRF_UART0->EVENTS_TXDRDY != 1){
			//Timeout if it was not possible to put the character
			if(i > 100000){
				uartActive = false;
				return;
			}
			i++;
			//FIXME: Do we need error handling here? Will cause lost characters
		}
		NRF_UART0->EVENTS_TXDRDY = 0;
	}

	//SeggerRttPrintf("> TX, ");
}

void Terminal::UartPutCharBlockingWithTimeout(const char character)
{
	char tmp[2] = {character, '\0'};
	UartPutStringBlockingWithTimeout(tmp);
}

//############################ UART_NON_BLOCKING_READ
#define _________UART_NON_BLOCKING_READ____________


void Terminal::UartInterruptHandler()
{
	//SeggerRttPrintf("Intrpt <");
	//Checks if an error occured
	if (nrf_uart_int_enable_check(NRF_UART0, NRF_UART_INT_MASK_ERROR) &&
		nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_ERROR))
	{
		nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_ERROR);

		UartHandleError(NRF_UART0->ERRORSRC);
	}

	//Checks if the receiver received a new byte
	if (nrf_uart_int_enable_check(NRF_UART0, NRF_UART_INT_MASK_RXDRDY) &&
			 nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_RXDRDY))
	{
		//Reads the byte
		nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXDRDY);
		char byte = NRF_UART0->RXD;

		//Disable the interrupt to stop receiving until instructed further
		nrf_uart_int_disable(NRF_UART0, NRF_UART_INT_MASK_RXDRDY | NRF_UART_INT_MASK_ERROR);

		//Tell somebody that we received something
		UartHandleInterruptRX(byte);
	}

	//Checks if a timeout occured
	if (nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_RXTO))
	{
		nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXTO);

		readBufferOffset = 0;

		//Restart transmission and clear previous buffer
		nrf_uart_task_trigger(NRF_UART0, NRF_UART_TASK_STARTRX);

		//TODO: can we check if this works???
	}

	//SeggerRttPrintf("> Intrpt, ");
}
void Terminal::UartHandleInterruptRX(char byte)
{
	//Set uart active if input was received
	uartActive = true;

	//Read the received byte
	readBuffer[readBufferOffset] = byte;
	readBufferOffset++;

	//If the line is finished, it should be processed before additional data is read
	if(byte == '\r' || readBufferOffset >= READ_BUFFER_LENGTH - 1)
	{
		readBuffer[readBufferOffset-1] = '\0';
		lineToReadAvailable = true; //Should be the last statement
		// => next, the main event loop will process the line from the main context
	}
	//Otherwise, we keep reading more bytes
	else
	{
		UartEnableReadInterrupt();
	}
}

void Terminal::UartEnableReadInterrupt()
{
	//SeggerRttPrintf("RX Inerrupt enabled, ");
	nrf_uart_int_enable(NRF_UART0, NRF_UART_INT_MASK_RXDRDY | NRF_UART_INT_MASK_ERROR);
}
#endif
//############################ SEGGER RTT
#define ________________SEGGER_RTT___________________

#ifdef USE_SEGGER_RTT
void Terminal::SeggerRttInit()
{

}

void Terminal::SeggerRttCheckAndProcessLine(){

	char byte = 0;
	if(SEGGER_RTT_HasKey()){
		while(byte != '\r' || readBufferOffset >= READ_BUFFER_LENGTH - 1){
			byte = SEGGER_RTT_GetKey();
			readBuffer[readBufferOffset] = byte;
			readBufferOffset++;
		}
		readBuffer[readBufferOffset-1] = '\0';
		lineToReadAvailable = true;

		ProcessLine(readBuffer);

		//Reset buffer
		readBufferOffset = 0;
		lineToReadAvailable = false;
	}
}

void Terminal::SeggerRttPrintf(const char* message, ...)
{
	char tmp[250];
	memset(tmp, 0, 250);

	//Variable argument list must be passed to vnsprintf
	va_list aptr;
	va_start(aptr, message);
	vsnprintf(tmp, 250, message, aptr);
	va_end(aptr);

	SeggerRttPutString(tmp);
}

void Terminal::SeggerRttPutString(const char*message)
{
	SEGGER_RTT_WriteString(0, (const char*) message);
}

void Terminal::SeggerRttPutChar(char character)
{
	u8 buffer[1];
	buffer[0] = character;
	SEGGER_RTT_Write(0, (const char*)buffer, 1);
}
#endif





/*############## Terminal Listener ####################*/
#define ________________TERMINAL_LISTENER___________________

//Register a string that will call the callback function with the rest of the string
void Terminal::AddTerminalCommandListener(TerminalCommandListener* callback)
{
#ifdef TERMINAL_ENABLED
	registeredCallbacks->Push((u8*)callback);
#endif
}


TerminalCommandListener::TerminalCommandListener()
{
}

TerminalCommandListener::~TerminalCommandListener()
{
}
