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
 * This file contains the mesh configuration, which is a singleton. Some of the
 * values can be changed at runtime to alter the meshing behaviour.
 */

#pragma once

#include <types.h>
#include <LedWrapper.h>

extern "C" {
#include <ble_gap.h>
#include <app_util.h>
}

#define FM_VERSION_MAJOR 0 //0-400
#define FM_VERSION_MINOR 3 //0-999
#define FM_VERSION_PATCH 1 //0-9999
#define FM_VERSION (10000000 * FM_VERSION_MAJOR + 10000 * FM_VERSION_MINOR + FM_VERSION_PATCH);

extern LedWrapper* LedRed;
extern LedWrapper* LedGreen;
extern LedWrapper* LedBlue;

#define MESH_IN_CONNECTIONS 1
#define MESH_OUT_CONNECTIONS 3


extern u32 __application_start_address[]; //Variable is set in the linker script
extern u32 __application_end_address[]; //Variable is set in the linker script

extern u32 __application_ram_start_address[]; //Variable is set in the linker script

typedef enum {
	PCA_10031	 		= 0x000, //nRF51 Dongle
	PCA_10028	 		= 0x001, //nRF51-DK
	ARS_100748		 	= 0x002, //ARSv1
	PCA_10036	 		= 0x003, //nRF52-Preview-DK
	PCA_10040	 		= 0x004, //nRF52-DK
} boardTypes;

//Alright, I know this is bad, but it's for readability....
//And static classes do need a seperate declaration and definition...
#define Config Conf::getInstance()

//This class holds the configuration and some bits are changeable at runtime
class Conf
{
	private:
		Conf(){};
		static Conf* instance;
		static void generateRandomSerial();
		static bool isEmpty(u32* mem, u8 numWords);

	public:

		static Conf* getInstance();

		// ########### DEBUGGING ################################################

		//Do not load the persistent node configuration
		//Beware: Persistent config must be updated with every connection loss or random shoudl be used....?
		bool ignorePersistentNodeConfigurationOnBoot = false;

		//Do not use any persistently saved module data
		bool ignorePersistentModuleConfigurationOnBoot = false;

		//This variable can be toggled via the Terminal "BREAK" and can be used
		//to toggle conditional breakpoints because the softdevice does not allow
		//runtime breakpoints while in a connection or stepping
		bool breakpointToggleActive = false;

		//If in debug mode, the node will run in endless loops when errors occur
		bool debugMode = true;

		//Instruct the Advertising Module to advertise Debug Packets
		bool advertiseDebugPackets = false;

		ledMode defaultLedMode = ledMode::LED_MODE_CONNECTIONS;


		// ########### TIMINGS ################################################

		//Main timer tick interval
		u32 mainTimerTickMs = 200;

		//Mesh connection parameters (used when a connection is set up)
		u16 meshMinConnectionInterval = MSEC_TO_UNITS(10, UNIT_1_25_MS);   	//(7.5-4000) Minimum acceptable connection interval
		u16 meshMaxConnectionInterval = MSEC_TO_UNITS(10, UNIT_1_25_MS);   	//(7.5-4000) Maximum acceptable connection interval
		u16 meshPeripheralSlaveLatency = 0;                  					//(0-...) Slave latency in number of connection events
		u16 meshConnectionSupervisionTimeout = MSEC_TO_UNITS(6000, UNIT_10_MS);   	//(100-32000) Connection supervisory timeout
		u16 meshExtendedConnectionTimeout = 10000;	//(0 - 65000) Extended timeout which is used to reconnect a known connection upon connection timeout

		//Mesh discovery parameters
		//DISCOVERY_HIGH
		u16 meshAdvertisingIntervalHigh = MSEC_TO_UNITS(100, UNIT_0_625_MS);	//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
		u16 meshScanIntervalHigh = MSEC_TO_UNITS(20, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
		u16 meshScanWindowHigh = MSEC_TO_UNITS(4, UNIT_0_625_MS);	//(2.5-1024) Determines scan window in units of 0.625 millisecond.


		//DISCOVERY_LOW
		u16 meshAdvertisingIntervalLow = MSEC_TO_UNITS(5000, UNIT_0_625_MS);	//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
		u16 meshScanIntervalLow = MSEC_TO_UNITS(1000, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
		u16 meshScanWindowLow = MSEC_TO_UNITS(50, UNIT_0_625_MS);	//(2.5-1024) Determines scan window in units of 0.625 millisecond.


		//INITIATING
		u16 meshConnectingScanInterval = MSEC_TO_UNITS(20, UNIT_0_625_MS); //(20-1024) in 0.625ms units
		u16 meshConnectingScanWindow = MSEC_TO_UNITS(4, UNIT_0_625_MS); //(2.5-1024) in 0.625ms units
		u16 meshConnectingScanTimeout = 1; //(0-...) in seconds

		//HANDSHAKE
		u16 meshHandshakeTimeout = 10; //If the handshake has not finished after this time, the connection will be disconnected


		//STATE timeouts
		u16 meshStateTimeoutHigh = 1 * 1000; //Timeout of the High discovery state before deciding to which partner to connect
		u16 meshStateTimeoutLow = 10 * 1000; //Timeout of the Low discovery state before deciding to which partner to connect
		u16 meshStateTimeoutBackOff = 0 * 1000; //Timeout until the back_off state will return to discovery
		u16 meshStateTimeoutBackOffVariance = 1 * 1000;  //Up to ... ms will be added randomly to the back off state timeout

		u16 discoveryHighToLowTransitionDuration = 10; // When discovery returns # times without results, the node will switch to low discovery

		/*
		 * If both conn_sup_timeout and max_conn_interval are specified, then the following constraint applies:
		 * conn_sup_timeout * 4 > (1 + slave_latency) * max_conn_interval that corresponds to the following
		 * BT Spec 4.1 Vol 2 Part E, Section 7.8.12 requirement: The Supervision_Timeout in milliseconds shall be
		 * larger than (1 + Conn_Latency) * Conn_Interval_Max * 2, where Conn_Interval_Max is given in milliseconds.
		 * https://devzone.nordicsemi.com/question/60/what-is-connection-parameters/
		 * */

		// ########### ADVERTISING ################################################
		u8 advertiseOnChannel37 = 1;
		u8 advertiseOnChannel38 = 0;
		u8 advertiseOnChannel39 = 0;

		// ########### CONNECTION ################################################

		const u8 meshMaxInConnections = MESH_IN_CONNECTIONS; // Will probably never change and code will not allow this to change without modifications
		const u8 meshMaxOutConnections = MESH_OUT_CONNECTIONS; //Configurable from 1-7
		const u8 meshMaxConnections = meshMaxInConnections + meshMaxOutConnections; //for convenience

		const bool enableRadioNotificationHandler = false;

		const bool enableConnectionRSSIMeasurement = true;


		// ########### BOARD_SPECIFICS ################################################
		//Default board is pca10031, modify SET_BOARD if different board is required
		//Or flash config data to UICR
		u8 Led1Pin;
		u8 Led2Pin;
		u8 Led3Pin;
		bool LedActiveHigh; //Defines if writing 0 or 1 to an LED turns it on

		u8 uartRXPin;
		u8 uartTXPin;
		u8 uartCTSPin;
		u8 uartRTSPin;
		bool uartFlowControl;

		i8 calibratedTX = -63; // This value should be calibrated at 1m distance

		// ########### VALUES from UICR / TEST_DEVICE (initialized in Config.cpp) #############
		enum deviceConfigOrigins{ RANDOM_CONFIG, UICR_CONFIG, TESTDEVICE_CONFIG };
		u8 deviceConfigOrigin = RANDOM_CONFIG;

		//Set a default boardId for NRF51 and NRF52 in case no other data is available
#if defined(NRF51)
		u32 boardType = PCA_10031;
#elif defined(NRF52)
		u32 boardType = PCA_10040;
#else
#error "Specify SoC model (NR51 or NRF52)"
#endif

		char serialNumber[6];;
		u16 manufacturerId = 0; //According to the BLE company identifiers: https://www.bluetooth.org/en-us/specification/assigned-numbers/company-identifiers

		//When enabling encryption, the mesh handle can only be read through an encrypted connection
		//And connections will perform an encryption before the handshake
		const bool encryptionEnabled = false;
		u8 meshNetworkKey[16] = {1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6}; //16 byte Long term key in little endian format
		//06050403020100090807060504030201 => How to enter it in the MCP
		//01:02:03:04:05:06:07:08:09:00:01:02:03:04:05:06 => Format for TI Sniffer

		//Allows a number of mesh networks to coexist in the same physical space without collision
		//Allowed range is 0x0000 - 0xFF00 (0 - 65280), others are reserved for special purpose
		networkID meshNetworkIdentifier = 1;
		nodeID defaultNodeId = 0;
		deviceTypes deviceType = deviceTypes::DEVICE_TYPE_STATIC;

		ble_gap_addr_t staticAccessAddress = {0xFF, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};


		// ########### TEST DEVICES ################################################
		//For our test devices
		typedef struct{
			u32 chipID;
			nodeID id;
			deviceTypes deviceType;
			ble_gap_addr_t addr;
		} testDevice;

		#define NUM_TEST_DEVICES 10
		static testDevice testDevices[];

		#define NUM_TEST_COLOUR_IDS 12
		static nodeID testColourIDs[];

		Conf::testDevice* getTestDevice();


		// ########### OTHER ################################################
		u16 firmwareVersionMajor = FM_VERSION_MAJOR; //0-400
		u16 firmwareVersionMinor = FM_VERSION_MINOR; //0-999
		u16 firmwareVersionPatch = FM_VERSION_PATCH; //0-9999
		u32 firmwareVersion = FM_VERSION;

		i8 radioTransmitPower = 0; //The power at which the radio transmits advertisings and data packets
};


// ########### COMPILE TIME SETTINGS ##########################################

//Selecting the board can be done at runtime
#include <board_pca10031.h>
#include <board_ars100748.h>
#include <board_pca10036.h>

#define SET_BOARD() do{					\
		SET_PCA10031_BOARD_IF_FIT(Config->boardType);		\
		SET_PCA10036_BOARD_IF_FIT(Config->boardType);		\
		SET_ARS100748_BOARD_IF_FIT(Config->boardType);	\
}while(0);

//Each of the Connections has a buffer for outgoing packets, this is its size in bytes
#define PACKET_SEND_BUFFER_SIZE 600

//Each connection does also have a buffer to assemble packets that were split into 20 byte chunks
#define PACKET_REASSEMBLY_BUFFER_SIZE 200

//Maximum length that can be read
#define TERMINAL_READ_BUFFER
//Size for tracing messages to UART, if it is too short, messages will get truncated
#define TRACE_BUFFER_SIZE 500

//If this is defined, message logging is done via segger RTT instead of UART
//In J-Link RTT view, set line ending to CR and send input on enter, echo input to off
//#define USE_SEGGER_RTT_INSTEAD_OF_UART

//Number of supported Modules
#define MAX_MODULE_COUNT 10

//Defines the maximum size of the mesh write attribute. This space is required in the ATTR table
#define MESH_CHARACTERISTIC_MAX_LENGTH 100

//Size of Attribute table can be set lower than the default if we do not need that much
#define ATTR_TABLE_MAX_SIZE 0x200

//Identifiers
#define COMPANY_IDENTIFIER 0x024D // Company identifier for manufacturer specific data header (M-Way Solutions GmbH) - Should not be changed to ensure compatibility with the mesh protocol
#define MESH_IDENTIFIER 0xF0 //Identifier that defines this as the fruitymesh protocol

//GAP device name
#define DEVICE_NAME "FRUITY"

//Serial should be short but unique for the given manufacturer id
#define MANUFACTURER_ID 0xFFFF //The manufacturer id should match your company identifier that should be registered with the bluetooth sig: https://www.bluetooth.org/en-us/specification/assigned-numbers/company-identifiers
#define SERIAL_NUMBER_LENGTH 5 //A serial could use the alphabet "BCDFGHJKLMNPQRSTVWXYZ123456789". This is good for readability (short, no inter-digit resemblance, 25 million possible combinations, no funny words)

//Storage
#define STORAGE_BLOCK_SIZE 128 //Determines the maximum size for a module configuration
#define STORAGE_BLOCK_NUMBER 8 //Determines the number of blocks that are available

//Terminal
#define TERMINAL_PROMPT_MODE_ON_BOOT true

/*############ LOGGER ################*/

#define EOL "\r\n"
#define SEP "\r\n"

//If undefined, the final build will have no logging / Terminal functionality built in
#define ENABLE_LOGGING
#define ENABLE_TERMINAL
#define ENABLE_UART


/*############ SERVICES ################*/
//Fruity Mesh Service UUID 310bfe40-ed6b-11e3-a1be-0002a5d5c51b
#define MESH_SERVICE_BASE_UUID128 0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00
#define MESH_SERVICE_UUID 0x1523
#define MESH_SERVICE_CHARACTERISTIC_UUID 0x1524
#define MESH_SERVICE_INITIAL_CHARACTERISTIC_VALUE {1,2,3}
#define MESH_SERVICE_CHARACTERISTIC_DESCRIPTOR_UUID  0x1525


/*############ MODULES ################*/
//The module ids are used to identify a module over the network
//Numbers below 150 are standard defined, numbers obove this range are free to use for custom modules
enum moduleID{
	//Standard modules
	NODE=0, // Not a module per se, but why not let it send module messages
	ADVERTISING_MODULE_ID=1,
	SCANNING_MODULE_ID=2,
	STATUS_REPORTER_MODULE_ID=3,
	DFU_MODULE_ID=4,
	ENROLLMENT_MODULE_ID=5,
	IO_MODULE_ID=6,
	DEBUG_MODULE_ID=7,

	//Custom modules
	MY_CUSTOM_MODULE_ID=150,

	//Invalid Module: 0xFF is the flash memory default and is therefore invalid
	INVALID_MODULE=255
};

//Activate and deactivate modules by un-/commenting these defines
#define ACTIVATE_ADVERTISING_MODULE
//#define ACTIVATE_SCANNING_MODULE
#define ACTIVATE_STATUS_REPORTER_MODULE
//#define ACTIVATE_DFU_MODULE
#define ACTIVATE_ENROLLMENT_MODULE
#define ACTIVATE_IO_MODULE
#define ACTIVATE_DEBUG_MODULE

/*############ Stuff for DFU ################*/
//This is where the bootloader settings are saved
#define REGION_BOOTLOADER_SETTINGS_START 0x0003FC00

//Maximum size that a DFU chunk can be
#define DFU_DATA_BUFFER_SIZE 64
