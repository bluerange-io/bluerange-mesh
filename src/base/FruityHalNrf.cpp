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

extern "C"
{
#ifdef NRF52
#include <nrf_power.h>
#endif
#ifndef SIM_ENABLED
#include <app_util_platform.h>
#include <nrf_uart.h>
#include <nrf_mbr.h>
#ifdef NRF52
#include <nrf_drv_twi.h>
#include <nrf_drv_spi.h>
#endif
#endif
}

#define APP_TIMER_PRESCALER 0	 // Value of the RTC1 PRESCALER register
#define APP_TIMER_OP_QUEUE_SIZE 1 //Size of timer operation queues

#define APP_TIMER_MAX_TIMERS 5 //Maximum number of simultaneously created timers (2 + BSP_APP_TIMERS_NUMBER)

#ifndef SIM_ENABLED
static SimpleArray<app_timer_t, APP_TIMER_MAX_TIMERS> swTimers;
#endif

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
	ram_total_size = block_size * NRF_FICR->NUMRAMBLOCK;
#else
	ram_total_size = NRF_FICR->INFO.RAM * 1024;
#endif

	return 0x20000000 + ram_total_size;
}

static inline FruityHal::BleGattEror nrfErrToGenericGatt(u32 code)
{
	if (code == BLE_GATT_STATUS_SUCCESS)
	{
		return FruityHal::BleGattEror::SUCCESS;
	}
	else if ((code == BLE_GATT_STATUS_ATTERR_INVALID) || (code == BLE_GATT_STATUS_ATTERR_INVALID_HANDLE))
	{
		return FruityHal::BleGattEror::UNKNOWN;
	}
	else
	{
		return FruityHal::BleGattEror(code - 0x0100);
	}
}

static inline ErrorType nrfErrToGeneric(u32 code)
{
	//right now generic error has the same meaning and numering
	//FIXME: This is not true
	return (ErrorType)code;
}

ErrorType FruityHal::BleStackInit()
{
	u32 err = 0;

	//Hotfix for NRF52 MeshGW v3 boards to disable NFC and use GPIO Pins 9 and 10
#ifdef CONFIG_NFCT_PINS_AS_GPIOS
	if (NRF_UICR->NFCPINS == 0xFFFFFFFF)
	{
		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
		{
		}
		NRF_UICR->NFCPINS = 0xFFFFFFFEUL;
		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
		{
		}

		NVIC_SystemReset();
	}
#endif

	logt("NODE", "Init Softdevice version 0x%x, Boardid %d", 3, Boardconfig->boardType);

//TODO: Would be better to get the NRF52 part working in the simulator, here is a reduced version for the simulator
#ifdef SIM_ENABLED
	ble_enable_params_t params;
	CheckedMemset(&params, 0x00, sizeof(params));

	//Configure the number of connections as peripheral and central
	params.gap_enable_params.periph_conn_count = GS->config.totalInConnections;   //Number of connections as Peripheral
	params.gap_enable_params.central_conn_count = GS->config.totalOutConnections; //Number of connections as Central

	err = sd_ble_enable(&params, nullptr);
#endif

#if defined(NRF51)
	// Initialize the SoftDevice handler with the low frequency clock source
	//And a reference to the previously allocated buffer
	//No event handler is given because the event handling is done in the main loop
	nrf_clock_lf_cfg_t clock_lf_cfg;

	if (Boardconfig->lfClockSource == NRF_CLOCK_LF_SRC_XTAL)
	{
		clock_lf_cfg.source = NRF_CLOCK_LF_SRC_XTAL;
		clock_lf_cfg.rc_ctiv = 0;
	}
	else
	{
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
	params.gap_enable_params.periph_conn_count = Conf::totalInConnections;   //Number of connections as Peripheral
	params.gap_enable_params.central_conn_count = Conf::totalOutConnections; //Number of connections as Central
	params.gap_enable_params.central_sec_count = 1;							 //this application only needs to be able to pair in one central link at a time

	//Configure Bandwidth (We want all our connections to have a high throughput for RX and TX
	ble_conn_bw_counts_t bwCounts;
	CheckedMemset(&bwCounts, 0x00, sizeof(ble_conn_bw_counts_t));
	bwCounts.rx_counts.high_count = Conf::totalInConnections + Conf::totalOutConnections;
	bwCounts.tx_counts.high_count = Conf::totalInConnections + Conf::totalOutConnections;
	params.common_enable_params.p_conn_bw_counts = &bwCounts;

	//Configure the GATT Server Parameters
	params.gatts_enable_params.service_changed = 1;								//we require the Service Changed characteristic
	params.gatts_enable_params.attr_tab_size = BLE_GATTS_ATTR_TAB_SIZE_DEFAULT; //the default Attribute Table size is appropriate for our application

	//The base ram address is gathered from the linker
	uint32_t app_ram_base = (u32)__application_ram_start_address;
	/* enable the BLE Stack */
	logt("NODE", "Ram base at 0x%x", (u32)app_ram_base);
	err = sd_ble_enable(&params, &app_ram_base);
	if (err == NRF_SUCCESS)
	{
		/* Verify that __LINKER_APP_RAM_BASE matches the SD calculations */
		if (app_ram_base != (u32)__application_ram_start_address)
		{
			logt("WARNING", "Warning: unused memory: 0x%x", ((u32)__application_ram_start_address) - (u32)app_ram_base);
		}
	}
	else if (err == NRF_ERROR_NO_MEM)
	{
		/* Not enough memory for the SoftDevice. Use output value in linker script */
		logt("ERROR", "Fatal: Not enough memory for the selected configuration. Required:0x%x", (u32)app_ram_base);
	}
	else
	{
		APP_ERROR_CHECK(err); //OK
	}

	//After the SoftDevice was enabled, we need to specifiy that we want a high bandwidth configuration
	// for both peripheral and central (6 packets for connInterval
	ble_opt_t opt;
	CheckedMemset(&opt, 0x00, sizeof(opt));
	opt.common_opt.conn_bw.role = BLE_GAP_ROLE_PERIPH;
	opt.common_opt.conn_bw.conn_bw.conn_bw_rx = BLE_CONN_BW_HIGH;
	opt.common_opt.conn_bw.conn_bw.conn_bw_tx = BLE_CONN_BW_HIGH;
	err = sd_ble_opt_set(BLE_COMMON_OPT_CONN_BW, &opt);
	if (err != 0)
		logt("ERROR", "could not set bandwith %u", err);

	CheckedMemset(&opt, 0x00, sizeof(opt));
	opt.common_opt.conn_bw.role = BLE_GAP_ROLE_CENTRAL;
	opt.common_opt.conn_bw.conn_bw.conn_bw_rx = BLE_CONN_BW_HIGH;
	opt.common_opt.conn_bw.conn_bw.conn_bw_tx = BLE_CONN_BW_HIGH;
	err = sd_ble_opt_set(BLE_COMMON_OPT_CONN_BW, &opt);
	if (err != 0)
		logt("ERROR", "could not set bandwith %u", err);

#elif defined(NRF52)

	//######### Enables the Softdevice
	u32 finalErr = 0;

	// Configure the clock source
	nrf_clock_lf_cfg_t clock_lf_cfg;
	if (Boardconfig->lfClockSource == NRF_CLOCK_LF_SRC_XTAL || Boardconfig->lfClockSource == NRF_CLOCK_LF_SRC_SYNTH)
	{
		clock_lf_cfg.source = Boardconfig->lfClockSource;
		clock_lf_cfg.rc_ctiv = 0;	  //Must be 0 for this clock source
		clock_lf_cfg.rc_temp_ctiv = 0; //Must be 0 for this clock source
		clock_lf_cfg.accuracy = Boardconfig->lfClockAccuracy;
	}
	else
	{
		//Use the internal clock of the nrf52 with recommended settings
		clock_lf_cfg.source = NRF_CLOCK_LF_SRC_RC;
		clock_lf_cfg.rc_ctiv = 16;							   //recommended setting for nrf52
		clock_lf_cfg.rc_temp_ctiv = 2;						   //recommended setting for nrf52
		clock_lf_cfg.accuracy = NRF_CLOCK_LF_ACCURACY_500_PPM; //ppm of the internal clock source for all nrf52 (due to chip errata
	}

	//Enable SoftDevice using given clock source
	err = sd_softdevice_enable(&clock_lf_cfg, app_error_fault_handler);
	if (err)
	{
		if (finalErr == 0)
			finalErr = err;
		logt("ERROR", "%u", err);
	}

	logt("ERROR", "SD Enable %u", err);

	uint32_t ram_start = (u32)__application_ram_start_address;

	//######### Sets our custom SoftDevice configuration

	//Create a SoftDevice configuration
	ble_cfg_t ble_cfg;

	// Configure the connection count.
	CheckedMemset(&ble_cfg, 0, sizeof(ble_cfg));
	ble_cfg.conn_cfg.conn_cfg_tag = BLE_CONN_CFG_TAG_FM;
	ble_cfg.conn_cfg.params.gap_conn_cfg.conn_count = Conf::totalInConnections + Conf::totalOutConnections;
	ble_cfg.conn_cfg.params.gap_conn_cfg.event_length = 4; //4 units = 5ms (1.25ms steps) this is the time used to handle one connection

	err = sd_ble_cfg_set(BLE_CONN_CFG_GAP, &ble_cfg, ram_start);
	if (err)
	{
		if (finalErr == 0)
			finalErr = err;
		logt("ERROR", "BLE_CONN_CFG_GAP %u", err);
	}

	// Configure the connection roles.
	CheckedMemset(&ble_cfg, 0, sizeof(ble_cfg));
	ble_cfg.gap_cfg.role_count_cfg.periph_role_count = Conf::totalInConnections;
	ble_cfg.gap_cfg.role_count_cfg.central_role_count = Conf::totalOutConnections;
	ble_cfg.gap_cfg.role_count_cfg.central_sec_count = BLE_GAP_ROLE_COUNT_CENTRAL_SEC_DEFAULT; //TODO: Could change this

	err = sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &ble_cfg, ram_start);
	if (err)
	{
		if (finalErr == 0)
			finalErr = err;
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
	CheckedMemset(&ble_cfg, 0x00, sizeof(ble_cfg));
	ble_cfg.conn_cfg.conn_cfg_tag = BLE_CONN_CFG_TAG_FM;
	ble_cfg.conn_cfg.params.gatt_conn_cfg.att_mtu = MAX_MTU_SIZE;

	err = sd_ble_cfg_set(BLE_CONN_CFG_GATT, &ble_cfg, ram_start);
	APP_ERROR_CHECK(err);

	// Configure number of custom UUIDS.
	CheckedMemset(&ble_cfg, 0, sizeof(ble_cfg));
	ble_cfg.common_cfg.vs_uuid_cfg.vs_uuid_count = BLE_UUID_VS_COUNT_DEFAULT; //TODO: look in implementation

	err = sd_ble_cfg_set(BLE_COMMON_CFG_VS_UUID, &ble_cfg, ram_start);
	if (err)
	{
		if (finalErr == 0)
			finalErr = err;
		logt("ERROR", "BLE_COMMON_CFG_VS_UUID %u", err);
	}

	// Configure the GATTS attribute table.
	CheckedMemset(&ble_cfg, 0x00, sizeof(ble_cfg));
	ble_cfg.gatts_cfg.attr_tab_size.attr_tab_size = NRF_SDH_BLE_GATTS_ATTR_TAB_SIZE;

	err = sd_ble_cfg_set(BLE_GATTS_CFG_ATTR_TAB_SIZE, &ble_cfg, ram_start);
	if (err)
	{
		if (finalErr == 0)
			finalErr = err;
		logt("ERROR", "BLE_GATTS_CFG_ATTR_TAB_SIZE %u", err);
	}

	// Configure Service Changed characteristic.
	CheckedMemset(&ble_cfg, 0x00, sizeof(ble_cfg));
	ble_cfg.gatts_cfg.service_changed.service_changed = BLE_GATTS_SERVICE_CHANGED_DEFAULT;

	err = sd_ble_cfg_set(BLE_GATTS_CFG_SERVICE_CHANGED, &ble_cfg, ram_start);
	if (err)
	{
		if (finalErr == 0)
			finalErr = err;
		logt("ERROR", "BLE_GATTS_CFG_SERVICE_CHANGED %u", err);
	}

	//######### Enables the BLE stack
	err = sd_ble_enable(&ram_start);
	logt("ERROR", "Err %u, Linker Ram section should be at %x, len %x", err, (u32)ram_start, (u32)(getramend() - ram_start));
	APP_ERROR_CHECK(finalErr);
	APP_ERROR_CHECK(err);

	//We also configure connection event length extension to increase the throughput
	ble_opt_t opt;
	CheckedMemset(&opt, 0x00, sizeof(opt));
	opt.common_opt.conn_evt_ext.enable = 1;

	err = sd_ble_opt_set(BLE_COMMON_OPT_CONN_EVT_EXT, &opt);
	if (err != 0)
		logt("ERROR", "Could not configure conn length extension %u", err);

#endif //NRF52

	//Enable DC/DC (needs external LC filter, cmp. nrf51 reference manual page 43)
	err = sd_power_dcdc_mode_set(Boardconfig->dcDcEnabled ? NRF_POWER_DCDC_ENABLE : NRF_POWER_DCDC_DISABLE);
	logt("ERROR", "sd_power_dcdc_mode_set %u", err);
	APP_ERROR_CHECK(err); //OK

	// Set power mode
	err = sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
	logt("ERROR", "sd_power_mode_set %u", err);
	APP_ERROR_CHECK(err); //OK

	err = (u32)FruityHal::RadioSetTxPower(Conf::defaultDBmTX, FruityHal::TxRole::SCAN_INIT, 0);
	APP_ERROR_CHECK(err); //OK

	return (ErrorType)err;
}

void FruityHal::BleStackDeinit()
{
#ifndef SIM_ENABLED
	sd_softdevice_disable();
#endif
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
	if (FM_WATCHDOG_TIMEOUT < 32768UL * 60)
	{
		FruityHal::FeedWatchdog();
	}
#endif

	while (true)
	{
		//Check if there is input on uart
		GS->terminal.CheckAndProcessLine();

		//Fetch the event
		u16 eventSize = GlobalState::SIZE_OF_EVENT_BUFFER;
		u32 err = sd_ble_evt_get((u8 *)GS->currentEventBuffer, &eventSize);

		//Handle ble event event
		if (err == NRF_SUCCESS)
		{
			FruityHal::DispatchBleEvents((void *)GS->currentEventBuffer);
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
	if (GS->button1State == ButtonState::PRESSED || GS->button1State == ButtonState::RELEASED)
	{
		u32 holdTimeDs = GS->button1HoldTimeDs;
		GS->buttonEventHandler(0, holdTimeDs);

		GS->button1HoldTimeDs = 0;				// clear timer value
		GS->button1State = ButtonState::INITAL; // clear state
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
	while (true)
	{
		uint32_t evt_id;
		u32 err = sd_evt_get(&evt_id);

		if (err == NRF_ERROR_NOT_FOUND)
		{
			break;
		}
		else
		{
			GS->systemEventHandler((u32)evt_id); // Call handler
		}
	}

	u32 err = sd_app_evt_wait();
	APP_ERROR_CHECK(err); // OK
	err = sd_nvic_ClearPendingIRQ(SD_EVT_IRQn);
	APP_ERROR_CHECK(err); // OK
}

void FruityHal::DispatchBleEvents(void *eventVirtualPointer)
{
	ble_evt_t &bleEvent = *((ble_evt_t *)eventVirtualPointer);
	u16 eventId = bleEvent.header.evt_id;
	u32 err;
	if (eventId == BLE_GAP_EVT_ADV_REPORT || eventId == BLE_GAP_EVT_RSSI_CHANGED)
	{
		logt("EVENTS2", "BLE EVENT %s (%d)", FruityHal::getBleEventNameString(eventId), eventId);
	}
	else
	{
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
#if (SDK == 15)
		//In the later version of the SDK, we need to call sd_ble_gap_scan_start again with a nullpointer to continue to receive scan data
		if (bleEvent.evt.gap_evt.params.adv_report.type.status != BLE_GAP_ADV_DATA_STATUS_INCOMPLETE_MORE_DATA)
		{
			ble_data_t scan_data;
			scan_data.len = BLE_GAP_SCAN_BUFFER_MAX;
			scan_data.p_data = GS->scanBuffer;
			err = sd_ble_gap_scan_start(NULL, &scan_data);
			APP_ERROR_CHECK(err);
		}
#endif
#if IS_ACTIVE(FAKE_NODE_POSITIONS)
		if (!GS->node.modifyEventForFakePositions(are))
		{
			//The position was faked to such a far place that we
			//should not do anything with the event.
			return;
		}
#endif
		ScanController::getInstance().ScanEventHandler(are);
		for (u32 i = 0; i < GS->amountOfModules; i++)
		{
			if (GS->activeModules[i]->configurationPointer->moduleActive)
			{
				GS->activeModules[i]->GapAdvertisementReportEventHandler(are);
			}
		}
	}
	break;
	case BLE_GAP_EVT_CONNECTED:
	{
		FruityHal::GapConnectedEvent ce(&bleEvent);
		GAPController::getInstance().GapConnectedEventHandler(ce);
		AdvertisingController::getInstance().GapConnectedEventHandler(ce);
		for (u32 i = 0; i < GS->amountOfModules; i++)
		{
			if (GS->activeModules[i]->configurationPointer->moduleActive)
			{
				GS->activeModules[i]->GapConnectedEventHandler(ce);
			}
		}
	}
	break;
	case BLE_GAP_EVT_DISCONNECTED:
	{
		FruityHal::GapDisconnectedEvent de(&bleEvent);
		GAPController::getInstance().GapDisconnectedEventHandler(de);
		AdvertisingController::getInstance().GapDisconnectedEventHandler(de);
		for (u32 i = 0; i < GS->amountOfModules; i++)
		{
			if (GS->activeModules[i]->configurationPointer->moduleActive)
			{
				GS->activeModules[i]->GapDisconnectedEventHandler(de);
			}
		}
	}
	break;
	case BLE_GAP_EVT_TIMEOUT:
	{
		FruityHal::GapTimeoutEvent gte(&bleEvent);
		GAPController::getInstance().GapTimeoutEventHandler(gte);
	}
	break;
	case BLE_GAP_EVT_SEC_INFO_REQUEST:
	{
		FruityHal::GapSecurityInfoRequestEvent sire(&bleEvent);
		GAPController::getInstance().GapSecurityInfoRequestEvenetHandler(sire);
	}
	break;
	case BLE_GAP_EVT_CONN_SEC_UPDATE:
	{
		FruityHal::GapConnectionSecurityUpdateEvent csue(&bleEvent);
		GAPController::getInstance().GapConnectionSecurityUpdateEventHandler(csue);
	}
	break;
	case BLE_GATTC_EVT_WRITE_RSP:
	{
#ifdef SIM_ENABLED
		//if(cherrySimInstance->currentNode->id == 37 && bleEvent.evt.gattc_evt.conn_handle == 680) printf("%04u Q@NODE %u EVT_WRITE_RSP received" EOL, cherrySimInstance->globalBreakCounter++, cherrySimInstance->currentNode->id);
#endif
		FruityHal::GattcWriteResponseEvent wre(&bleEvent);
		ConnectionManager::getInstance().GattcWriteResponseEventHandler(wre);
	}
	break;
	case BLE_GATTC_EVT_TIMEOUT: //jstodo untested event
	{
		FruityHal::GattcTimeoutEvent gte(&bleEvent);
		ConnectionManager::getInstance().GattcTimeoutEventHandler(gte);
	}
	break;
	case BLE_GATTS_EVT_WRITE:
	{
		FruityHal::GattsWriteEvent gwe(&bleEvent);
		ConnectionManager::getInstance().GattsWriteEventHandler(gwe);
	}
	break;
	case BLE_GATTC_EVT_HVX:
	{
		FruityHal::GattcHandleValueEvent hve(&bleEvent);
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
		FruityHal::GattDataTransmittedEvent gdte(&bleEvent);
		ConnectionManager::getInstance().GattDataTransmittedEventHandler(gdte);
		for (int i = 0; i < MAX_MODULE_COUNT; i++)
		{
			if (GS->activeModules[i] != nullptr && GS->activeModules[i]->configurationPointer->moduleActive)
			{
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
	case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
	{
		//We automatically answer all data length update requests
		//The softdevice will chose the parameters so that our configured MAX_MTU_SIZE fits into a link layer packet
		sd_ble_gap_data_length_update(bleEvent.evt.gap_evt.conn_handle, nullptr, nullptr);
	}
	break;

	case BLE_GAP_EVT_DATA_LENGTH_UPDATE:
	{
		ble_gap_evt_data_length_update_t *params = (ble_gap_evt_data_length_update_t *)&bleEvent.evt.gap_evt.params.data_length_update;

		logt("FH", "DLE Result: rx %u/%u, tx %u/%u",
			 params->effective_params.max_rx_octets,
			 params->effective_params.max_rx_time_us,
			 params->effective_params.max_tx_octets,
			 params->effective_params.max_tx_time_us);

		// => We do not notify the application and can assume that it worked if the other device has enough resources
		//    If it does not work, this link will have a slightly reduced throughput, so this is monitored in another place
	}
	break;

	case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
	{
		//We answer all MTU update requests with our max mtu that was configured
		u32 err = sd_ble_gatts_exchange_mtu_reply(bleEvent.evt.gatts_evt.conn_handle, MAX_MTU_SIZE);

		u16 partnerMtu = bleEvent.evt.gatts_evt.params.exchange_mtu_request.client_rx_mtu;
		u16 effectiveMtu = MAX_MTU_SIZE < partnerMtu ? MAX_MTU_SIZE : partnerMtu;

		logt("FH", "Reply MTU Exchange (%u) on conn %u with %u", err, bleEvent.evt.gatts_evt.conn_handle, effectiveMtu);

		ConnectionManager::getInstance().MtuUpdatedHandler(bleEvent.evt.gatts_evt.conn_handle, effectiveMtu);

		break;
	}

	case BLE_GATTC_EVT_EXCHANGE_MTU_RSP:
	{
		u16 partnerMtu = bleEvent.evt.gattc_evt.params.exchange_mtu_rsp.server_rx_mtu;
		u16 effectiveMtu = MAX_MTU_SIZE < partnerMtu ? MAX_MTU_SIZE : partnerMtu;

		logt("FH", "MTU for hnd %u updated to %u", bleEvent.evt.gattc_evt.conn_handle, effectiveMtu);

		ConnectionManager::getInstance().MtuUpdatedHandler(bleEvent.evt.gattc_evt.conn_handle, effectiveMtu);
	}
	break;

#endif
	case BLE_GATTS_EVT_SYS_ATTR_MISSING: //jstodo untested event
	{
		u32 err = 0;
		//Handles missing Attributes, don't know why it is needed
		err = sd_ble_gatts_sys_attr_set(bleEvent.evt.gatts_evt.conn_handle, nullptr, 0, 0);
		logt("ERROR", "SysAttr %u", err);
	}
	break;
#ifdef NRF52
	case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
	{
		//Required for some iOS devices.
		ble_gap_phys_t phy;
		phy.rx_phys = BLE_GAP_PHY_1MBPS;
		phy.tx_phys = BLE_GAP_PHY_1MBPS;

		sd_ble_gap_phy_update(bleEvent.evt.gap_evt.conn_handle, &phy);
	}
	break;
#endif
	}

	if (eventId == BLE_GAP_EVT_ADV_REPORT || eventId == BLE_GAP_EVT_RSSI_CHANGED)
	{
		logt("EVENTS2", "End of event");
	}
	else
	{
		logt("EVENTS", "End of event");
	}
}

static ble_evt_t *currentEvent = nullptr;

FruityHal::GapConnParamUpdateEvent::GapConnParamUpdateEvent(void *_evt)
	: GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_CONN_PARAM_UPDATE)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

FruityHal::GapEvent::GapEvent(void *_evt)
	: BleEvent(_evt)
{
}

u16 FruityHal::GapEvent::getConnectionHandle() const
{
	return currentEvent->evt.gap_evt.conn_handle;
}

u16 FruityHal::GapConnParamUpdateEvent::getMaxConnectionInterval() const
{
	return currentEvent->evt.gap_evt.params.conn_param_update.conn_params.max_conn_interval;
}

FruityHal::GapRssiChangedEvent::GapRssiChangedEvent(void *_evt)
	: GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_RSSI_CHANGED)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

i8 FruityHal::GapRssiChangedEvent::getRssi() const
{
	return currentEvent->evt.gap_evt.params.rssi_changed.rssi;
}

FruityHal::GapAdvertisementReportEvent::GapAdvertisementReportEvent(void *_evt)
	: GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_ADV_REPORT)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

i8 FruityHal::GapAdvertisementReportEvent::getRssi() const
{
#if IS_ACTIVE(FAKE_NODE_POSITIONS)
	if (fakeRssiSet)
	{
		return fakeRssi;
	}
#endif
	return currentEvent->evt.gap_evt.params.adv_report.rssi;
}

const u8 *FruityHal::GapAdvertisementReportEvent::getData() const
{
#if (SDK == 15)
	return currentEvent->evt.gap_evt.params.adv_report.data.p_data;
#else
	return currentEvent->evt.gap_evt.params.adv_report.data;
#endif
}

u32 FruityHal::GapAdvertisementReportEvent::getDataLength() const
{
#if (SDK == 15)
	return currentEvent->evt.gap_evt.params.adv_report.data.len;
#else
	return currentEvent->evt.gap_evt.params.adv_report.dlen;
#endif
}

const u8 *FruityHal::GapAdvertisementReportEvent::getPeerAddr() const
{
	return currentEvent->evt.gap_evt.params.adv_report.peer_addr.addr;
}

FruityHal::BleGapAddrType FruityHal::GapAdvertisementReportEvent::getPeerAddrType() const
{
	return (BleGapAddrType)currentEvent->evt.gap_evt.params.adv_report.peer_addr.addr_type;
}

bool FruityHal::GapAdvertisementReportEvent::isConnectable() const
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

FruityHal::BleEvent::BleEvent(void *_evt)
{
	if (currentEvent != nullptr)
	{
		//This is thrown if two events are processed at the same time, which is illegal.
		SIMEXCEPTION(IllegalStateException); //LCOV_EXCL_LINE assertion
	}
	currentEvent = (ble_evt_t *)_evt;
}

#ifdef SIM_ENABLED
FruityHal::BleEvent::~BleEvent()
{
	currentEvent = nullptr;
}
#endif

FruityHal::GapConnectedEvent::GapConnectedEvent(void *_evt)
	: GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_CONNECTED)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

FruityHal::GapRole FruityHal::GapConnectedEvent::getRole() const
{
	return (GapRole)(currentEvent->evt.gap_evt.params.connected.role);
}

u8 FruityHal::GapConnectedEvent::getPeerAddrType() const
{
	return (currentEvent->evt.gap_evt.params.connected.peer_addr.addr_type);
}

u16 FruityHal::GapConnectedEvent::getMinConnectionInterval() const
{
	return currentEvent->evt.gap_evt.params.connected.conn_params.min_conn_interval;
}

const u8 *FruityHal::GapConnectedEvent::getPeerAddr() const
{
	return (currentEvent->evt.gap_evt.params.connected.peer_addr.addr);
}

FruityHal::GapDisconnectedEvent::GapDisconnectedEvent(void *_evt)
	: GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_DISCONNECTED)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

FruityHal::BleHciError FruityHal::GapDisconnectedEvent::getReason() const
{
	return (FruityHal::BleHciError)currentEvent->evt.gap_evt.params.disconnected.reason;
}

FruityHal::GapTimeoutEvent::GapTimeoutEvent(void *_evt)
	: GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_TIMEOUT)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

FruityHal::GapTimeoutSource FruityHal::GapTimeoutEvent::getSource() const
{
#if defined(NRF51) || defined(SIM_ENABLED)
	switch (currentEvent->evt.gap_evt.params.timeout.src)
	{
	case BLE_GAP_TIMEOUT_SRC_ADVERTISING:
		return GapTimeoutSource::ADVERTISING;
	case BLE_GAP_TIMEOUT_SRC_SECURITY_REQUEST:
		return GapTimeoutSource::SECURITY_REQUEST;
	case BLE_GAP_TIMEOUT_SRC_SCAN:
		return GapTimeoutSource::SCAN;
	case BLE_GAP_TIMEOUT_SRC_CONN:
		return GapTimeoutSource::CONNECTION;
	default:
		return GapTimeoutSource::INVALID;
	}
#elif defined(NRF52)
	switch (currentEvent->evt.gap_evt.params.timeout.src)
	{
#if (SDK != 15)
	case BLE_GAP_TIMEOUT_SRC_ADVERTISING:
		return GapTimeoutSource::ADVERTISING;
#endif
	case BLE_GAP_TIMEOUT_SRC_SCAN:
		return GapTimeoutSource::SCAN;
	case BLE_GAP_TIMEOUT_SRC_CONN:
		return GapTimeoutSource::CONNECTION;
	case BLE_GAP_TIMEOUT_SRC_AUTH_PAYLOAD:
		return GapTimeoutSource::AUTH_PAYLOAD;
	default:
		return GapTimeoutSource::INVALID;
	}
#endif
}

FruityHal::GapSecurityInfoRequestEvent::GapSecurityInfoRequestEvent(void *_evt)
	: GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_SEC_INFO_REQUEST)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

FruityHal::GapConnectionSecurityUpdateEvent::GapConnectionSecurityUpdateEvent(void *_evt)
	: GapEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GAP_EVT_CONN_SEC_UPDATE)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

u8 FruityHal::GapConnectionSecurityUpdateEvent::getKeySize() const
{
	return currentEvent->evt.gap_evt.params.conn_sec_update.conn_sec.encr_key_size;
}

FruityHal::SecurityLevel FruityHal::GapConnectionSecurityUpdateEvent::getSecurityLevel() const
{
	return (FruityHal::SecurityLevel)(currentEvent->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.lv);
}

FruityHal::SecurityMode FruityHal::GapConnectionSecurityUpdateEvent::getSecurityMode() const
{
	return (FruityHal::SecurityMode)(currentEvent->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.sm);
}

FruityHal::GattcEvent::GattcEvent(void *_evt)
	: BleEvent(_evt)
{
}

u16 FruityHal::GattcEvent::getConnectionHandle() const
{
	return currentEvent->evt.gattc_evt.conn_handle;
}

FruityHal::BleGattEror FruityHal::GattcEvent::getGattStatus() const
{
	return nrfErrToGenericGatt(currentEvent->evt.gattc_evt.gatt_status);
}

FruityHal::GattcWriteResponseEvent::GattcWriteResponseEvent(void *_evt)
	: GattcEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GATTC_EVT_WRITE_RSP)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

FruityHal::GattcTimeoutEvent::GattcTimeoutEvent(void *_evt)
	: GattcEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GATTC_EVT_TIMEOUT)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

FruityHal::GattDataTransmittedEvent::GattDataTransmittedEvent(void *_evt)
	: BleEvent(_evt)
{
#if defined(NRF51) || defined(SIM_ENABLED)
	if (currentEvent->header.evt_id != BLE_EVT_TX_COMPLETE)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
#endif
}

u16 FruityHal::GattDataTransmittedEvent::getConnectionHandle() const
{
#if defined(NRF51) || defined(SIM_ENABLED)
	return currentEvent->evt.common_evt.conn_handle;
#elif defined(NRF52)
	if (currentEvent->header.evt_id == BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE)
	{
		return currentEvent->evt.gattc_evt.conn_handle;
	}
	else if (currentEvent->header.evt_id == BLE_GATTS_EVT_HVN_TX_COMPLETE)
	{
		return currentEvent->evt.gatts_evt.conn_handle;
	}
	SIMEXCEPTION(InvalidStateException);
	return -1; //This must never be executed!
#endif
}

bool FruityHal::GattDataTransmittedEvent::isConnectionHandleValid() const
{
	return getConnectionHandle() != BLE_CONN_HANDLE_INVALID;
}

u32 FruityHal::GattDataTransmittedEvent::getCompleteCount() const
{
#if defined(NRF51) || defined(SIM_ENABLED)
	return currentEvent->evt.common_evt.params.tx_complete.count;
#elif defined(NRF52)
	if (currentEvent->header.evt_id == BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE)
	{
		return currentEvent->evt.gattc_evt.params.write_cmd_tx_complete.count;
	}
	else if (currentEvent->header.evt_id == BLE_GATTS_EVT_HVN_TX_COMPLETE)
	{
		return currentEvent->evt.gatts_evt.params.hvn_tx_complete.count;
	}
	SIMEXCEPTION(InvalidStateException);
	return -1; //This must never be executed!
#endif
}

FruityHal::GattsWriteEvent::GattsWriteEvent(void *_evt)
	: BleEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GATTS_EVT_WRITE)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

u16 FruityHal::GattsWriteEvent::getAttributeHandle() const
{
	return currentEvent->evt.gatts_evt.params.write.handle;
}

bool FruityHal::GattsWriteEvent::isWriteRequest() const
{
	return currentEvent->evt.gatts_evt.params.write.op == BLE_GATTS_OP_WRITE_REQ;
}

u16 FruityHal::GattsWriteEvent::getLength() const
{
	return currentEvent->evt.gatts_evt.params.write.len;
}

u16 FruityHal::GattsWriteEvent::getConnectionHandle() const
{
	return currentEvent->evt.gatts_evt.conn_handle;
}

u8 *FruityHal::GattsWriteEvent::getData() const
{
	return (u8 *)currentEvent->evt.gatts_evt.params.write.data;
}

FruityHal::GattcHandleValueEvent::GattcHandleValueEvent(void *_evt)
	: GattcEvent(_evt)
{
	if (currentEvent->header.evt_id != BLE_GATTC_EVT_HVX)
	{
		SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
	}
}

u16 FruityHal::GattcHandleValueEvent::getHandle() const
{
	return currentEvent->evt.gattc_evt.params.hvx.handle;
}

u16 FruityHal::GattcHandleValueEvent::getLength() const
{
	return currentEvent->evt.gattc_evt.params.hvx.len;
}

u8 *FruityHal::GattcHandleValueEvent::getData() const
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

u32 FruityHal::BleGapAddressSet(FruityHal::BleGapAddr const *address)
{
	ble_gap_addr_t addr;
	CheckedMemcpy(addr.addr, address->addr, FH_BLE_GAP_ADDR_LEN);
	addr.addr_type = (u8)address->addr_type;

#if defined(NRF51) || defined(SIM_ENABLED)
	u32 err = sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &addr);
#elif defined(NRF52)
	u32 err = sd_ble_gap_addr_set(&addr);
#endif
	logt("FH", "Gap Addr Set (%u)", err);

	return err;
}

u32 FruityHal::BleGapAddressGet(FruityHal::BleGapAddr *address)
{
	u32 err;
	ble_gap_addr_t p_addr;

#if defined(NRF51) || defined(SIM_ENABLED)
	err = sd_ble_gap_address_get(&p_addr);
#elif defined(NRF52)
	err = sd_ble_gap_addr_get(&p_addr);
#endif

	if (err == NRF_SUCCESS)
	{
		CheckedMemcpy(address->addr, p_addr.addr, FH_BLE_GAP_ADDR_LEN);
		address->addr_type = (BleGapAddrType)p_addr.addr_type;
	}

	logt("FH", "Gap Addr Get (%u)", err);

	return err;
}

ErrorType FruityHal::BleGapScanStart(BleGapScanParams const *scanParams)
{
	u32 err;
	ble_gap_scan_params_t scan_params;
	CheckedMemset(&scan_params, 0x00, sizeof(ble_gap_scan_params_t));

	scan_params.active = 0;
	scan_params.interval = scanParams->interval;
	scan_params.timeout = scanParams->timeout;
	scan_params.window = scanParams->window;

#if (SDK == 15)
	scan_params.report_incomplete_evts = 0;
	scan_params.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL;
	scan_params.extended = 0;
	scan_params.scan_phys = BLE_GAP_PHY_1MBPS;
	scan_params.timeout = scanParams->timeout * 100; //scanTimeout is now in ms since SDK15 instead of seconds
	CheckedMemset(scan_params.channel_mask, 0, sizeof(ble_gap_ch_mask_t));
#else
#if defined(NRF51) || defined(SIM_ENABLED)
	scan_params.p_whitelist = nullptr;
	scan_params.selective = 0;
#elif defined(NRF52)
	scan_params.adv_dir_report = 0;
	scan_params.use_whitelist = 0;
#endif
#endif

#if (SDK == 15)
	ble_data_t scan_data;
	scan_data.len = BLE_GAP_SCAN_BUFFER_MAX;
	scan_data.p_data = GS->scanBuffer;
	err = sd_ble_gap_scan_start(&scan_params, &scan_data);
#else
	err = sd_ble_gap_scan_start(&scan_params);
#endif
	logt("FH", "Scan start(%u) iv %u, w %u, t %u", err, scan_params.interval, scan_params.window, scan_params.timeout);
	return nrfErrToGeneric(err);
}

ErrorType FruityHal::BleGapScanStop()
{
	u32 err = sd_ble_gap_scan_stop();
	logt("FH", "Scan stop(%u)", err);
	return nrfErrToGeneric(err);
}

ErrorType FruityHal::BleGapAdvStart(u8 advHandle, BleGapAdvParams const *advParams)
{
	u32 err;
#if (SDK == 15)
	err = sd_ble_gap_adv_start(advHandle, BLE_CONN_CFG_TAG_FM);
	logt("FH", "Adv start (%u)", err);
#else
	ble_gap_adv_params_t adv_params;
	adv_params.channel_mask.ch_37_off = advParams->channelMask.ch37Off;
	adv_params.channel_mask.ch_38_off = advParams->channelMask.ch38Off;
	adv_params.channel_mask.ch_39_off = advParams->channelMask.ch39Off;
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
	logt("FH", "Adv start (%u) typ %u, iv %u, mask %u", err, adv_params.type, adv_params.interval, *((u8 *)&adv_params.channel_mask));
#endif
	return nrfErrToGeneric(err);
}

ErrorType FruityHal::BleGapAdvDataSet(u8 *p_advHandle, BleGapAdvParams const *p_advParams, u8 *advData, u8 advDataLength, u8 *scanData, u8 scanDataLength)
{
	u32 err = 0;
#if (SDK == 15)
	ble_gap_adv_params_t adv_params;
	ble_gap_adv_data_t adv_data;
	CheckedMemset(&adv_params, 0, sizeof(adv_params));
	CheckedMemset(&adv_data, 0, sizeof(adv_data));
	if (p_advParams != nullptr)
	{
		adv_params.channel_mask[4] |= (p_advParams->channelMask.ch37Off << 5);
		adv_params.channel_mask[4] |= (p_advParams->channelMask.ch38Off << 6);
		adv_params.channel_mask[4] |= (p_advParams->channelMask.ch39Off << 7);
		adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
		adv_params.interval = p_advParams->interval;
		adv_params.p_peer_addr = nullptr;
		adv_params.duration = p_advParams->timeout;
		adv_params.properties.type = (u8)p_advParams->type;
	}
	adv_data.adv_data.p_data = (u8 *)advData;
	adv_data.adv_data.len = advDataLength;
	adv_data.scan_rsp_data.p_data = (u8 *)scanData;
	adv_data.scan_rsp_data.len = scanDataLength;

	if (p_advParams != nullptr)
	{
		err = sd_ble_gap_adv_set_configure(
			p_advHandle,
			&adv_data,
			&adv_params);
	}
	else
	{
		err = sd_ble_gap_adv_set_configure(
			p_advHandle,
			&adv_data,
			nullptr);
	}

	logt("FH", "Adv data set (%u) typ %u, iv %lu, mask %u, handle %u", err, adv_params.properties.type, adv_params.interval, adv_params.channel_mask[4], *p_advHandle);
#else
	err = sd_ble_gap_adv_data_set(
		advData,
		advDataLength,
		scanData,
		scanDataLength);

	logt("FH", "Adv data set (%u)", err);
#endif
	return nrfErrToGeneric(err);
}

ErrorType FruityHal::BleGapAdvStop(u8 advHandle)
{
	u32 err;
#if (SDK == 15)
	err = sd_ble_gap_adv_stop(advHandle);
#else
	err = sd_ble_gap_adv_stop();
#endif
	logt("FH", "Adv stop (%u)", err);
	return nrfErrToGeneric(err);
}

ErrorType FruityHal::BleGapConnect(FruityHal::BleGapAddr const *peerAddress, BleGapScanParams const *scanParams, BleGapConnParams const *connectionParams)
{
	u32 err;
	ble_gap_addr_t p_peer_addr;
	CheckedMemset(&p_peer_addr, 0x00, sizeof(p_peer_addr));
	p_peer_addr.addr_type = (u8)peerAddress->addr_type;
	CheckedMemcpy(p_peer_addr.addr, peerAddress->addr, sizeof(peerAddress->addr));

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
	p_conn_params.conn_sup_timeout = connectionParams->connSupTimeout;
	p_conn_params.max_conn_interval = connectionParams->maxConnInterval;
	p_conn_params.min_conn_interval = connectionParams->minConnInterval;
	p_conn_params.slave_latency = connectionParams->slaveLatency;

#if defined(NRF51) || defined(SIM_ENABLED)
	err = sd_ble_gap_connect(&p_peer_addr, &p_scan_params, &p_conn_params);
#elif defined(NRF52)
	err = sd_ble_gap_connect(&p_peer_addr, &p_scan_params, &p_conn_params, BLE_CONN_CFG_TAG_FM);
#endif

	logt("FH", "Connect (%u) iv:%u, tmt:%u", err, p_conn_params.min_conn_interval, p_scan_params.timeout);

	//Tell our ScanController, that scanning has stopped
	GS->scanController.ScanningHasStopped();

	return nrfErrToGeneric(err);
}

u32 FruityHal::ConnectCancel()
{
	u32 err = sd_ble_gap_connect_cancel();

	logt("FH", "Connect Cancel (%u)", err);

	return err;
}

ErrorType FruityHal::Disconnect(u16 conn_handle, FruityHal::BleHciError hci_status_code)
{
	u32 err = sd_ble_gap_disconnect(conn_handle, (u8)hci_status_code);

	logt("FH", "Disconnect (%u)", err);

	return nrfErrToGeneric(err);
}

ErrorType FruityHal::BleTxPacketCountGet(u16 connectionHandle, u8 *count)
{
#if defined(NRF51) || defined(SIM_ENABLED)
	return nrfErrToGeneric(sd_ble_tx_packet_count_get(connectionHandle, count));
#elif defined(NRF52)
	//TODO: must be read from somewhere else
	*count = BLE_CONN_CFG_GAP_PACKET_BUFFERS;
	return ErrorType::SUCCESS;
#endif
}

u32 FruityHal::BleGapNameSet(BleGapConnSecMode &mode, u8 const *p_dev_name, u16 len)
{
	ble_gap_conn_sec_mode_t sec_mode;
	CheckedMemset(&sec_mode, 0, sizeof(sec_mode));
	sec_mode.sm = mode.securityMode;
	sec_mode.lv = mode.level;
	return sd_ble_gap_device_name_set(&sec_mode, p_dev_name, len);
}

u32 FruityHal::BleGapAppearance(BleAppearance appearance)
{
	return sd_ble_gap_appearance_set((u32)appearance);
}

ble_gap_conn_params_t translate(FruityHal::BleGapConnParams const &params)
{
	ble_gap_conn_params_t gapConnectionParams;
	CheckedMemset(&gapConnectionParams, 0, sizeof(gapConnectionParams));

	gapConnectionParams.min_conn_interval = params.minConnInterval;
	gapConnectionParams.max_conn_interval = params.maxConnInterval;
	gapConnectionParams.slave_latency = params.slaveLatency;
	gapConnectionParams.conn_sup_timeout = params.connSupTimeout;
	return gapConnectionParams;
}

u32 FruityHal::BleGapConnectionParamsUpdate(u16 conn_handle, BleGapConnParams const &params)
{
	ble_gap_conn_params_t gapConnectionParams = translate(params);
	return sd_ble_gap_conn_param_update(conn_handle, &gapConnectionParams);
}

u32 FruityHal::BleGapConnectionPreferredParamsSet(BleGapConnParams const &params)
{
	ble_gap_conn_params_t gapConnectionParams = translate(params);
	return sd_ble_gap_ppcp_set(&gapConnectionParams);
}

u32 FruityHal::BleGapSecInfoReply(u16 conn_handle, BleGapEncInfo *p_info, u8 *p_id_info, u8 *p_sign_info)
{
	ble_gap_enc_info_t info;
	CheckedMemset(&info, 0, sizeof(info));
	CheckedMemcpy(info.ltk, p_info->longTermKey, p_info->longTermKeyLength);
	info.lesc = p_info->isGeneratedUsingLeSecureConnections;
	info.auth = p_info->isAuthenticatedKey;
	info.ltk_len = p_info->longTermKeyLength;

	return sd_ble_gap_sec_info_reply(
		conn_handle,
		&info,   //This is our stored long term key
		nullptr, //We do not have an identity resolving key
		nullptr  //We do not have signing info
	);
}

u32 FruityHal::BleGapEncrypt(u16 conn_handle, BleGapMasterId const &master_id, BleGapEncInfo const &enc_info)
{
	ble_gap_master_id_t keyId;
	CheckedMemset(&keyId, 0, sizeof(keyId));
	keyId.ediv = master_id.encryptionDiversifier;
	CheckedMemcpy(keyId.rand, master_id.rand, BLE_GAP_SEC_RAND_LEN);

	ble_gap_enc_info_t info;
	CheckedMemset(&info, 0, sizeof(info));
	CheckedMemcpy(info.ltk, enc_info.longTermKey, enc_info.longTermKeyLength);
	info.lesc = enc_info.isGeneratedUsingLeSecureConnections;
	info.auth = enc_info.isAuthenticatedKey;
	info.ltk_len = enc_info.longTermKeyLength;

	return sd_ble_gap_encrypt(conn_handle, &keyId, &info);
}

u32 FruityHal::BleGapRssiStart(u16 conn_handle, u8 threshold_dbm, u8 skip_count)
{
	return sd_ble_gap_rssi_start(conn_handle, threshold_dbm, skip_count);
}

u32 FruityHal::BleGapRssiStop(u16 conn_handle)
{
	return sd_ble_gap_rssi_stop(conn_handle);
}

u32 FruityHal::DiscovereServiceInit(DBDiscoveryHandler dbEventHandler)
{
#ifndef SIM_ENABLED
	return ble_db_discovery_init(dbEventHandler);
#else // ! SIM_ENABLED
	GS->dbDiscoveryHandler = dbEventHandler;
#endif
	return 0;
}

u32 FruityHal::DiscoverService(u16 connHandle, const BleGattUuid &p_uuid, ble_db_discovery_t *p_discoveredServices)
{
	uint32_t err = 0;
	ble_uuid_t uuid;
	CheckedMemset(&uuid, 0, sizeof(uuid));
	uuid.uuid = p_uuid.uuid;
	uuid.type = p_uuid.type;
#ifndef SIM_ENABLED
	CheckedMemset(p_discoveredServices, 0x00, sizeof(*p_discoveredServices));
	err = ble_db_discovery_evt_register(&uuid);
	if (err)
	{
		logt("ERROR", "err %u", (u32)err);
		return err;
	}

	err = ble_db_discovery_start(p_discoveredServices, connHandle);
	if (err)
	{
		logt("ERROR", "err %u", (u32)err);
		return err;
	}
#else
	cherrySimInstance->StartServiceDiscovery(connHandle, uuid, 1000);
#endif
	return err;
}

u32 FruityHal::BleGattSendNotification(u16 connHandle, BleGattWriteParams &params)
{
	ble_gatts_hvx_params_t notificationParams;
	CheckedMemset(&notificationParams, 0, sizeof(ble_gatts_hvx_params_t));
	notificationParams.handle = params.handle;
	notificationParams.offset = params.offset;
	notificationParams.p_data = params.p_data;
	notificationParams.p_len = &params.len;

	if (params.type == BleGattWriteType::NOTIFICATION)
		notificationParams.type = BLE_GATT_HVX_NOTIFICATION;
	else if (params.type == BleGattWriteType::INDICATION)
		notificationParams.type = BLE_GATT_HVX_INDICATION;
	else
		return NRF_ERROR_INVALID_PARAM;

	return sd_ble_gatts_hvx(connHandle, &notificationParams);
}

u32 FruityHal::BleGattWrite(u16 connHandle, BleGattWriteParams const &params)
{
	ble_gattc_write_params_t writeParameters;
	CheckedMemset(&writeParameters, 0, sizeof(ble_gattc_write_params_t));
	writeParameters.handle = params.handle;
	writeParameters.offset = params.offset;
	writeParameters.len = params.len;
	writeParameters.p_value = params.p_data;

	if (params.type == BleGattWriteType::WRITE_REQ)
		writeParameters.write_op = BLE_GATT_OP_WRITE_REQ;
	else if (params.type == BleGattWriteType::WRITE_CMD)
		writeParameters.write_op = BLE_GATT_OP_WRITE_CMD;
	else
		return NRF_ERROR_INVALID_PARAM;

	return sd_ble_gattc_write(connHandle, &writeParameters);
}

u32 FruityHal::BleUuidVsAdd(u8 const *p_vs_uuid, u8 *p_uuid_type)
{
	ble_uuid128_t vs_uuid;
	CheckedMemset(&vs_uuid, 0, sizeof(vs_uuid));
	CheckedMemcpy(vs_uuid.uuid128, p_vs_uuid, sizeof(vs_uuid));
	return sd_ble_uuid_vs_add(&vs_uuid, p_uuid_type);
}

u32 FruityHal::BleGattServiceAdd(BleGattSrvcType type, BleGattUuid const &p_uuid, u16 *p_handle)
{
	ble_uuid_t uuid;
	CheckedMemset(&uuid, 0, sizeof(uuid));
	uuid.uuid = p_uuid.uuid;
	uuid.type = p_uuid.type;
	return sd_ble_gatts_service_add((u8)type, &uuid, p_handle);
}

u32 FruityHal::BleGattCharAdd(u16 service_handle, BleGattCharMd const &char_md, BleGattAttribute const &attr_char_value, BleGattCharHandles &handles)
{
	ble_gatts_char_md_t sd_char_md;
	ble_gatts_attr_t sd_attr_char_value;

	static_assert(SDK <= 15, "Check mapping");

	CheckedMemcpy(&sd_char_md, &char_md, sizeof(ble_gatts_char_md_t));
	CheckedMemcpy(&sd_attr_char_value, &attr_char_value, sizeof(ble_gatts_attr_t));
	return sd_ble_gatts_characteristic_add(service_handle, &sd_char_md, &sd_attr_char_value, (ble_gatts_char_handles_t *)&handles);
}

u32 FruityHal::BleGapDataLengthExtensionRequest(u16 connHandle)
{
#ifdef NRF52
	//We let the SoftDevice decide the maximum according to the MAX_MTU_SIZE and connection configuration
	u32 err = sd_ble_gap_data_length_update(connHandle, nullptr, nullptr);
	logt("FH", "Start DLE Update (%u) on conn %u", err, connHandle);

	return err;
#else
	//TODO: We should implement DLE in the Simulator as soon as it is based on the NRF52

	return NRF_ERROR_NOT_SUPPORTED;
#endif
}

u32 FruityHal::BleGattMtuExchangeRequest(u16 connHandle, u16 clientRxMtu)
{
#ifdef NRF52
	u32 err = sd_ble_gattc_exchange_mtu_request(connHandle, clientRxMtu);

	logt("FH", "Start MTU Exchange (%u) on conn %u with %u", err, connHandle, clientRxMtu);

	return err;
#else
	//TODO: We should implement MTU Exchange in the Simulator as soon as it is based on the NRF52

	return NRF_ERROR_NOT_SUPPORTED;
#endif
}

u32 FruityHal::BleGattMtuExchangeReply(u16 connHandle, u16 clientRxMtu)
{
#ifdef NRF52
	u32 err = sd_ble_gatts_exchange_mtu_reply(connHandle, clientRxMtu);

	logt("ERROR", "Reply MTU Exchange (%u) on conn %u with %u", err, connHandle, clientRxMtu);

	return err;
#else
	return NRF_ERROR_NOT_SUPPORTED;
#endif
}

// Supported tx_power values for this implementation: -40dBm, -20dBm, -16dBm, -12dBm, -8dBm, -4dBm, 0dBm, +3dBm and +4dBm.
ErrorType FruityHal::RadioSetTxPower(i8 tx_power, TxRole role, u16 handle)
{
	if (tx_power != -40 &&
		tx_power != -30 &&
		tx_power != -20 &&
		tx_power != -16 &&
		tx_power != -12 &&
		tx_power != -8 &&
		tx_power != -4 &&
		tx_power != 0 &&
		tx_power != 4)
	{
		SIMEXCEPTION(IllegalArgumentException);
		return ErrorType::INVALID_PARAM;
	}

	u32 err;
#if (SDK == 15)
	u8 txRole;
	if (role == TxRole::CONNECTION)
		txRole = BLE_GAP_TX_POWER_ROLE_CONN;
	else if (role == TxRole::ADVERTISING)
		txRole = BLE_GAP_TX_POWER_ROLE_ADV;
	else if (role == TxRole::SCAN_INIT)
		txRole = BLE_GAP_TX_POWER_ROLE_SCAN_INIT;
	else
		return ErrorType::INVALID_PARAM;
	;
	err = sd_ble_gap_tx_power_set(txRole, handle, tx_power);
#else
	err = sd_ble_gap_tx_power_set(tx_power);
#endif

	return nrfErrToGeneric(err);
}

//################################################
#define _________________BUTTONS__________________

#ifndef SIM_ENABLED
void button_interrupt_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
	// GS->ledGreen.Toggle();

	// Because we don't know which state the button is in, we have to read it
	u32 state = nrf_gpio_pin_read(pin);

	if (pin == (u8)Boardconfig->button1Pin)
	{
		if (state == Boardconfig->buttonsActiveHigh)
		{
			GS->button1PressTimeDs = GS->appTimerDs;
			GS->button1State = ButtonState::PRESSED;
		}
		else
		{
			GS->button1HoldTimeDs = GS->appTimerDs - GS->button1PressTimeDs;
			GS->button1State = ButtonState::RELEASED;
		}
	}
}
#endif

ErrorType FruityHal::WaitForEvent()
{
	return nrfErrToGeneric(sd_app_evt_wait());
}

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
	err = nrf_drv_gpiote_in_init(Boardconfig->button1Pin, &buttonConfig, button_interrupt_handler);

	//Enable the events
	nrf_drv_gpiote_in_event_enable(Boardconfig->button1Pin, true);
#endif

	return err;
}

ErrorType FruityHal::GetRandomBytes(u8 *p_data, u8 len)
{
	return nrfErrToGeneric(sd_rand_application_vector_get(p_data, len));
}

u32 FruityHal::ClearGeneralPurposeRegister(u32 gpregId, u32 mask)
{
#ifdef NRF52
	return sd_power_gpregret_clr(gpregId, mask);
#elif NRF51
	return sd_power_gpregret_clr(gpregId);
#else
	return NRF_SUCCESS;
#endif
}

u32 FruityHal::WriteGeneralPurposeRegister(u32 gpregId, u32 mask)
{
#ifdef NRF52
	return sd_power_gpregret_set(gpregId, mask);
#elif NRF51
	return sd_power_gpregret_set(gpregId);
#else
	return NRF_SUCCESS;
#endif
}

//################################################
#define _________________UART_____________________

//This handler receives UART interrupts (terminal json mode)
#if !defined(UART_ENABLED) || UART_ENABLED == 0 //Only enable if nordic library for UART is not used
extern "C"
{
	void UART0_IRQHandler(void)
	{
		if (GS->uartEventHandler == nullptr)
		{
			SIMEXCEPTION(UartNotSetException);
		}
		else
		{
			GS->uartEventHandler();
		}
	}
}
#endif

//################################################
#define _________________TIMERS___________________

extern "C"
{
	static const u32 TICKS_PER_DS_TIMES_TEN = 32768;

	void app_timer_handler(void *p_context)
	{
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
	SIMEXCEPTION(NotImplementedException);
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
	SIMEXCEPTION(NotImplementedException);
	u32 err = 0;
#ifndef SIM_ENABLED

	APP_TIMER_DEF(mainTimerMsId);

	err = app_timer_create(&mainTimerMsId, APP_TIMER_MODE_REPEATED, app_timer_handler);
	if (err != NRF_SUCCESS)
		return err;

	err = app_timer_start(mainTimerMsId, MAIN_TIMER_TICK, nullptr);
#endif // SIM_ENABLED
	return err;
}

ErrorType FruityHal::CreateTimer(FruityHal::swTimer &timer, bool repeated, TimerHandler handler)
{
	SIMEXCEPTION(NotImplementedException);
	u32 err;
#ifndef SIM_ENABLED
	static u8 timersCreated = 0;

	timer = (u32 *)&swTimers[timersCreated];
	CheckedMemset(timer, 0x00, sizeof(app_timer_t));

	app_timer_mode_t mode = repeated ? APP_TIMER_MODE_REPEATED : APP_TIMER_MODE_SINGLE_SHOT;

	err = app_timer_create((app_timer_id_t *)(&timer), mode, handler);
	if (err != NRF_SUCCESS)
		return nrfErrToGeneric(err);

	timersCreated++;
#endif
	return ErrorType::SUCCESS;
}

ErrorType FruityHal::StartTimer(FruityHal::swTimer timer, u32 timeoutMs)
{
	SIMEXCEPTION(NotImplementedException);
#ifndef SIM_ENABLED
	if (timer == nullptr)
		return ErrorType::INVALID_PARAM;

#ifdef NRF51
	u32 err = app_timer_start((app_timer_id_t)timer, APP_TIMER_TICKS(timeoutMs, APP_TIMER_PRESCALER), NULL);
#else
	// APP_TIMER_TICKS number of parameters has changed from SDK 11 to SDK 14
	// cppcheck-suppress preprocessorErrorDirective
	u32 err = app_timer_start((app_timer_id_t)timer, APP_TIMER_TICKS(timeoutMs), NULL);
#endif
	return nrfErrToGeneric(err);
#else
	return ErrorType::SUCCESS;
#endif
}

ErrorType FruityHal::StopTimer(FruityHal::swTimer timer)
{
	SIMEXCEPTION(NotImplementedException);
#ifndef SIM_ENABLED
	if (timer == nullptr)
		return ErrorType::INVALID_PARAM;

	u32 err = app_timer_stop((app_timer_id_t)timer);
	return nrfErrToGeneric(err);
#else
	return ErrorType::SUCCESS;
#endif
}

u32 FruityHal::GetRtcMs()
{
	uint32_t rtcTicks;
#if defined(NRF51) || defined(SIM_ENABLED)
	app_timer_cnt_get(&rtcTicks);
#elif defined(NRF52)
	rtcTicks = app_timer_cnt_get();
#endif
	return rtcTicks * 1000 / APP_TIMER_CLOCK_FREQ;
}

// In this port limitation is that max diff between nowTimeMs and previousTimeMs
// can be 0xFFFFFE measured in app_timer ticks. If the difference is bigger the
// result will be faulty
u32 FruityHal::GetRtcDifferenceMs(u32 nowTimeMs, u32 previousTimeMs)
{
	u32 nowTimeTicks = nowTimeMs * APP_TIMER_CLOCK_FREQ / 1000;
	u32 previousTimeTicks = previousTimeMs * APP_TIMER_CLOCK_FREQ / 1000;
	uint32_t diffTicks;
#if defined(NRF51) || defined(SIM_ENABLED)
	app_timer_cnt_diff_compute(nowTimeTicks, previousTimeTicks, &diffTicks);
#elif defined(NRF52)
	diffTicks = app_timer_cnt_diff_compute(nowTimeTicks, previousTimeTicks);
#endif
	return diffTicks * 1000 / APP_TIMER_CLOCK_FREQ;
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
	void app_error_handler(uint32_t error_code, uint32_t line_num, const u8 *p_file_name)
	{
		app_error_handler_bare(error_code);
		logt("ERROR", "App error code:%s(%u), file:%s, line:%u", Logger::getGeneralErrorString((ErrorType)error_code), (u32)error_code, p_file_name, (u32)line_num);
	}

	//Called when the softdevice crashes
	void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
	{
		GS->stackErrorHandler(id, pc, info);
	}

#ifndef SIM_ENABLED
	//We use the nordic hardfault handler that stacks all fault variables for us before calling this function
	__attribute__((used)) void HardFault_c_handler(stacked_regs_t *stack)
	{
		GS->hardfaultHandler(stack);
	}
#endif

	//NRF52 uses more handlers, we currently just reboot if they are called
	//TODO: Redirect to hardfault handler, but be aware that the stack will shift by calling the function
#ifdef NRF52
	__attribute__((used)) void MemoryManagement_Handler()
	{
		GS->ramRetainStructPtr->rebootReason = RebootReason::MEMORY_MANAGEMENT;
		NVIC_SystemReset();
	}
	__attribute__((used)) void BusFault_Handler()
	{
		GS->ramRetainStructPtr->rebootReason = RebootReason::BUS_FAULT;
		NVIC_SystemReset();
	}
	__attribute__((used)) void UsageFault_Handler()
	{
		GS->ramRetainStructPtr->rebootReason = RebootReason::USAGE_FAULT;
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

	if (reason & POWER_RESETREAS_DOG_Msk)
	{
		return RebootReason::WATCHDOG;
	}
	else if (reason & POWER_RESETREAS_RESETPIN_Msk)
	{
		return RebootReason::PIN_RESET;
	}
	else if (reason & POWER_RESETREAS_OFF_Msk)
	{
		return RebootReason::FROM_OFF_STATE;
	}
	else
	{
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
	if (!safeBoot)
	{
		nrf_wdt_reload_value_set(FM_WATCHDOG_TIMEOUT);
	}
	else
	{
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
	if (BOOTLOADER_UICR_ADDRESS != 0xFFFFFFFF)
	{
		return *(u32 *)(BOOTLOADER_UICR_ADDRESS + 1024);
	}
	else
	{
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
	while (delayMs != 0)
	{
		delayMs--;
		nrf_delay_us(999);
	}
#endif
}

u32 FruityHal::EcbEncryptBlock(const u8 *p_key, const u8 *p_clearText, u8 *p_cipherText)
{
	u32 error;
	nrf_ecb_hal_data_t ecbData;
	CheckedMemset(&ecbData, 0x00, sizeof(ecbData));
	CheckedMemcpy(ecbData.key, p_key, SOC_ECB_KEY_LENGTH);
	CheckedMemcpy(ecbData.cleartext, p_clearText, SOC_ECB_CLEARTEXT_LENGTH);
	error = sd_ecb_block_encrypt(&ecbData);
	CheckedMemcpy(p_cipherText, ecbData.ciphertext, SOC_ECB_CIPHERTEXT_LENGTH);
	return error;
}

ErrorType FruityHal::FlashPageErase(u32 page)
{
	return nrfErrToGeneric(sd_flash_page_erase(page));
}

ErrorType FruityHal::FlashWrite(u32 *p_addr, u32 *p_data, u32 len)
{
	return nrfErrToGeneric(sd_flash_write((uint32_t *)p_addr, (uint32_t *)p_data, len));
}

void FruityHal::nvicEnableIRQ(u32 irqType)
{
#ifndef SIM_ENABLED
	sd_nvic_EnableIRQ((IRQn_Type)irqType);
#endif
}

void FruityHal::nvicDisableIRQ(u32 irqType)
{
#ifndef SIM_ENABLED
	sd_nvic_DisableIRQ((IRQn_Type)irqType);
#endif
}

void FruityHal::nvicSetPriorityIRQ(u32 irqType, u8 level)
{
#ifndef SIM_ENABLED
	sd_nvic_SetPriority((IRQn_Type)irqType, (uint32_t)level);
#endif
}

void FruityHal::nvicClearPendingIRQ(u32 irqType)
{
#ifndef SIM_ENABLED
	sd_nvic_ClearPendingIRQ((IRQn_Type)irqType);
#endif
}

#ifndef SIM_ENABLED
extern "C"
{
	//Eliminate Exception overhead when using pure virutal functions
	//http://elegantinvention.com/blog/information/smaller-binary-size-with-c-on-baremetal-g/
	void __cxa_pure_virtual()
	{
		// Must never be called.
		logt("ERROR", "PVF call");
		constexpr u32 pureVirtualFunctionCalledError = 0xF002;
		APP_ERROR_CHECK(pureVirtualFunctionCalledError);
	}
}
#endif

#define __________________CONVERT____________________

ble_gap_addr_t FruityHal::Convert(const FruityHal::BleGapAddr *address)
{
	ble_gap_addr_t addr;
	CheckedMemset(&addr, 0x00, sizeof(addr));
	CheckedMemcpy(addr.addr, address->addr, FH_BLE_GAP_ADDR_LEN);
	addr.addr_type = (u8)address->addr_type;
#ifdef NRF52
	addr.addr_id_peer = 0;
#endif
	return addr;
}
FruityHal::BleGapAddr FruityHal::Convert(const ble_gap_addr_t *p_addr)
{
	FruityHal::BleGapAddr address;
	CheckedMemset(&address, 0x00, sizeof(address));
	CheckedMemcpy(address.addr, p_addr->addr, FH_BLE_GAP_ADDR_LEN);
	address.addr_type = (BleGapAddrType)p_addr->addr_type;

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
	if (bootloaderAddress != 0xFFFFFFFFUL)
	{
		u32 *magicNumberAddress = (u32 *)NORDIC_DFU_MAGIC_NUMBER_ADDRESS;
		//Check if the magic number is currently set to enable nordic dfu
		if (*magicNumberAddress == ENABLE_NORDIC_DFU_MAGIC_NUMBER)
		{
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
	//Even though the soft device size is not strictly dependent on the chipset, it is a good approximation.
	//These values were measured on real hardware on 26.09.2019.
	switch (GET_CHIPSET())
	{
	case Chipset::CHIP_NRF51:
		return 110592;
	case Chipset::CHIP_NRF52:
	case Chipset::CHIP_NRF52840:
		return 143360;
	default:
		SIMEXCEPTION(IllegalStateException);
	}
	return 0;
#else
	return SD_SIZE_GET(MBR_SIZE);
#endif
}

u32 FruityHal::GetSoftDeviceVersion()
{
#ifdef SIM_ENABLED
	switch (GetBleStackType())
	{
	case BleStackType::NRF_SD_130_ANY:
		return 0x0087;
	case BleStackType::NRF_SD_132_ANY:
		return 0x00A8;
	case BleStackType::NRF_SD_140_ANY:
		return 0x00A9;
	default:
		SIMEXCEPTION(IllegalStateException);
	}
	return 0;
#else
	return SD_FWID_GET(MBR_SIZE);
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
	switch (id)
	{
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
		u8 len = (u8)strlen((const char *)((assert_info_t *)info)->p_file_name);
		if (len > (RAM_PERSIST_STACKSTRACE_SIZE - 1) * 4)
			len = (RAM_PERSIST_STACKSTRACE_SIZE - 1) * 4;
		CheckedMemcpy(GS->ramRetainStructPtr->stacktrace + 1, ((assert_info_t *)info)->p_file_name, len);
		break;
	}
	case NRF_FAULT_ID_SDK_ERROR: //SDK errors
	{
		GS->ramRetainStructPtr->code2 = ((error_info_t *)info)->line_num;
		GS->ramRetainStructPtr->code3 = ((error_info_t *)info)->err_code;

		//Copy filename to stacktrace
		u8 len = (u8)strlen((const char *)((error_info_t *)info)->p_file_name);
		if (len > (RAM_PERSIST_STACKSTRACE_SIZE - 1) * 4)
			len = (RAM_PERSIST_STACKSTRACE_SIZE - 1) * 4;
		CheckedMemcpy(GS->ramRetainStructPtr->stacktrace + 1, ((error_info_t *)info)->p_file_name, len);
		break;
	}
	}
}

const char *FruityHal::getBleEventNameString(u16 bleEventId)
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
	case BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE:
		return "BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE";
	case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
		return "BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST";
	case BLE_GAP_EVT_DATA_LENGTH_UPDATE:
		return "BLE_GAP_EVT_DATA_LENGTH_UPDATE";
	case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
		return "BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST";
	case BLE_GATTC_EVT_EXCHANGE_MTU_RSP:
		return "BLE_GATTC_EVT_EXCHANGE_MTU_RSP";
	case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
		return "BLE_GAP_EVT_PHY_UPDATE_REQUEST";
#endif
	default:
		SIMEXCEPTION(ErrorCodeUnknownException); //Could be an error or should be added to the list
		return "UNKNOWN_EVENT";
	}
#else
	return nullptr;
#endif
}

u32 *FruityHal::getUicrDataPtr()
{
	//We are using a magic number to determine if the UICR data present was put there by fruitydeploy
	if (NRF_UICR->CUSTOMER[0] == UICR_SETTINGS_MAGIC_WORD)
	{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
		// The following casts away volatile, which is intended behavior and is okay, as the CUSTOMER data won't change during runtime.
		return (u32 *)NRF_UICR->CUSTOMER;
#pragma GCC diagnostic pop
	}
	else if (GS->recordStorage.IsInit())
	{
		//On some devices, we are not able to store data in UICR as they are flashed by a 3rd party
		//and we are only updating to fruitymesh. We have a dedicated record for these instances
		//which is used the same as if the data were stored in UICR
		SizedData data = GS->recordStorage.GetRecordData(RECORD_STORAGE_RECORD_ID_UICR_REPLACEMENT);
		if (data.length >= 16 * 4 && ((u32 *)data.data)[0] == UICR_SETTINGS_MAGIC_WORD)
		{
			return (u32 *)data.data;
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

	if (Boardconfig->uartRTSPin != -1)
	{
		if (NRF_UART0->PSELRTS != NRF_UART_PSEL_DISCONNECTED)
			nrf_gpio_cfg_default(Boardconfig->uartRTSPin);
		if (NRF_UART0->PSELCTS != NRF_UART_PSEL_DISCONNECTED)
			nrf_gpio_cfg_default(Boardconfig->uartCTSPin);
	}
#endif
}

void FruityHal::UartHandleError(u32 error)
{
	//Errorsource is given, but has to be cleared to be handled
	NRF_UART0->ERRORSRC = error;

	//FIXME: maybe we need some better error handling here
}

bool FruityHal::UartCheckInputAvailable()
{
	return NRF_UART0->EVENTS_RXDRDY == 1;
}

FruityHal::UartReadCharBlockingResult FruityHal::UartReadCharBlocking()
{
	UartReadCharBlockingResult retVal;

#if IS_INACTIVE(GW_SAVE_SPACE)
	while (NRF_UART0->EVENTS_RXDRDY != 1)
	{
		if (NRF_UART0->EVENTS_ERROR)
		{
			FruityHal::UartHandleError(NRF_UART0->ERRORSRC);
			retVal.didError = true;
		}
		// Info: No timeout neede here, as we are waiting for user input
	}
	NRF_UART0->EVENTS_RXDRDY = 0;
	retVal.c = NRF_UART0->RXD;
#endif

	return retVal;
}

void FruityHal::UartPutStringBlockingWithTimeout(const char *message)
{
	uint_fast8_t i = 0;
	uint8_t byte = message[i++];

	while (byte != '\0')
	{
		NRF_UART0->TXD = byte;
		byte = message[i++];

		int i = 0;
		while (NRF_UART0->EVENTS_TXDRDY != 1)
		{
			//Timeout if it was not possible to put the character
			if (i > 10000)
			{
				return;
			}
			i++;
			//FIXME: Do we need error handling here? Will cause lost characters
		}
		NRF_UART0->EVENTS_TXDRDY = 0;
	}
}

bool FruityHal::IsUartErroredAndClear()
{
	if (nrf_uart_int_enable_check(NRF_UART0, NRF_UART_INT_MASK_ERROR) &&
		nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_ERROR))
	{
		nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_ERROR);

		FruityHal::UartHandleError(NRF_UART0->ERRORSRC);

		return true;
	}
	return false;
}

bool FruityHal::IsUartTimedOutAndClear()
{
	if (nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_RXTO))
	{
		nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXTO);

		//Restart transmission and clear previous buffer
		nrf_uart_task_trigger(NRF_UART0, NRF_UART_TASK_STARTRX);

		return true;

		//TODO: can we check if this works???
	}
	return false;
}

FruityHal::UartReadCharResult FruityHal::UartReadChar()
{
	UartReadCharResult retVal;

	if (nrf_uart_int_enable_check(NRF_UART0, NRF_UART_INT_MASK_RXDRDY) &&
		nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_RXDRDY))
	{
		//Reads the byte
		nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXDRDY);
#ifndef SIM_ENABLED
		retVal.c = NRF_UART0->RXD;
#else
		retVal.c = nrf_uart_rxd_get(NRF_UART0);
#endif
		retVal.hasNewChar = true;

		//Disable the interrupt to stop receiving until instructed further
		nrf_uart_int_disable(NRF_UART0, NRF_UART_INT_MASK_RXDRDY | NRF_UART_INT_MASK_ERROR);
	}

	return retVal;
}

void FruityHal::UartEnableReadInterrupt()
{
	nrf_uart_int_enable(NRF_UART0, NRF_UART_INT_MASK_RXDRDY | NRF_UART_INT_MASK_ERROR);
}

void FruityHal::EnableUart(bool promptAndEchoMode)
{
	//Configure pins
	nrf_gpio_pin_set(Boardconfig->uartTXPin);
	nrf_gpio_cfg_output(Boardconfig->uartTXPin);
	nrf_gpio_cfg_input(Boardconfig->uartRXPin, NRF_GPIO_PIN_NOPULL);

	nrf_uart_baudrate_set(NRF_UART0, (nrf_uart_baudrate_t)Boardconfig->uartBaudRate);
	nrf_uart_configure(NRF_UART0, NRF_UART_PARITY_EXCLUDED, Boardconfig->uartRTSPin != -1 ? NRF_UART_HWFC_ENABLED : NRF_UART_HWFC_DISABLED);
	nrf_uart_txrx_pins_set(NRF_UART0, Boardconfig->uartTXPin, Boardconfig->uartRXPin);

	//Configure RTS/CTS (if RTS is -1, disable flow control)
	if (Boardconfig->uartRTSPin != -1)
	{
		nrf_gpio_cfg_input(Boardconfig->uartCTSPin, NRF_GPIO_PIN_NOPULL);
		nrf_gpio_pin_set(Boardconfig->uartRTSPin);
		nrf_gpio_cfg_output(Boardconfig->uartRTSPin);
		nrf_uart_hwfc_pins_set(NRF_UART0, Boardconfig->uartRTSPin, Boardconfig->uartCTSPin);
	}

	if (!promptAndEchoMode)
	{
		//Enable Interrupts + timeout events
		nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXTO);
		nrf_uart_int_enable(NRF_UART0, NRF_UART_INT_MASK_RXTO);

		sd_nvic_SetPriority(UART0_IRQn, APP_IRQ_PRIORITY_LOW);
		sd_nvic_ClearPendingIRQ(UART0_IRQn);
		sd_nvic_EnableIRQ(UART0_IRQn);
	}

	//Enable UART
	nrf_uart_enable(NRF_UART0);

	//Enable Receiver
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_ERROR);
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXDRDY);
	nrf_uart_task_trigger(NRF_UART0, NRF_UART_TASK_STARTRX);

	//Enable Transmitter
	nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_TXDRDY);
	nrf_uart_task_trigger(NRF_UART0, NRF_UART_TASK_STARTTX);

	if (!promptAndEchoMode)
	{
		//Start receiving RX events
		FruityHal::enableUartReadInterrupt();
	}
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

// We only need twi and spi for asset and there is no target for nrf51
#if !defined(NRF51) && defined(ACTIVATE_ASSET_MODULE)
#ifndef SIM_ENABLED
#define TWI_INSTANCE_ID 1
static const nrf_drv_twi_t twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);
static volatile bool twiXferDone = false;
static bool twiInitDone = false;
#endif

#ifndef SIM_ENABLED
static void twi_handler(nrf_drv_twi_evt_t const *pEvent, void *pContext)
{
	switch (pEvent->type)
	{
	// Transfer completed event.
	case NRF_DRV_TWI_EVT_DONE:
		twiXferDone = true;
		break;

	// NACK received after sending the address
	case NRF_DRV_TWI_EVT_ADDRESS_NACK:
		break;

	// NACK received after sending a data byte.
	case NRF_DRV_TWI_EVT_DATA_NACK:
		break;

	default:
		break;
	}
}
#endif

ErrorType FruityHal::twi_init(u32 sclPin, u32 sdaPin)
{
	u32 errCode = NRF_SUCCESS;
#ifndef SIM_ENABLED
	// twi.reg          = {NRF_DRV_TWI_PERIPHERAL(TWI_INSTANCE_ID)};
	// twi.drv_inst_idx = CONCAT_3(TWI, TWI_INSTANCE_ID, _INSTANCE_INDEX);
	// twi.use_easy_dma = TWI_USE_EASY_DMA(TWI_INSTANCE_ID);

	const nrf_drv_twi_config_t twiConfig = {
		.scl = sclPin,
		.sda = sdaPin,
#if SDK == 15
		.frequency = NRF_DRV_TWI_FREQ_250K,
#else
		.frequency = NRF_TWI_FREQ_250K,
#endif
		.interrupt_priority = APP_IRQ_PRIORITY_HIGH,
		.clear_bus_init = false,
		.hold_bus_uninit = false
	};

	errCode = nrf_drv_twi_init(&twi, &twiConfig, twi_handler, NULL);
	if (errCode != NRF_ERROR_INVALID_STATE && errCode != NRF_SUCCESS)
	{
		APP_ERROR_CHECK(errCode);
	}

	nrf_drv_twi_enable(&twi);

	twiInitDone = true;
#else
	errCode = cherrySimInstance->currentNode->twiWasInit ? NRF_ERROR_INVALID_STATE : NRF_SUCCESS;
	if (cherrySimInstance->currentNode->twiWasInit)
	{
		//Was already initialized!
		SIMEXCEPTION(IllegalStateException);
	}
	cherrySimInstance->currentNode->twiWasInit = true;
#endif
	return nrfErrToGeneric(errCode);
}

void FruityHal::twi_uninit(u32 sclPin, u32 sdaPin)
{
#ifndef SIM_ENABLED
	nrf_drv_twi_disable(&twi);
	nrf_drv_twi_uninit(&twi);
	nrf_gpio_cfg_default(sclPin);
	nrf_gpio_cfg_default(sdaPin);
	NRF_TWI1->ENABLE = 0;

	twiInitDone = false;
#endif
}

void FruityHal::twi_gpio_address_pin_set_and_wait(bool high, u8 sdaPin)
{
#ifndef SIM_ENABLED
	nrf_gpio_cfg_output(sdaPin);
	if (high)
	{
		nrf_gpio_pin_set(sdaPin);
		nrf_delay_us(200);
	}
	else
	{
		nrf_gpio_pin_clear(sdaPin);
		nrf_delay_us(200);
	}

	nrf_gpio_pin_set(sdaPin);
#endif
}

ErrorType FruityHal::twi_registerWrite(u8 slaveAddress, u8 const *pTransferData, u8 length)
{
	// Slave Address (Command) (7 Bit) + WriteBit (1 Bit) + register Byte (1 Byte) + Data (n Bytes)

	u32 errCode = NRF_SUCCESS;
#ifndef SIM_ENABLED
	twiXferDone = false;

	errCode = nrf_drv_twi_tx(&twi, slaveAddress, pTransferData, length, false);

	if (errCode != NRF_SUCCESS)
	{
		return nrfErrToGeneric(errCode);
	}
	// wait for transmission complete
	while (twiXferDone == false)
		;
	twiXferDone = false;
#endif
	return nrfErrToGeneric(errCode);
}

ErrorType FruityHal::twi_registerRead(u8 slaveAddress, u8 reg, u8 *pReceiveData, u8 length)
{
	// Slave Address (7 Bit) (Command) + WriteBit (1 Bit) + register Byte (1 Byte) + Repeated Start + Slave Address + ReadBit + Data.... + nAck
	u32 errCode = NRF_SUCCESS;
#ifndef SIM_ENABLED
	twiXferDone = false;

	nrf_drv_twi_xfer_desc_t xfer = NRF_DRV_TWI_XFER_DESC_TXRX(slaveAddress, &reg, 1, pReceiveData, length);

	errCode = nrf_drv_twi_xfer(&twi, &xfer, 0);

	if (errCode != NRF_SUCCESS)
	{
		return nrfErrToGeneric(errCode);
	}

	// wait for transmission and read complete
	while (twiXferDone == false)
		;
	twiXferDone = false;
#endif
	return nrfErrToGeneric(errCode);
}

bool FruityHal::twi_isInitialized(void)
{
#ifndef SIM_ENABLED
	return twiInitDone;
#else
	return cherrySimInstance->currentNode->twiWasInit;
#endif
}

ErrorType FruityHal::twi_read(u8 slaveAddress, u8 *pReceiveData, u8 length)
{
	// Slave Address (7 Bit) (Command) + ReadBit (1 Bit) + Data.... + nAck

	u32 errCode = NRF_SUCCESS;
#ifndef SIM_ENABLED
	twiXferDone = false;

	nrf_drv_twi_xfer_desc_t xfer; // = NRF_DRV_TWI_XFER_DESC_RX(slaveAddress, pReceiveData, length);
	CheckedMemset(&xfer, 0x00, sizeof(xfer));
	xfer.type = NRF_DRV_TWI_XFER_RX;
	xfer.address = slaveAddress;
	xfer.primary_length = length;
	xfer.p_primary_buf = pReceiveData;

	errCode = nrf_drv_twi_xfer(&twi, &xfer, 0);

	if (errCode != NRF_SUCCESS)
	{
		return nrfErrToGeneric(errCode);
	}

	// wait for transmission and read complete
	while (twiXferDone == false)
		;
	twiXferDone = false;
#endif
	return nrfErrToGeneric(errCode);
}

#ifndef SIM_ENABLED
#define SPI_INSTANCE 0
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);
volatile bool spiXferDone;
static bool spiInitDone = false;
#endif

#ifndef SIM_ENABLED
void spi_event_handler(nrf_drv_spi_evt_t const *p_event, void *p_context)
{
	spiXferDone = true;
	logt("FH", "SPI Xfer done");
}
#endif

void FruityHal::spi_init(u8 sckPin, u8 misoPin, u8 mosiPin)
{
#ifndef SIM_ENABLED
	/* Conigure SPI Interface */
	nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
	spi_config.sck_pin = sckPin;
	spi_config.miso_pin = misoPin;
	spi_config.mosi_pin = mosiPin;
	spi_config.frequency = NRF_DRV_SPI_FREQ_4M;

	APP_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, spi_event_handler, NULL));

	spiXferDone = true;
	spiInitDone = true;
#else
	if (cherrySimInstance->currentNode->spiWasInit)
	{
		//Already initialized!
		SIMEXCEPTION(IllegalStateException);
	}
	cherrySimInstance->currentNode->spiWasInit = true;
#endif
}

bool FruityHal::spi_isInitialized(void)
{
#ifndef SIM_ENABLED
	return spiInitDone;
#else
	return cherrySimInstance->currentNode->spiWasInit;
#endif
}

ErrorType FruityHal::spi_transfer(u8 *const p_toWrite, u8 count, u8 *const p_toRead, u8 slaveSelectPin)
{

	u32 retVal = NRF_SUCCESS;
#ifndef SIM_ENABLED
	logt("FH", "Transferring to BME");

	if ((NULL == p_toWrite) || (NULL == p_toRead))
	{
		retVal = NRF_ERROR_INTERNAL;
	}

	/* check if an other SPI transfer is running */
	if ((true == spiXferDone) && (NRF_SUCCESS == retVal))
	{
		spiXferDone = false;

		nrf_gpio_pin_clear(slaveSelectPin);
		APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, p_toWrite, count, p_toRead, count));
		//Locks if run in interrupt context
		while (!spiXferDone)
		{
			sd_app_evt_wait();
		}
		nrf_gpio_pin_set(slaveSelectPin);
		retVal = NRF_SUCCESS;
	}
	else
	{
		retVal = NRF_ERROR_BUSY;
	}
#endif
	return nrfErrToGeneric(retVal);
}

void FruityHal::spi_configureSlaveSelectPin(u32 pin)
{
#ifndef SIM_ENABLED
	nrf_gpio_pin_dir_set(pin, NRF_GPIO_PIN_DIR_OUTPUT);
	nrf_gpio_cfg_output(pin);
	nrf_gpio_pin_set(pin);
#endif
}
#endif // ifndef NRF51