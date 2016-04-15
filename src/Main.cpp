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

#include <Main.h>
#include <Node.h>
#include <Terminal.h>
#include <Storage.h>
#include <AdvertisingController.h>
#include <ScanController.h>
#include <GAPController.h>
#include <GATTController.h>
#include <Logger.h>
#include <Testing.h>
#include <LedWrapper.h>
#include <Module.h>
#include <Utility.h>
#include <types.h>
#include <TestBattery.h>
#include <Config.h>
#include <NewStorage.h>


extern "C"{
#include <stdlib.h>
#include <nrf_soc.h>
#include <app_error.h>
#include <softdevice_handler.h>
#include <app_timer.h>
#include <malloc.h>
#include <nrf_gpio.h>
#include <nrf_mbr.h>
#include <nrf_sdm.h>
#include <nrf_delay.h>
#include <nrf_nvic.h>
}

//A global buffer for the current event, which must be 4-byte aligned
#pragma pack(push)
#pragma pack(4)
static u32 currentEventBuffer[CEIL_DIV(BLE_STACK_EVT_MSG_BUF_SIZE, sizeof(uint32_t))];
static ble_evt_t* currentEvent = (ble_evt_t *) currentEventBuffer;
static const u16 sizeOfEvent = sizeof(currentEventBuffer);
static u16 sizeOfCurrentEvent = sizeOfEvent;
#pragma pack(pop)

// Include (or do not) the service_changed characteristic.
// If not enabled, the server's database cannot be changed for the lifetime of the device
#define IS_SRVC_CHANGED_CHARACT_PRESENT 1




#define APP_TIMER_PRESCALER       0 // Value of the RTC1 PRESCALER register
#define APP_TIMER_MAX_TIMERS      1 //Maximum number of simultaneously created timers (2 + BSP_APP_TIMERS_NUMBER)
#define APP_TIMER_OP_QUEUE_SIZE   1 //Size of timer operation queues


//Reference to Node
Node* node = NULL;

Conf* Conf::instance;

LedWrapper* LedRed = NULL;
LedWrapper* LedGreen = NULL;
LedWrapper* LedBlue = NULL;

//Put the firmware version in a special section right after the initialization vector
uint32_t app_version __attribute__((section(".Version"), used)) = FM_VERSION;

int main(void)
{
	u32 err;

	//Detect the used board at runtime or select one at compile time in the config
	SET_BOARD();

	//Configure LED pins as output
	LedRed = new LedWrapper(Config->Led1Pin, Config->LedActiveHigh);
	LedGreen = new LedWrapper(Config->Led2Pin, Config->LedActiveHigh);
	LedBlue = new LedWrapper(Config->Led3Pin, Config->LedActiveHigh);

	LedRed->Off();
	LedGreen->On();
	LedBlue->Off();

	//Initialize the UART Terminal
	Terminal::Init();

	//Testing* testing = new Testing();

	//testing->testPacketQueue();

	uart("ERROR", "{\"version\":2}" SEP);

	//Enable logging for some interesting log tags
	Logger::getInstance().enableTag("NODE");
	Logger::getInstance().enableTag("STORAGE");
	Logger::getInstance().enableTag("DATA");
	Logger::getInstance().enableTag("SEC");
//	Logger::getInstance().enableTag("HANDSHAKE");
//	Logger::getInstance().enableTag("DISCOVERY");
//	Logger::getInstance().enableTag("CONN");
//	Logger::getInstance().enableTag("STATES");
//	Logger::getInstance().enableTag("ADV");
//	Logger::getInstance().enableTag("SINK");
//	Logger::getInstance().enableTag("CM");
//	Logger::getInstance().enableTag("CONN");
//	Logger::getInstance().enableTag("CONN_DATA");

	//Initialialize the SoftDevice and the BLE stack
	bleInit();

	//Initialize the storage class
	Storage::getInstance();

	//Initialize the new storage
	NewStorage::Init();

	//Init the magic
	node = new Node(Config->meshNetworkIdentifier);



	new Testing();


	//Start Timers
	initTimers();

	pendingSysEvent = 0;

	while (true)
	{
		u32 err = NRF_ERROR_NOT_FOUND;

		//Check if there is input on uart
		Terminal::PollUART();

		do
		{
			//Fetch the event
			sizeOfCurrentEvent = sizeOfEvent;
			err = sd_ble_evt_get((u8*)currentEventBuffer, &sizeOfCurrentEvent);

			//Handle ble event event
			if (err == NRF_SUCCESS)
			{
				//logt("EVENT", "--- EVENT_HANDLER %d -----", currentEvent->header.evt_id);
				bleDispatchEventHandler(currentEvent);
			}
			//No more events available
			else if (err == NRF_ERROR_NOT_FOUND)
			{

				//Handle Timer event that was waiting
				if (node && node->passsedTimeSinceLastTimerHandler > 0)
				{
					//Call the timer handler from the node
					node->TimerTickHandler(node->passsedTimeSinceLastTimerHandler);

					//Dispatch timer to all other modules
					timerEventDispatch(node->passsedTimeSinceLastTimerHandler, node->appTimerMs);

					node->passsedTimeSinceLastTimerHandler = 0;
				}

				//If a pending system event is waiting, call the handler
				if(pendingSysEvent != 0){
					u32 copy = pendingSysEvent;
					pendingSysEvent = 0;
					sysDispatchEventHandler(copy);
				}

				err = sd_app_evt_wait();
				APP_ERROR_CHECK(err); // OK
				err = sd_nvic_ClearPendingIRQ(SD_EVT_IRQn);
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
}

void detectBoardAndSetConfig(){
#ifdef DETECT_AND_SET_ARS100748_BOARD
	DETECT_AND_SET_ARS100748_BOARD();
#endif

//Could add other detect methods here...
}

//INIT function that starts up the Softdevice and registers the needed handlers
void bleInit(void){
	u32 err = 0;

	logt("NODE", "Initializing Softdevice version 0x%x, Board %d", SD_FWID_GET(MBR_SIZE), Config->boardType);

    // Initialize the SoftDevice handler with the low frequency clock source
	//And a reference to the previously allocated buffer
	//No event handler is given because the event handling is done in the main loop
	nrf_clock_lf_cfg_t clock_lf_cfg;

	clock_lf_cfg.source = NRF_CLOCK_LF_SRC_XTAL;
	clock_lf_cfg.rc_ctiv = 0;
	clock_lf_cfg.rc_temp_ctiv = 0;
	clock_lf_cfg.xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_100_PPM;

	//SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);
    err = softdevice_handler_init(&clock_lf_cfg, currentEventBuffer, sizeOfEvent, NULL);
    APP_ERROR_CHECK(err);

    logt("NODE", "Softdevice Init OK");

    // Register with the SoftDevice handler module for System events.
    err = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err);

    //Now we will enable the Softdevice. RAM usage depends on the values chosen
    ble_enable_params_t params;
    memset(&params, 0x00, sizeof(params));

    params.common_enable_params.vs_uuid_count = 5; //set the number of Vendor Specific UUIDs to 5

    //TODO: configure Bandwidth
    //params.common_enable_params.p_conn_bw_counts->

    params.gap_enable_params.periph_conn_count = Config->meshMaxInConnections; //Number of connections as Peripheral
    params.gap_enable_params.central_conn_count = Config->meshMaxOutConnections; //Number of connections as Central
    params.gap_enable_params.central_sec_count = 1; //this application only needs to be able to pair in one central link at a time

    params.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT; //we require the Service Changed characteristic
    params.gatts_enable_params.attr_tab_size = BLE_GATTS_ATTR_TAB_SIZE_DEFAULT; //the default Attribute Table size is appropriate for our application

    //The base ram address is gathered from the linker
    u32 app_ram_base = (u32)__application_ram_start_address;
    /* enable the BLE Stack */
    logt("ERROR", "Ram base at 0x%x", app_ram_base);
    err = sd_ble_enable(&params, &app_ram_base);
    if(err == NRF_SUCCESS){
    /* Verify that __LINKER_APP_RAM_BASE matches the SD calculations */
		if(app_ram_base != (u32)__application_ram_start_address){
			logt("ERROR", "Warning: unused memory: 0x%x", ((u32)__application_ram_start_address) - app_ram_base);
		}
	} else if(err == NRF_ERROR_NO_MEM) {
		/* Not enough memory for the SoftDevice. Use output value in linker script */
		logt("ERROR", "Fatal: Not enough memory for the selected configuration. Required:0x%x", app_ram_base);
    } else {
    	APP_ERROR_CHECK(err); //OK
    }

    //Enable DC/DC (needs external LC filter, cmp. nrf51 reference manual page 43)
	err = sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
	APP_ERROR_CHECK(err); //OK

	//Set power mode
	err = sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
	APP_ERROR_CHECK(err); //OK

	//Set preferred TX power
	err = sd_ble_gap_tx_power_set(Config->radioTransmitPower);
	APP_ERROR_CHECK(err); //OK

//	//Enable UART interrupt
//	NRF_UART0->INTENSET = UART_INTENSET_RXDRDY_Enabled << UART_INTENSET_RXDRDY_Pos;
//	//Enable interrupt forwarding for UART
//	err = sd_nvic_SetPriority(UART0_IRQn, NRF_APP_PRIORITY_LOW);
//	APP_ERROR_CHECK(err);
//	err = sd_nvic_EnableIRQ(UART0_IRQn);
//	APP_ERROR_CHECK(err);
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
		LedBlue->Off();
		LedRed->Off();
		if(Config->debugMode) while(1){
			LedGreen->Toggle();
			nrf_delay_us(50000);
		}

		//Output Error message to UART
		if(error_code != NRF_SUCCESS){
			const char* errorString = Logger::getNrfErrorString(error_code);
			logt("ERROR", "ERROR CODE %d: %s in file %s@%d", error_code, errorString, p_file_name, line_num);
		}

		//NRF_ERROR_BUSY is not an error(tm)
		//FIXME: above statement is not true
		if (error_code == NRF_ERROR_BUSY)
		{
			return;
		}

		//All other errors will run into endless loop for debugging
		if(Config->debugMode) while(1){}
		else NVIC_SystemReset();
	}

	//Called when the softdevice crashes
	void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
	{
		keepId = id;
		keepPc = pc;
		keepInfo = info;

		LedRed->Off();
		LedGreen->Off();
		if(Config->debugMode) while(1){
			LedBlue->Toggle();
			nrf_delay_us(50000);
		}
		else NVIC_SystemReset();

		logt("ERROR", "Softdevice fault id %u, pc %u, info %u", keepId, keepPc, keepInfo);
	}

	//Dispatches system events
	void sys_evt_dispatch(uint32_t sys_evt)
	{
		//Because there can only be one flash event at a time before registering a new flash operation
		//We do not need an event queue to handle this. If we want other sys_events, we probably need a queue
		if(sys_evt == NRF_EVT_FLASH_OPERATION_ERROR
			|| sys_evt == NRF_EVT_FLASH_OPERATION_SUCCESS
		){
			pendingSysEvent = sys_evt;
		}
	}

	//This is, where the program will get stuck in the case of a Hard fault
	void HardFault_Handler(void)
	{
		LedBlue->Off();
		LedGreen->Off();
		if(Config->debugMode) while(1){
			LedRed->Toggle();
			nrf_delay_us(50000);
		}
		else NVIC_SystemReset();
		NVIC_SystemReset();
		for (;;)
		{
			// Endless debugger loop
			LedRed->On();
			for(int i=0; i<100000; i++){}
			LedRed->Off();
			for(int i=0; i<100000; i++){}
			// Endless debugger loop
		}
	}

//	void UART0_IRQHandler(void){
//		LedBlue->Toggle();
//
//		// Clear UART ERROR event flag.
//		NRF_UART0->EVENTS_ERROR = 0;
//	}

}

void bleDispatchEventHandler(ble_evt_t * bleEvent)
{
	u16 eventId = bleEvent->header.evt_id;


	logt("EVENTS", "BLE EVENT %s (%d)", Logger::getBleEventNameString(eventId), eventId);

	if(
			bleEvent->header.evt_id != BLE_GAP_EVT_RSSI_CHANGED &&
			bleEvent->header.evt_id != BLE_GAP_EVT_ADV_REPORT
	) trace("<");

	//Give events to all controllers
	GAPController::bleConnectionEventHandler(bleEvent);
	AdvertisingController::AdvertiseEventHandler(bleEvent);
	ScanController::ScanEventHandler(bleEvent);
	GATTController::bleMeshServiceEventHandler(bleEvent);

	if(node != NULL){
		node->cm->BleEventHandler(bleEvent);
	}

	//Dispatch ble events to all modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(node != NULL && node->activeModules[i] != 0  && node->activeModules[i]->configurationPointer->moduleActive){
			node->activeModules[i]->BleEventHandler(bleEvent);
		}
	}

	logt("EVENTS", "End of event");

	if(
				bleEvent->header.evt_id != BLE_GAP_EVT_RSSI_CHANGED &&
				bleEvent->header.evt_id != BLE_GAP_EVT_ADV_REPORT
		) trace(">");
}

void sysDispatchEventHandler(u32 sys_evt)
{
	//Hand system events to new storage class
	NewStorage::SystemEventHandler(sys_evt);

	//Hand system events to the pstorage library
	pstorage_sys_event_handler(sys_evt);

	//Dispatch system events to all modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(node != NULL && node->activeModules[i] != NULL && node->activeModules[i]->configurationPointer->moduleActive){
			node->activeModules[i]->SystemEventHandler(sys_evt);
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
    node->passsedTimeSinceLastTimerHandler += Config->mainTimerTickMs;

    //Timer handlers are called from the main event handling queue and from timerEventDispatch
}

//This function is called from the main event handling
void timerEventDispatch(u16 passedTime, u32 appTimer){
	//Dispatch event to all modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(node != NULL && node->activeModules[i] != 0  && node->activeModules[i]->configurationPointer->moduleActive){
			node->activeModules[i]->TimerEventHandler(passedTime, appTimer);
		}
	}
}

//Starts an application timer
void initTimers(void){
	u32 err = 0;

	APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, NULL);

	err = app_timer_create(&mainTimerMsId, APP_TIMER_MODE_REPEATED, ble_timer_dispatch);
    APP_ERROR_CHECK(err);

	err = app_timer_start(mainTimerMsId, APP_TIMER_TICKS(Config->mainTimerTickMs, APP_TIMER_PRESCALER), NULL);
    APP_ERROR_CHECK(err);
}

/**
 *@}
 **/
