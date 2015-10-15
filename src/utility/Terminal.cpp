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
#include <simple_uart.h>
}


#ifdef ENABLE_TERMINAL
bool Terminal::terminalIsInitialized = false;
bool Terminal::promptAndEchoMode = true;
SimplePushStack* Terminal::registeredCallbacks;
#endif

//Initialize the mhTerminal
void Terminal::Init()
{
#ifdef ENABLE_TERMINAL
	registeredCallbacks = new SimplePushStack(MAX_TERMINAL_COMMAND_LISTENER_CALLBACKS);

	//Start UART communication
	simple_uart_config(RTS_PIN_NUMBER, TX_PIN_NUMBER, CTS_PIN_NUMBER, RX_PIN_NUMBER, HWFC);

	char versionString[15];
	Utility::GetVersionStringFromInt(Config->firmwareVersion, versionString);

	if (promptAndEchoMode)
	{
		//Send Escape sequence
		simple_uart_put(27); //ESC
		simple_uart_putstring((const u8*) "[2J"); //Clear Screen
		simple_uart_put(27); //ESC
		simple_uart_putstring((const u8*) "[H"); //Cursor to Home

		//Send App start header
		simple_uart_putstring((const u8*) "--------------------------------------------------" EOL);
		simple_uart_putstring((const u8*) "Terminal started, compile date: ");
		simple_uart_putstring((const u8*) __DATE__);
		simple_uart_putstring((const u8*) "  ");
		simple_uart_putstring((const u8*) __TIME__);
		simple_uart_putstring((const u8*) ", version: ");
		simple_uart_putstring((const u8*) versionString);

#ifdef NRF52
		simple_uart_putstring((const u8*) ", nRF52");
#else
		simple_uart_putstring((const u8*) ", nRF51");
#endif

		simple_uart_putstring((const u8*) EOL "--------------------------------------------------" EOL);
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

	if (simple_uart_get_with_timeout(0, (u8*) readBuffer))
	{

		//Output query string and typed symbol to terminal
		if (promptAndEchoMode)
		{
			simple_uart_putstring((const u8*) EOL "mhTerm: "); //Display prompt
			simple_uart_put(readBuffer[0]); //echo back symbol
		}

		//Read line from uart
		ReadlineUART(readBuffer, 250, 1);

		//FIXME: remove after finding problem
		memcpy(testCopy, readBuffer, 250);

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
			simple_uart_put(27); //ESC
			simple_uart_putstring((const u8*) "[2J"); //Clear Screen
			simple_uart_put(27); //ESC
			simple_uart_putstring((const u8*) "[H"); //Cursor to Home
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
					simple_uart_putstring((const u8*)"Command not found" EOL);
				} else {
					uart_error(Logger::COMMAND_NOT_FOUND);
				}
				//FIXME: to find problems with uart input
				uart("ERROR", "{\"user_input\":\"%s\"}" SEP, testCopy);
			}
		}
	}
#endif
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
		byteBuffer = simple_uart_get();

		//BACKSPACE
		if (byteBuffer == 127)
		{
			if (counter > 0)
			{
				//Output Backspace
				if(promptAndEchoMode) simple_uart_put(byteBuffer);

				readBuffer[counter - 1] = 0;
				counter--;
			}
			//ALL OTHER CHARACTERS
		}
		else
		{

			//Display entered character in terminal
			if(promptAndEchoMode) simple_uart_put(byteBuffer);

			if (byteBuffer == '\r' || counter >= readBufferLength || counter >= 250)
			{
				readBuffer[counter] = '\0';
				if(promptAndEchoMode) simple_uart_putstring((const u8*) EOL);
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
