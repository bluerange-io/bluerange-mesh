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

/*
 * This is the main class that initializes the SoftDevice and starts the code.
 * It contains error handlers for all unfetched errors.
 */

#pragma once



#include <types.h>
#include <LedWrapper.h>

extern "C"{
#include <ble.h>


#include <nrf_gpiote.h>
#include <nrf_drv_gpiote.h>
}

//Time when the button 1 was pressed down and how long it was held
u32 button1PressTimeDs = 0;
u32 button1HoldTimeDs = 0;

u32 pendingSysEvent;

void bleDispatchEventHandler(ble_evt_t * p_ble_evt);
void sysDispatchEventHandler(u32 sys_evt);

int app_main();

void detectBoardAndSetConfig(void);
void bleInit(void);
u32 initNodeID(void);

void initTimers(void);

void timerEventDispatch(u16 passedTime, u32 appTimer);
void dispatchUartInterrupt();

void initGpioteButtons();
void buttonInterruptHandler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action);
void dispatchButtonEvents(u8 buttonId, u32 buttonHoldTime);

//These are the event handlers that are notified by the SoftDevice
//The events are then broadcasted throughout the application
extern "C"{
	void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name);
	void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name);
	void sys_evt_dispatch(uint32_t sys_evt);
}




/** @} */
