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

#include <FruityHalNrf.h>
#include <types.h>
#include <GlobalState.h>
#include <Logger.h>
#include <ScanController.h>
#include <Node.h>
#ifdef SIM_ENABLED
#include <CherrySim.h>
#endif
#ifdef ACTIVATE_CLC_MODULE
#include <ClcComm.h>
#endif
#ifdef ACTIVATE_VS_MODULE
#include <VsComm.h>
#endif


#define APP_TIMER_PRESCALER       0 // Value of the RTC1 PRESCALER register
#define APP_TIMER_MAX_TIMERS      1 //Maximum number of simultaneously created timers (2 + BSP_APP_TIMERS_NUMBER)
#define APP_TIMER_OP_QUEUE_SIZE   1 //Size of timer operation queues


//This tag is used to set the SoftDevice configuration and must be used when advertising or creating connections
#define BLE_CONN_CFG_TAG_FM 1

#define BLE_CONN_CFG_GAP_PACKET_BUFFERS 7

#include <string.h>


//################################################
#define ________________NRF_HANDLERS______________

//When radio activity happens, this handler will be called
void RadioHandler(bool radioActive)
{
	//TODO: Allow to register handler and dispatch to it
}


#define __________________BLE_STACK_EVT_HANDLING____________________
// ############### BLE Stack and Event Handling ########################

BleEventHandler    FruityHal::bleEventHandler    = nullptr;
SystemEventHandler FruityHal::systemEventHandler = nullptr;
TimerEventHandler  FruityHal::timerEventHandler  = nullptr;
ButtonEventHandler FruityHal::buttonEventHandler = nullptr;
static UartEventHandler uartEventHandler         = nullptr;
AppErrorHandler    FruityHal::appErrorHandler    = nullptr;
StackErrorHandler  FruityHal::stackErrorHandler  = nullptr;
HardfaultHandler   FruityHal::hardfaultHandler   = nullptr;
#ifdef SIM_ENABLED
DBDiscoveryHandler FruityHal::dbDiscoveryHandler = nullptr;
#endif

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

u32 FruityHal::SetEventHandlers(
		BleEventHandler   bleEventHandler,   SystemEventHandler systemEventHandler,
		TimerEventHandler timerEventHandler, ButtonEventHandler buttonEventHandler,
		AppErrorHandler   appErrorHandler,   StackErrorHandler  stackErrorHandler, 
		HardfaultHandler  hardfaultHandler)
{
	FruityHal::bleEventHandler = bleEventHandler;
	FruityHal::systemEventHandler = systemEventHandler;
	FruityHal::timerEventHandler = timerEventHandler;
	FruityHal::buttonEventHandler = buttonEventHandler;

	FruityHal::appErrorHandler = appErrorHandler;
	FruityHal::stackErrorHandler = stackErrorHandler;
	FruityHal::hardfaultHandler = hardfaultHandler;

	return 0;
}

u32 FruityHal::BleStackInit()
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

	err = softdevice_handler_init(&clock_lf_cfg, GS->currentEventBuffer, GS->sizeOfEvent, nullptr);
	APP_ERROR_CHECK(err);

	logt("NODE", "Softdevice Init OK");

	//We now configure the parameters for enabling the softdevice, this will determine the needed RAM for the SD
	ble_enable_params_t params;
	memset(&params, 0x00, sizeof(params));

	//Configre the number of Vendor Specific UUIDs
	params.common_enable_params.vs_uuid_count = 5;

	//Configure the number of connections as peripheral and central
	params.gap_enable_params.periph_conn_count = Config->totalInConnections; //Number of connections as Peripheral
	params.gap_enable_params.central_conn_count = Config->totalOutConnections; //Number of connections as Central
	params.gap_enable_params.central_sec_count = 1; //this application only needs to be able to pair in one central link at a time

	//Configure Bandwidth (We want all our connections to have a high throughput for RX and TX
	ble_conn_bw_counts_t bwCounts;
	memset(&bwCounts, 0x00, sizeof(ble_conn_bw_counts_t));
	bwCounts.rx_counts.high_count = Config->totalInConnections + Config->totalOutConnections;
	bwCounts.tx_counts.high_count = Config->totalInConnections + Config->totalOutConnections;
	params.common_enable_params.p_conn_bw_counts = &bwCounts;

	//Configure the GATT Server Parameters
	params.gatts_enable_params.service_changed = 1; //we require the Service Changed characteristic
	params.gatts_enable_params.attr_tab_size = BLE_GATTS_ATTR_TAB_SIZE_DEFAULT; //the default Attribute Table size is appropriate for our application

	//The base ram address is gathered from the linker
	u32 app_ram_base = (u32)__application_ram_start_address;
	/* enable the BLE Stack */
	logt("NODE", "Ram base at 0x%x", app_ram_base);
	err = sd_ble_enable(&params, &app_ram_base);
	if(err == NRF_SUCCESS){
	/* Verify that __LINKER_APP_RAM_BASE matches the SD calculations */
		if(app_ram_base != (u32)__application_ram_start_address){
			logt("WARNING", "Warning: unused memory: 0x%x", ((u32)__application_ram_start_address) - app_ram_base);
		}
	} else if(err == NRF_ERROR_NO_MEM) {
		/* Not enough memory for the SoftDevice. Use output value in linker script */
		logt("ERROR", "Fatal: Not enough memory for the selected configuration. Required:0x%x", app_ram_base);
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
    clock_lf_cfg.xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_100_PPM;
#else
    clock_lf_cfg.accuracy = NRF_CLOCK_LF_ACCURACY_100_PPM;
#endif

    //Enable SoftDevice using given clock source
    err = sd_softdevice_enable(&clock_lf_cfg, app_error_fault_handler);
	if(err){
		if(finalErr == 0) finalErr = err;
		logt("ERROR", "%u");
	}

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
	err = sd_power_dcdc_mode_set(Boardconfig->dcDcEnabled ? NRF_POWER_DCDC_ENABLE : NRF_POWER_DCDC_DISABLE);
	APP_ERROR_CHECK(err); //OK

	//Set power mode
	err = sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
	APP_ERROR_CHECK(err); //OK

	//Set preferred TX power
	err = sd_ble_gap_tx_power_set(Config->defaultDBmTX);
	APP_ERROR_CHECK(err); //OK

	//Enable radio notifications if desired
#ifdef ACTIVATE_RADIO_NOTIFICATIONS
	if(Config->enableRadioNotificationHandler){
		//TODO: Will sometimes not initialize if there are flash operations at that time
		err = ble_radio_notification_init(RADIO_NOTIFICATION_IRQ_PRIORITY, NRF_RADIO_NOTIFICATION_DISTANCE_800US, RadioHandler);
		if(err) logt("ERROR", "Could not initialize radio notifications %u", err);
	}
#endif

	return 0;
}

void FruityHal::EventLooper()
{
	while (true)
	{
		u32 err = NRF_ERROR_NOT_FOUND;

#ifdef ACTIVATE_CLC_COMM
		if(GS->clcComm) GS->clcComm->CheckAndProcess();
#endif

#ifdef ACTIVATE_VS_MODULE
		if (GS->vsComm) GS->vsComm->CheckAndProcess();
#endif

		//When using the watchdog with a timeout smaller than 60 seconds, we feed it in our event loop
#ifdef FM_WATCHDOG_TIMEOUT
		if(FM_WATCHDOG_TIMEOUT < 32768UL * 60){
			FruityHal::FeedWatchdog();
		}
#endif

		do
		{
			//Check if there is input on uart
			GS->terminal->CheckAndProcessLine();

			//Fetch the event
			GS->sizeOfCurrentEvent = GS->sizeOfEvent;
			err = sd_ble_evt_get((u8*)GS->currentEventBuffer, &GS->sizeOfCurrentEvent);

			//Handle ble event event
			if (err == NRF_SUCCESS)
			{
				//logt("EVENT", "--- EVENT_HANDLER %d -----", currentEvent->header.evt_id);
				bleEventHandler(*GS->currentEvent);
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
				if (GS->passsedTimeSinceLastTimerHandlerDs > 0)
				{
					u16 timerDs = GS->passsedTimeSinceLastTimerHandlerDs;

					//Dispatch timer to all other modules
					timerEventHandler(timerDs);

					//FIXME: Should protect this with a semaphore
					//because the timerInterrupt works asynchronously
					GS->passsedTimeSinceLastTimerHandlerDs -= timerDs;
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

#ifdef SIM_ENABLED
				//The Simulator must simulate multiple nodes, so we have to stop the event looper here
				return;
#endif
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

void FruityHal::SetUartHandler(UartEventHandler _uartEventHandler)
{
	uartEventHandler = _uartEventHandler;
}

UartEventHandler FruityHal::GetUartHandler()
{
	return uartEventHandler;
}


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

u32 FruityHal::BleGapScanStop()
{
	u32 err = sd_ble_gap_scan_stop();
	logt("FH", "Scan stop(%u)", err);
	return err;
}

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
	adv_params.type = advParams->type;

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

u32 FruityHal::BleGapConnect(fh_ble_gap_addr_t const *peerAddress, fh_ble_gap_scan_params_t const *scanParams, fh_ble_gap_conn_params_t const *connectionParams)
{
	u32 err;
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
	p_scan_params.p_whitelist = nullptr;
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
	err = sd_ble_gap_connect(&p_peer_addr, &p_scan_params, &p_conn_params);
#elif defined(NRF52)
	err = sd_ble_gap_connect(&p_peer_addr, &p_scan_params, &p_conn_params, BLE_CONN_CFG_TAG_FM);
#endif

	logt("FH", "Connect (%u) iv:%u, tmt:%u", err, p_conn_params.min_conn_interval, p_scan_params.timeout);

	//Tell our ScanController, that scanning has stopped
	GS->scanController->ScanningHasStopped();

	return err;
}


uint32_t FruityHal::ConnectCancel()
{
	u32 err = sd_ble_gap_connect_cancel();

	logt("FH", "Connect Cancel (%u)", err);

	return err;
}

uint32_t FruityHal::Disconnect(uint16_t conn_handle, uint8_t hci_status_code)
{
	u32 err = sd_ble_gap_disconnect(conn_handle, hci_status_code);

	logt("FH", "Disconnect (%u)", err);

	return err;
}

uint32_t FruityHal::BleTxPacketCountGet(u16 connectionHandle, u8* count)
{
#if defined(NRF51) || defined(SIM_ENABLED)
	return sd_ble_tx_packet_count_get(connectionHandle, count);
#elif defined(NRF52)
//TODO: must be read from somewhere else
	*count = BLE_CONN_CFG_GAP_PACKET_BUFFERS;
	return 0;
#endif
}

uint32_t FruityHal::DiscovereServiceInit(DBDiscoveryHandler dbEventHandler)
{
#ifndef  SIM_ENABLED
	return ble_db_discovery_init(dbEventHandler);
#else // ! SIM_ENABLED
	FruityHal::dbDiscoveryHandler = dbEventHandler;
#endif
	return 0;
}

uint32_t FruityHal::DiscoverService(u16 connHandle, const ble_uuid_t &p_uuid, ble_db_discovery_t * p_discoveredServices)
{
	uint32_t err = 0;
#ifndef SIM_ENABLED
	memset(p_discoveredServices, 0x00, sizeof(*p_discoveredServices));
	err = ble_db_discovery_evt_register(&p_uuid);
	if (err) {
		logt("ERROR", "err %u", err);
		return err;
	}

	err = ble_db_discovery_start(p_discoveredServices, connHandle);
	if (err) {
		logt("ERROR", "err %u", err);
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

uint32_t FruityHal::InitializeButtons()
{
	u32 err = 0;
#if defined(USE_BUTTONS) && !defined(SIM_ENABLED)
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
	err =  nrf_drv_gpiote_in_init(Boardconfig->button1Pin, &buttonConfig, button_interrupt_handler);

	//Enable the events
	nrf_drv_gpiote_in_event_enable(Boardconfig->button1Pin, true);
#endif

	return err;
}

//################################################
#define _________________UART_____________________

//This handler receives UART interrupts (terminal json mode)
extern "C"{
	void UART0_IRQHandler(void)
	{
		if (uartEventHandler == nullptr) {
			SIMEXCEPTION(UartNotSetException);
		} else {
		    uartEventHandler();
		}
	}
}

//################################################
#define _________________TIMERS___________________

extern "C"{
	void app_timer_handler(void * p_context){
		UNUSED_PARAMETER(p_context);

		//We just increase the time that has passed since the last handler
		//And call the timer from our main event handling queue
		GS->passsedTimeSinceLastTimerHandlerDs += MAIN_TIMER_TICK_DS;
	}
}

uint32_t FruityHal::StartTimers()
{
	u32 err = 0;

#if defined(NRF51)
	APP_TIMER_DEF(mainTimerMsId);

	APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, nullptr);

	err = app_timer_create(&mainTimerMsId, APP_TIMER_MODE_REPEATED, app_timer_handler);
    APP_ERROR_CHECK(err);

	err = app_timer_start(mainTimerMsId, APP_TIMER_TICKS(MAIN_TIMER_TICK_DS * 100, APP_TIMER_PRESCALER), nullptr);
    APP_ERROR_CHECK(err);
#elif defined(NRF52)
	APP_TIMER_DEF(mainTimerMsId);

    app_timer_init();

	err = app_timer_create(&mainTimerMsId, APP_TIMER_MODE_REPEATED, app_timer_handler);
	APP_ERROR_CHECK(err);

	err = app_timer_start(mainTimerMsId, APP_TIMER_TICKS(MAIN_TIMER_TICK_DS * 100), nullptr);
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

//################################################
#define _____________FAULT_HANDLERS_______________

//These error handlers are declared WEAK in the nRF SDK and can be implemented here
//Will be called if an error occurs somewhere in the code, but not if it's a hardfault
extern "C"
{
	//The app_error handler_bare is called by all APP_ERROR_CHECK functions when DEBUG is undefined
	void app_error_handler_bare(uint32_t error_code)
	{
		FruityHal::appErrorHandler(error_code);
	}

	//The app_error handler is called by all APP_ERROR_CHECK functions when DEBUG is defined
	void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
	{
		app_error_handler_bare(error_code);
		logt("ERROR", "App error code:%s(%u), file:%s, line:%u", GS->logger->getNrfErrorString(error_code), error_code, p_file_name, line_num);
	}

	//Called when the softdevice crashes
	void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
	{
		FruityHal::stackErrorHandler(id, pc, info);
	}

#ifndef SIM_ENABLED
	//We use the nordic hardfault handler that stacks all fault variables for us before calling this function
	__attribute__((used)) void HardFault_c_handler(stacked_regs_t* stack)
	{
		FruityHal::hardfaultHandler(stack);
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

u32 FruityHal::GetBootloaderVersion()
{
	if(BOOTLOADER_UICR_ADDRESS != 0xFFFFFFFF){
		return *(u32*)(BOOTLOADER_UICR_ADDRESS + 1024);
	} else {
		return 0;
	}
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
