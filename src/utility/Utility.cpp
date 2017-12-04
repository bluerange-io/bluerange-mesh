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

#include <Utility.h>
#include <Logger.h>

extern "C"{
#include <nrf_soc.h>
#include <nrf_error.h>
#include <stdlib.h>
#ifndef __ICCARM__
#include <malloc.h>
#endif
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
#ifdef __GNUC__
	struct mallinfo used = mallinfo();
	u32 size = used.uordblks + used.hblkhd;

	logjson("NODE", "{\"heap\":%u}" SEP, size);
#endif
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


uint8_t Utility::CalculateCrc8(u8* data, u16 dataLength)
{
	uint8_t CRC = 0x00;
	uint16_t tmp;

	while (dataLength > 0)
	{
		tmp = CRC << 1;
		tmp += *data;
		CRC = (tmp & 0xFF) + (tmp >> 8);
		data++;
		--dataLength;
	}

	return CRC;
}

//void Utility::CalculateCRC16
/**@brief Function for calculating CRC-16 in blocks.
 *
 * Feed each consecutive data block into this function, along with the current value of p_crc as
 * returned by the previous call of this function. The first call of this function should pass NULL
 * as the initial value of the crc in p_crc.
 *
 * @param[in] p_data The input data block for computation.
 * @param[in] size   The size of the input data block in bytes.
 * @param[in] p_crc  The previous calculated CRC-16 value or NULL if first call.
 *
 * @return The updated CRC-16 value, based on the input supplied.
 */
uint16_t Utility::CalculateCrc16(const uint8_t * p_data, uint32_t size, const uint16_t * p_crc){
	uint32_t i;
	uint16_t crc = (p_crc == NULL) ? 0xffff : *p_crc;

	for (i = 0; i < size; i++)
	{
		crc  = (unsigned char)(crc >> 8) | (crc << 8);
		crc ^= p_data[i];
		crc ^= (unsigned char)(crc & 0xff) >> 4;
		crc ^= (crc << 8) << 4;
		crc ^= ((crc & 0xff) << 4) << 1;
	}

	return crc;
}

//Taken from http://www.hackersdelight.org/hdcodetxt/crc.c.txt
u32 Utility::CalculateCrc32(u8* message, i32 messageLength) {
#pragma warning( push )
#pragma warning( disable : 4146)
   i32 i, j;
   unsigned int byte, crc, mask;

   i = 0;
   crc = 0xFFFFFFFF;
   while (i < messageLength) {
	  byte = message[i];            // Get next byte.
	  crc = crc ^ byte;
	  for (j = 7; j >= 0; j--) {    // Do eight times.
		 mask = -(crc & 1);
		 crc = (crc >> 1) ^ (0xEDB88320 & mask);
	  }
	  i = i + 1;
   }
   return ~crc;
#pragma warning( pop ) 
}

//Encrypts a message
void Utility::Aes128BlockEncrypt(Aes128Block* messageBlock, Aes128Block* key, Aes128Block* encryptedMessage)
{
	u32 err;

	nrf_ecb_hal_data_t blockToEncrypt;
	memcpy(blockToEncrypt.key, key->data, 16);
	memcpy(blockToEncrypt.cleartext, messageBlock->data, 16);

	err = sd_ecb_block_encrypt(&blockToEncrypt);
	memcpy(encryptedMessage->data, blockToEncrypt.ciphertext, 16);
}
