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
#include <LogTransport.h>

extern "C"
{
#include "nrf.h"
}


#ifdef ENABLE_TERMINAL
bool Terminal::terminalIsInitialized = false;
bool Terminal::promptAndEchoMode = TERMINAL_PROMPT_MODE_ON_BOOT;
SimplePushStack* Terminal::registeredCallbacks;
#endif

//Initialize the mhTerminal
void Terminal::Init()
{
#ifdef ENABLE_TERMINAL
	registeredCallbacks = new SimplePushStack(MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS);

	//Start UART communication
	log_transport_init();

	char versionString[15];
	Utility::GetVersionStringFromInt(Config->firmwareVersion, versionString);

	if (promptAndEchoMode)
	{
		//Send Escape sequence
		log_transport_put(27); //ESC
		log_transport_putstring((const u8*) "[2J"); //Clear Screen
		log_transport_put(27); //ESC
		log_transport_putstring((const u8*) "[H"); //Cursor to Home

		//Send App start header
		log_transport_putstring((const u8*) "--------------------------------------------------" EOL);
		log_transport_putstring((const u8*) "Terminal started, compile date: ");
		log_transport_putstring((const u8*) __DATE__);
		log_transport_putstring((const u8*) "  ");
		log_transport_putstring((const u8*) __TIME__);
		log_transport_putstring((const u8*) ", version: ");
		log_transport_putstring((const u8*) versionString);

#ifdef NRF52
		log_transport_putstring((const u8*) ", nRF52");
#else
		log_transport_putstring((const u8*) ", nRF51");
#endif

		if(Config->deviceConfigOrigin == Conf::deviceConfigOrigins::RANDOM_CONFIG){
			log_transport_putstring((const u8*) ", RANDOM Config");
		} else if(Config->deviceConfigOrigin == Conf::deviceConfigOrigins::UICR_CONFIG){
			log_transport_putstring((const u8*) ", UICR Config");
		} else if(Config->deviceConfigOrigin == Conf::deviceConfigOrigins::TESTDEVICE_CONFIG){
			log_transport_putstring((const u8*) ", TESTDEVICE Config");
		}


		log_transport_putstring((const u8*) EOL "--------------------------------------------------" EOL);
	} else {
		uart("NODE", "{\"type\":\"reboot\"}" SEP);
	}

	terminalIsInitialized = true;
#endif
}

//Checks weather there is a character in the UART Buffer and starts
//Terminal mode if there is
//This function must be called repetitively in order to spawn the terminal on request
string Terminal::commandName;
vector<string> Terminal::commandArgs;

void Terminal::PollUART()
{
#ifdef ENABLE_TERMINAL
	if (!terminalIsInitialized)
		return;

	static char readBuffer[250] = { 0 };
	static char testCopy[250] = {0};
	readBuffer[0] = 0;

	if (log_transport_get_char_nonblocking((u8*)readBuffer))
	{

		//Output query string and typed symbol to terminal
		if (promptAndEchoMode)
		{
			log_transport_putstring((const u8*) EOL "mhTerm: "); //Display prompt
			log_transport_put(readBuffer[0]); //echo back symbol
		}

		//Read line from uart
		ReadlineUART(readBuffer, 250, 1);

		//FIXME: remove after finding problem
		memcpy(testCopy, readBuffer, 250);
		//logt("ERROR", "inpout was: %s", testCopy);

		//Clear previous command
		commandName.clear();
		commandArgs.clear();

		//Tokenize input string into vector
		char* token = strtok(readBuffer, " ");
		if (token != NULL)
			commandName.assign(token);

		while (token != NULL)
		{
			token = strtok(NULL, " ");
			if (token != NULL)
				commandArgs.push_back(string(token));
		}

		//Check for clear screen
		if (commandName == "cls")
		{
			//Send Escape sequence
			log_transport_put(27); //ESC
			log_transport_putstring((const u8*) "[2J"); //Clear Screen
			log_transport_put(27); //ESC
			log_transport_putstring((const u8*) "[H"); //Cursor to Home
		}
		else
		{
			//Call all callbacks
			int handled = 0;

			for(u32 i=0; i<registeredCallbacks->size(); i++){
				handled += ((TerminalCommandListener*)registeredCallbacks->GetItemAt(i))->TerminalCommandHandler(commandName, commandArgs);
			}

			if (handled == 0){
				if(promptAndEchoMode){
					log_transport_putstring((const u8*)"Command not found" EOL);
				} else {
					uart_error(Logger::COMMAND_NOT_FOUND);
				}
				//FIXME: to find problems with uart input
				//uart("ERROR", "{\"user_input\":\"%s\"}" SEP, testCopy);
			} else if(!promptAndEchoMode){
				uart_error(Logger::NO_ERROR);
			}
		}
	}
#endif
}

void Terminal::ReadFromUARTNonBlocking(){

	    if(NRF_UART0->EVENTS_RXDRDY == 1){
		    NRF_UART0->EVENTS_RXDRDY = 0; //Clear ready register
		    u8 myChar = (uint8_t)NRF_UART0->RXD;

		    //send char
		    NRF_UART0->TXD = (uint8_t)myChar;
		    while (NRF_UART0->EVENTS_TXDRDY != 1){}
	    }

	    for(u32 i=0; i<15000; i++){}
}

//reads a String from the terminal (until the user has pressed ENTER)
//offset can be set to a value above 0 if the readBuffer already contains text
void Terminal::ReadlineUART(char* readBuffer, u8 readBufferLength, u8 offset)
{
#ifdef ENABLE_TERMINAL
	if (!terminalIsInitialized)
		return;

	u8 byteBuffer;
	u8 counter = offset;

	//Read in an infinite loop until \r is recognized
	while (true)
	{
		//Read from terminal
		byteBuffer = log_transport_get_char_blocking();

		//BACKSPACE
		if (byteBuffer == 127)
		{
			if (counter > 0)
			{
				//Output Backspace
				if(promptAndEchoMode) log_transport_put(byteBuffer);

				readBuffer[counter - 1] = 0;
				counter--;
			}
			//ALL OTHER CHARACTERS
		}
		else
		{

			//Display entered character in terminal
			if(promptAndEchoMode) log_transport_put(byteBuffer);

			if (byteBuffer == '\r' || counter >= readBufferLength || counter >= 250)
			{
				readBuffer[counter] = '\0';
				if(promptAndEchoMode) log_transport_putstring((const u8*) EOL);
				break;
			}
			else
			{
				memcpy(readBuffer + counter, &byteBuffer, sizeof(u8));
			}

			counter++;
		}
	}
#endif
}

//Register a string that will call the callback function with the rest of the string
void Terminal::AddTerminalCommandListener(TerminalCommandListener* callback)
{
#ifdef ENABLE_TERMINAL
	registeredCallbacks->Push((u8*)callback);
#endif
}


/*############## Terminal Listener ####################*/

TerminalCommandListener::TerminalCommandListener()
{
}

TerminalCommandListener::~TerminalCommandListener()
{
}
