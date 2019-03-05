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
 * In order to simplify typing and readability, a few custom types have been
 * declared in this file.
 */
#pragma once

#ifdef SIM_ENABLED
#include "Exceptions.h"
#else
#define IGNOREEXCEPTION(T)
#define SIMEXCEPTION(T)
#endif

#ifdef IAR
#include "vector"
#endif

#ifdef __cplusplus
#define STATIC_ASSERT_SIZE(T, size) static_assert(sizeof(T) == size, "STATIC_ASSERT_SIZE failed!");
#else
#define STATIC_ASSERT_SIZE(T, size)
#endif

#ifdef __cplusplus
extern "C"{
#endif

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
#include "ble_db_discovery.h"

#if defined(NRF51) || defined(SIM_ENABLED)
#include <ble_stack_handler_types.h>
#endif

#ifndef SIM_ENABLED
#include <nrf_mbr.h>
#endif

#ifdef __cplusplus
}
#endif

/*## GENERAL TYPEDEFS FOR CONVENIENCE ##############################################*/

//Unsigned ints
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

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
typedef u16 NetworkId;
typedef u16 NodeId;
typedef u32 ClusterId;
typedef i16 ClusterSize;

/*############ Regarding node ids ################*/
// Refer to protocol specification @https://github.com/mwaylabs/fruitymesh/wiki/Protocol-Specification
#define NODE_ID_BROADCAST 0 //A broadcast will be received by all nodes within one mesh
#define NODE_ID_DEVICE_BASE 1 //Beginning from 1, we can assign nodeIds to individual devices
#define NODE_ID_VIRTUAL_BASE 2000 //Used to assign sub-addresses to connections that do not belong to the mesh but want to perform mesh activity. Used as a multiplier.
#define NODE_ID_GROUP_BASE 20000 //Used to assign group ids to nodes. A node can take part in many groups at once

#define NODE_ID_LOCAL_LOOPBACK 30000 //30000 is like a local loopback address that will only send to the current node,
#define NODE_ID_HOPS_BASE 30000 //30001 sends to the local node and one hop further, 30002 two hops

#define NODE_ID_SHORTEST_SINK 31000

#define NODE_ID_APP_BASE 32000 //Custom GATT services, connections with Smartphones, should use (APP_BASE + moduleId)

#define NODE_ID_GLOBAL_DEVICE_BASE 33000 //Can be used to assign nodeIds that are valid organization wide (e.g. for assets)

#define NODE_ID_RESERVED 40000 //Yet unassigned nodIds

#define NODE_ID_GROUP_BASE_SIZE 10000
#define NODE_ID_HOPS_BASE_SIZE 1000
#define NODE_ID_APP_BASE_SIZE 1000

#define GROUP_ID_NRF51 20000
#define GROUP_ID_NRF52 20001

#define GROUP_ID_NRF51_SINK 20002
#define GROUP_ID_NRF51_MESH 20003
#define GROUP_ID_NRF52_MESH 20004
#define GROUP_ID_NRF51_ASSET 20005
#define GROUP_ID_NRF52_ASSET 20006
#define GROUP_ID_NRF52_CLC_MESH 20007
#define GROUP_ID_NRF51_CLC_SINK 20008
#define GROUP_ID_NRF52_VS_MESH 20009
#define GROUP_ID_NRF52_VS_SINK 20010
#define GROUP_ID_NRF52_SINK 20011
#define GROUP_ID_NRF51_EINK 20012
#define GROUP_ID_NRF52_WM_MESH 20013




//Types of keys used by the mesh and other modules
#define FM_KEY_ID_NONE 0
#define FM_KEY_ID_NODE 1
#define FM_KEY_ID_NETWORK 2
#define FM_KEY_ID_BASE_USER 3
#define FM_KEY_ID_ORGANIZATION 4
#define FM_KEY_ID_USER_DERIVED_START 10
#define FM_KEY_ID_USER_DERIVED_END UINT32_MAX

//Maximum data that can be transmitted with one write
//Max value according to: http://developer.nordicsemi.com/nRF51_SDK/doc/7.1.0/s120/html/a00557.html
#define MAX_DATA_SIZE_PER_WRITE 20

#define EMPTY_WORD 0xFFFFFFFF

/*############ OTHER STUFF ################*/


#define UICR_SETTINGS_MAGIC_WORD 0xF07700 //Set in the first 4 bytes of UICR if factory settings are available

#define APP_ID_MAGIC_NUMBER 0xF012F134 //Magic number for recognizing the app (saved at a fixed position)

//Magic number for enabling nordic secure dfu
#define NORDIC_DFU_MAGIC_NUMBER_ADDRESS ((u8*)bootloaderAddress + (0x400 + sizeof(uint32_t) * 1))
#define ENABLE_NORDIC_DFU_MAGIC_NUMBER 0xF012F117 //Magic number to enable the nordic dfu functionality in the bootloader

#define REBOOT_MAGIC_NUMBER 0xE0F7213C //Magic number that is used chose whether to reboot in safe mode or not

//This struct is used for saving information that is retained between reboots
enum RebootReason {REBOOT_REASON_UNKNOWN, REBOOT_REASON_HARDFAULT, REBOOT_REASON_APP_FAULT, REBOOT_REASON_SD_FAULT, REBOOT_REASON_PIN_RESET, REBOOT_REASON_WATCHDOG, REBOOT_REASON_FROM_OFF_STATE};
#pragma pack(push)
#pragma pack(1)
#define RAM_PERSIST_STACKSTRACE_SIZE 8
struct RamRetainStruct {
		u8 rebootReason; // e.g. hardfault, softdevice fault, app fault,...
		u32 code1;
		u32 code2;
		u32 code3;
		u8 stacktraceSize;
		u32 stacktrace[RAM_PERSIST_STACKSTRACE_SIZE];
		u32 crc32;
};
STATIC_ASSERT_SIZE(RamRetainStruct, RAM_PERSIST_STACKSTRACE_SIZE * 4 + 18);
#pragma pack(pop)

typedef struct
{
	NodeId	id; 		//NodeId
	u8 bleAddressType;  /**< See @ref BLE_GAP_ADDR_TYPES. */
	u8 bleAddress[BLE_GAP_ADDR_LEN];  /**< 48-bit address, LSB format. */
}nodeAddress;

/*
 Different kind of mesh devices:
 - Static - A normal node that remains static
 - Roaming - A node that is moving constantly or often
 - Sink - A static node that wants to acquire data
 - Asset - A roaming node that is sporadically or never connected but broadcasts data
 - Leaf - A node that will never act as a slave but will only connect as a master
 * */
enum deviceTypes{
	DEVICE_TYPE_INVALID=0,
	DEVICE_TYPE_STATIC=1,
	DEVICE_TYPE_ROAMING=2,
	DEVICE_TYPE_SINK=3,
	DEVICE_TYPE_ASSET=4,
	DEVICE_TYPE_LEAF=5
};

#pragma pack(push)
#pragma pack(1)
//All module configs have to be packed with #pragma pack and their declarations
//have to be aligned on a 4 byte boundary to be able to save them
#define SIZEOF_MODULE_CONFIGURATION_HEADER 4
typedef struct ModuleConfiguration{
	u8 moduleId; //Id of the module, compared upon load and must match
	u8 moduleVersion; //version of the configuration
	u8 moduleActive; //Activate or deactivate the module
	u8 reserved;
	//IMPORTANT: Each individual module configuration should add a reserved u32 at
	//its end that is set to 0. Because we can only save data that is a
	//multiple of 4 bytes. We use this variable to pad the data.
} ModuleConfiguration;
STATIC_ASSERT_SIZE(ModuleConfiguration, SIZEOF_MODULE_CONFIGURATION_HEADER);
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
#ifdef __cplusplus
struct BoardConfiguration : ModuleConfiguration {
#else
struct BoardConfigurationC {
#endif
	// ########### BOARD_SPECIFICS ################################################

	//Board Type (aka. boardId) identifies a PCB with its wiring and configuration
	//Multiple boards can be added and the correct one is chosen at runtime depending on the UICR boardId
	//Custom boardTypes should start from 10000
	uint16_t boardType;

	//Default board is pca10031, modify SET_BOARD if different board is required
	//Or flash config data to UICR
	int8_t led1Pin;
	int8_t led2Pin;
	int8_t led3Pin;
	//Defines if writing 0 or 1 to an LED turns it on
	uint8_t ledActiveHigh : 8;

	int8_t button1Pin;
	uint8_t buttonsActiveHigh : 8;

	//UART configuration. Set RX-Pin to -1 to disable UART
	int8_t uartRXPin;
	int8_t uartTXPin;
	int8_t uartCTSPin;
	int8_t uartRTSPin;
	//Default, can be overridden by boards
	uint32_t uartBaudRate : 32;

	//Receiver sensitivity of this device, set from board configs
	int8_t dBmRX;
	// This value should be calibrated at 1m distance, set by board configs
	int8_t calibratedTX;

	uint8_t lfClockSource;
	int8_t batteryAdcAin;
	int8_t batteryCheckDIO;

	//SPI Configuration
	int8_t spiM0SckPin; // Spi Clock Pin
	int8_t spiM0MosiPin; // Spi Master Out, Slave In Pin
	int8_t spiM0MisoPin; // Spi Master In, Slave Out Pin
	int8_t spiM0SSAccPin; // Slave Select Pin for Spi Device 1
	int8_t spiM0SSBmePin; // Slave Select Pin for Spi Device 2

	int8_t lis2dh12Interrupt1Pin; //Wake up interrupt pin for acceleromter

	uint8_t dcDcEnabled;
};
#pragma pack(pop)


enum TerminalMode {
	TERMINAL_JSON_MODE, //Interrupt based terminal input and blocking output
	TERMINAL_PROMPT_MODE, //blockin in and out with echo and backspace options
	TERMINAL_DISABLED //Terminal is disabled, no in and output
};


enum EnrollmentState { NOT_ENROLLED, ENROLLED };

//These codes are returned from the PreEnrollmentHandler
enum PreEnrollmentReturnCode {
	PRE_ENROLLMENT_DONE = 0, //PreEnrollment of the Module was either not necessary or successfully done
	PRE_ENROLLMENT_WAITING = 1, //PreEnrollment must do asynchronous work and will afterwards call the PreEnrollmentDispatcher
	PRE_ENROLLMENT_FAILED = 2 //PreEnrollment was not successfuly, so enrollment should continue
};

//Used to store fake node positions for modifying events
typedef struct FakeNodePositionRecordEntry{
	ble_gap_addr_t addr;
	u8 xM; //x position in metre
	u8 yM; //y position in metre
} FakeNodePositionRecordEntry;
typedef struct FakeNodePositionRecord {
	u8 count;
	FakeNodePositionRecordEntry entries[1];
} FakeNodePositionRecord;

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
#define BOOTLOADER_UICR_ADDRESS           (FLASH_REGION_START_ADDRESS + NRF_UICR->BOOTLOADERADDR)
#endif

//This is where the bootloader settings are saved
#if defined(NRF51) || defined(SIM_ENABLED)
#define REGION_BOOTLOADER_SETTINGS_START (FLASH_REGION_START_ADDRESS + 0x0003FC00) //Last page of flash
#elif defined(NRF52)
#define REGION_BOOTLOADER_SETTINGS_START (FLASH_REGION_START_ADDRESS + 0x0007F000) //Last page of flash
#endif

enum ImageType{
	IMAGE_TYPE_SOFTDEVICE = 0,
	IMAGE_TYPE_APP = 1,
	IMAGE_TYPE_APP_FORCED = 2,
	IMAGE_TYPE_SD_AND_APP = 3,
	IMAGE_TYPE_BOOTLOADER = 4,
	IMAGE_TYPE_COMPONENT = 5
};
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
	u32 chunksStored[BOOTLOADER_BITMASK_SIZE]; //bit sequence of how many CHUNKS have been stored successfully
	u32 pagesMoved[BOOTLOADER_BITMASK_SIZE]; //bit sequence of how many pages the bootloader has moved so far

	u32 moduleId; //Stores the moduleId that handles the update
	u32 componentId; //Stores the componentId (if many updateable devices are connected to a module)


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
	SCAN_STATE_OFF 		= 0,
	SCAN_STATE_LOW 		= 1,
	SCAN_STATE_MEDIUM 	= 2,
	SCAN_STATE_HIGH 	= 3,
	SCAN_STATE_CUSTOM 	= 4
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
	INVALID_STATE, DISCOVERY_HIGH, DISCOVERY_LOW, DISCOVERY_OFF};

//TODO: Move
//All known Subtypes of BaseConnection supported by the ConnectionManager
enum ConnectionTypes { CONNECTION_TYPE_INVALID, CONNECTION_TYPE_FRUITYMESH, CONNECTION_TYPE_APP, CONNECTION_TYPE_CLC_APP, CONNECTION_TYPE_RESOLVER, CONNECTION_TYPE_MESH_ACCESS };

//This enum defines packet authorization for MeshAccessConnetions
//First auth is undetermined, then rights decrease until the last entry, biggest entry num has preference always
enum MeshAccessAuthorization
{
	MA_AUTH_UNDETERMINED = 0, //Packet was not checked by any module
	MA_AUTH_WHITELIST = 1, //Packet was whitelisted by a module
	MA_AUTH_LOCAL_ONLY = 2, //Packet must only be processed by the receiving node and not by the mesh
	MA_AUTH_BLACKLIST = 3, //Packet was blacklisted by a module (This always wins over whitelisted)
};

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
#include <malloc.h>
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

//This struct represents the registers as dumped on the stack
//by ARM Cortex Hardware once a hardfault occurs
#pragma pack(push, 4)
typedef struct
{
        uint32_t r0;
        uint32_t r1;
        uint32_t r2;
        uint32_t r3;
        uint32_t r12;
        uint32_t lr;
        uint32_t pc;
        uint32_t psr;
} stacked_regs_t;
#pragma pack(pop)

/*############ HELPFUL MACROS ################*/
#define PAGE_SIZE NRF_FICR->CODEPAGESIZE
#define FLASH_SIZE (NRF_FICR->CODESIZE*NRF_FICR->CODEPAGESIZE)

#define TO_ADDR(page) ((u32*)(page*PAGE_SIZE + FLASH_REGION_START_ADDRESS))
#define TO_PAGE(addr) (((((u32)(addr)) - FLASH_REGION_START_ADDRESS)/PAGE_SIZE))
#define TO_PAGES_CEIL(size) ((size + PAGE_SIZE - 1) / PAGE_SIZE) //Calculates the number of pages and rounds up

//Returns true if the timer should have trigered the interval in the passedTime
#define SHOULD_IV_TRIGGER(timer, passedTime, interval) (interval != 0 && (((timer)-(passedTime)) % (interval) >= (timer) % (interval)))

//Returns true if the button action should execute
#define SHOULD_BUTTON_EVT_EXEC(BUTTON_DS) (BUTTON_DS != 0 && holdTimeDs > BUTTON_DS && holdTimeDs < (u32)(BUTTON_DS + 20))
//Converts Seconds to Deciseconds
#define SEC_TO_DS(sec) (((u32)(sec))*10)
//Checks if packet contains the variable
#define CHECK_MSG_SIZE(packetHeader, variable, size, dataLength) ((((u8*)variable) + (size)) - ((u8*)packetHeader) <= (dataLength))
//Macros for concatenating
#define CONCAT(x,y) x##y
#define XCONCAT(x,y) CONCAT(x,y)

#ifndef SIM_ENABLED
#define SIMSTATCOUNT(cmd) do{}while(0)
#define SIMSTATAVG(key, value) do{}while(0)
#define SIMERROR() do{}while(0)
#endif

/*############ Include packet definitions ################*/
#include <adv_packets.h>
#include <conn_packets.h>


