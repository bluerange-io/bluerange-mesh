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

#include <Utility.h>
#include <Logger.h>

extern "C"{
#include <nrf_soc.h>
#include <nrf_error.h>
#include <stdlib.h>
#include <malloc.h>
}

u32 Utility::GetRandomInteger(void)
{
	u32 randomNumber;

	u32 err = sd_rand_application_vector_get((u8*) &randomNumber, 4);
	if (err == NRF_SUCCESS)
		return randomNumber;
	else{
		i32 temp;
		err = sd_temp_get(&temp);
		logt("ERROR", "Random number generator not yet initialized, temperature used");
		return temp;
	}
}

void Utility::CheckFreeHeap(void)
{
	struct mallinfo used = mallinfo();
	u32 size = used.uordblks + used.hblkhd;

	uart("NODE", "{\"heap\":%u}", size);
}
