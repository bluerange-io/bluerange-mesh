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
 * This file contains the mesh configuration, which is a singleton. Some of the
 * values can be changed at runtime to alter the meshing behaviour.
 */

#pragma once

#include <types.h>
#include <LedWrapper.h>
#include <GlobalState.h>

class RecordStorageEventListener;

extern "C" {
#include <ble_gap.h>
#include <app_util.h>
#include <nrf_sdm.h>
#ifndef SIM_ENABLED
#include <nrf_uart.h>
#endif
}

//major (0-400), minor (0-999), patch (0-9999)
#define FM_VERSION_MAJOR 0
#define FM_VERSION_MINOR 7
#define FM_VERSION_PATCH 2
#define FM_VERSION (10000000 * FM_VERSION_MAJOR + 10000 * FM_VERSION_MINOR + FM_VERSION_PATCH);

#define MESH_IN_CONNECTIONS 1
#define MESH_OUT_CONNECTIONS 3
#define MAX_NUM_MESH_CONNECTIONS (MESH_IN_CONNECTIONS+MESH_OUT_CONNECTIONS)

#define APP_IN_CONNECTIONS 1
#define APP_OUT_CONNECTIONS 1
#define MAX_NUM_APP_CONNECTIONS (APP_IN_CONNECTIONS+APP_OUT_CONNECTIONS)

#define MAX_NUM_CONNECTIONS (MESH_IN_CONNECTIONS+MESH_OUT_CONNECTIONS+APP_IN_CONNECTIONS+APP_OUT_CONNECTIONS)

extern uint32_t fruityMeshVersion;

#if defined(SIM_ENABLED)
	extern u32 __application_start_address;
	extern u32 __application_end_address;
	extern u32 __application_ram_start_address;
	extern u32 __start_conn_type_resolvers;
	extern u32 __stop_conn_type_resolvers;
#elif defined(__ICCARM__)
	extern u32 __ICFEDIT_region_ROM_start__; //Variable is set in the linker script
	extern u32 __ICFEDIT_region_ROM_end__; //Variable is set in the linker script
	extern u32 __ICFEDIT_region_RAM_start__; //Variable is set in the linker script
	extern u32 __start_conn_type_resolvers;
	extern u32 __stop_conn_type_resolvers;
#else
	extern u32 __application_start_address[]; //Variable is set in the linker script
	extern u32 __application_end_address[]; //Variable is set in the linker script
	extern u32 __application_ram_start_address[]; //Variable is set in the linker script
	extern u32 __start_conn_type_resolvers[];
	extern u32 __stop_conn_type_resolvers[];
#endif

enum deviceConfigOrigins{ RANDOM_CONFIG, UICR_CONFIG, TESTDEVICE_CONFIG };

//Alright, I know this is bad, but it's for readability....
//And static classes do need a seperate declaration and definition...
#ifndef Config
#define Config (&(Conf::getInstance()->configuration))
#endif


// ########### COMPILE TIME SETTINGS ##########################################

//Each of the Connections has a buffer for outgoing packets, this is its size in bytes
#define PACKET_SEND_BUFFER_SIZE 600

//Each connection does also have a buffer to assemble packets that were split into 20 byte chunks
#define PACKET_REASSEMBLY_BUFFER_SIZE 200

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
#define NODE_SERIAL_NUMBER_LENGTH 5 //A serial could use the alphabet "BCDFGHJKLMNPQRSTVWXYZ123456789". This is good for readability (short, no inter-digit resemblance, 25 million possible combinations, no funny words)

//Storage
#define RECORD_STORAGE_NUM_PAGES 2

/*############ TERMINAL AND LOGGER ################*/

//Size for tracing messages to the log transport, if it is too short, messages will get truncated
#define TRACE_BUFFER_SIZE 500

//Define to enable terminal in-/output through UART
#define USE_UART
//Use the SEGGER RTT protocol for in and output
//In J-Link RTT view, set line ending to CR and send input on enter, echo input to off
//#define USE_SEGGER_RTT
//In case stdout should be used, enable this (wont't work on nrf hardware)
//#define USE_STDIO

#define USE_BUTTONS

#define EOL "\r\n"
#define SEP "\r\n"

//If undefined, the final build will have no logging / Terminal functionality built in
#define ENABLE_LOGGING
//If defined, will log all json output
#define ENABLE_JSON_LOGGING
//Define this to automatically set the putty terminal title if in terminal mode
//#define SET_TERMINAL_TITLE


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
	NODE_ID=0, // Not a module per se, but why not let it send module messages
	ADVERTISING_MODULE_ID=1,
	SCANNING_MODULE_ID=2,
	STATUS_REPORTER_MODULE_ID=3,
	DFU_MODULE_ID=4,
	ENROLLMENT_MODULE_ID=5,
	IO_MODULE_ID=6,
	DEBUG_MODULE_ID=7,
	CONFIG_ID=8,
	BOARD_CONFIG_ID=9,

	//Custom modules
	CLC_MODULE_ID=150,
	MY_CUSTOM_MODULE_ID=151,

	//Invalid Module: 0xFF is the flash memory default and is therefore invalid
	INVALID_MODULE=255
};

//Activate and deactivate modules by un-/commenting these defines
#define ACTIVATE_ADVERTISING_MODULE
#define ACTIVATE_STATUS_REPORTER_MODULE
#define ACTIVATE_SCANNING_MODULE
//#define ACTIVATE_DFU_MODULE
#define ACTIVATE_ENROLLMENT_MODULE
#define ACTIVATE_IO_MODULE
#define ACTIVATE_DEBUG_MODULE
//#define ACTIVATE_CLC_MODULE

//The watchdog will trigger a system reset if it is not feed in time
//Using the safe boot mode will allow the beacon to reboot with its default configuration
//each second reboot (it will not read the config from flash)
//#define ACTIVATE_WATCHDOG
//#define ACTIVATE_WATCHDOG_SAFE_BOOT_MODE
//#define FM_WATCHDOG_TIMEOUT (32768UL * 20)
//#define FM_WATCHDOG_TIMEOUT (32768UL * 60 * 60 * 2)

/*############ Stuff for DFU ################*/

//Maximum size that a DFU chunk can be
#define DFU_DATA_BUFFER_SIZE 64


/*############ Config class ################*/
//This class holds the configuration and some bits are changeable at runtime

class Conf
{
	private:
		Conf();
		void generateRandomSerial();
		bool isEmpty(u32* mem, u8 numWords);

		static bool loadConfigFromFlash;

	public:
		static Conf* getInstance(){
			if(!GS->config){
				GS->config = new Conf();
			}
			return GS->config;
		}


		void Initialize(bool loadConfigFromFlash);


		void LoadDefaults();
		void LoadUicr();
		void LoadTestDevices();

		//General methods for loading settings
		static u32 GetSettingsPageBaseAddress();
		static bool LoadSettingsFromFlash(moduleID moduleId, ModuleConfiguration* configurationPointer, u16 configurationLength);
		static bool SaveModuleSettingsToFlash(moduleID moduleId, ModuleConfiguration* configurationPointer, u16 configurationLength, RecordStorageEventListener* listener, u32 userType, u8* userData, u16 userDataLength);

#pragma pack(push)
#pragma pack(1)
	struct ConfigConfiguration:ModuleConfiguration {
		// ########### DEBUGGING ################################################

		//This variable can be toggled via the Terminal "BREAK" and can be used
		//to toggle conditional breakpoints because the softdevice does not allow
		//runtime breakpoints while in a connection or stepping
		bool breakpointToggleActive : 8;

		//If in debug mode, the node will run in endless loops when errors occur
		bool debugMode : 8;

		//Instruct the Advertising Module to advertise Debug Packets
		bool advertiseDebugPackets : 8;

		ledMode defaultLedMode : 8;

		//Configures whether the terminal will start in interactive mode or not
		TerminalMode terminalMode : 8;

		// ########### TIMINGS ################################################

		//Main timer tick interval, must be set to a multiple of 1 decisecond (1/10th o a second)
		u16 mainTimerTickDs;

		//Mesh connection parameters (used when a connection is set up)
		//(7.5-4000) Minimum acceptable connection interval
		u16 meshMinConnectionInterval;
		//(7.5-4000) Maximum acceptable connection interval
		u16 meshMaxConnectionInterval;
		//(0-...) Slave latency in number of connection events
		u16 meshPeripheralSlaveLatency;
		//(100-32000) Connection supervisory timeout
		u16 meshConnectionSupervisionTimeout;
		//(0 - 65000) Extended timeout which is used to reconnect a known connection upon connection timeout
		u16 meshExtendedConnectionTimeoutSec;

		//Mesh discovery parameters
		//DISCOVERY_HIGH
		//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
		u16 meshAdvertisingIntervalHigh;
		//(20-1024) Determines scan interval in units of 0.625 millisecond.
		u16 meshScanIntervalHigh;
		//(2.5-1024) Determines scan window in units of 0.625 millisecond.
		u16 meshScanWindowHigh;


		//DISCOVERY_LOW
		//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
		u16 meshAdvertisingIntervalLow;
		//(20-1024) Determines scan interval in units of 0.625 millisecond.
		u16 meshScanIntervalLow;
		//(2.5-1024) Determines scan window in units of 0.625 millisecond.
		u16 meshScanWindowLow;


		//INITIATING
		//(20-1024) in 0.625ms units
		u16 meshConnectingScanInterval;
		//(2.5-1024) in 0.625ms units
		u16 meshConnectingScanWindow;
		//(0-...) in seconds
		u16 meshConnectingScanTimeout;

		//HANDSHAKE
		//If the handshake has not finished after this time, the connection will be disconnected
		u16 meshHandshakeTimeoutDs;


		//STATE timeouts
		//Timeout of the High discovery state before deciding to which partner to connect
		u16 meshStateTimeoutHighDs;
		//Timeout of the Low discovery state before deciding to which partner to connect
		u16 meshStateTimeoutLowDs;
		//Timeout until the back_off state will return to discovery
		u16 meshStateTimeoutBackOffDs;
		//Up to ... ms will be added randomly to the back off state timeout
		u16 meshStateTimeoutBackOffVarianceDs;
		// When discovery returns # times without results, the node will switch to low discovery
		u16 discoveryHighToLowTransitionDuration;

		/*
		 * If both conn_sup_timeout and max_conn_interval are specified, then the following constraint applies:
		 * conn_sup_timeout * 4 > (1 + slave_latency) * max_conn_interval that corresponds to the following
		 * BT Spec 4.1 Vol 2 Part E, Section 7.8.12 requirement: The Supervision_Timeout in milliseconds shall be
		 * larger than (1 + Conn_Latency) * Conn_Interval_Max * 2, where Conn_Interval_Max is given in milliseconds.
		 * https://devzone.nordicsemi.com/question/60/what-is-connection-parameters/
		 * */

		// ########### ADVERTISING ################################################
		u8 advertiseOnChannel37;
		u8 advertiseOnChannel38;
		u8 advertiseOnChannel39;

		// ########### CONNECTION ################################################

		// Will probably never change and code will not allow this to change without modifications
		u8 meshMaxInConnections;
		//Configurable from 1-7
		u8 meshMaxOutConnections;
		//for convenience
		u8 meshMaxConnections;

		bool enableRadioNotificationHandler : 8;

		bool enableConnectionRSSIMeasurement : 8;

		//Transmit Power used as default for this node
		i8 defaultDBmTX;


		// ########### VALUES from UICR / TEST_DEVICE (initialized in Config.cpp) #############

		u8 deviceConfigOrigin;

		char serialNumber[6];
		u32 serialNumberIndex;
		//According to the BLE company identifiers: https://www.bluetooth.org/en-us/specification/assigned-numbers/company-identifiers
		u16 manufacturerId;

		//When enabling encryption, the mesh handle can only be read through an encrypted connection
		//And connections will perform an encryption before the handshake
		bool encryptionEnabled : 8;
		//16 byte Long term key in little endian format
		u8 meshNetworkKey[16];
		//06050403020100090807060504030201;
		//01:02:03:04:05:06:07:08:09:00:01:02:03:04:05:06;

		//Allows a number of mesh networks to coexist in the same physical space without collision
		//Allowed range is 0x0000 - 0xFF00 (0 - 65280), others are reserved for special purpose
		networkID meshNetworkIdentifier;
		nodeID defaultNodeId;
		deviceTypes deviceType : 8;

		fh_ble_gap_addr_t staticAccessAddress;

		//Total connection count for in and out including mesh connections
		u8 totalInConnections;
		u8 totalOutConnections;
		//Time used for each connectionInterval in 1.25ms steps (Controls throughput)
		u8 gapEventLength;
	};
#pragma pack(pop)
	DECLARE_CONFIG_AND_PACKED_STRUCT(ConfigConfiguration);

	// ########### TEST DEVICES ################################################
	#ifdef ENABLE_TEST_DEVICES
	//For our test devices
	typedef struct{
		u32 chipID;
		nodeID id;
		deviceTypes deviceType;
		fh_ble_gap_addr_t addr;
	} testDevice;

	#define NUM_TEST_DEVICES 10
	static testDevice testDevices[];

	#define NUM_TEST_COLOUR_IDS 12
	static nodeID testColourIDs[];

	Conf::testDevice* getTestDevice();

	#endif
};

#include "featureset.h"
