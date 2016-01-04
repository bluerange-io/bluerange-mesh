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

extern "C" {
#include <ble_gap.h>
#include <app_util.h>
}


//Alright, I know this is bad, but it's for readability....
//And static classes do need a seperate declaration and definition...
#define Config Conf::getInstance()

//This class holds the configuration and some buts are changeable at runtime
class Conf
{
	private:
		Conf(){};
		static Conf* instance;

	public:

		static Conf* getInstance(){
				if(!instance) instance = new Conf();
				return instance;
			}

		// ########### DEBUGGING ################################################

		//Do not load the persistent node configuration
		//Beware: Persistent config must be updated with every connection loss or random shoudl be used....?
		bool ignorePersistentNodeConfigurationOnBoot = true;

		//Do not use any persistently saved module data
		bool ignorePersistentModuleConfigurationOnBoot = true;

		//This variable can be toggled via the Terminal "BREAK" and can be used
		//to toggle conditional breakpoints because the softdevice does not allow
		//runtime breakpoints while in a connection or stepping
		bool breakpointToggleActive = false;


		// ########### TIMINGS ################################################

		//Main timer tick interval
		u32 mainTimerTickMs = 200;

		//Mesh connection parameters (used when a connection is set up)
		u16 meshMinConnectionInterval = MSEC_TO_UNITS(100, UNIT_1_25_MS);   	//(7.5-4000) Minimum acceptable connection interval
		u16 meshMaxConnectionInterval = MSEC_TO_UNITS(100, UNIT_1_25_MS);   	//(7.5-4000) Maximum acceptable connection interval
		u16 meshPeripheralSlaveLatency = 0;                  					//(0-...) Slave latency in number of connection events
		u16 meshConnectionSupervisionTimeout = MSEC_TO_UNITS(6000, UNIT_10_MS);   	//(100-32000) Connection supervisory timeout

		//Mesh discovery parameters
		//DISCOVERY_HIGH
		u16 meshAdvertisingIntervalHigh = MSEC_TO_UNITS(100, UNIT_0_625_MS);	//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
		u16 meshScanIntervalHigh = MSEC_TO_UNITS(100, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
		u16 meshScanWindowHigh = MSEC_TO_UNITS(40, UNIT_0_625_MS);	//(2.5-1024) Determines scan window in units of 0.625 millisecond.


		//DISCOVERY_LOW
		u16 meshAdvertisingIntervalLow = MSEC_TO_UNITS(5000, UNIT_0_625_MS);	//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
		u16 meshScanIntervalLow = MSEC_TO_UNITS(1000, UNIT_0_625_MS);	//(20-1024) Determines scan interval in units of 0.625 millisecond.
		u16 meshScanWindowLow = MSEC_TO_UNITS(5, UNIT_0_625_MS);	//(2.5-1024) Determines scan window in units of 0.625 millisecond.


		//INITIATING
		u16 meshConnectingScanInterval = MSEC_TO_UNITS(100, UNIT_0_625_MS); //(20-1024) in 0.625ms units
		u16 meshConnectingScanWindow = MSEC_TO_UNITS(80, UNIT_0_625_MS); //(2.5-1024) in 0.625ms units
		u16 meshConnectingScanTimeout = 5; //(0-...) in seconds

		//HANDSHAKE
		u16 meshHandshakeTimeout = 10; //If the handshake has not finished after this time, the connection will be disconnected


		//STATE timeouts
		u16 meshStateTimeoutHigh = 3 * 1000; //Timeout of the High discovery state before deciding to which partner to connect
		u16 meshStateTimeoutLow = 10 * 1000; //Timeout of the Low discovery state before deciding to which partner to connect
		u16 meshStateTimeoutBackOff = 1 * 1000; //Timeout until the back_off state will return to discovery
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

		//Allows a number of mesh networks to coexist in the same physical space without collision
		//Allowed range is 0x0000 - 0xFF00 (0 - 65280), others are reserved for special purpose
		u16 meshNetworkIdentifier = 3;

		const u8 meshMaxInConnections = 1; // Will probably never change and code will not allow this to change without modifications
		const u8 meshMaxOutConnections = 3; //Will certainly change with future S130 versions
		const u8 meshMaxConnections = meshMaxInConnections + meshMaxOutConnections; //for convenience

		const bool enableRadioNotificationHandler = false;
		const bool enableConnectionRSSIMeasurement = true;

		// ########### ENCRYPTION ################################################
		//When enabling encryption, the mesh handle can only be read through an encrypted connection
		//And connections will perform an encryption before the handshake
		const bool encryptionEnabled = false;
		const u8 meshNetworkKey[16] = {1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6}; //16 byte Long term key in little endian format
		//06050403020100090807060504030201 => How to enter it in the MCP
		//01:02:03:04:05:06:07:08:09:00:01:02:03:04:05:06 => Format for TI Sniffer



		// ########### OTHER ################################################
		u16 firmwareVersionMajor = 0; //0-400
		u16 firmwareVersionMinor = 1; //0-999
		u16 firmwareVersionPatch = 9; //0-9999
		u32 firmwareVersion = 10000000 * firmwareVersionMajor + 10000 * firmwareVersionMinor + firmwareVersionPatch;

};


// ########### COMPILE TIME SETTINGS ##########################################

//Select the board for which to compile
#ifdef NRF51
	#include <board_pca10031.h>
//#include <board_ars100748.h>
#endif
#ifdef NRF52
	#include <board_pca10036.h>
#endif

//Each of the Connections has a buffer for outgoing packets, this is its size in bytes
#define PACKET_SEND_BUFFER_SIZE 400

//Each connection does also have a buffer to assemble packets that were split into 20 byte chunks
#define PACKET_REASSEMBLY_BUFFER_SIZE 200

//Size for tracing messages to UART, if it is too short, messages will get truncated
#define TRACE_BUFFER_SIZE 500

//Number of supported Modules
#define MAX_MODULE_COUNT 10

//Number of connections that the mesh can use
#define MAXIMUM_CONNECTIONS 4

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
#define STORAGE_BLOCK_NUMBER 10 //Determines the number of blocks that are available

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
//The module ids are used to identify a module over the network, these must fit in two bytes
//Numbers below 30000 are standard defined, numbers obove this range are free to use for
//custom types, always leave a space of 10 to allow for multiple module instances
enum moduleID{
	//Standard modules
	ADVERTISING_MODULE_ID=10,
	SCANNING_MODULE_ID=20,
	STATUS_REPORTER_MODULE_ID=30,
	DFU_MODULE_ID=40,
	ENROLLMENT_MODULE_ID=50,
	IO_MODULE_ID=60,

	//Custom modules
    DEBUG_MODULE_ID=30000,
    RSSI_MODULE_ID=30001
};

/*############ Regarding node ids ################*/
// Refer to protocol specification
#define NODE_ID_BROADCAST 0
#define NODE_ID_DEVICE_BASE 0
#define NODE_ID_GROUP_BASE 20000
#define NODE_ID_HOPS_BASE 30000
#define NODE_ID_SHORTEST_SINK 31001



