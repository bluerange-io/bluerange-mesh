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
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <ble_gap.h>
#include <ble_gatts.h>
#include <cstring>
}





/*## GENERAL TYPEDEFS FOR CONVENIENCE ##############################################*/

//Unsigned ints
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

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

//Maximum data that can be transmitted with one write
//Max value according to: http://developer.nordicsemi.com/nRF51_SDK/doc/7.1.0/s120/html/a00557.html
#define MAX_DATA_SIZE_PER_WRITE 20


/*############ OTHER STUFF ################*/	
typedef struct
{
	nodeID	id; 		//nodeID
	u8 bleAddressType;  /**< See @ref BLE_GAP_ADDR_TYPES. */
	u8 bleAddress[BLE_GAP_ADDR_LEN];  /**< 48-bit address, LSB format. */
}nodeAddress;


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
	INVALID_STATE, BOOTUP, DISCOVERY, DISCOVERY_HIGH, DISCOVERY_LOW, DECIDING, HANDSHAKE, CONNECTING, BACK_OFF, DISCOVERY_OFF
};

