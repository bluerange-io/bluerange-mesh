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

/*
 * This is the HAL for the NRF51 and NRF52 chipsets
 * It is also the HAL used by the CherrySim simulator
 */


#include "FruityHal.h"
#include <FruityHalNrf.h>
#include <types.h>
#include <GlobalState.h>
#include <Logger.h>
#include <ScanController.h>
#include <Node.h>
#include "Utility.h"
#ifdef SIM_ENABLED
#include <CherrySim.h>
#endif
#if IS_ACTIVE(CLC_MODULE)
#include <ClcComm.h>
#endif
#if IS_ACTIVE(VS_MODULE)
#include <VsComm.h>
#endif
#if IS_ACTIVE(WM_MODULE)
#include <WmComm.h>
#endif

extern "C" {
#ifdef NRF52
#include <nrf_power.h>
#endif
#ifndef SIM_ENABLED
#include <app_util_platform.h>
#include <nrf_uart.h>
#include <nrf_mbr.h>
#endif
}


#define APP_TIMER_PRESCALER     0 // Value of the RTC1 PRESCALER register
#define APP_TIMER_MAX_TIMERS    1 //Maximum number of simultaneously created timers (2 + BSP_APP_TIMERS_NUMBER)
#define APP_TIMER_OP_QUEUE_SIZE 1 //Size of timer operation queues


//This tag is used to set the SoftDevice configuration and must be used when advertising or creating connections
constexpr int BLE_CONN_CFG_TAG_FM = 1;

constexpr int BLE_CONN_CFG_GAP_PACKET_BUFFERS = 7;

#include <string.h>

#define __________________BLE_STACK_EVT_HANDLING____________________
// ############### BLE Stack and Event Handling ########################

static u32 getramend(void)
{
	u32 ram_total_size;

#if defined(NRF51) || defined(SIM_ENABLED)
	u32 block_size = NRF_FICR->SIZERAMBLOCKS;
	ram_total_size      = block_size * NRF_FICR->NUMRAMBLOCK;
#else
	ram_total_size      = NRF_FICR->INFO.RAM * 1024;
#endif

	return 0x20000000 + ram_total_size;
}

FruityHal::GeneralHardwareError FruityHal::BleStackInit()
{
	u32 err = 0;

	//Hotfix for NRF52 MeshGW v3 boards to disable NFC and use GPIO Pins 9 and 10
#ifdef CONFIG_NFCT_PINS_AS_GPIOS
	if (NRF_UICR->NFCPINS == 0xFFFFFFFF)
	{
		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}
		NRF_UICR->NFCPINS = 0xFFFFFFFEUL;
		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}

		NVIC_SystemReset();
	}
#endif

	logt("NODE", "Init Softdevice version 0x%x, Boardid %d", 3, Boardconfig->boardType);

//TODO: Would be better to get the NRF52 part working in the simulator, here is a reduced version for the simulator
#ifdef SIM_ENABLED
	ble_enable_params_t params;
	CheckedMemset(&params, 0x00, sizeof(params));

	//Configure the number of connections as peripheral and central
	params.gap_enable_params.periph_conn_count = Conf::totalInConnections; //Number of connections as Peripheral
	params.gap_enable_params.central_conn_count = Conf::totalOutConnections; //Number of connections as Central

	err = sd_ble_enable(&params, nullptr);
#endif

#if defined(NRF51)
	// Initialize the SoftDevice handler with the low frequency clock source
	//And a reference to the previously allocated buffer
	//No event handler is given because the event handling is done in the main loop
	nrf_clock_lf_cfg_t clock_lf_cfg;

	if(Boardconfig->lfClockSource == NRF_CLOCK_LF_SRC_XTAL){
		clock_lf_cfg.source = NRF_CLOCK_LF_SRC_XTAL;
		clock_lf_cfg.rc_ctiv = 0;
	} else {
		clock_lf_cfg.source = NRF_CLOCK_LF_SRC_RC;
		clock_lf_cfg.rc_ctiv = 1;
	}
	clock_lf_cfg.rc_temp_ctiv = 0;
	clock_lf_cfg.xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_100_PPM;

	err = softdevice_handler_init(&clock_lf_cfg, GS->currentEventBuffer, GlobalState::SIZE_OF_EVENT_BUFFER, nullptr);
	APP_ERROR_CHECK(err);

	logt("NODE", "Softdevice Init OK");

	//We now configure the parameters for enabling the softdevice, this will determine the needed RAM for the SD
	ble_enable_params_t params;
	CheckedMemset(&params, 0x00, sizeof(params));

	//Configre the number of Vendor Specific UUIDs
	params.common_enable_params.vs_uuid_count = 5;

	//Configure the number of connections as peripheral and central
	params.gap_enable_params.periph_conn_count = Conf::totalInConnections; //Number of connections as Peripheral
	params.gap_enable_params.central_conn_count = Conf::totalOutConnections; //Number of connections as Central
	params.gap_enable_params.central_sec_count = 1; //this application only needs to be able to pair in one central link at a time

	//Configure Bandwidth (We want all our connections to have a high throughput for RX and TX
	ble_conn_bw_counts_t bwCounts;
	CheckedMemset(&bwCounts, 0x00, sizeof(ble_conn_bw_counts_t));
	bwCounts.rx_counts.high_count = Conf::totalInConnections + Conf::totalOutConnections;
	bwCounts.tx_counts.high_count = Conf::totalInConnections + Conf::totalOutConnections;
	params.common_enable_params.p_conn_bw_counts = &bwCounts;

	//Configure the GATT Server Parameters
	params.gatts_enable_params.service_changed = 1; //we require the Service Changed characteristic
	params.gatts_enable_params.attr_tab_size = BLE_GATTS_ATTR_TAB_SIZE_DEFAULT; //the default Attribute Table size is appropriate for our application

	//The base ram address is gathered from the linker
	uint32_t app_ram_base = (u32)__application_ram_start_address;
	/* enable the BLE Stack */
	logt("NODE", "Ram base at 0x%x", (u32)app_ram_base);
	err = sd_ble_enable(&params, &app_ram_base);
	if(err == NRF_SUCCESS){
	/* Verify that __LINKER_APP_RAM_BASE matches the SD calculations */
		if(app_ram_base != (u32)__application_ram_start_address){
			logt("WARNING", "Warning: unused memory: 0x%x", ((u32)__application_ram_start_address) - (u32)app_ram_base);
		}
	} else if(err == NRF_ERROR_NO_MEM) {
		/* Not enough memory for the SoftDevice. Use output value in linker script */
		logt("ERROR", "Fatal: Not enough memory for the selected configuration. Required:0x%x", (u32)app_ram_base);
	} else {
		APP_ERROR_CHECK(err); //OK
	}

	//After the SoftDevice was enabled, we need to specifiy that we want a high bandwidth configuration
	// for both peripheral and central (6 packets for connInterval
	ble_opt_t bw_opt;
	bw_opt.common_opt.conn_bw.role               = BLE_GAP_ROLE_PERIPH;
	bw_opt.common_opt.conn_bw.conn_bw.conn_bw_rx = BLE_CONN_BW_HIGH;
	bw_opt.common_opt.conn_bw.conn_bw.conn_bw_tx = BLE_CONN_BW_HIGH;
	err = sd_ble_opt_set(BLE_COMMON_OPT_CONN_BW, &bw_opt);
	if(err != 0) logt("ERROR", "could not set bandwith %u", err);

	bw_opt.common_opt.conn_bw.role               = BLE_GAP_ROLE_CENTRAL;
	bw_opt.common_opt.conn_bw.conn_bw.conn_bw_rx = BLE_CONN_BW_HIGH;
	bw_opt.common_opt.conn_bw.conn_bw.conn_bw_tx = BLE_CONN_BW_HIGH;
	err = sd_ble_opt_set(BLE_COMMON_OPT_CONN_BW, &bw_opt);
	if(err != 0) logt("ERROR", "could not set bandwith %u", err);


#elif defined(NRF52)

	//######### Enables the Softdevice
	u32 finalErr = 0;

	// Configure the clock source
    nrf_clock_lf_cfg_t clock_lf_cfg;
    if(Boardconfig->lfClockSource == NRF_CLOCK_LF_SRC_XTAL){
        clock_lf_cfg.source = NRF_CLOCK_LF_SRC_XTAL;
        clock_lf_cfg.rc_ctiv = 0;
        clock_lf_cfg.rc_temp_ctiv = 0;
    } else {
        clock_lf_cfg.source = NRF_CLOCK_LF_SRC_RC;
        clock_lf_cfg.rc_ctiv = 16;
        clock_lf_cfg.rc_temp_ctiv = 2;
    }
#if defined(NRF52840)
    clock_lf_cfg.accuracy = NRF_CLOCK_LF_ACCURACY_100_PPM;
#else
    clock_lf_cfg.accuracy = NRF_CLOCK_LF_ACCURACY_100_PPM;
#endif

    //Enable SoftDevice using given clock source
    err = sd_softdevice_enable(&clock_lf_cfg, app_error_fault_handler);
	if(err){
		if(finalErr == 0) finalErr = err;
		logt("ERROR", "%u", err);
	}

	logt("ERROR", "SD Enable %u", err);

	uint32_t ram_start = (u32)__application_ram_start_address;

	//######### Sets our custom SoftDevice configuration

	//Create a SoftDevice configuration
	ble_cfg_t ble_cfg;

	// Configure the connection count.
	CheckedMemset(&ble_cfg, 0, sizeof(ble_cfg));
	ble_cfg.conn_cfg.conn_cfg_tag                     = BLE_CONN_CFG_TAG_FM;
	ble_cfg.conn_cfg.params.gap_conn_cfg.conn_count   = Conf::totalInConnections + Conf::totalOutConnections;
	ble_cfg.conn_cfg.params.gap_conn_cfg.event_length = NRF_SDH_BLE_GAP_EVENT_LENGTH; //TODO: do some tests and put in config

	err = sd_ble_cfg_set(BLE_CONN_CFG_GAP, &ble_cfg, ram_start);
	if(err){
		if(finalErr == 0) finalErr = err;
		logt("ERROR", "BLE_CONN_CFG_GAP %u", err);
	}

	// Configure the connection roles.
	CheckedMemset(&ble_cfg, 0, sizeof(ble_cfg));
	ble_cfg.gap_cfg.role_count_cfg.periph_role_count  = Conf::totalInConnections;
	ble_cfg.gap_cfg.role_count_cfg.central_role_count = Conf::totalOutConnections;
	ble_cfg.gap_cfg.role_count_cfg.central_sec_count  = BLE_GAP_ROLE_COUNT_CENTRAL_SEC_DEFAULT; //TODO: Could change this

	err = sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &ble_cfg, ram_start);
	if(err){
		if(finalErr == 0) finalErr = err;
		logt("ERROR", "BLE_GAP_CFG_ROLE_COUNT %u", err);
	}

	// set HVN queue size
	CheckedMemset(&ble_cfg, 0, sizeof(ble_cfg_t));
	ble_cfg.conn_cfg.conn_cfg_tag = BLE_CONN_CFG_TAG_FM;
	ble_cfg.conn_cfg.params.gatts_conn_cfg.hvn_tx_queue_size = BLE_CONN_CFG_GAP_PACKET_BUFFERS; /* application wants to queue 7 HVNs */
	sd_ble_cfg_set(BLE_CONN_CFG_GATTS, &ble_cfg, ram_start);

	// set WRITE_CMD queue size
	CheckedMemset(&ble_cfg, 0, sizeof(ble_cfg_t));
	ble_cfg.conn_cfg.conn_cfg_tag = BLE_CONN_CFG_TAG_FM;
	ble_cfg.conn_cfg.params.gattc_conn_cfg.write_cmd_tx_queue_size = BLE_CONN_CFG_GAP_PACKET_BUFFERS;
	sd_ble_cfg_set(BLE_CONN_CFG_GATTC, &ble_cfg, ram_start);

	// Configure the maximum ATT MTU.
#if false
	CheckedMemset(&ble_cfg, 0x00, sizeof(ble_cfg));
	ble_cfg.conn_cfg.conn_cfg_tag                 = BLE_CONN_CFG_TAG_FM;
	ble_cfg.conn_cfg.params.gatt_conn_cfg.att_mtu = BLE_GATT_ATT_MTU_DEFAULT;

	err = sd_ble_cfg_set(BLE_CONN_CFG_GATT, &ble_cfg, ram_start);
	APP_ERROR_CHECK(err);
#endif

	// Configure number of custom UUIDS.
	CheckedMemset(&ble_cfg, 0, sizeof(ble_cfg));
	ble_cfg.common_cfg.vs_uuid_cfg.vs_uuid_count = BLE_UUID_VS_COUNT_DEFAULT; //TODO: look in implementation

	err = sd_ble_cfg_set(BLE_COMMON_CFG_VS_UUID, &ble_cfg, ram_start);
	if(err){
		if(finalErr == 0) finalErr = err;
		logt("ERROR", "BLE_COMMON_CFG_VS_UUID %u", err);
	}

	// Configure the GATTS attribute table.
	CheckedMemset(&ble_cfg, 0x00, sizeof(ble_cfg));
	ble_cfg.gatts_cfg.attr_tab_size.attr_tab_size = NRF_SDH_BLE_GATTS_ATTR_TAB_SIZE;

	err = sd_ble_cfg_set(BLE_GATTS_CFG_ATTR_TAB_SIZE, &ble_cfg, ram_start);
	if(err){
		if(finalErr == 0) finalErr = err;
		logt("ERROR", "BLE_GATTS_CFG_ATTR_TAB_SIZE %u", err);
	}

	// Configure Service Changed characteristic.
	CheckedMemset(&ble_cfg, 0x00, sizeof(ble_cfg));
	ble_cfg.gatts_cfg.service_changed.service_changed = BLE_GATTS_SERVICE_CHANGED_DEFAULT;

	err = sd_ble_cfg_set(BLE_GATTS_CFG_SERVICE_CHANGED, &ble_cfg, ram_start);
	if(err){
		if(finalErr == 0) finalErr = err;
		logt("ERROR", "BLE_GATTS_CFG_SERVICE_CHANGED %u", err);
	}

	//######### Enables the BLE stack
	err = sd_ble_enable(&ram_start);
	logt("ERROR", "Err %u, Linker Ram section should be at %x, len %x", err, (u32)ram_start, (u32)(getramend() - ram_start));
	APP_ERROR_CHECK(finalErr);
	APP_ERROR_CHECK(err);
#endif //NRF52

	//Enable DC/DC (needs external LC filter, cmp. nrf51 reference manual page 43)
	err = sd_power_dcdc_mode_set(Boardconfig->dcDcEnabled ? NRF_POWER_DCDC_ENABLE : NRF_POWER_DCDC_DISABLE);
	logt("ERROR", "sd_power_dcdc_mode_set %u", err);
	APP_ERROR_CHECK(err); //OK

	// Set power mode
	err = sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
	logt("ERROR", "sd_power_mode_set %u", err);
	APP_ERROR_CHECK(err); //OK

#if SDK != 15
	//Set preferred TX power
	err = sd_ble_gap_tx_power_set(Conf::defaultDBmTX);
	APP_ERROR_CHECK(err); //OK
#else
	err = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_SCAN_INIT, 0, Conf::defaultDBmTX);
	logt("ERROR", "sd_ble_gap_tx_power_set %u", err);
	APP_ERROR_CHECK(err); //OK
#endif

	return (FruityHal::GeneralHardwareError)err;
}


/*
	########################
	###                  ###
	###      EVENTS      ###
	###                  ###
	########################
*/


void FruityHal::EventLooper()
{
	for (u32 i = 0; i < GS->amountOfEventLooperHandlers; i++)
	{
		GS->eventLooperHandlers[i]();
	}

	//When using the watchdog with a timeout smaller than 60 seconds, we feed it in our event loop
#if IS_ACTIVE(WATCHDOG)
	if(FM_WATCHDOG_TIMEOUT < 32768UL * 60){
		FruityHal::FeedWatchdog();
	}
#endif

	while (true)
	{
		//Check if there is input on uart
		GS->terminal.CheckAndProcessLine();

		//Fetch the event
		u16 eventSize = GlobalState::SIZE_OF_EVENT_BUFFER;
		u32 err = sd_ble_evt_get((u8*)GS->currentEventBuffer, &eventSize);

		//Handle ble event event
		if (err == NRF_SUCCESS)
		{
			FruityHal::DispatchBleEvents((void*)GS->currentEventBuffer);
		}
		//No more events available
		else if (err == NRF_ERROR_NOT_FOUND)
		{
			break;
		}
		else
		{
			APP_ERROR_CHECK(err); //FIXME: NRF_ERROR_DATA_SIZE not handeled //LCOV_EXCL_LINE assertion
			break;
		}
	}
#if IS_ACTIVE(BUTTONS)
	//Handle waiting button event
	if(GS->button1HoldTimeDs != 0){
		u32 holdTimeDs = GS->button1HoldTimeDs;
		GS->button1HoldTimeDs = 0;

		GS->buttonEventHandler(0, holdTimeDs);
	}
#endif


	//Handle Timer event that was waiting
	if (GS->passsedTimeSinceLastTimerHandlerDs > 0)
	{
		u16 timerDs = GS->passsedTimeSinceLastTimerHandlerDs;

		//Dispatch timer to all other modules
		GS->timerEventHandler(timerDs);

		//FIXME: Should protect this with a semaphore
		//because the timerInterrupt works asynchronously
		GS->passsedTimeSinceLastTimerHandlerDs -= timerDs;
	}

	// Pull event from soc
	while(true){
		uint32_t evt_id;
		u32 err = sd_evt_get(&evt_id);

		if (err == NRF_ERROR_NOT_FOUND){
			break;
		} else {
			GS->systemEventHandler((u32)evt_id); // Call handler
		}
	}

	u32 err = sd_app_evt_wait();
	APP_ERROR_CHECK(err); // OK
	err = sd_nvic_ClearPendingIRQ(SD_EVT_IRQn);
	APP_ERROR_CHECK(err);  // OK
}

void FruityHal::DispatchBleEvents(void* eventVirtualPointer)
{
	ble_evt_t& bleEvent = *((ble_evt_t*)eventVirtualPointer);
	u16 eventId = bleEvent.header.evt_id;

	if (eventId == BLE_GAP_EVT_ADV_REPORT || eventId == BLE_GAP_EVT_RSSI_CHANGED) {
		logt("EVENTS2", "BLE EVENT %s (%d)", FruityHal::getBleEventNameString(eventId), eventId);
	}
	else {
		logt("EVENTS", "BLE EVENT %s (%d)", FruityHal::getBleEventNameString(eventId), eventId);
	}
	
	//Calls the Db Discovery modules event handler
#ifdef NRF51
	ble_db_discovery_on_ble_evt(&GATTController::getInstance().discoveredServices, &bleEvent);
#elif defined(NRF52)
	ble_db_discovery_on_ble_evt(&bleEvent, &GATTController::getInstance().discoveredServices);
#endif

	switch (bleEvent.header.evt_id)
	{
	case BLE_GAP_EVT_RSSI_CHANGED:
		{
			GapRssiChangedEvent rce(&bleEvent);
			GS->cm.GapRssiChangedEventHandler(rce);
		}
		break;
	case BLE_GAP_EVT_ADV_REPORT:
		{
			GapAdvertisementReportEvent are(&bleEvent);
#if IS_ACTIVE(FAKE_NODE_POSITIONS)
			if (!GS->node.modifyEventForFakePositions(are))
			{
				//The position was faked to such a far place that we
				//should not do anything with the event.
				return;
			}
#endif
			ScanController::getInstance().ScanEventHandler(are);
			for (u32 i = 0; i < GS->amountOfModules; i++) {
				if (GS->activeModules[i]->configurationPointer->moduleActive) {
					GS->activeModules[i]->GapAdvertisementReportEventHandler(are);
				}
			}
		}
		break;
	case BLE_GAP_EVT_CONNECTED:
		{
			GapConnectedEvent ce(&bleEvent);
			GAPController::getInstance().GapConnectedEventHandler(ce);
			AdvertisingController::getInstance().GapConnectedEventHandler(ce);
			for (u32 i = 0; i < GS->amountOfModules; i++) {
				if (GS->activeModules[i]->configurationPointer->moduleActive) {
					GS->activeModules[i]->GapConnectedEventHandler(ce);
				}
			}
		}
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		{
			GapDisconnectedEvent de(&bleEvent);
			GAPController::getInstance().GapDisconnectedEventHandler(de);
			AdvertisingController::getInstance().GapDisconnectedEventHandler(de);
			for (u32 i = 0; i < GS->amountOfModules; i++) {
				if (GS->activeModules[i]->configurationPointer->moduleActive) {
					GS->activeModules[i]->GapDisconnectedEventHandler(de);
				}
			}
		}
		break;
	case BLE_GAP_EVT_TIMEOUT:
		{
			GapTimeoutEvent gte(&bleEvent);
			GAPController::getInstance().GapTimeoutEventHandler(gte);
		}
		break;
	case BLE_GAP_EVT_SEC_INFO_REQUEST:
		{
			GapSecurityInfoRequestEvent sire(&bleEvent);
			GAPController::getInstance().GapSecurityInfoRequestEvenetHandler(sire);
		}
		break;
	case BLE_GAP_EVT_CONN_SEC_UPDATE:
		{
			GapConnectionSecurityUpdateEvent csue(&bleEvent);
			GAPController::getInstance().GapConnectionSecurityUpdateEventHandler(csue);
		}
		break;
	case BLE_GATTC_EVT_WRITE_RSP:
		{
#ifdef SIM_ENABLED
			//if(cherrySimInstance->currentNode->id == 37 && bleEvent.evt.gattc_evt.conn_handle == 680) printf("%04u Q@NODE %u EVT_WRITE_RSP received" EOL, cherrySimInstance->globalBreakCounter++, cherrySimInstance->currentNode->id);
#endif
			GattcWriteResponseEvent wre(&bleEvent);
			ConnectionManager::getInstance().GattcWriteResponseEventHandler(wre);
		}
		break;
	case BLE_GATTC_EVT_TIMEOUT: //jstodo untested event
		{
			GattcTimeoutEvent gte(&bleEvent);
			ConnectionManager::getInstance().GattcTimeoutEventHandler(gte);
		}
		break;
	case BLE_GATTS_EVT_WRITE:
		{
			GattsWriteEvent gwe(&bleEvent);
			ConnectionManager::getInstance().GattsWriteEventHandler(gwe);
		}
		break;
	case BLE_GATTC_EVT_HVX:
		{
			GattcHandleValueEvent hve(&bleEvent);	
			ConnectionManager::getInstance().GattcHandleValueEventHandler(hve);
		}
		break;
#if defined(NRF51) || defined(SIM_ENABLED)
	case BLE_EVT_TX_COMPLETE:
#elif defined(NRF52)
	case BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE:
	case BLE_GATTS_EVT_HVN_TX_COMPLETE:
#endif
		{
#ifdef SIM_ENABLED
			//if (cherrySimInstance->currentNode->id == 37 && bleEvent.evt.common_evt.conn_handle == 680) printf("%04u Q@NODE %u WRITE_CMD_TX_COMPLETE %u received" EOL, cherrySimInstance->globalBreakCounter++, cherrySimInstance->currentNode->id, bleEvent.evt.common_evt.params.tx_complete.count);
#endif
			GattDataTransmittedEvent gdte(&bleEvent);
			ConnectionManager::getInstance().GattDataTransmittedEventHandler(gdte);
			for (int i = 0; i < MAX_MODULE_COUNT; i++) {
				if (GS->activeModules[i] != nullptr
					&& GS->activeModules[i]->configurationPointer->moduleActive) {
					GS->activeModules[i]->GattDataTransmittedEventHandler(gdte);
				}
			}
		}
		break;




		/* Extremly platform dependent events below! 
		   Because they are so platform dependent, 
		   there is no handler for them and we have
		   to deal with them here. */

#if defined(NRF52)
#ifndef NRF52840
	case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
		{
			ble_gap_phys_t phy;
			phy.rx_phys = BLE_GAP_PHY_1MBPS;
			phy.tx_phys = BLE_GAP_PHY_1MBPS;

			sd_ble_gap_phy_update(bleEvent.evt.gap_evt.conn_handle, &phy);
		}
		break;
#endif
	case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
		{
			sd_ble_gap_data_length_update(bleEvent.evt.gap_evt.conn_handle, nullptr, nullptr);
		}
		break;
#endif
#ifdef NRF52
	case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
	{
		//Use default MTU if requested
		sd_ble_gatts_exchange_mtu_reply(bleEvent.evt.gatts_evt.conn_handle, BLE_GATT_ATT_MTU_DEFAULT);

		break;
	}
#endif
	case BLE_GATTS_EVT_SYS_ATTR_MISSING:	//jstodo untested event
		{
			u32 err = 0;
			//Handles missing Attributes, don't know why it is needed
			err = sd_ble_gatts_sys_attr_set(bleEvent.evt.gatts_evt.conn_handle, nullptr, 0, 0);
			logt("ERROR", "SysAttr %u", err);
		}
		break;
	}

	if (eventId == BLE_GAP_EVT_ADV_REPORT || eventId == BLE_GAP_EVT_RSSI_CHANGED) {
		logt("EVENTS2", "End of event");
	}
	else {
		logt("EVENTS", "End of event");
	}
}

static ble_evt_t* currentEvent = nullptr;

GapConnParamUpdateEvent::GapConnParamUpdateEvent(void* _evt)
	:GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_CONN_PARAM_UPDATE)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

GapEvent::GapEvent(void* _evt)
	: BleEvent(_evt)
{
}

u16 GapEvent::getConnectionHandle() const
{
	return currentEvent->evt.gap_evt.conn_handle;
}

u16 GapConnParamUpdateEvent::getMaxConnectionInterval() const
{
	return currentEvent->evt.gap_evt.params.conn_param_update.conn_params.max_conn_interval;
}

GapRssiChangedEvent::GapRssiChangedEvent(void* _evt)
	:GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_RSSI_CHANGED)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

i8 GapRssiChangedEvent::getRssi() const
{
	return currentEvent->evt.gap_evt.params.rssi_changed.rssi;
}

GapAdvertisementReportEvent::GapAdvertisementReportEvent(void* _evt)
	:GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_ADV_REPORT)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

i8 GapAdvertisementReportEvent::getRssi() const
{
#if IS_ACTIVE(FAKE_NODE_POSITIONS)
	if (fakeRssiSet) {
		return fakeRssi;
	}
#endif
	return currentEvent->evt.gap_evt.params.adv_report.rssi;
}

const u8 * GapAdvertisementReportEvent::getData() const
{
#if (SDK == 15)
	return currentEvent->evt.gap_evt.params.adv_report.data.p_data;
#else
	return currentEvent->evt.gap_evt.params.adv_report.data;
#endif
}

u32 GapAdvertisementReportEvent::getDataLength() const
{
#if (SDK == 15)
	return currentEvent->evt.gap_evt.params.adv_report.data.len;
#else
	return currentEvent->evt.gap_evt.params.adv_report.dlen;
#endif
}

const u8 * GapAdvertisementReportEvent::getPeerAddr() const
{
	return currentEvent->evt.gap_evt.params.adv_report.peer_addr.addr;
}

u8 GapAdvertisementReportEvent::getPeerAddrType() const
{
	return currentEvent->evt.gap_evt.params.adv_report.peer_addr.addr_type;
}

bool GapAdvertisementReportEvent::isConnectable() const
{
#if (SDK == 15)
	return currentEvent->evt.gap_evt.params.adv_report.type.connectable == 0x01;
#else
	return currentEvent->evt.gap_evt.params.adv_report.type == BLE_GAP_ADV_TYPE_ADV_IND;
#endif
}

#if IS_ACTIVE(FAKE_NODE_POSITIONS)
void AdvertisementReportEvent::setFakeRssi(i8 rssi)
{
	this->fakeRssi = rssi;
	fakeRssiSet = true;
}
#endif

BleEvent::BleEvent(void *_evt)
{
	if (currentEvent != nullptr) {
		//This is thrown if two events are processed at the same time, which is illegal.
		SIMEXCEPTION(IllegalStateException); //LCOV_EXCL_LINE assertion
	}
	currentEvent = (ble_evt_t*)_evt;
}

#ifdef SIM_ENABLED
BleEvent::~BleEvent()
{
	currentEvent = nullptr;
}
#endif

GapConnectedEvent::GapConnectedEvent(void* _evt)
	:GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_CONNECTED)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

GapRole GapConnectedEvent::getRole() const
{
	return (GapRole)(currentEvent->evt.gap_evt.params.connected.role);
}

u8 GapConnectedEvent::getPeerAddrType() const
{
	return (currentEvent->evt.gap_evt.params.connected.peer_addr.addr_type);
}

u16 GapConnectedEvent::getMinConnectionInterval() const
{
	return currentEvent->evt.gap_evt.params.connected.conn_params.min_conn_interval;
}

const u8 * GapConnectedEvent::getPeerAddr() const
{
	return (currentEvent->evt.gap_evt.params.connected.peer_addr.addr);
}

GapDisconnectedEvent::GapDisconnectedEvent(void* _evt)
	: GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_DISCONNECTED)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

u8 GapDisconnectedEvent::getReason() const
{
	return currentEvent->evt.gap_evt.params.disconnected.reason;
}

GapTimeoutEvent::GapTimeoutEvent(void* _evt)
	: GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_TIMEOUT)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

GapTimeoutSource GapTimeoutEvent::getSource() const
{
	return (GapTimeoutSource)(currentEvent->evt.gap_evt.params.timeout.src);
}

GapSecurityInfoRequestEvent::GapSecurityInfoRequestEvent(void* _evt)
	: GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_SEC_INFO_REQUEST)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

GapConnectionSecurityUpdateEvent::GapConnectionSecurityUpdateEvent(void* _evt)
	: GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_CONN_SEC_UPDATE)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

u8 GapConnectionSecurityUpdateEvent::getKeySize() const
{
	return currentEvent->evt.gap_evt.params.conn_sec_update.conn_sec.encr_key_size;
}

SecurityLevel GapConnectionSecurityUpdateEvent::getSecurityLevel() const
{
	return (SecurityLevel)(currentEvent->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.lv);
}

SecurityMode GapConnectionSecurityUpdateEvent::getSecurityMode() const
{
	return (SecurityMode)(currentEvent->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.sm);
}

GattcEvent::GattcEvent(void* _evt)
	: BleEvent(_evt)
{
}

u16 GattcEvent::getConnectionHandle() const
{
	return currentEvent->evt.gattc_evt.conn_handle;
}

u16 GattcEvent::getGattStatus() const
{
	return currentEvent->evt.gattc_evt.gatt_status;
}

GattcWriteResponseEvent::GattcWriteResponseEvent(void* _evt)
	: GattcEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GATTC_EVT_WRITE_RSP)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

GattcTimeoutEvent::GattcTimeoutEvent(void* _evt)
	: GattcEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GATTC_EVT_TIMEOUT)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

GattDataTransmittedEvent::GattDataTransmittedEvent(void* _evt)
	:BleEvent(_evt)
{
#if defined(NRF51) || defined(SIM_ENABLED)
	if (currentEvent->header.evt_id != BLE_EVT_TX_COMPLETE)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
#endif
}

u16 GattDataTransmittedEvent::getConnectionHandle() const
{
#if defined(NRF51) || defined(SIM_ENABLED)
	return currentEvent->evt.common_evt.conn_handle;
#elif defined(NRF52)
	if (currentEvent->header.evt_id == BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE) {
		return currentEvent->evt.gattc_evt.conn_handle;
	}
	else if (currentEvent->header.evt_id == BLE_GATTS_EVT_HVN_TX_COMPLETE) {
		return currentEvent->evt.gatts_evt.conn_handle;
	}
	SIMEXCEPTION(InvalidStateException);
	return -1; //This must never be executed!
#endif
}

bool GattDataTransmittedEvent::isConnectionHandleValid() const
{
	return getConnectionHandle() != BLE_CONN_HANDLE_INVALID;
}

u32 GattDataTransmittedEvent::getCompleteCount() const
{
#if defined(NRF51) || defined(SIM_ENABLED)
	return currentEvent->evt.common_evt.params.tx_complete.count;
#elif defined(NRF52)
	if (currentEvent->header.evt_id == BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE) {
		return currentEvent->evt.gattc_evt.params.write_cmd_tx_complete.count;
	}
	else if (currentEvent->header.evt_id == BLE_GATTS_EVT_HVN_TX_COMPLETE) {
		return currentEvent->evt.gatts_evt.params.hvn_tx_complete.count;
	}
	SIMEXCEPTION(InvalidStateException);
	return -1; //This must never be executed!
#endif
}

GattsWriteEvent::GattsWriteEvent(void* _evt)
	: BleEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GATTS_EVT_WRITE)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

u16 GattsWriteEvent::getAttributeHandle() const
{
	return currentEvent->evt.gatts_evt.params.write.handle;
}

bool GattsWriteEvent::isWriteRequest() const
{
	return currentEvent->evt.gatts_evt.params.write.op == BLE_GATTS_OP_WRITE_REQ;
}

u16 GattsWriteEvent::getLength() const
{
	return currentEvent->evt.gatts_evt.params.write.len;
}

u16 GattsWriteEvent::getConnectionHandle() const
{
	return currentEvent->evt.gatts_evt.conn_handle;
}

u8 * GattsWriteEvent::getData() const
{
	return (u8*)currentEvent->evt.gatts_evt.params.write.data;
}

GattcHandleValueEvent::GattcHandleValueEvent(void* _evt)
	:GattcEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GATTC_EVT_HVX)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

u16 GattcHandleValueEvent::getHandle() const
{
	return currentEvent->evt.gattc_evt.params.hvx.handle;
}

u16 GattcHandleValueEvent::getLength() const
{
	return currentEvent->evt.gattc_evt.params.hvx.len;
}

u8 * GattcHandleValueEvent::getData() const
{
	return currentEvent->evt.gattc_evt.params.hvx.data;
}

/*
	#####################
	###               ###
	###      GAP      ###
	###               ###
	#####################
*/


#define __________________GAP____________________

u32 FruityHal::BleGapAddressSet(fh_ble_gap_addr_t const *address)
{
	ble_gap_addr_t addr;
	memcpy(addr.addr, address->addr, FH_BLE_GAP_ADDR_LEN);
	addr.addr_type = address->addr_type;

#if defined(NRF51) || defined(SIM_ENABLED)
	u32 err = sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &addr);
#elif defined(NRF52)
	u32 err = sd_ble_gap_addr_set(&addr);
#endif
	logt("FH", "Gap Addr Set (%u)", err);

	return err;
}

u32 FruityHal::BleGapAddressGet(fh_ble_gap_addr_t *address)
{
	u32 err;
	ble_gap_addr_t p_addr;

#if defined(NRF51) || defined(SIM_ENABLED)
	err = sd_ble_gap_address_get(&p_addr);
#elif defined(NRF52)
	err = sd_ble_gap_addr_get(&p_addr);
#endif

	if(err == NRF_SUCCESS){
		memcpy(address->addr, p_addr.addr, FH_BLE_GAP_ADDR_LEN);
		address->addr_type = p_addr.addr_type;
	}

	logt("FH", "Gap Addr Get (%u)", err);

	return err;
}

#if (SDK == 15)
u32 FruityHal::BleGapScanStart(fh_ble_gap_scan_params_t const *scanParams, u8 * p_scanBuffer)
{
	u32 err;
	ble_data_t scan_data;
	scan_data.len = BLE_GAP_SCAN_BUFFER_MAX;
	scan_data.p_data = p_scanBuffer;
	ble_gap_scan_params_t scan_params;
	scan_params.active = 0;
	scan_params.interval = scanParams->interval;
	scan_params.timeout = scanParams->timeout;
	scan_params.window = scanParams->window;
	scan_params.report_incomplete_evts = 0;
	scan_params.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL;
	scan_params.extended = 0;
	CheckedMemset(scan_params.channel_mask, 0, sizeof(u8) * 5);

	err = sd_ble_gap_scan_start(&scan_params, &scan_data);
	logt("FH", "Scan start(%u) iv %u, w %u", err, scan_params.interval, scan_params.window);
	return err;
}
#else
u32 FruityHal::BleGapScanStart(fh_ble_gap_scan_params_t const *scanParams)
{
	u32 err;
	ble_gap_scan_params_t scan_params;
	scan_params.active = 0;
	scan_params.interval = scanParams->interval;
	scan_params.timeout = scanParams->timeout;
	scan_params.window = scanParams->window;

#if defined(NRF51) || defined(SIM_ENABLED)
	scan_params.p_whitelist = nullptr;
	scan_params.selective = 0;
#elif defined(NRF52)
	scan_params.adv_dir_report = 0;
	scan_params.use_whitelist = 0;
#endif


	err = sd_ble_gap_scan_start(&scan_params);
	logt("FH", "Scan start(%u) iv %u, w %u", err, scan_params.interval, scan_params.window);
	return err;
}
#endif


u32 FruityHal::BleGapScanStop()
{
	u32 err = sd_ble_gap_scan_stop();
	logt("FH", "Scan stop(%u)", err);
	return err;
}

#if (SDK == 15)
u32 FruityHal::BleGapAdvDataSet(u8 * p_advHandle, fh_ble_gap_adv_params_t const * p_advParams, const u8 *advData, u8 advDataLength, const u8 *scanData, u8 scanDataLength)
{
	u32 err = 0;
	ble_gap_adv_params_t adv_params;
	ble_gap_adv_data_t adv_data;
	CheckedMemset(&adv_params, 0, sizeof(adv_params));
	CheckedMemset(&adv_data, 0, sizeof(adv_data));
	if (p_advParams != nullptr)	{
		adv_params.channel_mask[4] |= (p_advParams->channel_mask.ch_37_off << 5);
		adv_params.channel_mask[4] |= (p_advParams->channel_mask.ch_38_off << 6);
		adv_params.channel_mask[4] |= (p_advParams->channel_mask.ch_39_off << 7);
		adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
		adv_params.interval = p_advParams->interval;
		adv_params.p_peer_addr = nullptr;
		adv_params.duration = p_advParams->timeout;
		adv_params.properties.type = (uint8_t)p_advParams->type;
	}
	adv_data.adv_data.p_data = (u8 *)advData;
	adv_data.adv_data.len = advDataLength;
	adv_data.scan_rsp_data.p_data = (u8 *)scanData;
	adv_data.scan_rsp_data.len = scanDataLength;

	if (p_advParams != nullptr){
		err = sd_ble_gap_adv_set_configure(
					p_advHandle,
					&adv_data,
				  &adv_params
				);
	}
	else {
		err = sd_ble_gap_adv_set_configure(
					p_advHandle,
					&adv_data,
				  nullptr
				);
	}

	logt("FH", "Adv data set (%u) typ %u, iv %lu, mask %u, handle %u", err, adv_params.properties.type, adv_params.interval, adv_params.channel_mask[4], *p_advHandle);
	return err;

}

u32 FruityHal::BleGapAdvStart(u8 advHandle)
{
	u32 err;
	err = sd_ble_gap_adv_start(advHandle, BLE_CONN_CFG_TAG_FM);
	logt("FH", "Adv start (%u)", err);
	return err;
}

u32 FruityHal::BleGapAdvStop(u8 advHandle)
{
	u32 err = sd_ble_gap_adv_stop(advHandle);
	logt("FH", "Adv stop (%u)", err);
	return err;
}
#else
u32 FruityHal::BleGapAdvDataSet(const u8 *advData, u8 advDataLength, const u8 *scanData, u8 scanDataLength)
{
	u32 err;
	err = sd_ble_gap_adv_data_set(
				advData,
				advDataLength,
				scanData,
				scanDataLength
			);

	logt("FH", "Adv data set (%u)", err);
	return err;

}

u32 FruityHal::BleGapAdvStart(fh_ble_gap_adv_params_t const *advParams)
{
	u32 err;
	ble_gap_adv_params_t adv_params;
	adv_params.channel_mask.ch_37_off = advParams->channel_mask.ch_37_off;
	adv_params.channel_mask.ch_38_off = advParams->channel_mask.ch_38_off;
	adv_params.channel_mask.ch_39_off= advParams->channel_mask.ch_39_off;
	adv_params.fp = BLE_GAP_ADV_FP_ANY;
	adv_params.interval = advParams->interval;
	adv_params.p_peer_addr = nullptr;
	adv_params.timeout = advParams->timeout;
	adv_params.type = (u8)advParams->type;

#if defined(NRF51) || defined(SIM_ENABLED)
	err = sd_ble_gap_adv_start(&adv_params);
#elif defined(NRF52)
	err = sd_ble_gap_adv_start(&adv_params, BLE_CONN_CFG_TAG_FM);
#endif

	logt("FH", "Adv start (%u) typ %u, iv %u, mask %u", err, adv_params.type, adv_params.interval, *((u8*)&adv_params.channel_mask));
	return err;
}

u32 FruityHal::BleGapAdvStop()
{
	u32 err = sd_ble_gap_adv_stop();
	logt("FH", "Adv stop (%u)", err);
	return err;
}
#endif

u32 FruityHal::BleGapConnect(fh_ble_gap_addr_t const *peerAddress, fh_ble_gap_scan_params_t const *scanParams, fh_ble_gap_conn_params_t const *connectionParams)
{
	u32 err;
	ble_gap_addr_t p_peer_addr;
	CheckedMemset(&p_peer_addr, 0x00, sizeof(p_peer_addr));
	p_peer_addr.addr_type = peerAddress->addr_type;
	memcpy(p_peer_addr.addr, peerAddress->addr, sizeof(peerAddress->addr));

	ble_gap_scan_params_t p_scan_params;
	CheckedMemset(&p_scan_params, 0x00, sizeof(p_scan_params));
	p_scan_params.active = 0;
	p_scan_params.interval = scanParams->interval;
	p_scan_params.timeout = scanParams->timeout;
	p_scan_params.window = scanParams->window;
#if defined(NRF51) || defined(SIM_ENABLED)
	p_scan_params.p_whitelist = nullptr;
	p_scan_params.selective = 0;
#elif defined(NRF52) && (SDK == 14)
	p_scan_params.adv_dir_report = 0;
	p_scan_params.use_whitelist = 0;
#else
	p_scan_params.report_incomplete_evts = 0;
	p_scan_params.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL;
	p_scan_params.extended = 0;
	CheckedMemset(p_scan_params.channel_mask, 0, sizeof(p_scan_params.channel_mask));
#endif

	ble_gap_conn_params_t p_conn_params;
	CheckedMemset(&p_conn_params, 0x00, sizeof(p_conn_params));
	p_conn_params.conn_sup_timeout = connectionParams->conn_sup_timeout;
	p_conn_params.max_conn_interval = connectionParams->max_conn_interval;
	p_conn_params.min_conn_interval = connectionParams->min_conn_interval;
	p_conn_params.slave_latency = connectionParams->slave_latency;


#if defined(NRF51) || defined(SIM_ENABLED)
	err = sd_ble_gap_connect(&p_peer_addr, &p_scan_params, &p_conn_params);
#elif defined(NRF52)
	err = sd_ble_gap_connect(&p_peer_addr, &p_scan_params, &p_conn_params, BLE_CONN_CFG_TAG_FM);
#endif

	logt("FH", "Connect (%u) iv:%u, tmt:%u", err, p_conn_params.min_conn_interval, p_scan_params.timeout);

	//Tell our ScanController, that scanning has stopped
	GS->scanController.ScanningHasStopped();

	return err;
}


u32 FruityHal::ConnectCancel()
{
	u32 err = sd_ble_gap_connect_cancel();

	logt("FH", "Connect Cancel (%u)", err);

	return err;
}

u32 FruityHal::Disconnect(uint16_t conn_handle, FruityHal::HciErrorCode hci_status_code)
{
	u32 err = sd_ble_gap_disconnect(conn_handle, (u8)hci_status_code);

	logt("FH", "Disconnect (%u)", err);

	return err;
}

u32 FruityHal::BleTxPacketCountGet(u16 connectionHandle, u8* count)
{
#if defined(NRF51) || defined(SIM_ENABLED)
	return sd_ble_tx_packet_count_get(connectionHandle, count);
#elif defined(NRF52)
//TODO: must be read from somewhere else
	*count = BLE_CONN_CFG_GAP_PACKET_BUFFERS;
	return 0;
#endif
}

u32 FruityHal::DiscovereServiceInit(DBDiscoveryHandler dbEventHandler)
{
#ifndef  SIM_ENABLED
	return ble_db_discovery_init(dbEventHandler);
#else // ! SIM_ENABLED
	GS->dbDiscoveryHandler = dbEventHandler;
#endif
	return 0;
}

u32 FruityHal::DiscoverService(u16 connHandle, const ble_uuid_t &p_uuid, ble_db_discovery_t * p_discoveredServices)
{
	uint32_t err = 0;
#ifndef SIM_ENABLED
	CheckedMemset(p_discoveredServices, 0x00, sizeof(*p_discoveredServices));
	err = ble_db_discovery_evt_register(&p_uuid);
	if (err) {
		logt("ERROR", "err %u", (u32)err);
		return err;
	}

	err = ble_db_discovery_start(p_discoveredServices, connHandle);
	if (err) {
		logt("ERROR", "err %u", (u32)err);
		return err;
	}
#else
	cherrySimInstance->StartServiceDiscovery(connHandle, p_uuid, 1000);
#endif
	return err;
}



//################################################
#define _________________BUTTONS__________________

#ifndef SIM_ENABLED
void button_interrupt_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action){
	//GS->ledGreen->Toggle();

	//Because we don't know which state the button is in, we have to read it
	u32 state = nrf_gpio_pin_read(pin);

	//An interrupt generated by our button
	if(pin == (u8)Boardconfig->button1Pin){
		if(state == Boardconfig->buttonsActiveHigh){
			GS->button1PressTimeDs = GS->appTimerDs;
		} else if(state == !Boardconfig->buttonsActiveHigh && GS->button1PressTimeDs != 0){
			GS->button1HoldTimeDs = GS->appTimerDs - GS->button1PressTimeDs;
			GS->button1PressTimeDs = 0;
		}
	}
}
#endif

u32 FruityHal::InitializeButtons()
{
	u32 err = 0;
#if IS_ACTIVE(BUTTONS) && !defined(SIM_ENABLED)
	//Activate GPIOTE if not already active
	nrf_drv_gpiote_init();

	//Register for both HighLow and LowHigh events
	//IF this returns NO_MEM, increase GPIOTE_CONFIG_NUM_OF_LOW_POWER_EVENTS
	nrf_drv_gpiote_in_config_t buttonConfig;
	buttonConfig.sense = NRF_GPIOTE_POLARITY_TOGGLE;
	buttonConfig.pull = NRF_GPIO_PIN_PULLUP;
	buttonConfig.is_watcher = 0;
	buttonConfig.hi_accuracy = 0;
#if SDK == 15
	buttonConfig.skip_gpio_setup = 0;
#endif

	//This uses the SENSE low power feature, all pin events are reported
	//at the same GPIOTE channel
	err =  nrf_drv_gpiote_in_init(Boardconfig->button1Pin, &buttonConfig, button_interrupt_handler);

	//Enable the events
	nrf_drv_gpiote_in_event_enable(Boardconfig->button1Pin, true);
#endif

	return err;
}

//################################################
#define _________________UART_____________________

//This handler receives UART interrupts (terminal json mode)
#if !defined(UART_ENABLED) || UART_ENABLED == 0 //Only enable if nordic library for UART is not used
extern "C"{
	void UART0_IRQHandler(void)
	{
		if (GS->uartEventHandler == nullptr) {
			SIMEXCEPTION(UartNotSetException);
		} else {
		    GS->uartEventHandler();
		}
	}
}
#endif

//################################################
#define _________________TIMERS___________________

extern "C"{
	static const u32 TICKS_PER_DS_TIMES_TEN = 32768;

	void app_timer_handler(void * p_context){
		UNUSED_PARAMETER(p_context);

		//We just increase the time that has passed since the last handler
		//And call the timer from our main event handling queue
		GS->tickRemainderTimesTen += ((u32)MAIN_TIMER_TICK) * 10;
		u32 passedDs = GS->tickRemainderTimesTen / TICKS_PER_DS_TIMES_TEN;
		GS->tickRemainderTimesTen -= passedDs * TICKS_PER_DS_TIMES_TEN;
		GS->passsedTimeSinceLastTimerHandlerDs += passedDs;

		GS->timeManager.AddTicks(MAIN_TIMER_TICK);
	}
}

u32 FruityHal::InitTimers()
{
#if defined(NRF51)
	APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, nullptr);
#elif defined(NRF52)
	uint32_t ret = app_timer_init();
	return ret;
#endif
	return 0;
}

u32 FruityHal::StartTimers()
{
	u32 err = 0;

#if defined(NRF51) || defined (NRF52)
	APP_TIMER_DEF(mainTimerMsId);

	err = app_timer_create(&mainTimerMsId, APP_TIMER_MODE_REPEATED, app_timer_handler);
	if (err != NRF_SUCCESS) return err;

	err = app_timer_start(mainTimerMsId, MAIN_TIMER_TICK, nullptr);
#endif
	return err;
}

u32 FruityHal::GetRtc()
{
#if defined(NRF51) || defined(SIM_ENABLED)
	uint32_t count;
	app_timer_cnt_get(&count);
	return count;
#elif defined(NRF52)
	return app_timer_cnt_get();
#endif
}

u32 FruityHal::GetRtcDifference(u32 ticksTo, u32 ticksFrom)
{
#if defined(NRF51) || defined(SIM_ENABLED)
	uint32_t diff;
	app_timer_cnt_diff_compute(ticksTo, ticksFrom, &diff);
	return diff;
#elif defined(NRF52)
	return app_timer_cnt_diff_compute(ticksTo, ticksFrom);
#endif
}

//################################################
#define _____________FAULT_HANDLERS_______________

//These error handlers are declared WEAK in the nRF SDK and can be implemented here
//Will be called if an error occurs somewhere in the code, but not if it's a hardfault
extern "C"
{
	//The app_error handler_bare is called by all APP_ERROR_CHECK functions when DEBUG is undefined
	void app_error_handler_bare(uint32_t error_code)
	{
		GS->appErrorHandler((u32)error_code);
	}

	//The app_error handler is called by all APP_ERROR_CHECK functions when DEBUG is defined
	void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
	{
		app_error_handler_bare(error_code);
		logt("ERROR", "App error code:%s(%u), file:%s, line:%u", FruityHal::getGeneralErrorString((FruityHal::GeneralHardwareError)error_code), (u32)error_code, p_file_name, (u32)line_num);
	}

	//Called when the softdevice crashes
	void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
	{
		GS->stackErrorHandler(id, pc, info);
	}

#ifndef SIM_ENABLED
	//We use the nordic hardfault handler that stacks all fault variables for us before calling this function
	__attribute__((used)) void HardFault_c_handler(stacked_regs_t* stack)
	{
		GS->hardfaultHandler(stack);
	}
#endif

	//NRF52 uses more handlers, we currently just reboot if they are called
	//TODO: Redirect to hardfault handler, but be aware that the stack will shift by calling the function
#ifdef NRF52
	void MemoryManagement_Handler(){
		NVIC_SystemReset();
	}
	void BusFault_Handler(){
		NVIC_SystemReset();
	}
	void UsageFault_Handler(){
		NVIC_SystemReset();
	}
#endif



}

//################################################
#define __________________MISC____________________


void FruityHal::SystemReset()
{
	sd_nvic_SystemReset();
}

// Retrieves the reboot reason from the RESETREAS register
RebootReason FruityHal::GetRebootReason()
{
#ifndef SIM_ENABLED
	u32 reason = NRF_POWER->RESETREAS;

	if(reason & POWER_RESETREAS_DOG_Msk){
		return RebootReason::WATCHDOG;
	} else if (reason & POWER_RESETREAS_RESETPIN_Msk){
		return RebootReason::PIN_RESET;
	} else if (reason & POWER_RESETREAS_OFF_Msk){
		return RebootReason::FROM_OFF_STATE;
	} else {
		return RebootReason::UNKNOWN;
	}
#else
	return (RebootReason)ST_getRebootReason();
#endif
}



//Clears the Reboot reason because the RESETREAS register is cumulative
u32 FruityHal::ClearRebootReason()
{
	sd_power_reset_reason_clr(0xFFFFFFFFUL);
	return 0;
}

//Starts the Watchdog with a static interval so that changing a config can do no harm
void FruityHal::StartWatchdog(bool safeBoot)
{
#if IS_ACTIVE(WATCHDOG)
	u32 err = 0;

	//Configure Watchdog to default: Run while CPU sleeps
	nrf_wdt_behaviour_set(NRF_WDT_BEHAVIOUR_RUN_SLEEP);
	//Configure Watchdog timeout
	if (!safeBoot) {
		nrf_wdt_reload_value_set(FM_WATCHDOG_TIMEOUT);
	}
	else {
		nrf_wdt_reload_value_set(FM_WATCHDOG_TIMEOUT_SAFE_BOOT);
	}
	// Configure Reload Channels
	nrf_wdt_reload_request_enable(NRF_WDT_RR0);

	//Enable
	nrf_wdt_task_trigger(NRF_WDT_TASK_START);

	logt("ERROR", "Watchdog started");
#endif
}

//Feeds the Watchdog to keep it quiet
void FruityHal::FeedWatchdog()
{
#if IS_ACTIVE(WATCHDOG)
	nrf_wdt_reload_request_set(NRF_WDT_RR0);
#endif
}

u32 FruityHal::GetBootloaderVersion()
{
	if(BOOTLOADER_UICR_ADDRESS != 0xFFFFFFFF){
		return *(u32*)(BOOTLOADER_UICR_ADDRESS + 1024);
	} else {
		return 0;
	}
}

void FruityHal::DelayUs(u32 delayMicroSeconds)
{
#ifndef SIM_ENABLED
	nrf_delay_us(delayMicroSeconds);
#endif
}

void FruityHal::DelayMs(u32 delayMs)
{
#ifndef SIM_ENABLED
    while(delayMs != 0)
    {
    	delayMs--;
        nrf_delay_us(999);
    }
#endif
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

#define __________________CONVERT____________________

ble_gap_addr_t FruityHal::Convert(const fh_ble_gap_addr_t* address)
{
	ble_gap_addr_t addr;
	memcpy(addr.addr, address->addr, FH_BLE_GAP_ADDR_LEN);
	addr.addr_type = address->addr_type;
#ifdef NRF52
	addr.addr_id_peer = 0;
#endif
	return addr;
}
fh_ble_gap_addr_t FruityHal::Convert(const ble_gap_addr_t* p_addr)
{
	fh_ble_gap_addr_t address;
	memcpy(address.addr, p_addr->addr, FH_BLE_GAP_ADDR_LEN);
	address.addr_type = p_addr->addr_type;

	return address;
}

bool FruityHal::setRetentionRegisterTwo(u8 val)
{
#ifdef NRF52
	nrf_power_gpregret2_set(val);
	return true;
#else
	return false;
#endif
}

void FruityHal::disableHardwareDfuBootloader()
{
#ifndef SIM_ENABLED
	bool bootloaderAvailable = (BOOTLOADER_UICR_ADDRESS != 0xFFFFFFFF);
	u32 bootloaderAddress = BOOTLOADER_UICR_ADDRESS;

	//Check if a bootloader exists
	if (bootloaderAddress != 0xFFFFFFFFUL) {
		u32* magicNumberAddress = (u32*)NORDIC_DFU_MAGIC_NUMBER_ADDRESS;
		//Check if the magic number is currently set to enable nordic dfu
		if (*magicNumberAddress == ENABLE_NORDIC_DFU_MAGIC_NUMBER) {
			logt("WARNING", "Disabling nordic dfu");

			//Overwrite the magic number so that the nordic dfu will be inactive afterwards
			u32 data = 0x00;
			GS->flashStorage.CacheAndWriteData(&data, magicNumberAddress, sizeof(u32), nullptr, 0);
		}
	}
#endif
}

u32 FruityHal::getMasterBootRecordSize()
{
#ifdef SIM_ENABLED
	return 1024 * 4;
#else
	return MBR_SIZE;
#endif
}

u32 FruityHal::getSoftDeviceSize()
{
#ifdef SIM_ENABLED
	return 1024 * 110;
#else
	return SD_SIZE_GET(MBR_SIZE);
#endif
}

BleStackType FruityHal::GetBleStackType()
{
	//TODO: We can later also determine the exact version of the stack if necessary
	//It is however not easy to implement this for Nordic as there is no public list
	//available.
#ifdef SIM_ENABLED
	return (BleStackType)sim_get_stack_type();
#elif defined(S130)
	return BleStackType::NRF_SD_130_ANY;
#elif defined(S132)
	return BleStackType::NRF_SD_132_ANY;
#elif defined(S140)
	return BleStackType::NRF_SD_140_ANY;
#else
#error "Unsupported Stack"
#endif
}

void FruityHal::bleStackErrorHandler(u32 id, u32 info)
{
	switch (id) {
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
			GS->ramRetainStructPtr->code2 = ((assert_info_t *)info)->line_num;
			u8 len = (u8)strlen((const char*)((assert_info_t *)info)->p_file_name);
			if (len > (RAM_PERSIST_STACKSTRACE_SIZE - 1) * 4) len = (RAM_PERSIST_STACKSTRACE_SIZE - 1) * 4;
			memcpy(GS->ramRetainStructPtr->stacktrace + 1, ((assert_info_t *)info)->p_file_name, len);
			break;
		}
		case NRF_FAULT_ID_SDK_ERROR: //SDK errors
		{
			GS->ramRetainStructPtr->code2 = ((error_info_t *)info)->line_num;
			GS->ramRetainStructPtr->code3 = ((error_info_t *)info)->err_code;

			//Copy filename to stacktrace
			u8 len = (u8)strlen((const char*)((error_info_t *)info)->p_file_name);
			if (len > (RAM_PERSIST_STACKSTRACE_SIZE - 1) * 4) len = (RAM_PERSIST_STACKSTRACE_SIZE - 1) * 4;
			memcpy(GS->ramRetainStructPtr->stacktrace + 1, ((error_info_t *)info)->p_file_name, len);
			break;
		}
	}
}

const char* FruityHal::getBleEventNameString(u16 bleEventId)
{
#if defined(TERMINAL_ENABLED)
	switch (bleEventId)
	{
#if defined(NRF51) || defined SIM_ENABLED
	case BLE_EVT_TX_COMPLETE:
		return "BLE_EVT_TX_COMPLETE";
#endif
	case BLE_EVT_USER_MEM_REQUEST:
		return "BLE_EVT_USER_MEM_REQUEST";
	case BLE_EVT_USER_MEM_RELEASE:
		return "BLE_EVT_USER_MEM_RELEASE";
	case BLE_GAP_EVT_CONNECTED:
		return "BLE_GAP_EVT_CONNECTED";
	case BLE_GAP_EVT_DISCONNECTED:
		return "BLE_GAP_EVT_DISCONNECTED";
	case BLE_GAP_EVT_CONN_PARAM_UPDATE:
		return "BLE_GAP_EVT_CONN_PARAM_UPDATE";
	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		return "BLE_GAP_EVT_SEC_PARAMS_REQUEST";
	case BLE_GAP_EVT_SEC_INFO_REQUEST:
		return "BLE_GAP_EVT_SEC_INFO_REQUEST";
	case BLE_GAP_EVT_PASSKEY_DISPLAY:
		return "BLE_GAP_EVT_PASSKEY_DISPLAY";
	case BLE_GAP_EVT_AUTH_KEY_REQUEST:
		return "BLE_GAP_EVT_AUTH_KEY_REQUEST";
	case BLE_GAP_EVT_AUTH_STATUS:
		return "BLE_GAP_EVT_AUTH_STATUS";
	case BLE_GAP_EVT_CONN_SEC_UPDATE:
		return "BLE_GAP_EVT_CONN_SEC_UPDATE";
	case BLE_GAP_EVT_TIMEOUT:
		return "BLE_GAP_EVT_TIMEOUT";
	case BLE_GAP_EVT_RSSI_CHANGED:
		return "BLE_GAP_EVT_RSSI_CHANGED";
	case BLE_GAP_EVT_ADV_REPORT:
		return "BLE_GAP_EVT_ADV_REPORT";
	case BLE_GAP_EVT_SEC_REQUEST:
		return "BLE_GAP_EVT_SEC_REQUEST";
	case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
		return "BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST";
	case BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP:
		return "BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP";
	case BLE_GATTC_EVT_REL_DISC_RSP:
		return "BLE_GATTC_EVT_REL_DISC_RSP";
	case BLE_GATTC_EVT_CHAR_DISC_RSP:
		return "BLE_GATTC_EVT_CHAR_DISC_RSP";
	case BLE_GATTC_EVT_DESC_DISC_RSP:
		return "BLE_GATTC_EVT_DESC_DISC_RSP";
	case BLE_GATTC_EVT_CHAR_VAL_BY_UUID_READ_RSP:
		return "BLE_GATTC_EVT_CHAR_VAL_BY_UUID_READ_RSP";
	case BLE_GATTC_EVT_READ_RSP:
		return "BLE_GATTC_EVT_READ_RSP";
	case BLE_GATTC_EVT_CHAR_VALS_READ_RSP:
		return "BLE_GATTC_EVT_CHAR_VALS_READ_RSP";
	case BLE_GATTC_EVT_WRITE_RSP:
		return "BLE_GATTC_EVT_WRITE_RSP";
	case BLE_GATTC_EVT_HVX:
		return "BLE_GATTC_EVT_HVX";
	case BLE_GATTC_EVT_TIMEOUT:
		return "BLE_GATTC_EVT_TIMEOUT";
	case BLE_GATTS_EVT_WRITE:
		return "BLE_GATTS_EVT_WRITE";
	case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
		return "BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST";
	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		return "BLE_GATTS_EVT_SYS_ATTR_MISSING";
	case BLE_GATTS_EVT_HVC:
		return "BLE_GATTS_EVT_HVC";
	case BLE_GATTS_EVT_SC_CONFIRM:
		return "BLE_GATTS_EVT_SC_CONFIRM";
	case BLE_GATTS_EVT_TIMEOUT:
		return "BLE_GATTS_EVT_TIMEOUT";
#ifdef NRF52
	case BLE_GATTS_EVT_HVN_TX_COMPLETE:
		return "BLE_GATTS_EVT_HVN_TX_COMPLETE";
#endif
	default:
		SIMEXCEPTION(ErrorCodeUnknownException); //Could be an error or should be added to the list
		return "UNKNOWN_EVENT";
	}
#else
	return nullptr;
#endif
}

const char* FruityHal::getGattStatusErrorString(u16 gattStatusCode)
{
#if defined(TERMINAL_ENABLED)
	switch (gattStatusCode)
	{
	case BLE_GATT_STATUS_SUCCESS:
		return "Success";
	case BLE_GATT_STATUS_UNKNOWN:
		return "Unknown or not applicable status";
	case BLE_GATT_STATUS_ATTERR_INVALID:
		return "ATT Error: Invalid Error Code";
	case BLE_GATT_STATUS_ATTERR_INVALID_HANDLE:
		return "ATT Error: Invalid Attribute Handle";
	case BLE_GATT_STATUS_ATTERR_READ_NOT_PERMITTED:
		return "ATT Error: Read not permitted";
	case BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED:
		return "ATT Error: Write not permitted";
	case BLE_GATT_STATUS_ATTERR_INVALID_PDU:
		return "ATT Error: Used in ATT as Invalid PDU";
	case BLE_GATT_STATUS_ATTERR_INSUF_AUTHENTICATION:
		return "ATT Error: Authenticated link required";
	case BLE_GATT_STATUS_ATTERR_REQUEST_NOT_SUPPORTED:
		return "ATT Error: Used in ATT as Request Not Supported";
	case BLE_GATT_STATUS_ATTERR_INVALID_OFFSET:
		return "ATT Error: Offset specified was past the end of the attribute";
	case BLE_GATT_STATUS_ATTERR_INSUF_AUTHORIZATION:
		return "ATT Error: Used in ATT as Insufficient Authorisation";
	case BLE_GATT_STATUS_ATTERR_PREPARE_QUEUE_FULL:
		return "ATT Error: Used in ATT as Prepare Queue Full";
	case BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_FOUND:
		return "ATT Error: Used in ATT as Attribute not found";
	case BLE_GATT_STATUS_ATTERR_ATTRIBUTE_NOT_LONG:
		return "ATT Error: Attribute cannot be read or written using read/write blob requests";
	case BLE_GATT_STATUS_ATTERR_INSUF_ENC_KEY_SIZE:
		return "ATT Error: Encryption key size used is insufficient";
	case BLE_GATT_STATUS_ATTERR_INVALID_ATT_VAL_LENGTH:
		return "ATT Error: Invalid value size";
	case BLE_GATT_STATUS_ATTERR_UNLIKELY_ERROR:
		return "ATT Error: Very unlikely error";
	case BLE_GATT_STATUS_ATTERR_INSUF_ENCRYPTION:
		return "ATT Error: Encrypted link required";
	case BLE_GATT_STATUS_ATTERR_UNSUPPORTED_GROUP_TYPE:
		return "ATT Error: Attribute type is not a supported grouping attribute";
	case BLE_GATT_STATUS_ATTERR_INSUF_RESOURCES:
		return "ATT Error: Encrypted link required";
	case BLE_GATT_STATUS_ATTERR_RFU_RANGE1_BEGIN:
		return "ATT Error: Reserved for Future Use range #1 begin";
	case BLE_GATT_STATUS_ATTERR_RFU_RANGE1_END:
		return "ATT Error: Reserved for Future Use range #1 end";
	case BLE_GATT_STATUS_ATTERR_APP_BEGIN:
		return "ATT Error: Application range begin";
	case BLE_GATT_STATUS_ATTERR_APP_END:
		return "ATT Error: Application range end";
	case BLE_GATT_STATUS_ATTERR_RFU_RANGE2_BEGIN:
		return "ATT Error: Reserved for Future Use range #2 begin";
	case BLE_GATT_STATUS_ATTERR_RFU_RANGE2_END:
		return "ATT Error: Reserved for Future Use range #2 end";
	case BLE_GATT_STATUS_ATTERR_RFU_RANGE3_BEGIN:
		return "ATT Error: Reserved for Future Use range #3 begin";
	case BLE_GATT_STATUS_ATTERR_RFU_RANGE3_END:
		return "ATT Error: Reserved for Future Use range #3 end";
	case BLE_GATT_STATUS_ATTERR_CPS_CCCD_CONFIG_ERROR:
		return "ATT Common Profile and Service Error: Client Characteristic Configuration Descriptor improperly configured";
	case BLE_GATT_STATUS_ATTERR_CPS_PROC_ALR_IN_PROG:
		return "ATT Common Profile and Service Error: Procedure Already in Progress";
	case BLE_GATT_STATUS_ATTERR_CPS_OUT_OF_RANGE:
		return "ATT Common Profile and Service Error: Out Of Range";
	default:
		SIMEXCEPTION(ErrorCodeUnknownException); //Could be an error or should be added to the list
		return "Unknown GATT status";
	}
#else
	return nullptr;
#endif
}

const char* FruityHal::getGeneralErrorString(GeneralHardwareError nrfErrorCode)
{
#if defined(TERMINAL_ENABLED)
	switch ((u32)nrfErrorCode)
	{
	case NRF_SUCCESS:
		return "NRF_SUCCESS";
	case NRF_ERROR_SVC_HANDLER_MISSING:
		return "NRF_ERROR_SVC_HANDLER_MISSING";
	case NRF_ERROR_SOFTDEVICE_NOT_ENABLED:
		return "NRF_ERROR_SOFTDEVICE_NOT_ENABLED";
	case NRF_ERROR_INTERNAL:
		return "NRF_ERROR_INTERNAL";
	case NRF_ERROR_NO_MEM:
		return "NRF_ERROR_NO_MEM";
	case NRF_ERROR_NOT_FOUND:
		return "NRF_ERROR_NOT_FOUND";
	case NRF_ERROR_NOT_SUPPORTED:
		return "NRF_ERROR_NOT_SUPPORTED";
	case NRF_ERROR_INVALID_PARAM:
		return "NRF_ERROR_INVALID_PARAM";
	case NRF_ERROR_INVALID_STATE:
		return "NRF_ERROR_INVALID_STATE";
	case NRF_ERROR_INVALID_LENGTH:
		return "NRF_ERROR_INVALID_LENGTH";
	case NRF_ERROR_INVALID_FLAGS:
		return "NRF_ERROR_INVALID_FLAGS";
	case NRF_ERROR_INVALID_DATA:
		return "NRF_ERROR_INVALID_DATA";
	case NRF_ERROR_DATA_SIZE:
		return "NRF_ERROR_DATA_SIZE";
	case NRF_ERROR_TIMEOUT:
		return "NRF_ERROR_TIMEOUT";
	case NRF_ERROR_NULL:
		return "NRF_ERROR_NULL";
	case NRF_ERROR_FORBIDDEN:
		return "NRF_ERROR_FORBIDDEN";
	case NRF_ERROR_INVALID_ADDR:
		return "NRF_ERROR_INVALID_ADDR";
	case NRF_ERROR_BUSY:
		return "NRF_ERROR_BUSY";
	case BLE_ERROR_INVALID_CONN_HANDLE:
		return "BLE_ERROR_INVALID_CONN_HANDLE";
	case BLE_ERROR_INVALID_ATTR_HANDLE:
		return "BLE_ERROR_INVALID_ATTR_HANDLE";
#if defined(NRF51)
	case BLE_ERROR_NO_TX_PACKETS:
		return "BLE_ERROR_NO_TX_PACKETS";
#endif
	case 0xDEADBEEF:
		return "DEADBEEF";
	default:
		SIMEXCEPTION(ErrorCodeUnknownException); //Could be an error or should be added to the list
		return "UNKNOWN_ERROR";
	}
#else
	return nullptr;
#endif
}

const char* FruityHal::getHciErrorString(FruityHal::HciErrorCode hciErrorCode)
{
#if defined(TERMINAL_ENABLED)
	switch (hciErrorCode)
	{
	case FruityHal::HciErrorCode::SUCCESS:
		return "Success";

	case FruityHal::HciErrorCode::UNKNOWN_BTLE_COMMAND:
		return "Unknown BLE Command";

	case FruityHal::HciErrorCode::UNKNOWN_CONNECTION_IDENTIFIER:
		return "Unknown Connection Identifier";

	case FruityHal::HciErrorCode::AUTHENTICATION_FAILURE:
		return "Authentication Failure";

	case FruityHal::HciErrorCode::CONN_FAILED_TO_BE_ESTABLISHED:
		return "Connection Failed to be Established";

	case FruityHal::HciErrorCode::CONN_INTERVAL_UNACCEPTABLE:
		return "Connection Interval Unacceptable";

	case FruityHal::HciErrorCode::CONN_TERMINATED_DUE_TO_MIC_FAILURE:
		return "Connection Terminated due to MIC Failure";

	case FruityHal::HciErrorCode::CONNECTION_TIMEOUT:
		return "Connection Timeout";

	case FruityHal::HciErrorCode::CONTROLLER_BUSY:
		return "Controller Busy";

	case FruityHal::HciErrorCode::DIFFERENT_TRANSACTION_COLLISION:
		return "Different Transaction Collision";

	case FruityHal::HciErrorCode::DIRECTED_ADVERTISER_TIMEOUT:
		return "Directed Adverisement Timeout";

	case FruityHal::HciErrorCode::INSTANT_PASSED:
		return "Instant Passed";

	case FruityHal::HciErrorCode::LOCAL_HOST_TERMINATED_CONNECTION:
		return "Local Host Terminated Connection";

	case FruityHal::HciErrorCode::MEMORY_CAPACITY_EXCEEDED:
		return "Memory Capacity Exceeded";

	case FruityHal::HciErrorCode::PAIRING_WITH_UNIT_KEY_UNSUPPORTED:
		return "Pairing with Unit Key Unsupported";

	case FruityHal::HciErrorCode::REMOTE_DEV_TERMINATION_DUE_TO_LOW_RESOURCES:
		return "Remote Device Terminated Connection due to low resources";

	case FruityHal::HciErrorCode::REMOTE_DEV_TERMINATION_DUE_TO_POWER_OFF:
		return "Remote Device Terminated Connection due to power off";

	case FruityHal::HciErrorCode::REMOTE_USER_TERMINATED_CONNECTION:
		return "Remote User Terminated Connection";

	case FruityHal::HciErrorCode::COMMAND_DISALLOWED:
		return "Command Disallowed";

	case FruityHal::HciErrorCode::INVALID_BTLE_COMMAND_PARAMETERS:
		return "Invalid BLE Command Parameters";

	case FruityHal::HciErrorCode::INVALID_LMP_PARAMETERS:
		return "Invalid LMP Parameters";

	case FruityHal::HciErrorCode::LMP_PDU_NOT_ALLOWED:
		return "LMP PDU Not Allowed";

	case FruityHal::HciErrorCode::LMP_RESPONSE_TIMEOUT:
		return "LMP Response Timeout";

	case FruityHal::HciErrorCode::PIN_OR_KEY_MISSING:
		return "Pin or Key missing";

	case FruityHal::HciErrorCode::UNSPECIFIED_ERROR:
		return "Unspecified Error";

	case FruityHal::HciErrorCode::UNSUPPORTED_REMOTE_FEATURE:
		return "Unsupported Remote Feature";
	default:
		SIMEXCEPTION(ErrorCodeUnknownException); //Could be an error or should be added to the list
		return "Unknown HCI error";
	}
#else
	return nullptr;
#endif
}

const char* FruityHal::getErrorLogErrorType(ErrorTypes type)
{
#if defined(TERMINAL_ENABLED)
	switch (type)
	{
		case ErrorTypes::SD_CALL_ERROR:
			return "SD_CALL_ERROR";
		case ErrorTypes::HCI_ERROR:
			return "HCI_ERROR";
		case ErrorTypes::CUSTOM:
			return "CUSTOM";
		case ErrorTypes::GATT_STATUS:
			return "GATT_STATUS";
		case ErrorTypes::REBOOT:
			return "REBOOT";
		default:
			SIMEXCEPTION(ErrorCodeUnknownException); //Could be an error or should be added to the list
			return "UNKNOWN_ERROR";
	}
#else
	return nullptr;
#endif
}

const char* FruityHal::getErrorLogCustomError(CustomErrorTypes type)
{
#if defined(TERMINAL_ENABLED)
	switch (type)
	{
	case CustomErrorTypes::FATAL_BLE_GATTC_EVT_TIMEOUT_FORCED_US:
		return "FATAL_BLE_GATTC_EVT_TIMEOUT_FORCED_US";
	case CustomErrorTypes::INFO_TRYING_CONNECTION_SUSTAIN:
		return "INFO_TRYING_CONNECTION_SUSTAIN";
	case CustomErrorTypes::WARN_CONNECTION_SUSTAIN_FAILED_TO_ESTABLISH:
		return "WARN_CONNECTION_SUSTAIN_FAILED_TO_ESTABLISH";
	case CustomErrorTypes::COUNT_CONNECTION_SUCCESS:
		return "COUNT_CONNECTION_SUCCESS";
	case CustomErrorTypes::COUNT_HANDSHAKE_DONE:
		return "COUNT_HANDSHAKE_DONE";
	case CustomErrorTypes::WARN_HANDSHAKE_TIMEOUT:
		return "WARN_HANDSHAKE_TIMEOUT";
	case CustomErrorTypes::WARN_CM_FAIL_NO_SPOT:
		return "WARN_CM_FAIL_NO_SPOT";
	case CustomErrorTypes::FATAL_QUEUE_NUM_MISMATCH:
		return "FATAL_QUEUE_NUM_MISMATCH";
	case CustomErrorTypes::WARN_GATT_WRITE_ERROR:
		return "WARN_GATT_WRITE_ERROR";
	case CustomErrorTypes::WARN_TX_WRONG_DATA:
		return "WARN_TX_WRONG_DATA";
	case CustomErrorTypes::WARN_RX_WRONG_DATA:
		return "WARN_RX_WRONG_DATA";
	case CustomErrorTypes::FATAL_CLUSTER_UPDATE_FLOW_MISMATCH:
		return "FATAL_CLUSTER_UPDATE_FLOW_MISMATCH";
	case CustomErrorTypes::WARN_HIGH_PRIO_QUEUE_FULL:
		return "WARN_HIGH_PRIO_QUEUE_FULL";
	case CustomErrorTypes::COUNT_NO_PENDING_CONNECTION:
		return "COUNT_NO_PENDING_CONNECTION";
	case CustomErrorTypes::FATAL_HANDLE_PACKET_SENT_ERROR:
		return "FATAL_HANDLE_PACKET_SENT_ERROR";
	case CustomErrorTypes::COUNT_DROPPED_PACKETS:
		return "COUNT_DROPPED_PACKETS";
	case CustomErrorTypes::COUNT_SENT_PACKETS_RELIABLE:
		return "COUNT_SENT_PACKETS_RELIABLE";
	case CustomErrorTypes::COUNT_SENT_PACKETS_UNRELIABLE:
		return "COUNT_SENT_PACKETS_UNRELIABLE";
	case CustomErrorTypes::INFO_ERRORS_REQUESTED:
		return "INFO_ERRORS_REQUESTED";
	case CustomErrorTypes::INFO_CONNECTION_SUSTAIN_SUCCESS:
		return "INFO_CONNECTION_SUSTAIN_SUCCESS";
	case CustomErrorTypes::COUNT_JOIN_ME_RECEIVED:
		return "COUNT_JOIN_ME_RECEIVED";
	case CustomErrorTypes::WARN_CONNECT_AS_MASTER_NOT_POSSIBLE:
		return "WARN_CONNECT_AS_MASTER_NOT_POSSIBLE";
	case CustomErrorTypes::FATAL_PENDING_NOT_CLEARED:
		return "FATAL_PENDING_NOT_CLEARED";
	case CustomErrorTypes::FATAL_PROTECTED_PAGE_ERASE:
		return "FATAL_PROTECTED_PAGE_ERASE";
	case CustomErrorTypes::INFO_IGNORING_CONNECTION_SUSTAIN:
		return "INFO_IGNORING_CONNECTION_SUSTAIN";
	case CustomErrorTypes::INFO_IGNORING_CONNECTION_SUSTAIN_LEAF:
		return "INFO_IGNORING_CONNECTION_SUSTAIN_LEAF";
	case CustomErrorTypes::COUNT_GATT_CONNECT_FAILED:
		return "COUNT_GATT_CONNECT_FAILED";
	case CustomErrorTypes::FATAL_PACKET_PROCESSING_FAILED:
		return "FATAL_PACKET_PROCESSING_FAILED";
	case CustomErrorTypes::FATAL_PACKET_TOO_BIG:
		return "FATAL_PACKET_TOO_BIG";
	case CustomErrorTypes::COUNT_HANDSHAKE_ACK1_DUPLICATE:
		return "COUNT_HANDSHAKE_ACK1_DUPLICATE";
	case CustomErrorTypes::COUNT_HANDSHAKE_ACK2_DUPLICATE:
		return "COUNT_HANDSHAKE_ACK2_DUPLICATE";
	case CustomErrorTypes::COUNT_ENROLLMENT_NOT_SAVED:
		return "COUNT_ENROLLMENT_NOT_SAVED";
	case CustomErrorTypes::COUNT_FLASH_OPERATION_ERROR:
		return "COUNT_FLASH_OPERATION_ERROR";
	case CustomErrorTypes::FATAL_WRONG_FLASH_STORAGE_COMMAND:
		return "FATAL_WRONG_FLASH_STORAGE_COMMAND";
	case CustomErrorTypes::FATAL_ABORTED_FLASH_TRANSACTION:
		return "FATAL_ABORTED_FLASH_TRANSACTION";
	case CustomErrorTypes::FATAL_PACKETQUEUE_PACKET_TOO_BIG:
		return "FATAL_PACKETQUEUE_PACKET_TOO_BIG";
	case CustomErrorTypes::FATAL_NO_RECORDSTORAGE_SPACE_LEFT:
		return "FATAL_NO_RECORDSTORAGE_SPACE_LEFT";
	case CustomErrorTypes::FATAL_RECORD_CRC_WRONG:
		return "FATAL_RECORD_CRC_WRONG";
	case CustomErrorTypes::COUNT_3RD_PARTY_TIMEOUT:
		return "COUNT_3RD_PARTY_TIMEOUT";
	case CustomErrorTypes::FATAL_CONNECTION_ALLOCATOR_OUT_OF_MEMORY:
		return "FATAL_CONNECTION_ALLOCATOR_OUT_OF_MEMORY";
	case CustomErrorTypes::FATAL_CONNECTION_REMOVED_WHILE_TIME_SYNC:
		return "FATAL_CONNECTION_REMOVED_WHILE_TIME_SYNC";
	case CustomErrorTypes::FATAL_COULD_NOT_RETRIEVE_CAPABILITIES:
		return "FATAL_COULD_NOT_RETRIEVE_CAPABILITIES";
	default:
		SIMEXCEPTION(ErrorCodeUnknownException); //Could be an error or should be added to the list
		return "UNKNOWN_ERROR";
	}
#else
	return nullptr;
#endif
}

const char* FruityHal::getErrorLogRebootReason(RebootReason type)
{
#if defined(TERMINAL_ENABLED)
	switch (type)
	{
	case RebootReason::UNKNOWN:
		return "UNKNOWN";
	case RebootReason::HARDFAULT:
		return "HARDFAULT";
	case RebootReason::APP_FAULT:
		return "APP_FAULT";
	case RebootReason::SD_FAULT:
		return "SD_FAULT";
	case RebootReason::PIN_RESET:
		return "PIN_RESET";
	case RebootReason::WATCHDOG:
		return "WATCHDOG";
	case RebootReason::FROM_OFF_STATE:
		return "FROM_OFF_STATE";
	case RebootReason::LOCAL_RESET:
		return "LOCAL_RESET";
	case RebootReason::REMOTE_RESET:
		return "REMOTE_RESET";
	case RebootReason::ENROLLMENT:
		return "ENROLLMENT";
	case RebootReason::PREFERRED_CONNECTIONS:
		return "PREFERRED_CONNECTIONS";
	case RebootReason::DFU:
		return "DFU";
	default:
		SIMEXCEPTION(ErrorCodeUnknownException); //Could be an error or should be added to the list
		return "UNDEFINED";
	}
#else
	return nullptr;
#endif
}

const char * FruityHal::getErrorLogError(ErrorTypes type, u32 code)
{
#if defined(TERMINAL_ENABLED)
	switch (type)
	{
		case ErrorTypes::SD_CALL_ERROR:
			return getGeneralErrorString((GeneralHardwareError)code);
		case ErrorTypes::HCI_ERROR:
			return getHciErrorString((HciErrorCode)code);
		case ErrorTypes::CUSTOM:
			return getErrorLogCustomError((CustomErrorTypes)code);
		case ErrorTypes::GATT_STATUS:
			return getGattStatusErrorString(code);
		case ErrorTypes::REBOOT:
			return getErrorLogRebootReason((RebootReason)code);
		default:
			SIMEXCEPTION(ErrorCodeUnknownException); //Could be an error or should be added to the list
			return "UNKNOWN_TYPE";
	}
#else
	return nullptr;
#endif
}

u32 * FruityHal::getUicrDataPtr()
{
	//We are using a magic number to determine if the UICR data present was put there by fruitydeploy
	if (NRF_UICR->CUSTOMER[0] == UICR_SETTINGS_MAGIC_WORD) {
		return (u32*)NRF_UICR->CUSTOMER;
	}
	else if(GS->recordStorage.IsInit()){
		//On some devices, we are not able to store data in UICR as they are flashed by a 3rd party
		//and we are only updating to fruitymesh. We have a dedicated record for these instances
		//which is used the same as if the data were stored in UICR
		SizedData data = GS->recordStorage.GetRecordData(RECORD_STORAGE_RECORD_ID_UICR_REPLACEMENT);
		if (data.length >= 16 * 4 && ((u32*)data.data)[0] == UICR_SETTINGS_MAGIC_WORD) {
			return (u32*)data.data;
		}
	}

	return nullptr;
}

void FruityHal::disableUart()
{
#ifndef SIM_ENABLED
	//Disable UART interrupt
	sd_nvic_DisableIRQ(UART0_IRQn);

	//Disable all UART Events
	nrf_uart_int_disable(NRF_UART0, NRF_UART_INT_MASK_RXDRDY |
		NRF_UART_INT_MASK_TXDRDY |
		NRF_UART_INT_MASK_ERROR |
		NRF_UART_INT_MASK_RXTO);
	//Clear all pending events
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_CTS);
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_NCTS);
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXDRDY);
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_TXDRDY);
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_ERROR);
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXTO);

	//Disable UART
	NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Disabled;

	//Reset all Pinx to default state
	nrf_uart_txrx_pins_disconnect(NRF_UART0);
	nrf_uart_hwfc_pins_disconnect(NRF_UART0);

	nrf_gpio_cfg_default(Boardconfig->uartTXPin);
	nrf_gpio_cfg_default(Boardconfig->uartRXPin);

	if (Boardconfig->uartRTSPin != -1) {
		if (NRF_UART0->PSELRTS != NRF_UART_PSEL_DISCONNECTED) nrf_gpio_cfg_default(Boardconfig->uartRTSPin);
		if (NRF_UART0->PSELCTS != NRF_UART_PSEL_DISCONNECTED) nrf_gpio_cfg_default(Boardconfig->uartCTSPin);
	}
#endif
}

void FruityHal::enableUart()
{
	//Configure pins
	nrf_gpio_pin_set(Boardconfig->uartTXPin);
	nrf_gpio_cfg_output(Boardconfig->uartTXPin);
	nrf_gpio_cfg_input(Boardconfig->uartRXPin, NRF_GPIO_PIN_NOPULL);

	nrf_uart_baudrate_set(NRF_UART0, (nrf_uart_baudrate_t)Boardconfig->uartBaudRate);
	nrf_uart_configure(NRF_UART0, NRF_UART_PARITY_EXCLUDED, Boardconfig->uartRTSPin != -1 ? NRF_UART_HWFC_ENABLED : NRF_UART_HWFC_DISABLED);
	nrf_uart_txrx_pins_set(NRF_UART0, Boardconfig->uartTXPin, Boardconfig->uartRXPin);

	//Configure RTS/CTS (if RTS is -1, disable flow control)
	if (Boardconfig->uartRTSPin != -1) {
		nrf_gpio_cfg_input(Boardconfig->uartCTSPin, NRF_GPIO_PIN_NOPULL);
		nrf_gpio_pin_set(Boardconfig->uartRTSPin);
		nrf_gpio_cfg_output(Boardconfig->uartRTSPin);
		nrf_uart_hwfc_pins_set(NRF_UART0, Boardconfig->uartRTSPin, Boardconfig->uartCTSPin);
	}

	//Enable Interrupts + timeout events
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXTO);
	nrf_uart_int_enable(NRF_UART0, NRF_UART_INT_MASK_RXTO);

	sd_nvic_SetPriority(UART0_IRQn, APP_IRQ_PRIORITY_LOW);
	sd_nvic_ClearPendingIRQ(UART0_IRQn);
	sd_nvic_EnableIRQ(UART0_IRQn);

	//Enable UART
	nrf_uart_enable(NRF_UART0);

	//Enable Receiver
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_ERROR);
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXDRDY);
	nrf_uart_task_trigger(NRF_UART0, NRF_UART_TASK_STARTRX);

	//Enable Transmitter
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_TXDRDY);
	nrf_uart_task_trigger(NRF_UART0, NRF_UART_TASK_STARTTX);

	//Start receiving RX events
	FruityHal::enableUartReadInterrupt();

}

void FruityHal::enableUartReadInterrupt()
{
	//Enables Interrupts
	nrf_uart_int_enable(NRF_UART0, NRF_UART_INT_MASK_RXDRDY | NRF_UART_INT_MASK_ERROR);
}

bool FruityHal::checkAndHandleUartTimeout()
{
#ifndef SIM_ENABLED
	if (nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_RXTO))
	{
		nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXTO);

		//Restart transmission and clear previous buffer
		nrf_uart_task_trigger(NRF_UART0, NRF_UART_TASK_STARTRX);

		return true;
	}
#endif

	return false;
}

u32 FruityHal::checkAndHandleUartError()
{
#ifndef SIM_ENABLED
	//Checks if an error occured
	if (nrf_uart_int_enable_check(NRF_UART0, NRF_UART_INT_MASK_ERROR) &&
		nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_ERROR))
	{
		nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_ERROR);

		//Errorsource is given, but has to be cleared to be handled
		NRF_UART0->ERRORSRC = NRF_UART0->ERRORSRC;

		return NRF_UART0->ERRORSRC;
	}
#endif
	return 0;
}

bool FruityHal::readUartByte(char * outByte)
{

	if (nrf_uart_int_enable_check(NRF_UART0, NRF_UART_INT_MASK_RXDRDY) &&
		nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_RXDRDY))
	{
		//Reads the byte
		nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXDRDY);
#ifndef SIM_ENABLED
		*outByte = NRF_UART0->RXD;
#else
		*outByte = nrf_uart_rxd_get(NRF_UART0);
#endif

		//Disable the interrupt to stop receiving until instructed further
		nrf_uart_int_disable(NRF_UART0, NRF_UART_INT_MASK_RXDRDY | NRF_UART_INT_MASK_ERROR);

		return true;
	}

	return false;
}
