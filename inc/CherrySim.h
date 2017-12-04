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
 * This is the main class that initializes the SoftDevice and starts the code.
 * It contains error handlers for all unfetched errors.
 */

#pragma once

#ifdef SIM_ENABLED
#include <types.h>
#include <GlobalState.h>
#include <FruitySimPipe.h>
#include <Terminal.h>
#include <LedWrapper.h>
#include <queue>
#include <random>

extern "C"{
#include <ble.h>
#include <ble_hci.h>
}

typedef struct {
	ble_evt_t bleEvent;
	u32 size;
	u32 globalId;

} simBleEvent;

#define SIM_EVT_QUEUE_SIZE 50
#define SIM_CONNECTION_NUM 4

#define SIM_NUM_RELIABLE_BUFFERS 1
#define SIM_NUM_UNRELIABLE_BUFFERS 7

struct nodeEntry; //Forward declaration
typedef struct {
	nodeEntry* sender;
	nodeEntry* receiver;
	u16 connHandle;
	ble_gattc_write_params_t writeParams;
	uint8_t data[30];

} SoftDeviceBufferedPacket;

typedef struct SoftdeviceConnection {
	int connectionIndex = 0;
	int connectionHandle = 0;
	bool connectionActive = false;
	bool rssiMeasurementActive = false;
	nodeEntry* partner = NULL;
	struct SoftdeviceConnection* partnerConnection = NULL;
	int connectionInterval = 0;

	SoftDeviceBufferedPacket reliableBuffers[SIM_NUM_RELIABLE_BUFFERS];
	SoftDeviceBufferedPacket unreliableBuffers[SIM_NUM_UNRELIABLE_BUFFERS];

} SoftdeviceConnection;

typedef struct {
	//Physical stuff
	float x = 0;
	float y = 0;

	bool initialized = false;

	int timeOffsetMs = 0;
	int timeMs = 0;

	i8 txPower = 0;

	//Advertising
	bool advertisingActive = false;
	int advertisingIntervalMs = 0;
	u8 advertisingType = 0;
	u8 advertisingData[40] = { 0 };
	u8 advertisingDataLength = 0;

	ble_radio_notification_evt_handler_t radioNotificationHandler = NULL;

	//Scanning
	bool scanningActive = false;
	int scanIntervalMs = 0;
	int scanWindowMs = 0;

	//Connecting
	bool connectingActive = false;
	int connectingStartTimeMs = 0;
	fh_ble_gap_addr_t connectingPartnerAddr;
	int connectingIntervalMs = 0;
	int connectingWindowMs = 0;
	int connectingTimeoutTimestampMs = 0;
	int connectingParamIntervalMs = 0;

	//Connections
	SoftdeviceConnection connections[SIM_CONNECTION_NUM];

} SoftdeviceState;

typedef struct nodeEntry {
	int index;
	int id;
	fh_ble_gap_addr_t address;
	GlobalState gs;
	NRF_FICR_Type ficr;
	NRF_UICR_Type uicr;
	NRF_GPIO_Type gpio;
	NRF_GPIO_Type gpioBackup;
	u8 flash[SIM_PAGES*SIM_PAGE_SIZE];
	SoftdeviceState state;
	std::queue<simBleEvent>* eventQueue;
	bool ledOn;
	char terminalInput[PIPE_BUFFER_SIZE];
	u32 numWaitingFlashOperations;

} nodeEntry;

//Make this publicly available
nodeEntry* currentNode;

typedef struct {
	u32 numNodes;
	u32 simTickDurationMs;
	int mapWidthMeters;
	int mapHeightMeters;
	u32 simTimeMs;
	std::uniform_real_distribution<double> dist;
	std::mt19937 rnd;
	u16 globalConnHandleCounter;
	u32 globalEventIdCounter;
} SimulatorState;

#define PSRNG() simState.dist(simState.rnd)

class CherrySim : public TerminalCommandListener
{
	public:
		CherrySim();
		void init();
		void flashNode(u32 i);
		void start();

		void configureNode();

		void PrepareForSimRestart();

		void registerSimulatorTerminalHandler();

		void OutputSimDebugInfo();

		bool isSimPipeEnabled();

		void setNode(int i);
		void ChooseSimulatorTerminal();
		void initCurrentNode();
		void bootCurrentNode();
		void simNodeStepHandler();
		void simulateRadioNotifications();
		void simulateTimer();
		void simulateTimeouts();
		void simulateBroadcast();
		void ConnectMasterToSlave(nodeEntry * master, nodeEntry* slave);
		u32 DisconnectSimulatorConnection(SoftdeviceConnection * connection, u32 hciReason, u32 hciReasonPartner);
		float RssiToDistance(int rssi, int calibratedRssi);
		float GetDistanceBetween(nodeEntry * nodeA, nodeEntry * nodeB);
		float GetReceptionRssi(nodeEntry * sender, nodeEntry * receiver);
		void SimulateConnections();
		void SimulateGPIO();
		void simulateCurrentNode();

		void PacketHandler(u32 senderId, u32 receiverId, u8* data, u32 dataLength);

		double calculateReceptionProbability(nodeEntry* sendingNode, nodeEntry* receivingNode);

		fh_ble_gap_addr_t GetGapAddress(nodeEntry * node);

		void SendCurrentAdvertisingData();


		void GenerateWrite(nodeEntry * sender, nodeEntry * receiver, uint16_t conn_handle, const ble_gattc_write_params_t * p_write_params);

		
	private:


		// Inherited via TerminalCommandListener
		virtual bool TerminalCommandHandler(std::string commandName, std::vector<std::string> commandArgs);

		void dumpSimulatorStateToFile();

		void resetSimulator();

};

SimulatorState simState;

void setSimLed(bool state);

void bleDispatchEventHandler(ble_evt_t * p_ble_evt);
void sysDispatchEventHandler(u32 sys_evt);

int app_main();

void detectBoardAndSetConfig(void);
void bleInit(void);
u32 initNodeID(void);

void initTimers(void);

void timerEventDispatch(u16 passedTime, u32 appTimer);
void dispatchUartInterrupt();

void initGpioteButtons();
void buttonInterruptHandler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action);
void dispatchButtonEvents(u8 buttonId, u32 buttonHoldTime);
void radioEventDispatcher(bool radioActive);

//These are the event handlers that are notified by the SoftDevice
//The events are then broadcasted throughout the application
extern "C"{
	static void ble_timer_dispatch(void * p_context);
	void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name);
	void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name);
	void sys_evt_dispatch(uint32_t sys_evt);
}



#endif
/** @} */
