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
#ifndef PCA10040_H
#define PCA10040_H

// Definitions for PCA10040 (nrf52 development kit)


#define SET_PCA10040_BOARD()			\
do{ 									\
	Config->Led1Pin = 17;				\
	Config->Led2Pin = 18;				\
	Config->Led3Pin = 19;				\
	Config->LedActiveHigh = false;				\
	Config->uartRXPin = 8;				\
	Config->uartTXPin = 6;				\
	Config->uartCTSPin = 7;				\
	Config->uartRTSPin = 5;				\
	Config->uartFlowControl = true;				\
	Config->calibratedTX = -60;				\
										\
} while(0)

//This macro checks whether the boardId is for PCA10040 board
#define SET_PCA10040_BOARD_IF_FIT(boardid)		\
do{												\
	if(boardid == 0x004){							\
		SET_PCA10040_BOARD(); 						\
	}												\
} while(0)

#endif // PCA10040_H
