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
 * This file contains custom types as well as values from the specification which must not be
 * changed.
 */
#pragma once

 /*## General types for convenience #############################################################*/
#include "PrimitiveTypes.h"

extern "C" {

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <string.h>
#include "ble_db_discovery.h"
}


#ifdef SIM_ENABLED
#include "Exceptions.h"
#else
#define IGNOREEXCEPTION(T)
#define SIMEXCEPTION(T)
#define SIMEXCEPTIONFORCE(T)
#endif

#ifdef SIM_ENABLED
#include "StackWatcher.h"
#define START_OF_FUNCTION() StackWatcher sw;
#else
#define START_OF_FUNCTION
#endif

#ifdef CHERRYSIM_TESTER_ENABLED
#define TESTER_PUBLIC public
#else
#define TESTER_PUBLIC private
#endif

#ifdef IAR
#include "vector"
#endif

/*## General defines #############################################################*/

// Mesh identifiers
#define COMPANY_IDENTIFIER 0x024D // Company identifier for manufacturer specific data header (M-Way Solutions GmbH) - Should not be changed to ensure compatibility with the mesh protocol
#define MESH_IDENTIFIER 0xF0 // Identifier that defines this as the fruitymesh protocol under the manufacturer specific data
#define SERVICE_DATA_SERVICE_UUID16 0xFE12 // Service UUID (M-Way Solutions) to identify our custom mesh service, e.g. used for MeshAccessConnections

// Serial numbers in ASCII format are currently 5 bytes long and can be extended in the future
// A serial uses the alphabet "BCDFGHJKLMNPQRSTVWXYZ123456789".
// This is good for readability (short, no inter-digit resemblance,
// 25 million possible combinations, no funny words)
#define NODE_SERIAL_NUMBER_LENGTH 5

// End of line seperators to use
#define EOL "\r\n"
#define SEP "\r\n"

//Maximum data that can be transmitted with one write
//Max value according to: http://developer.nordicsemi.com/nRF51_SDK/doc/7.1.0/s120/html/a00557.html
#define MAX_DATA_SIZE_PER_WRITE 20

#define BLE_GAP_SCAN_PACKET_BUFFER_SIZE (31)

// An invalid entry or an empty word
#define EMPTY_WORD 0xFFFFFFFFUL
#define INVALID_SERIAL_NUMBER 0xFFFFFFFFUL

// Set in the first 4 bytes of UICR if factory settings are available
#define UICR_SETTINGS_MAGIC_WORD 0xF07700

// Magic number for recognizing the app (saved at a fixed position)
#define APP_ID_MAGIC_NUMBER 0xF012F134

// Magic numbers for enabling nordic secure dfu
#define NORDIC_DFU_MAGIC_NUMBER_ADDRESS ((u8*)bootloaderAddress + (0x400 + sizeof(uint32_t) * 1))
#define ENABLE_NORDIC_DFU_MAGIC_NUMBER 0xF012F117 //Magic number to enable the nordic dfu functionality in the bootloader

// Magic number that is used chose whether to reboot in safe mode or not
#define REBOOT_MAGIC_NUMBER 0xE0F7213C

// Featureset group ids, these are used to determine if a firmware can be updated over the
// air with a given firmware. For an update, the firmware group ids must match.
enum class FeatureSetGroup : NodeId
{
	INVALID                                               = 0,
	//These comments are used to parse values with FruityDeploy (do not remove)
	//                              CHIP_NRF51            = 20000,
	//                              CHIP_NRF52            = 20001,
	/*FruityDeploy-FeatureSetGroup*/NRF51_SINK            = 20002,
	/*FruityDeploy-FeatureSetGroup*/NRF51_MESH            = 20003,
	/*FruityDeploy-FeatureSetGroup*/NRF52_MESH            = 20004,
	/*FruityDeploy-FeatureSetGroup*/NRF51_ASSET           = 20005,
	/*FruityDeploy-FeatureSetGroup*/NRF52_ASSET           = 20006,
	/*FruityDeploy-FeatureSetGroup*/NRF52_CLC_MESH        = 20007,
	/*FruityDeploy-FeatureSetGroup*/NRF51_CLC_SINK        = 20008,
	/*FruityDeploy-FeatureSetGroup*/NRF52_VS_MESH         = 20009,
	/*FruityDeploy-FeatureSetGroup*/NRF52_VS_SINK         = 20010,
	/*FruityDeploy-FeatureSetGroup*/NRF52_SINK            = 20011,
	/*FruityDeploy-FeatureSetGroup*/NRF51_EINK            = 20012,
	/*FruityDeploy-FeatureSetGroup*/NRF52840_WM_MESH      = 20013,
	/*FruityDeploy-FeatureSetGroup*/NRF52_PC_BRIDGE       = 20014,
	//                              CHIP_NRF52840         = 20015,
	/*FruityDeploy-FeatureSetGroup*/NRF52840_MESH         = 20016,
	/*FruityDeploy-FeatureSetGroup*/NRF52840_SINK_USB     = 20017,
};

//Sets the maximum number of firmware group ids that can be compiled into the firmware
#define MAX_NUM_FW_GROUP_IDS 2

/*## Types and enums #############################################################*/
#define STATIC_ASSERT_SIZE(T, size) static_assert(sizeof(T) == (size), "STATIC_ASSERT_SIZE failed!")

//This struct is used for saving information that is retained between reboots
#pragma pack(push)
#pragma pack(1)
#define RAM_PERSIST_STACKSTRACE_SIZE 4
struct RamRetainStruct {
	RebootReason rebootReason; // e.g. hardfault, softdevice fault, app fault,...
	u32 code1;
	u32 code2;
	u32 code3;
	u8 stacktraceSize;
	u32 stacktrace[RAM_PERSIST_STACKSTRACE_SIZE];
	u32 crc32;
};
STATIC_ASSERT_SIZE(RamRetainStruct, RAM_PERSIST_STACKSTRACE_SIZE * 4 + 18);
#pragma pack(pop)

// Header for all module configurations
#pragma pack(push)
#pragma pack(1)
//All module configs have to be packed with #pragma pack and their declarations
//have to be aligned on a 4 byte boundary to be able to save them
#define SIZEOF_MODULE_CONFIGURATION_HEADER 4
typedef struct ModuleConfiguration{
	ModuleId moduleId; //Id of the module, compared upon load and must match
	u8 moduleVersion; //version of the configuration
	u8 moduleActive; //Activate or deactivate the module
	u8 reserved;
	//IMPORTANT: Each individual module configuration should add a reserved u32 at
	//its end that is set to 0. Because we can only save data that is a
	//multiple of 4 bytes. We use this variable to pad the data.
} ModuleConfiguration;
STATIC_ASSERT_SIZE(ModuleConfiguration, SIZEOF_MODULE_CONFIGURATION_HEADER);
#pragma pack(pop)

/*## Bootloader stuff #############################################################*/
// These defines are shared with the bootloader and must not change as this will make
// the flashed bootloaders incompatible with fruitymesh
//Uses word-sized variables to avoid padding problems. No need to save space in our bootloader flash-page

// Magic number to check if a new firmware should be installed
#define BOOTLOADER_MAGIC_NUMBER 0xF0771234

// Bitmask to store if a chunk has been stored in flash and was crc checked
#define BOOTLOADER_BITMASK_SIZE 60

// Location of the bootloader
#if defined(NRF51)
#define FLASH_REGION_START_ADDRESS			0x00000000
#elif defined(NRF52) || defined(NRF52840)
#define FLASH_REGION_START_ADDRESS			0x00000000UL
#endif

// Bootloader settings page where the app needs to store the update information
#if defined(NRF51) || defined(SIM_ENABLED)
#define REGION_BOOTLOADER_SETTINGS_START (FLASH_REGION_START_ADDRESS + 0x0003FC00) //Last page of flash
#elif defined(NRF52840)
#define REGION_BOOTLOADER_SETTINGS_START (FLASH_REGION_START_ADDRESS + 0x000FF000) //Last page of flash
#elif defined(NRF52)
#define REGION_BOOTLOADER_SETTINGS_START (FLASH_REGION_START_ADDRESS + 0x0007F000) //Last page of flash
#endif

// Image types supported by the bootloader or component if a 3rd party device should be updated
enum class ImageType : u8{
	SOFTDEVICE = 0,
	APP        = 1,
	APP_FORCED = 2,
	SD_AND_APP = 3,
	BOOTLOADER = 4,
	COMPONENT  = 5,
	INVALID    = 0xFF,
};

// Struct that contains the bootloader settings share between fruitymesh and bootloader
struct BootloaderSettings {
	u32 updatePending; //Should be set to the magic number, don't move!
	ImageType imageType;
	u8 reserved[3];
	u32 sdStartPage;
	u32 sdNumPages;
	u32 appStartPage;
	u32 appNumPages;
	u32 imageStartPage;
	u32 imageNumPages;

	u32 dfuMasterNodeId;
	u32 dfuInProgressRequestHandle;

	u32 dfuFirmwareVersion;
	u32 dfuNumChunks;
	u32 dfuChunkSize;
	u32 dfuImageCRC;

	//ATTENTION: nRF52 has nWrites specified with 181 word writes after which a flash page must be erased
	u32 chunksStored[BOOTLOADER_BITMASK_SIZE]; //bit sequence of how many CHUNKS have been stored successfully
	u32 pagesMoved[BOOTLOADER_BITMASK_SIZE]; //bit sequence of how many pages the bootloader has moved so far

	ModuleId moduleId; //Stores the moduleId that handles the update
	u8 reserved2[3];
	u32 componentId; //Stores the componentId (if many updateable devices are connected to a module)
};
STATIC_ASSERT_SIZE(BootloaderSettings, (16 + BOOTLOADER_BITMASK_SIZE + BOOTLOADER_BITMASK_SIZE) * sizeof(u32));

/*## Alignment #############################################################*/
//In order to send data packets across the mesh in an efficiant manner
//we have to keep the size as small as possible which is why all network structures
//are packed. Storing module data also has to be as small as possible to save flash
//space, but we need to align each module configuration on a 4-byte boundary
//to be able to save it.

#ifdef __GNUC__
//Because the sizeof operator does not work in the intended way when a struct
//is aligned, we have to calculate the size by extending the struct and aligning it
#define DECLARE_CONFIG_AND_PACKED_STRUCT(structname) struct structname##Aligned : structname {} __attribute__((packed, aligned(4))); structname##Aligned configuration
//Because Visual Studio does not support C99 dynamic arrays
#define DYNAMIC_ARRAY(arrayName, size) alignas(4) u8 arrayName[size]
#endif
#if defined(_MSC_VER)
#include <malloc.h>
#define DECLARE_CONFIG_AND_PACKED_STRUCT(structname) structname configuration
#define DYNAMIC_ARRAY(arrayName, size) u8* arrayName = (u8*)alloca(size)
#endif

/*############ HELPFUL MACROS ################*/

//Returns true if the timer should have trigered the interval in the passedTime
#define SHOULD_IV_TRIGGER(timer, passedTime, interval) (interval != 0 && (((timer)-(passedTime)) % (interval) >= (timer) % (interval)))

//Returns true if the button action should execute
#define SHOULD_BUTTON_EVT_EXEC(BUTTON_DS) (BUTTON_DS != 0 && holdTimeDs > BUTTON_DS && holdTimeDs < (u32)(BUTTON_DS + 20))
//Converts Seconds to Deciseconds and vice versa
#define SEC_TO_DS(sec) (((u32)(sec))*10)
#define DS_TO_SEC(ds)  (((u32)(ds))/10)
//Checks if packet contains the variable
#define CHECK_MSG_SIZE(packetHeader, variable, size, dataLength) ((((const u8*)variable) + (size)) - ((const u8*)packetHeader) <= (dataLength))
//Macros for concatenating
#define CONCAT(x,y) x##y
#define XCONCAT(x,y) CONCAT(x,y)

/* var=target variable, pos=bit position number to act upon 0-n */
#define BIT_CHECK(var,pos) (!!((var) & (1ULL<<(pos)))) //check whether bit is 1 or 0
#define BIT_SET(var,pos) ((var) |= (1ULL<<(pos))) // set bit to 1
#define BIT_CLEAR(var,pos) ((var) &= ~(1ULL<<(pos)))//set bit to 0

#ifndef SIM_ENABLED
#define SIMSTATCOUNT(cmd) do{}while(0)
#define SIMSTATAVG(key, value) do{}while(0)
#endif

//Check if a feature is active
#define IS_ACTIVE(featureName) (ACTIVATE_##featureName == 1)
#define IS_INACTIVE(featureName) (ACTIVATE_##featureName == 0)

//Converts pointer to HEX string array.
#define PRINT_DEBUG(data, dataSize) DYNAMIC_ARRAY(data##Hex, (dataSize)*3+1); Logger::convertBufferToHexString(data, (dataSize), (char*)data##Hex, (dataSize)*3+1)

/*############ Include packet definitions ################*/
#include <adv_packets.h>
#include <conn_packets.h>
