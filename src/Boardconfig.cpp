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

#include <Boardconfig.h>
#include <Config.h>
#include <GlobalState.h>
#include <RecordStorage.h>

extern "C"{
}

void* fmBoardConfigPtr;

Boardconf::Boardconf()
{

}

void Boardconf::Initialize(bool loadConfigFromFlash)
{
	ResetToDefaultConfiguration();

	if (loadConfigFromFlash) {
		sizedData configData = GS->recordStorage->GetRecordData(moduleID::BOARD_CONFIG_ID);

		if (configData.length != 0) {
			memcpy((u8*)&configuration, configData.data, sizeof(BoardConfiguration));
		}

		//Migration code if no boardconfig available
		Migrate();
	}

	//Can be used from C code to access the config
	fmBoardConfigPtr = (void*)&(configuration.boardType);
}

void Boardconf::Migrate()
{
	if(configuration.moduleId != moduleID::BOARD_CONFIG_ID){
		//Check if UICR BoardId is available
		if(NRF_UICR->CUSTOMER[0] == 0xF07700 && NRF_UICR->CUSTOMER[1] != EMPTY_WORD){
			u16 uicrBoardType = NRF_UICR->CUSTOMER[1];

			configuration.moduleId = moduleID::BOARD_CONFIG_ID;
			configuration.moduleVersion = 1;
			configuration.moduleActive = 1;

			switch(uicrBoardType){
				case 0:
				case 4:
					ResetToDefaultConfiguration();
					break;

				case 7:{
					configuration.boardType = 7;
					configuration.led1Pin = -1;//15;
					configuration.led2Pin = -1;//14;
					configuration.led3Pin = -1;
					configuration.ledActiveHigh = true;
					configuration.button1Pin = -1;//7;
					configuration.buttonsActiveHigh = false;
					configuration.uartRXPin = 11;
					configuration.uartTXPin = 9;
					configuration.uartCTSPin = -1;
					configuration.uartRTSPin = -1;
					configuration.uartBaudRate = UART_BAUDRATE_BAUDRATE_Baud38400;
					configuration.dBmRX = -90;
					configuration.calibratedTX = -65;
					configuration.lfClockSource = NRF_CLOCK_LF_SRC_XTAL;

					break;
				}
				case 8:{
					configuration.led1Pin = 15;
					configuration.led2Pin = 14;
					configuration.led3Pin = -1;
					configuration.ledActiveHigh = true;
					configuration.button1Pin = 7;
					configuration.buttonsActiveHigh = false;
					configuration.uartRXPin = 11;
					configuration.uartTXPin = 9;
					configuration.uartCTSPin = 10;
					configuration.uartRTSPin = 8;
					configuration.uartBaudRate = UART_BAUDRATE_BAUDRATE_Baud38400;
					configuration.dBmRX = -90;
					configuration.calibratedTX = -60;
					configuration.lfClockSource = NRF_CLOCK_LF_SRC_XTAL;

					break;
				}
			}

		} else {
			ResetToDefaultConfiguration();
		}
	}
}

void Boardconf::ResetToDefaultConfiguration()
{
	configuration.moduleId = moduleID::BOARD_CONFIG_ID;
	configuration.moduleVersion = 1;
	configuration.moduleActive = 1;

#ifdef NRF51
	configuration.boardType = 0;
	configuration.led1Pin = 21;
	configuration.led2Pin = 22;
	configuration.led3Pin = 23;
	configuration.ledActiveHigh = false;
	configuration.button1Pin = 17;
	configuration.buttonsActiveHigh = false;
	configuration.uartRXPin = 11;
	configuration.uartTXPin = 9;
	configuration.uartCTSPin = 10;
	configuration.uartRTSPin = 8;
	configuration.uartBaudRate = NRF_UART_BAUDRATE_38400;
	configuration.dBmRX = -90;
	configuration.calibratedTX = -63;
	configuration.lfClockSource = NRF_CLOCK_LF_SRC_XTAL;
#else
	configuration.boardType = 4;
	configuration.led1Pin = 17;
	configuration.led2Pin = 18;
	configuration.led3Pin = 19;
	configuration.ledActiveHigh = false;
	configuration.button1Pin = 13;
	configuration.buttonsActiveHigh = false;
	configuration.uartRXPin = 8;
	configuration.uartTXPin = 6;
	configuration.uartCTSPin = 7;
	configuration.uartRTSPin = 5;
	configuration.uartBaudRate = NRF_UART_BAUDRATE_38400;
	configuration.dBmRX = -96;
	configuration.calibratedTX = -63;
	configuration.lfClockSource = NRF_CLOCK_LF_SRC_XTAL;
#endif
}
