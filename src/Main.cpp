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
#ifndef SIM_ENABLED

#include <Main.h>
#include <Node.h>
#include <Terminal.h>
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

#ifdef ACTIVATE_CLC_MODULE
#include <ClcComm.h>
#endif

#include <unwind.h>

extern "C"{
#if defined(NRF51) || defined(SIM_ENABLED)
#include <softdevice_handler.h>
#elif defined(NRF52)
#include <nrf_sdh.h>
#endif

#include <app_timer.h>
#ifndef __ICCARM__
#include <malloc.h>
#endif
#include <nrf_gpio.h>
#include <nrf_mbr.h>
#include <nrf_sdm.h>
#include <nrf_delay.h>
#include <nrf_nvic.h>
#ifndef SIM_ENABLED
#include <ble_radio_notification.h>
#endif
}


// Include (or do not) the service_changed characteristic.
// If not enabled, the server's database cannot be changed for the lifetime of the device
#define IS_SRVC_CHANGED_CHARACT_PRESENT 1

#define TERM_TEST_ID 53

int main(void)
{
	u32 err;

	//Check for reboot reason
	checkRamRetainStruct();

	//If reboot reason is empty (clean power bootup) or
	bool useFlashConfig = true;
#ifdef ACTIVATE_WATCHDOG_SAFE_BOOT_MODE
	if(GS->ramRetainStructPtr->rebootReason == 0 || *GS->rebootMagicNumberPtr == REBOOT_MAGIC_NUMBER){
		useFlashConfig = true;
		*GS->rebootMagicNumberPtr = 0;
	} else {
		useFlashConfig = false;
		*GS->rebootMagicNumberPtr = REBOOT_MAGIC_NUMBER;
	}
#endif

	//Instanciate RecordSTorage to load the board config
	RecordStorage::getInstance();
	Boardconf::getInstance()->Initialize(useFlashConfig);

	//Configure LED pins as output
	GS->ledRed = new LedWrapper(Boardconfig->led1Pin, Boardconfig->ledActiveHigh);
	GS->ledGreen = new LedWrapper(Boardconfig->led2Pin, Boardconfig->ledActiveHigh);
	GS->ledBlue = new LedWrapper(Boardconfig->led3Pin, Boardconfig->ledActiveHigh);

	GS->ledRed->Off();
	GS->ledGreen->Off();
	GS->ledBlue->Off();

	Conf::getInstance()->Initialize(useFlashConfig);

	//Initialize the UART Terminal
	Terminal::getInstance()->Init();
	Logger::getInstance()->Init();

//	Testing* testing = new Testing();
//	testing->testPacketQueue();

	//Enable logging for some interesting log tags
	Logger::getInstance()->enableTag("MAIN");
	Logger::getInstance()->enableTag("NODE");
	Logger::getInstance()->enableTag("STORAGE");
	Logger::getInstance()->enableTag("NEWSTORAGE");
	Logger::getInstance()->enableTag("DATA");
	Logger::getInstance()->enableTag("SEC");
	Logger::getInstance()->enableTag("HANDSHAKE");
//	Logger::getInstance()->enableTag("DISCOVERY");
//	Logger::getInstance()->enableTag("CONN");
//	Logger::getInstance()->enableTag("STATES");
//	Logger::getInstance()->enableTag("ADV");
//	Logger::getInstance()->enableTag("SINK");
	Logger::getInstance()->enableTag("CM");
	Logger::getInstance()->enableTag("DISCONNECT");
//	Logger::getInstance()->enableTag("JOIN");
//	Logger::getInstance()->enableTag("CONN");
//	Logger::getInstance()->enableTag("CONN_DATA");
//	Logger::getInstance()->enableTag("STATES");
	Logger::getInstance()->enableTag("CONFIG");
	Logger::getInstance()->enableTag("RS");
//	Logger::getInstance()->enableTag("PQ");

	Logger::getInstance()->enableTag("MODULE");
	Logger::getInstance()->enableTag("STATUSMOD");
//	Logger::getInstance()->enableTag("DEBUGMOD");
//	Logger::getInstance()->enableTag("ENROLLMOD");
//	Logger::getInstance()->enableTag("IOMOD");
//	Logger::getInstance()->enableTag("SCANMOD");
//	Logger::getInstance()->enableTag("PINGMOD");
//	Logger::getInstance()->enableTag("DFUMOD");
//	Logger::getInstance()->enableTag("CLCMOD");
//	Logger::getInstance()->enableTag("CLCCOMM");

	logjson("MAIN", "{\"type\":\"reboot\"}" SEP);
	logjson("MAIN", "{\"version\":2}" SEP);

	if(BOOTLOADER_UICR_ADDRESS != 0xFFFFFFFF) logt("MAIN", "UICR boot address is %x, bootloader v%u", BOOTLOADER_UICR_ADDRESS, *(u32*)(BOOTLOADER_UICR_ADDRESS + 1024));
	logt("ERROR", "Reboot reason was %u, Flash config used:%u", GS->ramRetainStructPtr->rebootReason, useFlashConfig);

	//Start Watchdog and feed it
	FruityHal::StartWatchdog();
	FruityHal::FeedWatchdog();

	//Initialize GPIOTE for Buttons
	initGpioteButtons();

	//Initialialize the SoftDevice and the BLE stack
	FruityHal::BleStackInit(bleDispatchEventHandler, sysDispatchEventHandler, timerEventDispatch, dispatchButtonEvents);

	//Initialize NewStorage and RecordStorage
	NewStorage::getInstance();
	RecordStorage::getInstance()->InitialRepair();

	//Initialize GAP and GATT
	GAPController::getInstance()->bleConfigureGAP();
	GATTController::getInstance();
	AdvertisingController::getInstance()->Initialize();

	//Register a pre/post transmit hook for radio events
	if(Config->enableRadioNotificationHandler){
		//TODO: Will sometimes not initialize if there are flash operations at that time
		err = ble_radio_notification_init(RADIO_NOTIFICATION_IRQ_PRIORITY, NRF_RADIO_NOTIFICATION_DISTANCE_800US, radioEventDispatcher);
		if(err) logt("ERROR", "Could not initialize radio notifications %u", err);
	}

	//Init the magic
	GS->node = new Node();
	GS->node->Initialize();

	//Start Timers
	FruityHal::StartTimers();

//	TestBattery* testBattery = new TestBattery();
//	testBattery->startTesting();
//	testBattery->scanAt50Percent();

	FruityHal::EventLooper(bleDispatchEventHandler, sysDispatchEventHandler, timerEventDispatch, dispatchButtonEvents);
}

void detectBoardAndSetConfig(){
#ifdef DETECT_AND_SET_ARS100748_BOARD
	DETECT_AND_SET_ARS100748_BOARD();
#endif

//Could add other detect methods here...
}

//Checks the retained ram for crc failures and resets the reboot reason on mismatch
void checkRamRetainStruct(){
	//Check if crc matches and reset reboot reason if not
	if(GS->ramRetainStructPtr->rebootReason != 0){
		u32 crc = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, SIZEOF_RAM_RETAIN_STRUCT - 4);
		if(crc != GS->ramRetainStructPtr->crc32){
			memset(GS->ramRetainStructPtr, 0x00, SIZEOF_RAM_RETAIN_STRUCT);
		}
	}
	//If we did not save the reboot reason before rebooting, check if the HAL knows something
	if(GS->ramRetainStructPtr->rebootReason == 0){
		GS->ramRetainStructPtr->rebootReason = FruityHal::GetRebootReason();
	}
}

//Will be called if an error occurs somewhere in the code, but not if it's a hardfault
extern "C"
{
	//Trace function for unwinding the stack and saving the result in the retained ram
	_Unwind_Reason_Code stacktrace_handler(_Unwind_Context *ctx, void *d){
		int* depth = (int*)d;
		if(*depth < RAM_PERSIST_STACKSTRACE_SIZE){
			GS->ramRetainStructPtr->stacktrace[*depth] = _Unwind_GetIP(ctx);
			GS->ramRetainStructPtr->stacktraceSize = *depth;
		}
		(*depth)++;
		return _URC_NO_REASON;
	}

	//The app_error handler_bare is called by all APP_ERROR_CHECK functions when DEBUG is undefined
	void app_error_handler_bare(uint32_t error_code)
	{
		//Save the crashdump to the ramRetain struct so that we can evaluate it when rebooting
		memset((u8*)GS->ramRetainStructPtr, 0x00, SIZEOF_RAM_RETAIN_STRUCT);
		GS->ramRetainStructPtr->rebootReason = RebootReason::REBOOT_REASON_APP_FAULT;
		GS->ramRetainStructPtr->code1 = error_code;

		int depth = 0;
		_Unwind_Backtrace(&stacktrace_handler, &depth); //Unwind the stack and save into the retained ram

		GS->ramRetainStructPtr->crc32 = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, SIZEOF_RAM_RETAIN_STRUCT-4);

		GS->ledBlue->Off();
		GS->ledRed->Off();
		if(Config->debugMode) while(1){
			GS->ledGreen->Toggle();
			nrf_delay_us(50000);
		}

		else FruityHal::SystemReset();
	}

	//The app_error handler is called by all APP_ERROR_CHECK functions when DEBUG is defined
	void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
	{
		logt("ERROR", "App error code:%s(%u), file:%s, line:%u", Logger::getInstance()->getNrfErrorString(error_code), error_code, p_file_name, line_num);
		app_error_handler_bare(error_code);
	}

	//Called when the softdevice crashes
	void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
	{
		//Save the crashdump to the ramRetain struct so that we can evaluate it when rebooting
		memset((u8*)GS->ramRetainStructPtr, 0x00, SIZEOF_RAM_RETAIN_STRUCT);
		GS->ramRetainStructPtr->rebootReason = RebootReason::REBOOT_REASON_SD_FAULT;
		GS->ramRetainStructPtr->stacktraceSize = 1;
		GS->ramRetainStructPtr->stacktrace[0] = pc;
		GS->ramRetainStructPtr->code1 = id;

		switch(id){
			case NRF_FAULT_ID_SD_RANGE_START: //Softdevice Asserts, info is NULL
			{
				break;
			}
			case NRF_FAULT_ID_APP_RANGE_START: //Application asserts (e.g. wrong memory access), info is memory address
			{
				GS->ramRetainStructPtr->code2 = info;
				break;
			}
			 case NRF_FAULT_ID_SDK_ASSERT: //SDK asserts
			 {
				GS->ramRetainStructPtr->code2      		= ((assert_info_t *)info)->line_num;
				u8 len = strlen((const char*)((assert_info_t *)info)->p_file_name);
				if(len > (RAM_PERSIST_STACKSTRACE_SIZE-1)*4) len = (RAM_PERSIST_STACKSTRACE_SIZE-1)*4;
				memcpy(GS->ramRetainStructPtr->stacktrace + 1, ((assert_info_t *)info)->p_file_name, len);
				break;
			 }
			case NRF_FAULT_ID_SDK_ERROR: //SDK errors
			{
				GS->ramRetainStructPtr->code2     		 = ((error_info_t *)info)->line_num;
				GS->ramRetainStructPtr->code3     		 = ((error_info_t *)info)->err_code;

				//Copy filename to stacktrace
				u8 len = strlen((const char*)((error_info_t *)info)->p_file_name);
				if(len > (RAM_PERSIST_STACKSTRACE_SIZE-1)*4) len = (RAM_PERSIST_STACKSTRACE_SIZE-1)*4;
				memcpy(GS->ramRetainStructPtr->stacktrace + 1, ((error_info_t *)info)->p_file_name, len);
				break;
			}
		}

		GS->ramRetainStructPtr->crc32 = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, SIZEOF_RAM_RETAIN_STRUCT-4);

		logt("ERROR", "Softdevice fault id %u, pc %u, info %u", id, pc, info);

		GS->ledRed->Off();
		GS->ledGreen->Off();
		GS->ledBlue->Off();
		if(Config->debugMode) while(1){
			GS->ledRed->Toggle();
			GS->ledGreen->Toggle();
			nrf_delay_us(50000);
		}
		else FruityHal::SystemReset();
	}

	void HardFault_Handler(void)
	{
	    uint32_t *sp = (uint32_t *) __get_MSP(); // Get stack pointer
#if defined(NRF51)
	    uint32_t stacked_lr = sp[11];
	    uint32_t stacked_pc = sp[12];
	    uint32_t stacked_psr = sp[13];
#elif defined(NRF52)//Hasn't been thoroughly tested, pc seems to be alright, but not sure if lr and psr are correct
	    uint32_t stacked_lr = sp[7];
	    uint32_t stacked_pc = sp[8];
	    uint32_t stacked_psr = sp[9];
#endif

	    //Save the crashdump to the ramRetain struct so that we can evaluate it when rebooting
	    memset((u8*)GS->ramRetainStructPtr, 0x00, SIZEOF_RAM_RETAIN_STRUCT);
	    GS->ramRetainStructPtr->rebootReason = RebootReason::REBOOT_REASON_HARDFAULT;
	    GS->ramRetainStructPtr->stacktraceSize = 1;
	    GS->ramRetainStructPtr->stacktrace[0] = stacked_pc;
	    GS->ramRetainStructPtr->code1 = 0; //TODO: get called handler address
	    GS->ramRetainStructPtr->code2 = stacked_lr;
	    GS->ramRetainStructPtr->code3 = stacked_psr;
	    GS->ramRetainStructPtr->crc32 = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, SIZEOF_RAM_RETAIN_STRUCT-4);

	    GS->ledBlue->Off();
		GS->ledGreen->Off();
		if(Config->debugMode) while(1){
			GS->ledRed->Toggle();
			nrf_delay_us(50000);
		}
		else FruityHal::SystemReset();

	}

	//NRF52 uses more handlers, we rediect them for starters
#ifdef NRF52
	void MemoryManagement_Handler(){
		HardFault_Handler();
	}
	void BusFault_Handler(){
		HardFault_Handler();
	}
	void UsageFault_Handler(){
		HardFault_Handler();
	}
#endif

	//This handler receives UART interrupts if terminal mode is disabled
	void UART0_IRQHandler(void)
	{
		dispatchUartInterrupt();
	}

}

void dispatchButtonEvents(u8 buttonId, u32 buttonHoldTime)
{
	GS->node->ButtonHandler(buttonId, buttonHoldTime);

	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(GS->node != NULL && GS->node->activeModules[i] != 0  && GS->node->activeModules[i]->configurationPointer->moduleActive){
			GS->node->activeModules[i]->ButtonHandler(buttonId, buttonHoldTime);
		}
	}
}



void radioEventDispatcher(bool radioActive)
{
	Node::RadioEventHandler(radioActive);
}

void bleDispatchEventHandler(ble_evt_t * bleEvent)
{
	u16 eventId = bleEvent->header.evt_id;

	if(eventId == BLE_GAP_EVT_ADV_REPORT || eventId == BLE_GAP_EVT_RSSI_CHANGED){
		logt("EVENTS2", "BLE EVENT %s (%d)", Logger::getInstance()->getBleEventNameString(eventId), eventId);
	} else {
		logt("EVENTS", "BLE EVENT %s (%d)", Logger::getInstance()->getBleEventNameString(eventId), eventId);
	}

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
	if(eventId == BLE_GAP_EVT_ADV_REPORT || eventId == BLE_GAP_EVT_RSSI_CHANGED){
		logt("EVENTS2", "End of event");
	} else {
		logt("EVENTS", "End of event");
	}

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


//This function is called from the main event handling
void timerEventDispatch(u16 passedTime, u32 appTimer)
{
	//Call the timer handler from the node
	GS->node->TimerTickHandler(passedTime);

	GS->cm->TimerEventHandler(passedTime, appTimer);

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
#ifdef USE_BUTTONS
	//Activate GPIOTE if not already active
	nrf_drv_gpiote_init();

	//Register for both HighLow and LowHigh events
	//IF this returns NO_MEM, increase GPIOTE_CONFIG_NUM_OF_LOW_POWER_EVENTS
	nrf_drv_gpiote_in_config_t buttonConfig;
	buttonConfig.sense = NRF_GPIOTE_POLARITY_TOGGLE;
	buttonConfig.pull = NRF_GPIO_PIN_PULLUP;
	buttonConfig.is_watcher = false;
	buttonConfig.hi_accuracy = false;

	//This uses the SENSE low power feature, all pin events are reported
	//at the same GPIOTE channel
	u32 err =  nrf_drv_gpiote_in_init(Boardconfig->button1Pin, &buttonConfig, buttonInterruptHandler);

	//Enable the events
	nrf_drv_gpiote_in_event_enable(Boardconfig->button1Pin, true);
#endif
}

void buttonInterruptHandler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action){
	//GS->ledGreen->Toggle();

	//Because we don't know which state the button is in, we have to read it
	u32 state = nrf_gpio_pin_read(pin);

	//An interrupt generated by our button
	if(pin == (u8)Boardconfig->button1Pin){
		if(state == Boardconfig->buttonsActiveHigh){
			GS->button1PressTimeDs = GS->node->appTimerDs;
		} else if(state == !Boardconfig->buttonsActiveHigh && GS->button1PressTimeDs != 0){
			GS->button1HoldTimeDs = GS->node->appTimerDs - GS->button1PressTimeDs;
			GS->button1PressTimeDs = 0;
		}
	}
}

#ifndef SIM_ENABLED
extern "C"{
//Eliminate Exception overhead when using pure virutal functions
//http://elegantinvention.com/blog/information/smaller-binary-size-with-c-on-baremetal-g/
	void __cxa_pure_virtual() {
		logt("ERROR", "PVF call");
		APP_ERROR_CHECK(FRUITYMESH_ERROR_PURE_VIRTUAL_FUNCTION_CALL);
	}
}
#endif


#endif
/**
 *@}
 **/
