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

#include <Config.h>

extern "C"
{
#ifdef USE_SEGGER_RTT_INSTEAD_OF_UART
#include <SEGGER_RTT.h>
#else
#include <simple_uart.h>
#endif
}

#pragma once

#ifdef USE_SEGGER_RTT_INSTEAD_OF_UART

extern "C"{
	#define log_transport_init() do{}while(0)

	void log_transport_putstring(const u8* message);
	void log_transport_put(u8 character);

	bool log_transport_get_char_nonblocking(u8* buffer);
	u8 log_transport_get_char_blocking();
}

#else

#define log_transport_init() simple_uart_config(Config->uartRTSPin, Config->uartTXPin, Config->uartCTSPin, Config->uartRXPin, Config->uartFlowControl)

#define log_transport_putstring(message) simple_uart_putstring((const uint8_t*) message)
#define log_transport_put(character) simple_uart_put(character)

#define log_transport_get_char_nonblocking(buffer) simple_uart_get_with_timeout(0, buffer)
#define log_transport_get_char_blocking() simple_uart_get()

#endif
