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

		//Do not use any persistently saved module data
		bool ignorePersistentConfigurationOnBoot = true;

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
		u16 meshStateTimeoutHigh = 1 * 1000; //Timeout of the High discovery state before deciding to which partner to connect
		u16 meshStateTimeoutLow = 10 * 1000; //Timeout of the Low discovery state before deciding to which partner to connect
		u16 meshStateTimeoutBackOff = 2 * 1000; //Timeout until the back_off state will return to discovery
		u16 meshStateTimeoutBackOffVariance = 2 * 1000;  //Up to ... ms will be added randomly to the back off state timeout

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
		u8 advertiseOnChannel38 = 1;
		u8 advertiseOnChannel39 = 1;



		// ########### CONNECTION ################################################

		u16 meshNetworkIdentifier = 1; //Allows a number of mesh networks to coexist in the same physical space without collision

		const u8 meshMaxInConnections = 1; // Will probably never change and code will not allow this to change without modifications
		const u8 meshMaxOutConnections = 3; //Will certainly change with future S130 versions
		const u8 meshMaxConnections = meshMaxInConnections + meshMaxOutConnections; //for convenience

};


// ########### COMPILE TIME SETTINGS ##########################################

//Select the board for which to compile
#include <board_pca10031.h>
//#include <board_adafruit_ble_friend.h>

#define VERSION_STRING "0.1"

//Each of the Connections has a buffer for outgoing packets, this is its size in bytes
#define PACKET_SEND_BUFFER_SIZE 400

//Each connection does also have a buffer to assemble packets that were split into 20 byte chunks
#define PACKET_REASSEMBLY_BUFFER_SIZE 200

//Number of supported Modules
#define MAX_MODULE_COUNT 10

//Number of connections that the mesh can use
#define MAXIMUM_CONNECTIONS 4

//Defines the maximum size of the mesh write attribute. This space is required in the ATTR table
#define MESH_CHARACTERISTIC_MAX_LENGTH 100

//Size of Attribute table can be set lower than the default if we do not need that much
#define ATTR_TABLE_MAX_SIZE 0x200

//Identifiers
#define COMPANY_IDENTIFIER 0x024D // Company identifier for manufacturer specific data header
#define MESH_IDENTIFIER 0xF1 //Identifier that defines this as the mesh protocol

//Specifiy some chip-intern ids and use the to give the nodes human readable numbers
#define ID045_DEVICE_ID 0x625B70B2
#define ID458_DEVICE_ID 0x378023FE
#define ID847_DEVICE_ID 0xD0F1F53A
#define ID635_DEVICE_ID 0x745EF251
#define ID880_DEVICE_ID 0x3e0a4045
#define ID072_DEVICE_ID 0x872bbd44
#define ID667_DEVICE_ID 0xfbb59d31

//GAP device name
#define DEVICE_NAME "FRUITY"

/*############ LOGGER ################*/

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

	//Custom modules
	TEST_MODULE_ID=30000
};

/*############ Regarding node ids ################*/
// Refer to protocol specification
#define NODE_ID_BROADCAST 0
#define NODE_ID_DEVICE_BASE 0
#define NODE_ID_GROUP_BASE 20000
#define NODE_ID_HOPS_BASE 30000
#define NODE_ID_SHORTEST_SINK 31001
