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
#ifndef PCA10031_H
#define PCA10031_H

// LEDs definitions for PCA10031 (nrf51 Dongle)

#define SET_PCA10031_BOARD()			\
do{ 									\
	Config->Led1Pin = 21;				\
	Config->Led2Pin = 22;				\
	Config->Led3Pin = 23;				\
	Config->LedActiveHigh = false;				\
	Config->Button1Pin = 17;			\
	Config->ButtonsActiveHigh = false;			\
	Config->uartRXPin = 11;				\
	Config->uartTXPin = 9;				\
	Config->uartCTSPin = 10;				\
	Config->uartRTSPin = 8;				\
	Config->calibratedTX = -63;				\
										\
} while(0)

//This macro checks whether the boardId is for PCA10031 board
#define SET_PCA10031_BOARD_IF_FIT(boardid)		\
do{												\
	if(boardid == 0x000){							\
		SET_PCA10031_BOARD(); 						\
	}												\
} while(0)

#endif
