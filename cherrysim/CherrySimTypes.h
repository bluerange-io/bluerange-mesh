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
#pragma once
#include <types.h>
#include <GlobalState.h>
#include <queue>
#include "SimpleArray.h"
#include "MersenneTwister.h"
#ifndef GITHUB_RELEASE
#include "ClcMock.h"
#endif //GITHUB_RELEASE

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

constexpr int PACKET_STAT_SIZE = 2*1024;

#define PSRNG() (cherrySimInstance->simState.rnd.nextDouble())
#define PSRNGINT(min, max) ((u32)cherrySimInstance->simState.rnd.nextU32(min, max)) //Generates random int from min (inclusive) up to max (inclusive)

//A BLE Event that is sent by the Simulator is wrapped
typedef struct {
	ble_evt_t bleEvent;
	u8 data[GATT_MTU_SIZE_DEFAULT]; //overflow area for ble_evt_t as sizeof(ble_evt_t) does not include write data, this must be added using the MTU
	u32 size;
	u32 globalId;
	u32 additionalInfo; //Can be used to store a pointer or other information
} simBleEvent;


//A packet that is buffered in the SoftDevice for sending
struct nodeEntry;
typedef struct {
	nodeEntry* sender;
	nodeEntry* receiver;
	u16 connHandle;
	u32 globalPacketId;
	u32 queueTimeMs;
	union
	{
		ble_gattc_write_params_t writeParams;
		ble_gatts_hvx_params_t   hvxParams;
	}params;
	bool isHvx;
	uint8_t data[30];

} SoftDeviceBufferedPacket;

constexpr int packetStatCompareBytes = 4;
struct PacketStat {
	MessageType messageType = MessageType::INVALID;
	ModuleId moduleId = ModuleId::INVALID_MODULE;
	u8 actionType = 0;
	u8 isSplit = 0;
	u32 count = 0;
};


//Simulator ble connection representation
typedef struct SoftdeviceConnection {
	int connectionIndex = 0;
	int connectionHandle = 0;
	bool connectionActive = false;
	bool connectionEncrypted = false;
	bool rssiMeasurementActive = false;
	nodeEntry* owningNode = nullptr;
	nodeEntry* partner = nullptr;
	struct SoftdeviceConnection* partnerConnection = nullptr;
	int connectionInterval = 0;
	int connectionMtu = 0;
	bool isCentral = false;

	SoftDeviceBufferedPacket reliableBuffers[SIM_NUM_RELIABLE_BUFFERS] = { 0 };
	SoftDeviceBufferedPacket unreliableBuffers[SIM_NUM_UNRELIABLE_BUFFERS] = { 0 };

	//Clustering validity
	i16 validityClusterSizeToSend;

} SoftdeviceConnection;

typedef struct
{
	ble_uuid_t  uuid = { 0 };
	uint16_t    handle = 0;
	uint16_t    cccd_handle = 0;
}CharacteristicDB_t;

typedef struct
{
	ble_uuid_t          uuid = { 0 };
	uint16_t            handle = 0;
	int                 charCount = 0;
	CharacteristicDB_t  charateristics[SIM_NUM_CHARS];
} ServiceDB_t;

//The state of a SoftDevice
typedef struct {
	//Softdevice / Generic
	bool initialized = false;
	int timeMs = 0;
	i8 txPower = 0;

	//Advertising
	bool advertisingActive = false;
	int advertisingIntervalMs = 0;
	FruityHal::BleGapAdvType advertisingType = FruityHal::BleGapAdvType::ADV_IND;
	u8 advertisingData[40] = { 0 };
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
	int connectingParamIntervalMs = 0;

	//Connecting security
	u8 currentLtkForEstablishingSecurity[16] = { 0 }; //The Long Term key used to initiate the last encryption request for a connection

	//Connections
	SoftdeviceConnection connections[SIM_MAX_CONNECTION_NUM];

	//Flash Access
	u32 numWaitingFlashOperations = 0;

	//Service Disovery
	u16         connHandle = 0; //Service discovery can only run for one connHandle at a time
	ble_uuid_t  uuid = { 0 }; //Service uuid for the service currently being discovered
	u32         discoveryDoneTime = 0; //Time after which service discovery should be done for that node in the simulator

	int         servicesCount = 0; //Amount of services registered in the SoftDevice
	ServiceDB_t services[SIM_NUM_SERVICES];

	//UART
	NRF_UART_Type uartType;
	SimpleArray<char, 1024> uartBuffer;
	int uartReadIndex = 0;
	int uartBufferLength = 0;

	uint32_t currentlyEnabledUartInterrupts = 0;

	//Memory configuration
	u8 configuredPeripheralConnectionCount = 0;
	u8 configuredCentralConnectionCount = 0;
	u8 configuredTotalConnectionCount = 0;

	//Clustering validity
	i16 validityClusterSize;

} SoftdeviceState;

typedef struct nodeEntry {
	int index;
	int id;
	float x = 0;
	float y = 0;
	float z = 0;
	char nodeConfiguration[50] = { 0 };
	FruityHal::BleGapAddr address;
	GlobalState gs;
#ifndef GITHUB_RELEASE
	ClcMock clcMock;
#endif //GITHUB_RELEASE
	NRF_FICR_Type ficr;
	NRF_UICR_Type uicr;
	NRF_GPIO_Type gpio;
	u8 flash[SIM_MAX_FLASH_SIZE];
	SoftdeviceState state;
	std::deque<simBleEvent> eventQueue;
	simBleEvent currentEvent; //The event currently being processed, as a simBleEvent, this can have some additional data attached to it useful for debugging
	bool ledOn;
	u32 nanoAmperePerMsTotal;
	u8 *moduleMemoryBlock = nullptr;

	uint32_t restartCounter = 0; //Counts how many times the node was restarted
	int64_t simulatedFrames = 0;
	unsigned long long watchdogTimeout = 0; //After how many simulated unfeed ms the watchdog should kill the node.
	int lastWatchdogFeedTime = 0; //The timestamp at which the watchdog was fed last.
	RebootReason rebootReason = RebootReason::UNKNOWN;

	std::vector<int> impossibleConnection; //The rssi to these nodes is artificially increased to an unconnectable level.

	bool bmgWasInit        = false;
	bool twiWasInit        = false;
	bool Tlv49dA1b6WasInit = false;
	bool spiWasInit        = false;
	bool lis2dh12WasInit   = false;
	bool bme280WasInit     = false;

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

} nodeEntry;


typedef struct {
	u32 simTimeMs = 0;
	MersenneTwister rnd;
	u16 globalConnHandleCounter = 0;
	u32 globalEventIdCounter = 0;
	u32 globalPacketIdCounter = 0;
} SimulatorState;

struct SimConfiguration {
	uint32_t numNodes                         = 0;
	uint32_t numAssetNodes                    = 0;
	uint32_t seed                             = 0;
	uint32_t mapWidthInMeters                 = 0;
	uint32_t mapHeightInMeters                = 0;
	uint32_t simTickDurationMs                = 0;
	int32_t terminalId                        = 0; //Enter -1 to disable, 0 for all nodes, or a specific id
	int32_t simOtherDelay                     = 0; // Enter 1 - 100000 to send sim_other message only each ... simulation steps, this increases the speed significantly
	int32_t playDelay                         = 0; //Allows us to view the simulation slower than simulated, is added after each step
	double connectionTimeoutProbabilityPerSec = 0; //Every minute or so: 0.00001;
	double sdBleGapAdvDataSetFailProbability  = 0;// 0.0001; //Simulate fails on setting adv Data
	double sdBusyProbability                  = 0; // 0.0001; //Simulates getting back busy errors from softdevice
	bool simulateAsyncFlash                   = false;
	double asyncFlashCommitTimeProbability    = 0; // 0.0 - 1.0 where 1 is instant commit in the next simulation step
	bool importFromJson                       = false; //Set to true and specify siteJsonPath and devicesJsonPath to read a scenario from json
	char siteJsonPath[100]                    = {};
	char devicesJsonPath[100]                 = {};
	char defaultNodeConfigName[50]            = {};
	char defaultSinkConfigName[50]            = {};
	u32 defaultNetworkId                      = 0;
	std::vector<std::pair<double, double>> preDefinedPositions;
	bool rssiNoise                            = false;
	bool simulateWatchdog                     = false;
	bool simulateJittering                    = false;
	bool verbose                              = false;

	bool enableClusteringValidityCheck        = false; //Enable automatic checking of the clustering after each step
	bool enableSimStatistics                  = false;
	const char* storeFlashToFile              = nullptr;

	bool verboseCommands                      = false;


	//BLE Stack capabilities
	BleStackType defaultBleStackType          = BleStackType::INVALID;
};

//Notifies other classes of events happening in the simulator, e.g. node reset
class CherrySimEventListener {
public:
	CherrySimEventListener() {};
	virtual ~CherrySimEventListener() {};

	virtual void CherrySimEventHandler(const char* eventType) = 0;
	virtual void CherrySimBleEventHandler(nodeEntry* currentNode, simBleEvent* simBleEvent, u16 eventSize) = 0;
};

//Notifies of Terminal Output
class TerminalPrintListener {
public:
	TerminalPrintListener() {};
	virtual ~TerminalPrintListener() {};

	//This method can be implemented by any subclass and will be notified when
	//a command is entered via uart.
	virtual void TerminalPrintHandler(nodeEntry* currentNode, const char* message) = 0;

};
