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

#include <LogTransport.h>

#ifdef USE_SEGGER_RTT_INSTEAD_OF_UART

extern "C"{
	void log_transport_putstring(const u8* message){
		SEGGER_RTT_WriteString(0, (const char*) message);
	}

	void log_transport_put(u8 character){
		u8 buffer[1];
		buffer[0] = character;
		SEGGER_RTT_Write(0, (const char*)buffer, 1);
	}

	bool log_transport_get_char_nonblocking(u8* buffer){
		if(SEGGER_RTT_HasKey()){
			char a = SEGGER_RTT_WaitKey();
			buffer[0] = a;

			return true;
		} else {
			return false;
		}
	}
	u8 log_transport_get_char_blocking(){
		return SEGGER_RTT_WaitKey();
	}
}

#endif
