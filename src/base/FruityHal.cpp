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
 * This is the HAL for the NRF51 and NRF52 chipsets
 */

#include <FruityHal.h>
#include <types.h>
#include <GlobalState.h>
#include <Logger.h>
#include <Node.h>

extern "C"{
#include <ble.h>
#include <ble_gap.h>
#include <ble_gatt.h>
#include <ble_gatts.h>
#include <nrf_soc.h>
#include <app_timer.h>

#ifndef SIM_ENABLED
#include <nrf_nvic.h>
#include <nrf_mbr.h>
#include <nrf_wdt.h>
#endif

#if defined(NRF51) || defined(SIM_ENABLED)
#include <softdevice_handler.h>
#endif

}

#define APP_TIMER_PRESCALER       0 // Value of the RTC1 PRESCALER register
#define APP_TIMER_MAX_TIMERS      1 //Maximum number of simultaneously created timers (2 + BSP_APP_TIMERS_NUMBER)
#define APP_TIMER_OP_QUEUE_SIZE   1 //Size of timer operation queues


//This tag is used to set the SoftDevice configuration and must be used when advertising or creating connections
#define BLE_CONN_CFG_TAG_FM 1

#define BLE_CONN_CFG_GAP_PACKET_BUFFERS 7

#include <string.h>

#pragma warning ("SDK 14 must be modified, change gcc_startup_nrf52.S to lowercase .s for startup scripts,...")
#pragma warning ("nrf_nvic.h must be included in ble_radio_notification.c in the sdk or it will fail to compile")
#pragma warning ("Must update FruityApi with new config, added totalIn and totalOut Connections")
#pragma warning ("Must check in sim if boardconfigPtr is a problem as global variable")
#pragma warning ("Check app_timer prescaler")

#define __________________BLE_STACK_EVT_HANDLING____________________
// ############### BLE Stack and Event Handling ########################

BleEventHandler FruityHal::bleEventHandler;
SystemEventHandler FruityHal::systemEventHandler;
TimerEventHandler FruityHal::timerEventHandler;

extern "C"
{

	static void timer_dispatch(void * p_context)
	{
	    UNUSED_PARAMETER(p_context);

	    //We just increase the time that has passed since the last handler
	    //And call the timer from our main event handling queue
	    GS->node->passsedTimeSinceLastTimerHandlerDs += Config->mainTimerTickDs;

	    //Timer handlers are called from the main event handling queue and from timerEventDispatch
	}

	static uint32_t getramend(void)
	{
	    uint32_t ram_total_size;

	#if defined(NRF51) || defined(SIM_ENABLED)
	    uint32_t block_size = NRF_FICR->SIZERAMBLOCKS;
	    ram_total_size      = block_size * NRF_FICR->NUMRAMBLOCK;
	#else
	    ram_total_size      = NRF_FICR->INFO.RAM * 1024;
	#endif

	    return 0x20000000 + ram_total_size;
	}
}

u32 FruityHal::BleStackInit(BleEventHandler bleEventHandler, SystemEventHandler systemEventHandler, TimerEventHandler timerEventHandler, ButtonEventHandler buttonEventHandler)
{
	u32 err = 0;

	FruityHal::bleEventHandler = bleEventHandler;
	FruityHal::systemEventHandler = systemEventHandler;
	FruityHal::timerEventHandler = timerEventHandler;


	logt("NODE", "Init Softdevice version 0x%x, Boardid %d", SD_FWID_GET(MBR_SIZE), Boardconfig->boardType);

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

	err = softdevice_handler_init(&clock_lf_cfg, GS->currentEventBuffer, GS->sizeOfEvent, NULL);
	APP_ERROR_CHECK(err);

	logt("NODE", "Softdevice Init OK");

	// Register with the SoftDevice handler module for System events.
	err = softdevice_sys_evt_handler_set(systemEventHandler);
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
	params.common_enable_params.p_conn_bw_counts->rx_counts.high_count = 4;
	params.common_enable_params.p_conn_bw_counts->tx_counts.high_count = 4;

	params.gap_enable_params.central_conn_count = Config->meshMaxOutConnections; //Number of connections as Central
	params.gap_enable_params.central_sec_count = 1; //this application only needs to be able to pair in one central link at a time

	params.gatts_enable_params.service_changed = 1; //we require the Service Changed characteristic
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

	//Enable 6 packets per connection interval
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

#pragma message ("Must implement block below for NRF52")

	//######### Enables the Softdevice
	u32 finalErr = 0;

	// Configure the clock source
    nrf_clock_lf_cfg_t clock_lf_cfg;
    if(Boardconfig->lfClockSource == NRF_CLOCK_LF_SRC_XTAL){
        clock_lf_cfg.source = NRF_CLOCK_LF_SRC_XTAL;
        clock_lf_cfg.rc_ctiv = 0;
    } else {
        clock_lf_cfg.source = NRF_CLOCK_LF_SRC_RC;
        clock_lf_cfg.rc_ctiv = 1;
    }
    clock_lf_cfg.rc_temp_ctiv = 0;
    clock_lf_cfg.accuracy = NRF_CLOCK_LF_ACCURACY_100_PPM;

    //Enable SoftDevice using given clock source
    err = sd_softdevice_enable(&clock_lf_cfg, app_error_fault_handler);
	if(err){
		if(finalErr == 0) finalErr = err;
		logt("ERROR", "%u");
	}

//		If using interrupts to handle BLE events
//    // Enable BLE event interrupt (interrupt priority has already been set by the stack).
//    err = sd_nvic_EnableIRQ((IRQn_Type)SD_EVT_IRQn);
//    if (err){
//    	if(finalErr == 0) finalErr = err;
//    	logt("ERROR", "%u");
//    }

	logt("ERROR", "SD Enable %u", err);

	uint32_t ram_start = (u32)__application_ram_start_address;

	//######### Sets our custom SoftDevice configuration

	//Create a SoftDevice configuration
	ble_cfg_t ble_cfg;

	// Configure the connection count.
	memset(&ble_cfg, 0, sizeof(ble_cfg));
	ble_cfg.conn_cfg.conn_cfg_tag                     = BLE_CONN_CFG_TAG_FM;
	ble_cfg.conn_cfg.params.gap_conn_cfg.conn_count   = Config->totalInConnections + Config->totalOutConnections;
	ble_cfg.conn_cfg.params.gap_conn_cfg.event_length = NRF_SDH_BLE_GAP_EVENT_LENGTH; //TODO: do some tests and put in config

	err = sd_ble_cfg_set(BLE_CONN_CFG_GAP, &ble_cfg, ram_start);
	if(err){
		if(finalErr == 0) finalErr = err;
		logt("ERROR", "%u");
	}

	// Configure the connection roles.
	memset(&ble_cfg, 0, sizeof(ble_cfg));
	ble_cfg.gap_cfg.role_count_cfg.periph_role_count  = Config->totalInConnections;
	ble_cfg.gap_cfg.role_count_cfg.central_role_count = Config->totalOutConnections;
	ble_cfg.gap_cfg.role_count_cfg.central_sec_count  = BLE_GAP_ROLE_COUNT_CENTRAL_SEC_DEFAULT; //TODO: Could change this

	err = sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &ble_cfg, ram_start);
	if(err){
		if(finalErr == 0) finalErr = err;
		logt("ERROR", "%u");
	}

	// set HVN queue size
	memset(&ble_cfg, 0, sizeof(ble_cfg_t));
	ble_cfg.conn_cfg.conn_cfg_tag = BLE_CONN_CFG_TAG_FM;
	ble_cfg.conn_cfg.params.gatts_conn_cfg.hvn_tx_queue_size = BLE_CONN_CFG_GAP_PACKET_BUFFERS; /* application wants to queue 7 HVNs */
	sd_ble_cfg_set(BLE_CONN_CFG_GATTS, &ble_cfg, ram_start);

	// set WRITE_CMD queue size
	memset(&ble_cfg, 0, sizeof(ble_cfg_t));
	ble_cfg.conn_cfg.conn_cfg_tag = BLE_CONN_CFG_TAG_FM;
	ble_cfg.conn_cfg.params.gattc_conn_cfg.write_cmd_tx_queue_size = BLE_CONN_CFG_GAP_PACKET_BUFFERS;
	sd_ble_cfg_set(BLE_CONN_CFG_GATTC, &ble_cfg, ram_start);

	// Configure the maximum ATT MTU.
#if false
	memset(&ble_cfg, 0x00, sizeof(ble_cfg));
	ble_cfg.conn_cfg.conn_cfg_tag                 = BLE_CONN_CFG_TAG_FM;
	ble_cfg.conn_cfg.params.gatt_conn_cfg.att_mtu = BLE_GATT_ATT_MTU_DEFAULT;

	err = sd_ble_cfg_set(BLE_CONN_CFG_GATT, &ble_cfg, ram_start);
	APP_ERROR_CHECK(err);
#endif

	// Configure number of custom UUIDS.
	memset(&ble_cfg, 0, sizeof(ble_cfg));
	ble_cfg.common_cfg.vs_uuid_cfg.vs_uuid_count = BLE_UUID_VS_COUNT_DEFAULT; //TODO: look in implementation

	err = sd_ble_cfg_set(BLE_COMMON_CFG_VS_UUID, &ble_cfg, ram_start);
	if(err){
		if(finalErr == 0) finalErr = err;
		logt("ERROR", "%u");
	}

	// Configure the GATTS attribute table.
	memset(&ble_cfg, 0x00, sizeof(ble_cfg));
	ble_cfg.gatts_cfg.attr_tab_size.attr_tab_size = NRF_SDH_BLE_GATTS_ATTR_TAB_SIZE;

	err = sd_ble_cfg_set(BLE_GATTS_CFG_ATTR_TAB_SIZE, &ble_cfg, ram_start);
	if(err){
		if(finalErr == 0) finalErr = err;
		logt("ERROR", "%u");
	}

	// Configure Service Changed characteristic.
	memset(&ble_cfg, 0x00, sizeof(ble_cfg));
	ble_cfg.gatts_cfg.service_changed.service_changed = BLE_GATTS_SERVICE_CHANGED_DEFAULT;

	err = sd_ble_cfg_set(BLE_GATTS_CFG_SERVICE_CHANGED, &ble_cfg, ram_start);
	if(err){
		if(finalErr == 0) finalErr = err;
		logt("ERROR", "%u");
	}

	//######### Enables the BLE stack
	err = sd_ble_enable(&ram_start);
	logt("ERROR", "Linker Ram section should be at %x, len %x", ram_start, getramend() - ram_start);
	APP_ERROR_CHECK(finalErr);
	APP_ERROR_CHECK(err);
#endif //NRF52

	//Enable DC/DC (needs external LC filter, cmp. nrf51 reference manual page 43)
	err = sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
	APP_ERROR_CHECK(err); //OK

	//Set power mode
	err = sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
	APP_ERROR_CHECK(err); //OK

	//Set preferred TX power
	err = sd_ble_gap_tx_power_set(Config->defaultDBmTX);
	APP_ERROR_CHECK(err); //OK

	return 0;
}

void FruityHal::EventLooper(BleEventHandler bleEventHandler, SystemEventHandler systemEventHandler, TimerEventHandler timerEventHandler, ButtonEventHandler buttonEventHandler)
{
	while (true)
	{
		u32 err = NRF_ERROR_NOT_FOUND;

#ifdef ACTIVATE_CLC_MODULE
		ClcComm::getInstance()->CheckAndProcess();
#endif

		do
		{
			//Check if there is input on uart
			Terminal::getInstance()->CheckAndProcessLine();

			//Fetch the event
			GS->sizeOfCurrentEvent = GS->sizeOfEvent;
			err = sd_ble_evt_get((u8*)GS->currentEventBuffer, &GS->sizeOfCurrentEvent);

			//Handle ble event event
			if (err == NRF_SUCCESS)
			{
				//logt("EVENT", "--- EVENT_HANDLER %d -----", currentEvent->header.evt_id);
				bleEventHandler(GS->currentEvent);
			}
			//No more events available
			else if (err == NRF_ERROR_NOT_FOUND)
			{
				//Handle waiting button event
				if(GS->button1HoldTimeDs != 0){
					u32 holdTimeDs = GS->button1HoldTimeDs;
					GS->button1HoldTimeDs = 0;

					buttonEventHandler(0, holdTimeDs);
				}

				//Handle Timer event that was waiting
				if (GS->node && GS->node->passsedTimeSinceLastTimerHandlerDs > 0)
				{
					u16 timerDs = GS->node->passsedTimeSinceLastTimerHandlerDs;

					//Dispatch timer to all other modules
					timerEventHandler(timerDs, GS->node->appTimerDs);

					//FIXME: Should protect this with a semaphore
					//because the timerInterrupt works asynchronously
					GS->node->passsedTimeSinceLastTimerHandlerDs -= timerDs;
				}

				// Pull event from soc
				while(true){
					u32 evt_id;
					err = sd_evt_get(&evt_id);

					if (err == NRF_ERROR_NOT_FOUND){
						break;
					} else {
						systemEventHandler(evt_id); // Call handler
					}
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


#define __________________GAP____________________

u32 FruityHal::BleGapAddressSet(fh_ble_gap_addr_t const *address)
{
	ble_gap_addr_t addr;
	memcpy(addr.addr, address->addr, FH_BLE_GAP_ADDR_LEN);
	addr.addr_type = address->addr_type;

#if defined(NRF51) || defined(SIM_ENABLED)
	return sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &addr);
#elif defined(NRF52)
	return sd_ble_gap_addr_set(&addr);
#endif

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

	return err;
}

u32 FruityHal::BleGapScanStart(fh_ble_gap_scan_params_t const *scanParams)
{
	ble_gap_scan_params_t scan_params;
	scan_params.active = 0;
	scan_params.interval = scanParams->interval;
	scan_params.timeout = scanParams->timeout;
	scan_params.window = scanParams->window;

#if defined(NRF51) || defined(SIM_ENABLED)
	scan_params.p_whitelist = NULL;
	scan_params.selective = 0;
#elif defined(NRF52)
	scan_params.adv_dir_report = 0;
	scan_params.use_whitelist = 0;
#endif

	return sd_ble_gap_scan_start(&scan_params);
}

u32 FruityHal::BleGapScanStop()
{
	return sd_ble_gap_scan_stop();
}

u32 FruityHal::BleGapAdvStart(fh_ble_gap_adv_params_t const *advParams)
{
	ble_gap_adv_params_t adv_params;
	adv_params.channel_mask.ch_37_off = advParams->channel_mask.ch_37_off;
	adv_params.channel_mask.ch_38_off = advParams->channel_mask.ch_38_off;
	adv_params.channel_mask.ch_39_off= advParams->channel_mask.ch_39_off;
	adv_params.fp = BLE_GAP_ADV_FP_ANY;
	adv_params.interval = advParams->interval;
	adv_params.p_peer_addr = NULL;
	adv_params.timeout = advParams->timeout;
	adv_params.type = advParams->type;

#if defined(NRF51) || defined(SIM_ENABLED)
	return sd_ble_gap_adv_start(&adv_params);
#elif defined(NRF52)
	return sd_ble_gap_adv_start(&adv_params, BLE_CONN_CFG_TAG_FM);
#endif

}

u32 FruityHal::BleGapAdvStop()
{
	return sd_ble_gap_adv_stop();
}

u32 FruityHal::BleGapConnect(fh_ble_gap_addr_t const *peerAddress, fh_ble_gap_scan_params_t const *scanParams, fh_ble_gap_conn_params_t const *connectionParams)
{
	ble_gap_addr_t p_peer_addr;
	memset(&p_peer_addr, 0x00, sizeof(p_peer_addr));
	p_peer_addr.addr_type = peerAddress->addr_type;
	memcpy(p_peer_addr.addr, peerAddress->addr, sizeof(peerAddress->addr));

	ble_gap_scan_params_t p_scan_params;
	memset(&p_scan_params, 0x00, sizeof(p_scan_params));
	p_scan_params.active = 0;
	p_scan_params.interval = scanParams->interval;
	p_scan_params.timeout = scanParams->timeout;
	p_scan_params.window = scanParams->window;
#if defined(NRF51) || defined(SIM_ENABLED)
	p_scan_params.p_whitelist = NULL;
	p_scan_params.selective = 0;
#elif defined(NRF52)
	p_scan_params.adv_dir_report = 0;
	p_scan_params.use_whitelist = 0;
#endif

	ble_gap_conn_params_t p_conn_params;
	memset(&p_conn_params, 0x00, sizeof(p_conn_params));
	p_conn_params.conn_sup_timeout = connectionParams->conn_sup_timeout;
	p_conn_params.max_conn_interval = connectionParams->max_conn_interval;
	p_conn_params.min_conn_interval = connectionParams->min_conn_interval;
	p_conn_params.slave_latency = connectionParams->slave_latency;


#if defined(NRF51) || defined(SIM_ENABLED)
	return sd_ble_gap_connect(&p_peer_addr, &p_scan_params, &p_conn_params);
#elif defined(NRF52)
	return sd_ble_gap_connect(&p_peer_addr, &p_scan_params, &p_conn_params, BLE_CONN_CFG_TAG_FM);
#endif
}

uint32_t FruityHal::BleTxPacketCountGet(u16 connectionHandle, u8* count)
{
#if defined(NRF51) || defined(SIM_ENABLED)
	return sd_ble_tx_packet_count_get(connectionHandle, count);
#elif defined(NRF52)
#pragma message ("Must read the softdevice configuration")
	*count = BLE_CONN_CFG_GAP_PACKET_BUFFERS;
	return 0;
#endif
}

#define __________________MISC____________________

uint32_t FruityHal::StartTimers()
{
	u32 err = 0;
	APP_TIMER_DEF(mainTimerMsId);

#if defined(NRF51)
	APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, NULL);

	err = app_timer_create(&mainTimerMsId, APP_TIMER_MODE_REPEATED, timer_dispatch);
    APP_ERROR_CHECK(err);

	err = app_timer_start(mainTimerMsId, APP_TIMER_TICKS(Config->mainTimerTickDs * 100, APP_TIMER_PRESCALER), NULL);
    APP_ERROR_CHECK(err);
#elif defined(NRF52)
    app_timer_init();

	err = app_timer_create(&mainTimerMsId, APP_TIMER_MODE_REPEATED, timer_dispatch);
	APP_ERROR_CHECK(err);

	err = app_timer_start(mainTimerMsId, APP_TIMER_TICKS(Config->mainTimerTickDs * 100), NULL);
	APP_ERROR_CHECK(err);
#endif
	return 0;
}

uint32_t FruityHal::GetRtc()
{
#if defined(NRF51) || defined(SIM_ENABLED)
	u32 count;
	app_timer_cnt_get(&count);
	return count;
#elif defined(NRF52)
	return app_timer_cnt_get();
#endif
}

void FruityHal::SystemReset()
{
	sd_nvic_SystemReset();
}

// Retrieves the reboot reason from the RESETREAS register
uint8_t FruityHal::GetRebootReason()
{
#ifndef SIM_ENABLED
	u32 reason = NRF_POWER->RESETREAS;

	if(reason & POWER_RESETREAS_DOG_Msk){
		return REBOOT_REASON_WATCHDOG;
	} else if (reason & POWER_RESETREAS_RESETPIN_Msk){
		return REBOOT_REASON_PIN_RESET;
	} else if (reason & POWER_RESETREAS_OFF_Msk){
		return REBOOT_REASON_FROM_OFF_STATE;
	} else {
		return REBOOT_REASON_UNKNOWN;
	}
#else
	return REBOOT_REASON_UNKNOWN;
#endif
}

//Clears the Reboot reason because the RESETREAS register is cumulative
uint32_t FruityHal::ClearRebootReason()
{
	sd_power_reset_reason_clr(0xFFFFFFFFUL);
	return 0;
}

uint32_t FruityHal::GetRtcDifference(u32 ticksTo, u32 ticksFrom)
{
#if defined(NRF51) || defined(SIM_ENABLED)
	u32 diff;
	app_timer_cnt_diff_compute(ticksTo, ticksFrom, &diff);
	return diff;
#elif defined(NRF52)
	return app_timer_cnt_diff_compute(ticksTo, ticksFrom);
#endif
}

//Starts the Watchdog with a static interval so that changing a config can do no harm
void FruityHal::StartWatchdog()
{
#ifdef ACTIVATE_WATCHDOG
	u32 err = 0;

	//Configure Watchdog to default: Run while CPU sleeps
	nrf_wdt_behaviour_set(NRF_WDT_BEHAVIOUR_RUN_SLEEP);
	//Configure Watchdog timeout
	nrf_wdt_reload_value_set(FM_WATCHDOG_TIMEOUT);
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
#ifdef ACTIVATE_WATCHDOG
	u32 err = 0;
	nrf_wdt_reload_request_set(NRF_WDT_RR0);
#endif
}

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
