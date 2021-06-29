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
#pragma once
#include <FmTypes.h>
#include <GlobalState.h>
#include <queue>
#include <map>
#include <array>
#include <string>
#include "MersenneTwister.h"
#include "json.hpp"
#include "MoveAnimation.h"
#if IS_ACTIVE(CLC_MODULE)
#include "ClcMock.h"
#endif //ACTIVATE_CLC_MODULE

extern "C" {
#include <ble_hci.h>
}

#include <FruityHal.h>

//Making the instance available to softdevice calls and others
class CherrySim;
extern CherrySim* cherrySimInstance;

constexpr int SIM_EVT_QUEUE_SIZE = 50;
constexpr int SIM_MAX_CONNECTION_NUM = 10; //Maximum total num of connections supported by the simulator

constexpr int SIM_NUM_RELIABLE_BUFFERS   = 1;
constexpr int SIM_NUM_UNRELIABLE_BUFFERS = 7;

constexpr int SIM_NUM_SERVICES = 6;
constexpr int SIM_NUM_CHARS    = 5;

constexpr int PACKET_STAT_SIZE = 10*1024;

#define PSRNG(prob) (cherrySimInstance->simState.rnd.NextPsrng((prob)))
#define PSRNGINT(min, max) ((u32)cherrySimInstance->simState.rnd.NextU32(min, max)) //Generates random int from min (inclusive) up to max (inclusive)

//A BLE Event that is sent by the Simulator is wrapped
struct simBleEvent {
    ble_evt_t bleEvent;
    // The overflow area for ble_evt_t, as sizeof(ble_evt_t) does not include
    // the memory used for storing write data. The ble_evt_t is used more like
    // an event header with additional data written directly after it.
    // Use the maximum MTU as the size of the overflow area.
    // IMPORTANT: This must always be the member directly after the ble_evt_t
    //            member!
    u8 bleEventOverflowData[NRF_SDH_BLE_GATT_MAX_MTU_SIZE];

    u32 size;
    u32 globalId;
    u32 additionalInfo; //Can be used to store a pointer or other information
};


//A packet that is buffered in the SoftDevice for sending
struct NodeEntry;
struct SoftDeviceBufferedPacket {
    NodeEntry* sender;
    NodeEntry* receiver;
    u16 connHandle;
    u32 globalPacketId;
    u32 queueTimeMs;
    union
    {
        ble_gattc_write_params_t writeParams;
        ble_gatts_hvx_params_t   hvxParams;
    }params;
    bool isHvx;
    uint8_t data[NRF_SDH_BLE_GATT_MAX_MTU_SIZE];

};

#pragma pack(push, 1)
struct PacketStat {
    MessageType messageType = MessageType::INVALID;
    ModuleIdWrapper moduleId = INVALID_WRAPPED_MODULE_ID;
    u8 actionType = 0;
    u8 isSplit = 0;
    u8 requestHandle = 0;
    u32 count = 0;
};
constexpr int packetStatCompareBytes = sizeof(PacketStat) - sizeof(u32);
static_assert(sizeof(PacketStat) == 12);
#pragma pack(pop)


//Simulator ble connection representation
struct SoftdeviceConnection {
    int connectionIndex = 0;
    int connectionHandle = 0;
    bool connectionActive = false;
    bool connectionEncrypted = false;
    bool rssiMeasurementActive = false;
    NodeEntry* owningNode = nullptr;
    NodeEntry* partner = nullptr;
    struct SoftdeviceConnection* partnerConnection = nullptr;
    int connectionInterval = 0;
    int connectionMtu = 0;
    u32 connectionSupervisionTimeoutMs = 0;
    bool isCentral = false;
    u32 lastReceivedPacketTimestampMs = 0;
    u32 connectionSetupTimeMs = 0;
    u32 lastConnectionTimestampMs = 0;

    SoftDeviceBufferedPacket reliableBuffers[SIM_NUM_RELIABLE_BUFFERS] = {};
    SoftDeviceBufferedPacket unreliableBuffers[SIM_NUM_UNRELIABLE_BUFFERS] = {};

    //Clustering validity
    i16 validityClusterSizeToSend;

    // Connection Parameter Update (only used when isCentral == true)
    bool connParamUpdateRequestPending = false;
    u32 connParamUpdateRequestTimeoutDs = 0;
    FruityHal::BleGapConnParams connParamUpdateRequestParameters = {};
};

struct CharacteristicDB_t
{
    ble_uuid_t  uuid = {};
    uint16_t    handle = 0;
    uint16_t    cccd_handle = 0;
};

struct ServiceDB_t
{
    ble_uuid_t          uuid = {};
    uint16_t            handle = 0;
    int                 charCount = 0;
    CharacteristicDB_t  charateristics[SIM_NUM_CHARS];
};

//The state of a SoftDevice
struct SoftdeviceState {
    //Softdevice / Generic
    bool initialized = false;
    u32 timeMs = 0;
    i8 txPower = 0;

    //Advertising
    bool advertisingActive = false;
    int advertisingIntervalMs = 0;
    FruityHal::BleGapAdvType advertisingType = FruityHal::BleGapAdvType::ADV_IND;
    u8 advertisingData[40] = {};
    u8 advertisingDataLength = 0;

    //Scanning
    bool scanningActive = false;
    int scanIntervalMs = 0;
    int scanWindowMs = 0;

    //Connecting
    bool connectingActive = false;
    int connectingStartTimeMs = 0;
    FruityHal::BleGapAddr connectingPartnerAddr;
    int connectingIntervalMs = 0;
    int connectingWindowMs = 0;
    int connectingTimeoutTimestampMs = 0;

    //Connection
    int connectionParamIntervalMs = 0;
    int connectionTimeoutMs = 0;

    //Connecting security
    u8 currentLtkForEstablishingSecurity[16] = {}; //The Long Term key used to initiate the last encryption request for a connection

    //Connections
    SoftdeviceConnection connections[SIM_MAX_CONNECTION_NUM];

    //Flash Access
    u32 numWaitingFlashOperations = 0;

    //Service Disovery
    u16         connHandle = 0; //Service discovery can only run for one connHandle at a time
    ble_uuid_t  uuid = {}; //Service uuid for the service currently being discovered
    u32         discoveryDoneTime = 0; //Time after which service discovery should be done for that node in the simulator

    int         servicesCount = 0; //Amount of services registered in the SoftDevice
    ServiceDB_t services[SIM_NUM_SERVICES];

    //UART
    NRF_UART_Type uartType;
    std::array<char, 1024> uartBuffer;
    u32 uartReadIndex = 0;
    u32 uartBufferLength = 0;

    uint32_t currentlyEnabledUartInterrupts = 0;

    //Memory configuration
    u8 configuredPeripheralConnectionCount = 0;
    u8 configuredCentralConnectionCount = 0;
    u8 configuredTotalConnectionCount = 0;

    //Clustering validity
    ClusterSize validityClusterSize;

};

struct InterruptSettings
{
    bool isEnabled                       = false;
    nrf_drv_gpiote_evt_handler_t handler = nullptr;
};

struct FeaturesetPointers;

struct NodeEntry {
    u32 index;
    int id;
    float x = 0;
    float y = 0;
    float z = 0;
    std::string nodeConfiguration = "";
    FeaturesetPointers* featuresetPointers = nullptr;
    FruityHal::BleGapAddr address;
    GlobalState gs;
#if IS_ACTIVE(CLC_MODULE)
    ClcMock clcMock;
#endif //ACTIVATE_CLC_MODULE
    NRF_FICR_Type ficr;
    NRF_UICR_Type uicr;
    NRF_GPIO_Type gpio;
    NRF_RADIO_Type radio;
    u8 flash[SIM_MAX_FLASH_SIZE];
    SoftdeviceState state;
    std::deque<simBleEvent> eventQueue;
    simBleEvent currentEvent; //The event currently being processed, as a simBleEvent, this can have some additional data attached to it useful for debugging
    bool led1On = false;
    bool led2On = false;
    bool led3On = false;
    u32 nanoAmperePerMsTotal;
    u8 *moduleMemoryBlock = nullptr;

    uint32_t restartCounter = 0; //Counts how many times the node was restarted
    int64_t simulatedFrames = 0;
    u32 watchdogTimeout = 0; //After how many simulated unfeed ms the watchdog should kill the node.
    u32 lastWatchdogFeedTime = 0; //The timestamp at which the watchdog was fed last.
    RebootReason rebootReason = RebootReason::UNKNOWN;

    std::vector<int> impossibleConnection; //The rssi to these nodes is artificially increased to an unconnectable level.

    std::map<u32, InterruptSettings> gpioInitializedPins; // Map from pin to settings
    std::queue<u32> interruptQueue;

    bool bmgWasInit          = false;
    bool twiWasInit          = false;
    bool Tlv49dA1b6WasInit   = false;
    bool spiWasInit          = false;
    bool lis2dh12WasInit     = false;
    bool bme280WasInit       = false;
    bool discoveryAlwaysBusy = false;

    u32 lastMovementSimTimeMs = 0;

    u32 fakeDfuVersion = 0;
    bool fakeDfuVersionArmed = false;

    //BLE Stack limits and config
    BleStackType bleStackType;
    u8 bleStackMaxTotalConnections;
    u8 bleStackMaxPeripheralConnections;
    u8 bleStackMaxCentralConnections;

    //Statistics
    PacketStat sentPackets[PACKET_STAT_SIZE];
    PacketStat routedPackets[PACKET_STAT_SIZE];

    MoveAnimation animation;

    // Timeslot simulation
    nrf_radio_signal_callback_t timeslotRadioSignalCallback = nullptr;
    bool timeslotCloseSessionRequested = false;
    bool timeslotRequested = false;
    bool timeslotActive = false;
};


struct SimulatorState {
    u32 simTimeMs = 0;
    MersenneTwister rnd;
    u16 globalConnHandleCounter = 0;
    u32 globalEventIdCounter = 0;
    u32 globalPacketIdCounter = 0;
};

struct SimConfiguration {
    // CAREFUL!
    // If you change anything in this type, including adding new members,
    // make sure that they are properly translated inside of the to_json
    // and from_json functions below!

    std::map<std::string, int> nodeConfigName;
    uint32_t    seed                               = 0;
    uint32_t    mapWidthInMeters                   = 0;
    uint32_t    mapHeightInMeters                  = 0;
    uint32_t    mapElevationInMeters               = 0;
    uint32_t    simTickDurationMs                  = 0;
    int32_t     terminalId                         = 0; //Enter -1 to disable, 0 for all nodes, or a specific id
    int32_t     simOtherDelay                      = 0; // Enter 1 - 100000 to send sim_other message only each ... simulation steps, this increases the speed significantly
    int32_t     playDelay                          = 0; //Allows us to view the simulation slower than simulated, is added after each step
    uint32_t    interruptProbability               = 0; // The probability that a queued interrupt is simulated.
    uint32_t    connectionTimeoutProbabilityPerSec = 0; // UINT32_MAX * 0.00001; //Simulates a connection timout around every minute
    uint32_t    sdBleGapAdvDataSetFailProbability  = 0; // UINT32_MAX * 0.0001; //Simulate fails on setting adv Data
    uint32_t    sdBusyProbability                  = 0; // UINT32_MAX * 0.0001; //Simulates getting back busy errors from softdevice
    uint32_t    sdBusyProbabilityUnlikely          = 0; // UINT32_MAX * 0.0001; //Simulates getting back busy errors from softdevice for methods where it is very unlikely to get a BUSY error
    bool        simulateAsyncFlash                 = false;
    uint32_t    asyncFlashCommitTimeProbability    = 0; // 0 - UINT32_MAX where UINT32_MAX is instant commit in the next simulation step
    bool        importFromJson                     = false; //Set to true and specify siteJsonPath and devicesJsonPath to read a scenario from json
    bool        realTime                           = false; //If set to true, the simulator will only tick when the real time clock passed the necessary time. On false: As fast as possible.
    uint32_t    receptionProbabilityVeryClose      = UINT32_MAX / 10 * 9;
    uint32_t    receptionProbabilityClose          = UINT32_MAX / 10 * 8;
    uint32_t    receptionProbabilityFar            = UINT32_MAX / 10 * 5;
    uint32_t    receptionProbabilityVeryFar        = UINT32_MAX / 10 * 3;
    std::string siteJsonPath                       = "";
    std::string devicesJsonPath                    = "";
    std::string replayPath                         = ""; //If set, a replay is loaded from this path.
    bool        logReplayCommands                  = false; //If set, lines are logged out that can be used as input for the replay feature.
    bool        useLogAccumulator                  = false; //If set, all logs are written to CherrySim::logAccumulator
    u32         defaultNetworkId                   = 0;
    std::vector<std::pair<double, double>> preDefinedPositions;
    bool        rssiNoise                          = false;
    bool        simulateWatchdog                   = false;
    bool        simulateJittering                  = false;
    bool        verbose                            = false;
    uint32_t    fastLaneToSimTimeMs                = 0; //Set to a value bigger than 0 to speed up the simulation until this simulation time was reached (disables native rendering & terminal in the meantime)

    bool        enableClusteringValidityCheck      = false; //Enable automatic checking of the clustering after each step
    bool        enableSimStatistics                = false;
    std::string storeFlashToFile                   = "";

    bool        verboseCommands                    = false;


    //BLE Stack capabilities
    BleStackType defaultBleStackType          = BleStackType::INVALID;

    void SetToPerfectConditions();
};

//These two functions must be named exactly like they are! They are used by nlohmann::json.
void to_json(nlohmann::json& j, const SimConfiguration& config);
void from_json(const nlohmann::json& j, SimConfiguration& config);

//Notifies other classes of events happening in the simulator, e.g. node reset
class CherrySimEventListener {
public:
    CherrySimEventListener() {};
    virtual ~CherrySimEventListener() {};

    virtual void CherrySimEventHandler(const char* eventType) = 0;
    virtual void CherrySimBleEventHandler(NodeEntry* currentNode, simBleEvent* simBleEvent, u16 eventSize) = 0;
};

//Notifies of Terminal Output
class TerminalPrintListener {
public:
    TerminalPrintListener() {};
    virtual ~TerminalPrintListener() {};

    //This method can be implemented by any subclass and will be notified when
    //a command is entered via uart.
    virtual void TerminalPrintHandler(NodeEntry* currentNode, const char* message) = 0;

};
