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

extern "C" {

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <string.h>
#include "ble_db_discovery.h"

#if defined(NRF51) || defined(SIM_ENABLED)
#include <ble_stack_handler_types.h>
#endif
}

 //Unsigned ints
typedef uint8_t u8;
typedef uint16_t u16;
typedef unsigned u32;   //This is not defined uint32_t because GCC defines uint32_t as unsigned long, 
						//which is a problem when working with printf placeholders.

//Signed ints
typedef int8_t i8;
typedef int16_t i16;
typedef int i32;		//This is not defined int32_t because GCC defines int32_t as long,
						//which is a problem when working with printf placeholders.

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

#ifdef IAR
#include "vector"
#endif


static_assert(sizeof(u8 ) == 1, "");
static_assert(sizeof(u16) == 2, "");
static_assert(sizeof(u32) == 4, "");

static_assert(sizeof(i8 ) == 1, "");
static_assert(sizeof(i16) == 2, "");
static_assert(sizeof(i32) == 4, "");


//Data types for the mesh
typedef u16 NetworkId;
typedef u16 NodeId;
typedef u32 ClusterId;
typedef i16 ClusterSize;

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

//Different types of supported BLE stacks, specific versions can be added later if necessary
enum class BleStackType {
	INVALID = 0,
	NRF_SD_130_ANY = 100,
	NRF_SD_132_ANY = 200,
	NRF_SD_140_ANY = 300
};

/*## Available Node ids #############################################################*/
// Refer to protocol specification @https://github.com/mwaylabs/fruitymesh/wiki/Protocol-Specification

constexpr NodeId NODE_ID_BROADCAST               =     0; //A broadcast will be received by all nodes within one mesh
constexpr NodeId NODE_ID_DEVICE_BASE             =     1; //Beginning from 1, we can assign nodeIds to individual devices
constexpr NodeId NODE_ID_DEVICE_BASE_SIZE        =  1999;

constexpr NodeId NODE_ID_VIRTUAL_BASE            =  2000; //Used to assign sub-addresses to connections that do not belong to the mesh but want to perform mesh activity. Used as a multiplier.
constexpr NodeId NODE_ID_GROUP_BASE              = 20000; //Used to assign group ids to nodes. A node can take part in many groups at once
constexpr NodeId NODE_ID_GROUP_BASE_SIZE         = 10000;

constexpr NodeId NODE_ID_LOCAL_LOOPBACK          = 30000; //30000 is like a local loopback address that will only send to the current node,
constexpr NodeId NODE_ID_HOPS_BASE               = 30000; //30001 sends to the local node and one hop further, 30002 two hops
constexpr NodeId NODE_ID_HOPS_BASE_SIZE          =  1000;

constexpr NodeId NODE_ID_SHORTEST_SINK           = 31000;
constexpr NodeId NODE_ID_ANYCAST_THEN_BROADCAST  = 31001; //31001 will send the message to any one of the connected nodes and only that node will then broadcast this message

constexpr NodeId NODE_ID_APP_BASE                = 32000; //Custom GATT services, connections with Smartphones, should use (APP_BASE + moduleId)
constexpr NodeId NODE_ID_APP_BASE_SIZE           =  1000;

constexpr NodeId NODE_ID_GLOBAL_DEVICE_BASE      = 33000; //Can be used to assign nodeIds that are valid organization wide (e.g. for assets)
constexpr NodeId NODE_ID_GLOBAL_DEVICE_BASE_SIZE =  7000;

constexpr NodeId NODE_ID_CLC_SPECIFIC            = 40000; //see usage in CLC module
constexpr NodeId NODE_ID_RESERVED                = 40001; //Yet unassigned nodIds
constexpr NodeId NODE_ID_INVALID                 = 0xFFFF; //Special node id that is used in error cases. It must never be used as an sender or receiver.

// The following static groupIds are predefined and are part of the nodeId range
#define GROUP_ID_STATIC_BASE 20000

// Chipset group ids. These define what kind of chipset the firmware is running on
enum class Chipset : NodeId
{
	CHIP_INVALID  = 0,
	CHIP_NRF51    = 20000,
	CHIP_NRF52    = 20001,
	CHIP_NRF52840 = 20015,
};

// Featureset group ids, these are used to determine if a firmware can be updated over the
// air with a given firmware. For an update, the firmware group ids must match.
enum class FeatureSetGroup : NodeId
{
	INVALID                                          = 0,
	//These comments are used to parse values with FruityDeploy.
	/*FruityDeploy-FeatureSetGroup*/NRF51_SINK       = 20002,
	/*FruityDeploy-FeatureSetGroup*/NRF51_MESH       = 20003,
	/*FruityDeploy-FeatureSetGroup*/NRF52_MESH       = 20004,
	/*FruityDeploy-FeatureSetGroup*/NRF51_ASSET      = 20005,
	/*FruityDeploy-FeatureSetGroup*/NRF52_ASSET      = 20006,
	/*FruityDeploy-FeatureSetGroup*/NRF52_CLC_MESH   = 20007,
	/*FruityDeploy-FeatureSetGroup*/NRF51_CLC_SINK   = 20008,
	/*FruityDeploy-FeatureSetGroup*/NRF52_VS_MESH    = 20009,
	/*FruityDeploy-FeatureSetGroup*/NRF52_VS_SINK    = 20010,
	/*FruityDeploy-FeatureSetGroup*/NRF52_SINK       = 20011,
	/*FruityDeploy-FeatureSetGroup*/NRF51_EINK       = 20012,
	/*FruityDeploy-FeatureSetGroup*/NRF52840_WM_MESH = 20013,
	/*FruityDeploy-FeatureSetGroup*/NRF52_PC_BRIDGE  = 20014,
	//                              CHIP_NRF52840    = 20015,
};

// This range of groupIds can be given dynamically
#define GROUP_ID_DYNAMIC_BASE 21000
#define GROUP_ID_DYNAMIC_BASE_SIZE 9000

//Sets the maximum number of firmware group ids that can be compiled into the firmware
#define MAX_NUM_FW_GROUP_IDS 2

/*## Key Types #############################################################*/

//Types of keys used by the mesh and other modules
enum class FmKeyId : u32
{
	ZERO               =  0,
	NODE               =  1,
	NETWORK            =  2,
	BASE_USER          =  3,
	ORGANIZATION       =  4,
	RESTRAINED         =  5,
	USER_DERIVED_START = 10,
	USER_DERIVED_END   = (UINT32_MAX / 2),
};


/*## RecordIds #############################################################*/
// The modules use their moduleId as a recordId, records outside this range can be used
// for other types of storage

//Specific Record Ids
#define RECORD_STORAGE_RECORD_ID_UPDATE_STATUS 1000 //Stores the done status of an update
#define RECORD_STORAGE_RECORD_ID_UICR_REPLACEMENT 1001 //Can be used, if UICR can not be flashed, e.g. when updating another beacon with different firmware
#define RECORD_STORAGE_RECORD_ID_DEPRECATED 1002 //Was used to store fake positions for nodes to modify the incoming events


/*## Modules #############################################################*/
//The module ids are used to identify a module over the network
//Numbers below 150 are standard defined, numbers obove this range are free to use for custom modules

enum class ModuleId : u8{
	//Standard modules
	NODE=0, // Not a module per se, but why not let it send module messages
	ADVERTISING_MODULE=1,
	SCANNING_MODULE=2,
	STATUS_REPORTER_MODULE=3,
	DFU_MODULE=4,
	ENROLLMENT_MODULE=5,
	IO_MODULE=6,
	DEBUG_MODULE=7,
	CONFIG=8,
	BOARD_CONFIG=9,
	MESH_ACCESS_MODULE=10,
	//MANAGEMENT_MODULE=11, //deprecated as of 22.05.2019
	TESTING_MODULE=12,
	BULK_MODULE=13,

	//M-way Modules
	CLC_MODULE=150,
	VS_MODULE=151,
	ENOCEAN_MODULE=152,
	ASSET_MODULE=153,
	EINK_MODULE=154,
	WM_MODULE=155,

	//Other Modules
	MY_CUSTOM_MODULE=200,
	PING_MODULE=201,
	TEMPLATE_MODULE=202,

	//Invalid Module: 0xFF is the flash memory default and is therefore invalid
	INVALID_MODULE=255
};

// The reason why the device was rebooted
enum class RebootReason : u8 {
	UNKNOWN                          = 0,
	HARDFAULT                        = 1,
	APP_FAULT                        = 2,
	SD_FAULT                         = 3,
	PIN_RESET                        = 4,
	WATCHDOG                         = 5,
	FROM_OFF_STATE                   = 6,
	LOCAL_RESET                      = 7,
	REMOTE_RESET                     = 8,
	ENROLLMENT                       = 9,
	PREFERRED_CONNECTIONS            = 10,
	DFU                              = 11,
	MODULE_ALLOCATOR_OUT_OF_MEMORY   = 12,
	MEMORY_MANAGEMENT                = 13,
	BUS_FAULT                        = 14,
	USAGE_FAULT                      = 15,
	ENROLLMENT_REMOVE                = 16,
	FACTORY_RESET_FAILED             = 17,
	FACTORY_RESET_SUCCEEDED_FAILSAFE = 18,
	SET_SERIAL_SUCCESS               = 19,
	SET_SERIAL_FAILED                = 20,

	USER_DEFINED_START               = 200,
	USER_DEFINED_END                 = 255,
};

/*############ Live Report types ################*/
//Live reports are sent through the mesh as soon as something notable happens
//Could be some info, a warning or an error

enum class LiveReportTypes : u8 {
	LEVEL_ERROR = 0,
	LEVEL_WARN = 50,
	HANDSHAKED_MESH_DISCONNECTED = 51, //extra is partnerid, extra2 is appDisconnectReason
	WARN_GAP_DISCONNECTED = 52, //extra is partnerid, extra2 is hci code

	//########
	LEVEL_INFO = 100,
	GAP_CONNECTED_INCOMING = 101, //extra is connHandle, extra2 is 4 bytes of gap addr
	GAP_TRYING_AS_MASTER = 102, //extra is partnerId, extra2 is 4 bytes of gap addr
	GAP_CONNECTED_OUTGOING = 103, //extra is connHandle, extra2 is 4 byte of gap addr
	//Deprecated: GAP_DISCONNECTED = 104,

	HANDSHAKE_FAIL = 105, //extra is tempPartnerId, extra2 is handshakeFailCode
	MESH_CONNECTED = 106, //extra is partnerid, extra2 is asWinner
	//Deprecated: MESH_DISCONNECTED = 107,

	//########
	LEVEL_DEBUG = 150,
	DECISION_RESULT = 151 //extra is decision type, extra2 is preferredPartner
};

enum class LiveReportHandshakeFailCode : u8
{
	SUCCESS,
	SAME_CLUSTERID,
	NETWORK_ID_MISMATCH,
	WRONG_DIRECTION,
	UNPREFERRED_CONNECTION
};

/*## Types and enums #############################################################*/
#define STATIC_ASSERT_SIZE(T, size) static_assert(sizeof(T) == (size), "STATIC_ASSERT_SIZE failed!")


//A struct that combines a data pointer and the accompanying length
struct SizedData {
    u8*		data; //Pointer to data
    u16		length; //Length of Data
};

template<typename T>
struct TwoDimStruct
{
	T x;
	T y;
};

template<typename T>
struct ThreeDimStruct
{
	T x;
	T y;
	T z;
};

// To determine from which location the node config was loaded
enum class DeviceConfigOrigins : u8 {
	RANDOM_CONFIG = 0,
	UICR_CONFIG = 1,
	TESTDEVICE_CONFIG = 2
};

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

// The different kind of nodes supported by FruityMesh
enum class DeviceType : u8{
	INVALID = 0,
	STATIC  = 1, // A normal node that remains static at one position
	ROAMING = 2, // A node that is moving constantly or often (not implemented)
	SINK    = 3, // A static node that wants to acquire data, e.g. a MeshGateway
	ASSET   = 4, // A roaming node that is sporadically or never connected but broadcasts data
	LEAF    = 5  // A node that will never act as a slave but will only connect as a master (useful for roaming nodes, but no relaying possible)
};

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

// The different terminal modes
enum class TerminalMode : u8{
	JSON = 0, //Interrupt based terminal input and blocking output
	PROMPT = 1, //blockin in and out with echo and backspace options
	DISABLED = 2 //Terminal is disabled, no in and output
};

//Enrollment states
enum class EnrollmentState : u8{
	NOT_ENROLLED = 0,
	ENROLLED = 1
};

//These codes are returned from the PreEnrollmentHandler
enum class PreEnrollmentReturnCode : u8{
	DONE = 0, //PreEnrollment of the Module was either not necessary or successfully done
	WAITING = 1, //PreEnrollment must do asynchronous work and will afterwards call the PreEnrollmentDispatcher
	FAILED = 2 //PreEnrollment was not successfuly, so enrollment should continue
};

//Used for intercepting messages befoure they are routed through the mesh
typedef u32 RoutingDecision;
#define ROUTING_DECISION_BLOCK_TO_MESH 0x1
#define ROUTING_DECISION_BLOCK_TO_MESH_ACCESS 0x2

// Used to represent the button states
enum class ButtonState : u8
{
	INITAL = 0,  // no-change
	PRESSED = 1, // button was pressed
	RELEASED = 2 // button was released
};

/*## BoardConfiguration #############################################################*/
// The BoardConfiguration must contain the correct settings for the board that the firmware
// is flashed on. The featureset must contain all board configurations that the featureset
// wants to run on.

#pragma pack(push)
#pragma pack(1)
struct BoardConfiguration : ModuleConfiguration {
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
	uint8_t lfClockAccuracy;

	int8_t batteryAdcInputPin;
	int8_t batteryMeasurementEnablePin;

	//SPI Configuration
	int8_t spiM0SckPin; // Spi Clock Pin
	int8_t spiM0MosiPin; // Spi Master Out, Slave In Pin
	int8_t spiM0MisoPin; // Spi Master In, Slave Out Pin
	int8_t spiM0SSAccPin; // Slave Select Pin for Spi Device 1
	int8_t twiM1SDAAccPin; // Acc SDA pin, if interfaced on i2c
	int8_t twiM1SCLAccPin; // Acc SCL pin, if interfaced on i2c
	int8_t spiM0SSBmePin; // Slave Select Pin for Spi Device 2
	int8_t sensorPowerEnablePin; //Enable Sensors on I2C bus
	int8_t lis2dh12Interrupt1Pin; //Wake up interrupt pin for acceleromter

	int8_t twiM1SCLPin;
	int8_t twiM1SDAPin;

	uint32_t voltageDividerR1;
	uint32_t voltageDividerR2;
	uint8_t dcDcEnabled;
};
#pragma pack(pop)

//Used to store fake node positions for modifying events
struct FakeNodePositionRecordEntry{
	ble_gap_addr_t addr;
	u8 xM; //x position in metre
	u8 yM; //y position in metre
};
struct FakeNodePositionRecord {
	u8 count;
	FakeNodePositionRecordEntry entries[1];
};

//Defines the different scanning intervals for each state
enum class ScanState : u8{
	LOW     = 0,
	HIGH    = 1,
	CUSTOM  = 2,
};

// Radio notification irq priority
#if defined(NRF51)
#define RADIO_NOTIFICATION_IRQ_PRIORITY      3
#elif defined(NRF52)
#define RADIO_NOTIFICATION_IRQ_PRIORITY      6
#endif

// Mesh discovery states
enum class DiscoveryState : u8{
	INVALID = 0,
	HIGH  = 1, // Scanning and advertising at a high duty cycle
	LOW   = 2, // Scanning and advertising at a low duty cycle
	OFF   = 3, // Scanning and advertising not enabled by the node to save power (Other modules might still advertise or scan)
};

//All known Subtypes of BaseConnection supported by the ConnectionManager
enum class ConnectionType : u8{
	INVALID     = 0,
	FRUITYMESH  = 1, // A mesh connection
	APP         = 2, // Base class of a customer specific connection (deprecated)
	CLC_APP     = 3,
	RESOLVER    = 4, // Resolver connection used to determine the correct connection
	MESH_ACCESS = 5, // MeshAccessConnection
};

//This enum defines packet authorization for MeshAccessConnetions
//First auth is undetermined, then rights decrease until the last entry, biggest entry num has preference always
enum class MeshAccessAuthorization : u8{
	UNDETERMINED = 0, //Packet was not checked by any module
	WHITELIST    = 1, //Packet was whitelisted by a module
	LOCAL_ONLY   = 2, //Packet must only be processed by the receiving node and not by the mesh
	BLACKLIST    = 3, //Packet was blacklisted by a module (This always wins over whitelisted)
};

//Led mode that defines what the LED does (mainly for debugging)
enum class LedMode : u8{
	OFF         = 0, // Led is off
	ON          = 1, // Led is constantly on
	CONNECTIONS = 2, // Led blinks red if not connected and green for the number of connections
	RADIO       = 3, // Led shows radio activity
	CLUSTERING  = 4, // Led colour chosen according to clusterId (deprecated)
	ASSET       = 5,
	CUSTOM      = 6, // Led controlled by a specific module
};

//DFU ERROR CODES
enum class DfuStartDfuResponseCode : u8
{
	OK = 0,
	SAME_VERSION = 1,
	RUNNING_NEWER_VERSION = 2,
	ALREADY_IN_PROGRESS = 3,
	NO_BOOTLOADER = 4,
	FLASH_BUSY = 5,
	NOT_ENOUGH_SPACE = 6,
	CHUNKS_TOO_BIG = 7,
	MODULE_NOT_AVAILABLE = 8,
	MODULE_NOT_UPDATABLE = 9,
	COMPONENT_NOT_UPDATEABLE = 10,
	MODULE_QUERY_WAITING = 11, //Special code that is used internally if a module queries another controller and continues the process later
	TOO_MANY_CHUNKS = 12,
};

enum class ErrorType : u32
{
	SUCCESS                     = 0,  ///< Successful command
	SVC_HANDLER_MISSING         = 1,  ///< SVC handler is missing
	BLE_STACK_NOT_ENABLED       = 2,  ///< Ble stack has not been enabled
	INTERNAL                    = 3,  ///< Internal Error
	NO_MEM                      = 4,  ///< No Memory for operation
	NOT_FOUND                   = 5,  ///< Not found
	NOT_SUPPORTED               = 6,  ///< Not supported
	INVALID_PARAM               = 7,  ///< Invalid Parameter
	INVALID_STATE               = 8,  ///< Invalid state, operation disallowed in this state
	INVALID_LENGTH              = 9,  ///< Invalid Length
	INVALID_FLAGS               = 10, ///< Invalid Flags
	INVALID_DATA                = 11, ///< Invalid Data
	DATA_SIZE                   = 12, ///< Data size exceeds limit
	TIMEOUT                     = 13, ///< Operation timed out
	NULL_ERROR                  = 14, ///< Null Pointer
	FORBIDDEN                   = 15, ///< Forbidden Operation
	INVALID_ADDR                = 16, ///< Bad Memory Address
	BUSY                        = 17, ///< Busy
	CONN_COUNT                  = 18, ///< Connection Count exceeded
};

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
#define BOOTLOADER_UICR_ADDRESS           (NRF_UICR->BOOTLOADERADDR)
#define FLASH_REGION_START_ADDRESS			0x00000000
#elif defined(NRF52) || defined(NRF52840)
#define BOOTLOADER_UICR_ADDRESS           (NRF_UICR->NRFFW[0])
#define FLASH_REGION_START_ADDRESS			0x00000000UL
#elif defined(SIM_ENABLED)
#define BOOTLOADER_UICR_ADDRESS           (FLASH_REGION_START_ADDRESS + NRF_UICR->BOOTLOADERADDR)
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
#define PACK_AND_ALIGN_4 __attribute__((aligned(4)))

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

//This struct represents the registers as dumped on the stack
//by ARM Cortex Hardware once a hardfault occurs
#pragma pack(push, 4)
struct stacked_regs_t
{
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
	uint32_t r12;
	uint32_t lr;
	uint32_t pc;
	uint32_t psr;
};
#pragma pack(pop)

/*############ HELPFUL MACROS ################*/
#define PAGE_SIZE NRF_FICR->CODEPAGESIZE
#define FLASH_SIZE (NRF_FICR->CODESIZE*NRF_FICR->CODEPAGESIZE)

#define TO_ADDR(page) ((u32*)(page*PAGE_SIZE + FLASH_REGION_START_ADDRESS))
#define TO_PAGE(addr) (u32)(((((u32)(addr)) - FLASH_REGION_START_ADDRESS)/PAGE_SIZE))
#define TO_PAGES_CEIL(size) ((size + PAGE_SIZE - 1) / PAGE_SIZE) //Calculates the number of pages and rounds up

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

//Replacement for old TO_HEX macro. Converts pointer to HEX string array.
#define PRINT_DEBUG(data, dataSize) DYNAMIC_ARRAY(data##Hex, (dataSize)*3+1); Logger::convertBufferToHexString(data, (dataSize), (char*)data##Hex, (dataSize)*3+1)

/*############ Include packet definitions ################*/
#include <adv_packets.h>
#include <conn_packets.h>
