////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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
 * 
 * *** ATTENTION! ***
 * The preferred way to change this configuration is by using a featureset
 * in which you are able to define all values and they will have precedence over the
 * values that are defined in this configuration.
 * *** ATTENTION! ***
 */

#pragma once

#include <FmTypes.h>
#include "RecordStorage.h"
#include "Boardconfig.h"

#ifdef __cplusplus
#include <LedWrapper.h>

class RecordStorageEventListener;
#endif //__cplusplus

// ########### FruityMesh Version ##########################################

// major (0-400), minor (0-999), patch (0-9999)
#define FM_VERSION_MAJOR 0
#define FM_VERSION_MINOR 8
//WARNING! The Patch version line is automatically changed by a python script on every master merge!
//Do not change by hand unless you understood the exact behaviour of the said script.
#define FM_VERSION_PATCH 4830
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
#define SET_BOARD_CONFIGURATION XCONCAT(SetBoardConfiguration_,FEATURESET)
extern void SET_BOARD_CONFIGURATION(BoardConfiguration* config);
#define SET_FEATURESET_CONFIGURATION XCONCAT(SetFeaturesetConfiguration_,FEATURESET)
extern void SET_FEATURESET_CONFIGURATION(ModuleConfiguration* config, void* module);
#define SET_FEATURESET_CONFIGURATION_VENDOR XCONCAT(SetFeaturesetConfigurationVendor_,FEATURESET)
extern void SET_FEATURESET_CONFIGURATION_VENDOR(VendorModuleConfiguration* config, void* module);
#define INITIALIZE_MODULES XCONCAT(InitializeModules_,FEATURESET)
extern u32 INITIALIZE_MODULES(bool createModule);
#define GET_DEVICE_TYPE XCONCAT(GetDeviceType_,FEATURESET)
extern DeviceType GET_DEVICE_TYPE();
#define GET_CHIPSET XCONCAT(GetChipset_,FEATURESET)
extern Chipset GET_CHIPSET();
#define GET_FEATURE_SET_GROUP XCONCAT(GetFeatureSetGroup_,FEATURESET)
extern FeatureSetGroup GET_FEATURE_SET_GROUP();
#define GET_WATCHDOG_TIMEOUT XCONCAT(GetWatchdogTimeout_,FEATURESET)
extern u32 GET_WATCHDOG_TIMEOUT();
#define GET_WATCHDOG_TIMEOUT_SAFE_BOOT XCONCAT(GetWatchdogTimeoutSafeBoot_,FEATURESET)
extern u32 GET_WATCHDOG_TIMEOUT_SAFE_BOOT();
#elif defined(SIM_ENABLED)
#define SET_BOARD_CONFIGURATION(configuration) SetBoardConfiguration_CherrySim(configuration);

//This line should be called in every module to be able to load featureset dependent configuration
//Use SET_FEATURESET_CONFIGURATION_VENDOR for vendor modules instead
#define SET_FEATURESET_CONFIGURATION(configuration, module) SetFeaturesetConfiguration_CherrySim(configuration, module);
#define SET_FEATURESET_CONFIGURATION_VENDOR(configuration, module) SetFeaturesetConfigurationVendor_CherrySim(configuration, module);

#define INITIALIZE_MODULES(createModule) InitializeModules_CherrySim((createModule));
extern DeviceType GetDeviceType_CherrySim();
#define GET_DEVICE_TYPE() GetDeviceType_CherrySim()
extern Chipset GetChipset_CherrySim();
#define GET_CHIPSET() GetChipset_CherrySim()
extern FeatureSetGroup GetFeatureSetGroup_CherrySim();
#define GET_FEATURE_SET_GROUP() GetFeatureSetGroup_CherrySim();
extern u32 GetWatchdogTimeout_CherrySim();
#define GET_WATCHDOG_TIMEOUT() GetWatchdogTimeout_CherrySim()
extern u32 GetWatchdogTimeoutSafeBoot_CherrySim();
#define GET_WATCHDOG_TIMEOUT_SAFE_BOOT() GetWatchdogTimeoutSafeBoot_CherrySim()
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
#define PACKET_SEND_BUFFER_SIZE 2000
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

// Maximum MTU size for GATT operations. Using a higher MTU will increase the RAM usage of the SoftDevice
// enormously as it will consume multiple buffers per connection. Linker script needs to be changed
// This should be a multiple of 20 bytes + 3 as the ATT header adds 3 bytes, this will make it easier to
// optimize the application packets in 20 byte chunks. Default and minimum MTU according to BLE is 23 byte
// => look for the platform specific configuration for the max MTU to change this (for Nordic e.g. the sdk_config.h)

//The maximum number of advertising jobs that can be managed by the AdvertisingController
#ifndef ADVERTISING_CONTROLLER_MAX_NUM_JOBS
#define ADVERTISING_CONTROLLER_MAX_NUM_JOBS 4
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
// For some projects, the preset M-Way identifier can be used for free if the serial number range and the
// modules use the Vendor range. Consult us if you are unsure
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

// ########### Config class ##########################################
//This class holds the configuration and some bits are changeable at runtime

#ifdef __cplusplus
enum class PreferredConnectionMode : u8 {
    // Unpreferred connections...
    PENALTY = 0,    //        ...receive a penalty in cluster score
    IGNORED = 1        //        ...are completly ignored (cluster score is set to zero)
};

class Module;
class Conf
    : public RecordStorageEventListener
{
    private:
        void GenerateRandomSerialAndNodeId();
        bool IsEmpty(const u8* mem, u16 numBytes) const;

        //Buffer for the serialNumber in ASCII format
        mutable char _serialNumber[NODE_SERIAL_NUMBER_MAX_CHAR_LENGTH];
        mutable u32 serialNumberIndex = 0;


        enum class RecordTypeConf : u32
        {
            SET_SERIAL = 0,
        };

    public:
        Conf();
        static Conf& GetInstance();

        bool safeBootEnabled = false;

        void LoadSettingsFromFlash(Module* module, u16 recordId, u8* configurationPointer, u16 configurationLength);
        RecordStorageResultCode SaveConfigToFlash(RecordStorageEventListener* listener, u32 userType, u8* userData, u16 userDataLength);

        u32 GetSerialNumberIndex() const;
        const char* GetSerialNumber() const;
        void SetSerialNumberIndex(u32 serialNumber);

        const u8* GetNodeKey() const;

        void GetRestrainedKey(u8* buffer) const;

        static constexpr const char* RESTRAINED_KEY_CLEAR_TEXT = "RESTRAINED_KEY00";
        void SetNodeKey(const u8 *key);

        //The Firmware GroupIds are used to check update compatibility if a firmware update is
        //requested. First id should be reserved for hardware type (e.g. nrf52)
        NodeId fwGroupIds[MAX_NUM_FW_GROUP_IDS];

        //################ The following data can use defaults from the code but is
        //################ overwritten if it exists in the DeviceConfiguration
        //Not loaded from DeviceConfiguration but set to the place id that the config was loaded from
        DeviceConfigOrigins deviceConfigOrigin = DeviceConfigOrigins::RANDOM_CONFIG;
        //According to the BLE company identifiers: https://www.bluetooth.org/en-us/specification/assigned-numbers/company-identifiers
        // (loaded from DeviceConfiguration if 0)
        u16 manufacturerId = 0;
        //Allows a number of mesh networks to coexist in the same physical space without collision
        //Allowed range is 0x0000 - 0xFF00 (0 - 65280), others are reserved for special purpose
        // (loaded from DeviceConfiguration if 0)
        NetworkId defaultNetworkId = 0;
        //Default network key if preenrollment should be used  (loaded from DeviceConfiguration if 0)
        u8 defaultNetworkKey[16];
        //Default user base key
        u8 defaultUserBaseKey[16];
        //The default nodeId after flashing (loaded from DeviceConfiguration if 0)
        NodeId defaultNodeId = 0;
        //Used to set a static random BLE address (loaded from DeviceConfiguration if type set to 0xFF)
        FruityHal::BleGapAddr staticAccessAddress;
        //##################

        void RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength) override;

        void Initialize(bool safeBootEnabled);


        void LoadDefaults();
        void LoadDeviceConfiguration();
        void LoadTestDevices() const;

        //If in debug mode, the node will run in endless loops when errors occur
        static constexpr bool debugMode = false;

        //(0 - 65000) Extended timeout which is used to reconnect a known connection upon connection timeout
        static constexpr u16 meshExtendedConnectionTimeoutSec = 10;

        //(0-...) Slave latency in number of connection events
        static constexpr u16 meshPeripheralSlaveLatency = 0;

        //(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
        static constexpr u16 meshAdvertisingIntervalLow = (u16)MSEC_TO_UNITS(200, CONFIG_UNIT_0_625_MS);

        //INITIATING
        //(20-1024) in 0.625ms units
        static constexpr u16 meshConnectingScanInterval = 120; //FIXME_HAL: 120 units = 75ms (0.625ms steps)
        //(2.5-1024) in 0.625ms units
        static constexpr u16 meshConnectingScanWindow = 60; //FIXME_HAL: 60 units = 37.5ms (0.625ms steps)
        //(0-...) in seconds
        static constexpr u16 meshConnectingScanTimeout = 2;

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
        u16 highToLowDiscoveryTimeSec = 0; // if is not configured in featureset, low discovery will be disabled and will always be in high discovery mode

        LedMode defaultLedMode = LedMode::OFF;

        //If set, the node won't send anything via UART if the reboot reason is unknown and it hasn't received anything yet.
        //This is so because the meshGW bootloader thinks that incomming UART chars are keyboard inputs.
        bool silentStart = false;

        //Configures whether the terminal will start in interactive mode or not
        TerminalMode terminalMode : 8;

        bool enableSinkRouting = false;
        // ########### TIMINGS ################################################

        //Mesh connection parameters (used when a connection is set up)
        //(7.5-4000) Minimum acceptable connection interval
        u16 meshMinConnectionInterval = 0;
        //(7.5-4000) Maximum acceptable connection interval
        u16 meshMaxConnectionInterval = 0;
        //(100-32000) Connection supervisory timeout
        static constexpr u16 meshConnectionSupervisionTimeout = (u16)MSEC_TO_UNITS(1000, CONFIG_UNIT_10_MS);

        //Mesh discovery parameters
        //DISCOVERY_HIGH
        //(20-1024) (100-1024 for non connectable advertising!) Determines advertising interval in units of 0.625 millisecond.
        static constexpr u16 meshAdvertisingIntervalHigh = (u16)MSEC_TO_UNITS(100, CONFIG_UNIT_0_625_MS);
        //From 4 to 16384 (2.5ms to 10s) in 0.625ms Units
        u16 meshScanIntervalHigh = 0;
        //From 4 to 16384 (2.5ms to 10s) in 0.625ms Units
        u16 meshScanWindowHigh = 0;


        //DISCOVERY_LOW
        //(20-1024) Determines scan interval in units of 0.625 millisecond.
        u16 meshScanIntervalLow = 0;
        //(2.5-1024) Determines scan window in units of 0.625 millisecond.
        u16 meshScanWindowLow = 0;


        // ########### CONNECTION ################################################

        //Transmit Power used as default for this node
        static constexpr i8 defaultDBmTX = 4;

        //Depending on platform capabilities, we need to set a different amount of
        //possible connnections, whereas the simulator will need to select that at runtime
        //Having two meshInConnections allows us to perform clustering more easily and
        //prevents most denial of service attacks
#ifndef SIM_ENABLED
        static constexpr u8 totalInConnections = 3;
        static constexpr u8 totalOutConnections = 3;
        u8 meshMaxInConnections = 2;
        static constexpr u8 meshMaxOutConnections = 3;
#else
        u8 totalInConnections = 3;
        u8 totalOutConnections = 3;
        u8 meshMaxInConnections = 2;
        u8 meshMaxOutConnections = 3;
#endif // SIM_ENABLED

#ifndef SIM_ENABLED
        static_assert(totalOutConnections >= meshMaxOutConnections, "meshMaxOutConnections must not be bigger than totalOutConnections");
#endif

        static constexpr size_t MAX_AMOUNT_PREFERRED_PARTNER_IDS = 8;

        u32 GetFruityMeshVersion() const;

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
#define RamConfig (&(Conf::GetInstance()))
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

//Check if the packet size is valid
#if PACKET_REASSEMBLY_BUFFER_SIZE < MAX_MESH_PACKET_SIZE
#error "Wrong send buffer configuration"
#endif

// Set the Terminal to enabled if one of the log transports is defined
#if (ACTIVATE_SEGGER_RTT == 1) || (ACTIVATE_UART == 1) || (ACTIVATE_STDIO == 1)
#define TERMINAL_ENABLED
#endif
