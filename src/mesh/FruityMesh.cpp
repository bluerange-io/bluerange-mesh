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
#include "Boardconfig.h"
#include "GlobalState.h"


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
#include <FlashStorage.h>

#ifndef GITHUB_RELEASE
#if IS_ACTIVE(ASSET_MODULE)
#include <AssetModule.h>
#endif

#if IS_ACTIVE(EINK_MODULE)
#include <EinkModule.h>
#endif

#if IS_ACTIVE(CLC_MODULE)
#include <ClcModule.h>
#include <ClcComm.h>
#endif
#endif //GITHUB_RELEASE

#if IS_ACTIVE(VS_MODULE)
#ifndef GITHUB_RELEASE
#include <VsModule.h>
#include <VsComm.h>
#endif //GITHUB_RELEASE
#endif

#if IS_ACTIVE(STACK_UNWINDING)
#include <unwind.h>
#endif

#if IS_ACTIVE(WM_MODULE)
#ifndef GITHUB_RELEASE
#include <WmModule.h>
#endif //GITHUB_RELEASE
#endif

#ifdef SIM_ENABLED
#include <CherrySim.h>
#endif

void BootFruityMesh()
{

	//Check for reboot reason
	checkRamRetainStruct();

	//If reboot reason is empty (clean power bootup) or
	bool safeBootEnabled = false;
#if IS_ACTIVE(WATCHDOG_SAFE_BOOT_MODE)
	if (Utility::IsUnknownRebootReason(GS->ramRetainStructPtr->rebootReason)
		|| *GS->rebootMagicNumberPtr == REBOOT_MAGIC_NUMBER) {
		safeBootEnabled = false;
		*GS->rebootMagicNumberPtr = 0;
	}
	else {
		safeBootEnabled = true;
		*GS->rebootMagicNumberPtr = REBOOT_MAGIC_NUMBER;
		SIMEXCEPTION(SafeBootTriggeredException);
	}
#endif

	//Resetting the GPREGRET2 Retained register will block the nordic secure DFU bootloader
	//from starting the DFU process
	FruityHal::setRetentionRegisterTwo(0x0);

	//Instanciate RecordStorage to load the board config
	RecordStorage::getInstance().Init();

	//Load the board configuration which should then give us all the necessary pins and
	//configuration to proceed initializing everything else
	//Load board configuration from flash only if it is not in safe boot mode
	Boardconf::getInstance().Initialize();

	//Configure LED pins as output
	GS->ledRed.Init(Boardconfig->led1Pin, Boardconfig->ledActiveHigh);
	GS->ledGreen.Init(Boardconfig->led2Pin, Boardconfig->ledActiveHigh);
	GS->ledBlue.Init(Boardconfig->led3Pin, Boardconfig->ledActiveHigh);

	//Blink LEDs once during boot as a signal for the user
	GS->ledRed.On();
	GS->ledGreen.On();
	GS->ledBlue.On();
	FruityHal::DelayMs(500);
	GS->ledRed.Off();
	GS->ledGreen.Off();
	GS->ledBlue.Off();

	//Load the configuration from flash only if it is not in safeBoot mode
	Conf::getInstance().Initialize(safeBootEnabled);


	//Initialize the UART Terminal
	Terminal::getInstance().Init();
	Logger::getInstance().Init();

	//Initialize ConnectionManager
	ConnectionManager::getInstance();
	ConnectionManager::getInstance().Init();
#ifdef SIM_ENABLED
	cherrySimInstance->ChooseSimulatorTerminal(); //TODO: Maybe remove
#endif

#if IS_INACTIVE(GW_SAVE_SPACE)
	//Enable logging for some interesting log tags
	Logger::getInstance().enableTag("MAIN");
	Logger::getInstance().enableTag("INS");
	Logger::getInstance().enableTag("NODE");
	Logger::getInstance().enableTag("STORAGE");
	Logger::getInstance().enableTag("FLASH"); //FLASHSTORAGE
	Logger::getInstance().enableTag("DATA");
	Logger::getInstance().enableTag("SEC");
	Logger::getInstance().enableTag("HANDSHAKE");
//	Logger::getInstance().enableTag("DECISION");
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
//	Logger::getInstance().enableTag("VSCOMM");
	Logger::getInstance().enableTag("ASMOD");
//	Logger::getInstance().enableTag("EVENTS");
//	Logger::getInstance().enableTag("SC");
//	Logger::getInstance().enableTag("WMCOMM");
//	Logger::getInstance().enableTag("WMMOD");
//	Logger::getInstance().enableTag("BME");
//	Logger::getInstance().enableTag("ADVS");
#endif
	
	//Log the reboot reason to our ram log so that it is automatically queried by the sink
	Logger::getInstance().logError(LoggingError::REBOOT, (u32)GS->ramRetainStructPtr->rebootReason, GS->ramRetainStructPtr->code1);
	
	//If the nordic secure dfu bootloader is enabled, disable it as soon as fruitymesh boots the first time
#if IS_INACTIVE(GW_SAVE_SPACE)
	FruityHal::disableHardwareDfuBootloader();
#endif

	if (GS->ramRetainStructPtr->rebootReason == RebootReason::FACTORY_RESET_SUCCEEDED_FAILSAFE)
	{
		//The failsafe should not happen. If it does, it indicates to some implementation error.
		SIMEXCEPTION(IllegalStateException);
	}

	Utility::LogRebootJson();

	//Stacktrace can be evaluated using addr2line -e FruityMesh.out 1D3FF
	//Note: Convert to hex first!
	if(FruityHal::GetBootloaderAddress() != 0xFFFFFFFF) logt("MAIN", "UICR boot address is %x, bootloader v%u", (u32)FruityHal::GetBootloaderAddress(), FruityHal::GetBootloaderVersion());
	logt("MAIN", "Reboot reason was %u, SafeBootEnabled:%u, stacktrace %u", (u32)GS->ramRetainStructPtr->rebootReason, safeBootEnabled, GS->ramRetainStructPtr->stacktrace[0]);

	//Start Watchdog and feed it
	FruityHal::StartWatchdog(safeBootEnabled);
	FruityHal::FeedWatchdog();

	//Initialize GPIOTE for Buttons
	FruityHal::InitializeButtons();

#if IS_ACTIVE(BUTTONS)
	FruityHal::ButtonEventHandler buttonHandler = DispatchButtonEvents;
#else
	FruityHal::ButtonEventHandler buttonHandler = nullptr;
#endif

	//Register error and event handlers
	GS->SetEventHandlers(
			DispatchSystemEvents, DispatchTimerEvents,
			buttonHandler, FruityMeshErrorHandler,
			BleStackErrorHandler, HardFaultErrorHandler);

#if IS_ACTIVE(VIRTUAL_COM_PORT)
	FruityHal::VirtualComInitBeforeStack();
#endif

	//Initialize the BLE Stack
	ErrorType err = FruityHal::BleStackInit();

#if IS_ACTIVE(VIRTUAL_COM_PORT)
	FruityHal::VirtualComInitAfterStack(Terminal::VirtualComPortEventHandler);
#endif

#ifdef SIM_ENABLED
	if(err == ErrorType::SUCCESS) cherrySimInstance->currentNode->state.initialized = true; //TODO: Remove or move to FruityHal
#endif

	//Initialize GAP and GATT
	GAPController::getInstance().bleConfigureGAP();
	GATTController::getInstance().Init();
	AdvertisingController::getInstance().Initialize();
	ScanController::getInstance();

}

void BootModules()
{
	//Instanciating the node is mandatory as many other modules use its functionality
	GS->node.Init();
	GS->activeModules[0] = &GS->node;
	GS->amountOfModules++;

	//Instanciate all other modules as necessary

	// Init timers in case any module is using it.
#ifndef SIM_ENABLED
	FruityHal::InitTimers();
#endif

	INITIALIZE_MODULES(true);

	//Start all Modules
	for (u32 i = 0; i < GS->amountOfModules; i++) {
		GS->activeModules[i]->LoadModuleConfigurationAndStart();
	}

	//Configure a periodic timer that will call the TimerEventHandlers
#ifndef SIM_ENABLED
	logt("ERROR", "Timer start");
	FruityHal::StartTimers();
#endif

	GS->ramRetainStructPreviousBoot = *GS->ramRetainStructPtr;
	FruityHal::ClearRebootReason();
	CheckedMemset(GS->ramRetainStructPtr, 0, sizeof(RamRetainStruct));
	GS->ramRetainStructPtr->rebootReason = RebootReason::UNKNOWN_BUT_BOOTED;
	GS->ramRetainStructPtr->crc32 = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, sizeof(RamRetainStruct) - 4);
}

void StartFruityMesh()            //LCOV_EXCL_LINE Simulated in a different way
{					              //LCOV_EXCL_LINE Simulated in a different way
	//Start an Event Looper that will fetch all sorts of events and pass them to the correct dispatcher function
	while (true) {				  //LCOV_EXCL_LINE Simulated in a different way
		FruityHal::EventLooper(); //LCOV_EXCL_LINE Simulated in a different way
	}							  //LCOV_EXCL_LINE Simulated in a different way
}

/**
################ Event Dispatchers ###################
The Event Dispatchers will distribute events to all necessary parts of FruityMesh
 */
#define ___________EVENT_DISPATCHERS____________________

//System Events such as e.g. flash operation success or failure are dispatched with this function
void DispatchSystemEvents(FruityHal::SystemEvents sys_evt)
{
	//Hand system events to new storage class
	FlashStorage::getInstance().SystemEventHandler(sys_evt);
}

//This function dispatches once a Button was pressed for some time
#if IS_ACTIVE(BUTTONS)
void DispatchButtonEvents(u8 buttonId, u32 buttonHoldTime)
{
	logt("WARN", "Button %u pressed %u", buttonId, buttonHoldTime);
	for(u32 i=0; i<GS->amountOfModules; i++){
		if(GS->activeModules[i]->configurationPointer->moduleActive){
			GS->activeModules[i]->ButtonHandler(buttonId, buttonHoldTime);
		}
	}
}
#endif

//This function is called from the main event handling
void DispatchTimerEvents(u16 passedTimeDs)
{
	//Update the app timer (The app timer has a drift when comparing it to the
	//config value in deciseconds because these do not convert nicely into ticks)
	GS->appTimerDs += passedTimeDs;

	GS->timeManager.ProcessTicks();

	GS->cm.TimerEventHandler(passedTimeDs);

	FlashStorage::getInstance().TimerEventHandler(passedTimeDs);

	AdvertisingController::getInstance().TimerEventHandler(passedTimeDs);

	ScanController::getInstance().TimerEventHandler(passedTimeDs);

	//Dispatch event to all modules
	for(u32 i=0; i<GS->amountOfModules; i++){
		if(GS->activeModules[i]->configurationPointer->moduleActive){
			GS->activeModules[i]->TimerEventHandler(passedTimeDs);
		}
	}
}

void DispatchEvent(const FruityHal::GapRssiChangedEvent & e)
{
	GS->cm.GapRssiChangedEventHandler(e);
}

void DispatchEvent(const FruityHal::GapAdvertisementReportEvent & e)
{
	ScanController::getInstance().ScanEventHandler(e);
	for (u32 i = 0; i < GS->amountOfModules; i++) {
		if (GS->activeModules[i]->configurationPointer->moduleActive) {
			GS->activeModules[i]->GapAdvertisementReportEventHandler(e);
		}
	}
}

void DispatchEvent(const FruityHal::GapConnectedEvent & e)
{
	GAPController::getInstance().GapConnectedEventHandler(e);
	AdvertisingController::getInstance().GapConnectedEventHandler(e);
	for (u32 i = 0; i < GS->amountOfModules; i++) {
		if (GS->activeModules[i]->configurationPointer->moduleActive) {
			GS->activeModules[i]->GapConnectedEventHandler(e);
		}
	}
}

void DispatchEvent(const FruityHal::GapDisconnectedEvent & e)
{
	GAPController::getInstance().GapDisconnectedEventHandler(e);
	AdvertisingController::getInstance().GapDisconnectedEventHandler(e);
	for (u32 i = 0; i < GS->amountOfModules; i++) {
		if (GS->activeModules[i]->configurationPointer->moduleActive) {
			GS->activeModules[i]->GapDisconnectedEventHandler(e);
		}
	}
}

void DispatchEvent(const FruityHal::GapTimeoutEvent & e)
{
	GAPController::getInstance().GapTimeoutEventHandler(e);
}

void DispatchEvent(const FruityHal::GapSecurityInfoRequestEvent & e)
{
	GAPController::getInstance().GapSecurityInfoRequestEvenetHandler(e);
}

void DispatchEvent(const FruityHal::GapConnectionSecurityUpdateEvent & e)
{
	GAPController::getInstance().GapConnectionSecurityUpdateEventHandler(e);
}

void DispatchEvent(const FruityHal::GattcWriteResponseEvent & e)
{
	ConnectionManager::getInstance().GattcWriteResponseEventHandler(e);
}

void DispatchEvent(const FruityHal::GattcTimeoutEvent & e)
{
	ConnectionManager::getInstance().GattcTimeoutEventHandler(e);
}

void DispatchEvent(const FruityHal::GattsWriteEvent & e)
{
	ConnectionManager::getInstance().GattsWriteEventHandler(e);
}

void DispatchEvent(const FruityHal::GattcHandleValueEvent & e)
{
	ConnectionManager::getInstance().GattcHandleValueEventHandler(e);
}

void DispatchEvent(const FruityHal::GattDataTransmittedEvent & e)
{
	ConnectionManager::getInstance().GattDataTransmittedEventHandler(e);
	for (int i = 0; i < MAX_MODULE_COUNT; i++) {
		if (GS->activeModules[i] != nullptr
			&& GS->activeModules[i]->configurationPointer->moduleActive) {
			GS->activeModules[i]->GattDataTransmittedEventHandler(e);
		}
	}
}

//################################################
#define ______________ERROR_HANDLERS______________

extern "C"{
	#if IS_ACTIVE(STACK_UNWINDING)
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

	//1 Second delay to write out debug messages before reboot
	FruityHal::DelayMs(1000);

	//Protect against saving the fault if another fault was the case for this fault
	if(!Utility::IsUnknownRebootReason(GS->ramRetainStructPtr->rebootReason)){
		NVIC_SystemReset();
	}

	//Save the crashdump to the ramRetain struct so that we can evaluate it when rebooting
	CheckedMemset((u8*)GS->ramRetainStructPtr, 0x00, sizeof(RamRetainStruct));
	GS->ramRetainStructPtr->rebootReason = RebootReason::APP_FAULT;
	GS->ramRetainStructPtr->code1 = err;

#if IS_ACTIVE(STACK_UNWINDING)
	int depth = 0;
	_Unwind_Backtrace(&stacktrace_handler, &depth); //Unwind the stack and save into the retained ram
	#endif

	GS->ramRetainStructPtr->crc32 = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, sizeof(RamRetainStruct)-4);


	GS->ledBlue.Off();
	GS->ledRed.Off();
	if(Conf::debugMode) while(1){
		GS->ledGreen.Toggle();
		FruityHal::DelayMs(50);
	}

	else FruityHal::SystemReset();
}

//If the BLE Stack fails, it will call this function with the error id, the programCounter and some additional info
void BleStackErrorHandler(u32 id, u32 pc, u32 info)
{
	SIMEXCEPTION(BLEStackError);

    //1 Second delay to write out debug messages before reboot
	FruityHal::DelayMs(1000);

	//Protect against saving the fault if another fault was the case for this fault
	if(!Utility::IsUnknownRebootReason(GS->ramRetainStructPtr->rebootReason)){
		NVIC_SystemReset();
	}

	//Save the crashdump to the ramRetain struct so that we can evaluate it when rebooting
	CheckedMemset((u8*)GS->ramRetainStructPtr, 0x00, sizeof(RamRetainStruct));
	GS->ramRetainStructPtr->rebootReason = RebootReason::SD_FAULT;
	GS->ramRetainStructPtr->code1 = pc;
	GS->ramRetainStructPtr->code2 = id;

	FruityHal::bleStackErrorHandler(id, info);

	GS->ramRetainStructPtr->crc32 = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, sizeof(RamRetainStruct)-4);

	logt("ERROR", "Softdevice fault id %u, pc %u, info %u", id, pc, info);

    //1 Second delay to write out debug messages before reboot
	FruityHal::DelayMs(1000);

	GS->ledRed.Off();
	GS->ledGreen.Off();
	GS->ledBlue.Off();
	if(Conf::debugMode) while(1){
		GS->ledRed.Toggle();
		GS->ledGreen.Toggle();
		FruityHal::DelayMs(50);
	}
	else NVIC_SystemReset();
}

//Once a hardfault occurs, this handler is called with a pointer to a register dump
void HardFaultErrorHandler(stacked_regs_t* stack)
{
	SIMEXCEPTION(HardfaultException);

    //1 Second delay to write out debug messages before reboot
	FruityHal::DelayMs(1000);

	//Save the crashdump to the ramRetain struct so that we can evaluate it when rebooting
	CheckedMemset((u8*)GS->ramRetainStructPtr, 0x00, sizeof(RamRetainStruct));
	GS->ramRetainStructPtr->rebootReason = RebootReason::HARDFAULT;
	GS->ramRetainStructPtr->stacktraceSize = 1;
	GS->ramRetainStructPtr->stacktrace[0] = stack->pc;
	GS->ramRetainStructPtr->code1 = stack->pc;
	GS->ramRetainStructPtr->code2 = stack->lr;
	GS->ramRetainStructPtr->code3 = stack->psr;
	GS->ramRetainStructPtr->crc32 = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, sizeof(RamRetainStruct) -4);

	if(Conf::debugMode){
		GS->ledBlue.Off();
		GS->ledGreen.Off();
		while(1){
			GS->ledRed.Toggle();
			FruityHal::DelayMs(50);
		}
	}
	else NVIC_SystemReset();
}



//################ Other
#define ______________OTHER______________

/**
 * The RamRetainStruct is saved in a special section in RAM that is persisted between system resets (most of the time)
 * We use this section to store information about the last error that occured before a reboot
 * After rebooting, we can read that struct to send the error information over the mesh
 * Because the ram might be corrupted upon reset, we also save a crc and clear the struct if it does not match
 */
void checkRamRetainStruct(){
	//Check if crc matches and reset reboot reason if not
	if(GS->ramRetainStructPtr->rebootReason != RebootReason::UNKNOWN){
		u32 crc = Utility::CalculateCrc32((u8*)GS->ramRetainStructPtr, sizeof(RamRetainStruct) - 4);
		if(crc != GS->ramRetainStructPtr->crc32){
			CheckedMemset(GS->ramRetainStructPtr, 0x00, sizeof(RamRetainStruct));
		}
	}
	//If we did not save the reboot reason before rebooting, check if the HAL knows something
	if(Utility::IsUnknownRebootReason(GS->ramRetainStructPtr->rebootReason)){
		RebootReason halReason = FruityHal::GetRebootReason();
		if (!Utility::IsUnknownRebootReason(halReason))
		{
			CheckedMemset(GS->ramRetainStructPtr, 0x00, sizeof(RamRetainStruct));
			GS->ramRetainStructPtr->rebootReason = halReason;
		}
	}
}
