////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2021 M-Way Solutions GmbH
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

/**
 * This file contains a number of types that are used throughout the implementation.
 */

#pragma once

#include <stdint.h>
#include <type_traits>

// The [[nodiscard]] attribute is a C++17 feature
// and thus is only supported in the Simulator.
#ifdef SIM_ENABLED
#define NO_DISCARD [[nodiscard]]
#else
#define NO_DISCARD
#endif

//If a NO_DISCARD value is intentionally unhandeled, use this
#define DISCARD(value) static_cast<void>(value)

// The [[fallthrough]] attribute is a C++17 feature
// and thus is only supported in the Simulator.
#ifdef SIM_ENABLED
#define FALLTHROUGH [[fallthrough]]
#else
#define FALLTHROUGH
#endif

//std::is_trivially_copyable seems to be unavailable in GCC 4.9, although it is specified to be available in C++11.
#if defined(__GNUG__) && __GNUC__ < 5
#define HAS_TRIVIAL_COPY(T) __has_trivial_copy(T)
#else
#define HAS_TRIVIAL_COPY(T) std::is_trivially_copyable<T>::value
#endif

//Unsigned ints
typedef uint8_t u8;
typedef uint16_t u16;
typedef unsigned u32;   //This is not defined uint32_t because GCC defines uint32_t as unsigned long, 
                        //which is a problem when working with printf placeholders.

//Signed ints
typedef int8_t i8;
typedef int16_t i16;
typedef int i32;        //This is not defined int32_t because GCC defines int32_t as long,
                        //which is a problem when working with printf placeholders.

static_assert(sizeof(u8) == 1, "");
static_assert(sizeof(u16) == 2, "");
static_assert(sizeof(u32) == 4, "");

static_assert(sizeof(i8) == 1, "");
static_assert(sizeof(i16) == 2, "");
static_assert(sizeof(i32) == 4, "");

//Data types for the mesh
typedef u16 NetworkId;
typedef u16 NodeId;
typedef u32 ClusterId;
typedef i16 ClusterSize;

/*## Available Node ids #############################################################*/
// Refer to protocol specification @https://github.com/mwaylabs/fruitymesh/wiki/Protocol-Specification

constexpr NodeId NODE_ID_BROADCAST = 0; //A broadcast will be received by all nodes within one mesh
constexpr NodeId NODE_ID_DEVICE_BASE = 1; //Beginning from 1, we can assign nodeIds to individual devices
constexpr NodeId NODE_ID_DEVICE_BASE_SIZE = 1999;

constexpr NodeId NODE_ID_VIRTUAL_BASE = 2000; //Used to assign sub-addresses to connections that do not belong to the mesh but want to perform mesh activity. Used as a multiplier.
constexpr NodeId NODE_ID_GROUP_BASE = 20000; //Used to assign group ids to nodes. A node can take part in many groups at once
constexpr NodeId NODE_ID_GROUP_BASE_SIZE = 10000;

constexpr NodeId NODE_ID_LOCAL_LOOPBACK = 30000; //30000 is like a local loopback address that will only send to the current node,
constexpr NodeId NODE_ID_HOPS_BASE = 30000; //30001 sends to the local node and one hop further, 30002 two hops
constexpr NodeId NODE_ID_HOPS_BASE_SIZE = 1000;

constexpr NodeId NODE_ID_SHORTEST_SINK = 31000;
constexpr NodeId NODE_ID_ANYCAST_THEN_BROADCAST = 31001; //31001 will send the message to any one of the connected nodes and only that node will then broadcast this message

constexpr NodeId NODE_ID_APP_BASE = 32000; //Custom GATT services, connections with Smartphones, should use (APP_BASE + moduleId)
constexpr NodeId NODE_ID_APP_BASE_SIZE = 1000;

constexpr NodeId NODE_ID_GLOBAL_DEVICE_BASE = 33000; //Can be used to assign nodeIds that are valid organization wide (e.g. for assets)
constexpr NodeId NODE_ID_GLOBAL_DEVICE_BASE_SIZE = 7000;

constexpr NodeId NODE_ID_CLC_SPECIFIC = 40000; //see usage in CLC module
constexpr NodeId NODE_ID_RESERVED = 40001; //Yet unassigned nodIds
constexpr NodeId NODE_ID_INVALID = 0xFFFF; //Special node id that is used in error cases. It must never be used as an sender or receiver.

//Different types of supported BLE stacks, specific versions can be added later if necessary
enum class BleStackType {
    INVALID = 0,
    //NRF_SD_130_ANY = 100, //Deprecated as of 09.04.2020
    NRF_SD_132_ANY = 200,
    NRF_SD_140_ANY = 300,
};

// Chipset group ids. These define what kind of chipset the firmware is running on
enum class Chipset : NodeId
{
    CHIP_INVALID = 0,
    //CHIP_NRF51 = 20000, //Deprecated as of 09.04.2020
    CHIP_NRF52 = 20001,
    CHIP_NRF52833 = 20014,
    CHIP_NRF52840 = 20015,
};

/*## Key Types #############################################################*/
//Types of keys used by the mesh and other modules
enum class FmKeyId : u32
{
    ZERO = 0,
    NODE = 1,
    NETWORK = 2,
    BASE_USER = 3,
    ORGANIZATION = 4,
    RESTRAINED = 5,
    USER_DERIVED_START = 10,
    USER_DERIVED_END = (UINT32_MAX / 2),
};

/*## Modules #############################################################*/
//The module ids are used to identify a module over the network
//Modules MUST NOT be created within this range
//For experimenting, the section for "Other Modules" can be used, but no guarantee
//for any future changes to this section is made and the moduleIds will clash as soon
//as nodes of other vendors are used within the same network
//Use the VendorModuleId for a future proof and supported way of implementing modules
enum class ModuleId : u8 {
    //Standard modules
    NODE = 0, // Not a module per se, but why not let it send module messages
    BEACONING_MODULE = 1,
    SCANNING_MODULE = 2,
    STATUS_REPORTER_MODULE = 3,
    DFU_MODULE = 4,
    ENROLLMENT_MODULE = 5,
    IO_MODULE = 6,
    DEBUG_MODULE = 7,
    CONFIG = 8,
    //BOARD_CONFIG = 9, //deprecated as of 20.01.2020 (boardconfig is not a module anymore)
    MESH_ACCESS_MODULE = 10,
    //MANAGEMENT_MODULE=11, //deprecated as of 22.05.2019
    TESTING_MODULE = 12,
    BULK_MODULE = 13,
    ENVIRONMENT_SENSING_MODULE = 14, //Placeholder for environmental sensing module

    //M-Way Modules
    CLC_MODULE = 150,
    VS_MODULE = 151,
    ENOCEAN_MODULE = 152,
    ASSET_MODULE = 153,
    EINK_MODULE = 154,
    WM_MODULE = 155,
    ET_MODULE = 156, //Placeholder for Partner
    MODBUS_MODULE = 157,
    BP_MODULE = 158,
    ASSET_SCANNING_MODULE = 159,

    //Other Modules, this range can be used for experimenting but must not be used if FruityMesh
    //nodes are to be used in a network with nodes of different vendors as their moduleIds will clash
    MY_CUSTOM_MODULE = 200,
    //PING_MODULE = 201, Deprecated as of 26.08.2020, now uses a VendorModuleId
    //VENDOR_TEMPLATE_MODULE = 202, Deprecated as of 20.08.2020, now uses a VendorModuleId
    SIG_EXAMPLE_MODULE = 203,

    //The VendorModuleId was introduced to have a range of moduleIds that do not clash
    //between different vendors
    VENDOR_MODULE_ID_PREFIX = 240, //0xF0

    //The in between space is reserved for later extensions, e.g. of the vendor module ids

    //Invalid Module: 0xFF is the flash memory default and is therefore invalid
    INVALID_MODULE = 255,
};

//The vendor module id is a future proof way of implementing modules that will not clash
//if different nodes of different vendors are used within one network
typedef u32 VendorModuleId;
constexpr u32 INVALID_VENDOR_MODULE_ID = 0xFFFFFFFFUL;

//This is used to specify either a VendorModuleId or a ModuleId when offering functionality that must be available for both types
//Using a u32 and the conversion methods in the Utility class makes it simpler than working with the below union/struct
typedef u32 ModuleIdWrapper;
constexpr u32 INVALID_WRAPPED_MODULE_ID = 0xFFFFFFFFUL;

//The ModuleIdWrapper is used to build a wrapped module id
//This wrapper is mostly inteded for internal usage, use the appropriate Methods from the Utility class
#pragma pack(push, 1)
typedef union {
    struct {
        ModuleId prefix; //For a VendorModuleId, the prefix must be set to VENDOR_MODULE_ID_PREFIX; for a ModuleId it must be lower than this value
        u8 subId; // For a VendorModuleId, the subId is used to create a number of different modules for each vendor; for a ModuleId, this must be set to 0xFF
        u16 vendorId; // For a VendorModuleId, the vendor id must be set to the company identifier given by the BLE SIG (see https://www.bluetooth.com/de/specifications/assigned-numbers/company-identifiers/); for a ModuleId, this must be 0xFFFF
    };
    ModuleIdWrapper wrappedModuleId; //This is used to access the full 4 byte VendorModuleId
} ModuleIdWrapperUnion;
static_assert(sizeof(ModuleIdWrapperUnion) == 4, "You have a weird compiler");
#pragma pack(pop)

constexpr VendorModuleId GET_VENDOR_MODULE_ID(u16 vendorId, u8 subId) { return ( ((u8)ModuleId::VENDOR_MODULE_ID_PREFIX) | ((u8)subId << 8) | ((u16)vendorId << 16) ); }

enum class SensorType : u8 {
    CO2_SENSOR = 0x30, // Measured CO2 concentration in ppm
};

// The reason why the device was rebooted
enum class RebootReason : u8 {
    UNKNOWN = 0,
    HARDFAULT = 1,
    APP_FAULT = 2,
    SD_FAULT = 3,
    PIN_RESET = 4,
    WATCHDOG = 5,
    FROM_OFF_STATE = 6,
    LOCAL_RESET = 7,
    REMOTE_RESET = 8,
    ENROLLMENT = 9,
    PREFERRED_CONNECTIONS = 10,
    DFU = 11,
    MODULE_ALLOCATOR_OUT_OF_MEMORY = 12,
    MEMORY_MANAGEMENT = 13,
    BUS_FAULT = 14,
    USAGE_FAULT = 15,
    ENROLLMENT_REMOVE = 16,
    FACTORY_RESET_FAILED = 17,
    FACTORY_RESET_SUCCEEDED_FAILSAFE = 18,
    SET_SERIAL_SUCCESS = 19,
    SET_SERIAL_FAILED = 20,
    SEND_TO_BOOTLOADER = 21,
    UNKNOWN_BUT_BOOTED = 22,    // This is set after successful boot when we can't figure out reset reason eg. after flashing.
    STACK_OVERFLOW = 23,
    NO_CHUNK_FOR_NEW_CONNECTION = 24,
    IMPLEMENTATION_ERROR_NO_QUEUE_SPACE_AFTER_CHECK = 25,
    IMPLEMENTATION_ERROR_SPLIT_WITH_NO_LOOK_AHEAD = 26,
    CONFIG_MIGRATION = 27,
    DEVICE_OFF = 28,
    DEVICE_WAKE_UP = 29,
    FACTORY_RESET = 30,

    //INFO: Make sure to add new enum values to the Logger.cpp class

    USER_DEFINED_START = 200,
    USER_DEFINED_END = 255,
};

/*############ Live Report types ################*/
//Live reports are sent through the mesh as soon as something notable happens
//Could be some info, a warning or an error

enum class LiveReportTypes : u8
{
    //##### Error Reporting disabled #####
    LEVEL_OFF = 0,

    //##### Fatal Error Level (Below 50) #####

    LEVEL_FATAL = 50,

    //##### Warning Level (Below 100) #####
    HANDSHAKED_MESH_DISCONNECTED = 51, //extra is partnerid, extra2 is appDisconnectReason
    WARN_GAP_DISCONNECTED = 52, //extra is partnerid, extra2 is hci code
    LEVEL_WARN = 100,

    //##### Info Level (below 150) #####
    GAP_CONNECTED_INCOMING = 101, //extra is connHandle, extra2 is 4 bytes of gap addr
    GAP_TRYING_AS_MASTER = 102, //extra is partnerId, extra2 is 4 bytes of gap addr
    GAP_CONNECTED_OUTGOING = 103, //extra is connHandle, extra2 is 4 byte of gap addr
    //Deprecated: GAP_DISCONNECTED = 104,
    HANDSHAKE_FAIL = 105, //extra is tempPartnerId, extra2 is handshakeFailCode
    MESH_CONNECTED = 106, //extra is partnerid, extra2 is asWinner
    //Deprecated: MESH_DISCONNECTED = 107,
    LEVEL_INFO = 150,

    //##### Debug Level (Everything else) #####
    DECISION_RESULT = 151, //extra is decision type, extra2 is preferredPartner
    LEVEL_DEBUG
};

enum class LiveReportHandshakeFailCode : u8
{
    SUCCESS,
    SAME_CLUSTERID,
    NETWORK_ID_MISMATCH,
    WRONG_DIRECTION,
    UNPREFERRED_CONNECTION,
};

enum class PinsetIdentifier : u16 {
    UNKNOWN = 0,
    BME280 = 1, //Barometer
    LIS2DH12 = 2, //Accelerometer
    TLV493D = 3, //Magnetometer
    BMG250 = 4, //Gyroscope
    GDEWO27W3 = 5, //Eink Display
};

struct CustomPins {
    PinsetIdentifier pinsetIdentifier = PinsetIdentifier::UNKNOWN;
};

struct Bme280Pins : CustomPins {
    i32 misoPin = -1;
    i32 mosiPin = -1;
    i32 sckPin = -1;
    i32 ssPin = -1;
    i32 sensorEnablePin = -1;
    bool sensorEnablePinActiveHigh = true;
};

struct Tlv493dPins : CustomPins {
    i32 sckPin = -1;
    i32 sdaPin = -1;
    i32 sensorEnablePin = -1;
    i32 twiEnablePin = -1;
    bool sensorEnablePinActiveHigh = true;
    bool twiEnablePinActiveHigh = true;
};

struct Bmg250Pins : CustomPins {
    i32 sckPin = -1;
    i32 sdaPin = -1;
    i32 interrupt1Pin = -1;
    i32 sensorEnablePin = -1;//-1 if sensor enable pin is not present
    i32 twiEnablePin = -1; // -1 if twi enable pin is not present
    bool sensorEnablePinActiveHigh = true;
    bool twiEnablePinActiveHigh = true;
};

struct Lis2dh12Pins : CustomPins {
    i32 mosiPin = -1;
    i32 misoPin = -1;
    i32 sckPin = -1;
    i32 ssPin = -1;
    i32 sdaPin = -1;//if lis2dh12 is attached to twi interface then we use only sda and sck pin
    i32 interrupt1Pin = -1;//FiFo interrupt is on pin 1
    i32 interrupt2Pin = -1;//movement detection interrupt is on pin 2
    i32 sensorEnablePin = -1;//if equal to -1 means that enable Pin is not present
    bool sensorEnablePinActiveHigh = true;
};

struct Gdewo27w3Pins : CustomPins {
    i32 dcPin = -1;
    i32 mosiPin = -1;
    i32 misoPin = -1;
    i32 sckPin = -1;
    i32 ssPin = -1;
    i32 resPin = -1;
    i32 busyPin = -1;
    i32 epdEnablePin = -1;
    bool epdEnablePinActiveHigh = true;
};

// Not as primitive as one might hope but other primitive types require this class.
class MessageLength
{
    // The main purpose of this class is to make it harder and thus more secure to use
    // equality == checks. The reason why this should be hard is that equality checks
    // are not upward compatible.
private:
    u16 m_length = 0;

public:
    MessageLength() { /*Do nothing*/ }

    // The following constructor may be none-explicit to make the usage of this type easier
    // as it can be treated just like a numeric value (except the missing == operator)
    // cppcheck-suppress noExplicitConstructor
    /*none-explicit*/ MessageLength(u16 length)
    {
        m_length = length;
    }

    bool operator>=(u16 otherLength) const
    {
        return m_length >= otherLength;
    }
    bool operator<=(u16 otherLength) const
    {
        return m_length <= otherLength;
    }
    bool operator>(u16 otherLength) const
    {
        return m_length > otherLength;
    }
    bool operator<(u16 otherLength) const
    {
        return m_length < otherLength;
    }
    bool operator!=(u16 otherLength) const
    {
        return m_length != otherLength;
    }
    bool operator!=(MessageLength otherLength) const
    {
        return m_length != otherLength.m_length;
    }
    friend bool operator>=(u16 otherLength, const MessageLength& length)
    {
        return otherLength >= length.GetRaw();
    }
    friend bool operator<=(u16 otherLength, const MessageLength& length)
    {
        return otherLength <= length.GetRaw();
    }
    friend bool operator>(u16 otherLength, const MessageLength& length)
    {
        return otherLength > length.GetRaw();
    }
    friend bool operator<(u16 otherLength, const MessageLength& length)
    {
        return otherLength < length.GetRaw();
    }
    friend bool operator!=(u16 otherLength, const MessageLength& length)
    {
        return otherLength != length.GetRaw();
    }
    // NOTE: The operator== is undefined on purpose! In most of the cases it
    //       is much better to check for >= to preserver upward compatibility.
    //       as such, the name for the == check method is so long to give a
    //       hint to others that they probably don't want to use it.
    bool IsZero() const
    {
        return m_length == 0;
    }

    MessageLength operator-(u16 length) const
    {
        return MessageLength(m_length - length);
    }
    MessageLength operator+(u16 length) const
    {
        return MessageLength(m_length + length);
    }
    MessageLength& operator+=(u16 length)
    {
        m_length += length;
        return *this;
    }
    MessageLength& operator-=(u16 length)
    {
        m_length -= length;
        return *this;
    }
    u8* operator+(u8* ptr)
    {
        return ptr + m_length;
    }
    u8 const * operator+(u8 const * ptr)
    {
        return ptr + m_length;
    }
    friend u8* operator+(u8* ptr, const MessageLength& length)
    {
        return ptr + length.GetRaw();
    }
    friend u8 const * operator+(u8 const * ptr, const MessageLength& length)
    {
        return ptr + length.GetRaw();
    }
    friend MessageLength operator-(u16 otherLength, const MessageLength& length)
    {
        return MessageLength(otherLength - length.GetRaw());
    }
    friend MessageLength operator+(u16 otherLength, const MessageLength& length)
    {
        return MessageLength(otherLength + length.GetRaw());
    }

    u16 GetRaw() const
    {
        // NOTE: Do NOT use the return value from this function with equality checks! Use >=, <=, >, < instead!
        return m_length;
    }

    u16& GetRawRef()
    {
        // NOTE: Do NOT use the return value from this function with equality checks! Use >=, <=, >, < instead!
        return m_length;
    }
    const u16& GetRawRef() const
    {
        // NOTE: Do NOT use the return value from this function with equality checks! Use >=, <=, >, < instead!
        return m_length;
    }
};
static_assert(HAS_TRIVIAL_COPY(MessageLength), "MessageLength must be trivially copyable as it is used in a lot of POD types.");
// The following checks make sure that MessageType<T> is binary compatible with places that previously just used T.
static_assert(sizeof(MessageLength) == sizeof(u16), "MessageLength must be binary compatible with a u16.");

//A struct that combines a data pointer and the accompanying length
struct SizedData {
    u8*           data; //Pointer to data
    MessageLength length; //Length of Data
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

enum class DeliveryPriority : u8{
    VITAL  = 0, //Must only be used for mesh relevant data
    HIGH   = 1,
    MEDIUM = 2,
    LOW    = 3,
    // CAREFUL: If you add more priorities, do not forget to change AMOUNT_OF_SEND_QUEUE_PRIORITIES!

    INVALID = 255,
};
constexpr u32 AMOUNT_OF_SEND_QUEUE_PRIORITIES = 4;

// To determine from which location the node config was loaded
enum class DeviceConfigOrigins : u8 {
    RANDOM_CONFIG = 0,
    UICR_CONFIG = 1,
    TESTDEVICE_CONFIG = 2,
};

// The different kind of nodes supported by FruityMesh
enum class DeviceType : u8 {
    INVALID = 0,
    STATIC = 1, // A normal node that remains static at one position
    ROAMING = 2, // A node that is moving constantly or often (not implemented)
    SINK = 3, // A static node that wants to acquire data, e.g. a MeshGateway
    ASSET = 4, // A roaming node that is sporadically or never connected but broadcasts data
    LEAF = 5,  // A node that will never act as a slave but will only connect as a master (useful for roaming nodes, but no relaying possible)
};

// The different terminal modes
enum class TerminalMode : u8 {
    JSON = 0, //Interrupt based terminal input and blocking output
    PROMPT = 1, //blockin in and out with echo and backspace options
    DISABLED = 2, //Terminal is disabled, no in and output
};

//Enrollment states
enum class EnrollmentState : u8 {
    NOT_ENROLLED = 0,
    ENROLLED = 1,
};

//These codes are returned from the PreEnrollmentHandler
enum class PreEnrollmentReturnCode : u8 {
    DONE = 0, //PreEnrollment of the Module was either not necessary or successfully done
    WAITING = 1, //PreEnrollment must do asynchronous work and will afterwards call the PreEnrollmentDispatcher
    FAILED = 2, //PreEnrollment was not successfuly, so enrollment should continue
};

//Used for intercepting messages befoure they are routed through the mesh
typedef u32 RoutingDecision;
constexpr RoutingDecision ROUTING_DECISION_BLOCK_TO_MESH = 0x1;
constexpr RoutingDecision ROUTING_DECISION_BLOCK_TO_MESH_ACCESS = 0x2;

//Defines the different scanning intervals for each state
enum class ScanState : u8 {
    LOW = 0,
    HIGH = 1,
    CUSTOM = 2,
};

// Mesh discovery states
enum class DiscoveryState : u8 {
    INVALID = 0,
    HIGH = 1, // Scanning and advertising at a high duty cycle
    LOW = 2, // Scanning and advertising at a low duty cycle
    IDLE = 3, // Scanning disabled and advertising at a low duty cycle
    OFF = 4, // Scanning and advertising not enabled by the node to save power (Other modules might still advertise or scan)
};

//All known Subtypes of BaseConnection supported by the ConnectionManager
enum class ConnectionType : u8 {
    INVALID = 0,
    FRUITYMESH = 1, // A mesh connection
    APP = 2, // Base class of a customer specific connection (deprecated)
    CLC_APP = 3,
    RESOLVER = 4, // Resolver connection used to determine the correct connection
    MESH_ACCESS = 5, // MeshAccessConnection
};

//This enum defines packet authorization for MeshAccessConnetions
//First auth is undetermined, then rights decrease until the last entry, biggest entry num has preference always
enum class MeshAccessAuthorization : u8 {
    UNDETERMINED = 0, //Packet was not checked by any module
    WHITELIST = 1, //Packet was whitelisted by a module
    LOCAL_ONLY = 2, //Packet must only be processed by the receiving node and not by the mesh
    BLACKLIST = 3, //Packet was blacklisted by a module (This always wins over whitelisted)
};

//Led mode that defines what the LED does (mainly for debugging)
enum class LedMode : u8 {
    OFF = 0, // Led is off
    ON = 1, // Led is constantly on
    CONNECTIONS = 2, // Led blinks red if not connected and green for the number of connections
    RADIO_DEPRECATED = 3, // deprecated since 26/03/2021 (not implemented)
    CLUSTERING_DEPRECATED = 4, // deprecated 26/03/2021 (not implemented)
    ASSET_DEPRECATED = 5, // deprecated 26/03/2021 (not implemented)
    CUSTOM = 6, // Led controlled by a specific module
};

// Identification mode defines LED behaviour or vendor specific behaviour
enum class IdentificationMode : u8 {
    IDENTIFICATION_START = 1,
    IDENTIFICATION_STOP = 2,
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

enum class NO_DISCARD ErrorType : u32
{
    SUCCESS = 0,  ///< Successful command
    SVC_HANDLER_MISSING = 1,  ///< SVC handler is missing
    BLE_STACK_NOT_ENABLED = 2,  ///< Ble stack has not been enabled
    INTERNAL = 3,  ///< Internal Error
    NO_MEM = 4,  ///< No Memory for operation
    NOT_FOUND = 5,  ///< Not found
    NOT_SUPPORTED = 6,  ///< Not supported
    INVALID_PARAM = 7,  ///< Invalid Parameter
    INVALID_STATE = 8,  ///< Invalid state, operation disallowed in this state
    INVALID_LENGTH = 9,  ///< Invalid Length
    INVALID_FLAGS = 10, ///< Invalid Flags
    INVALID_DATA = 11, ///< Invalid Data
    DATA_SIZE = 12, ///< Data size exceeds limit
    TIMEOUT = 13, ///< Operation timed out
    NULL_ERROR = 14, ///< Null Pointer
    FORBIDDEN = 15, ///< Forbidden Operation
    INVALID_ADDR = 16, ///< Bad Memory Address
    BUSY = 17, ///< Busy
    CONN_COUNT = 18, ///< Connection Count exceeded
    RESOURCES = 19, ///< Not enough resources for operation
    UNKNOWN = 20,
    BLE_INVALID_CONN_HANDLE = 101,
    BLE_INVALID_ATTR_HANDLE = 102,
    BLE_NO_TX_PACKETS = 103,
    BLE_INVALID_ROLE = 104,
    BLE_INVALID_ATTR_TYPE = 105,
    BLE_SYS_ATTR_MISSING = 106,
    BLE_INVALID_BLE_ADDR = 107,
};

enum class ErrorTypeUnchecked : std::underlying_type<ErrorType>::type {
    SUCCESS =                 (u32)ErrorType::SUCCESS,
    SVC_HANDLER_MISSING =     (u32)ErrorType::SVC_HANDLER_MISSING,
    BLE_STACK_NOT_ENABLED =   (u32)ErrorType::BLE_STACK_NOT_ENABLED,
    INTERNAL =                (u32)ErrorType::INTERNAL,
    NO_MEM =                  (u32)ErrorType::NO_MEM,
    NOT_FOUND =               (u32)ErrorType::NOT_FOUND,
    NOT_SUPPORTED =           (u32)ErrorType::NOT_SUPPORTED,
    INVALID_PARAM =           (u32)ErrorType::INVALID_PARAM,
    INVALID_STATE =           (u32)ErrorType::INVALID_STATE,
    INVALID_LENGTH =          (u32)ErrorType::INVALID_LENGTH,
    INVALID_FLAGS =           (u32)ErrorType::INVALID_FLAGS,
    INVALID_DATA =            (u32)ErrorType::INVALID_DATA,
    DATA_SIZE =               (u32)ErrorType::DATA_SIZE,
    TIMEOUT =                 (u32)ErrorType::TIMEOUT,
    NULL_ERROR =              (u32)ErrorType::NULL_ERROR,
    FORBIDDEN =               (u32)ErrorType::FORBIDDEN,
    INVALID_ADDR =            (u32)ErrorType::INVALID_ADDR,
    BUSY =                    (u32)ErrorType::BUSY,
    CONN_COUNT =              (u32)ErrorType::CONN_COUNT,
    RESOURCES =               (u32)ErrorType::RESOURCES,
    UNKNOWN =                 (u32)ErrorType::UNKNOWN,
    BLE_INVALID_CONN_HANDLE = (u32)ErrorType::BLE_INVALID_CONN_HANDLE,
    BLE_INVALID_ATTR_HANDLE = (u32)ErrorType::BLE_INVALID_ATTR_HANDLE,
    BLE_NO_TX_PACKETS =       (u32)ErrorType::BLE_NO_TX_PACKETS,
    BLE_INVALID_ROLE =        (u32)ErrorType::BLE_INVALID_ROLE,
    BLE_INVALID_ATTR_TYPE =   (u32)ErrorType::BLE_INVALID_ATTR_TYPE,
    BLE_SYS_ATTR_MISSING =    (u32)ErrorType::BLE_SYS_ATTR_MISSING,
    BLE_INVALID_BLE_ADDR =    (u32)ErrorType::BLE_INVALID_BLE_ADDR,
};

struct DeviceConfiguration {
    u32 magicNumber;           // must be set to 0xF07700 when UICR data is available
    u32 boardType;             // accepts an integer that defines the hardware board that fruitymesh should be running on
    u32 serialNumber[2];       // Deprecated (since 12.05.2020), should be set to FFF...FFF and is now generated from the serialNumberIndex (2 words)
    u32 nodeKey[4];            // randomly generated (4 words)
    u32 manufacturerId;        // set to manufacturer id according to the BLE company identifiers: https://www.bluetooth.org/en-us/specification/assigned-numbers/company-identifiers
    u32 defaultNetworkId;      // network id if preenrollment should be used
    u32 defaultNodeId;         // node id to be used if not enrolled
    u32 deviceType;            // type of device (sink, mobile, etc,..)
    u32 serialNumberIndex;     // unique index that represents the serial number
    u32 networkKey[4];         // default network key if preenrollment should be used (4 words)
};

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
