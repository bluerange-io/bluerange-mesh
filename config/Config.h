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
#include <FruityHalNrf.h>

class RecordStorageEventListener;
#endif //__cplusplus

// ########### FruityMesh Version ##########################################

// major (0-400), minor (0-999), patch (0-9999)
#define FM_VERSION_MAJOR 0
#define FM_VERSION_MINOR 8
//WARNING! The Patch version line is automatically changed by a python script on every master merge!
//Do not change by hand unless you understood the exact behaviour of the said script.
#define FM_VERSION_PATCH 540
#define FM_VERSION (10000000 * FM_VERSION_MAJOR + 10000 * FM_VERSION_MINOR + FM_VERSION_PATCH)
#ifdef __cplusplus
static_assert(FM_VERSION_MAJOR >= 0                            , "Malformed Major version!");
static_assert(FM_VERSION_MINOR >= 0 && FM_VERSION_MINOR <= 999 , "Malformed Minor version!");
static_assert(FM_VERSION_PATCH >= 0 && FM_VERSION_PATCH <= 9999, "Malformed Patch version!");
#endif

// ########### Featureset inclusion ##########################################
// The normal way to configure FruityMesh is to define the values in the featureset
// This way, the config does not have to be edited. Module default settings can also
// be set in the featureset. This can be seen as kind of an inheritance of the Config.

//Includes a header for for a featureset
#ifdef FEATURESET_NAME
#include FEATURESET_NAME
#endif

#ifdef FEATURESET
struct ModuleConfiguration;
#define SET_FEATURESET_CONFIGURATION XCONCAT(setFeaturesetConfiguration_,FEATURESET)
extern void SET_FEATURESET_CONFIGURATION(ModuleConfiguration* config, void* module);
#define INITIALIZE_MODULES XCONCAT(initializeModules_,FEATURESET)
extern u32 INITIALIZE_MODULES(bool createModule);
#define GET_DEVICE_TYPE XCONCAT(getDeviceType_,FEATURESET)
extern DeviceType GET_DEVICE_TYPE();
#elif SIM_ENABLED
#define SET_FEATURESET_CONFIGURATION(configuration, module) setFeaturesetConfiguration_CherrySim(configuration, module);
#define INITIALIZE_MODULES(createModule) initializeModules_CherrySim((createModule));
extern DeviceType getDeviceType_CherrySim();
#define GET_DEVICE_TYPE() getDeviceType_CherrySim()
#else
static_assert(false, "Featureset was not defined, which is mandatory!");
#endif

// ########### Connection Setup ##########################################
// Using a higher number of connections will require a change to the linker
// script and the ram allocation

//The total number of connections supported must be bigger than the total number
//of connections configured in the ble stack (necessary for array sizing)
//Cannot be changed in featureset as this must also be changed in the
//linker script ram section at the same time
#define TOTAL_NUM_CONNECTIONS 5

// ########### Mesh Settings ##########################################

// Used to check if an advertising packet is good enough to be connected to
// Settings this too low will result in bad connections that might disconnect
// or reduce the possible mesh throughput
#ifndef STABLE_CONNECTION_RSSI_THRESHOLD
#define STABLE_CONNECTION_RSSI_THRESHOLD -85
#endif

// ########### Ram Buffer Settings ##########################################
// These settings affect the ram or usage a lot

// Each of the connections has a buffer for outgoing packets, this is its size in bytes
#ifndef MAX_MESH_PACKET_SIZE
#define MAX_MESH_PACKET_SIZE 200
#endif

// Each of the connections has a buffer for outgoing packets, this is its size in bytes
#ifndef PACKET_SEND_BUFFER_SIZE
#ifdef NRF51
#define PACKET_SEND_BUFFER_SIZE 600
#else
#define PACKET_SEND_BUFFER_SIZE 2000
#endif
#endif

// Each connection also has a high prio buffer e.g. for mesh clustering packets
#ifndef PACKET_SEND_BUFFER_HIGH_PRIO_SIZE
#define PACKET_SEND_BUFFER_HIGH_PRIO_SIZE 100
#endif

// Each connection does also have a buffer to assemble packets that were split into 20 byte chunks
// This is the maximum size that these packets can have
#ifndef PACKET_REASSEMBLY_BUFFER_SIZE
#define PACKET_REASSEMBLY_BUFFER_SIZE MAX_MESH_PACKET_SIZE
#endif

// Defines the maximum size of the mesh write attribute. This space is required in the ATTR table
#ifndef MESH_CHARACTERISTIC_MAX_LENGTH
#define MESH_CHARACTERISTIC_MAX_LENGTH 100
#endif

// Size of Attribute table can be set lower than the default if we do not need that much
#ifndef ATTR_TABLE_MAX_SIZE
#define ATTR_TABLE_MAX_SIZE 0x200
#endif

// ########### Flash Settings ##########################################
// Number of pages used to store records, at least 2 are required for swapping
#ifndef RECORD_STORAGE_NUM_PAGES
#define RECORD_STORAGE_NUM_PAGES 2
#endif

// ########### General ##########################################
// GAP device name (Not used by the mesh)
#ifndef DEVICE_NAME
#define DEVICE_NAME "FRUITY"
#endif

// The manufacturer id used to identify who manufactured the device
// It must match your company identifier that has to be registered with
// the bluetooth sig: https://www.bluetooth.org/en-us/specification/assigned-numbers/company-identifiers
// For production, the preset M-Way identifier can be used
#ifndef MANUFACTURER_ID
#define MANUFACTURER_ID 0x024D
#endif

// The main timer tick interval in ticks defines how often the node
// is woken up without receiving events. 6554 is a good value (32768 times a second)
#ifndef MAIN_TIMER_TICK
#define MAIN_TIMER_TICK 6554 //roughly 2 ds
#endif

// Define this to automatically set the putty terminal title if in terminal mode
#ifndef ACTIVATE_SET_TERMINAL_TITLE
#define ACTIVATE_SET_TERMINAL_TITLE 0
#endif

// Allows us to unwind the stack if an error occured, to save space (5 kb), we can enable this
// but we must also add -funwind-tables to the Makefile.
#ifndef ACTIVATE_STACK_UNWINDING
#define ACTIVATE_STACK_UNWINDING 0
#endif

// By enabling this, we can store a record with positions for all beaocns in a mesh, the rssi of incoming events
// will then be manipulated to reflect these positions. Useful for easier mesh testing but complicated to use
#ifndef ACTIVATE_FAKE_NODE_POSITIONS
#define ACTIVATE_FAKE_NODE_POSITIONS 0
#endif

// ########### Logging ##########################################
// Define which kind of output should be compiled in or not
// Enabling different kinds of output will increase the size of the binary a lot

//TODO: These defines need to be check for their value in the implementation

// Compile log output into the binary (logt)
// This affects the binary size a lot
#ifndef ACTIVATE_LOGGING
#define ACTIVATE_LOGGING 1
#endif

// Compile json output into the binary (logjson)
// used for e.g. communication with a gateway over UART
#ifndef ACTIVATE_JSON_LOGGING
#define ACTIVATE_JSON_LOGGING 1
#endif

//Compile trace statements into the binary (trace)
#ifndef ACTIVATE_TRACE
#define ACTIVATE_TRACE 1
#endif

// ########### Log Transport ##########################################
// Define which method for input and output should be used

// Define to enable terminal in-/output through UART
#ifndef ACTIVATE_UART
#define ACTIVATE_UART 0
#endif

// Use the SEGGER RTT protocol for in and output
// In J-Link RTT view, set line ending to CR and send input on enter, echo input to off
#ifndef ACTIVATE_SEGGER_RTT
#define ACTIVATE_SEGGER_RTT 0
#endif

// In case stdout should be used, enable this (wont't work on nrf hardware)
#ifndef ACTIVATE_STDIO
#define ACTIVATE_STDIO 0
#endif

// ########### Features ##########################################
//TODO: check everywhere

// Activate to enable button press support
#ifndef ACTIVATE_BUTTONS
#define ACTIVATE_BUTTONS 1
#endif

// Activate periodic battery measurement that is reported through the node status
#ifndef ACTIVATE_BATTERY_MEASUREMENT
#define ACTIVATE_BATTERY_MEASUREMENT 1
#endif

// ########### Watchdog ##########################################

//The watchdog will trigger a system reset if it is not feed in time
#ifndef ACTIVATE_WATCHDOG
#define ACTIVATE_WATCHDOG 1
#endif

#ifndef FM_WATCHDOG_TIMEOUT
#define FM_WATCHDOG_TIMEOUT (32768UL * 60 * 60 * 2)
#endif

#ifndef FM_WATCHDOG_TIMEOUT_SAFE_BOOT
#define FM_WATCHDOG_TIMEOUT_SAFE_BOOT (32768UL * 20) // Timeout in safe boot mode
#endif

//Using the safe boot mode will allow the beacon to reboot with its default configuration
//each second reboot (it will not read the config from flash)
#ifndef ACTIVATE_WATCHDOG_SAFE_BOOT_MODE
#define ACTIVATE_WATCHDOG_SAFE_BOOT_MODE 1
#endif



// ########### Config class ##########################################
//This class holds the configuration and some bits are changeable at runtime

#ifdef __cplusplus
enum class PreferredConnectionMode : u8 {
	// Unpreferred connections...
	PENALTY = 0,	//		...receive a penalty in cluster score
	IGNORED = 1		//		...are completly ignored (cluster score is set to zero)
};

class Module;
class Conf
{
	private:
		void generateRandomSerialAndNodeId();
		bool isEmpty(const u8* mem, u16 numBytes) const;

		//Buffer for the serialNumber in ASCII format
		mutable char _serialNumber[6];
		mutable u32 serialNumberIndex;

	public:
		Conf();
		static Conf& getInstance();

		bool safeBootEnabled;

		void LoadSettingsFromFlash(Module* module, ModuleId moduleId, ModuleConfiguration* configurationPointer, u16 configurationLength);
		void LoadSettingsFromFlashWithId(ModuleId moduleId, ModuleConfiguration* configurationPointer, u16 configurationLength);

		u32 GetSerialNumberIndex() const;
		const char* GetSerialNumber() const;
		void SetSerialNumberIndex(u32 serialNumber);

		const u8* GetNodeKey() const;
		void GetRestrainedKey(u8* buffer) const;

		static constexpr const char* RESTRAINED_KEY_CLEAR_TEXT = "RESTRAINED_KEY00";
		void SetNodeKey(const u8 *key);

		//The Firmware GroupIds are used to check update compatibility if a firmware update is
		//requested. First id should be reserved for hardware type (e.g. nrf51/nrf52)
		NodeId fwGroupIds[MAX_NUM_FW_GROUP_IDS];

		//################ The following data is can use defaults from the code but is
		//################ overwritten if it exists in the UICR
		//Not loaded from UICR but set to the place id that the config was loaded from
		DeviceConfigOrigins deviceConfigOrigin;
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
		//Used to set a static random BLE address (loaded from UICR if type set to 0xFF)
		fh_ble_gap_addr_t staticAccessAddress;
		//##################



		void Initialize(bool safeBootEnabled);


		void LoadDefaults();
		void LoadUicr();
		void LoadTestDevices() const;

		//If in debug mode, the node will run in endless loops when errors occur
		static constexpr bool debugMode = false;

		//(0 - 65000) Extended timeout which is used to reconnect a known connection upon connection timeout
		static constexpr u16 meshExtendedConnectionTimeoutSec = 10;

		//(0-...) Slave latency in number of connection events
		static constexpr u16 meshPeripheralSlaveLatency = 0;

		//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
		static constexpr u16 meshAdvertisingIntervalLow = (u16)MSEC_TO_UNITS(200, UNIT_0_625_MS);

		//INITIATING
		//(20-1024) in 0.625ms units
		static constexpr u16 meshConnectingScanInterval = (u16)MSEC_TO_UNITS(20, UNIT_0_625_MS);
		//(2.5-1024) in 0.625ms units
		static constexpr u16 meshConnectingScanWindow = (u16)MSEC_TO_UNITS(4, UNIT_0_625_MS);
		//(0-...) in seconds
		static constexpr u16 meshConnectingScanTimeout = 3;

		//HANDSHAKE
		//If the handshake has not finished after this time, the connection will be disconnected
		static constexpr u16 meshHandshakeTimeoutDs = SEC_TO_DS(4);

		/*
		 * If both conn_sup_timeout and max_conn_interval are specified, then the following constraint applies:
		 * conn_sup_timeout * 4 > (1 + slave_latency) * max_conn_interval that corresponds to the following
		 * BT Spec 4.1 Vol 2 Part E, Section 7.8.12 requirement: The Supervision_Timeout in milliseconds shall be
		 * larger than (1 + Conn_Latency) * Conn_Interval_Max * 2, where Conn_Interval_Max is given in milliseconds.
		 * https://devzone.nordicsemi.com/question/60/what-is-connection-parameters/
		 * */

		 // ########### ADVERTISING ################################################
		static constexpr u8 advertiseOnChannel37 = 1;
		static constexpr u8 advertiseOnChannel38 = 1;
		static constexpr u8 advertiseOnChannel39 = 1;

		static constexpr bool enableRadioNotificationHandler = false;

		static constexpr bool enableConnectionRSSIMeasurement = true;
		//Time used for each connectionInterval in 1.25ms steps (Controls throughput)
		static constexpr u8 gapEventLength = 3;

		//When enabling encryption, the mesh handle can only be read through an encrypted connection
		//And connections will perform an encryption before the handshake
		static constexpr bool encryptionEnabled = true;

		//If more than # nodes were found, decide immediately
		static constexpr u8 numNodesForDecision = 4;
		//If not enough nodes were found, decide after this timeout
		static constexpr u16 maxTimeUntilDecisionDs = SEC_TO_DS(2);
		//Switch to low discovery if no other nodes were found for # seconds, set to 0 to disable low discovery state
		u16 highToLowDiscoveryTimeSec; // if is not configured in featureset, low discovery will be disabled and will always be in high discovery mode

		LedMode defaultLedMode;

		//Configures whether the terminal will start in interactive mode or not
		TerminalMode terminalMode : 8;

		bool enableSinkRouting;
		// ########### TIMINGS ################################################

		//Mesh connection parameters (used when a connection is set up)
		//(7.5-4000) Minimum acceptable connection interval
		u16 meshMinConnectionInterval;
		//(7.5-4000) Maximum acceptable connection interval
		u16 meshMaxConnectionInterval;
		//(100-32000) Connection supervisory timeout
		static constexpr u16 meshConnectionSupervisionTimeout = (u16)MSEC_TO_UNITS(6000, UNIT_10_MS);

		//Mesh discovery parameters
		//DISCOVERY_HIGH
		//(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
		static constexpr u16 meshAdvertisingIntervalHigh = (u16)MSEC_TO_UNITS(100, UNIT_0_625_MS);
		//From 4 to 16384 (2.5ms to 10s) in 0.625ms Units
		u16 meshScanIntervalHigh;
		//From 4 to 16384 (2.5ms to 10s) in 0.625ms Units
		u16 meshScanWindowHigh;


		//DISCOVERY_LOW
		//(20-1024) Determines scan interval in units of 0.625 millisecond.
		u16 meshScanIntervalLow;
		//(2.5-1024) Determines scan window in units of 0.625 millisecond.
		u16 meshScanWindowLow;


		// ########### CONNECTION ################################################

		//Transmit Power used as default for this node
		static constexpr i8 defaultDBmTX = 4;

		//Depending on NRF51 / NRF52 capabilities, we need to set a different amount of
		//possible connnections, whereas the simulator will need to select that at runtime
		//Having two meshInConnections allows us to perform clustering more easily and
		//prevents most denial of service attacks
#ifdef NRF51
		// Max amount of connections that the BLE stack will be configured with
		static constexpr u8 totalInConnections = 1;
		static constexpr u8 totalOutConnections = 3;
		// Total connections for building the mesh, if more than one inConnection is configured,
		// it will be used only temporarily but not for permanent connections
		static constexpr u8 meshMaxInConnections = 1;
		static constexpr u8 meshMaxOutConnections = 3;
#elif NRF52
		static constexpr u8 totalInConnections = 3;
		static constexpr u8 totalOutConnections = 3;
		static constexpr u8 meshMaxInConnections = 2;
		static constexpr u8 meshMaxOutConnections = 3;
#elif SIM_ENABLED
		//Use a default setting for the nrf52
		inline static u8 totalInConnections = 3;
		inline static u8 totalOutConnections = 3;
		inline static u8 meshMaxInConnections = 2;
		inline static u8 meshMaxOutConnections = 3;
#endif

		static constexpr size_t MAX_AMOUNT_PREFERRED_PARTNER_IDS = 8;

		u32 getFruityMeshVersion() const;

#pragma pack(push)
#pragma pack(1)
	struct ConfigConfiguration : ModuleConfiguration {
		u32 overwrittenSerialNumberIndex;
		bool isSerialNumberIndexOverwritten : 8;

		NodeId preferredPartnerIds[MAX_AMOUNT_PREFERRED_PARTNER_IDS];
		u8 amountOfPreferredPartnerIds;
		PreferredConnectionMode preferredConnectionMode;

		u8 nodeKey[16];
	};
#pragma pack(pop)
	DECLARE_CONFIG_AND_PACKED_STRUCT(ConfigConfiguration);
};
#endif // __cplusplus


// ########### External variables ##########################################
// This section makes some external variables accessible to the implementation

// Linker variables
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

//Alright, I know this is bad, but it's for readability....
//And static classes do need a seperate declaration and definition...
#ifndef Config
#define RamConfig (&(Conf::getInstance()))
#endif


// ##########################################################
// Checking of values and calculations
// ###########################################################

// Calculate max number of mesh connections
#define MAX_NUM_MESH_CONNECTIONS (MESH_IN_CONNECTIONS+MESH_OUT_CONNECTIONS)

// Calculate max number of app connections
#define MAX_NUM_APP_CONNECTIONS (APP_IN_CONNECTIONS+APP_OUT_CONNECTIONS)

// Calculate the total connections
#define MAX_NUM_CONNECTIONS (MESH_IN_CONNECTIONS+MESH_OUT_CONNECTIONS+APP_IN_CONNECTIONS+APP_OUT_CONNECTIONS)

// Check if the connection vonfiguration is valid
#if (defined(NRF51) || defined(SIM_ENABLED)) && (APP_IN_CONNECTIONS) > 1
#error "NRF51 only supports 1 connection as a peripheral"
#elif NRF52
#define MAX_NUM_CONNECTIONS (MESH_IN_CONNECTIONS+MESH_OUT_CONNECTIONS+APP_IN_CONNECTIONS+APP_OUT_CONNECTIONS)
#endif

//Check if the packet size is valid
#if PACKET_REASSEMBLY_BUFFER_SIZE < MAX_MESH_PACKET_SIZE
#error "Wrong send buffer configuration"
#endif

// Set the Terminal to enabled if one of the log transports is defined
#if (ACTIVATE_SEGGER_RTT == 1) || (ACTIVATE_UART == 1) || (ACTIVATE_STDIO == 1)
#define TERMINAL_ENABLED
#endif
