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


extern "C"{
#include <stdlib.h>
#include <pstorage_platform.h>
#include <nrf_soc.h>
#include <app_error.h>
#include <softdevice_handler.h>
#include <app_timer.h>
#include <malloc.h>
}

//A global buffer for the current event, which must be 4-byte aligned
#pragma pack(push)
#pragma pack(4)
static u8 currentEventBuffer[sizeof(ble_evt_t) + BLE_L2CAP_MTU_DEF];
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

//Debug variable
bool lookingForInvalidStateErrors = false;

Conf* Conf::instance;

int main(void)
{
	u32 err;

	//Initialize the UART Terminal
	Terminal::Init();

	//Initialialize the SoftDevice and the BLE stack
	bleInit();

	//Enable logging for some interesting log tags
	Logger::getInstance().enableTag("NODE");
	Logger::getInstance().enableTag("STORAGE");
	Logger::getInstance().enableTag("DATA");

	//Initialize the storage class
	Storage::getInstance();

	//Init the magic
	node = new Node(Config->meshNetworkIdentifier);


	new Testing();

	struct mallinfo used = mallinfo();
	volatile u32 size = used.uordblks + used.hblkhd;

	//Start Timers
	initTimers();

	while (true)
	{
		u32 err = NRF_ERROR_NOT_FOUND;

		//Check if there is input on uart
		Terminal::PollUART();

		do
		{
			//Fetch the event
			sizeOfCurrentEvent = sizeOfEvent;
			err = sd_ble_evt_get(currentEventBuffer, &sizeOfCurrentEvent);

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

				err = sd_app_evt_wait();
				APP_ERROR_CHECK(err);
				sd_nvic_ClearPendingIRQ(SD_EVT_IRQn);
				break;
			}
			else
			{
				APP_ERROR_CHECK(err);
				break;
			}
		} while (true);
	}
}

//INIT function that starts up the Softdevice and registers the needed handlers
void bleInit(void){
	u32 err = 0;

    // Initialize the SoftDevice handler with the low frequency clock source
	//And a reference to the previously allocated buffer
	//No event handler is given because the event handling is done in the main loop
	//SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, NULL);
    err = softdevice_handler_init(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, currentEventBuffer, sizeOfEvent, NULL);
    APP_ERROR_CHECK(err);

    // Register with the SoftDevice handler module for System events.
    err = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err);

	//FOR THE S130 WE MUST NOW CALL sd_ble_enable() TO ENABLE BLE FUNCTIONALITY
	//Decide if we include the service changed characteristic in our services
	ble_enable_params_t bleSdEnableParams;
    memset(&bleSdEnableParams, 0, sizeof(bleSdEnableParams));
    bleSdEnableParams.gatts_enable_params.attr_tab_size = ATTR_TABLE_MAX_SIZE;
    bleSdEnableParams.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT;
	err = sd_ble_enable(&bleSdEnableParams);
    APP_ERROR_CHECK(err);

    //Enable DC/DC (needs external LC filter, cmp. nrf51 reference manual page 43)
	err = sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
	APP_ERROR_CHECK(err);

	//Set power mode
	err = sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
	APP_ERROR_CHECK(err);
}


//Will be called if an error occurs somewhere in the code, but not if it's a hardfault
extern "C"
{

	//The app_error handler is called by all APP_ERROR_CHECK functions
	void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
	{
		//We want to debug DEADBEEF => Endless loop.
		if(error_code == 0xDEADBEEF){
			while(1){

			}
		}

		//Output Error message to UART
		if(error_code != NRF_SUCCESS){
			const char* errorString = Logger::getNrfErrorString(error_code);
			//logt("ERROR", "ERROR CODE %d: %s in file %s@%d", error_code, errorString, p_file_name, line_num);
		}

		//Invalid states are bad and should be debugged, but should not necessarily
		//Break the program every time they happen.
		//FIXME: must not ever happen, so fix that
		if (!Node::lookingForInvalidStateErrors)
		{
			if (error_code == NRF_ERROR_INVALID_STATE)
			{

				return;
			}
		}

		//NRF_ERROR_BUSY is not an error(tm)
		//FIXME: above statement is not true
		if (error_code == NRF_ERROR_BUSY)
		{
			return;
		}

		//All other errors will run into endless loop for debugging
		while(1){

		}
	}

	//Called when the softdevice crashes
	void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
	{
		//Does not produce interesting filename,....
	    app_error_handler(0xDEADBEEF, 0, NULL);
	}

	//Dispatches system events
	void sys_evt_dispatch(uint32_t sys_evt)
	{
		//Hand system events to the pstorage library
	    pstorage_sys_event_handler(sys_evt);

	    //Dispatch system events to all modules
		for(int i=0; i<MAX_MODULE_COUNT; i++){
			if(node != NULL && node->activeModules[i] != NULL && node->activeModules[i]->configurationPointer->moduleActive){
				node->activeModules[i]->SystemEventHandler(sys_evt);
			}
		}

	}

	//This is, where the program will get stuck in the case of a Hard fault
	void HardFault_Handler(void)
	{
		for (;;)
		{
			// Endless debugger loop
		}
	}
}

void bleDispatchEventHandler(ble_evt_t * bleEvent)
{
	u16 eventId = bleEvent->header.evt_id;

	logt("EVENTS", "BLE EVENT %s (%d)", Logger::getBleEventNameString(eventId), eventId);

	//Give events to all controllers
	GAPController::bleConnectionEventHandler(bleEvent);
	AdvertisingController::AdvertiseEventHandler(bleEvent);
	ScanController::ScanEventHandler(bleEvent);
	GATTController::bleMeshServiceEventHandler(bleEvent);

	//Dispatch ble events to all modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(node != NULL && node->activeModules[i] != 0  && node->activeModules[i]->configurationPointer->moduleActive){
			node->activeModules[i]->BleEventHandler(bleEvent);
		}
	}

	logt("EVENTS", "End of event");
}

//### TIMERS ##############################################################
static app_timer_id_t mainTimerMsId; // Main timer

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
static void timerEventDispatch(u16 passedTime, u32 appTimer){
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

	APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, false);

	err = app_timer_create(&mainTimerMsId, APP_TIMER_MODE_REPEATED, ble_timer_dispatch);
    APP_ERROR_CHECK(err);

	err = app_timer_start(mainTimerMsId, APP_TIMER_TICKS(Config->mainTimerTickMs, APP_TIMER_PRESCALER), NULL);
    APP_ERROR_CHECK(err);
}

/**
 *@}
 **/
