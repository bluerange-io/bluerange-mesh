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

#include <FruityMesh.h>
#include <Terminal.h>
#include <AdvertisingController.h>
#include <ScanController.h>
#include <GAPController.h>
#include <GATTController.h>


#include <Module.h>
#include <Node.h>
#include <StatusReporterModule.h>
#include <AdvertisingModule.h>
#include <DebugModule.h>
#include <ScanningModule.h>
#include <EnrollmentModule.h>
#include <MeshAccessModule.h>
#include <IoModule.h>
#include <Logger.h>
#include <LedWrapper.h>
#include <Utility.h>
#include <types.h>
#include <TestBattery.h>
#include <Config.h>
#include <FlashStorage.h>

#ifdef ACTIVATE_ASSET_MODULE
#include <AssetModule.h>
#endif

#ifdef ACTIVATE_DFU_MODULE
#include <DfuModule.h>
#endif

#ifdef ACTIVATE_EINK_MODULE
#include <EinkModule.h>
#endif

#ifdef ACTIVATE_ENOCEAN_MODULE
#include <EnOceanModule.h>
#endif

#ifdef ACTIVATE_MANAGEMENT_MODULE
#include <ManagementModule.h>
#endif

#ifdef ACTIVATE_CLC_MODULE
#include <ClcModule.h>
#include <ClcComm.h>
#endif

#ifdef ACTIVATE_VS_MODULE
#include <VsModule.h>
#include <VsComm.h>
#endif

#ifdef ACTIVATE_STACK_UNWINDING
#include <unwind.h>
#endif

#ifdef ACTIVATE_TESTING_MODULE
#include <TestingModule.h>
#endif

extern "C"{
#if defined(NRF51) || defined(SIM_ENABLED)
#include <softdevice_handler.h>
#elif defined(NRF52)
#include <nrf_sdh.h>
#include <nrf_power.h>
#endif

#include <app_timer.h>
#ifndef __ICCARM__
#include <malloc.h>
#endif
#include <nrf_sdm.h>
#ifndef SIM_ENABLED
#include <nrf_nvic.h>
#include <nrf_gpio.h>
#include <nrf_mbr.h>
#endif
}

#ifdef SIM_ENABLED
#include <CherrySim.h>
#endif

void BootFruityMesh()
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

#ifdef NRF52
	//Resetting the GPREGRET2 Retained register will block the nordic secure DFU bootloader
	//from starting the DFU process
	nrf_power_gpregret2_set(0x0);
#endif

	//Instanciate RecordStorage to load the board config
	FlashStorage::getInstance();
	RecordStorage::getInstance();

	//Load the board configuration which should then give us all the necessary pins and
	//configuration to proceed initializing everything else
	Boardconf::getInstance().Initialize(useFlashConfig);

	//Configure LED pins as output
	GS->ledRed = new LedWrapper(Boardconfig->led1Pin, Boardconfig->ledActiveHigh);
	GS->ledGreen = new LedWrapper(Boardconfig->led2Pin, Boardconfig->ledActiveHigh);
	GS->ledBlue = new LedWrapper(Boardconfig->led3Pin, Boardconfig->ledActiveHigh);

	//Blink LEDs once during boot as a signal for the user
	GS->ledRed->On();
	GS->ledGreen->On();
	GS->ledBlue->On();
	FruityHal::DelayMs(500);
	GS->ledRed->Off();
	GS->ledGreen->Off();
	GS->ledBlue->Off();

	//Load the configuration
	Conf::getInstance().Initialize(useFlashConfig);


	//Initialize the UART Terminal
	Terminal::getInstance().Init();
	Logger::getInstance().Init();

#ifdef SIM_ENABLED
	cherrySimInstance->ChooseSimulatorTerminal(); //TODO: Maybe remove
#endif

	//Enable logging for some interesting log tags
	Logger::getInstance().enableTag("MAIN");
	Logger::getInstance().enableTag("NODE");
	Logger::getInstance().enableTag("STORAGE");
	Logger::getInstance().enableTag("FLASH"); //FLASHSTORAGE
	Logger::getInstance().enableTag("DATA");
	Logger::getInstance().enableTag("SEC");
	Logger::getInstance().enableTag("HANDSHAKE");
//	Logger::getInstance().enableTag("DISCOVERY");
	Logger::getInstance().enableTag("CONN");
	Logger::getInstance().enableTag("STATES");
//	Logger::getInstance().enableTag("ADV");
//	Logger::getInstance().enableTag("SINK");
//	Logger::getInstance().enableTag("CM");
	Logger::getInstance().enableTag("DISCONNECT");
//	Logger::getInstance().enableTag("JOIN");
	Logger::getInstance().enableTag("GATTCTRL");
	Logger::getInstance().enableTag("CONN");
//	Logger::getInstance().enableTag("CONN_DATA");
	Logger::getInstance().enableTag("MACONN");
	Logger::getInstance().enableTag("EINK");
	Logger::getInstance().enableTag("RCONN");
	Logger::getInstance().enableTag("CONFIG");
	Logger::getInstance().enableTag("RS");
	//	Logger::getInstance().enableTag("PQ");
	Logger::getInstance().enableTag("C");
//	Logger::getInstance().enableTag("FH");
	Logger::getInstance().enableTag("TEST");
	Logger::getInstance().enableTag("MODULE");
	Logger::getInstance().enableTag("STATUSMOD");
//	Logger::getInstance().enableTag("DEBUGMOD");
	Logger::getInstance().enableTag("ENROLLMOD");
//	Logger::getInstance().enableTag("IOMOD");
//	Logger::getInstance().enableTag("SCANMOD");
//	Logger::getInstance().enableTag("PINGMOD");
	Logger::getInstance().enableTag("DFUMOD");
	Logger::getInstance().enableTag("CLCMOD");
	Logger::getInstance().enableTag("MAMOD");
	//	Logger::getInstance().enableTag("CLCCOMM");
//	Logger::getInstance().enableTag("VSMOD");
	Logger::getInstance().enableTag("VSDBG");
//	Logger::getInstance().enableTag("ASMOD");
//	Logger::getInstance().enableTag("EVENTS");
	
	Logger::getInstance().logError(ErrorTypes::REBOOT, GS->ramRetainStructPtr->stacktrace[0], GS->ramRetainStructPtr->rebootReason);

	//If the nordic secure dfu bootloader is enabled, disable it as soon as fruitymesh boots the first time
	disableNordicDfuBootloader();

	logjson("MAIN", "{\"type\":\"reboot\"}" SEP);
	logjson("MAIN", "{\"version\":2}" SEP);

	//Stacktrace can be evaluated using addr2line -e FruityMesh.out 12345
	if(BOOTLOADER_UICR_ADDRESS != 0xFFFFFFFF) logt("MAIN", "UICR boot address is %x, bootloader v%u", BOOTLOADER_UICR_ADDRESS, FruityHal::GetBootloaderVersion());
	logt("MAIN", "Reboot reason was %u, Flash config used:%u, stacktrace %u", GS->ramRetainStructPtr->rebootReason, useFlashConfig, GS->ramRetainStructPtr->stacktrace[0]);

	//Start Watchdog and feed it
	FruityHal::StartWatchdog();
	FruityHal::FeedWatchdog();

	//Initialize GPIOTE for Buttons
	FruityHal::InitializeButtons();

	//Initialialize the SoftDevice and the BLE stack
	FruityHal::SetEventHandlers(
			DispatchBleEvents, DispatchSystemEvents, DispatchTimerEvents,
			DispatchButtonEvents, FruityMeshErrorHandler,
			BleStackErrorHandler, HardFaultErrorHandler);
	err = FruityHal::BleStackInit();

#ifdef SIM_ENABLED
	if(err == NRF_SUCCESS) cherrySimInstance->currentNode->state.initialized = true; //TODO: Remove or move to FruityHal
#endif

	//Initialize NewStorage and RecordStorage
	FlashStorage::getInstance();
	RecordStorage::getInstance().InitialRepair();

	//Initialize GAP and GATT
	GAPController::getInstance().bleConfigureGAP();
	GATTController::getInstance();
	AdvertisingController::getInstance().Initialize();
	ScanController::getInstance();

	//Initialize ConnectionManager
	ConnectionManager::getInstance();

	//Instanciating the node is mandatory as many other modules use its functionality
	GS->activeModules[0] = GS->node = new Node();

	//Instanciate all other modules as necessary

#ifdef ACTIVATE_DEBUG_MODULE
	GS->activeModules[1] = new DebugModule();
#endif
#ifdef ACTIVATE_DFU_MODULE
	GS->activeModules[2] = new DFUModule();
#endif
#ifdef ACTIVATE_STATUS_REPORTER_MODULE
	GS->activeModules[3] = new StatusReporterModule();
#endif
#ifdef ACTIVATE_ADVERTISING_MODULE
	GS->activeModules[4] = new AdvertisingModule();
#endif
#ifdef ACTIVATE_SCANNING_MODULE
	GS->activeModules[5] = new ScanningModule();
#endif
#ifdef ACTIVATE_ENROLLMENT_MODULE
	GS->activeModules[6] = new EnrollmentModule();
#endif
#ifdef ACTIVATE_IO_MODULE
	GS->activeModules[7] = new IoModule();
#endif
#ifdef ACTIVATE_CLC_MODULE
	GS->activeModules[8] = new ClcModule();
#endif
#ifdef ACTIVATE_MA_MODULE
	GS->activeModules[9] = new MeshAccessModule();
#endif
#ifdef ACTIVATE_VS_MODULE
	GS->activeModules[10] = new VsModule();
#endif
#ifdef ACTIVATE_ENOCEAN_MODULE
	GS->activeModules[11] = new EnOceanModule();
#endif
#ifdef ACTIVATE_ASSET_MODULE
	GS->activeModules[12] = new AssetModule();
#endif
#ifdef ACTIVATE_MANAGEMENT_MODULE
	GS->activeModules[13] = new ManagementModule();
#endif
#ifdef ACTIVATE_EINK_MODULE
	GS->activeModules[14] = new EinkModule();
#endif
#ifdef ACTIVATE_TESTING_MODULE
	GS->activeModules[15] = new TestingModule();
#endif
	//Start all Modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(GS->activeModules[i] != nullptr){
			GS->activeModules[i]->LoadModuleConfigurationAndStart();
		}
	}

//	TestBattery* testBattery = new TestBattery();
//	testBattery->startTesting();
//	testBattery->scanAt50Percent();

	//Configure a periodic timer that will call the TimerEventHandlers
	FruityHal::StartTimers();

}

void StartFruityMesh()
{
	//Start an Event Looper that will fetch all sorts of events and pass them to the correct dispatcher function
	FruityHal::EventLooper();
}

/**
################ Event Dispatchers ###################
The Event Dispatchers will distribute events to all necessary parts of FruityMesh
 */
#define ___________EVENT_DISPATCHERS____________________

void DispatchBleEvents(ble_evt_t &bleEvent)
{
	u16 eventId = bleEvent.header.evt_id;

	if(eventId == BLE_GAP_EVT_ADV_REPORT || eventId == BLE_GAP_EVT_RSSI_CHANGED){
		logt("EVENTS2", "BLE EVENT %s (%d)", Logger::getInstance().getBleEventNameString(eventId), eventId);
	} else {
		logt("EVENTS", "BLE EVENT %s (%d)", Logger::getInstance().getBleEventNameString(eventId), eventId);
	}

//	if(
//			bleEvent->header.evt_id != BLE_GAP_EVT_RSSI_CHANGED &&
//			bleEvent->header.evt_id != BLE_GAP_EVT_ADV_REPORT
//	) trace("<");

#ifdef ENABLE_FAKE_NODE_POSITIONS
	GS->node->modifyEventForFakePositions(&bleEvent);
#endif

	//Give events to all controllers
	GAPController::getInstance().bleConnectionEventHandler(bleEvent);
	AdvertisingController::getInstance().AdvertiseEventHandler(bleEvent);
	ScanController::getInstance().ScanEventHandler(bleEvent);
	GATTController::getInstance().bleMeshServiceEventHandler(bleEvent);

	if(GS->node != nullptr){
		GS->cm->BleEventHandler(bleEvent);
	}

	//Dispatch ble events to all modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(GS->activeModules[i] != nullptr && GS->activeModules[i]->configurationPointer->moduleActive){
			GS->activeModules[i]->BleEventHandler(bleEvent);
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

//System Events such as e.g. flash operation success or failure are dispatched with this function
void DispatchSystemEvents(u32 sys_evt)
{
	//Hand system events to new storage class
	FlashStorage::getInstance().SystemEventHandler(sys_evt);

	//Dispatch system events to all modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(GS->activeModules[i] != nullptr && GS->activeModules[i]->configurationPointer->moduleActive){
			GS->activeModules[i]->SystemEventHandler(sys_evt);
		}
	}
}

//This function dispatches once a Button was pressed for some time
void DispatchButtonEvents(u8 buttonId, u32 buttonHoldTime)
{
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(GS->activeModules[i] != nullptr && GS->activeModules[i]->configurationPointer->moduleActive){
			GS->activeModules[i]->ButtonHandler(buttonId, buttonHoldTime);
		}
	}
}

//This function is called from the main event handling
void DispatchTimerEvents(u16 passedTimeDs)
{
	//Update the app timer (The app timer has a drift when comparing it to the
	//config value in deciseconds because these do not convert nicely into ticks)
	GS->appTimerDs += passedTimeDs;

	UpdateGlobalTime();

	GS->cm->TimerEventHandler(passedTimeDs);

	FlashStorage::getInstance().TimerEventHandler(passedTimeDs);

	AdvertisingController::getInstance().TimerEventHandler(passedTimeDs);

	ScanController::getInstance().TimerEventHandler(passedTimeDs);

	//Dispatch event to all modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(GS->activeModules[i] != nullptr && GS->activeModules[i]->configurationPointer->moduleActive){
			GS->activeModules[i]->TimerEventHandler(passedTimeDs);
		}
	}

#ifdef ACTIVATE_CLC_COMM
	//The CLC Communication is not a module, so we need to distribute our timer as well
	ClcComm::getInstance().TimerEventHandler(passedTimeDs);
#endif

#ifdef ACTIVATE_VS_MODULE
	//The VS Communication is not a module, so we need to distribute our timer as well
	VsComm::getInstance().TimerEventHandler(passedTimeDs);
#endif
}

//################################################
#define ______________ERROR_HANDLERS______________

extern "C"{
	#ifdef ACTIVATE_STACK_UNWINDING
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
	#endif
}

//Called if the error check routine is called at some point in FruityMesh and the result is not SUCCESS
void FruityMeshErrorHandler(u32 err)
{
	SIMEXCEPTION(FruityMeshException);

	//Protect against saving the fault if another fault was the case for this fault
	if(GS->ramRetainStructPtr->rebootReason != 0){
		NVIC_SystemReset();
	}

	//Save the crashdump to the ramRetain struct so that we can evaluate it when rebooting
	memset((u8*)GS->ramRetainStructPtr, 0x00, sizeof(RamRetainStruct));
	GS->ramRetainStructPtr->rebootReason = RebootReason::REBOOT_REASON_APP_FAULT;
	GS->ramRetainStructPtr->code1 = err;

	#ifdef ACTIVATE_STACK_UNWINDING
	int depth = 0;
	_Unwind_Backtrace(&stacktrace_handler, &depth); //Unwind the stack and save into the retained ram
	#endif

	GS->ramRetainStructPtr->crc32 = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, sizeof(RamRetainStruct)-4);

	//1 Second delay to write out debug messages before reboot
	FruityHal::DelayMs(1000);

	GS->ledBlue->Off();
	GS->ledRed->Off();
	if(Config->debugMode) while(1){
		GS->ledGreen->Toggle();
		FruityHal::DelayMs(50);
	}

	else FruityHal::SystemReset();
}

//If the BLE Stack fails, it will call this function with the error id, the programCounter and some additional info
void BleStackErrorHandler(u32 id, u32 pc, u32 info)
{
	SIMEXCEPTION(BLEStackError);

	//Protect against saving the fault if another fault was the case for this fault
	if(GS->ramRetainStructPtr->rebootReason != 0){
		NVIC_SystemReset();
	}

	//Save the crashdump to the ramRetain struct so that we can evaluate it when rebooting
	memset((u8*)GS->ramRetainStructPtr, 0x00, sizeof(RamRetainStruct));
	GS->ramRetainStructPtr->rebootReason = RebootReason::REBOOT_REASON_SD_FAULT;
	GS->ramRetainStructPtr->stacktraceSize = 1;
	GS->ramRetainStructPtr->stacktrace[0] = pc;
	GS->ramRetainStructPtr->code1 = id;

	switch(id){
		case NRF_FAULT_ID_SD_RANGE_START: //Softdevice Asserts, info is nullptr
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
			u8 len = (u8)strlen((const char*)((assert_info_t *)info)->p_file_name);
			if(len > (RAM_PERSIST_STACKSTRACE_SIZE-1)*4) len = (RAM_PERSIST_STACKSTRACE_SIZE-1)*4;
			memcpy(GS->ramRetainStructPtr->stacktrace + 1, ((assert_info_t *)info)->p_file_name, len);
			break;
		 }
		case NRF_FAULT_ID_SDK_ERROR: //SDK errors
		{
			GS->ramRetainStructPtr->code2     		 = ((error_info_t *)info)->line_num;
			GS->ramRetainStructPtr->code3     		 = ((error_info_t *)info)->err_code;

			//Copy filename to stacktrace
			u8 len = (u8)strlen((const char*)((error_info_t *)info)->p_file_name);
			if(len > (RAM_PERSIST_STACKSTRACE_SIZE-1)*4) len = (RAM_PERSIST_STACKSTRACE_SIZE-1)*4;
			memcpy(GS->ramRetainStructPtr->stacktrace + 1, ((error_info_t *)info)->p_file_name, len);
			break;
		}
	}

	GS->ramRetainStructPtr->crc32 = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, sizeof(RamRetainStruct)-4);

	logt("ERROR", "Softdevice fault id %u, pc %u, info %u", id, pc, info);

    //1 Second delay to write out debug messages before reboot
	FruityHal::DelayMs(1000);

	GS->ledRed->Off();
	GS->ledGreen->Off();
	GS->ledBlue->Off();
	if(Config->debugMode) while(1){
		GS->ledRed->Toggle();
		GS->ledGreen->Toggle();
		FruityHal::DelayMs(50);
	}
	else NVIC_SystemReset();
}

//Once a hardfault occurs, this handler is called with a pointer to a register dump
void HardFaultErrorHandler(stacked_regs_t* stack)
{
	SIMEXCEPTION(HardfaultException);

	//Protect against saving the fault if another fault was the case for this fault
	if(GS->ramRetainStructPtr->rebootReason != 0){
		NVIC_SystemReset();
	}

	//Save the crashdump to the ramRetain struct so that we can evaluate it when rebooting
	memset((u8*)GS->ramRetainStructPtr, 0x00, sizeof(RamRetainStruct));
	GS->ramRetainStructPtr->rebootReason = RebootReason::REBOOT_REASON_HARDFAULT;
	GS->ramRetainStructPtr->stacktraceSize = 1;
	GS->ramRetainStructPtr->stacktrace[0] = stack->pc;
	GS->ramRetainStructPtr->code1 = stack->pc;
	GS->ramRetainStructPtr->code2 = stack->lr;
	GS->ramRetainStructPtr->code3 = stack->psr;
	GS->ramRetainStructPtr->crc32 = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, sizeof(RamRetainStruct) -4);

	//1 Second delay to write out debug messages via segger rtt before reboot
	FruityHal::DelayMs(1000);

	if(Config->debugMode){
		GS->ledBlue->Off();
		GS->ledGreen->Off();
		while(1){
			GS->ledRed->Toggle();
			FruityHal::DelayMs(50);
		}
	}
	else NVIC_SystemReset();
}



//################ Other
#define ______________OTHER______________

void UpdateGlobalTime(){
	//Request the Realtimeclock counter
	u32 rtc1Ticks, passedTimeTicks;
	rtc1Ticks = FruityHal::GetRtc();
	passedTimeTicks = FruityHal::GetRtcDifference(rtc1Ticks, GS->previousRtcTicks);
	GS->previousRtcTicks = rtc1Ticks;

	//Update the global time seconds and save the remainder for the next iteration
	passedTimeTicks += GS->globalTimeRemainderTicks;
	GS->globalTimeSec += passedTimeTicks / APP_TIMER_CLOCK_FREQ;
	GS->globalTimeRemainderTicks = passedTimeTicks % APP_TIMER_CLOCK_FREQ;
}

/**
 * The RamRetainStruct is saved in a special section in RAM that is persisted between system resets (most of the time)
 * We use this section to store information about the last error that occured before a reboot
 * After rebooting, we can read that struct to send the error information over the mesh
 * Because the ram might be corrupted upon reset, we also save a crc and clear the struct if it does not match
 */
void checkRamRetainStruct(){
#ifndef SIM_ENABLED
	//Check if crc matches and reset reboot reason if not
	if(GS->ramRetainStructPtr->rebootReason != 0){
		u32 crc = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, sizeof(RamRetainStruct) - 4);
		if(crc != GS->ramRetainStructPtr->crc32){
			memset(GS->ramRetainStructPtr, 0x00, sizeof(RamRetainStruct));
		}
	}
	//If we did not save the reboot reason before rebooting, check if the HAL knows something
	if(GS->ramRetainStructPtr->rebootReason == 0){
		memset(GS->ramRetainStructPtr, 0x00, sizeof(RamRetainStruct));
		GS->ramRetainStructPtr->rebootReason = FruityHal::GetRebootReason();
	}
#endif
}

void disableNordicDfuBootloader(){
#ifndef SIM_ENABLED
	bool bootloaderAvailable = (BOOTLOADER_UICR_ADDRESS != 0xFFFFFFFF);
	u32 bootloaderAddress = BOOTLOADER_UICR_ADDRESS;

	//Check if a bootloader exists
	if(bootloaderAddress != 0xFFFFFFFFUL){
		u32* magicNumberAddress = (u32*)NORDIC_DFU_MAGIC_NUMBER_ADDRESS;
		//Check if the magic number is currently set to enable nordic dfu
		if(*magicNumberAddress == ENABLE_NORDIC_DFU_MAGIC_NUMBER){
			logt("WARNING", "Disabling nordic dfu");

			//Overwrite the magic number so that the nordic dfu will be inactive afterwards
			u32 data = 0x00;
			GS->flashStorage->CacheAndWriteData(&data, magicNumberAddress, sizeof(u32), nullptr, 0);
		}
	}
#endif
}


