////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2019 M-Way Solutions GmbH
// ** Contact: https://www.blureange.io/licensing
// **
// ** This file is part of the Bluerange/FruityMesh implementation
// **
// ** $BR_BEGIN_LICENSE:GPL-EXCEPT$
// ** Commercial License Usage
// ** Licensees holding valid commercial Bluerange licenses may use this file in
// ** accordance with the commercial license agreement provided with the
// ** Software or, alternatively, in accordance with the terms contained in
// ** a written agreement between them and M-Way Solutions GmbH. 
// ** For licensing terms and conditions see https://www.bluerange.io/terms-conditions. For further
// ** information use the contact form at https://www.bluerange.io/contact.
// **
// ** GNU General Public License Usage
// ** Alternatively, this file may be used under the terms of the GNU
// ** General Public License version 3 as published by the Free Software
// ** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
// ** included in the packaging of this file. Please review the following
// ** information to ensure the GNU General Public License requirements will
// ** be met: https://www.gnu.org/licenses/gpl-3.0.html.
// **
// ** $BR_END_LICENSE$
// **
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////

#include <Utility.h>
#include <Logger.h>
#include <mini-printf.h>
#include <RecordStorage.h>
#include <Module.h>
#include <cctype>

extern "C"{
#include <nrf_soc.h>
#include <nrf_error.h>
#include <stdlib.h>
#ifndef __ICCARM__
#include <malloc.h>
#endif
}


u32 Utility::GetSettingsPageBaseAddress()
{
	const bool bootloaderAvailable = (BOOTLOADER_UICR_ADDRESS != 0xFFFFFFFF);
	const u32 bootloaderAddress = bootloaderAvailable ? BOOTLOADER_UICR_ADDRESS : FLASH_SIZE;
	const u32 appSettingsAddress = bootloaderAddress - (RECORD_STORAGE_NUM_PAGES)* PAGE_SIZE;

	return (appSettingsAddress);
}

bool Utility::LoadSettingsFromFlashWithId(moduleID moduleId, ModuleConfiguration* configurationPointer, u16 configurationLength)
{
	return Utility::LoadSettingsFromFlash(nullptr, moduleId, configurationPointer, configurationLength);
}

bool Utility::LoadSettingsFromFlash(Module* module, moduleID moduleId, ModuleConfiguration* configurationPointer, u16 configurationLength)
{
	if (Conf::loadConfigFromFlash) {
		sizedData configData = GS->recordStorage->GetRecordData(moduleId);

		//Check if configuration exists and has the correct version, if yes, copy to module configuration struct
		if (configData.length > SIZEOF_MODULE_CONFIGURATION_HEADER && ((ModuleConfiguration*)configData.data)->moduleVersion == configurationPointer->moduleVersion) {
			memcpy((u8*)configurationPointer, configData.data, configData.length);

			logt("CONFIG", "Config for module %u loaded", moduleId, configurationPointer->moduleVersion, configData.length);

			if(module != nullptr) module->ConfigurationLoadedHandler(nullptr, 0);

			return true;
		}
		//If the configuration has a different version, we call the migration if it exists
		else if(configData.length > SIZEOF_MODULE_CONFIGURATION_HEADER){
			logt("CONFIG", "Flash config for module %u has mismatching version", moduleId);

			if(module != nullptr) module->ConfigurationLoadedHandler((ModuleConfiguration*)configData.data, configData.length);
		}
		else {
			logt("CONFIG", "No flash config for module %u found, using defaults", moduleId);

			if(module != nullptr) module->ConfigurationLoadedHandler(nullptr, 0);
		}
	}

	return false;
}

RecordStorageResultCode Utility::SaveModuleSettingsToFlash(const Module* module, ModuleConfiguration* configurationPointer, const u16 configurationLength, RecordStorageEventListener* listener, u32 userType, u8* userData, u16 userDataLength)
{
	RecordStorageResultCode err = GS->recordStorage->SaveRecord(module->moduleId, (u8*)configurationPointer, configurationLength, listener, userType, userData, userDataLength);

	return err;
}


u32 Utility::GetRandomInteger(void)
{
	u32 err = NRF_ERROR_BUSY;
	u32 randomNumber;

	while(err != NRF_SUCCESS){
		//A busy loop is fine here because the nordic spec guarantees us, that we will, at some point, get a random number. If not, the node itself is broken.
		err = sd_rand_application_vector_get((u8*) &randomNumber, 4);
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
void Utility::GetVersionStringFromInt(const u32 version, char* outputBuffer)
{
	u16 major = version / 10000000;
	u16 minor = (version - 10000000 * major) / 10000;
	u16 patch = (version - 10000000 * major - 10000 * minor);

	snprintf(outputBuffer, 15, "%u.%u.%u", major, minor, patch);

}

int ipow(int base, int exp)
{
	int result = 1;
	while (exp){
		if (exp & 1) result *= base;
		exp >>= 1;
		base *= base;
	}
	return result;
}

//Compares a memory region with a byte, useful for checking if a memory is empty with 0x00 (e.g. ram) or with 0xFF (e.g. flash)
bool Utility::CompareMem(const u8 byte, const u8* data, const u16 dataLength){
	for(u32 i=0; i<dataLength; i++){
		if(data[i] != byte) return false;
	}
	return true;
}

void Utility::ToUpperCase(char * str)
{
	while (*str = toupper(*str)) str++;
}

u32 Utility::GetIndexForSerial(const char* serialNumber){
	u32 index = 0;
	for(int i=0; i<NODE_SERIAL_NUMBER_LENGTH; i++){
		if(i == NODE_SERIAL_NUMBER_LENGTH-1 && serialNumber[0] == 'A') continue;
		char currentChar = serialNumber[NODE_SERIAL_NUMBER_LENGTH-i-1];
		const char* charPos = strchr(serialAlphabet, currentChar);
		if(charPos == nullptr) return 0;
		u32 charValue = (u32)charPos - (u32)serialAlphabet;
		index += ipow(sizeof(serialAlphabet)-1, i) * charValue;
	}
	return index;
}

void Utility::GenerateBeaconSerialForIndex(u32 index, char* serialBuffer)
{
	memset(serialBuffer, 0x00, NODE_SERIAL_NUMBER_LENGTH+1);
	for(u32 i=0; i<NODE_SERIAL_NUMBER_LENGTH; i++){
		int rest = (int)(index % strlen(serialAlphabet));
		serialBuffer[NODE_SERIAL_NUMBER_LENGTH-i-1] = serialAlphabet[rest];
		index /= strlen(serialAlphabet);
	}

}

/*
void Utility::GetVersionStringFromInts(u16 major, char* outputBuffer)
{
	u16 major = version / 10000000;
	u16 minor = (version - 10000000 * major) / 10000;
	u16 patch = (version - 10000000 * major - 10000 * minor);

	sprintf(outputBuffer, "%u.%u.%u", major, minor, patch);

}*/


uint8_t Utility::CalculateCrc8(const u8* data, u16 dataLength)
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
 * returned by the previous call of this function. The first call of this function should pass nullptr
 * as the initial value of the crc in p_crc.
 * Conforms to CRC-CCITT (0xFFFF), can be calculated with https://www.lammertbies.nl/comm/info/crc-calculation.html
 *
 * @param[in] p_data The input data block for computation.
 * @param[in] size   The size of the input data block in bytes.
 * @param[in] p_crc  The previous calculated CRC-16 value or nullptr if first call.
 *
 * @return The updated CRC-16 value, based on the input supplied.
 */
uint16_t Utility::CalculateCrc16(const uint8_t * p_data, const uint32_t size, const uint16_t * p_crc){
	uint32_t i;
	uint16_t crc = (p_crc == nullptr) ? 0xffff : *p_crc;

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
u32 Utility::CalculateCrc32(const u8* message, const i32 messageLength) {
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
void Utility::Aes128BlockEncrypt(const Aes128Block* messageBlock, const Aes128Block* key, Aes128Block* encryptedMessage)
{
	u32 err;

	nrf_ecb_hal_data_t blockToEncrypt;
	memcpy(blockToEncrypt.key, key->data, 16);
	memcpy(blockToEncrypt.cleartext, messageBlock->data, 16);

	err = sd_ecb_block_encrypt(&blockToEncrypt);
	memcpy(encryptedMessage->data, blockToEncrypt.ciphertext, 16);
}

void Utility::XorBytes(const u8* src1, const u8* src2, const u8 numBytes, u8* out) {
	for(u8 i = 0; i < numBytes; i++) {
		out[i] = src1[i] ^ src2[i];
	}
}

void Utility::swapBytes(u8 *data, const size_t length)
{
    u8 *p = data;
    size_t lo, hi;
    for(lo=0, hi=length-1; hi>lo; lo++, hi--)
    {
        char tmp=p[lo];
        p[lo] = p[hi];
        p[hi] = tmp;
    }
}

void Utility::XorWords(const u32* src1, const u32* src2, const u8 numWords, u32* out) {
	for(u8 i = 0; i < numWords; i++) {
		out[i] = src1[i] ^ src2[i];
	}
}

