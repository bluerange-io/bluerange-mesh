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

/*
 * The Utility class holds a number of auxiliary functions
 */

#pragma once

#include <types.h>
#include <Config.h>
#include <Module.h>

typedef struct Aes128Block {
	uint8_t data[16];
}Aes128Block;

class Module;
class RecordStorageEventListener;

namespace Utility
{

	const char serialAlphabet[] = "BCDFGHJKLMNPQRSTVWXYZ123456789";

	//General methods for loading settings
	u32 GetSettingsPageBaseAddress();
	bool LoadSettingsFromFlash(Module* module, moduleID moduleId, ModuleConfiguration* configurationPointer, u16 configurationLength);
	bool LoadSettingsFromFlashWithId(moduleID moduleId, ModuleConfiguration* configurationPointer, u16 configurationLength);
	RecordStorageResultCode SaveModuleSettingsToFlash(const Module* module, ModuleConfiguration* configurationPointer, const u16 configurationLength, RecordStorageEventListener* listener, u32 userType, u8* userData, u16 userDataLength);

	//Serial number and version utilities
	u32 GetIndexForSerial(const char* serialNumber);
	void GenerateBeaconSerialForIndex(u32 index, char* serialBuffer);
	void GetVersionStringFromInt(const u32 version, char* outputBuffer);

	//Random functionality
	u32 GetRandomInteger(void);

	//CRC calculation
	uint8_t CalculateCrc8(const u8* data, u16 dataLength);
	uint16_t CalculateCrc16(const uint8_t * p_data, const uint32_t size, const uint16_t * p_crc);
	u32 CalculateCrc32(const u8* message, const i32 messageLength);

	//Encryption Functionality
	void Aes128BlockEncrypt(const Aes128Block* messageBlock, const Aes128Block* key, Aes128Block* encryptedMessage);
	void XorWords(const u32* src1, const u32* src2, const u8 numWords, u32* out);
	void XorBytes(const u8* src1, const u8* src2, const u8 numBytes, u8* out);

	//Memory modification
	void swapBytes(u8 *data, const size_t length);//Reverses the direction of bytes according to the length
	bool CompareMem(const u8 byte, const u8* data, const u16 dataLength);

	//String manipulation
	void ToUpperCase(char* str);

	//Other
	void CheckFreeHeap(void);


}

