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
#ifdef SIM_ENABLED
#include <CherrySim.h>
#include <FruitySimPipe.h>
#include <FruityHal.h>

#include <malloc.h>
#include <iostream>
#include <string>
#include <functional>
#include <json.hpp>
#include <fstream>


extern "C"{
#include <nrf_soc.h>
#include <app_error.h>
#include <softdevice_handler.h>
#include <app_timer.h>
//#include <nrf_gpio.h>
//#include <nrf_mbr.h>
#include <nrf_sdm.h>
//#include <nrf_delay.h>
//#include <nrf_nvic.h>
#include <aes.h>
}

#include <GlobalState.h>
#include <Node.h>
#include <MeshConnection.h>
#include <AppConnection.h>
#include <Terminal.h>
#include <AdvertisingController.h>
#include <ScanController.h>
#include <GAPController.h>
#include <IoModule.h>
#include <GATTController.h>
#include <Logger.h>
#include <Testing.h>
#include <LedWrapper.h>
#include <Module.h>
#include <Utility.h>
#include <types.h>
#include <Config.h>
#include <Boardconfig.h>
#include <NewStorage.h>
#include <ClcComm.h>

#include <TestBattery.h>
#include <TestRecordStorage.h>
#include <TestPacketQueue.h>

using json = nlohmann::json;


/**
 * TODO:
 * cleanup main.cpp
 * @return
 */

//###### Config ######
uint32_t numNodes = 50;
uint32_t seed = 9;
uint32_t mapWidthInMeters = 40;
uint32_t mapHeightInMeters = 30;
uint32_t simTickDurationMs = 50;
int32_t terminalId = 1; //Enter -1 to disable, 0 for all nodes, or a specific id

int32_t simOtherDelay = 10000; // Enter 1 - 100000 to send sim_other message only each ... simulation steps, this increases the speed significantly
int32_t playDelay = 0; //Allows us to view the simulation slower than simulated, is added after each step
int32_t disablePipeUntilSimTimeMs = -1; //Helps to get to a certain simulation time faster by not writing everything to the simpipe
bool sendAdvDataToPipe = false;
bool sendLedDataToPipe = true;

bool simulateConnectionsProperly = true;
double connectionTimeoutProbabilityPerSec = 0;// 0.00001; //Every minute or so: 0.00001;
double sdBleGapAdvDataSetFailProbability = 0;// 0.0001; //Simulate fails on setting adv Data
bool simulateAsyncFlash = true;

bool enableClusteringTest = false;

// Include (or do not) the service_changed characteristic.
// If not enabled, the server's database cannot be changed for the lifetime of the device
#define IS_SRVC_CHANGED_CHARACT_PRESENT 1
#define RADIO_NOTIFICATION_IRQ_PRIORITY 0

#define APP_TIMER_PRESCALER       0 // Value of the RTC1 PRESCALER register
#define APP_TIMER_MAX_TIMERS      1 //Maximum number of simultaneously created timers (2 + BSP_APP_TIMERS_NUMBER)
#define APP_TIMER_OP_QUEUE_SIZE   1 //Size of timer operation queues

//Holds the data of all nodes
#define MAX_NUM_NODES 1000
nodeEntry nodes[MAX_NUM_NODES];

nodeEntry nodesBackup[MAX_NUM_NODES];


std::queue<simBleEvent> eventQueues[MAX_NUM_NODES];


uint32_t __application_start_address;
uint32_t __application_end_address;


uint32_t __start_conn_type_resolvers;
uint32_t __stop_conn_type_resolvers;

ConnTypeResolver connTypeResolvers[] = {
	MeshConnection::ConnTypeResolver
};


CherrySim* cherrySimInstance = NULL;

bool shouldRestartSim = false;

int globalBreakCounter = 0;


#pragma warning( push )
#pragma warning( disable : 4996)
#pragma warning( disable : 4297)

int main(void){
	while (true) {
		printf("## SIM Starting ##");
		cherrySimInstance = new CherrySim();
		cherrySimInstance->init();
		cherrySimInstance->start();
	}
}

void testPrint() {
	for (u32 i = 0; i < simState.numNodes; i++) {
		for (u32 j = 0; j < SIM_CONNECTION_NUM; j++) {
			if (nodes[i].state.connections[j].connectionActive) {
				printf("Node %u connected with %u\n", nodes[i].id, nodes[i].state.connections[j].partner->id);
			}
		}
	}
}

SoftdeviceConnection* findConnectionByHandle(nodeEntry* node, int connectionHandle) {
	for (u32 i = 0; i < SIM_CONNECTION_NUM; i++) {
		if (node->state.connections[i].connectionActive && node->state.connections[i].connectionHandle == connectionHandle) {
			return &node->state.connections[i];
		}
	}
	return NULL;
}

nodeEntry* findNodeById(int id) {
	for (u32 i = 0; i < simState.numNodes; i++) {
		if (nodes[i].id == id) {
			return &nodes[i];
		}
	}
	return NULL;
}


CherrySim::CherrySim()
{
	
}

void CherrySim::init() {

	simState.simTimeMs = 0;
	simState.numNodes = numNodes;
	simState.simTickDurationMs = simTickDurationMs;
	simState.mapHeightMeters = mapHeightInMeters;
	simState.mapWidthMeters = mapWidthInMeters;
	simState.globalConnHandleCounter = 0;

	//When the sim is resetted, it is important to clear all event queues
	for (u32 i = 0; i < simState.numNodes; i++) {
		while (!eventQueues[i].empty()) eventQueues[i].pop();
		
	}

	//In case we want a random scenario each time
	//seed = rand();

	//Generate a psuedo random number generator with a uniform distribution
	simState.rnd = std::mt19937(seed);
	simState.dist = std::uniform_real_distribution<double>(0.0, 1.0);

	for (u32 i = 0; i<simState.numNodes; i++) {
		flashNode(i);
	}

	//Finally, make a backup of the node struct to be able to reset the simulator
	memcpy(nodesBackup, nodes, sizeof(nodes));


}

void CherrySim::flashNode(u32 i) {
	nodes[i].index = i;
	nodes[i].id = i + 1;

	//Set some random x and y position for the node
	nodes[i].state.x = (float)PSRNG();
	nodes[i].state.y = (float)PSRNG();

	//Initializes a random time offset for the node
	nodes[i].state.timeOffsetMs = ((int)(PSRNG() * 3)) * simState.simTickDurationMs;



	//Configure FICR
	nodes[i].ficr.CODESIZE = SIM_PAGES;
	nodes[i].ficr.CODEPAGESIZE = SIM_PAGE_SIZE;

	//Initialize UICR
	nodes[i].uicr.BOOTLOADERADDR = 0xFFFFFFFFUL;

	//Configure UICR
	nodes[i].uicr.CUSTOMER[0] = 0xF07700; //magicNumber
	nodes[i].uicr.CUSTOMER[1] = 0; //boardType
	sprintf((char*)(nodes[i].uicr.CUSTOMER + 2), "F%04u", i+1);
	//memcpy(nodes[i].uicr.CUSTOMER + 4, {0,1,2,3,4,5,6,7,8,9,1,2,3,4,5,6}, 16); //Networkkey
	nodes[i].uicr.CUSTOMER[8] = 0; //boardType
	nodes[i].uicr.CUSTOMER[9] = 10; //meshnetworkidentifier (do not put it in 1, as this is the enrollment network
	nodes[i].uicr.CUSTOMER[10] = i + 1; //defaultNodeId
	nodes[i].uicr.CUSTOMER[11] = 0; //deviceType
	nodes[i].uicr.CUSTOMER[12] = 567; //serialNumberIndex

	//Initialize flash memory
	memset(nodes[i].flash, 0xFF, sizeof(nodes[i].flash));
	//TODO: copy softdevice and some app data? Maybe some settings in flash

	//Generate device address based on the id
	nodes[i].address.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
	memset(&nodes[i].address.addr, 0x00, 6);
	*((u32*)&(nodes[i].address.addr[2])) = nodes[i].id;

	//Put some settings in place
	BoardConfiguration* boardConfig = (BoardConfiguration*)(nodes[i].flash + (254 * SIM_PAGE_SIZE));
	boardConfig->boardType = 0;
	boardConfig->calibratedTX = -63;
	boardConfig->led1Pin = 21;
	boardConfig->led2Pin = 22;
	boardConfig->led3Pin = 23;
	boardConfig->uartTXPin = 1;
	boardConfig->uartRXPin = 1;
	boardConfig->uartRTSPin = 1;
	boardConfig->uartCTSPin = 1;

	nodes[i].numWaitingFlashOperations = 0;

}

void CherrySim::start()
{

#ifdef USE_SIM_PIPE
	FruitySimPipe::ConnectPipe();
	FruitySimPipe::WriteToPipe("HELLO_FROM_SIM\n");
	FruitySimPipe::ReadFromPipe(); //Waits for HELLO_FROM_CLIENT

	FruitySimPipe::WriteToPipe("STEP_START\n");
#endif

	//Print sim_info json
	json j;
	j["type"] = "sim_info";
	j["numNodes"] = simState.numNodes;
	j["widthInMeters"] = simState.mapWidthMeters;
	j["heightInMeters"] = simState.mapHeightMeters;

	FruitySimPipe::WriteToPipeF("%s" EOL, j.dump().c_str());

	//Boot up all nodes
	for(u32 i=0; i<simState.numNodes; i++){
		//printf("##SIMSIMSIM## Initializing Node %u x:%f, y:%f ##SIMSIMSIM##\n", i, nodes[i].state.x, nodes[i].state.y);
		
		setNode(i);
		initCurrentNode();

		//Print sim_nodeinfo json
		json j;
		j["type"] = "sim_nodeinfo";
		j["nodeId"] = nodes[i].id;
		j["x"] = nodes[i].state.x;
		j["y"] = nodes[i].state.y;
		j["serial"] = (char*)(nodes[i].uicr.CUSTOMER + 2);
		printf("%s" EOL, j.dump().c_str());

		FruitySimPipe::WriteToPipeF("%s" EOL, j.dump().c_str());

		bootCurrentNode();

	}

	FruitySimPipe::WriteToPipe("STEP_END\n");

	simState.simTimeMs = 0;
	//Simulate all nodes
	while(true){
		//printf("##SIMSIMSIM## Simulating time %u ##SIMSIMSIM##\n", simState.simTimeMs);
		//testPrint();

		//Wait for message
#ifdef USE_SIM_PIPE

		if (isSimPipeEnabled()) {
			char* message = NULL;
			while (message == NULL || strcmp(message, "NEXT_STEP") != 0) {
				message = FruitySimPipe::ReadFromPipe(); //Waits for commands to node until NEXT_STEP is received

				if (message != NULL && strstr(message, "NODE_CMD") == message) {
					printf("%s", message);

					//TODO: zerlegen
				}
			}
		}

		//printf("STEP\n");
		if (isSimPipeEnabled()) {
			FruitySimPipe::WriteToPipe("STEP_START\n");
		}
#endif

		for(u32 i=0; i<simState.numNodes; i++){
			setNode(i);
			simulateRadioNotifications();
			simulateTimer();
			simulateTimeouts();
			simulateBroadcast();
			SimulateConnections();
			SimulateGPIO();
			try {
				simulateCurrentNode();
				simNodeStepHandler();
			}
			catch (const std::exception& e) {
				if (strcmp(e.what(), "NODE_RESET") != 0) {
					throw e;
				}
			}
			if(simOtherDelay == 0 || simState.simTimeMs % simOtherDelay == 0) OutputSimDebugInfo();
		}

#ifdef USE_SIM_PIPE
		if (isSimPipeEnabled()) {
			FruitySimPipe::WriteToPipe("STEP_END\n");
		}
#endif

		simState.simTimeMs += simState.simTickDurationMs;
		//Initialize RNG with new seed in order to be able to jump to a frame and resimulate it
		simState.rnd = std::mt19937(simState.simTimeMs);

		if (playDelay > 0) {
			_sleep(playDelay);
		}

		if (simState.simTimeMs % 100000 == 0) {
			//printf("simTimeMs: %u;", simState.simTimeMs);
		}

		if (shouldRestartSim) {
			PrepareForSimRestart();
			return;
		}
	}
}

u32 resetCounter = 0;
u32 nodesToReset = 0;
void CherrySim::simNodeStepHandler() {
	if (enableClusteringTest) {
		//Check if all nodes report the correct clusterSize
		bool clusteringDone = true;
		for (u32 i = 0; i < numNodes; i++) {
			if (nodes[i].gs.node->clusterSize != numNodes) {
				clusteringDone = false;
			}
		}

		//If clustering has finished, either restart simulator or calculate a random number of nodes 0-100% to be reset
		if (clusteringDone && nodesToReset == 0 && !shouldRestartSim) {
			resetCounter++;
			if (resetCounter == 5) {
				resetCounter = 0;
				seed++;
				resetSimulator();
			}
			else {
				nodesToReset = (u32)((numNodes - 1) * PSRNG() + 1);
				printf("Clustering Done, resetting %u nodes @%u\n", nodesToReset, simState.simTimeMs);
			}
		}

		//Reset the current node
		if (nodesToReset > 0 && PSRNG() < 0.01) {
			nodesToReset--;
			char cmd[] = "reset";
			Terminal::getInstance()->ProcessLine(cmd);
		}
	}
}

void CherrySim::configureNode() {
	Config->encryptionEnabled = false; //not yet supported
	Config->terminalMode = TerminalMode::TERMINAL_PROMPT_MODE;
	Config->defaultLedMode = ledMode::LED_MODE_OFF;
	char cmd[] = "set_active 0 status off";
	Terminal::getInstance()->ProcessLine(cmd);
	char cmd2[] = "action this io led off";
	Terminal::getInstance()->ProcessLine(cmd2);
	char cmd3[] = "set_active 0 scan off";
	Terminal::getInstance()->ProcessLine(cmd3);
	char cmd4[] = "set_active 0 clc on";
	Terminal::getInstance()->ProcessLine(cmd4);
	
}

void CherrySim::PrepareForSimRestart() {
	shouldRestartSim = false;
	FruitySimPipe::ClosePipe();
	//Copy our backup back to our nodes
	memcpy(nodes, nodesBackup, sizeof(nodes));
}

void CherrySim::registerSimulatorTerminalHandler() {
	Terminal::getInstance()->AddTerminalCommandListener(this);
}

void CherrySim::OutputSimDebugInfo() {
	//Output the 
	json j1;
	j1["type"] = "sim_other";
	j1["nodeId"] = currentNode->id;
	j1["clusterId"] = currentNode->gs.node->clusterId;
	j1["clusterSize"] = currentNode->gs.node->clusterSize;

	j1["inConnectionHasMasterBit"] = false;
	if (currentNode->state.connections[0].connectionActive) {
		for (int i = 0; i < MAX_NUM_MESH_CONNECTIONS; i++) {
			if (currentNode->gs.cm->allConnections[i] != NULL && currentNode->gs.cm->allConnections[i]->connectionType == ConnectionTypes::CONNECTION_TYPE_FRUITYMESH) {
				if (currentNode->gs.cm->allConnections[i]->direction == ConnectionDirection::CONNECTION_DIRECTION_IN) {
					j1["inConnectionHasMasterBit"] = ((MeshConnection*)currentNode->gs.cm->allConnections[i])->connectionMasterBit == 1;
				}
			}
		}
		nodeEntry* partnerNode = currentNode->state.connections[0].partner;
		bool hasMB = false;
		MeshConnections conn = partnerNode->gs.cm->GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_OUT);
		for (int i = 0; i < conn.count; i++) {
			if (conn.connections[i]->connectionHandle == currentNode->state.connections[0].connectionHandle) {
				hasMB = conn.connections[i]->connectionMasterBit;
			}
		}
		j1["inConnectionPartnerHasMasterBit"] = hasMB;
	}
	//printf("%s" EOL, j1.dump().c_str());
	if (isSimPipeEnabled()) {
		FruitySimPipe::WriteToPipeF("%s" EOL, j1.dump().c_str());
	}
}

bool CherrySim::isSimPipeEnabled() {
	return disablePipeUntilSimTimeMs < 0 || simState.simTimeMs > (u32)disablePipeUntilSimTimeMs;
}

//This sets some pointers that are used by the fruitymesh implementation to the current node
void CherrySim::setNode(int i)
{
	//printf("**SIM**: Setting node %u\n", i+1);
	currentNode = nodes + i;
	currentNode->eventQueue = eventQueues + i;
	myFicrPtr = &(nodes[i].ficr);
	myUicrPtr = &(nodes[i].uicr);
	myGpioPtr = &(nodes[i].gpio);
	myFlashPtr = nodes[i].flash;
	myGlobalStatePtr = &(nodes[i].gs);

	__application_start_address = (uint32_t)myFlashPtr + SD_SIZE_GET(MBR_SIZE);
	__application_end_address = (uint32_t)__application_start_address + 1024UL*50;

	//Point the linker sections for connectionTypeResolvers to the correct array as long as we don't use the linker sections for the 
	__start_conn_type_resolvers = (u32)connTypeResolvers;
	__stop_conn_type_resolvers = ((u32)connTypeResolvers) + sizeof(connTypeResolvers);

	if (currentNode->state.initialized) {
		ChooseSimulatorTerminal();
	}
}

void CherrySim::ChooseSimulatorTerminal() {
	//Enable or disable terminal based on the currently set terminal id
	if (terminalId == 0 || terminalId == currentNode->id) {
		currentNode->gs.terminal->terminalIsInitialized = true;
	}
	else {
		currentNode->gs.terminal->terminalIsInitialized = false;
	}

	//FIXME: remove
	//currentNode->gs.terminal->terminalIsInitialized = true;
}

void CherrySim::initCurrentNode()
{
	//Set the global state pointer to null to forget the old global state and reinitialize it
	GlobalState::instance = NULL;
	GlobalState::getInstance();
	myGlobalStatePtr = &currentNode->gs;

	//FIXME: Probably unfinished as there is still some state in the Softdevice that we need to reset

	memset(currentNode->state.connections, 0x00, sizeof(SoftdeviceConnection)*SIM_CONNECTION_NUM);
	
	memset(currentNode->terminalInput, 0x00, PIPE_BUFFER_SIZE);

	//Init GPIO
	memset(&currentNode->gpio, 0x00, sizeof(currentNode->gpio));
	memset(&currentNode->gpioBackup, 0x00, sizeof(currentNode->gpioBackup));
}


void CherrySim::bootCurrentNode()
{
	RecordStorage::getInstance()->InitialRepair();

	Boardconf::getInstance()->Initialize(true);

	//Configure LED pins as output
	GS->ledRed = new LedWrapper(Boardconfig->led1Pin, Boardconfig->ledActiveHigh);
	GS->ledGreen = new LedWrapper(Boardconfig->led2Pin, Boardconfig->ledActiveHigh);
	GS->ledBlue = new LedWrapper(Boardconfig->led3Pin, Boardconfig->ledActiveHigh);

	GS->ledRed->Off();
	GS->ledGreen->Off();
	GS->ledBlue->Off();


	Conf::getInstance()->Initialize(true);

	//Config->terminalMode = TerminalMode::TERMINAL_PROMPT_MODE;

	//Initialize the UART Terminal
	Terminal::getInstance()->Init();
	Logger::getInstance()->Init();

	ChooseSimulatorTerminal();

	//Testing* testing = new Testing();

	//testing->testPacketQueue();

	logjson("ERROR", "{\"type\":\"reboot\"}" SEP);
	logjson("ERROR", "{\"version\":2}" SEP);

	logt("ERROR", "UICR boot address is %x", BOOTLOADER_UICR_ADDRESS);


	//Enable logging for some interesting log tags
//	Logger::getInstance()->enableTag("NODE");
//	Logger::getInstance()->enableTag("STORAGE");
//	Logger::getInstance()->enableTag("DATA");
//	Logger::getInstance()->enableTag("SEC");
	Logger::getInstance()->enableTag("HANDSHAKE");
	Logger::getInstance()->enableTag("CONFIG");
//	Logger::getInstance()->enableTag("EVENTS");
//	Logger::getInstance()->enableTag("CONN");
////	Logger::getInstance()->enableTag("STATES");
//	Logger::getInstance()->enableTag("ADV");
////	Logger::getInstance()->enableTag("SINK");
	Logger::getInstance()->enableTag("CM");
//	Logger::getInstance()->enableTag("DISCONNECT");
////	Logger::getInstance()->enableTag("JOIN");
////	Logger::getInstance()->enableTag("CONN");
//	Logger::getInstance()->enableTag("CONN_DATA");
//	Logger::getInstance()->enableTag("DISCOVERY");


	//Initialize GPIOTE for Buttons
	initGpioteButtons();

	//Initialialize the SoftDevice and the BLE stack
	bleInit();

	//Initialize the new storage
	NewStorage::getInstance();

	//Test RecordStorage
	TestRecordStorage* trs = new TestRecordStorage();
	//trs->Start();

	//Test PacketQueue
	TestPacketQueue* tpq = new TestPacketQueue();
	//tpq->Start();

	//Initialize GAP and GATT
	GAPController::getInstance()->bleConfigureGAP();
	GATTController::getInstance();
	AdvertisingController::getInstance()->Initialize();
	
	//AdvertisingController::getInstance()->Test();

	//Register a pre/post transmit hook for radio events
	if (Config->enableRadioNotificationHandler) {
		ble_radio_notification_init(RADIO_NOTIFICATION_IRQ_PRIORITY, NRF_RADIO_NOTIFICATION_DISTANCE_800US, radioEventDispatcher);
	}

	//Init the magic
	GS->node = new Node();
	GS->node->Initialize();

	//Start Timers
	initTimers();

	GS->pendingSysEvent = 0;

	//Registers Handler for the simulator so that it can react to certain commands to the node
	registerSimulatorTerminalHandler();

	//Lets us do some configuration after the boot
	configureNode();
}

void CherrySim::simulateRadioNotifications() {
	if (currentNode->state.radioNotificationHandler != NULL) {
		currentNode->state.radioNotificationHandler(true);
		currentNode->state.radioNotificationHandler(false);
	}
}

//Simulates the timer events with an offset for each node
#define SHOULD_SIM_IV_TRIGGER(ivMs) (((currentNode->state.timeMs) % (ivMs)) == 0)
void CherrySim::simulateTimer(){
	//Advance time of this node
	currentNode->state.timeMs += simState.simTickDurationMs;

	if(SHOULD_SIM_IV_TRIGGER(Config->mainTimerTickDs * 100L)){
		ble_timer_dispatch(NULL);
	}
}

//Calls the system event dispatcher to mark flash operations complete
//The erases/writes themselves are executed immediately at the moment, though
//This will loop until all flash operations (also those that are queued in response to a successfuly operation) are executed
void sim_commit_flash_operations()
{
	if (simulateAsyncFlash) {
		while (currentNode->numWaitingFlashOperations > 0) {
			sysDispatchEventHandler(NRF_EVT_FLASH_OPERATION_SUCCESS);
			currentNode->numWaitingFlashOperations--;
		}
	}
}

//Uses a list of fails to simulate some successful and some failed flash operations
void sim_commit_some_flash_operations(uint8_t* failData, uint16_t numMaxEvents)
{
	u32 i = 0;
	if (simulateAsyncFlash) {
		while (currentNode->numWaitingFlashOperations > 0 && i < numMaxEvents) {
			if(failData[i] == 0) sysDispatchEventHandler(NRF_EVT_FLASH_OPERATION_SUCCESS);
			else sysDispatchEventHandler(NRF_EVT_FLASH_OPERATION_ERROR);
			currentNode->numWaitingFlashOperations--;
			i++;
		}
	}
}

//TODO: implement timeouts, e.g. for connecting

void CherrySim::simulateTimeouts() {
	if (currentNode->state.connectingActive && currentNode->state.connectingTimeoutTimestampMs <= (i32)simState.simTimeMs) {
		currentNode->state.connectingActive = false;

		simBleEvent s;
		s.globalId = simState.globalEventIdCounter++;
		s.bleEvent.header.evt_id = BLE_GAP_EVT_TIMEOUT;
		s.bleEvent.evt.gap_evt.conn_handle = BLE_CONN_HANDLE_INVALID;
		s.bleEvent.evt.gap_evt.params.timeout.src = BLE_GAP_TIMEOUT_SRC_CONN;

		currentNode->eventQueue->push(s);
	}
}

//Simuliert das aussenden von Advertising nachrichten. Wenn andere nodes gerade scannen bekommen sie es als advertising event mitgeteilt,
//Wenn eine andere node gerade eine verbindung zu diesem Partner aufbauen will, wird das advertisen der anderen node gestoppt, die verbindung wird
//connected und es wird an beide nodes ein Event geschickt, dass sie nun verbunden sind
void CherrySim::simulateBroadcast() {
	//Check for other nodes that are scanning and send them the events
	if (currentNode->state.advertisingActive) {
		if (SHOULD_SIM_IV_TRIGGER(currentNode->state.advertisingIntervalMs)) {
			
			//Distribute the event to all nodes in range
			for (u32 i = 0; i < simState.numNodes; i++) {
				if (i != currentNode->index) {

					//If the other node is scanning
					if (nodes[i].state.scanningActive) {
						//If the random value hits the probability, the event is sent
						double probability = calculateReceptionProbability(currentNode, &nodes[i]);
						if (PSRNG() < probability) {
							simBleEvent s;
							s.globalId = simState.globalEventIdCounter++;
							s.bleEvent.header.evt_id = BLE_GAP_EVT_ADV_REPORT;
							s.bleEvent.evt.gap_evt.conn_handle = BLE_CONN_HANDLE_INVALID;

							memcpy(&s.bleEvent.evt.gap_evt.params.adv_report.data, &currentNode->state.advertisingData, currentNode->state.advertisingDataLength);
							s.bleEvent.evt.gap_evt.params.adv_report.dlen = currentNode->state.advertisingDataLength;
							memcpy(&s.bleEvent.evt.gap_evt.params.adv_report.peer_addr, &currentNode->address, sizeof(ble_gap_addr_t));
							//TODO: bleEvent.evt.gap_evt.params.adv_report.peer_addr = ...;
							s.bleEvent.evt.gap_evt.params.adv_report.rssi = (i8)GetReceptionRssi(currentNode, &nodes[i]);
							s.bleEvent.evt.gap_evt.params.adv_report.scan_rsp = 0;
							s.bleEvent.evt.gap_evt.params.adv_report.type = currentNode->state.advertisingType;

							nodes[i].eventQueue->push(s);
						}
					//If the other node is connecting
					} else if (nodes[i].state.connectingActive && currentNode->state.advertisingType == BLE_GAP_ADV_TYPE_ADV_IND) {
						//If the other node matches our partnerId we are connecting to
						if(memcmp(&nodes[i].state.connectingPartnerAddr, &currentNode->address, sizeof(ble_gap_addr_t)) == 0){
							//If the random value hits the probability, the event is sent
							double probability = calculateReceptionProbability(currentNode, &nodes[i]);
							if (PSRNG() < probability) {

								ConnectMasterToSlave(&nodes[i], currentNode);
								
								//Disable advertising for the own node, because this will be stopped after a connection is made
								//Then, Immediately return to not broadcast more packets
								currentNode->state.advertisingActive = false;
								return;
							}
						}
					}
				}
			}
		}
	}
}

//Connects two nodes, the currentNode is the slave
void CherrySim::ConnectMasterToSlave(nodeEntry* master, nodeEntry* slave)
{
	//Increments global counter for global unique connection Handle
	simState.globalConnHandleCounter++;
	if (simState.globalConnHandleCounter > 65000) {
		//We can only simulate this far, output a warning that the state is inconsistent after this point in simulation
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! ATTENTION:Wrapping globalConnHandleCounter !!!!!!!!!!!!!!!!!!!!!!!\n");
		simState.globalConnHandleCounter = 1;
	}

	//Print json for sim_connect_central
	json j1;
	j1["type"] = "sim_connect";
	j1["nodeId"] = master->id;
	j1["partnerId"] = slave->id;
	j1["globalConnectionHandle"] = simState.globalConnHandleCounter;
	j1["rssi"] = (int)GetReceptionRssi(master, slave);
	j1["timeMs"] = simState.simTimeMs;

	//printf("%s" EOL, j1.dump().c_str());
	if (isSimPipeEnabled()) {
		FruitySimPipe::WriteToPipeF("%s" EOL, j1.dump().c_str());
	}

	//###### Current node
	SoftdeviceConnection* freeInConnection = &slave->state.connections[0];

	if (freeInConnection->connectionActive) {
		throw new std::error_code();
	}

	freeInConnection->connectionActive = true;
	freeInConnection->rssiMeasurementActive = false;
	freeInConnection->connectionIndex = 0;
	freeInConnection->connectionHandle = simState.globalConnHandleCounter;
	freeInConnection->connectionInterval = UNITS_TO_MSEC(Config->meshMinConnectionInterval, UNIT_1_25_MS); //TODO: which node's params are used?
	freeInConnection->partner = master;

	//Generate an event for the current node
	simBleEvent s2;
	s2.globalId = simState.globalEventIdCounter++;
	s2.bleEvent.header.evt_id = BLE_GAP_EVT_CONNECTED;
	s2.bleEvent.evt.gap_evt.conn_handle = simState.globalConnHandleCounter;

	s2.bleEvent.evt.gap_evt.params.connected.conn_params.min_conn_interval = slave->state.connectingParamIntervalMs;
	s2.bleEvent.evt.gap_evt.params.connected.conn_params.max_conn_interval = slave->state.connectingParamIntervalMs;
	s2.bleEvent.evt.gap_evt.params.connected.own_addr = FruityHal::Convert(&slave->address);
	s2.bleEvent.evt.gap_evt.params.connected.peer_addr = FruityHal::Convert(&master->address);
	s2.bleEvent.evt.gap_evt.params.connected.role = BLE_GAP_ROLE_PERIPH;

	slave->eventQueue->push(s2);

	//###### Remote node
	SoftdeviceConnection* freeOutConnection = NULL;
	for (int j = 1; j < SIM_CONNECTION_NUM; j++) {
		if (!master->state.connections[j].connectionActive) {
			freeOutConnection = &master->state.connections[j];

			freeOutConnection->connectionIndex = j;
			freeOutConnection->connectionActive = true;
			freeOutConnection->rssiMeasurementActive = false;
			freeOutConnection->connectionHandle = simState.globalConnHandleCounter;
			freeOutConnection->connectionInterval = UNITS_TO_MSEC(Config->meshMinConnectionInterval, UNIT_1_25_MS); //TODO: should take the proper interval
			freeOutConnection->partner = slave;

			break;
		}
	}

	if (freeOutConnection == NULL) {
		throw new std::error_code();
	}

	//Save connection references
	freeInConnection->partnerConnection = freeOutConnection;
	freeOutConnection->partnerConnection = freeInConnection;

	//Generate an event for the remote node
	simBleEvent s;
	s.globalId = simState.globalEventIdCounter++;
	s.bleEvent.header.evt_id = BLE_GAP_EVT_CONNECTED;
	s.bleEvent.evt.gap_evt.conn_handle = simState.globalConnHandleCounter;

	s.bleEvent.evt.gap_evt.params.connected.conn_params.min_conn_interval = slave->state.connectingParamIntervalMs;
	s.bleEvent.evt.gap_evt.params.connected.conn_params.max_conn_interval = slave->state.connectingParamIntervalMs;
	s.bleEvent.evt.gap_evt.params.connected.own_addr = FruityHal::Convert(&master->address);
	s.bleEvent.evt.gap_evt.params.connected.peer_addr = FruityHal::Convert(&slave->address);
	s.bleEvent.evt.gap_evt.params.connected.role = BLE_GAP_ROLE_CENTRAL;

	master->eventQueue->push(s);

	//Disable connecting for the other node because we just got the remote SoftDevice a connection
	master->state.connectingActive = false;
}

//Our calibration value for distance calculation
float N = 2.5;

//Takes an RSSI and the calibrated RSSI at 1m and returns the distance in m
float CherrySim::RssiToDistance(int rssi, int calibratedRssi){
	float distanceInMetre = (float)pow(10, (calibratedRssi - rssi) / (10 * N));
	return distanceInMetre;
}

float CherrySim::GetDistanceBetween(nodeEntry* nodeA, nodeEntry* nodeB) {
	float distX = abs(nodeA->state.x - nodeB->state.x) * simState.mapWidthMeters;
	float distY = abs(nodeA->state.y - nodeB->state.y) * simState.mapHeightMeters;
	float dist = sqrt(pow(distX, 2) + pow(distY, 2));

	return dist;
}

float CherrySim::GetReceptionRssi(nodeEntry* sender, nodeEntry* receiver) {
	float dist = GetDistanceBetween(sender, receiver);

	return (sender->gs.boardconf->configuration.calibratedTX + sender->gs.config->configuration.defaultDBmTX) - log10(dist) * 10 * N;
}

double CherrySim::calculateReceptionProbability(nodeEntry* sendingNode, nodeEntry* receivingNode) {
	double dist = GetDistanceBetween(sendingNode, receivingNode);

	//TODO: Add some randomness and use a function to do the mapping
	float rssi = GetReceptionRssi(sendingNode, receivingNode);

	if (rssi > -60) return 1.0;
	else if (rssi > -80) return 0.8;
	else if (rssi > -85) return 0.5;
	else if (rssi > -90) return 0.3;
	else return 0;

}


void CherrySim::SimulateConnections() {
	if (!simulateConnectionsProperly) return;

	/* Currently, the simulation will only take one connection event to transmit a reliable packet and both the packet event and the ACK will be generated
	* at the same time. Also, all unreliable packets are always sent in one conneciton event.
	* If many connections exist with short connection intervals, the behaviour is not realistic as the amount of packets that are being sent should decrease.
	* There is also no probability of failure and the buffer is always emptied
	*/

	//Simulate sending data for each connection individually
	for (int i = 0; i < SIM_CONNECTION_NUM; i++) {
		SoftdeviceConnection* connection = &currentNode->state.connections[i];
		if (connection->connectionActive) {
			//Each connecitonInterval, we trigger
			if (SHOULD_SIM_IV_TRIGGER(connection->connectionInterval)) {
				if (connection->reliableBuffers[0].sender != NULL) {
					//Sends the reliable packet, occupies one conneciton event
					SoftDeviceBufferedPacket* bufferedPacket = &connection->reliableBuffers[0];
					GenerateWrite(bufferedPacket->sender, bufferedPacket->receiver, bufferedPacket->connHandle, &bufferedPacket->writeParams);
					bufferedPacket->sender = NULL;
				}
				else {
					//Sends all packets from the unreliable buffers in one connection event
					for (int i = 0; i < SIM_NUM_UNRELIABLE_BUFFERS; i++) {
						if (connection->unreliableBuffers[i].sender != NULL) {
							SoftDeviceBufferedPacket* bufferedPacket = &connection->unreliableBuffers[i];
							GenerateWrite(bufferedPacket->sender, bufferedPacket->receiver, bufferedPacket->connHandle, &bufferedPacket->writeParams);
							bufferedPacket->sender = NULL;
						}
					}
				}
			}
		}
	}

	// Simulate Connection RSSI measurements
	for (int i = 0; i < SIM_CONNECTION_NUM; i++) {
		if (SHOULD_SIM_IV_TRIGGER(5000)) {
			SoftdeviceConnection* connection = &currentNode->state.connections[i];
			if (connection->connectionActive && connection->rssiMeasurementActive) {
				nodeEntry* master = i == 0 ? connection->partner : currentNode;
				nodeEntry* slave = i == 0 ? currentNode : connection->partner;

				simBleEvent s;
				s.globalId = simState.globalEventIdCounter++;
				s.bleEvent.header.evt_id = BLE_GAP_EVT_RSSI_CHANGED;
				s.bleEvent.evt.gap_evt.conn_handle = connection->connectionHandle;
				s.bleEvent.evt.gap_evt.params.rssi_changed.rssi = (i8)GetReceptionRssi(master, slave);

				currentNode->eventQueue->push(s);
			}
		}
	}

	//Simulate Connection Loss every second
	if (connectionTimeoutProbabilityPerSec != 0) {
		for (int i = 0; i < SIM_CONNECTION_NUM; i++) {
			if (currentNode->state.connections[i].connectionActive) {
				if (PSRNG() < connectionTimeoutProbabilityPerSec) {
					printf("Simulated Connection Loss for node %u on connectionHandle %u\n", currentNode->id, currentNode->state.connections[i].connectionHandle);
					DisconnectSimulatorConnection(&currentNode->state.connections[i], BLE_HCI_CONNECTION_TIMEOUT, BLE_HCI_CONNECTION_TIMEOUT);
				}
			}
		}
	}
}

void CherrySim::SimulateGPIO() {
	////Check if the GPIO pins have changed and reflect this new state
	//if (memcmp(&currentNode->gpio, &currentNode->gpioBackup, sizeof(currentNode->gpio)) != 0) {
	//	printf("CHANGE DETECTED");

	//	u32 m_io_msk = 1 << 18;

	//	if(NRF_GPIO->OUT & m_io_msk)

	//}

}

//Simuliert das Event handling dieser Node, sobald diese durch das Softdevice aufgeweckt wurde. Alle funktionalitaet muss auf den GlobalState zugreifen, sodass
//dieser ausgetauscht werden kann.
void CherrySim::simulateCurrentNode()
{
	u32 err = NRF_ERROR_NOT_FOUND;

	//Check if there is input on uart
	if (terminalId == currentNode->id || terminalId == 0) {
		Terminal::getInstance()->CheckAndProcessLine();
	}

#ifdef ACTIVATE_CLC_MODULE
	ClcComm::getInstance()->CheckAndProcess();
#endif

	do
	{
		//Fetch the event
		GS->sizeOfCurrentEvent = GS->sizeOfEvent;
		err = sd_ble_evt_get((u8*)GS->currentEventBuffer, &GS->sizeOfCurrentEvent);

		//Handle ble event event
		if (err == NRF_SUCCESS)
		{
			//logt("EVENT", "--- EVENT_HANDLER %d -----", currentEvent->header.evt_id);
			//trace("<");
			bleDispatchEventHandler(GS->currentEvent);
			//trace(">");
		}
		//No more events available
		else if (err == NRF_ERROR_NOT_FOUND)
		{
			//Handle waiting button event
			if(GS->button1HoldTimeDs != 0){
				u32 holdTimeDs = GS->button1HoldTimeDs;
				GS->button1HoldTimeDs = 0;

				GS->node->ButtonHandler(0, holdTimeDs);

				dispatchButtonEvents(0, holdTimeDs);
			}

			//Handle Timer event that was waiting
			if (GS->node && GS->node->passsedTimeSinceLastTimerHandlerDs > 0)
			{
				u16 timerDs = GS->node->passsedTimeSinceLastTimerHandlerDs;

				//Dispatch timer to all other modules
				timerEventDispatch(timerDs, GS->node->appTimerDs);

				//FIXME: Should protect this with a semaphore
				//because the timerInterrupt works asynchronously
				GS->node->passsedTimeSinceLastTimerHandlerDs -= timerDs;
			}

			//If a pending system event is waiting, call the handler
			if(GS->pendingSysEvent != 0){
				u32 copy = GS->pendingSysEvent;
				GS->pendingSysEvent = 0;
				sysDispatchEventHandler(copy);
			}

			err = sd_app_evt_wait();
			APP_ERROR_CHECK(err); // OK
			//err = sd_nvic_ClearPendingIRQ(SD_EVT_IRQn);
			APP_ERROR_CHECK(err);  // OK
			break;
		}
		else
		{
			APP_ERROR_CHECK(err); //FIXME: NRF_ERROR_DATA_SIZE not handeled
			break;
		}
	} while (true);
}

//INIT function that starts up the Softdevice and registers the needed handlers
void bleInit(void){
	u32 err = 0;

	logt("NODE", "Initializing Softdevice version 0x%x, Board %d", 34, Boardconfig->boardType);

    // Initialize the SoftDevice handler with the low frequency clock source
	//And a reference to the previously allocated buffer
	//No event handler is given because the event handling is done in the main loop
	nrf_clock_lf_cfg_t clock_lf_cfg;

	clock_lf_cfg.source = NRF_CLOCK_LF_SRC_XTAL;
	clock_lf_cfg.rc_ctiv = 0;
	clock_lf_cfg.rc_temp_ctiv = 0;
	clock_lf_cfg.xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_100_PPM;

	//SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);
    //err = softdevice_handler_init(&clock_lf_cfg, GS->currentEventBuffer, GS->sizeOfEvent, NULL);
    APP_ERROR_CHECK(err);

    logt("NODE", "Softdevice Init OK");

    // Register with the SoftDevice handler module for System events.
    //err = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err);

    //Now we will enable the Softdevice. RAM usage depends on the values chosen
    ble_enable_params_t params;
    memset(&params, 0x00, sizeof(params));

    params.common_enable_params.vs_uuid_count = 5; //set the number of Vendor Specific UUIDs to 5

    //TODO: configure Bandwidth
    //params.common_enable_params.p_conn_bw_counts->rx_counts.high_count = 4;
    //params.common_enable_params.p_conn_bw_counts->tx_counts.high_count = 4;


    params.gap_enable_params.periph_conn_count = Config->meshMaxInConnections; //Number of connections as Peripheral

    //FIXME: Adding this one line above will corrupt the executable???
    //But they don't do anything???
    //params.common_enable_params.p_conn_bw_counts->rx_counts.high_count = 4;
    //params.common_enable_params.p_conn_bw_counts->tx_counts.high_count = 4;



    params.gap_enable_params.central_conn_count = Config->meshMaxOutConnections; //Number of connections as Central
    params.gap_enable_params.central_sec_count = 1; //this application only needs to be able to pair in one central link at a time



    params.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT; //we require the Service Changed characteristic
    params.gatts_enable_params.attr_tab_size = BLE_GATTS_ATTR_TAB_SIZE_DEFAULT; //the default Attribute Table size is appropriate for our application


    //The base ram address is gathered from the linker
    u32 app_ram_base = (u32)456;
    /* enable the BLE Stack */
    logt("ERROR", "Ram base at 0x%x", app_ram_base);
    err = sd_ble_enable(&params, &app_ram_base);
//    if(err == NRF_SUCCESS){
//    /* Verify that __LINKER_APP_RAM_BASE matches the SD calculations */
//		if(app_ram_base != (u32)__application_ram_start_address){
//			logt("ERROR", "Warning: unused memory: 0x%x", ((u32)__application_ram_start_address) - app_ram_base);
//		}
//	} else if(err == NRF_ERROR_NO_MEM) {
//		/* Not enough memory for the SoftDevice. Use output value in linker script */
//		logt("ERROR", "Fatal: Not enough memory for the selected configuration. Required:0x%x", app_ram_base);
//    } else {
//    	APP_ERROR_CHECK(err); //OK
//    }

    //Enable 6 packets per connection interval
    ble_opt_t bw_opt;
    bw_opt.common_opt.conn_bw.role               = BLE_GAP_ROLE_PERIPH;
    bw_opt.common_opt.conn_bw.conn_bw.conn_bw_rx = BLE_CONN_BW_HIGH;
    bw_opt.common_opt.conn_bw.conn_bw.conn_bw_tx = BLE_CONN_BW_HIGH;
    //err = sd_ble_opt_set(BLE_COMMON_OPT_CONN_BW, &bw_opt);
    logt("ERROR", "could not set bandwith %u", err);

    bw_opt.common_opt.conn_bw.role               = BLE_GAP_ROLE_CENTRAL;
    bw_opt.common_opt.conn_bw.conn_bw.conn_bw_rx = BLE_CONN_BW_HIGH;
    bw_opt.common_opt.conn_bw.conn_bw.conn_bw_tx = BLE_CONN_BW_HIGH;
    //err = sd_ble_opt_set(BLE_COMMON_OPT_CONN_BW, &bw_opt);
    logt("ERROR", "could not set bandwith %u", err);

    //APP_ERROR_CHECK(err);

    //Enable DC/DC (needs external LC filter, cmp. nrf51 reference manual page 43)
	err = sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
	APP_ERROR_CHECK(err); //OK

	//Set power mode
	err = sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
	APP_ERROR_CHECK(err); //OK

	//Set preferred TX power
	err = sd_ble_gap_tx_power_set(Config->defaultDBmTX);
	APP_ERROR_CHECK(err); //OK

//	//Enable UART interrupt
//	NRF_UART0->INTENSET = UART_INTENSET_RXDRDY_Enabled << UART_INTENSET_RXDRDY_Pos;
//	//Enable interrupt forwarding for UART
//	err = sd_nvic_SetPriority(UART0_IRQn, NRF_APP_PRIORITY_LOW);
//	APP_ERROR_CHECK(err);
//	err = sd_nvic_EnableIRQ(UART0_IRQn);
//	APP_ERROR_CHECK(err);
}


//Receives all packets that are send over the mesh and can handle these
//Beware that sender and receiver are per hop. To get original sender and destination, check package contents
void CherrySim::PacketHandler(u32 senderId, u32 receiverId, u8* data, u32 dataLength) {

	connPacketHeader* packet = (connPacketHeader*)data;
	switch (packet->messageType) {
		case MESSAGE_TYPE_MODULE_TRIGGER_ACTION: {
			connPacketModule* modPacket = (connPacketModule*)packet;

			
		}
		break;
	}


}

//Will be called if an error occurs somewhere in the code, but not if it's a hardfault
extern "C"
{

volatile uint32_t keepId;
volatile uint32_t keepPc;
volatile uint32_t keepInfo;

	//The app_error handler is called by all APP_ERROR_CHECK functions
	void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
	{
		GS->ledBlue->Off();
		GS->ledRed->Off();
		if(Config->debugMode) while(1){
			GS->ledGreen->Toggle();
			//nrf_delay_us(50000);
		}

		//else sd_nvic_SystemReset();
	}

	//Called when the softdevice crashes
	void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
	{
		keepId = id;
		keepPc = pc;
		keepInfo = info;

		GS->ledRed->Off();
		GS->ledGreen->Off();
		if(Config->debugMode) while(1){
			GS->ledBlue->Toggle();
			//nrf_delay_us(50000);
		}
		//else sd_nvic_SystemReset();

		logt("ERROR", "Softdevice fault id %u, pc %u, info %u", keepId, keepPc, keepInfo);
	}

	//Dispatches system events
	void sim_sys_evt_dispatch(uint32_t sys_evt)
	{
		//Because there can only be one flash event at a time before registering a new flash operation
		//We do not need an event queue to handle this. If we want other sys_events, we probably need a queue
		if(sys_evt == NRF_EVT_FLASH_OPERATION_ERROR
			|| sys_evt == NRF_EVT_FLASH_OPERATION_SUCCESS
		){
			GS->pendingSysEvent = sys_evt;
		}
	}

	//This is, where the program will get stuck in the case of a Hard fault
	void HardFault_Handler(void)
	{
		GS->ledBlue->Off();
		GS->ledGreen->Off();
		if(Config->debugMode) while(1){
			GS->ledRed->Toggle();
			//nrf_delay_us(50000);
		}
		//else sd_nvic_SystemReset();
	}

	//This handler receives UART interrupts if terminal mode is disabled
	void UART0_IRQHandler(void)
	{
		dispatchUartInterrupt();
	}

}

void dispatchButtonEvents(u8 buttonId, u32 buttonHoldTime)
{
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(GS->node != NULL && GS->node->activeModules[i] != 0  && GS->node->activeModules[i]->configurationPointer->moduleActive){
			GS->node->activeModules[i]->ButtonHandler(buttonId, buttonHoldTime);
		}
	}
}

void radioEventDispatcher(bool radioActive)
{
	if (!radioActive) {
		//
		AdvertisingController::getInstance()->SetAdvertisingData();
	}

	Node::RadioEventHandler(radioActive);
}

void bleDispatchEventHandler(ble_evt_t * bleEvent)
{
	u16 eventId = bleEvent->header.evt_id;


	logt("EVENTS", "BLE EVENT %s (%d)", Logger::getInstance()->getBleEventNameString(eventId), eventId);

//	if(
//			bleEvent->header.evt_id != BLE_GAP_EVT_RSSI_CHANGED &&
//			bleEvent->header.evt_id != BLE_GAP_EVT_ADV_REPORT
//	) trace("<");

	//Give events to all controllers
	GAPController::getInstance()->bleConnectionEventHandler(bleEvent);
	AdvertisingController::getInstance()->AdvertiseEventHandler(bleEvent);
	ScanController::getInstance()->ScanEventHandler(bleEvent);
	GATTController::getInstance()->bleMeshServiceEventHandler(bleEvent);

	if(GS->node != NULL){
		GS->cm->BleEventHandler(bleEvent);
	}

	//Dispatch ble events to all modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(GS->node != NULL && GS->node->activeModules[i] != 0  && GS->node->activeModules[i]->configurationPointer->moduleActive){
			GS->node->activeModules[i]->BleEventHandler(bleEvent);
		}
	}

	logt("EVENTS", "End of event");

//	if(
//				bleEvent->header.evt_id != BLE_GAP_EVT_RSSI_CHANGED &&
//				bleEvent->header.evt_id != BLE_GAP_EVT_ADV_REPORT
//		) trace(">");
}

void sysDispatchEventHandler(u32 sys_evt)
{
	//Hand system events to new storage class
	NewStorage::getInstance()->SystemEventHandler(sys_evt);

	//Dispatch system events to all modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(GS->node != NULL && GS->node->activeModules[i] != NULL && GS->node->activeModules[i]->configurationPointer->moduleActive){
			GS->node->activeModules[i]->SystemEventHandler(sys_evt);
		}
	}
}

//### TIMERS ##############################################################
APP_TIMER_DEF(mainTimerMsId);

//Called by the app_timer module
static void ble_timer_dispatch(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    //We just increase the time that has passed since the last handler
    //And call the timer from our main event handling queue
    GS->node->passsedTimeSinceLastTimerHandlerDs += Config->mainTimerTickDs;

	//trace(".");

    //Timer handlers are called from the main event handling queue and from timerEventDispatch
}

//This function is called from the main event handling
void timerEventDispatch(u16 passedTime, u32 appTimer)
{
	//Call the timer handler from the node
	GS->node->TimerTickHandler(passedTime);

	AdvertisingController::getInstance()->TimerHandler(passedTime);

	//Dispatch event to all modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(GS->node != NULL && GS->node->activeModules[i] != 0  && GS->node->activeModules[i]->configurationPointer->moduleActive){
			GS->node->activeModules[i]->TimerEventHandler(passedTime, appTimer);
		}
	}

#ifdef ACTIVATE_CLC_MODULE
	ClcComm::getInstance()->TimerEventHandler(passedTime, appTimer);
#endif
}

//Starts an application timer
void initTimers(void){
	u32 err = 0;

	//APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, NULL);

	//err = app_timer_create(&mainTimerMsId, APP_TIMER_MODE_REPEATED, ble_timer_dispatch);
    APP_ERROR_CHECK(err);

	//err = app_timer_start(mainTimerMsId, APP_TIMER_TICKS(Config->mainTimerTickDs * 100, APP_TIMER_PRESCALER), NULL);
    APP_ERROR_CHECK(err);
}

// ######################### UART
void dispatchUartInterrupt(){
#ifdef USE_UART
	Terminal::getInstance()->UartInterruptHandler();
#endif
#ifdef ACTIVATE_CLC_MODULE
	ClcComm::getInstance()->InterruptHandler();
#endif
}

// ######################### GPIO Tasks and Events
void initGpioteButtons(){

}

//######################### Simulator specific ###########################

void setSimLed(bool state) {
	if (state == currentNode->ledOn) return;
	currentNode->ledOn = state;

	if (!sendLedDataToPipe) return;

	json j1;
	j1["type"] = "sim_led";
	j1["nodeId"] = currentNode->id;
	j1["ledOn"] = state;

	printf("%s" EOL, j1.dump().c_str());

	if (cherrySimInstance->isSimPipeEnabled()) {
		FruitySimPipe::WriteToPipeF("%s" EOL, j1.dump().c_str());
	}
}

fh_ble_gap_addr_t CherrySim::GetGapAddress(nodeEntry* node) {
	return currentNode->address;
}

//########################################### SD_CALLS AND STUFF #####################################################

extern "C" {

	uint32_t sd_ble_gap_adv_data_set(const uint8_t* p_data, uint8_t dlen, const uint8_t* p_sr_data, uint8_t srdlen)
	{
		if (sdBleGapAdvDataSetFailProbability != 0 && PSRNG() < sdBleGapAdvDataSetFailProbability) {
			printf("Simulated fail for sd_ble_gap_adv_data_set\n");
			return NRF_ERROR_INVALID_STATE;
		}

		memcpy(currentNode->state.advertisingData, p_data, dlen);
		currentNode->state.advertisingDataLength = dlen;


		cherrySimInstance->SendCurrentAdvertisingData();

		//TODO: could copy scan response data

		return 0;
	}

	uint32_t sd_ble_gap_adv_stop()
	{
		currentNode->state.advertisingActive = false;


		cherrySimInstance->SendCurrentAdvertisingData();

		//TODO: could return invalid sate

		return 0;
	}

	uint32_t sd_ble_gap_adv_start(const ble_gap_adv_params_t* p_adv_params)
	{
		currentNode->state.advertisingActive = true;
		currentNode->state.advertisingIntervalMs = UNITS_TO_MSEC(p_adv_params->interval, UNIT_0_625_MS);
		currentNode->state.advertisingType = p_adv_params->type;

		cherrySimInstance->SendCurrentAdvertisingData();

		//TODO: could return invalid state

		return 0;
	}

	//This sends the simulator info for the current advertising
	void CherrySim::SendCurrentAdvertisingData() {

		if (!sendAdvDataToPipe) return;

		char buffer[128];
		Logger::getInstance()->convertBufferToHexString(currentNode->state.advertisingData, currentNode->state.advertisingDataLength, buffer, 128);

		json j;
		j["type"] = "sim_advinfo";
		j["nodeId"] = currentNode->id;
		j["active"] = currentNode->state.advertisingActive;
		if (currentNode->state.advertisingActive) {
			j["interval"] = currentNode->state.advertisingIntervalMs;
			j["advType"] = currentNode->state.advertisingType;
			j["data"] = buffer;
		}

		//printf("%s" EOL, j.dump().c_str());

		if (isSimPipeEnabled()) {
			FruitySimPipe::WriteToPipeF("%s" EOL, j.dump().c_str());
		}
	}

	bool CherrySim::TerminalCommandHandler(std::string commandName, std::vector<std::string> commandArgs)
	{
		//Allows us to switch terminals
		if (commandName == "term" && commandArgs.size() == 1) {
			if (commandArgs[0] == "all") {
				terminalId = 0;
			}
			else {
				terminalId = atoi(commandArgs[0].c_str());
			}
			printf("Switched Terminal to %d\n", terminalId);

			return true;
		}
		else if (commandName == "nodes" && commandArgs.size() == 1) {
			numNodes = atoi(commandArgs[0].c_str());
			printf("numNodes set to %d\n", numNodes);
			return true;
		}
		else if (commandName == "seed" || commandName == "seedr") {
			if (commandArgs.size() == 1) {
				seed = atoi(commandArgs[0].c_str());
			}
			else {
				seed++;
			}
			printf("Seed set to %d\n", seed);

			if (commandName == "seedr") {
				resetSimulator();
			}

			return true;
		}
		else if (commandName == "width" && commandArgs.size() == 1) {
			mapWidthInMeters = atoi(commandArgs[0].c_str());
			printf("Width set to %d\n", mapWidthInMeters);
			return true;
		}
		else if (commandName == "flush" && commandArgs.size() == 0) {
			sim_commit_flash_operations();
			return true;
		}
		else if (commandName == "flushfail" && commandArgs.size() == 0) {
			u8 failData[] = { 1,1,1,1,1,1,1,1,1,1 };
			sim_commit_some_flash_operations(failData, 10);
			return true;
		}
		else if (commandName == "height" && commandArgs.size() == 1) {
			mapHeightInMeters = atoi(commandArgs[0].c_str());
			printf("Height set to %d\n", mapHeightInMeters);
			return true;
		}
		else if (commandName == "lossprob" && commandArgs.size() == 1) {
			connectionTimeoutProbabilityPerSec = atof(commandArgs[0].c_str());
			printf("COnnectionLossProb. set to %f\n", connectionTimeoutProbabilityPerSec);
			return true;
		}
		else if (commandName == "simstat") {
			printf("Terminal %d\n", terminalId);
			printf("numNodes set to %d\n", numNodes);
			printf("Seed set to %d\n", seed);
			printf("Width set to %d\n", mapWidthInMeters);
			printf("Height set to %d\n", mapHeightInMeters);
			printf("Simtime %d\n", simState.simTimeMs);
			printf("ConnectionLossProb %f\n", connectionTimeoutProbabilityPerSec);
			return true;
		}
		else if (commandName == "simreset") {
			resetSimulator();
			return true;
		}
		else if (commandName == "delay" && commandArgs.size() == 1) {
			playDelay = atoi(commandArgs[0].c_str());
			return true;
		}
		else if (commandName == "simdump") {
			dumpSimulatorStateToFile();
			return true;
		}

		return false;
	}

	void CherrySim::dumpSimulatorStateToFile() {
		std::ofstream outFile;
		outFile.open("simdump.bin", std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);

		outFile.write((char*)nodes, sizeof(nodes));
		outFile.write((char*)&simState, sizeof(simState));
		outFile.close();

		//TODO: dump event queues, remove objects from simState so we can dump it
	}

	void CherrySim::resetSimulator() {
		FruitySimPipe::ClosePipe();
		shouldRestartSim = true;
	}

	uint32_t sd_ble_gap_connect(const ble_gap_addr_t* p_peer_addr, const ble_gap_scan_params_t* p_scan_params, const ble_gap_conn_params_t* p_conn_params)
	{
		currentNode->state.connectingActive = true;
		currentNode->state.connectingStartTimeMs = simState.simTimeMs;
		memcpy(&currentNode->state.connectingPartnerAddr, p_peer_addr, sizeof(p_peer_addr));

		currentNode->state.connectingIntervalMs = UNITS_TO_MSEC(p_scan_params->interval, UNIT_0_625_MS);
		currentNode->state.connectingWindowMs = UNITS_TO_MSEC(p_scan_params->window, UNIT_0_625_MS);
		currentNode->state.connectingTimeoutTimestampMs = simState.simTimeMs + p_scan_params ->timeout * 1000UL;

		currentNode->state.connectingParamIntervalMs = UNITS_TO_MSEC(p_conn_params->min_conn_interval, UNIT_1_25_MS);

		//TODO: could save more params, could return invalid state

		return 0;
	}

	u32 CherrySim::DisconnectSimulatorConnection(SoftdeviceConnection* connection, u32 hciReason, u32 hciReasonPartner) {
		
		//If it could not be found, the connection might have been terminated by a peer or the sim already or something was wrong
		if (connection == NULL || !connection->connectionActive) {
			//TODO: maybe log possible SIM error?
			return BLE_ERROR_INVALID_CONN_HANDLE;
		}

		//Find the connection on the partners side
		nodeEntry* partnerNode = connection->partner;
		SoftdeviceConnection* partnerConnection = connection->partnerConnection;

		json j;
		j["type"] = "sim_disconnect";
		j["nodeId"] = currentNode->id;
		j["partnerId"] = partnerNode->id;
		j["globalConnectionHandle"] = connection->connectionHandle;
		j["timeMs"] = simState.simTimeMs;
		j["reason"] = hciReason;

		//printf("%s" EOL, j.dump().c_str());

		if (isSimPipeEnabled()) {
			FruitySimPipe::WriteToPipeF("%s" EOL, j.dump().c_str());
		}

		//TODO: this is wrong, fix when this happens
		if (partnerConnection == NULL) {
			throw new std::error_code();
		}

		//Clear the transmitbuffers for both nodes
		memset(connection->reliableBuffers, 0x00, sizeof(connection->reliableBuffers));
		memset(connection->unreliableBuffers, 0x00, sizeof(connection->unreliableBuffers));
		memset(partnerConnection->reliableBuffers, 0x00, sizeof(connection->reliableBuffers));
		memset(partnerConnection->unreliableBuffers, 0x00, sizeof(connection->unreliableBuffers));

		//#### Our own node
		connection->connectionActive = false;

		simBleEvent s1;
		s1.globalId = simState.globalEventIdCounter++;
		s1.bleEvent.header.evt_id = BLE_GAP_EVT_DISCONNECTED;
		s1.bleEvent.evt.gap_evt.conn_handle = connection->connectionHandle;
		s1.bleEvent.evt.gap_evt.params.disconnected.reason = hciReason;
		currentNode->eventQueue->push(s1);

		//#### Remote node
		partnerConnection->connectionActive = false;

		simBleEvent s2;
		s2.globalId = simState.globalEventIdCounter++;
		s2.bleEvent.header.evt_id = BLE_GAP_EVT_DISCONNECTED;
		s2.bleEvent.evt.gap_evt.conn_handle = partnerConnection->connectionHandle;
		s2.bleEvent.evt.gap_evt.params.disconnected.reason = hciReasonPartner;
		partnerNode->eventQueue->push(s2);

		return NRF_SUCCESS;
	}

	uint32_t sd_ble_gap_disconnect(uint16_t conn_handle, uint8_t hci_status_code)
	{
		//Find the connection by its handle
		SoftdeviceConnection* connection = findConnectionByHandle(currentNode, conn_handle);

		return cherrySimInstance->DisconnectSimulatorConnection(connection, BLE_HCI_LOCAL_HOST_TERMINATED_CONNECTION, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);


		return NRF_SUCCESS;
	}

	uint32_t sd_ble_gap_sec_info_reply(uint16_t conn_handle, const ble_gap_enc_info_t* p_enc_info, const ble_gap_irk_t* p_id_info, const ble_gap_sign_info_t* p_sign_info)
	{
		return 0;
	}

	uint32_t sd_ble_gap_encrypt(uint16_t conn_handle, const ble_gap_master_id_t* p_master_id, const ble_gap_enc_info_t* p_enc_info)
	{
		throw new std::error_code();// not supported yet
		return 0;
	}

	uint32_t sd_ble_gap_conn_param_update(uint16_t conn_handle, const ble_gap_conn_params_t* p_conn_params)
	{


		return 0;
	}

	uint32_t sd_ble_uuid_vs_add(const ble_uuid128_t* p_vs_uuid, uint8_t* p_uuid_type)
	{


		return 0;
	}

	uint32_t sd_ble_gatts_service_add(uint8_t type, const ble_uuid_t* p_uuid, uint16_t* p_handle)
	{


		return 0;
	}

	uint32_t sd_ble_gatts_characteristic_add(uint16_t service_handle, const ble_gatts_char_md_t* p_char_md, const ble_gatts_attr_t* p_attr_char_value, ble_gatts_char_handles_t* p_handles)
	{
		p_handles->value_handle = 14;

		return 0;
	}

	uint32_t sd_ble_gatts_sys_attr_set(uint16_t conn_handle, const uint8_t* p_sys_attr_data, uint16_t len, uint32_t flags)
	{


		return 0;
	}

	uint32_t sd_ble_gattc_primary_services_discover(uint16_t conn_handle, uint16_t start_handle, const ble_uuid_t* p_srvc_uuid)
	{


		return 0;
	}

	uint32_t sd_ble_gattc_characteristics_discover(uint16_t conn_handle, const ble_gattc_handle_range_t* p_handle_range)
	{


		return 0;
	}

	SoftDeviceBufferedPacket* findFreePacketBuffer(SoftdeviceConnection* connection) {
		for (int i = 0; i < SIM_NUM_UNRELIABLE_BUFFERS; i++) {
			if (connection->unreliableBuffers[i].sender == NULL) {
				return &connection->unreliableBuffers[i];
			}
		}
		return NULL;
	}

	uint32_t sd_ble_gattc_write(uint16_t conn_handle, const ble_gattc_write_params_t* p_write_params)
	{

		//TODO: Will send message immediately, should be simulated based on connectionEvents
		SoftdeviceConnection* connection = findConnectionByHandle(currentNode, conn_handle);
		//TODO: maybe log this?
		if (connection == NULL) {
			return BLE_ERROR_INVALID_CONN_HANDLE;
		}

		nodeEntry* partnerNode = connection->partner;
		SoftdeviceConnection*  partnerConnection = connection->partnerConnection;
		
		//Should not happen, sim connection is always terminated at both ends simultaniously
		if (partnerConnection == NULL) {
			throw new std::error_code();
		}

		if (simulateConnectionsProperly) {
			//Fill either reliable or unreliable buffer with the packet
			SoftDeviceBufferedPacket* buffer = NULL;
			if (p_write_params->write_op == BLE_GATT_OP_WRITE_REQ) {
				buffer = &connection->reliableBuffers[0];
			} else if (p_write_params->write_op == BLE_GATT_OP_WRITE_CMD) {
				buffer = findFreePacketBuffer(connection);
			} else {
				throw new std::error_code();
			}

			if (buffer == NULL || buffer->sender != 0) {
				return BLE_ERROR_NOT_ENABLED; //TODO: maybe log and return correct BLE_ERROR_NO_TX_BUFFERS   
			}

			buffer->sender = currentNode;
			buffer->receiver = partnerNode;
			buffer->connHandle = conn_handle;
			memcpy(buffer->data, p_write_params->p_value, p_write_params->len);
			buffer->writeParams = *p_write_params;
			buffer->writeParams.p_value = buffer->data; //Reassign data pointer to our buffer
		}
		else {
			//Send packet immediately
			cherrySimInstance->GenerateWrite(currentNode, partnerNode, partnerConnection->connectionHandle, p_write_params);
		}
		//TODO: check against p_write_params->handle?

		return 0;
	}

	//This function generates a WRITE event and a TX for two nodes that want to send data
	void CherrySim::GenerateWrite(nodeEntry* sender, nodeEntry* receiver, uint16_t conn_handle, const ble_gattc_write_params_t* p_write_params) {
		//Print json for data
		json j;
		j["type"] = "sim_data";
		j["nodeId"] = sender->id;
		j["partnerId"] = receiver->id;
		j["reliable"] = p_write_params->write_op == BLE_GATT_OP_WRITE_REQ;
		j["timeMs"] = simState.simTimeMs;
		char buffer[128];
		Logger::getInstance()->convertBufferToHexString(p_write_params->p_value, p_write_params->len, buffer, 128);
		j["data"] = buffer;
		//printf("%s" EOL, j.dump().c_str());


		//FruitySimPipe::WriteToPipeF("%s" EOL, j.dump().c_str());

		cherrySimInstance->PacketHandler(sender->id, receiver->id, p_write_params->p_value, p_write_params->len);


		//Generate WRITE event at our partners side
		simBleEvent s;
		s.globalId = simState.globalEventIdCounter++;
		s.bleEvent.header.evt_id = BLE_GATTS_EVT_WRITE;

		//Generate write event in partners event queue
		s.bleEvent.evt.gatts_evt.conn_handle = conn_handle;

		memcpy(&s.bleEvent.evt.gatts_evt.params.write.data, p_write_params->p_value, p_write_params->len);
		s.bleEvent.evt.gatts_evt.params.write.handle = p_write_params->handle;
		s.bleEvent.evt.gatts_evt.params.write.len = p_write_params->len;
		s.bleEvent.evt.gatts_evt.params.write.offset = 0;
		s.bleEvent.evt.gatts_evt.params.write.op = p_write_params->write_op;

		receiver->eventQueue->push(s);


		//Generates a Transmit_OK event for our own node
		if (p_write_params->write_op == BLE_GATT_OP_WRITE_CMD) {
			simBleEvent s2;
			s2.globalId = simState.globalEventIdCounter++;
			s2.bleEvent.header.evt_id = BLE_EVT_TX_COMPLETE;
			s2.bleEvent.evt.common_evt.conn_handle = conn_handle;
			s2.bleEvent.evt.common_evt.params.tx_complete.count = 1;

			sender->eventQueue->push(s2);
		}
		else if (p_write_params->write_op == BLE_GATT_OP_WRITE_REQ) {
			simBleEvent s2;
			s2.globalId = simState.globalEventIdCounter++;
			s2.bleEvent.header.evt_id = BLE_GATTC_EVT_WRITE_RSP;
			s2.bleEvent.evt.gattc_evt.conn_handle = conn_handle;
			s2.bleEvent.evt.gattc_evt.gatt_status = BLE_GATT_STATUS_SUCCESS;

			sender->eventQueue->push(s2);
		}
	}

	uint32_t sd_ble_gap_scan_stop()
	{
		currentNode->state.scanningActive = false;

		return 0;
	}

	uint32_t sd_ble_gap_scan_start(const ble_gap_scan_params_t* p_scan_params)
	{
		currentNode->state.scanningActive = true;
		currentNode->state.scanIntervalMs = UNITS_TO_MSEC(p_scan_params->interval, UNIT_0_625_MS);
		currentNode->state.scanWindowMs = UNITS_TO_MSEC(p_scan_params->window, UNIT_0_625_MS);


		return 0;
	}

	uint32_t sd_ble_tx_packet_count_get(uint16_t conn_handle, uint8_t* p_count)
	{
		*p_count = SIM_NUM_UNRELIABLE_BUFFERS;

		return 0;
	}

	uint32_t sd_ble_gap_address_set(uint8_t addr_cycle_mode, const ble_gap_addr_t* p_addr)
	{
		//Just for checking that we do not change the type, could be removed
		if (p_addr->addr_type != currentNode->address.addr_type) {
			throw new std::error_code();
		}

		//We just set it without any error detection
		currentNode->address = FruityHal::Convert(p_addr);

		return 0;
	}

	uint32_t sd_ble_gap_address_get(ble_gap_addr_t* p_addr)
	{
		fh_ble_gap_addr_t addr = cherrySimInstance->GetGapAddress(currentNode);
		memcpy(p_addr->addr, addr.addr, BLE_GAP_ADDR_LEN);
		p_addr->addr_type = addr.addr_type;

		return 0;
	}

	uint32_t sd_ble_gap_rssi_start(uint16_t conn_handle, uint8_t threshold_dbm, uint8_t skip_count)
	{
		SoftdeviceConnection* connection = findConnectionByHandle(currentNode, conn_handle);
		if (connection == NULL) {
			return BLE_ERROR_INVALID_CONN_HANDLE;
		}
		connection->rssiMeasurementActive = true;

		return 0;
	}

	uint32_t sd_ble_gap_rssi_stop(uint16_t conn_handle)
	{
		SoftdeviceConnection* connection = findConnectionByHandle(currentNode, conn_handle);
		if (connection == NULL) {
			return BLE_ERROR_INVALID_CONN_HANDLE;
		}
		connection->rssiMeasurementActive = false;

		return 0;
	}

	uint32_t sd_flash_page_erase(uint32_t page_number)
	{

		logt("RS", "Erasing Page %u", page_number);

		u32* p = (u32*)(FLASH_REGION_START_ADDRESS + (u32)page_number * PAGE_SIZE);

		for (u32 i = 0; i<PAGE_SIZE / 4; i++) {
			p[i] = 0xFFFFFFFF;
		}

		
		if (simulateAsyncFlash) {
			currentNode->numWaitingFlashOperations++;
		} else {
			sysDispatchEventHandler(NRF_EVT_FLASH_OPERATION_SUCCESS);
		}

		return NRF_SUCCESS;
	}

	uint32_t sd_flash_write(uint32_t* const p_dst, const uint32_t* const p_src, uint32_t size)
	{
		u32 sourcePage = ((u32)p_src - FLASH_REGION_START_ADDRESS) / PAGE_SIZE;
		u32 sourcePageOffset = ((u32)p_src - FLASH_REGION_START_ADDRESS) % PAGE_SIZE;
		u32 destinationPage = ((u32)p_dst - FLASH_REGION_START_ADDRESS) / PAGE_SIZE;
		u32 destinationPageOffset = ((u32)p_dst - FLASH_REGION_START_ADDRESS) % PAGE_SIZE;

		if ((u32)p_src >= FLASH_REGION_START_ADDRESS && (u32)p_src < FLASH_REGION_START_ADDRESS + FLASH_SIZE) {
			logt("RS", "Copy from page %u (+%u) to page %u (+%u), len %u", sourcePage, sourcePageOffset, destinationPage, destinationPageOffset, size*4);
		}
		else {
			logt("RS", "Write ram to page %u (+%u), len %u", destinationPage, destinationPageOffset, size*4);
		}

		if (((u32)p_src) % 4 != 0) {
			logt("ERROR", "source unaligned");
		}
		if (((u32)p_dst) % 4 != 0) {
			logt("ERROR", "dest unaligned");
		}

		//Only toggle bits from 1 to 0 when writing!
		for (u32 i = 0; i < size; i++) {
			p_dst[i] &= p_src[i];
		}

		if (simulateAsyncFlash) {
			currentNode->numWaitingFlashOperations++;
		}
		else {
			sysDispatchEventHandler(NRF_EVT_FLASH_OPERATION_SUCCESS);
		}

		return NRF_SUCCESS;
	}

	//Works
	uint32_t sd_rand_application_vector_get(uint8_t* p_buff, uint8_t length)
	{
		for (int i = 0; i < length; i++) {
			p_buff[i] = (u8)(PSRNG() * 256);
		}

		return 0;
	}

	uint32_t sd_ble_evt_get(uint8_t* p_dest, uint16_t* p_len)
	{
		if (currentNode->eventQueue->size() > 0) {

			simBleEvent bleEvent = currentNode->eventQueue->front();
			currentNode->eventQueue->pop();

			memcpy(p_dest, &bleEvent.bleEvent, GS->sizeOfEvent);
			*p_len = GS->sizeOfEvent;

			return NRF_SUCCESS;
		}
		else {
			return NRF_ERROR_NOT_FOUND;
		}
	}

	uint32_t sd_app_evt_wait()
	{


		return 0;
	}

	uint32_t sd_nvic_ClearPendingIRQ(IRQn_Type IRQn)
	{


		return 0;
	}

	uint32_t sd_nvic_EnableIRQ(IRQn_Type IRQn)
	{

		return 0;
	}

	uint32_t sd_nvic_SetPriority(IRQn_Type IRQn, uint32_t priority)
	{

		return 0;
	}

	uint32_t sd_radio_notification_cfg_set(uint8_t type, uint8_t distance)
	{

		return 0;
	}



	uint32_t ble_radio_notification_init(uint32_t irq_priority, uint8_t distance, ble_radio_notification_evt_handler_t evt_handler)
	{
		currentNode->state.radioNotificationHandler = evt_handler;
		return 0;
	}

	//Works
	uint32_t sd_ble_gap_tx_power_set(int8_t tx_power)
	{
		if (tx_power == -40 || tx_power == -30 || tx_power == -20 || tx_power == -16
			|| tx_power == -12 || tx_power == -8 || tx_power == -4 || tx_power == 0 || tx_power == 4) {
			currentNode->state.txPower = tx_power;
		}
		else {
			throw new std::error_code();

		}

		return 0;
	}

	//TODO: Implement this for SoC events
	uint32_t sd_evt_get(uint32_t* evt_id)
	{

		return 0;
	}

	//Resets a node
	uint32_t sd_nvic_SystemReset()
	{
		printf("Node %u resetted\n", currentNode->id);

		u32 index = currentNode->index;
		
		//Clean up simulator connections and broadcast this to map
		for (int i = 0; i < SIM_CONNECTION_NUM; i++){
			SoftdeviceConnection* connection = &nodes[index].state.connections[i];
			cherrySimInstance->DisconnectSimulatorConnection(connection, BLE_HCI_CONNECTION_TIMEOUT, BLE_HCI_CONNECTION_TIMEOUT);
		}

		//Clear the Simulator state of this node
		while (!nodes[index].eventQueue->empty()) nodes[index].eventQueue->pop();
		
		//Copy the initial node state back to this node
		memcpy(&nodes[index], &nodesBackup[index], sizeof(nodeEntry));
		//cherrySimInstance->flashNode(index);

		//Boot node again
		cherrySimInstance->setNode(index);
		cherrySimInstance->initCurrentNode();
		cherrySimInstance->bootCurrentNode();

		//throw an exception so we can step out of the current program flow
		throw std::exception("NODE_RESET");

		return 0;
	}

	//Works
	uint32_t app_timer_cnt_get(uint32_t* time)
	{
		if (APP_TIMER_PRESCALER != 0) {
			exit(1); //The Prescaler must be 0 for this function to work correctly
		}

		*time = (u32)(simState.simTimeMs * (APP_TIMER_CLOCK_FREQ / 1000.0));

		return 0;
	}

	//Working
	uint32_t app_timer_cnt_diff_compute(uint32_t nowTime, uint32_t previousTime, uint32_t* passedTime)
	{
		//Normal case
		if (nowTime >= previousTime) {
			*passedTime = nowTime - previousTime;
		}
		//In case of integer overflow
		else {
			*passedTime = UINT32_MAX - (previousTime - nowTime);
		}

		return 0;
	}


	//No work needed for now

	uint32_t sd_ble_enable(ble_enable_params_t* p_ble_enable_params, uint32_t* p_app_ram_base)
	{
		currentNode->state.initialized = true;
		return 0;
	}

	uint32_t sd_softdevice_enable(nrf_clock_lf_cfg_t* clock_source, uint32_t* unsure)
	{
		return 0;
	}

	uint32_t sd_ble_gap_device_name_set(const ble_gap_conn_sec_mode_t* p_write_perm, const uint8_t* p_dev_name, uint16_t len)
	{
		return 0;
	}

	uint32_t sd_ble_gap_appearance_set(uint16_t appearance)
	{
		return 0;
	}

	uint32_t sd_ble_gap_ppcp_set(const ble_gap_conn_params_t* p_conn_params)
	{
		return 0;
	}

	uint32_t sd_power_dcdc_mode_set(uint8_t dcdc_mode)
	{
		return 0;
	}

	uint32_t sd_power_mode_set(uint8_t power_mode)
	{
		return 0;
	}

	void nrf_delay_us(uint32_t usec) {
		//TODO: Implement
	}

	uint32_t sd_ble_opt_set(uint32_t opt_id, ble_opt_t const *p_opt) {
		return NRF_SUCCESS;
	}

	//TODO: implement
	uint32_t sd_ble_gatts_hvx(uint16_t conn_handle, ble_gatts_hvx_params_t const *p_hvx_params) {
		
		throw std::exception("sd_ble_gatts_hvx Not implkemented");
		return 0;
	}

	uint32_t sd_ecb_block_encrypt(nrf_ecb_hal_data_t * p_ecb_data) {
		AES_ECB_encrypt(p_ecb_data->cleartext, p_ecb_data->key, p_ecb_data->ciphertext, 16);

		return 0;
	}
	uint32_t sd_power_reset_reason_clr(uint32_t p) {
		return 0;
	}

}

#pragma warning( pop )

#endif
/**
 *@}
 **/
