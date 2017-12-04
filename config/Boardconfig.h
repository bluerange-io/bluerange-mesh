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

/*
 * This file contains the mesh configuration, which is a singleton. Some of the
 * values can be changed at runtime to alter the meshing behaviour.
 */

#ifndef BOARDCONFIG_H
#define BOARDCONFIG_H

#ifdef __cplusplus
	#include <types.h>
	#include <GlobalState.h>

	#ifndef Boardconfig
	#define Boardconfig (&(Boardconf::getInstance()->configuration))
	#endif
#endif //__cplusplus

#pragma pack(push)
#pragma pack(1)
#ifdef __cplusplus
struct BoardConfiguration : ModuleConfiguration {
#else
struct BoardConfigurationC {
#endif
	// ########### BOARD_SPECIFICS ################################################

	//Board Type identifies a PCB with its wiring and configuration
	uint16_t boardType;

	//Default board is pca10031, modify SET_BOARD if different board is required
	//Or flash config data to UICR
	int8_t led1Pin;
	int8_t led2Pin;
	int8_t led3Pin;
	//Defines if writing 0 or 1 to an LED turns it on
	uint8_t ledActiveHigh : 8;

	int8_t button1Pin;
	uint8_t buttonsActiveHigh : 8;

	//UART configuration. Set RX-Pin to -1 to disable UART
	int8_t uartRXPin;
	int8_t uartTXPin;
	int8_t uartCTSPin;
	int8_t uartRTSPin;
	//Default, can be overridden by boards
	uint32_t uartBaudRate : 32;

	//Receiver sensitivity of this device, set from board configs
	int8_t dBmRX;
	// This value should be calibrated at 1m distance, set by board configs
	int8_t calibratedTX;

	uint8_t lfClockSource;
};
#pragma pack(pop)


#ifdef __cplusplus
	class Boardconf
	{
		private:
			Boardconf();

		public:
			static Boardconf* getInstance(){
				if(!GS->boardconf){
					GS->boardconf = new Boardconf();
				}
				return GS->boardconf;
			}

			void Initialize(bool loadConfigFromFlash);
			void Migrate();
			void ResetToDefaultConfiguration();

		DECLARE_CONFIG_AND_PACKED_STRUCT(BoardConfiguration);
	};
#endif //__cplusplus

//Can be used to make the boardconfig available to C
#ifdef __cplusplus
extern void* fmBoardConfigPtr;
#else
extern struct BoardConfigurationC* fmBoardConfigPtr;
#endif

#endif //BOARDCONFIG_H
