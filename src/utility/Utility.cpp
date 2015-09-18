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
	u32 err = NRF_ERROR_BUSY;
	u32 counter = 0;
	u32 randomNumber;

	while(err != NRF_SUCCESS){
		err = sd_rand_application_vector_get((u8*) &randomNumber, 4);
		counter++;

		if(counter > 100000){
			logt("ERROR", "Random number generator is pigheaded and does not deliver random numbers...");
			return 5;
		}
	}

	return randomNumber;
}

void Utility::CheckFreeHeap(void)
{
	struct mallinfo used = mallinfo();
	u32 size = used.uordblks + used.hblkhd;

	uart("NODE", "{\"heap\":%u}" SEP, size);
}

//buffer should have a length of 15 bytes
//major.minor.patch - 111.222.4444
void Utility::GetVersionStringFromInt(u32 version, char* outputBuffer)
{
	u16 major = version / 10000000;
	u16 minor = (version - 10000000 * major) / 10000;
	u16 patch = (version - 10000000 * major - 10000 * minor);

	sprintf(outputBuffer, "%u.%u.%u", major, minor, patch);

}
/*
void Utility::GetVersionStringFromInts(u16 major, char* outputBuffer)
{
	u16 major = version / 10000000;
	u16 minor = (version - 10000000 * major) / 10000;
	u16 patch = (version - 10000000 * major - 10000 * minor);

	sprintf(outputBuffer, "%u.%u.%u", major, minor, patch);

}*/
