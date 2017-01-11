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
 * In order to simplify typing and readability, a few custom types have been
 * declared in this file.
 */

#pragma once


//Some outdated defines to get the project running under Visual Studio
#ifdef _MSC_VER
typedef unsigned char uint8_t;
#define __INLINE
#define _CRT_SECURE_NO_WARNINGS
#include <stdafx.h>
#define BLE_GAP_ADDR_LEN            6 //???? no clue, why VC does not find the ble_gap.h

//not #if defined(_WIN32) || defined(_WIN64) because we have strncasecmp in mingw
#define strncasecmp _strnicmp
#define strcasecmp _stricmp

//#define sprintf sprintf_s
#endif


extern "C"{
#include <sdk_common.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <ble_gap.h>
#include <ble_gatts.h>
#include <string.h>
}





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

//A struct that is used for passing messages
typedef struct
{
	u16 connectionHandle;
	bool reliable;
	u8* data;
	u16 dataLength;
}connectionPacket;


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
#define NODE_ID_SHORTEST_SINK 31001


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

/*############ BOOTLOADER ################*/
//Uses word-sized variables to avoid padding problems. No need to save space in our flash-page
#define BOOTLOADER_MAGIC_NUMBER 0xF0771234
#define BOOTLOADER_BITMASK_SIZE 60
enum imageType{ IMAGE_TYPE_SOFTDEVICE, IMAGE_TYPE_APP, IMAGE_TYPE_APP_FORCED };
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

//Led mode that defines what the LED does (mainly for debugging)
enum ledMode
{
	LED_MODE_OFF, LED_MODE_ON, LED_MODE_CONNECTIONS, LED_MODE_RADIO, LED_MODE_CLUSTERING, LED_MODE_ASSET
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
#endif


/*############ HELPFUL MACROS ################*/
#define PAGE_SIZE NRF_FICR->CODEPAGESIZE
#define FLASH_SIZE (NRF_FICR->CODESIZE*NRF_FICR->CODEPAGESIZE)

#define TO_ADDR(page) ((u32*)(page*PAGE_SIZE))
#define TO_PAGE(addr) ((((u32)addr)/PAGE_SIZE))
#define TO_PAGES_CEIL(size) ((size + PAGE_SIZE - 1) / PAGE_SIZE) //Calculates the number of pages and rounds up

//Returns true if the timer should have trigered the interval in the passedTime
#define SHOULD_IV_TRIGGER(timer, passedTime, interval) (((timer)-(passedTime)) % (interval) >= (timer) % (interval))

//Converts Seconds to Deciseconds
#define SEC_TO_DS(sec) ((sec)*10)
