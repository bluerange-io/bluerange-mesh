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
 * This file contains the mesh configuration, which is a singleton. Some of the
 * values can be changed at runtime to alter the meshing behaviour.
 */

#pragma once

#include <types.h>

#ifdef __cplusplus
#include <LedWrapper.h>
#include <GlobalState.h>
#include <FruityHalNrf.h>

class RecordStorageEventListener;

extern "C" {
#include <ble_gap.h>
#include <app_util.h>
#include <nrf_sdm.h>
#ifndef SIM_ENABLED
#include <nrf_uart.h>
#endif
}
#endif //__cplusplus

//major (0-400), minor (0-999), patch (0-9999)
#define FM_VERSION_MAJOR 0
#define FM_VERSION_MINOR 8
#define FM_VERSION_PATCH 8
#define FM_VERSION (10000000 * FM_VERSION_MAJOR + 10000 * FM_VERSION_MINOR + FM_VERSION_PATCH);

#define MESH_IN_CONNECTIONS 1
#define MESH_OUT_CONNECTIONS 3
#define MAX_NUM_MESH_CONNECTIONS (MESH_IN_CONNECTIONS+MESH_OUT_CONNECTIONS)

#define APP_IN_CONNECTIONS 2
#define APP_OUT_CONNECTIONS 0
#define MAX_NUM_APP_CONNECTIONS (APP_IN_CONNECTIONS+APP_OUT_CONNECTIONS)

#if defined(NRF51) || defined(SIM_ENABLED)
//NRF51 can not support that many connections, only use meshConnections
#define MAX_NUM_CONNECTIONS (MESH_IN_CONNECTIONS+MESH_OUT_CONNECTIONS)
#elif NRF52
#define MAX_NUM_CONNECTIONS (MESH_IN_CONNECTIONS+MESH_OUT_CONNECTIONS+APP_IN_CONNECTIONS+APP_OUT_CONNECTIONS)
#endif

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
#define Config (&(Conf::getInstance().configuration))
#define RamConfig (&(Conf::getInstance()))
#endif


// ########### COMPILE TIME SETTINGS ##########################################

//Used to check if an advertising packet is good enough to be connected to
#define STABLE_CONNECTION_RSSI_THRESHOLD -85

//Each of the Connections has a buffer for outgoing packets, this is its size in bytes
#define PACKET_SEND_BUFFER_SIZE 600
#define PACKET_SEND_BUFFER_HIGH_PRIO_SIZE 100

//Each connection does also have a buffer to assemble packets that were split into 20 byte chunks
#define PACKET_REASSEMBLY_BUFFER_SIZE 200

//Defines the maximum size of the mesh write attribute. This space is required in the ATTR table
#define MESH_CHARACTERISTIC_MAX_LENGTH 100

//Size of Attribute table can be set lower than the default if we do not need that much
#define ATTR_TABLE_MAX_SIZE 0x200

//Identifiers
#define COMPANY_IDENTIFIER 0x024D // Company identifier for manufacturer specific data header (M-Way Solutions GmbH) - Should not be changed to ensure compatibility with the mesh protocol
#define MESH_IDENTIFIER 0xF0 //Identifier that defines this as the fruitymesh protocol

#define SERVICE_DATA_SERVICE_UUID16 0xFE12 //UUID used for custom service

//GAP device name
#define DEVICE_NAME "FRUITY"

//Serial should be short but unique for the given manufacturer id
#define MANUFACTURER_ID 0xFFFF //The manufacturer id should match your company identifier that should be registered with the bluetooth sig: https://www.bluetooth.org/en-us/specification/assigned-numbers/company-identifiers
#define NODE_SERIAL_NUMBER_LENGTH 5 //A serial could use the alphabet "BCDFGHJKLMNPQRSTVWXYZ123456789". This is good for readability (short, no inter-digit resemblance, 25 million possible combinations, no funny words)

//Storage
#define RECORD_STORAGE_NUM_PAGES 2

#define RECORD_STORAGE_RECORD_ID_UPDATE_STATUS 1000 //Stores the done status of an update
#define RECORD_STORAGE_RECORD_ID_UICR_REPLACEMENT 1001 //Can be used, if UICR can not be flashed, e.g. when updating another beacon with different firmware
#define RECORD_STORAGE_RECORD_ID_FAKE_NODE_POSITIONS 1002 //Used to store fake positions for nodes to modify the incoming events

//TIMINGS
//Main timer tick interval, must be set to a multiple of 1 decisecond (1/10th of a second)
#define MAIN_TIMER_TICK_DS 2

/*############ TERMINAL AND LOGGER ################*/

//Size for tracing messages to the log transport, if it is too short, messages will get truncated
#define TRACE_BUFFER_SIZE 500

//Define to enable terminal in-/output through UART
//#define USE_UART
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
//Logs out trace statements
#define ACTIVATE_TRACE

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
	MESH_ACCESS_MODULE_ID=10,
	MANAGEMENT_MODULE_ID=11,
	TESTING_MODULE_ID=12,

	//M-way Modules
	CLC_MODULE_ID=150,
	VS_MODULE_ID=151,
	ENOCEAN_MODULE_ID=152,
	ASSET_MODULE_ID=153,
	EINK_MODULE_ID=154,
	WM_MODULE_ID=155,

	//Other Modules
	MY_CUSTOM_MODULE=200,

	//Invalid Module: 0xFF is the flash memory default and is therefore invalid
	INVALID_MODULE=255
};

//Activate and deactivate modules by un-/commenting these defines

//FruityMesh Standard Modules
#define ACTIVATE_ADVERTISING_MODULE
#define ACTIVATE_STATUS_REPORTER_MODULE
#define ACTIVATE_SCANNING_MODULE
#define ACTIVATE_ENROLLMENT_MODULE
#define ACTIVATE_IO_MODULE
#define ACTIVATE_DEBUG_MODULE

//FruityMesh non standard modules
//#define ACTIVATE_DFU_MODULE
//#define ACTIVATE_MA_MODULE

//Other features
#define ACTIVATE_BATTERY_MEASUREMENT
#define ACTIVATE_RADIO_NOTIFICATIONS

//The watchdog will trigger a system reset if it is not feed in time
//Using the safe boot mode will allow the beacon to reboot with its default configuration
//each second reboot (it will not read the config from flash)
//#define ACTIVATE_WATCHDOG
//#define ACTIVATE_WATCHDOG_SAFE_BOOT_MODE
//#define FM_WATCHDOG_TIMEOUT (32768UL * 20)
//#define FM_WATCHDOG_TIMEOUT (32768UL * 60 * 60 * 2)

//Allows us to unwind the stack if an error occured, to save space (5 kb), we can disable this
//but we must also remove -funwind-tables from the Makefile
//#define ACTIVATE_STACK_UNWINDING

//Configures the maximum number of firmware group ids that can be compiled into the firmware
#define MAX_NUM_FW_GROUP_IDS 2

//By enabling this, we can store a record with positions for all beaocns in a mesh, the rssi of incoming events
//will then be manipulated to reflect these positions
//#define ENABLE_FAKE_NODE_POSITIONS

/*############ Config class ################*/
//This class holds the configuration and some bits are changeable at runtime

#ifdef __cplusplus
class Conf
{
	private:
		Conf();
		void generateRandomSerialAndNodeId();
		bool isEmpty(const u8* mem, u16 numBytes) const;

	public:
		static Conf& getInstance(){
			if(!GS->config){
				GS->config = new Conf();
			}
			return *(GS->config);
		}

		static bool loadConfigFromFlash;

		//The Firmware GroupIds are used to check update compatibility if a firmware update is
		//requested. First id should be reserved for hardware type (e.g. nrf51/nrf52)
		NodeId fwGroupIds[MAX_NUM_FW_GROUP_IDS];

		//################ The following data is can use defaults from the code but is
		//################ overwritten if it exists in the UICR
		//Not loaded from UICR but set to the place id that the config was loaded from
		u8 deviceConfigOrigin;
		//Serial Number in ASCII and its index (loaded from UICR if available)
		char serialNumber[6];
		u32 serialNumberIndex;
		//According to the BLE company identifiers: https://www.bluetooth.org/en-us/specification/assigned-numbers/company-identifiers
		// (loaded from UICR if 0)
		u16 manufacturerId;
		//Allows a number of mesh networks to coexist in the same physical space without collision
		//Allowed range is 0x0000 - 0xFF00 (0 - 65280), others are reserved for special purpose
		// (loaded from UICR if 0)
		NetworkId defaultNetworkId;
		//Default network key if preenrollment should be used  (loaded from UICR if 0)
		u8 defaultNetworkKey[16];
		//Default user base key
		u8 defaultUserBaseKey[16];
		//The default nodeId after flashing (loaded from UICR if 0)
		NodeId defaultNodeId;
		//Type of device belongs to deviceTypes (loaded from UICR if 0)
		u8 deviceType : 8;
		//16 byte Long term node key in little endian format (loaded from UICR if 0)
		u8 nodeKey[16];
		//Used to set a static random BLE address (loaded from UICR if type set to 0xFF)
		fh_ble_gap_addr_t staticAccessAddress;
		//##################



		void Initialize(bool loadConfigFromFlash);


		void LoadDefaults();
		void LoadUicr();
		void LoadTestDevices() const;


#pragma pack(push)
#pragma pack(1)
	struct ConfigConfiguration : ModuleConfiguration {
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

		u16 mainTimerTickDs_deprecated; //Deprecated, is now set at compile time

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
		//From 4 to 16384 (2.5ms to 10s) in 0.625ms Units
		u16 meshScanIntervalHigh;
		//From 4 to 16384 (2.5ms to 10s) in 0.625ms Units
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
		u16 deprecated_meshStateTimeoutHighDs;
		//Timeout of the Low discovery state before deciding to which partner to connect
		u16 deprecated_meshStateTimeoutLowDs;
		//Timeout until the back_off state will return to discovery
		u16 deprecated_meshStateTimeoutBackOffDs;
		//Up to ... ms will be added randomly to the back off state timeout
		u16 deprecated_meshStateTimeoutBackOffVarianceDs;
		// When discovery returns # times without results, the node will switch to low discovery
		u16 deprecated_discoveryHighToLowTransitionDuration;

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

		//Total connection count for in and out including mesh connections
		u8 totalInConnections;
		u8 totalOutConnections;
		//Time used for each connectionInterval in 1.25ms steps (Controls throughput)
		u8 gapEventLength;

		//When enabling encryption, the mesh handle can only be read through an encrypted connection
		//And connections will perform an encryption before the handshake
		bool encryptionEnabled : 8;

		//If more than # nodes were found, decide immediately
		u8 numNodesForDecision;
		//If not enough nodes were found, decide after this timeout
		u16 maxTimeUntilDecisionDs;
		//Switch to low discovery if no other nodes were found for # seconds, set to 0 to disable low discovery state
		u16 highToLowDiscoveryTimeSec;

	};
#pragma pack(pop)
	DECLARE_CONFIG_AND_PACKED_STRUCT(ConfigConfiguration);

	// ########### TEST DEVICES ################################################
	#ifdef ENABLE_TEST_DEVICES
	//For our test devices
	typedef struct{
		u32 chipID;
		NodeId id;
		deviceTypes deviceType;
		fh_ble_gap_addr_t addr;
	} testDevice;

	#define NUM_TEST_DEVICES 10
	static testDevice testDevices[];

	#define NUM_TEST_COLOUR_IDS 12
	static NodeId testColourIDs[];

	Conf::testDevice* getTestDevice();

	#endif
};
#endif // __cplusplus

//Helpers
u32* getUicrDataPtr();


//Includes a header for for a featureset
#ifdef FEATURESET_NAME
#include FEATURESET_NAME
#endif

//If a featureset is included, we 
#ifdef FEATURESET
struct ModuleConfiguration;
#define SET_FEATURESET_CONFIGURATION XCONCAT(setFeaturesetConfiguration_,FEATURESET)
extern void SET_FEATURESET_CONFIGURATION(ModuleConfiguration* config);
#elif SIM_ENABLED
#define SET_FEATURESET_CONFIGURATION(configuration) setFeaturesetConfiguration_CherrySim(configuration);
#else
static_assert(false, "Featureset was not defined, which is mandatory!");
#endif

//We set a define so that the terminal is enabled if one of the following is defined
#if defined(USE_SEGGER_RTT) || defined(USE_UART) || defined(USE_STDIO)
#define TERMINAL_ENABLED
#endif
