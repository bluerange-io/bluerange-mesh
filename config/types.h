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
 * In order to simplify typing and readability, a few custom types have been
 * declared in this file.
 */

#pragma once

#ifdef SIM_ENABLED
#include "stdafx.h"
#endif

#ifdef IAR
#include "vector"
#endif


extern "C"{
#include <sdk_common.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <app_error.h>
#include <nrf_soc.h>
#include <ble.h>
#include <ble_gap.h>
#include <ble_gatt.h>
#include <ble_gatts.h>
#include <string.h>

#if defined(NRF51) || defined(SIM_ENABLED)
#include <ble_stack_handler_types.h>
#endif
}

#include <FruityHal.h>

/*## GENERAL TYPEDEFS FOR CONVENIENCE ##############################################*/

//Unsigned ints
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint32_t u64; //WARNING: u64 caused alignement fault when used

//Signed ints
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;

//A struct that combines a data pointer and the accompanying length
typedef struct
{
    u8*		data; //Pointer to data
    u16		length; //Length of Data
}sizedData;

//Data types for the mesh
typedef u16 networkID;
typedef u16 nodeID;
typedef u32 clusterID;
typedef i16 clusterSIZE;

/*############ Regarding node ids ################*/
// Refer to protocol specification @https://github.com/mwaylabs/fruitymesh/wiki/Protocol-Specification
#define NODE_ID_BROADCAST 0
#define NODE_ID_DEVICE_BASE 0
#define NODE_ID_GROUP_BASE 20000
#define NODE_ID_HOPS_BASE 30000
#define NODE_ID_SHORTEST_SINK 31000

#define NODE_ID_APP_BASE 32000 //Custom GATT services, connections with Smartphones, should use (APP_BASE + moduleId)

#define NODE_ID_RESERVED 33000 //Yet unassigned nodIds

#define NODE_ID_GROUP_BASE_SIZE 10000
#define NODE_ID_HOPS_BASE_SIZE 1000
#define NODE_ID_APP_BASE_SIZE 1000


//Maximum data that can be transmitted with one write
//Max value according to: http://developer.nordicsemi.com/nRF51_SDK/doc/7.1.0/s120/html/a00557.html
#define MAX_DATA_SIZE_PER_WRITE 20

#define EMPTY_WORD 0xFFFFFFFF

/*############ OTHER STUFF ################*/	
typedef struct
{
	nodeID	id; 		//nodeID
	u8 bleAddressType;  /**< See @ref BLE_GAP_ADDR_TYPES. */
	u8 bleAddress[BLE_GAP_ADDR_LEN];  /**< 48-bit address, LSB format. */
}nodeAddress;

/*
 Different kind of mesh devices:
 - 0:Static - A normal node that remains static
 - 1:Roaming - A node that is moving constantly or often
 - 2:Static - A static node that wants to acquire data
 * */
enum deviceTypes{DEVICE_TYPE_STATIC=0, DEVICE_TYPE_ROAMING=1, DEVICE_TYPE_SINK=2};

#pragma pack(push)
#pragma pack(1)
//All module configs have to be packed with #pragma pack and their declarations
//have to be aligned on a 4 byte boundary to be able to save them
struct ModuleConfiguration{
	u8 moduleId; //Id of the module, compared upon load and must match
	u8 moduleVersion; //version of the configuration
	u8 moduleActive; //Activate or deactivate the module
	u8 reserved;
	//IMPORTANT: Each individual module configuration should add a reserved u32 at
	//its end that is set to 0. Because we can only save data that is a
	//multiple of 4 bytes. We use this variable to pad the data.
};
#pragma pack(pop)


enum TerminalMode {
	TERMINAL_JSON_MODE, //Interrupt based terminal input and blocking output
	TERMINAL_PROMPT_MODE, //blockin in and out with echo and backspace options
	TERMINAL_DISABLED //Terminal is disabled, no in and output
};

/*############ BOOTLOADER ################*/
//Uses word-sized variables to avoid padding problems. No need to save space in our flash-page
#define BOOTLOADER_MAGIC_NUMBER 0xF0771234
#define BOOTLOADER_BITMASK_SIZE 60
#if defined(NRF51)
#define BOOTLOADER_UICR_ADDRESS           (NRF_UICR->BOOTLOADERADDR)
#define FLASH_REGION_START_ADDRESS			0x00000000
#elif defined(NRF52)
#define BOOTLOADER_UICR_ADDRESS           (NRF_UICR->NRFFW[0])
#define FLASH_REGION_START_ADDRESS			0x00000000UL
#elif defined(SIM_ENABLED)
#define BOOTLOADER_UICR_ADDRESS           (NRF_UICR->BOOTLOADERADDR)
#endif

//This is where the bootloader settings are saved
#if defined(NRF51) || defined(SIM_ENABLED)
#define REGION_BOOTLOADER_SETTINGS_START 0x0003FC00
#elif defined(NRF52)
#define REGION_BOOTLOADER_SETTINGS_START 0x0007F000
#endif

enum imageType{ IMAGE_TYPE_SOFTDEVICE, IMAGE_TYPE_APP, IMAGE_TYPE_APP_FORCED, IMAGE_TYPE_SD_AND_APP, IMAGE_TYPE_BOOTLOADER };
typedef struct
{
	u32 updatePending; //Should be set to the magic number, don't move!
	u32 imageType;
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
	u32 pagesStored[BOOTLOADER_BITMASK_SIZE]; //bit sequence of how many pages have been stored successfully
	u32 pagesMoved[BOOTLOADER_BITMASK_SIZE]; //bit sequence of how many pages the bootloader has moved so far

} bootloaderSettings;


/*############ ADVERTISING ################*/	
//Defines the different advertising intervals for each state
typedef enum {
	ADV_STATE_OFF 		= 0x000,
	ADV_STATE_LOW 		= 0xF00,
	ADV_STATE_MEDIUM 	= 0x800,
	ADV_STATE_HIGH 		= 0x400
} advState;

/*############ SCANNING ################*/	
//Defines the different scanning intervals for each state
typedef enum {
	SCAN_STATE_OFF 		= 0x000,
	SCAN_STATE_LOW 		= 0xF00,
	SCAN_STATE_MEDIUM 	= 0x800,
	SCAN_STATE_HIGH 	= 0x400
} scanState;

#if defined(NRF51)
#define RADIO_NOTIFICATION_IRQ_PRIORITY      3
#elif defined(NRF52)
#define RADIO_NOTIFICATION_IRQ_PRIORITY      6
#endif

//States
//These are the different states
//DISCOVERY_HIGH: Scanning and Advertising at high interval
//DISCOVERY_LOW: Only sporadically advertising
//Connection: Currently trying to establish a connection
//BACK_OFF: No nodes found for a while, backing off for a few seconds
//DISCOVERY_OFF: No new nodes will be discovered until discovery is switched on again.
enum discoveryState
{
	INVALID_STATE, BOOTUP, DISCOVERY, DISCOVERY_HIGH, DISCOVERY_LOW, DECIDING, HANDSHAKE, HANDSHAKE_TIMEOUT, CONNECTING, BACK_OFF, DISCOVERY_OFF
};

//TODO: Move
//All known Subtypes of BaseConnection supported by the ConnectionManager
enum ConnectionTypes { CONNECTION_TYPE_INVALID, CONNECTION_TYPE_FRUITYMESH, CONNECTION_TYPE_APP, CONNECTION_TYPE_CLC_APP, CONNECTION_TYPE_RESOLVER };

//Led mode that defines what the LED does (mainly for debugging)
enum ledMode
{
	LED_MODE_OFF, LED_MODE_ON, LED_MODE_CONNECTIONS, LED_MODE_RADIO, LED_MODE_CLUSTERING, LED_MODE_ASSET, LED_MODE_CUSTOM
};

/*########## Alignment ###########*/
//In order to send data packets across the mesh in an efficiant manner
//we have to keep the size as small as possible which is why all network structures
//are packed. Storing module data also has to be as small as possible to save flash
//space, but we need to align each module configuration on a 4-byte boundary
//to be able to save it.
#ifdef __GNUC__
#define PACK_AND_ALIGN_4 __attribute__((aligned(4)))

//Because the sizeof operator does not work in the intended way when a struct
//is aligned, we have to calculate the size by extending the struct and aligning it
#define DECLARE_CONFIG_AND_PACKED_STRUCT(structname) struct structname##Aligned : structname {} __attribute__((packed, aligned(4))); structname##Aligned configuration
//Because Visual Studio does not support C99 dynamic arrays
#define DYNAMIC_ARRAY(arrayName, size) u8 arrayName[size]
#endif
#if defined(_MSC_VER)
#define DECLARE_CONFIG_AND_PACKED_STRUCT(structname) structname configuration
#define DYNAMIC_ARRAY(arrayName, size) u8* arrayName = (u8*)alloca(size)
#endif
#ifdef __ICCARM__
#define DECLARE_CONFIG_AND_PACKED_STRUCT(structname) struct structname##Aligned : structname {} __attribute__((packed, aligned(4))); structname##Aligned configuration
#define DYNAMIC_ARRAY(arrayName, size) u8* arrayName = (std::vector<u8>(size)).data()
#endif


/*############ ERRORS ################*/
#define FRUITYMESH_ERROR_BASE 0xF000
#define FRUITYMESH_ERROR_NO_FREE_CONNECTION_SLOTS (FRUITYMESH_ERROR_BASE + 1)
#define FRUITYMESH_ERROR_PURE_VIRTUAL_FUNCTION_CALL (FRUITYMESH_ERROR_BASE + 2)

/*############ HELPFUL MACROS ################*/
#define PAGE_SIZE NRF_FICR->CODEPAGESIZE
#define FLASH_SIZE (NRF_FICR->CODESIZE*NRF_FICR->CODEPAGESIZE)

#define TO_ADDR(page) ((u32*)(page*PAGE_SIZE))
#define TO_PAGE(addr) ((((u32)addr)/PAGE_SIZE))
#define TO_PAGES_CEIL(size) ((size + PAGE_SIZE - 1) / PAGE_SIZE) //Calculates the number of pages and rounds up

//Returns true if the timer should have trigered the interval in the passedTime
#define SHOULD_IV_TRIGGER(timer, passedTime, interval) (interval != 0 && (((timer)-(passedTime)) % (interval) >= (timer) % (interval)))

//Returns true if the button action should execute
#define SHOULD_BUTTON_EVT_EXEC(BUTTON_DS) (BUTTON_DS != 0 && holdTimeDs > BUTTON_DS && holdTimeDs < (u32)(BUTTON_DS + 20))
//Converts Seconds to Deciseconds
#define SEC_TO_DS(sec) (((u32)(sec))*10)
