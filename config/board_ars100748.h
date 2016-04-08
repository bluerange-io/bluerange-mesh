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
#ifndef ARS10047_H
#define ARS10047_H

// Definitions for ARS10047 v1.0


#define SET_ARS100748_BOARD()			\
do{ 									\
	Config->Led1Pin = 0;				\
	Config->Led2Pin = 1;				\
	Config->Led3Pin = 1;				\
	Config->LedActiveHigh = true;				\
	Config->uartRXPin = 11;				\
	Config->uartTXPin = 9;				\
	Config->uartCTSPin = 10;				\
	Config->uartRTSPin = 8;				\
	Config->uartFlowControl = true;				\
	Config->calibratedTX = -65;				\
										\
} while(0)


//This macro checks whether the board is a ARS100748 board and sets the default config if it is
#define DETECT_AND_SET_ARS100748_BOARD()		\
do{ 									\
	u32 pinNumber = 1;														\
	nrf_gpio_cfg_input(pinNumber, NRF_GPIO_PIN_NOPULL);						\
	bool pinConnectedToGround = (NRF_GPIO->IN & (1UL << pinNumber)) > 0;	\
	nrf_gpio_cfg_default(pinNumber);										\
	if(!pinConnectedToGround){												\
		SET_ARS100748_BOARD(); \
	}																		\
} while(0)



#endif // ARS10047_H



