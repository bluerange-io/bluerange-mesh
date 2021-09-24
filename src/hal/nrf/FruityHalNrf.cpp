////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2021 M-Way Solutions GmbH
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
 * This is the HAL for NRF52 chipsets
 * It is also the HAL used by the CherrySim simulator
 */


#include <array>
#include "FruityHal.h"
#include "FruityMesh.h"
#include <FmTypes.h>
#include <GlobalState.h>
#include <Logger.h>
#include <ScanController.h>
#include <Timeslot.h>
#include <Node.h>
#include "Utility.h"
#ifdef SIM_ENABLED
#include <CherrySim.h>
#endif
#ifndef GITHUB_RELEASE
#if IS_ACTIVE(CLC_MODULE)
#include <ClcComm.h>
#endif
#if IS_ACTIVE(VS_MODULE)
#include <VsComm.h>
#endif
#if IS_ACTIVE(WM_MODULE)
#include <WmComm.h>
#endif
#endif //GITHUB_RELEASE

extern "C" {
#include <app_timer.h>
#include <ble_db_discovery.h>
#ifndef SIM_ENABLED
#include <app_util_platform.h>
#include <nrf_uart.h>
#include <nrf_mbr.h>
#include <nrf_drv_gpiote.h>
#include <nrf_wdt.h>
#include <nrf_delay.h>
#include <nrf_sdm.h>
#ifdef NRF52
#include <nrf_power.h>
#include <nrf_drv_twi.h>
#include <nrf_drv_spi.h>
#include <nrf_drv_saadc.h>
#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <nrf_sdh_soc.h>
#include <SEGGER_RTT.h>
#else
#include <softdevice_handler.h>
#include <ble_stack_handler_types.h>
#include <nrf_drv_adc.h>
#endif
#if IS_ACTIVE(VIRTUAL_COM_PORT)
#include <virtual_com_port.h>
#endif
#endif

#if FH_NRF_ENABLE_EASYDMA_TERMINAL
#include <nrf_serial.h>
#include <nrf_drv_uart.h>
#endif
}

#define APP_SOC_OBSERVER_PRIO           1
#define APP_BLE_OBSERVER_PRIO           2

#if defined(NRF52) || defined(NRF52840)
#define BOOTLOADER_UICR_ADDRESS           (NRF_UICR->NRFFW[0])
#elif defined(SIM_ENABLED)
#define BOOTLOADER_UICR_ADDRESS           (NRF_UICR->BOOTLOADERADDR)
#endif

#define APP_TIMER_PRESCALER     0 // Value of the RTC1 PRESCALER register
#define APP_TIMER_OP_QUEUE_SIZE 1 //Size of timer operation queues

#define APP_TIMER_MAX_TIMERS    5 //Maximum number of simultaneously created timers (2 + BSP_APP_TIMERS_NUMBER)

#define HIGHER_THAN_APP_PRIO    6 //IRQ Priority of interrupts such as UART that perform little work and should be fast
#define SD_EVT_IRQ_PRIORITY     7 //IRQ prio of our whole main application event fetching, timer handling, etc,....
#ifdef NRF52

//The priority of the SoftDevice event reporting is changed to the lowest priority (7) because we need two interrupt 
//levels that are called at a higher priority than our main context logic. On the other hand, we cannot use priority 
//3 as this would interrupt low priority SoftDevice logic too often. Therefore all handlers should be set to interrupt 
//level 7 so that they cannot occur at the same time and cause race conditions. This also applies to the timer interrupt. 
//High priority handling can use IRQ priority 6 or in some special cases even priority 3 but must make sure that it will 
//not cause race conditions with application code.
// In short, we are using:
//   - main context: long running processing (seldomly used)
//   - application interrupt level (7): most application logic, timer event handler, event processing, ....
//   - interrupt level 6: used for e.g. UART and other peripherals that write into buffers and do not interfere with the application logic
//   - interrupt level 3: very important high priority interrupts
static_assert(SD_EVT_IRQ_PRIORITY == APP_TIMER_CONFIG_IRQ_PRIORITY, "Check irq priorities");
#endif

#if (SDK == 16)

NRF_BLE_GQ_DEF(m_ble_gatt_queue,
               NRF_SDH_BLE_CENTRAL_LINK_COUNT,
               NRF_BLE_GQ_QUEUE_SIZE);

#endif

constexpr u8 MAX_GPIOTE_HANDLERS = 4;
struct GpioteHandlerValues
{
    FruityHal::GpioInterruptHandler handler;
    u32 pin;
};

struct NrfHalMemory
{
    std::array <app_timer_t, APP_TIMER_MAX_TIMERS> swTimers;
    ble_db_discovery_t discoveredServices;
    volatile bool twiXferDone;
    bool twiInitDone;
    volatile bool spiXferDone;
    bool spiInitDone;
    volatile bool nrfSerialDataAvailable = false;
    volatile bool nrfSerialErrorDetected = false;
    bool overflowPending;
    u32 time_ms;
    GpioteHandlerValues GpioHandler[MAX_GPIOTE_HANDLERS];
    u8 gpioteHandlersCreated;
    ble_evt_t const * currentEvent;
    u8 timersCreated;
#if SDK == 15 || SDK == 16
    ble_gap_adv_data_t advData;
#endif
#if IS_ACTIVE(TIMESLOT)
    nrf_radio_signal_callback_return_param_t timeslotRadioCallbackReturnParam;
    nrf_radio_request_t timeslotRadioRequest;
#endif // IS_ACTIVE(TIMESLOT)
};

//#################### Event Buffer ###########################
//A global buffer for the current event, which must be 4-byte aligned

#ifdef SIM_ENABLED
static constexpr u16 SIZE_OF_EVENT_BUFFER = GlobalState::SIZE_OF_EVENT_BUFFER;
#endif // SIM_ENABLED

// Bootloader settings page where the app needs to store the update information
#if defined(SIM_ENABLED)
#define REGION_BOOTLOADER_SETTINGS_START (FLASH_REGION_START_ADDRESS + 0x0003FC00) //Last page of flash
#elif defined(NRF52840)
#define REGION_BOOTLOADER_SETTINGS_START (FLASH_REGION_START_ADDRESS + 0x000FF000) //Last page of flash
#elif defined(NRF52)
#define REGION_BOOTLOADER_SETTINGS_START (FLASH_REGION_START_ADDRESS + 0x0007F000) //Last page of flash
#endif

//This tag is used to set the SoftDevice configuration and must be used when advertising or creating connections
constexpr int BLE_CONN_CFG_TAG_FM = 1;

constexpr int BLE_CONN_CFG_GAP_PACKET_BUFFERS = 7;

//Bootloader constants
constexpr int BOOTLOADER_DFU_START  = (0xB1);      /**< Value to set in GPREGRET to boot to DFU mode. */
constexpr int BOOTLOADER_DFU_START2 = (0xB2);      /**< Value to set in GPREGRET2 to boot to DFU mode. */

#include <string.h>

//Forward declarations
static ErrorType nrfErrToGeneric(u32 code);
static FruityHal::BleGattEror nrfErrToGenericGatt(u32 code);
static const char* getBleEventNameString(u16 bleEventId);
u32 ClearGeneralPurposeRegister(u32 gpregId, u32 mask);
u32 WriteGeneralPurposeRegister(u32 gpregId, u32 mask);
static uint32_t sdAppEvtWaitAnomaly87();
#if IS_ACTIVE(ASSET_MODULE)
//Both twiTurnOnAnomaly89() and twiTurnOffAnomaly89() is only added if ACTIVATE_ASSET_MODULE is active because currently
//twi is only activated for ASSET_MODULE so to avoid the cpp warning of unused functions. Tracked in BR-2082.
#if !defined(SIM_ENABLED)
static void twiTurnOffAnomaly89();
static void twiTurnOnAnomaly89();
#endif //!defined(SIM_ENABLED)
#endif //IS_ACTIVE(ASSET_MODULE)

#define __________________BLE_STACK_INIT____________________
// ############### BLE Stack Initialization ########################

static u32 getramend(void)
{
    u32 ram_total_size = NRF_FICR->INFO.RAM * 1024;
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
    if (code <= NRF_ERROR_RESOURCES) return (ErrorType)code;
    else{
        switch(code)
        {
            case BLE_ERROR_INVALID_CONN_HANDLE:
                return ErrorType::BLE_INVALID_CONN_HANDLE;
            case BLE_ERROR_INVALID_ATTR_HANDLE:
                return ErrorType::BLE_INVALID_ATTR_HANDLE;
            case BLE_ERROR_INVALID_ROLE:
                return ErrorType::BLE_INVALID_ROLE;
            case BLE_ERROR_GATTS_INVALID_ATTR_TYPE:
                return ErrorType::BLE_INVALID_ATTR_TYPE;
            case BLE_ERROR_GATTS_SYS_ATTR_MISSING:
                return ErrorType::BLE_SYS_ATTR_MISSING;
            case BLE_ERROR_GAP_INVALID_BLE_ADDR:
                return ErrorType::BLE_INVALID_BLE_ADDR;
            default:
                return ErrorType::UNKNOWN;
        }
    }
}

static inline nrf_gpio_pin_pull_t GenericPullModeToNrf(FruityHal::GpioPullMode mode)
{
    if (mode == FruityHal::GpioPullMode::GPIO_PIN_PULLUP) return NRF_GPIO_PIN_PULLUP;
    else if (mode == FruityHal::GpioPullMode::GPIO_PIN_PULLDOWN) return NRF_GPIO_PIN_PULLDOWN;
    else return NRF_GPIO_PIN_NOPULL;
}

static inline nrf_gpio_pin_sense_t GenericSenseModeToNrf(FruityHal::GpioSenseMode mode)
{
    if (mode == FruityHal::GpioSenseMode::GPIO_PIN_LOWSENSE) return NRF_GPIO_PIN_SENSE_LOW;
    else if (mode == FruityHal::GpioSenseMode::GPIO_PIN_HIGHSENSE) return NRF_GPIO_PIN_SENSE_HIGH;
    else return NRF_GPIO_PIN_NOSENSE;
}

static inline nrf_gpiote_polarity_t GenericPolarityToNrf(FruityHal::GpioTransistion transition)
{
    if (transition == FruityHal::GpioTransistion::GPIO_TRANSITION_TOGGLE) return NRF_GPIOTE_POLARITY_TOGGLE;
    else if (transition == FruityHal::GpioTransistion::GPIO_TRANSITION_LOW_TO_HIGH) return NRF_GPIOTE_POLARITY_LOTOHI;
    else return NRF_GPIOTE_POLARITY_HITOLO;
}

static inline FruityHal::GpioTransistion NrfPolarityToGeneric(nrf_gpiote_polarity_t polarity)
{
    if (polarity == NRF_GPIOTE_POLARITY_TOGGLE) return FruityHal::GpioTransistion::GPIO_TRANSITION_TOGGLE;
    else if (polarity == NRF_GPIOTE_POLARITY_LOTOHI) return FruityHal::GpioTransistion::GPIO_TRANSITION_LOW_TO_HIGH;
    else return FruityHal::GpioTransistion::GPIO_TRANSITION_HIGH_TO_LOW;
}

#ifdef NRF52
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context);
static void soc_evt_handler(uint32_t evt_id, void * p_context);

#endif

ErrorType FruityHal::BleStackInit()
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
    //######### Enables the Softdevice
    u32 finalErr = 0;

    err = nrf_sdh_enable_request();
    FRUITYMESH_ERROR_CHECK(err);

    //Use IRQ Priority 7 for SOC and BLE events instead of 6
    //This allows us to use IRQ Prio 6 for UART and other peripherals
    err = sd_nvic_SetPriority(SD_EVT_IRQn, SD_EVT_IRQ_PRIORITY);
    FRUITYMESH_ERROR_CHECK(err);

    logt("FH", "ENReq %u", err);

    uint32_t ram_start = (u32)__application_ram_start_address;

    //######### Sets our custom SoftDevice configuration

    //Create a SoftDevice configuration
    ble_cfg_t ble_cfg;
    CheckedMemset(&ble_cfg, 0, sizeof(ble_cfg));

    // Configure the connection count.
    ble_cfg.conn_cfg.conn_cfg_tag                     = BLE_CONN_CFG_TAG_FM;
    ble_cfg.conn_cfg.params.gap_conn_cfg.conn_count   = GS->config.totalInConnections + GS->config.totalOutConnections;
    ble_cfg.conn_cfg.params.gap_conn_cfg.event_length = 4; //4 units = 5ms (1.25ms steps) this is the time used to handle one connection

    err = sd_ble_cfg_set(BLE_CONN_CFG_GAP, &ble_cfg, ram_start);
    if(err){
        if(finalErr == 0) finalErr = err;
        logt("ERROR", "BLE_CONN_CFG_GAP %u", err);
    }

    // Configure the connection roles.
    CheckedMemset(&ble_cfg, 0, sizeof(ble_cfg));
    ble_cfg.gap_cfg.role_count_cfg.periph_role_count  = GS->config.totalInConnections;
    ble_cfg.gap_cfg.role_count_cfg.central_role_count = GS->config.totalOutConnections;
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
    CheckedMemset(&ble_cfg, 0x00, sizeof(ble_cfg));
    ble_cfg.conn_cfg.conn_cfg_tag                 = BLE_CONN_CFG_TAG_FM;
    ble_cfg.conn_cfg.params.gatt_conn_cfg.att_mtu = NRF_SDH_BLE_GATT_MAX_MTU_SIZE;

    err = sd_ble_cfg_set(BLE_CONN_CFG_GATT, &ble_cfg, ram_start);
    FRUITYMESH_ERROR_CHECK(err);

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
    err = nrf_sdh_ble_enable(&ram_start);
    const char* tag = "FH";
    if (err) tag = "ERROR";
    logt(tag, "Err %u, Linker Ram section should be at %x, len %x", err, (u32)ram_start, (u32)(getramend() - ram_start));
    FRUITYMESH_ERROR_CHECK(finalErr);
    FRUITYMESH_ERROR_CHECK(err);

#ifndef SIM_ENABLED
    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
    NRF_SDH_SOC_OBSERVER(m_soc_observer, APP_SOC_OBSERVER_PRIO, soc_evt_handler, NULL);
#endif

    //######### Other configuration

    //We also configure connection event length extension to increase the throughput
    ble_opt_t opt;
    CheckedMemset(&opt, 0x00, sizeof(opt));
    opt.common_opt.conn_evt_ext.enable = 1;

    err = sd_ble_opt_set(BLE_COMMON_OPT_CONN_EVT_EXT, &opt);
    if (err != 0) logt("ERROR", "Could not configure conn length extension %u", err);

    //Enable DC/DC (needs external LC filter, cmp. nrf51 reference manual page 43)
    err = sd_power_dcdc_mode_set(Boardconfig->dcDcEnabled ? NRF_POWER_DCDC_ENABLE : NRF_POWER_DCDC_DISABLE);
    if (err) tag = "ERROR";
    logt(tag, "sd_power_dcdc_mode_set %u", err);
    FRUITYMESH_ERROR_CHECK(err); //OK

    // Set power mode
    err = sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
    if (err) tag = "ERROR";
    logt(tag, "sd_power_mode_set %u", err);
    FRUITYMESH_ERROR_CHECK(err); //OK

    err = (u32)FruityHal::RadioSetTxPower(Conf::defaultDBmTX, FruityHal::TxRole::SCAN_INIT, 0);
    FRUITYMESH_ERROR_CHECK(err); //OK

    return (ErrorType)err;
}

void FruityHal::BleStackDeinit()
{
#ifndef SIM_ENABLED
    nrf_sdh_disable_request();
#endif // SIM_ENABLED
}

#define __________________BLE_STACK_EVT_FETCHING____________________
// ############### BLE Stack and Event Handling ########################

/*
    EventHandling for Simulator:
        Events are fetched from the BLE Stack using polling and a sleep function that will block until
        another event is generated. This ensures that all event handling code is executed in order.
        Low latency functionality such as Timer and UART events are implemented interrupt based.

    EventHandling for NRF52:
        The SoftDevice Handler library is used to fetch events interrupt based.
        The SoftDevice Interrupt is defined as SD_EVT_IRQn, which is SWI2_IRQn and is set to IRQ_PRIO 7.
        All high level functionality must be called from IRQ PRIO 7 so that it cannot interrupt the other handlers.
        Events such as Timer, UART RX, SPI RX, etc,... can be handeled on IRQ PRIO 6 but should only perform very
        little processing (mostly buffering). They can then set the SWI2 pending using the SetPendingEventIRQ method.
        IRQ PRIO 6 tasks should not modify any data except their own variables.
        The main() thread can be used as well but care must be taken as this will be interrupted by IRQ PRIO 7.
 */

static FruityHal::SystemEvents nrfSystemEventToGeneric(u32 event)
{
    switch (event)
    {
        case NRF_EVT_HFCLKSTARTED:
            return FruityHal::SystemEvents::HIGH_FREQUENCY_CLOCK_STARTED;
        case NRF_EVT_POWER_FAILURE_WARNING:
            return FruityHal::SystemEvents::POWER_FAILURE_WARNING;
        case NRF_EVT_FLASH_OPERATION_SUCCESS:
            return FruityHal::SystemEvents::FLASH_OPERATION_SUCCESS;
        case NRF_EVT_FLASH_OPERATION_ERROR:
            return FruityHal::SystemEvents::FLASH_OPERATION_ERROR;
        case NRF_EVT_RADIO_BLOCKED:
            return FruityHal::SystemEvents::RADIO_BLOCKED;
        case NRF_EVT_RADIO_CANCELED:
            return FruityHal::SystemEvents::RADIO_CANCELED;
        case NRF_EVT_RADIO_SIGNAL_CALLBACK_INVALID_RETURN:
            return FruityHal::SystemEvents::RADIO_SIGNAL_CALLBACK_INVALID_RETURN;
        case NRF_EVT_RADIO_SESSION_IDLE:
            return FruityHal::SystemEvents::RADIO_SESSION_IDLE;
        case NRF_EVT_RADIO_SESSION_CLOSED:
            return FruityHal::SystemEvents::RADIO_SESSION_CLOSED;
        case NRF_EVT_NUMBER_OF_EVTS:
            return FruityHal::SystemEvents::NUMBER_OF_EVTS;
        default:
            return FruityHal::SystemEvents::UNKOWN_EVENT;

    }
}

//Checks for high level application events generated e.g. by low level interrupt events
void ProcessAppEvents()
{
    FruityHal::VirtualComProcessEvents();

    for (u32 i = 0; i < GS->numApplicationInterruptHandlers; i++)
    {
        GS->applicationInterruptHandlers[i]();
    }

    //When using the watchdog with a timeout smaller than 60 seconds, we feed it in our event loop
    if (GET_WATCHDOG_TIMEOUT() != 0)
    {
        if (GET_WATCHDOG_TIMEOUT() < 32768UL * 60)
        {
            FruityHal::FeedWatchdog();
        }
    }

    //Check if there is input on uart
    GS->terminal.CheckAndProcessLine();

#if IS_ACTIVE(BUTTONS)
    //Handle waiting button event
    if(GS->button1HoldTimeDs != 0){
        u32 holdTimeDs = GS->button1HoldTimeDs;
        GS->button1HoldTimeDs = 0;

        ::DispatchButtonEvents(0, holdTimeDs);
    }
#endif

    //Handle Timer event that was waiting
    if (GS->passsedTimeSinceLastTimerHandlerDs > 0)
    {
        u16 timerDs = GS->passsedTimeSinceLastTimerHandlerDs;

        //Dispatch timer to all other modules
        DispatchTimerEvents(timerDs);

        GS->passsedTimeSinceLastTimerHandlerDs -= timerDs;
    }
}

#if defined(SIM_ENABLED)
void FruityHal::EventLooper()
{
    //TODO: We could execute this in a separate thread as this will typically be interrupted by interrupts
    //Call all main context handlers
    for (u32 i = 0; i < GS->numMainContextHandlers; i++)
    {
        GS->mainContextHandlers[i]();
    }

    //Check for waiting events from the application
    ProcessAppEvents();

    while (true)
    {
        //Fetch BLE events
        u16 eventSize = SIZE_OF_EVENT_BUFFER;
#ifndef SIM_ENABLED
    u32 err = sd_ble_evt_get((u8*)currentEventBuffer, &eventSize);
#else
    u32 err = sd_ble_evt_get((u8*)GS->currentEventBuffer, &eventSize);
#endif

        //Handle ble event event
        if (err == NRF_SUCCESS)
        {
#ifndef SIM_ENABLED
      FruityHal::DispatchBleEvents((void*)currentEventBuffer);
#else
      FruityHal::DispatchBleEvents((void*)GS->currentEventBuffer);
#endif
        }
        //No more events available
        else if (err == NRF_ERROR_NOT_FOUND)
        {
            break;
        }
        else
        {
            FRUITYMESH_ERROR_CHECK(err); //LCOV_EXCL_LINE assertion
            break;
        }
    }

    GS->inPullEventsLoop = true;
    // Pull event from soc
    while(true){
        uint32_t evt_id;
        u32 err = sd_evt_get(&evt_id);

        if (err == NRF_ERROR_NOT_FOUND){
            break;
        } else {
            ::DispatchSystemEvents(nrfSystemEventToGeneric(evt_id)); // Call handler
        }
    }
    GS->inPullEventsLoop = false;

    u32 err = sdAppEvtWaitAnomaly87();
    FRUITYMESH_ERROR_CHECK(err); // OK
    err = sd_nvic_ClearPendingIRQ(SD_EVT_IRQn);
    FRUITYMESH_ERROR_CHECK(err);  // OK
}

u16 FruityHal::GetEventBufferSize()
{
    return SIZE_OF_EVENT_BUFFER;
}



#else

//This will get called for all events on the ble stack and also for all other interrupts
static void nrf_sdh_fruitymesh_evt_handler(void * p_context)
{
    GS->fruitymeshEventLooperTriggerTimestamp = FruityHal::GetRtcMs();
    ProcessAppEvents();
}

//This is called for all BLE related events
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    GS->bleEventLooperTriggerTimestamp = FruityHal::GetRtcMs();
    FruityHal::DispatchBleEvents(p_ble_evt);
}

//This is called for all SoC related events
static void soc_evt_handler(uint32_t evt_id, void * p_context)
{
    GS->socEventLooperTriggerTimestamp = FruityHal::GetRtcMs();
    ::DispatchSystemEvents(nrfSystemEventToGeneric(evt_id));
}

//Register an Event handler for all stack events
NRF_SDH_STACK_OBSERVER(m_nrf_sdh_fruitymesh_evt_handler, NRF_SDH_BLE_STACK_OBSERVER_PRIO) =
{
    .handler   = nrf_sdh_fruitymesh_evt_handler,
    .p_context = NULL,
};

//This is our main() after the stack was initialized and is called in a while loop
void FruityHal::EventLooper()
{
    GS->eventLooperTriggerTimestamp = GetRtcMs();


    //Call all main context handlers so that they can do low priority long-running processing
    for (u32 i = 0; i < GS->numMainContextHandlers; i++)
    {
        GS->mainContextHandlers[i]();
    }

    u32 err = sdAppEvtWaitAnomaly87();
    FRUITYMESH_ERROR_CHECK(err); // OK
}

#endif

//Sets the SWI2 IRQ for events so that we can immediately process our main event handler
void FruityHal::SetPendingEventIRQ()
{
#ifndef SIM_ENABLED
    sd_nvic_SetPendingIRQ(SD_EVT_IRQn);
#endif
}

#define __________________EVENT_HANDLERS____________________
// ############### Methods to be called on events ########################

void FruityHal::DispatchBleEvents(void const * eventVirtualPointer)
{
    const ble_evt_t& bleEvent = *((ble_evt_t const *)eventVirtualPointer);
    u16 eventId = bleEvent.header.evt_id;
    if (eventId == BLE_GAP_EVT_ADV_REPORT || eventId == BLE_GAP_EVT_RSSI_CHANGED) {
        logt("EVENTS2", "BLE EVENT %s (%d)", getBleEventNameString(eventId), eventId);
    }
    else {
        logt("EVENTS", "BLE EVENT %s (%d)", getBleEventNameString(eventId), eventId);
    }
    
    //Calls the Db Discovery modules event handler
#if defined(NRF52)
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    ble_db_discovery_on_ble_evt(&bleEvent, &halMemory->discoveredServices);
#endif

    switch (bleEvent.header.evt_id)
    {
    case BLE_GAP_EVT_RSSI_CHANGED:
        {
            GapRssiChangedEvent rce(&bleEvent);
            DispatchEvent(rce);
        }
        break;
    case BLE_GAP_EVT_ADV_REPORT:
        {
#if (SDK == 15 || SDK == 16)
            //In the later version of the SDK, we need to call sd_ble_gap_scan_start again with a nullpointer to continue to receive scan data
            if (bleEvent.evt.gap_evt.params.adv_report.type.status != BLE_GAP_ADV_DATA_STATUS_INCOMPLETE_MORE_DATA)
            {
                ble_data_t scan_data;
                scan_data.len = BLE_GAP_SCAN_BUFFER_MAX;
                scan_data.p_data = GS->scanBuffer;
                u32 err = sd_ble_gap_scan_start(NULL, &scan_data);
                if ((err != NRF_ERROR_INVALID_STATE) &&
                    (err != NRF_ERROR_RESOURCES)) FRUITYMESH_ERROR_CHECK(err);
            }
#endif
            GS->advertismentReceivedTimestamp = GetRtcMs();
            GapAdvertisementReportEvent are(&bleEvent);
            DispatchEvent(are);
        }
        break;
    case BLE_GAP_EVT_CONNECTED:
        {
            FruityHal::GapConnectedEvent ce(&bleEvent);
            DispatchEvent(ce);
        }
        break;
    case BLE_GAP_EVT_DISCONNECTED:
        {
            FruityHal::GapDisconnectedEvent de(&bleEvent);
            DispatchEvent(de);
        }
        break;
    case BLE_GAP_EVT_TIMEOUT:
        {
            FruityHal::GapTimeoutEvent gte(&bleEvent);
            DispatchEvent(gte);
        }
        break;
    case BLE_GAP_EVT_SEC_INFO_REQUEST:
        {
            FruityHal::GapSecurityInfoRequestEvent sire(&bleEvent);
            DispatchEvent(sire);
        }
        break;
    case BLE_GAP_EVT_CONN_SEC_UPDATE:
        {
            FruityHal::GapConnectionSecurityUpdateEvent csue(&bleEvent);
            DispatchEvent(csue);
        }
        break;
#if IS_ACTIVE(CONN_PARAM_UPDATE)
    case BLE_GAP_EVT_CONN_PARAM_UPDATE:
        {
            FruityHal::GapConnParamUpdateEvent cpue(&bleEvent);
            DispatchEvent(cpue);
        }
        break;
    case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
        {
            FruityHal::GapConnParamUpdateRequestEvent cpure(&bleEvent);
            DispatchEvent(cpure);
        }
        break;
#endif
    case BLE_GATTC_EVT_WRITE_RSP:
        {
#ifdef SIM_ENABLED
            //if(cherrySimInstance->currentNode->id == 37 && bleEvent.evt.gattc_evt.conn_handle == 680) printf("%04u Q@NODE %u EVT_WRITE_RSP received" EOL, cherrySimInstance->globalBreakCounter++, cherrySimInstance->currentNode->id);
#endif
            FruityHal::GattcWriteResponseEvent wre(&bleEvent);
            DispatchEvent(wre);
        }
        break;
    case BLE_GATTC_EVT_TIMEOUT: //jstodo untested event
        {
            FruityHal::GattcTimeoutEvent gte(&bleEvent);
            DispatchEvent(gte);
        }
        break;
    case BLE_GATTS_EVT_WRITE:
        {
            GS->lastSendTimestamp = GetRtcMs();
            FruityHal::GattsWriteEvent gwe(&bleEvent);
            DispatchEvent(gwe);
        }
        break;
    case BLE_GATTC_EVT_HVX:
        {
            GS->lastSendTimestamp = GetRtcMs();
            FruityHal::GattcHandleValueEvent hve(&bleEvent);
            DispatchEvent(hve);
        }
        break;
    case BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE:
    case BLE_GATTS_EVT_HVN_TX_COMPLETE:
        {
#ifdef SIM_ENABLED
            //if (cherrySimInstance->currentNode->id == 37 && bleEvent.evt.common_evt.conn_handle == 680) printf("%04u Q@NODE %u WRITE_CMD_TX_COMPLETE %u received" EOL, cherrySimInstance->globalBreakCounter++, cherrySimInstance->currentNode->id, bleEvent.evt.common_evt.params.tx_complete.count);
#endif
            GS->lastReceivedTimestamp = GetRtcMs();
            FruityHal::GattDataTransmittedEvent gdte(&bleEvent);
            DispatchEvent(gdte);
        }
        break;

    case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
    {
        //We answer all MTU update requests with our max mtu that was configured
        u32 err = sd_ble_gatts_exchange_mtu_reply(bleEvent.evt.gatts_evt.conn_handle, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);

        u16 partnerMtu = bleEvent.evt.gatts_evt.params.exchange_mtu_request.client_rx_mtu;
        u16 effectiveMtu = NRF_SDH_BLE_GATT_MAX_MTU_SIZE < partnerMtu ? NRF_SDH_BLE_GATT_MAX_MTU_SIZE : partnerMtu;

        logt("FH", "Reply MTU Exchange (%u) on conn %u with %u", err, bleEvent.evt.gatts_evt.conn_handle, effectiveMtu);

        ConnectionManager::GetInstance().MtuUpdatedHandler(bleEvent.evt.gatts_evt.conn_handle, effectiveMtu);

        break;
    }

    case BLE_GATTC_EVT_EXCHANGE_MTU_RSP:
    {
        u16 partnerMtu = bleEvent.evt.gattc_evt.params.exchange_mtu_rsp.server_rx_mtu;
        u16 effectiveMtu = NRF_SDH_BLE_GATT_MAX_MTU_SIZE < partnerMtu ? NRF_SDH_BLE_GATT_MAX_MTU_SIZE : partnerMtu;

        logt("FH", "MTU for hnd %u updated to %u", bleEvent.evt.gattc_evt.conn_handle, effectiveMtu);

        ConnectionManager::GetInstance().MtuUpdatedHandler(bleEvent.evt.gattc_evt.conn_handle, effectiveMtu);
    }
    break;

    case BLE_GAP_EVT_DATA_LENGTH_UPDATE:
    {
        ble_gap_evt_data_length_update_t const* params = (ble_gap_evt_data_length_update_t const*)&bleEvent.evt.gap_evt.params.data_length_update;

        logt("FH", "DLE Result: rx %u/%u, tx %u/%u",
            params->effective_params.max_rx_octets,
            params->effective_params.max_rx_time_us,
            params->effective_params.max_tx_octets,
            params->effective_params.max_tx_time_us);

        // => We do not notify the application and can assume that it worked if the other device has enough resources
        //    If it does not work, this link will have a slightly reduced throughput, so this is monitored in another place
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
        //The softdevice will chose the parameters so that our configured NRF_SDH_BLE_GATT_MAX_MTU_SIZE fits into a link layer packet
        sd_ble_gap_data_length_update(bleEvent.evt.gap_evt.conn_handle, nullptr, nullptr);
    }
    break;

#endif
    case BLE_GATTS_EVT_SYS_ATTR_MISSING:    //jstodo untested event
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


    if (eventId == BLE_GAP_EVT_ADV_REPORT || eventId == BLE_GAP_EVT_RSSI_CHANGED) {
        logt("EVENTS2", "End of event");
    }
    else {
        logt("EVENTS", "End of event");
    }
}

FruityHal::GapConnParamUpdateEvent::GapConnParamUpdateEvent(void const * _evt)
    :GapEvent(_evt)
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id != BLE_GAP_EVT_CONN_PARAM_UPDATE)
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
    }
}

FruityHal::GapConnParamUpdateRequestEvent::GapConnParamUpdateRequestEvent(void const * _evt)
    :GapEvent(_evt)
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id != BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST)
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
    }
}

FruityHal::GapEvent::GapEvent(void const * _evt)
    : BleEvent(_evt)
{
}

u16 FruityHal::GapEvent::GetConnectionHandle() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.conn_handle;
}

u16 FruityHal::GapConnParamUpdateEvent::GetMinConnectionInterval() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.conn_param_update.conn_params.min_conn_interval;
}

u16 FruityHal::GapConnParamUpdateEvent::GetMaxConnectionInterval() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.conn_param_update.conn_params.max_conn_interval;
}

u16 FruityHal::GapConnParamUpdateEvent::GetSlaveLatency() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.conn_param_update.conn_params.slave_latency;
}

u16 FruityHal::GapConnParamUpdateEvent::GetConnectionSupervisionTimeout() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.conn_param_update.conn_params.conn_sup_timeout;
}

u16 FruityHal::GapConnParamUpdateRequestEvent::GetMinConnectionInterval() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.conn_param_update.conn_params.min_conn_interval;
}

u16 FruityHal::GapConnParamUpdateRequestEvent::GetMaxConnectionInterval() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.conn_param_update.conn_params.max_conn_interval;
}

u16 FruityHal::GapConnParamUpdateRequestEvent::GetSlaveLatency() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.conn_param_update.conn_params.slave_latency;
}

u16 FruityHal::GapConnParamUpdateRequestEvent::GetConnectionSupervisionTimeout() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.conn_param_update.conn_params.conn_sup_timeout;
}

FruityHal::GapRssiChangedEvent::GapRssiChangedEvent(void const * _evt)
    :GapEvent(_evt)
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id != BLE_GAP_EVT_RSSI_CHANGED)
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
    }
}

i8 FruityHal::GapRssiChangedEvent::GetRssi() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.rssi_changed.rssi;
}

FruityHal::GapAdvertisementReportEvent::GapAdvertisementReportEvent(void const * _evt)
    :GapEvent(_evt)
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id != BLE_GAP_EVT_ADV_REPORT)
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
    }
}

i8 FruityHal::GapAdvertisementReportEvent::GetRssi() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.adv_report.rssi;
}

const u8 * FruityHal::GapAdvertisementReportEvent::GetData() const
{
#if (SDK == 15 || SDK == 16)
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.adv_report.data.p_data;
#else
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.adv_report.data;
#endif
}

u32 FruityHal::GapAdvertisementReportEvent::GetDataLength() const
{
#if (SDK == 15 || SDK == 16)
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.adv_report.data.len;
#else
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.adv_report.dlen;
#endif
}

FruityHal::BleGapAddrBytes FruityHal::GapAdvertisementReportEvent::GetPeerAddr() const
{
    FruityHal::BleGapAddrBytes retVal{};
    CheckedMemcpy(retVal.data(), ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.adv_report.peer_addr.addr, FH_BLE_GAP_ADDR_LEN)
    return retVal;
}

FruityHal::BleGapAddrType FruityHal::GapAdvertisementReportEvent::GetPeerAddrType() const
{
    return (BleGapAddrType)((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.adv_report.peer_addr.addr_type;
}

bool FruityHal::GapAdvertisementReportEvent::IsConnectable() const
{
#if (SDK == 15 || SDK == 16)
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.adv_report.type.connectable == 0x01;
#else
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.adv_report.type == BLE_GAP_ADV_TYPE_ADV_IND;
#endif
}

FruityHal::BleEvent::BleEvent(void const *_evt)
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent != nullptr) {
        //This is thrown if two events are processed at the same time, which is illegal.
        SIMEXCEPTION(IllegalStateException); //LCOV_EXCL_LINE assertion
    }
    ((NrfHalMemory*)GS->halMemory)->currentEvent = (ble_evt_t const *)_evt;
}

#ifdef SIM_ENABLED
FruityHal::BleEvent::~BleEvent()
{
    ((NrfHalMemory*)GS->halMemory)->currentEvent = nullptr;
}
#endif

FruityHal::GapConnectedEvent::GapConnectedEvent(void const * _evt)
    :GapEvent(_evt)
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id != BLE_GAP_EVT_CONNECTED)
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
    }
}

FruityHal::GapRole FruityHal::GapConnectedEvent::GetRole() const
{
    return (GapRole)(((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.connected.role);
}

u8 FruityHal::GapConnectedEvent::GetPeerAddrType() const
{
    return (((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.connected.peer_addr.addr_type);
}

u16 FruityHal::GapConnectedEvent::GetMinConnectionInterval() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.connected.conn_params.min_conn_interval;
}

FruityHal::BleGapAddrBytes FruityHal::GapConnectedEvent::GetPeerAddr() const
{
    FruityHal::BleGapAddrBytes retVal{};
    CheckedMemcpy(retVal.data(), ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.connected.peer_addr.addr, FH_BLE_GAP_ADDR_LEN)
    return retVal;
}

FruityHal::GapDisconnectedEvent::GapDisconnectedEvent(void const * _evt)
    : GapEvent(_evt)
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id != BLE_GAP_EVT_DISCONNECTED)
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
    }
}

FruityHal::BleHciError FruityHal::GapDisconnectedEvent::GetReason() const
{
    return (FruityHal::BleHciError)((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.disconnected.reason;
}

FruityHal::GapTimeoutEvent::GapTimeoutEvent(void const * _evt)
    : GapEvent(_evt)
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id != BLE_GAP_EVT_TIMEOUT)
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
    }
}

FruityHal::GapTimeoutSource FruityHal::GapTimeoutEvent::GetSource() const
{
    switch (((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.timeout.src)
    {
#if (SDK == 14)
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
}

FruityHal::GapSecurityInfoRequestEvent::GapSecurityInfoRequestEvent(void const * _evt)
    : GapEvent(_evt)
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id != BLE_GAP_EVT_SEC_INFO_REQUEST)
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
    }
}

FruityHal::GapConnectionSecurityUpdateEvent::GapConnectionSecurityUpdateEvent(void const * _evt)
    : GapEvent(_evt)
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id != BLE_GAP_EVT_CONN_SEC_UPDATE)
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
    }
}

u8 FruityHal::GapConnectionSecurityUpdateEvent::GetKeySize() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.conn_sec_update.conn_sec.encr_key_size;
}

FruityHal::SecurityLevel FruityHal::GapConnectionSecurityUpdateEvent::GetSecurityLevel() const
{
    return (FruityHal::SecurityLevel)(((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.lv);
}

FruityHal::SecurityMode FruityHal::GapConnectionSecurityUpdateEvent::GetSecurityMode() const
{
    return (FruityHal::SecurityMode)(((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.sm);
}

FruityHal::GattcEvent::GattcEvent(void const * _evt)
    : BleEvent(_evt)
{
}

u16 FruityHal::GattcEvent::GetConnectionHandle() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gattc_evt.conn_handle;
}

FruityHal::BleGattEror FruityHal::GattcEvent::GetGattStatus() const
{
    return nrfErrToGenericGatt(((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gattc_evt.gatt_status);
}

FruityHal::GattcWriteResponseEvent::GattcWriteResponseEvent(void const * _evt)
    : GattcEvent(_evt)
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id != BLE_GATTC_EVT_WRITE_RSP)
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
    }
}

FruityHal::GattcTimeoutEvent::GattcTimeoutEvent(void const * _evt)
    : GattcEvent(_evt)
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id != BLE_GATTC_EVT_TIMEOUT)
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
    }
}

FruityHal::GattDataTransmittedEvent::GattDataTransmittedEvent(void const * _evt)
    :BleEvent(_evt)
{
#ifdef SIM_ENABLED
    const u32 evtId = ((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id;
    if (evtId != BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE && evtId != BLE_GATTS_EVT_HVN_TX_COMPLETE)
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
    }
#endif
}

u16 FruityHal::GattDataTransmittedEvent::GetConnectionHandle() const
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id == BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE) {
        return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gattc_evt.conn_handle;
    }
    else if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id == BLE_GATTS_EVT_HVN_TX_COMPLETE) {
        return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gatts_evt.conn_handle;
    }
    SIMEXCEPTION(IllegalArgumentException);
    return -1; //This must never be executed!
}

bool FruityHal::GattDataTransmittedEvent::IsConnectionHandleValid() const
{
    return GetConnectionHandle() != FruityHal::FH_BLE_INVALID_HANDLE;
}

u32 FruityHal::GattDataTransmittedEvent::GetCompleteCount() const
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id == BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE) {
        return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gattc_evt.params.write_cmd_tx_complete.count;
    }
    else if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id == BLE_GATTS_EVT_HVN_TX_COMPLETE) {
        return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gatts_evt.params.hvn_tx_complete.count;
    }
    SIMEXCEPTION(IllegalStateException);
    return -1; //This must never be executed!
}

FruityHal::GattsWriteEvent::GattsWriteEvent(void const * _evt)
    : BleEvent(_evt)
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id != BLE_GATTS_EVT_WRITE)
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
    }
}

u16 FruityHal::GattsWriteEvent::GetAttributeHandle() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gatts_evt.params.write.handle;
}

bool FruityHal::GattsWriteEvent::IsWriteRequest() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gatts_evt.params.write.op == BLE_GATTS_OP_WRITE_REQ;
}

u16 FruityHal::GattsWriteEvent::GetLength() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gatts_evt.params.write.len;
}

u16 FruityHal::GattsWriteEvent::GetConnectionHandle() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gatts_evt.conn_handle;
}

u8 const * FruityHal::GattsWriteEvent::GetData() const
{
    return (u8 const *)((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gatts_evt.params.write.data;
}

FruityHal::GattcHandleValueEvent::GattcHandleValueEvent(void const * _evt)
    :GattcEvent(_evt)
{
    if (((NrfHalMemory*)GS->halMemory)->currentEvent->header.evt_id != BLE_GATTC_EVT_HVX)
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE assertion
    }
}

u16 FruityHal::GattcHandleValueEvent::GetHandle() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gattc_evt.params.hvx.handle;
}

u16 FruityHal::GattcHandleValueEvent::GetLength() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gattc_evt.params.hvx.len;
}

u8 const * FruityHal::GattcHandleValueEvent::GetData() const
{
    return ((NrfHalMemory*)GS->halMemory)->currentEvent->evt.gattc_evt.params.hvx.data;
}

/*
    #####################
    ###               ###
    ###      GAP      ###
    ###               ###
    #####################
*/


#define __________________GAP____________________

ErrorType FruityHal::SetBleGapAddress(FruityHal::BleGapAddr const &address)
{
    ble_gap_addr_t addr;
    CheckedMemset(&addr, 0, sizeof(addr));
    CheckedMemcpy(addr.addr, address.addr.data(), FH_BLE_GAP_ADDR_LEN);
    addr.addr_type = (u8)address.addr_type;
    ErrorType err = nrfErrToGeneric(sd_ble_gap_addr_set(&addr));
    logt("FH", "Gap Addr Set (%u)", (u32)err);

    return err;
}

FruityHal::BleGapAddr FruityHal::GetBleGapAddress()
{
    FruityHal::BleGapAddr retVal;
    CheckedMemset(&retVal, 0, sizeof(retVal));
    ble_gap_addr_t p_addr;
    CheckedMemset(&p_addr, 0, sizeof(p_addr));

    //According to the nordic docs, the only possible none zero return value is
    //"NRF_ERROR_INVALID_ADDR" which is only returned if an invalid pointer is
    //supplied to the function. That would indicate to a clear implementation
    //error, thus a FRUITYMESH_ERROR_CHECK is valid. This applies to both
    //sd_ble_gap_address_get and sd_ble_gap_addr_get
    FRUITYMESH_ERROR_CHECK(sd_ble_gap_addr_get(&p_addr));

    CheckedMemcpy(retVal.addr.data(), p_addr.addr, FH_BLE_GAP_ADDR_LEN);
    retVal.addr_type = (BleGapAddrType)p_addr.addr_type;

    return retVal;
}

ErrorType FruityHal::BleGapScanStart(BleGapScanParams const &scanParams)
{
    u32 err;
    ble_gap_scan_params_t scan_params;
    CheckedMemset(&scan_params, 0x00, sizeof(ble_gap_scan_params_t));

    scan_params.active = 0;
    scan_params.interval = scanParams.interval;
    scan_params.timeout = scanParams.timeout;
    scan_params.window = scanParams.window;

#if (SDK == 15 || SDK == 16)    
    scan_params.report_incomplete_evts = 0;
    scan_params.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL;
    scan_params.extended = 0;
    scan_params.scan_phys = BLE_GAP_PHY_1MBPS;
    scan_params.timeout = scanParams.timeout * 100; //scanTimeout is now in ms since SDK15 instead of seconds
    CheckedMemset(scan_params.channel_mask, 0, sizeof(ble_gap_ch_mask_t));
#else
    scan_params.adv_dir_report = 0;
    scan_params.use_whitelist = 0;
#endif

#if (SDK == 15 || SDK == 16)
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

static u8 AdvertisingTypeToNrf(FruityHal::BleGapAdvType type)
{
    switch (type)
    {
#if SDK == 15 || SDK == 16
        case FruityHal::BleGapAdvType::ADV_IND:
            return BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
        case FruityHal::BleGapAdvType::ADV_DIRECT_IND:
            return BLE_GAP_ADV_TYPE_CONNECTABLE_NONSCANNABLE_DIRECTED;
        case FruityHal::BleGapAdvType::ADV_SCAN_IND:
            return BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED;
        case FruityHal::BleGapAdvType::ADV_NONCONN_IND:
            return BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED;
#else
        case FruityHal::BleGapAdvType::ADV_IND:
            return BLE_GAP_ADV_TYPE_ADV_IND;
        case FruityHal::BleGapAdvType::ADV_DIRECT_IND:
            return BLE_GAP_ADV_TYPE_ADV_DIRECT_IND;
        case FruityHal::BleGapAdvType::ADV_SCAN_IND:
            return BLE_GAP_ADV_TYPE_ADV_SCAN_IND;
        case FruityHal::BleGapAdvType::ADV_NONCONN_IND:
            return BLE_GAP_ADV_TYPE_ADV_NONCONN_IND;
#endif
        default:
            return 0xFF;
    }
}

ErrorType FruityHal::BleGapAdvStart(u8 * advHandle, BleGapAdvParams const &advParams)
{
    u32 err;
#if (SDK == 15 || SDK == 16)
    // logt("FH", "adv used: %u", (u32)myData.used);
    ble_gap_adv_params_t adv_params;
    CheckedMemset(&adv_params, 0x00, sizeof(adv_params));
    adv_params.channel_mask[4] |= (advParams.channelMask.ch37Off << 5);
    adv_params.channel_mask[4] |= (advParams.channelMask.ch38Off << 6);
    adv_params.channel_mask[4] |= (advParams.channelMask.ch39Off << 7);
    adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
    adv_params.interval = advParams.interval;
    adv_params.p_peer_addr = nullptr;
    adv_params.duration = advParams.timeout * 100;
    adv_params.properties.type = AdvertisingTypeToNrf(advParams.type);

    err = sd_ble_gap_adv_set_configure(
                advHandle,
                &((NrfHalMemory*)GS->halMemory)->advData,
              &adv_params
            );
    logt("FH", "Adv data set (%u) typ %u, iv %u, mask %u, handle %u", err, adv_params.properties.type, (u32)adv_params.interval, adv_params.channel_mask[4], *advHandle);
    if (err != NRF_SUCCESS) return nrfErrToGeneric(err);

    err = sd_ble_gap_adv_start(*advHandle, BLE_CONN_CFG_TAG_FM);
    logt("FH", "Adv start (%u)", err);
#else      
    ble_gap_adv_params_t adv_params;
    adv_params.channel_mask.ch_37_off = advParams.channelMask.ch37Off;
    adv_params.channel_mask.ch_38_off = advParams.channelMask.ch38Off;
    adv_params.channel_mask.ch_39_off= advParams.channelMask.ch39Off;
    adv_params.fp = BLE_GAP_ADV_FP_ANY;
    adv_params.interval = advParams.interval;
    adv_params.p_peer_addr = nullptr;
    adv_params.timeout = advParams.timeout;
    adv_params.type = AdvertisingTypeToNrf(advParams.type);
    err = sd_ble_gap_adv_start(&adv_params, BLE_CONN_CFG_TAG_FM);
    logt("FH", "Adv start (%u) typ %u, iv %u, mask %u", err, adv_params.type, adv_params.interval, *((u8*)&adv_params.channel_mask));
#endif // (SDK == 15 || SDK == 16)
    return nrfErrToGeneric(err);
}

ErrorType FruityHal::BleGapAdvDataSet(u8 * p_advHandle, u8 *advData, u8 advDataLength, u8 *scanData, u8 scanDataLength)
{
    u32 err = 0;
#if (SDK == 15 || SDK == 16)
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    halMemory->advData.adv_data.p_data = (u8 *)advData;
    halMemory->advData.adv_data.len = advDataLength;
    halMemory->advData.scan_rsp_data.p_data = (u8 *)scanData;
    halMemory->advData.scan_rsp_data.len = scanDataLength;
    if (*p_advHandle != BLE_GAP_ADV_SET_HANDLE_NOT_SET)
    {
        err = sd_ble_gap_adv_set_configure(
                    p_advHandle,
                    &halMemory->advData,
                    nullptr
                );
        logt("FH", "Adv data set (%u) handle %u", err, *p_advHandle);
    }
#else
    err = sd_ble_gap_adv_data_set(
                advData,
                advDataLength,
                scanData,
                scanDataLength
            );

    logt("FH", "Adv data set (%u)", err);
#endif // (SDK == 15 || SDK == 16)
    return nrfErrToGeneric(err);
}

ErrorType FruityHal::BleGapAdvStop(u8 advHandle)
{
    u32 err;
#if (SDK == 15 || SDK == 16)
    err = sd_ble_gap_adv_stop(advHandle);
#else
    err = sd_ble_gap_adv_stop();
#endif
    logt("FH", "Adv stop (%u)", err);
    return nrfErrToGeneric(err);
}

ErrorType FruityHal::BleGapConnect(FruityHal::BleGapAddr const &peerAddress, BleGapScanParams const &scanParams, BleGapConnParams const &connectionParams)
{
    u32 err;
    ble_gap_addr_t p_peer_addr;
    CheckedMemset(&p_peer_addr, 0x00, sizeof(p_peer_addr));
    p_peer_addr.addr_type = (u8)peerAddress.addr_type;
    CheckedMemcpy(p_peer_addr.addr, peerAddress.addr.data(), sizeof(peerAddress.addr));

    ble_gap_scan_params_t p_scan_params;
    CheckedMemset(&p_scan_params, 0x00, sizeof(p_scan_params));
    p_scan_params.active = 0;
    p_scan_params.interval = scanParams.interval;
    p_scan_params.timeout = scanParams.timeout;
    p_scan_params.window = scanParams.window;

#if (SDK == 15 || SDK == 16)    
    p_scan_params.report_incomplete_evts = 0;
    p_scan_params.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL;
    p_scan_params.extended = 0;
    p_scan_params.scan_phys = BLE_GAP_PHY_1MBPS;
    p_scan_params.timeout = scanParams.timeout * 100; //scanTimeout is now in ms since SDK15 instead of seconds
    CheckedMemset(p_scan_params.channel_mask, 0, sizeof(p_scan_params.channel_mask));
#else
    p_scan_params.adv_dir_report = 0;
    p_scan_params.use_whitelist = 0;
#endif

    ble_gap_conn_params_t p_conn_params;
    CheckedMemset(&p_conn_params, 0x00, sizeof(p_conn_params));
    p_conn_params.conn_sup_timeout = connectionParams.connSupTimeout;
    p_conn_params.max_conn_interval = connectionParams.maxConnInterval;
    p_conn_params.min_conn_interval = connectionParams.minConnInterval;
    p_conn_params.slave_latency = connectionParams.slaveLatency;

    err = sd_ble_gap_connect(&p_peer_addr, &p_scan_params, &p_conn_params, BLE_CONN_CFG_TAG_FM);

    logt("FH", "Connect (%u) iv:%u, tmt:%u", err, p_conn_params.min_conn_interval, p_scan_params.timeout);

    //Tell our ScanController, that scanning has stopped
    GS->scanController.ScanningHasStopped();

    return nrfErrToGeneric(err);
}


ErrorType FruityHal::ConnectCancel()
{
    u32 err = sd_ble_gap_connect_cancel();

    logt("FH", "Connect Cancel (%u)", err);

    return nrfErrToGeneric(err);
}

ErrorType FruityHal::Disconnect(u16 conn_handle, FruityHal::BleHciError hci_status_code)
{
    u32 err = sd_ble_gap_disconnect(conn_handle, (u8)hci_status_code);

    logt("FH", "Disconnect (%u)", err);

    return nrfErrToGeneric(err);
}

ErrorType FruityHal::BleTxPacketCountGet(u16 connectionHandle, u8* count)
{
//TODO: must be read from somewhere else
    *count = BLE_CONN_CFG_GAP_PACKET_BUFFERS;
    return ErrorType::SUCCESS;
}

ErrorType FruityHal::BleGapNameSet(const BleGapConnSecMode & mode, u8 const * p_dev_name, u16 len)
{
    ble_gap_conn_sec_mode_t sec_mode;
    CheckedMemset(&sec_mode, 0, sizeof(sec_mode));
    sec_mode.sm = mode.securityMode;
    sec_mode.lv = mode.level;
    return nrfErrToGeneric(sd_ble_gap_device_name_set(&sec_mode, p_dev_name, len));
}

ErrorType FruityHal::BleGapAppearance(BleAppearance appearance)
{
    return nrfErrToGeneric(sd_ble_gap_appearance_set((u32)appearance));
}

ble_gap_conn_params_t translate(FruityHal::BleGapConnParams const & params)
{
    ble_gap_conn_params_t gapConnectionParams;
    CheckedMemset(&gapConnectionParams, 0, sizeof(gapConnectionParams));

    gapConnectionParams.min_conn_interval = params.minConnInterval;
    gapConnectionParams.max_conn_interval = params.maxConnInterval;
    gapConnectionParams.slave_latency = params.slaveLatency;
    gapConnectionParams.conn_sup_timeout = params.connSupTimeout;
    return gapConnectionParams;
}

ErrorType FruityHal::BleGapConnectionParamsUpdate(u16 conn_handle, BleGapConnParams const & params)
{
    ble_gap_conn_params_t gapConnectionParams = translate(params);
    return nrfErrToGeneric(sd_ble_gap_conn_param_update(conn_handle, &gapConnectionParams));
}

#if IS_ACTIVE(CONN_PARAM_UPDATE)
ErrorType FruityHal::BleGapRejectConnectionParamsUpdate(u16 conn_handle)
{
    return nrfErrToGeneric(sd_ble_gap_conn_param_update(conn_handle, nullptr));
}
#endif

ErrorType FruityHal::BleGapConnectionPreferredParamsSet(BleGapConnParams const & params)
{
    ble_gap_conn_params_t gapConnectionParams = translate(params);
    return nrfErrToGeneric(sd_ble_gap_ppcp_set(&gapConnectionParams));
}

ErrorType FruityHal::BleGapSecInfoReply(u16 conn_handle, BleGapEncInfo * p_info_out, u8 * p_id_info, u8 * p_sign_info)
{
    ble_gap_enc_info_t info;
    CheckedMemset(&info, 0, sizeof(info));
    CheckedMemcpy(info.ltk, p_info_out->longTermKey, p_info_out->longTermKeyLength);
    info.lesc = p_info_out->isGeneratedUsingLeSecureConnections ;
    info.auth = p_info_out->isAuthenticatedKey;
    info.ltk_len = p_info_out->longTermKeyLength;

    return nrfErrToGeneric(sd_ble_gap_sec_info_reply(
        conn_handle,
        &info, //This is our stored long term key
        nullptr, //We do not have an identity resolving key
        nullptr //We do not have signing info
    ));
}

ErrorType FruityHal::BleGapEncrypt(u16 conn_handle, BleGapMasterId const & master_id, BleGapEncInfo const & enc_info)
{
    ble_gap_master_id_t keyId;
    CheckedMemset(&keyId, 0, sizeof(keyId));
    keyId.ediv = master_id.encryptionDiversifier;
    CheckedMemcpy(keyId.rand, master_id.rand, BLE_GAP_SEC_RAND_LEN);

    ble_gap_enc_info_t info;
    CheckedMemset(&info, 0, sizeof(info));
    CheckedMemcpy(info.ltk, enc_info.longTermKey, enc_info.longTermKeyLength);
    info.lesc = enc_info.isGeneratedUsingLeSecureConnections ;
    info.auth = enc_info.isAuthenticatedKey;
    info.ltk_len = enc_info.longTermKeyLength;

    return nrfErrToGeneric(sd_ble_gap_encrypt(conn_handle, &keyId, &info));
}

ErrorType FruityHal::BleGapRssiStart(u16 conn_handle, u8 threshold_dbm, u8 skip_count)
{
    return nrfErrToGeneric(sd_ble_gap_rssi_start(conn_handle, threshold_dbm, skip_count));
}

ErrorType FruityHal::BleGapRssiStop(u16 conn_handle)
{
    return nrfErrToGeneric(sd_ble_gap_rssi_stop(conn_handle));
}



/*
    #####################
    ###               ###
    ###      GATT     ###
    ###               ###
    #####################
*/


#define __________________GATT____________________


#ifndef SIM_ENABLED
FruityHal::DBDiscoveryHandler dbDiscoveryHandler = nullptr;

static void DatabaseDiscoveryHandler(ble_db_discovery_evt_t * p_evt)
{
    // other events are not supported
    if (!(p_evt->evt_type == BLE_DB_DISCOVERY_SRV_NOT_FOUND ||
          p_evt->evt_type == BLE_DB_DISCOVERY_COMPLETE)) return;

    FruityHal::BleGattDBDiscoveryEvent bleDbEvent;
    CheckedMemset(&bleDbEvent, 0x00, sizeof(bleDbEvent));
    bleDbEvent.connHandle = p_evt->conn_handle;
    bleDbEvent.type = p_evt->evt_type == BLE_DB_DISCOVERY_COMPLETE ? FruityHal::BleGattDBDiscoveryEventType::COMPLETE : FruityHal::BleGattDBDiscoveryEventType::SERVICE_NOT_FOUND;
    bleDbEvent.serviceUUID.uuid = p_evt->params.discovered_db.srv_uuid.uuid;
    bleDbEvent.serviceUUID.type = p_evt->params.discovered_db.srv_uuid.type;
    bleDbEvent.charateristicsCount = p_evt->params.discovered_db.char_count;
    for (u8 i = 0; i < bleDbEvent.charateristicsCount; i++)
    {
      bleDbEvent.dbChar[i].handleValue = p_evt->params.discovered_db.charateristics[i].characteristic.handle_value;
      bleDbEvent.dbChar[i].charUUID.uuid = p_evt->params.discovered_db.charateristics[i].characteristic.uuid.uuid;
      bleDbEvent.dbChar[i].charUUID.type = p_evt->params.discovered_db.charateristics[i].characteristic.uuid.type;
      bleDbEvent.dbChar[i].cccdHandle = p_evt->params.discovered_db.charateristics[i].cccd_handle;
    }
    logt("ERROR", "########################BLE DB EVENT");
    dbDiscoveryHandler(&bleDbEvent);
}
#endif //SIM_ENABLED

ErrorType FruityHal::DiscovereServiceInit(DBDiscoveryHandler dbEventHandler)
{
#ifndef  SIM_ENABLED
    dbDiscoveryHandler = dbEventHandler;

#if (SDK == 16)

    ble_db_discovery_init_t db_init;

    memset(&db_init, 0, sizeof(ble_db_discovery_init_t));

    db_init.evt_handler  = DatabaseDiscoveryHandler;
    db_init.p_gatt_queue = &m_ble_gatt_queue;

    return nrfErrToGeneric(ble_db_discovery_init(&db_init));

#else

    return nrfErrToGeneric(ble_db_discovery_init(DatabaseDiscoveryHandler));

#endif

#else
    GS->dbDiscoveryHandler = dbEventHandler;
#endif
    return ErrorType::SUCCESS;
}

ErrorType FruityHal::DiscoverService(u16 connHandle, const BleGattUuid &p_uuid)
{
    ErrorType err = ErrorType::SUCCESS;
    ble_uuid_t uuid;
    CheckedMemset(&uuid, 0, sizeof(uuid));
    uuid.uuid = p_uuid.uuid;
    uuid.type = p_uuid.type;
#ifndef SIM_ENABLED
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    CheckedMemset(&halMemory->discoveredServices, 0x00, sizeof(halMemory->discoveredServices));
    err = nrfErrToGeneric(ble_db_discovery_evt_register(&uuid));
    if (err != ErrorType::SUCCESS) {
        logt("ERROR", "err %u", (u32)err);
        return err;
    }

    err = nrfErrToGeneric(ble_db_discovery_start(&halMemory->discoveredServices, connHandle));
    if (err != ErrorType::SUCCESS) {
        logt("ERROR", "err %u", (u32)err);
        return err;
    }
#else
    cherrySimInstance->StartServiceDiscovery(connHandle, uuid, 1000);
#endif
    return err;
}

bool FruityHal::DiscoveryIsInProgress()
{
#ifndef SIM_ENABLED
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    return halMemory->discoveredServices.discovery_in_progress;
#else
    return sd_currently_in_discovery();
#endif
}

ErrorType FruityHal::BleGattSendNotification(u16 connHandle, BleGattWriteParams & params)
{    
    ble_gatts_hvx_params_t notificationParams;
    CheckedMemset(&notificationParams, 0, sizeof(ble_gatts_hvx_params_t));
    notificationParams.handle = params.handle;
    notificationParams.offset = params.offset;
    notificationParams.p_data = params.p_data;
    notificationParams.p_len = &params.len.GetRawRef();

    if (params.type == BleGattWriteType::NOTIFICATION) notificationParams.type = BLE_GATT_HVX_NOTIFICATION;
    else if (params.type == BleGattWriteType::INDICATION) notificationParams.type = BLE_GATT_HVX_INDICATION;
    else return ErrorType::INVALID_PARAM;
    
    ErrorType retVal = nrfErrToGeneric(sd_ble_gatts_hvx(connHandle, &notificationParams));

    logt("FH", "BleGattSendNotification(%u)", (u32)retVal);

    return retVal;
}

ErrorType FruityHal::BleGattWrite(u16 connHandle, BleGattWriteParams const & params)
{
    ble_gattc_write_params_t writeParameters;
    CheckedMemset(&writeParameters, 0, sizeof(ble_gattc_write_params_t));
    writeParameters.handle = params.handle;
    writeParameters.offset = params.offset;
    writeParameters.len = params.len.GetRaw();
    writeParameters.p_value = params.p_data;

    if (params.type == BleGattWriteType::WRITE_REQ) writeParameters.write_op = BLE_GATT_OP_WRITE_REQ;
    else if (params.type == BleGattWriteType::WRITE_CMD) writeParameters.write_op = BLE_GATT_OP_WRITE_CMD;
    else return ErrorType::INVALID_PARAM;

    return nrfErrToGeneric(sd_ble_gattc_write(connHandle, &writeParameters));    
}

ErrorType FruityHal::BleUuidVsAdd(u8 const * p_vs_uuid, u8 * p_uuid_type)
{
    ble_uuid128_t vs_uuid;
    CheckedMemset(&vs_uuid, 0, sizeof(vs_uuid));
    CheckedMemcpy(vs_uuid.uuid128, p_vs_uuid, sizeof(vs_uuid));
    return nrfErrToGeneric(sd_ble_uuid_vs_add(&vs_uuid, p_uuid_type));
}

ErrorType FruityHal::BleGattServiceAdd(BleGattSrvcType type, BleGattUuid const & p_uuid, u16 * p_handle)
{
    ble_uuid_t uuid;
    CheckedMemset(&uuid, 0, sizeof(uuid));
    uuid.uuid = p_uuid.uuid;
    uuid.type = p_uuid.type;
    return nrfErrToGeneric(sd_ble_gatts_service_add((u8)type, &uuid, p_handle));
}

ErrorType FruityHal::BleGattCharAdd(u16 service_handle, BleGattCharMd const & char_md, BleGattAttribute const & attr_char_value, BleGattCharHandles & handles)
{
    ble_gatts_char_md_t sd_char_md;
    ble_gatts_attr_t sd_attr_char_value;
    
    static_assert(SDK <= 16, "Check mapping");

    CheckedMemcpy(&sd_char_md, &char_md, sizeof(ble_gatts_char_md_t));
    CheckedMemcpy(&sd_attr_char_value, &attr_char_value, sizeof(ble_gatts_attr_t));
    return nrfErrToGeneric(sd_ble_gatts_characteristic_add(service_handle, &sd_char_md, &sd_attr_char_value, (ble_gatts_char_handles_t *)&handles));
}

ErrorType FruityHal::BleGapDataLengthExtensionRequest(u16 connHandle)
{
#if defined (NRF52) || defined (SIM_ENABLED)
    //We let the SoftDevice decide the maximum according to the -NRF_SDH_BLE_GATT_MAX_MTU_SIZE and connection configuration
    ErrorType err = nrfErrToGeneric(sd_ble_gap_data_length_update(connHandle, nullptr, nullptr));
    logt("FH", "Start DLE Update (%u) on conn %u", (u32)err, connHandle);

    return err;
#else
    //TODO: We should implement DLE in the Simulator as soon as it is based on the NRF52

    return ErrorType::NOT_SUPPORTED;
#endif
}

u32 FruityHal::BleGattGetMaxMtu()
{
#ifdef SIM_ENABLED
    return 63;
#else
    return NRF_SDH_BLE_GATT_MAX_MTU_SIZE; //MTU for nRF52 is defined through sdk_config.h
#endif
}

ErrorType FruityHal::BleGattMtuExchangeRequest(u16 connHandle, u16 clientRxMtu)
{
#if defined (NRF52) || defined (SIM_ENABLED)
    u32 err = sd_ble_gattc_exchange_mtu_request(connHandle, clientRxMtu);

    logt("FH", "Start MTU Exchange (%u) on conn %u with %u", err, connHandle, clientRxMtu);

    return nrfErrToGeneric(err);
#else
    return ErrorType::NOT_SUPPORTED;
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
          tx_power != -8  && 
          tx_power != -4  && 
          tx_power != 0   && 
          tx_power != 4) {
        SIMEXCEPTION(IllegalArgumentException);
        return ErrorType::INVALID_PARAM;
    }

    u32 err;
#if (SDK == 15 || SDK == 16)
    u8 txRole;
    if (role == TxRole::CONNECTION) txRole = BLE_GAP_TX_POWER_ROLE_CONN;
    else if (role == TxRole::ADVERTISING) txRole = BLE_GAP_TX_POWER_ROLE_ADV;
    else if (role == TxRole::SCAN_INIT) txRole = BLE_GAP_TX_POWER_ROLE_SCAN_INIT;
    else return ErrorType::INVALID_PARAM;;
    err = sd_ble_gap_tx_power_set(txRole, handle, tx_power);
#else
    err = sd_ble_gap_tx_power_set(tx_power);
#endif

    return nrfErrToGeneric(err);
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

ErrorType FruityHal::WaitForEvent()
{
    return nrfErrToGeneric(sdAppEvtWaitAnomaly87());
}

ErrorType FruityHal::InitializeButtons()
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
#if SDK == 15 || SDK == 16
    buttonConfig.skip_gpio_setup = 0;
#endif

    //This uses the SENSE low power feature, all pin events are reported
    //at the same GPIOTE channel
    err =  nrf_drv_gpiote_in_init(Boardconfig->button1Pin, &buttonConfig, button_interrupt_handler);

    //Enable the events
    nrf_drv_gpiote_in_event_enable(Boardconfig->button1Pin, true);
#endif

    return nrfErrToGeneric(err);
}

ErrorType FruityHal::GetRandomBytes(u8 * p_data, u8 len)
{
    return nrfErrToGeneric(sd_rand_application_vector_get(p_data, len));
}

//################################################
#define _________________VIRTUAL_COM_PORT_____________________

#if IS_ACTIVE(VIRTUAL_COM_PORT)
static_assert(TERMINAL_READ_BUFFER_LENGTH == VIRTUAL_COM_LINE_BUFFER_SIZE, "Buffer sizes should match");
#endif

void FruityHal::VirtualComInitBeforeStack()
{
#if IS_ACTIVE(VIRTUAL_COM_PORT)
    virtualComInit();
#endif
}
void FruityHal::VirtualComInitAfterStack(void (*portEventHandler)(bool))
{
#if IS_ACTIVE(VIRTUAL_COM_PORT)
    virtualComStart(portEventHandler);
#endif
}
void FruityHal::VirtualComProcessEvents()
{
#if IS_ACTIVE(VIRTUAL_COM_PORT)
    virtualComCheck();
#endif
}
ErrorType FruityHal::VirtualComCheckAndProcessLine(u8* buffer, u16 bufferLength)
{
#if IS_ACTIVE(VIRTUAL_COM_PORT)
    return nrfErrToGeneric(virtualComCheckAndProcessLine(buffer, bufferLength));
#else
    return ErrorType::SUCCESS;
#endif
}
void FruityHal::VirtualComWriteData(const u8* data, u16 dataLength)
{
#if IS_ACTIVE(VIRTUAL_COM_PORT)
    virtualComWriteData(data, dataLength);
#endif
}

//################################################
#define _________________TIMERS___________________

extern "C"{
    static const u32 TICKS_PER_DS_TIMES_TEN = 32768;

    void app_timer_handler(void * p_context){
        UNUSED_PARAMETER(p_context);
        
        // This line must be kept to provide recalculations of global time
        FruityHal::GetRtcMs();

        GS->timestampInAppTimerHandler = FruityHal::GetRtcMs();

        //We just increase the time that has passed since the last handler
        //And call the timer from our main event handling queue
        GS->tickRemainderTimesTen += ((u32)MAIN_TIMER_TICK) * 10;
        u32 passedDs = GS->tickRemainderTimesTen / TICKS_PER_DS_TIMES_TEN;
        GS->tickRemainderTimesTen -= passedDs * TICKS_PER_DS_TIMES_TEN;
        GS->passsedTimeSinceLastTimerHandlerDs += passedDs;

        FruityHal::SetPendingEventIRQ();

        GS->timeManager.AddTicks(MAIN_TIMER_TICK);

    }
}

ErrorType FruityHal::InitTimers()
{
    SIMEXCEPTION(NotImplementedException);
#if defined(NRF52)
    uint32_t ret = app_timer_init();
    return nrfErrToGeneric(ret);
#endif
    return ErrorType::SUCCESS;
}

ErrorType FruityHal::StartTimers()
{
    SIMEXCEPTION(NotImplementedException);
    u32 err = 0;
#ifndef SIM_ENABLED

    APP_TIMER_DEF(mainTimerMsId);

    err = app_timer_create(&mainTimerMsId, APP_TIMER_MODE_REPEATED, app_timer_handler);
    if (err != NRF_SUCCESS) return nrfErrToGeneric(err);

    err = app_timer_start(mainTimerMsId, MAIN_TIMER_TICK, nullptr);
#endif // SIM_ENABLED
    return nrfErrToGeneric(err);
}

ErrorType FruityHal::CreateTimer(FruityHal::swTimer &timer, bool repeated, TimerHandler handler)
{    
    SIMEXCEPTION(NotImplementedException);
#ifndef SIM_ENABLED

    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    timer = (u32 *)&halMemory->swTimers[halMemory->timersCreated];
    CheckedMemset(timer, 0x00, sizeof(app_timer_t));

    app_timer_mode_t mode = repeated ? APP_TIMER_MODE_REPEATED : APP_TIMER_MODE_SINGLE_SHOT;
    
    u32 err = app_timer_create((app_timer_id_t *)(&timer), mode, handler);
    if (err != NRF_SUCCESS) return nrfErrToGeneric(err);

    halMemory->timersCreated++;
#endif
    return ErrorType::SUCCESS;
}

ErrorType FruityHal::StartTimer(FruityHal::swTimer timer, u32 timeoutMs)
{
    SIMEXCEPTION(NotImplementedException);
#ifndef SIM_ENABLED
    if (timer == nullptr) return ErrorType::INVALID_PARAM;

    u32 err = app_timer_start((app_timer_id_t)timer, APP_TIMER_TICKS(timeoutMs), NULL);
    return nrfErrToGeneric(err);
#else
    return ErrorType::SUCCESS;
#endif
}

ErrorType FruityHal::StopTimer(FruityHal::swTimer timer)
{
    SIMEXCEPTION(NotImplementedException);
#ifndef SIM_ENABLED
    if (timer == nullptr) return ErrorType::INVALID_PARAM;

    u32 err = app_timer_stop((app_timer_id_t)timer);
    return nrfErrToGeneric(err);
#else
    return ErrorType::SUCCESS;
#endif
}

u32 FruityHal::GetRtcMs()
{
    uint32_t rtcTicks;
    rtcTicks = app_timer_cnt_get();
    rtcTicks %= UINT16_MAX;
    if (rtcTicks > (UINT16_MAX / 2))
    {
        ((NrfHalMemory*)GS->halMemory)->overflowPending = true;
    }
    else
    {
        static_assert(APP_TIMER_CONFIG_RTC_FREQUENCY == 0, "Please update calculation");
        // 65535 (UINT16_MAX) * 1000 / 32768 (APP_TIMER_CLOCK_FREQ) = 1999,97 . This means error of 0,03ms is accumulated.
        if (((NrfHalMemory*)GS->halMemory)->overflowPending == true)
        {
            ((NrfHalMemory*)GS->halMemory)->time_ms += 2000;
            ((NrfHalMemory*)GS->halMemory)->overflowPending = false;
        } 
    }
    return ((rtcTicks * 1000) / APP_TIMER_CLOCK_FREQ) + ((NrfHalMemory*)GS->halMemory)->time_ms;
}

u32 FruityHal::GetRtcDifferenceMs(u32 nowTimeMs, u32 previousTimeMs)
{
    return nowTimeMs - previousTimeMs;
}

//################################################
#define _____________FAULT_HANDLERS_______________

//These error handlers are declared WEAK in the nRF SDK and can be implemented here
//Will be called if an error occurs somewhere in the code, but not if it's a hardfault
extern "C"
{
    //The app_error handler_bare is called by all FRUITYMESH_ERROR_CHECK functions when DEBUG is undefined
    void app_error_handler_bare(uint32_t error_code)
    {
        GS->appErrorHandler((u32)error_code);
    }

    //The app_error handler is called by all FRUITYMESH_ERROR_CHECK functions when DEBUG is defined
    void app_error_handler(uint32_t error_code, uint32_t line_num, const u8 * p_file_name)
    {
        app_error_handler_bare(error_code);
        logt("ERROR", "App error code:%s(%u), file:%s, line:%u", Logger::GetGeneralErrorString((ErrorType)error_code), (u32)error_code, p_file_name, (u32)line_num);
    }

#ifndef SIM_ENABLED
    //Called when the softdevice crashes
    void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
    {
        ::BleStackErrorHandler(id, pc, info);
    }

    //We use the nordic hardfault handler that stacks all fault variables for us before calling this function
    __attribute__((used)) void HardFault_c_handler(stacked_regs_t* stack)
    {
        HardFaultErrorHandler(stack);
    }
#endif

    //NRF52 uses more handlers, we currently just reboot if they are called
    //TODO: Redirect to hardfault handler, but be aware that the stack will shift by calling the function
#ifdef NRF52
    __attribute__((used)) void MemoryManagement_Handler(){
        GS->ramRetainStructPtr->rebootReason = RebootReason::MEMORY_MANAGEMENT;
        NVIC_SystemReset();
    }
    __attribute__((used)) void BusFault_Handler(){
        GS->ramRetainStructPtr->rebootReason = RebootReason::BUS_FAULT;
        NVIC_SystemReset();
    }
    __attribute__((used)) void UsageFault_Handler(){
        GS->ramRetainStructPtr->rebootReason = RebootReason::USAGE_FAULT;
        NVIC_SystemReset();
    }
#endif



}

//################################################
#define __________________BOOTLOADER____________________

u32 FruityHal::GetBootloaderVersion()
{
    if(BOOTLOADER_UICR_ADDRESS != 0xFFFFFFFF){
        return *(u32*)(FLASH_REGION_START_ADDRESS + BOOTLOADER_UICR_ADDRESS + 1024);
    } else {
        return 0;
    }
}

u32 FruityHal::GetBootloaderAddress()
{
#ifndef SIM_ENABLED
    return BOOTLOADER_UICR_ADDRESS;
#else
    if (BOOTLOADER_UICR_ADDRESS == 0xFFFFFFFF) {
        return 0xFFFFFFFF;
    }
    else {
        return FLASH_REGION_START_ADDRESS + BOOTLOADER_UICR_ADDRESS;
    }
#endif
}

void FruityHal::ActivateBootloaderOnReset()
{
    logt("DFUMOD", "Setting flags for nRF Secure DFU Bootloader");

    //Write a magic number into the retained register that will persists over reboots
    ClearGeneralPurposeRegister(0, 0xffffffff);
    WriteGeneralPurposeRegister(0, BOOTLOADER_DFU_START);

    ClearGeneralPurposeRegister(1, 0xffffffff);
    WriteGeneralPurposeRegister(1, BOOTLOADER_DFU_START2);

    // => After rebooting, the bootloader will check this register and will start the DFU process
}

//################################################
#define __________________MISC____________________


void FruityHal::SystemReset()
{
    sd_nvic_SystemReset();
}

void FruityHal::SystemReset(bool softdeviceEnabled)
{
    if (softdeviceEnabled) 
        sd_nvic_SystemReset();
    else
        NVIC_SystemReset();
}

void FruityHal::SystemEnterOff(bool softdeviceEnabled)
{
    if (softdeviceEnabled)
    {
        sd_power_system_off();
    }
    else
    {
        nrf_power_system_off();
    }
}

// Retrieves the reboot reason from the RESETREAS register
RebootReason FruityHal::GetRebootReason()
{
#ifndef SIM_ENABLED
    u32 reason = NRF_POWER->RESETREAS;

    //Pin reset must be checked first because of errata [136] System: Bits in RESETREAS are set when they should not be
    if (reason & POWER_RESETREAS_RESETPIN_Msk) {
        return RebootReason::PIN_RESET;
    } else if (reason & POWER_RESETREAS_DOG_Msk) {
        return RebootReason::WATCHDOG;
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
ErrorType FruityHal::ClearRebootReason()
{
    return nrfErrToGeneric(sd_power_reset_reason_clr(0xFFFFFFFFUL));
}



u32 ClearGeneralPurposeRegister(u32 gpregId, u32 mask)
{
#ifdef NRF52
    return sd_power_gpregret_clr(gpregId, mask);
#else
    return NRF_SUCCESS;
#endif
}

u32 WriteGeneralPurposeRegister(u32 gpregId, u32 mask)
{
#ifdef NRF52
    return sd_power_gpregret_set(gpregId, mask);
#else
    return NRF_SUCCESS;
#endif
}

bool FruityHal::SetRetentionRegisterTwo(u8 val)
{
#ifdef NRF52
    nrf_power_gpregret2_set(val);
    return true;
#else
    return false;
#endif
}

#ifndef SIM_ENABLED
extern "C"{
    void WDT_IRQHandler(void)
    {
        if (nrf_wdt_int_enable_check(NRF_WDT_INT_TIMEOUT_MASK) == true)
        {
            u32 currentTime = FruityHal::GetRtcMs();
            if (currentTime - GS->lastSendTimestamp > 30 * 60 * 1000) *GS->watchdogExtraInfoFlagsPtr |= 1 << 0;
            if (currentTime - GS->lastReceivedTimestamp > 30 * 60 * 1000) *GS->watchdogExtraInfoFlagsPtr |= 1 << 1;
            if (GS->fruityMeshBooted)  *GS->watchdogExtraInfoFlagsPtr |= 1 << 2;
            if (GS->modulesBooted) *GS->watchdogExtraInfoFlagsPtr |= 1 << 3;
            if (currentTime - GS->timestampInAppTimerHandler > 60 * 1000) *GS->watchdogExtraInfoFlagsPtr |= 1 << 4;
            if (currentTime - GS->lastReceivedFromSinkTimestamp > 30 * 60 * 1000) *GS->watchdogExtraInfoFlagsPtr |= 1 << 5;
            if (currentTime - GS->eventLooperTriggerTimestamp > 60 * 1000) *GS->watchdogExtraInfoFlagsPtr |= 1 << 6;
            if (currentTime - GS->fruitymeshEventLooperTriggerTimestamp > 10 * 60 * 1000) *GS->watchdogExtraInfoFlagsPtr |= 1 << 7;
            if (currentTime - GS->bleEventLooperTriggerTimestamp > 10 * 60 * 1000) *GS->watchdogExtraInfoFlagsPtr |= 1 << 8;
            if (currentTime - GS->socEventLooperTriggerTimestamp > 10 * 60 * 1000) *GS->watchdogExtraInfoFlagsPtr |= 1 << 9;

            BaseConnections connections = GS->cm.GetBaseConnections(ConnectionDirection::INVALID);
            u8 handshakedConnections = 0;
            u8 meshaccessConnections = 0;
            for(int i=0; i<connections.count; i++){
                BaseConnectionHandle handle = connections.handles[i];
                if (handle)
                {
                    ConnectionState cs = handle.GetConnectionState();
                    if (cs == ConnectionState::HANDSHAKE_DONE) 
                    {
                        if (handle.GetConnection()->connectionType == ConnectionType::FRUITYMESH)
                        {
                            handshakedConnections++;
                        }
                        else if (handle.GetConnection()->connectionType == ConnectionType::MESH_ACCESS)
                        {
                            meshaccessConnections++;
                        }
                    }
                }
            }

            // Requires 3 bits at maximum
            *GS->watchdogExtraInfoFlagsPtr |= handshakedConnections << 10;

            // Requires 2 bits at maximum
            *GS->watchdogExtraInfoFlagsPtr |= meshaccessConnections << 13;

            if (GS->inGetRandomLoop) *GS->watchdogExtraInfoFlagsPtr |= 1 << 15;
            if (GS->inPullEventsLoop) *GS->watchdogExtraInfoFlagsPtr |= 1 << 16;
            if (GS->safeBootEnabled) *GS->watchdogExtraInfoFlagsPtr |= 1 << 17;

            nrf_wdt_event_clear(NRF_WDT_EVENT_TIMEOUT);
        }
    }
}
#endif //SIM_ENABLED

//Starts the Watchdog with a static interval so that changing a config can do no harm
void FruityHal::StartWatchdog(bool safeBoot)
{    
    if (GET_WATCHDOG_TIMEOUT() == 0) return;

    //Configure Watchdog to default: Run while CPU sleeps
    nrf_wdt_behaviour_set(NRF_WDT_BEHAVIOUR_RUN_SLEEP);
    //Configure Watchdog timeout
    if (!safeBoot) {
        nrf_wdt_reload_value_set(GET_WATCHDOG_TIMEOUT());
    }
    else {
        nrf_wdt_reload_value_set(GET_WATCHDOG_TIMEOUT_SAFE_BOOT());
    }
    // Configure Reload Channels
    nrf_wdt_reload_request_enable(NRF_WDT_RR0);

#ifndef SIM_ENABLED
    // Configure interrupt
    NVIC_SetPriority(WDT_IRQn, 3);
    NVIC_ClearPendingIRQ(WDT_IRQn);
    NVIC_EnableIRQ(WDT_IRQn);
    nrf_wdt_int_enable(NRF_WDT_INT_TIMEOUT_MASK);
#endif //SIM_ENABLED

    //Enable
    nrf_wdt_task_trigger(NRF_WDT_TASK_START);

    logt("FH", "Watchdog started");
}

//Feeds the Watchdog to keep it quiet
void FruityHal::FeedWatchdog()
{
#ifdef SIM_ENABLED
    logt("FH", "Feeding Watchdog");
#endif

    if (GET_WATCHDOG_TIMEOUT() != 0)
    {
        nrf_wdt_reload_request_set(NRF_WDT_RR0);
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
    nrf_delay_ms(delayMs);
}

void FruityHal::EcbEncryptBlock(const u8 * p_key, const u8 * p_clearText, u8 * p_cipherText)
{
    nrf_ecb_hal_data_t ecbData;
    CheckedMemset(&ecbData, 0x00, sizeof(ecbData));
    CheckedMemcpy(ecbData.key, p_key, SOC_ECB_KEY_LENGTH);
    CheckedMemcpy(ecbData.cleartext, p_clearText, SOC_ECB_CLEARTEXT_LENGTH);
    //Only returns NRF_SUCCESS
    sd_ecb_block_encrypt(&ecbData);
    CheckedMemcpy(p_cipherText, ecbData.ciphertext, SOC_ECB_CIPHERTEXT_LENGTH);
}

ErrorType FruityHal::FlashPageErase(u32 page)
{
    return nrfErrToGeneric(sd_flash_page_erase(page));
}

ErrorType FruityHal::FlashWrite(u32 * p_addr, u32 * p_data, u32 len)
{
    return nrfErrToGeneric(sd_flash_write((uint32_t *)p_addr, (uint32_t *)p_data, len));
}

void FruityHal::NvicEnableIRQ(u32 irqType)
{
#ifndef SIM_ENABLED
    sd_nvic_EnableIRQ((IRQn_Type)irqType);
#endif
}

void FruityHal::NvicDisableIRQ(u32 irqType)
{
#ifndef SIM_ENABLED
    sd_nvic_DisableIRQ((IRQn_Type)irqType);
#endif
}

void FruityHal::NvicSetPriorityIRQ(u32 irqType, u8 level)
{
#ifndef SIM_ENABLED
    sd_nvic_SetPriority((IRQn_Type)irqType, (uint32_t)level);
#endif
}

void FruityHal::NvicClearPendingIRQ(u32 irqType)
{
#ifndef SIM_ENABLED
    sd_nvic_ClearPendingIRQ((IRQn_Type)irqType);
#endif
}

#ifndef SIM_ENABLED
extern "C"{
//Eliminate Exception overhead when using pure virutal functions
//http://elegantinvention.com/blog/information/smaller-binary-size-with-c-on-baremetal-g/
    void __cxa_pure_virtual() {
        // Must never be called.
        logt("ERROR", "PVF call");
        constexpr u32 pureVirtualFunctionCalledError = 0xF002;
        FRUITYMESH_ERROR_CHECK(pureVirtualFunctionCalledError);
    }
}
#endif


// ######################### GPIO ############################
void FruityHal::GpioConfigureOutput(u32 pin)
{
    nrf_gpio_cfg_output(pin);
}

void FruityHal::GpioConfigureInput(u32 pin, GpioPullMode mode)
{
    nrf_gpio_cfg_input(pin, GenericPullModeToNrf(mode));
}

void FruityHal::GpioConfigureInputSense(u32 pin, GpioPullMode mode, GpioSenseMode sense)
{
    nrf_gpio_cfg_sense_input(pin, GenericPullModeToNrf(mode), GenericSenseModeToNrf(sense));
}

void FruityHal::GpioConfigureDefault(u32 pin)
{
    nrf_gpio_cfg_default(pin);
}

void FruityHal::GpioPinSet(u32 pin)
{
    nrf_gpio_pin_set(pin);

#ifdef SIM_ENABLED
    if ((int8_t)pin == cherrySimInstance->currentNode->gs.boardconf.configuration.led1Pin) cherrySimInstance->currentNode->led1On = true;
    if ((int8_t)pin == cherrySimInstance->currentNode->gs.boardconf.configuration.led2Pin) cherrySimInstance->currentNode->led2On = true;
    if ((int8_t)pin == cherrySimInstance->currentNode->gs.boardconf.configuration.led3Pin) cherrySimInstance->currentNode->led3On = true;
#endif
}

void FruityHal::GpioPinClear(u32 pin)
{
    nrf_gpio_pin_clear(pin);

#ifdef SIM_ENABLED
    if ((int8_t)pin == cherrySimInstance->currentNode->gs.boardconf.configuration.led1Pin) cherrySimInstance->currentNode->led1On = false;
    if ((int8_t)pin == cherrySimInstance->currentNode->gs.boardconf.configuration.led2Pin) cherrySimInstance->currentNode->led2On = false;
    if ((int8_t)pin == cherrySimInstance->currentNode->gs.boardconf.configuration.led3Pin) cherrySimInstance->currentNode->led3On = false;
#endif
}

u32 FruityHal::GpioPinRead(u32 pin)
{
    u32 state = nrf_gpio_pin_read(pin);
    return state;
}

void FruityHal::GpioPinToggle(u32 pin)
{
    nrf_gpio_pin_toggle(pin);

#ifdef SIM_ENABLED
    if ((int8_t)pin == cherrySimInstance->currentNode->gs.boardconf.configuration.led1Pin) cherrySimInstance->currentNode->led1On = !cherrySimInstance->currentNode->led1On;
    if ((int8_t)pin == cherrySimInstance->currentNode->gs.boardconf.configuration.led2Pin) cherrySimInstance->currentNode->led2On = !cherrySimInstance->currentNode->led2On;
    if ((int8_t)pin == cherrySimInstance->currentNode->gs.boardconf.configuration.led3Pin) cherrySimInstance->currentNode->led3On = !cherrySimInstance->currentNode->led2On;
#endif
}

static void GpioteHandler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    for (u8 i = 0; i < MAX_GPIOTE_HANDLERS; i++)
    {
        if (((u32)pin == halMemory->GpioHandler[i].pin) && (halMemory->GpioHandler[i].handler != nullptr))
            halMemory->GpioHandler[i].handler((u32)pin, NrfPolarityToGeneric(action));
    }
}

ErrorType FruityHal::GpioConfigureInterrupt(u32 pin, FruityHal::GpioPullMode mode, FruityHal::GpioTransistion trigger, FruityHal::GpioInterruptHandler handler)
{
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    if ((handler == nullptr) || (halMemory->gpioteHandlersCreated == MAX_GPIOTE_HANDLERS)) return ErrorType::INVALID_PARAM;

    halMemory->GpioHandler[halMemory->gpioteHandlersCreated].handler = handler;
    halMemory->GpioHandler[halMemory->gpioteHandlersCreated++].pin = pin;
    ErrorType err = ErrorType::SUCCESS;
    nrf_drv_gpiote_in_config_t in_config;
    CheckedMemset(&in_config, 0, sizeof(in_config));
    in_config.is_watcher = false;
    in_config.hi_accuracy = true;
    in_config.pull = GenericPullModeToNrf(mode);
    in_config.sense = GenericPolarityToNrf(trigger);
#if SDK == 15 || SDK == 16
    in_config.skip_gpio_setup = 0;
#endif

    err = nrfErrToGeneric(nrf_drv_gpiote_in_init(pin, &in_config, GpioteHandler));
    nrf_drv_gpiote_in_event_enable(pin, true);

    return err;
}

// ######################### ADC ############################

#ifndef SIM_ENABLED
FruityHal::AdcEventHandler AdcHandler;

#if defined(NRF52)
void SaadcCallback(nrf_drv_saadc_evt_t const * p_event)
{
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE)
    {
        AdcHandler();
    }
}
#else
void AdcCallback(nrf_drv_adc_evt_t const * p_event)
{
    if (p_event->type == NRF_DRV_ADC_EVT_DONE)
    {
        AdcHandler();
    }
}
#endif
#endif //SIM_ENABLED

ErrorType FruityHal::AdcInit(AdcEventHandler handler)
{
#ifndef SIM_ENABLED
    if (handler == nullptr) return ErrorType::INVALID_PARAM;
    AdcHandler = handler;

    ret_code_t err_code;
    err_code = nrf_drv_saadc_init(nullptr,SaadcCallback);
    FRUITYMESH_ERROR_CHECK(err_code);
#endif //SIM_ENABLED
    return ErrorType::SUCCESS;
}

void FruityHal::AdcUninit()
{
#ifndef SIM_ENABLED
    nrf_drv_saadc_uninit();
#endif //SIM_ENABLED
}

#ifndef SIM_ENABLED
#ifdef NRF52
static nrf_saadc_input_t NrfPinToAnalogInput(u32 pin)
{
    switch (pin)
    {
        case 2:
            return NRF_SAADC_INPUT_AIN0;
        case 3:
            return NRF_SAADC_INPUT_AIN1;
        case 4:
            return NRF_SAADC_INPUT_AIN2;
        case 5:
            return NRF_SAADC_INPUT_AIN3;
        case 28:
            return NRF_SAADC_INPUT_AIN4;
        case 29:
            return NRF_SAADC_INPUT_AIN5;
        case 30:
            return NRF_SAADC_INPUT_AIN6;
        case 31:
            return NRF_SAADC_INPUT_AIN7;
        case 0xFF:
            return NRF_SAADC_INPUT_VDD;
        default:
            return NRF_SAADC_INPUT_DISABLED;
    }
}
#else
static nrf_adc_config_input_t NrfPinToAnalogInput(u32 pin)
{
    switch (pin)
    {
        case 26:
            return NRF_ADC_CONFIG_INPUT_0;
        case 27:
            return NRF_ADC_CONFIG_INPUT_1;
        case 1:
            return NRF_ADC_CONFIG_INPUT_2;
        case 2:
            return NRF_ADC_CONFIG_INPUT_3;
        case 3:
            return NRF_ADC_CONFIG_INPUT_4;
        case 4:
            return NRF_ADC_CONFIG_INPUT_5;
        case 5:
            return NRF_ADC_CONFIG_INPUT_6;
        case 6:
            return NRF_ADC_CONFIG_INPUT_7;
        default:
            return NRF_ADC_CONFIG_INPUT_DISABLED;
    }
}
#endif
#endif //SIM_ENABLED

ErrorType FruityHal::AdcConfigureChannel(u32 pin, AdcReference reference, AdcResoultion resolution, AdcGain gain)
{
#ifndef SIM_ENABLED

//#define NRF52
#if defined(NRF52)
    nrf_saadc_resolution_t nrfResolution = resolution == FruityHal::AdcResoultion::ADC_8_BIT ? NRF_SAADC_RESOLUTION_8BIT : NRF_SAADC_RESOLUTION_10BIT;
    nrf_saadc_gain_t nrfGain = NRF_SAADC_GAIN1_6;
    nrf_saadc_reference_t nrfReference = NRF_SAADC_REFERENCE_VDD4;
    switch (reference)
    {
        case FruityHal::AdcReference::ADC_REFERENCE_0_6V:
            nrfReference = NRF_SAADC_REFERENCE_INTERNAL;
            break;
        case FruityHal::AdcReference::ADC_REFERENCE_1_4_POWER_SUPPLY:
            nrfReference = NRF_SAADC_REFERENCE_VDD4;
            break;
        case FruityHal::AdcReference::ADC_REFERENCE_1_2V:
        case FruityHal::AdcReference::ADC_REFERENCE_1_2_POWER_SUPPLY:
        case FruityHal::AdcReference::ADC_REFERENCE_1_3_POWER_SUPPLY:
            return ErrorType::INVALID_PARAM;
    }
    switch (gain)
    {
        case FruityHal::AdcGain::ADC_GAIN_1_6:
            nrfGain = NRF_SAADC_GAIN1_6;
            break;
        case FruityHal::AdcGain::ADC_GAIN_1_5:
            nrfGain = NRF_SAADC_GAIN1_5;
            break;
        case FruityHal::AdcGain::ADC_GAIN_1_4:
            nrfGain = NRF_SAADC_GAIN1_4;
            break;
        case FruityHal::AdcGain::ADC_GAIN_1_3:
            nrfGain = NRF_SAADC_GAIN1_3;
            break;
        case FruityHal::AdcGain::ADC_GAIN_1_2:
            nrfGain = NRF_SAADC_GAIN1_2;
            break;
        case FruityHal::AdcGain::ADC_GAIN_1:
            nrfGain = NRF_SAADC_GAIN1;
            break;
        case FruityHal::AdcGain::ADC_GAIN_2:
            nrfGain = NRF_SAADC_GAIN2;
            break;
        case FruityHal::AdcGain::ADC_GAIN_4:
            nrfGain = NRF_SAADC_GAIN4;
            break;
        case FruityHal::AdcGain::ADC_GAIN_2_3:
            return ErrorType::INVALID_PARAM;
    }
    ret_code_t err_code;
    nrf_saadc_channel_config_t channel_config;

    channel_config = NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NrfPinToAnalogInput(pin));
    channel_config.gain = nrfGain;
    channel_config.reference = nrfReference;
    err_code = nrf_drv_saadc_channel_init(0, &channel_config);
    FRUITYMESH_ERROR_CHECK(err_code);
    nrf_saadc_resolution_set(nrfResolution);
#endif
#endif //SIM_ENABLED
    return ErrorType::SUCCESS;
}

ErrorType FruityHal::AdcSample(i16 & buffer, u8 len)
{
    u32 err = NRF_SUCCESS;
#ifndef SIM_ENABLED

#if defined(NRF52)
    err = nrf_drv_saadc_buffer_convert(&buffer, len);
    if (err == NRF_SUCCESS)
    {
        err = nrf_drv_saadc_sample(); // Non-blocking triggering of SAADC Sampling
    } 
#endif
#endif //SIM_ENABLED
    return nrfErrToGeneric(err);
}

u8 FruityHal::AdcConvertSampleToDeciVoltage(u32 sample)
{
#if defined(NRF52)
constexpr double REF_VOLTAGE_INTERNAL_IN_MILLI_VOLTS = 600; // Maximum Internal Reference Voltage
constexpr double VOLTAGE_DIVIDER_INTERNAL_IN_MILLI_VOLTS = 166; //Internal voltage divider
constexpr double ADC_RESOLUTION_10BIT = 1023;
return sample * REF_VOLTAGE_INTERNAL_IN_MILLI_VOLTS / (VOLTAGE_DIVIDER_INTERNAL_IN_MILLI_VOLTS*ADC_RESOLUTION_10BIT)*10;
#else
constexpr int REF_VOLTAGE_IN_MILLIVOLTS = 1200;
return sample * REF_VOLTAGE_IN_MILLIVOLTS / 1024;
#endif
}

u8 FruityHal::AdcConvertSampleToDeciVoltage(u32 sample, u16 voltageDivider)
{
#if defined(NRF52)
constexpr double REF_VOLTAGE_EXTERNAL_IN_MILLI_VOLTS = 825; // Maximum Internal Reference Voltage
constexpr double VOLTAGE_GAIN_IN_MILLI_VOLTS = 200; //Internal voltage divider
constexpr double ADC_RESOLUTION_10BIT = 1023;
double result = sample * (REF_VOLTAGE_EXTERNAL_IN_MILLI_VOLTS/VOLTAGE_GAIN_IN_MILLI_VOLTS) * (1/ADC_RESOLUTION_10BIT) * (voltageDivider);
return (u8)result;
#else
return 0;
#endif
}

#define __________________CONVERT____________________

u8 FruityHal::ConvertPortToGpio(u8 port, u8 pin)
{
#if defined(NRF52) || defined(SIM_ENABLED)
    return NRF_GPIO_PIN_MAP(port, pin);
#else
    static_assert(false,"Convertion is not yet defined for this board");
#endif
}

void FruityHal::DisableHardwareDfuBootloader()
{
#ifndef SIM_ENABLED
    bool bootloaderAvailable = (FruityHal::GetBootloaderAddress() != 0xFFFFFFFF);
    u32 bootloaderAddress = FruityHal::GetBootloaderAddress();

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

u32 FruityHal::GetMasterBootRecordSize()
{
#ifdef SIM_ENABLED
    return 1024 * 4;
#else
    return MBR_SIZE;
#endif
}

u32 FruityHal::GetSoftDeviceSize()
{
#ifdef SIM_ENABLED
    //Even though the soft device size is not strictly dependent on the chipset, it is a good approximation.
    //These values were measured on real hardware on 26.09.2019.
    switch (GET_CHIPSET())
    {
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

void FruityHal::BleStackErrorHandler(u32 id, u32 info)
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
            CheckedMemcpy(GS->ramRetainStructPtr->stacktrace + 1, ((assert_info_t *)info)->p_file_name, len);
            break;
        }
        case NRF_FAULT_ID_SDK_ERROR: //SDK errors
        {
            GS->ramRetainStructPtr->code2 = ((error_info_t *)info)->line_num;
            GS->ramRetainStructPtr->code3 = ((error_info_t *)info)->err_code;

            //Copy filename to stacktrace
            u8 len = (u8)strlen((const char*)((error_info_t *)info)->p_file_name);
            if (len > (RAM_PERSIST_STACKSTRACE_SIZE - 1) * 4) len = (RAM_PERSIST_STACKSTRACE_SIZE - 1) * 4;
            CheckedMemcpy(GS->ramRetainStructPtr->stacktrace + 1, ((error_info_t *)info)->p_file_name, len);
            break;
        }
    }
}

const char* getBleEventNameString(u16 bleEventId)
{
#if IS_ACTIVE(ENUM_TO_STRING)
    switch (bleEventId)
    {
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
#if defined(NRF52) || defined(SIM_ENABLED)
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

ErrorType FruityHal::GetDeviceConfiguration(DeviceConfiguration & config)
{
    //We are using a magic number to determine if the UICR data present was put there by fruitydeploy
    if (NRF_UICR->CUSTOMER[0] == UICR_SETTINGS_MAGIC_WORD) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
        // The following casts away volatile, which is intended behavior and is okay, as the CUSTOMER data won't change during runtime.
        CheckedMemcpy(&config, (u32*)NRF_UICR->CUSTOMER, sizeof(DeviceConfiguration));
        return ErrorType::SUCCESS;
#pragma GCC diagnostic pop
    }
    else if(GS->recordStorage.IsInit()){
        //On some devices, we are not able to store data in UICR as they are flashed by a 3rd party
        //and we are only updating to fruitymesh. We have a dedicated record for these instances
        //which is used the same as if the data were stored in UICR
        SizedData data = GS->recordStorage.GetRecordData(RECORD_STORAGE_RECORD_ID_UICR_REPLACEMENT);
        if (data.length >= 16 * 4 && ((u32*)data.data)[0] == UICR_SETTINGS_MAGIC_WORD) {
            CheckedMemcpy(&config, (u32*)data.data, sizeof(DeviceConfiguration));
            return ErrorType::SUCCESS;
        }
    }

    return ErrorType::INVALID_STATE;
}

u32 * FruityHal::GetUserMemoryAddress()
{
    return (u32 *)NRF_UICR;
}

u32 * FruityHal::GetDeviceMemoryAddress()
{
    return (u32 *)NRF_FICR;
}

void FruityHal::GetCustomerData(u32 * p_data, u8 len)
{
    for (u8 i = 0; (i < len) && (i < 32); i++)
    {
        p_data[i] = NRF_UICR->CUSTOMER[i];
    }
}

// BLE stack has to be disabled before executing this function
void FruityHal::WriteCustomerData(u32 * p_data, u8 len)
{
#ifndef SIM_ENABLED
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
#endif
    for (u8 i = 0; (i < len) && (i < 32); i++)
    {
        NRF_UICR->CUSTOMER[i] = p_data[i];
    }
#ifndef SIM_ENABLED
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
#endif
}

u32 FruityHal::GetBootloaderSettingsAddress()
{
    return REGION_BOOTLOADER_SETTINGS_START;
}

u32 FruityHal::GetCodePageSize()
{
    return NRF_FICR->CODEPAGESIZE;
}

u32 FruityHal::GetCodeSize()
{
    return NRF_FICR->CODESIZE;
}

u32 FruityHal::GetDeviceId()
{
    return NRF_FICR->DEVICEID[0];
}

void FruityHal::GetDeviceAddress(u8 * p_address)
{
    for (u32 i = 0; i < 8; i++)
    {
        // Not CheckedMemcpy, as DEVICEADDR is volatile.
        p_address[i] = ((const volatile u8*)NRF_FICR->DEVICEADDR)[i];
    }
}

#define __________________UART____________________

static nrf_uart_baudrate_t UartBaudRateToNordic(FruityHal::UartBaudrate baudRate)
{
#ifndef SIM_ENABLED
    switch (baudRate)
    {
        case FruityHal::UartBaudrate::BAUDRATE_1M:
            return (nrf_uart_baudrate_t)NRF_UART_BAUDRATE_1000000;
        case FruityHal::UartBaudrate::BAUDRATE_115200:
            return (nrf_uart_baudrate_t)NRF_UART_BAUDRATE_115200;
        case FruityHal::UartBaudrate::BAUDRATE_38400:
            return (nrf_uart_baudrate_t)NRF_UART_BAUDRATE_38400;
        case FruityHal::UartBaudrate::BAUDRATE_57600:
        default:
            return (nrf_uart_baudrate_t)NRF_UART_BAUDRATE_57600;
    }
#else
    return (nrf_uart_baudrate_t)0;
#endif
}

/*
* FH_NRF_ENABLE_EASYDMA_TERMINAL: FruityHalNrf EasyDMA support for UART
* 
* We implemented the possibility to use UART together with EasyDMA to consume less CPU time when
* logging data through our UART terminal. This has only been tested with our Terminal with TerminalMode::JSON
* 
* The implementation might be usable for other use-cases but further testing is required.
*
* KNOWN ISSUES:
*    - When receiving lots of data during power-on, the controller migh reboot with an app error
*      because functionality will be called before the softdevice was properly enabled.
*    - When receiving many lines in a very short time, data might get lost.
*    - Implementation was only tested with nRf52832
*
* In order to enable this, put the following in yourFeatureset.h:

#include <easydma_terminal_nrf52_fragment.h>

* Also add the following to yourFeatureset.cmake:

include(config/featuresets/CMakeFragments/AddNrfEasyDmaUart.cmake)
*/

#if FH_NRF_ENABLE_EASYDMA_TERMINAL && !defined(SIM_ENABLED)

//This represents a chunk of data being queued for sending using EasyDMA
#define FH_EASYDMA_UART_SERIAL_BUFF_TX_SIZE                 64

//We only ask EasyDMA for a single byte (which is like not using DMA at all for receiving)
//As we will only get an interrupt once the buffer is full which does not work with variable length data
//This is acceptable for us as we do not need a high throughput for receiving data. If we do need this,
//we can rewrite it to use a timer to automatically start a new EasyDMA transfer if no data was received for some time
#define FH_EASYDMA_UART_SERIAL_BUFF_RX_SIZE                 1

//This is the FIFO for sending data, it should be big enough to be able to fit all the data that is being
//logged for one received packet. In the best case, it can fit all data being logged for all packets of a 
//connectionEvent. Only a short memcopy will have to be done by the CPU and the event handling will only be blocked
//for a very short time. Once the TX FIFO is full, any call to logging will block until the data was written out.
#define FH_EASYDMA_UART_SERIAL_FIFO_TX_SIZE                 2048

//The RX buffer can be rather small as we have another RX buffer in our Terminal that is only processed after a full
//line was received
#define FH_EASYDMA_UART_SERIAL_FIFO_RX_SIZE                 256

#define FH_EASYDMA_UART_PARITY          NRF_UART_PARITY_EXCLUDED

// Define our buffers
NRF_SERIAL_BUFFERS_DEF(serial_buffs, FH_EASYDMA_UART_SERIAL_BUFF_TX_SIZE, FH_EASYDMA_UART_SERIAL_BUFF_RX_SIZE);
NRF_SERIAL_QUEUES_DEF(serial_queues, FH_EASYDMA_UART_SERIAL_FIFO_TX_SIZE, FH_EASYDMA_UART_SERIAL_FIFO_RX_SIZE);

//A rather long block
static app_timer_t serial_uart_rx_timer_data = { {0} };
static const app_timer_id_t serial_uart_rx_timer = &serial_uart_rx_timer_data;
static app_timer_t serial_uart_tx_timer_data = { {0} };
static const app_timer_id_t serial_uart_tx_timer = &serial_uart_tx_timer_data;
static nrf_serial_ctx_t serial_uart_ctx;
#ifdef NRF52840
static nrf_drv_uart_t instance = { .inst_idx = 0, .uarte = {.p_reg = NRF_UARTE0, .drv_inst_idx = 0} };
#else
static nrf_drv_uart_t instance = { .reg = { NRF_UARTE0}, .drv_inst_idx = 0 };
#endif
static const nrf_serial_t serial_uart = {
    .instance = instance,
    .p_ctx = &serial_uart_ctx,
    .p_tx_timer = &serial_uart_tx_timer,
    .p_rx_timer = &serial_uart_rx_timer,
};

// Define handlers for the nrf_serial library
static void nrf_serial_event_handler(struct nrf_serial_s const * p_serial, nrf_serial_event_t event);

// Event Handler for NRF_SERIAL events
// WARNING: Do not log anything in here as it is called from a different interrupt and will
//          break you program if your logging (such as Segger RTT) is not configured with threads in mind
static void nrf_serial_event_handler(struct nrf_serial_s const * p_serial, nrf_serial_event_t event) {
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    switch (event) {
        //Called as soon as data was received and is now available in the FIFO RX Buffer
        case NRF_SERIAL_EVENT_RX_DATA: {
            halMemory->nrfSerialDataAvailable = true;
            GS->uartEventHandler();
        } break;

        //Called as soon as the FIFO is full
        //TODO: Currently not properly handeled but only an issue if a lot of data is received at a time
        case NRF_SERIAL_EVENT_FIFO_ERR: {
            //SEGGER_RTT_WriteString(0, "NRF_SERIAL_EVENT_FIFO_ERR" EOL);
            halMemory->nrfSerialErrorDetected = true;
            GS->uartEventHandler();
        } break;

        //Called in case there is an overrun in the UART, a framing error, etc,...
        case NRF_SERIAL_EVENT_DRV_ERR: {
            //SEGGER_RTT_WriteString(0, "NRF_SERIAL_EVENT_DRV_ERR" EOL);
            halMemory->nrfSerialErrorDetected = true;
            GS->uartEventHandler();
        } break;

        //Called once a transmission was done successfully but the nrf_serial library
        //will automatically queue new data if there is still data in the TX FIFO
        case NRF_SERIAL_EVENT_TX_DONE: { } break;
    }
}

//Creates the serial_uart instance
NRF_SERIAL_CONFIG_DEF(
        serial_config,
        NRF_SERIAL_MODE_DMA,
        &serial_queues,
        &serial_buffs,
        nrf_serial_event_handler,
        NULL);

#endif //FH_NRF_ENABLE_EASYDMA_TERMINAL

void FruityHal::EnableUart(bool promptAndEchoMode)
{
#if FH_NRF_ENABLE_EASYDMA_TERMINAL
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;

    nrf_drv_uart_config_t config;
    config.pselrxd                  = Boardconfig->uartRXPin;
    config.pseltxd                  = Boardconfig->uartTXPin;;
    config.pselrts                  = Boardconfig->uartRTSPin == -1 ? NRF_UART_PSEL_DISCONNECTED : Boardconfig->uartRTSPin;
    config.pselcts                  = Boardconfig->uartCTSPin == -1 ? NRF_UART_PSEL_DISCONNECTED : Boardconfig->uartCTSPin;
    config.hwfc                     = Boardconfig->uartRTSPin == -1 ? NRF_UART_HWFC_DISABLED : NRF_UART_HWFC_ENABLED;
    config.parity                   = FH_EASYDMA_UART_PARITY;
    config.baudrate                 = UartBaudRateToNordic((FruityHal::UartBaudrate)Boardconfig->uartBaudRate);
    config.interrupt_priority       = HIGHER_THAN_APP_PRIO; //We must be fast enough to not cause a bottleneck

    u32 err = nrf_serial_init(&serial_uart, &config, &serial_config);

    //Configures SWI1 with IRQ prio 6
    //This is later used to read additional data after a line was processed as
    //Multiple read interrupts might have been missed during line processing
    if(!err) err = sd_nvic_SetPriority(SWI1_EGU1_IRQn, 6);

    if(!err) err = sd_nvic_EnableIRQ(SWI1_EGU1_IRQn);

    if(!err){
        halMemory->nrfSerialDataAvailable = false;
        halMemory->nrfSerialErrorDetected = false;
    }
#else

    //Configure pins
    nrf_gpio_pin_set(Boardconfig->uartTXPin);
    nrf_gpio_cfg_output(Boardconfig->uartTXPin);
    nrf_gpio_cfg_input(Boardconfig->uartRXPin, NRF_GPIO_PIN_NOPULL);

    nrf_uart_baudrate_set(NRF_UART0, UartBaudRateToNordic((FruityHal::UartBaudrate)Boardconfig->uartBaudRate));
    nrf_uart_configure(NRF_UART0, NRF_UART_PARITY_EXCLUDED, Boardconfig->uartRTSPin != -1 ? NRF_UART_HWFC_ENABLED : NRF_UART_HWFC_DISABLED);
    nrf_uart_txrx_pins_set(NRF_UART0, Boardconfig->uartTXPin, Boardconfig->uartRXPin);

    //Configure RTS/CTS (if RTS is -1, disable flow control)
    if (Boardconfig->uartRTSPin != -1) {
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
        FruityHal::UartEnableReadInterrupt();
    }
#endif //FH_NRF_ENABLE_EASYDMA_TERMINAL
}

void FruityHal::DisableUart()
{
#ifndef SIM_ENABLED
#if FH_NRF_ENABLE_EASYDMA_TERMINAL
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;

    u32 err = nrf_serial_uninit(&serial_uart);

    if(!err){
        halMemory->nrfSerialDataAvailable = false;
        halMemory->nrfSerialErrorDetected = false;
    }
#else 
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
#endif //FH_NRF_ENABLE_EASYDMA_TERMINAL
#endif //SIM_ENABLED
}

// TODO: The error parameter is not abstracted (IOT-4663)
void FruityHal::UartHandleError(u32 error)
{
#if FH_NRF_ENABLE_EASYDMA_TERMINAL
    //Disabling and Enabling the UART might fail as UartHandleError is called from IRQ PRIO 6
    //We might however have a write call in progress on IRQ PRIO 7 which does not allow us to
    //reinitialize the nrf_serial library
    //In this case, the nrfSerialErrorDetected variable will stay true and reinitialization is
    //retried when writing out a character
    DisableUart();
    EnableUart(false); //TODO: We should store the promptAndEchoMode state once we use easydma uart for PROMPT mode as well

#else

    //Errorsource is given, but has to be cleared to be handled
    NRF_UART0->ERRORSRC = error;

    //FIXME: maybe we need some better error handling here
#endif
}

bool FruityHal::UartCheckInputAvailable()
{
#if FH_NRF_ENABLE_EASYDMA_TERMINAL
    return ((NrfHalMemory*)GS->halMemory)->nrfSerialDataAvailable;
#else
    return NRF_UART0->EVENTS_RXDRDY == 1;
#endif
}

FruityHal::UartReadCharBlockingResult FruityHal::UartReadCharBlocking()
{
    UartReadCharBlockingResult retVal;

#if FH_NRF_ENABLE_EASYDMA_TERMINAL
    size_t bytesRead = 0;
    while(bytesRead == 0){
        nrf_serial_read(&serial_uart, &retVal.c, 1, &bytesRead, 0);
        if(((NrfHalMemory*)GS->halMemory)->nrfSerialErrorDetected) retVal.didError = true;
    }
#else
    while (NRF_UART0->EVENTS_RXDRDY != 1) {
        if (NRF_UART0->EVENTS_ERROR) {
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

void FruityHal::UartPutStringBlockingWithTimeout(const char* message)
{
#if FH_NRF_ENABLE_EASYDMA_TERMINAL
    u16 dataLengthRemaining = strlen(message);
    const char* messagePtr = message;

    //We might be in a state where it was impossible to restart Uart (see UartHandleError())
    //So we try to restart it here
    if(((NrfHalMemory*)GS->halMemory)->nrfSerialErrorDetected){
        //We leave the error handling to our Interrupt handler to not cause threading issues
        sd_nvic_SetPendingIRQ(SWI1_EGU1_IRQn);
    }

    char buffer[FH_EASYDMA_UART_SERIAL_BUFF_TX_SIZE];

    while(dataLengthRemaining > 0){
        //We need to make a copy of the data chunks as it might not be in RAM, which is necessary for EasyDMA
        u16 chunkLengthRemaining = dataLengthRemaining < FH_EASYDMA_UART_SERIAL_BUFF_TX_SIZE ? dataLengthRemaining : FH_EASYDMA_UART_SERIAL_BUFF_TX_SIZE;
        CheckedMemcpy(buffer, messagePtr, chunkLengthRemaining);

        const char* chunkPtr = buffer;

        int i = 0;
        while(chunkLengthRemaining > 0) {
            size_t bytesWritten = 0;
            const u32 err = nrf_serial_write(&serial_uart, chunkPtr, chunkLengthRemaining, &bytesWritten, 0);

            //The error might e.g. be a timeout but bytes could have been written out nevertheless
            chunkLengthRemaining -= bytesWritten;
            dataLengthRemaining -= bytesWritten;
            chunkPtr += bytesWritten;
            messagePtr += bytesWritten;

            //In case we fail to write the message for a longer time, we time out
            if(i++ > 10000) return;
        }
        
    }
#else
    uint_fast8_t i = 0;
    uint8_t byte = message[i++];

    while (byte != '\0')
    {
        NRF_UART0->TXD = byte;
        byte = message[i++];

        int i = 0;
        while (NRF_UART0->EVENTS_TXDRDY != 1) {
            //Timeout if it was not possible to put the character
            if (i > 10000) {
                return;
            }
            i++;
            //FIXME: Do we need error handling here? Will cause lost characters
        }
        NRF_UART0->EVENTS_TXDRDY = 0;
    }
#endif
}

//TODO: This is a duplicate of CheckAndHandleUartError (IOT-4663)
bool FruityHal::IsUartErroredAndClear()
{
#if FH_NRF_ENABLE_EASYDMA_TERMINAL
    if(((NrfHalMemory*)GS->halMemory)->nrfSerialErrorDetected){
        UartHandleError(0);
        return true;
    } else {
        return false;
    }
#else
    if (nrf_uart_int_enable_check(NRF_UART0, NRF_UART_INT_MASK_ERROR) &&
        nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_ERROR))
    {
        nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_ERROR);

        FruityHal::UartHandleError(NRF_UART0->ERRORSRC);

        return true;
    }
    return false;
#endif
}

//TODO: This is a duplicate of CheckAndHandleUartTimeout (IOT-4663)
bool FruityHal::IsUartTimedOutAndClear()
{
#if FH_NRF_ENABLE_EASYDMA_TERMINAL
    //There is no known way that this can happen with EasyDMA UART
    return false;
#else
    if (nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_RXTO))
    {
        nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXTO);

        //Restart transmission and clear previous buffer
        nrf_uart_task_trigger(NRF_UART0, NRF_UART_TASK_STARTRX);

        return true;

        //TODO: can we check if this works???
    }
    return false;
#endif
}

FruityHal::UartReadCharResult FruityHal::UartReadChar()
{
    UartReadCharResult retVal;

#if FH_NRF_ENABLE_EASYDMA_TERMINAL
    size_t bytesRead = 0;
    nrf_serial_read(&serial_uart, &retVal.c, 1, &bytesRead, 0);
    if(bytesRead) retVal.hasNewChar = true;
    else ((NrfHalMemory*)GS->halMemory)->nrfSerialDataAvailable = false;
#else
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
#endif

    return retVal;
}

#if FH_NRF_ENABLE_EASYDMA_TERMINAL
extern "C"{
    void SWI1_IRQHandler(void)
    {
        //We can safely call this from here as it is running on the same IRQ prio as the UART
        GS->uartEventHandler();
    }
}
#endif

void FruityHal::UartEnableReadInterrupt()
{
#if FH_NRF_ENABLE_EASYDMA_TERMINAL
    //We trigger SWI1 to check if there is any more UART data
    volatile u32 err = sd_nvic_SetPendingIRQ(SWI1_EGU1_IRQn);

#else
    nrf_uart_int_enable(NRF_UART0, NRF_UART_INT_MASK_RXDRDY | NRF_UART_INT_MASK_ERROR);
#endif
}

bool FruityHal::CheckAndHandleUartTimeout()
{
#if FH_NRF_ENABLE_EASYDMA_TERMINAL
    //Can not happen for EasyDMA UART with nrf_serial library
    return false;
#else
#ifndef SIM_ENABLED
    if (nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_RXTO))
    {
        nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_RXTO);

        //Restart transmission and clear previous buffer
        nrf_uart_task_trigger(NRF_UART0, NRF_UART_TASK_STARTRX);

        return true;
    }
#endif //SIM_ENABLED
    return false;
#endif
}

// TODO: The returned error codes are not HAL abstracted (IOT-4663)
u32 FruityHal::CheckAndHandleUartError()
{
#if FH_NRF_ENABLE_EASYDMA_TERMINAL
    if(((NrfHalMemory*)GS->halMemory)->nrfSerialErrorDetected){
        UartHandleError(1);
        return 1;
    } else {
        return 0;
    }
#else
    //Checks if an error occured
    if (nrf_uart_int_enable_check(NRF_UART0, NRF_UART_INT_MASK_ERROR) &&
        nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_ERROR))
    {
        nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_ERROR);

        //Errorsource is given, but has to be cleared to be handled
        NRF_UART0->ERRORSRC = NRF_UART0->ERRORSRC;
        //TODO: How does this work, isn't it already cleared, isn't that the same as UartHandleError?
        return NRF_UART0->ERRORSRC;
    }
    return 0;
#endif
}

//This handler receives UART interrupts (terminal json mode)
#if !defined(UART_ENABLED) || UART_ENABLED == 0 || defined(SIM_ENABLED) //Only enable if nordic library for UART is not used
extern "C"{
    #ifndef FH_NRF_ENABLE_EASYDMA_TERMINAL
    void UART0_IRQHandler(void)
    {
        if (GS->uartEventHandler == nullptr) {
            SIMEXCEPTION(UartNotSetException);
        } else {
            GS->uartEventHandler();
        }
    }
    #endif
}
#endif

#define __________________TWI____________________

#if IS_ACTIVE(ASSET_MODULE)

#ifndef SIM_ENABLED
#define TWI_INSTANCE_ID     1
static constexpr nrf_drv_twi_t twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);
#endif

#ifndef SIM_ENABLED
static void twi_handler(nrf_drv_twi_evt_t const * pEvent, void * pContext)
{
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    switch (pEvent->type) {
    // Transfer completed event.
    case NRF_DRV_TWI_EVT_DONE:
        halMemory->twiXferDone = true;
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

ErrorType FruityHal::TwiInit(i32 sclPin, i32 sdaPin)
{
    u32 errCode = NRF_SUCCESS;
#ifndef SIM_ENABLED
    // twi.reg          = {NRF_DRV_TWI_PERIPHERAL(TWI_INSTANCE_ID)};
    // twi.drv_inst_idx = CONCAT_3(TWI, TWI_INSTANCE_ID, _INSTANCE_INDEX);
    // twi.use_easy_dma = TWI_USE_EASY_DMA(TWI_INSTANCE_ID);

    const nrf_drv_twi_config_t twiConfig = {
            .scl                = (u32)sclPin,
            .sda                = (u32)sdaPin,
#if SDK == 15 || SDK == 16
            .frequency          = NRF_DRV_TWI_FREQ_250K,
#else
            .frequency          = NRF_TWI_FREQ_250K,
#endif
            .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
            .clear_bus_init     = false,
            .hold_bus_uninit    = false
    };

    errCode = nrf_drv_twi_init(&twi, &twiConfig, twi_handler, NULL);
    if (errCode != NRF_ERROR_INVALID_STATE && errCode != NRF_SUCCESS) {
        FRUITYMESH_ERROR_CHECK(errCode);
    }

    nrf_drv_twi_enable(&twi);

    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    halMemory->twiInitDone = true;
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

void FruityHal::TwiUninit()
{
#ifndef SIM_ENABLED
    nrf_drv_twi_disable(&twi);
    nrf_drv_twi_uninit(&twi);
    NrfHalMemory *halMemory = (NrfHalMemory *)GS->halMemory;
    halMemory->twiInitDone = false;
#else
    cherrySimInstance->currentNode->twiWasInit = false;
#endif
}

//Difference between TwiStart and TwiInit is that it first
//uninitialises Twi afterwards switch off and later switch on
//TWI register and then initialises twi. This should be used
//When Twi and gpiote both are configured, so we avoid unwanted
//power consumption due to twi bus register
void FruityHal::TwiStart(i32 sclPin, i32 sdaPin)
{
#ifndef SIM_ENABLED
    TwiUninit();
    twiTurnOnAnomaly89();
    TwiInit(sclPin, sdaPin);
#endif
}

//Manually switch off twi register so we are not consuming power when not being 
//used. TwiStart() should be used to restart the twi bus if we want to switch
//it back on
void FruityHal::TwiStop()
{
#ifndef SIM_ENABLED
    twiTurnOffAnomaly89();
#endif
}

void FruityHal::TwiGpioAddressPinSetAndWait(bool high, i32 sdaPin)
{
#ifndef SIM_ENABLED
    nrf_gpio_cfg_output((u32)sdaPin);
    if (high) {
        nrf_gpio_pin_set((u32)sdaPin);
        nrf_delay_us(200);
    }
    else {
        nrf_gpio_pin_clear((u32)sdaPin);
        nrf_delay_us(200);
    }

    nrf_gpio_pin_set((u32)sdaPin);
#endif
}

ErrorType FruityHal::TwiRegisterWrite(u8 slaveAddress, u8 const * pTransferData, u8 length)
{
    // Slave Address (Command) (7 Bit) + WriteBit (1 Bit) + register Byte (1 Byte) + Data (n Bytes)

    u32 errCode = NRF_SUCCESS;
#ifndef SIM_ENABLED
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    halMemory->twiXferDone = false;

    errCode =  nrf_drv_twi_tx(&twi, slaveAddress, pTransferData, length, false);

    if (errCode != NRF_SUCCESS)
    {
        return nrfErrToGeneric(errCode);
    }
    // wait for transmission complete
    while(halMemory->twiXferDone == false)
    {
        sdAppEvtWaitAnomaly87();
    }
    halMemory->twiXferDone = false;
#endif
    return nrfErrToGeneric(errCode);
}

ErrorType FruityHal::TwiRegisterRead(u8 slaveAddress, u8 reg, u8 * pReceiveData, u8 length)
{
    // Slave Address (7 Bit) (Command) + WriteBit (1 Bit) + register Byte (1 Byte) + Repeated Start + Slave Address + ReadBit + Data.... + nAck
    u32 errCode = NRF_SUCCESS;
#ifndef SIM_ENABLED
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    halMemory->twiXferDone = false;

    nrf_drv_twi_xfer_desc_t xfer = NRF_DRV_TWI_XFER_DESC_TXRX(slaveAddress, &reg, 1, pReceiveData, length);

    errCode = nrf_drv_twi_xfer(&twi, &xfer, 0);

    if (errCode != NRF_SUCCESS)
    {
        return nrfErrToGeneric(errCode);
    }

    // wait for transmission and read complete
    while(halMemory->twiXferDone == false)
    {
        sdAppEvtWaitAnomaly87();
    }
    halMemory->twiXferDone = false;
#endif
    return nrfErrToGeneric(errCode);
}

ErrorType FruityHal::TwiRead(u8 slaveAddress, u8 * pReceiveData, u8 length)
{
    // Slave Address (7 Bit) (Command) + ReadBit (1 Bit) + Data.... + nAck
    uint32_t errCode = NRF_SUCCESS;
#ifndef SIM_ENABLED
    NrfHalMemory *halMemory = (NrfHalMemory *)GS->halMemory;
    halMemory->twiXferDone = false;

    nrf_drv_twi_xfer_desc_t xfer = {.type = NRF_DRV_TWI_XFER_RX,
                                          .address = slaveAddress,
                                          .primary_length = length,
                                          .secondary_length = 0,
                                          .p_primary_buf = pReceiveData,
                                          .p_secondary_buf = NULL

    };

    errCode = nrf_drv_twi_xfer(&twi, &xfer, 0);

    if (errCode != NRF_SUCCESS)
    {
        return nrfErrToGeneric(errCode);
    }

    // wait for transmission and read complete
    while (halMemory->twiXferDone == false)
    {
        sdAppEvtWaitAnomaly87();
    }
    halMemory->twiXferDone = false;
#endif
    return nrfErrToGeneric(errCode);
}

bool FruityHal::TwiIsInitialized(void)
{
#ifndef SIM_ENABLED
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    return halMemory->twiInitDone;
#else
    return cherrySimInstance->currentNode->twiWasInit;
#endif
}

#endif

#define __________________SPI____________________

#if defined(SPI_ENABLED) || IS_ACTIVE(ASSET_MODULE)

#ifndef SIM_ENABLED
#define SPI_INSTANCE  0
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);
#endif

#ifndef SIM_ENABLED
void spi_event_handler(nrf_drv_spi_evt_t const * p_event, void* p_context)
{
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    halMemory->spiXferDone = true;
    logt("FH", "SPI Xfer done");
}
#endif

void FruityHal::SpiInit(i32 sckPin, i32 misoPin, i32 mosiPin)
{
#ifndef SIM_ENABLED
    /* Conigure SPI Interface */
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.sck_pin = (u32)sckPin;
    spi_config.miso_pin = (misoPin == -1) ? NRF_DRV_SPI_PIN_NOT_USED : (u32)misoPin;
    spi_config.mosi_pin = (mosiPin == -1) ? NRF_DRV_SPI_PIN_NOT_USED : (u32)mosiPin;
    spi_config.frequency = NRF_DRV_SPI_FREQ_4M;

    FRUITYMESH_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, spi_event_handler,NULL));

    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    halMemory->spiXferDone = true;
    halMemory->spiInitDone = true;
#else
    if (cherrySimInstance->currentNode->spiWasInit)
    {
        //Already initialized!
        SIMEXCEPTION(IllegalStateException);
    }
    cherrySimInstance->currentNode->spiWasInit = true;
#endif

}


bool FruityHal::SpiIsInitialized(void)
{
#ifndef SIM_ENABLED
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;
    return halMemory->spiInitDone;
#else
    return cherrySimInstance->currentNode->spiWasInit;
#endif
}

ErrorType FruityHal::SpiTransfer(u8* const p_toWrite, u8 count, u8* const p_toRead, i32 slaveSelectPin)
{
    u32 retVal = NRF_SUCCESS;
#ifndef SIM_ENABLED
    NrfHalMemory* halMemory = (NrfHalMemory*)GS->halMemory;

    if ((NULL == p_toWrite) || (NULL == p_toRead))
    {
        retVal = NRF_ERROR_INTERNAL;
    }


    /* check if an other SPI transfer is running */
    if ((true == halMemory->spiXferDone) && (NRF_SUCCESS == retVal))
    {
        halMemory->spiXferDone = false;

        nrf_gpio_pin_clear((u32)slaveSelectPin);
        FRUITYMESH_ERROR_CHECK(nrf_drv_spi_transfer(&spi, p_toWrite, count, p_toRead, count));
        //Locks if run in interrupt context
        while (!halMemory->spiXferDone)
        {
            sdAppEvtWaitAnomaly87();
        }
        nrf_gpio_pin_set((u32)slaveSelectPin);
        retVal = NRF_SUCCESS;
    }
    else
    {
        retVal = NRF_ERROR_BUSY;
    }
#endif
    return nrfErrToGeneric(retVal);
}

void FruityHal::SpiConfigureSlaveSelectPin(i32 pin)
{
#ifndef SIM_ENABLED
    nrf_gpio_pin_dir_set((u32)pin, NRF_GPIO_PIN_DIR_OUTPUT);
    nrf_gpio_cfg_output((u32)pin);
    nrf_gpio_pin_set((u32)pin);
#endif
}

#endif // defined(SPI_ENABLED)

u32 FruityHal::GetHalMemorySize()
{
    return sizeof(NrfHalMemory);
}

#if IS_ACTIVE(TIMESLOT)

// ######################### Timeslot API ############################

static FruityHal::RadioCallbackSignalType nrfRadioCallbackSignalTypeToGeneric(uint8_t signal_type)
{
    switch (signal_type)
    {
        case NRF_RADIO_CALLBACK_SIGNAL_TYPE_START:
            return FruityHal::RadioCallbackSignalType::START;
        case NRF_RADIO_CALLBACK_SIGNAL_TYPE_RADIO:
            return FruityHal::RadioCallbackSignalType::RADIO;
        case NRF_RADIO_CALLBACK_SIGNAL_TYPE_TIMER0:
            return FruityHal::RadioCallbackSignalType::TIMER0;
        case NRF_RADIO_CALLBACK_SIGNAL_TYPE_EXTEND_SUCCEEDED:
            return FruityHal::RadioCallbackSignalType::EXTEND_SUCCEEDED;
        case NRF_RADIO_CALLBACK_SIGNAL_TYPE_EXTEND_FAILED:
            return FruityHal::RadioCallbackSignalType::EXTEND_FAILED;
        default:
            SIMEXCEPTION(IllegalArgumentException);
            return FruityHal::RadioCallbackSignalType::UNKNOWN_SIGNAL_TYPE;
    }
}

extern "C" {

static nrf_radio_signal_callback_return_param_t * nrfRadioCallback(uint8_t signal_type)
{
    // code adapted from https://devzone.nordicsemi.com/nordic/short-range-guides/b/software-development-kit/posts/setting-up-the-timeslot-api

    // ATTENTION: This signal handler runs at interrupt priority level 0,
    //            the highest priority. Since softdevice API functions
    //            are executed as SVC interrupts with priority 4, no such
    //            functions can be used.

    auto *halMemory = static_cast<NrfHalMemory *>(GS->halMemory);
    auto &returnParam = halMemory->timeslotRadioCallbackReturnParam;
    auto &radioRequest = halMemory->timeslotRadioRequest;

    returnParam.params.request.p_next = NULL;
    returnParam.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_NONE;

    const auto callbackAction = Timeslot::GetInstance().DispatchRadioSignalCallback(nrfRadioCallbackSignalTypeToGeneric(signal_type));
    switch (callbackAction)
    {
        case FruityHal::RadioCallbackAction::NONE:
            returnParam.params.request.p_next = NULL;
            returnParam.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_NONE;
            break;

        // case FruityHal::RadioCallbackAction::EXTEND:
        //     ... not supported by the POC ...

        case FruityHal::RadioCallbackAction::END:
            returnParam.params.request.p_next = NULL;
            returnParam.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_END;
#if defined(SIM_ENABLED)
            cherrySimInstance->currentNode->timeslotActive = false;
            logt("FH", "timeslot is over");
#endif
            break;

        case FruityHal::RadioCallbackAction::REQUEST_AND_END:
            returnParam.params.request.p_next = &radioRequest;
            returnParam.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_REQUEST_AND_END;
#if defined(SIM_ENABLED)
            cherrySimInstance->currentNode->timeslotActive = false;
            logt("FH", "timeslot is over");
#endif
            break;
    }

#if defined(SIM_ENABLED)
    cherrySimInstance->currentNode->timeslotRequested =
            returnParam.params.request.p_next != nullptr;
    if (cherrySimInstance->currentNode->timeslotRequested)
    {
        logt("FH", "another timeslot was requested");
    }
#endif

    return &returnParam;
}

}

ErrorType FruityHal::TimeslotOpenSession()
{
    ErrorType err = ErrorType::SUCCESS;

    err = nrfErrToGeneric(sd_radio_session_open(&nrfRadioCallback));
    if (err != ErrorType::SUCCESS)
    {
        logt("FH", "sd_radio_session_open failed (%u)", (u32)err);
    }

#if defined(SIM_ENABLED)
    logt("FH", "timeslot session successfully opened");
#endif

    return err;
}

void FruityHal::TimeslotCloseSession()
{
    const auto err = nrfErrToGeneric(sd_radio_session_close());
    if (err != ErrorType::SUCCESS)
    {
        logt("FH", "sd_radio_session_close failed (%u)", (u32)err);
    }

#if defined(SIM_ENABLED)
    logt("FH", "timeslot session closing successfully requested");
    cherrySimInstance->currentNode->timeslotCloseSessionRequested = true;
#endif
}

void FruityHal::TimeslotConfigureNextEventEarliest(u32 lengthMicroseconds)
{
    auto *halMemory = static_cast<NrfHalMemory *>(GS->halMemory);
    auto &radioRequest = halMemory->timeslotRadioRequest;

    // TODO: sanity-check arguments (eg. length > min length)

    radioRequest.request_type = NRF_RADIO_REQ_TYPE_EARLIEST;
    radioRequest.params.earliest.hfclk = NRF_RADIO_HFCLK_CFG_XTAL_GUARANTEED;
    radioRequest.params.earliest.priority = NRF_RADIO_PRIORITY_NORMAL;
    radioRequest.params.earliest.length_us = lengthMicroseconds;
    radioRequest.params.earliest.timeout_us = 1000000;

#if defined(SIM_ENABLED)
    logt("FH", "timeslot configured for earliest next event with length %u", radioRequest.params.earliest.timeout_us);
#endif
}

void FruityHal::TimeslotConfigureNextEventNormal(u32 lengthMicroseconds, u32 distanceMicroseconds)
{
    auto *halMemory = static_cast<NrfHalMemory *>(GS->halMemory);
    auto &radioRequest = halMemory->timeslotRadioRequest;

    // TODO: sanity-check arguments (eg. length > min length, length < distance)

    radioRequest.request_type = NRF_RADIO_REQ_TYPE_NORMAL;
    radioRequest.params.normal.hfclk = NRF_RADIO_HFCLK_CFG_XTAL_GUARANTEED;
    radioRequest.params.normal.priority = NRF_RADIO_PRIORITY_NORMAL;
    radioRequest.params.normal.length_us = lengthMicroseconds;
    radioRequest.params.normal.distance_us = distanceMicroseconds;

#if defined(SIM_ENABLED)
    logt("FH", "timeslot configured for next event with length %u and distance %u", radioRequest.params.earliest.timeout_us, radioRequest.params.normal.distance_us);
#endif
}

ErrorType FruityHal::TimeslotRequestNextEvent()
{
    auto *halMemory = static_cast<NrfHalMemory *>(GS->halMemory);
    auto &radioRequest = halMemory->timeslotRadioRequest;

#if defined(SIM_ENABLED)
    logt("FH", "timeslot next event requested");
    cherrySimInstance->currentNode->timeslotRequested = true;
#endif

    return nrfErrToGeneric(sd_radio_request(&radioRequest));
}
    
// ######################### RADIO ############################

void FruityHal::RadioUnmaskEvent(RadioEvent radioEvent)
{
#if defined(SIM_ENABLED)
    switch (radioEvent)
    {
        case RadioEvent::DISABLED:
            NRF_RADIO->EVENTS_DISABLED_MASKED = false;
            break;
        
        default:
            SIMEXCEPTION(IllegalArgumentException);
    }
#else
#define FRUITY_HAL_RADIO_INTENSET(EVENT_NAME) \
    ((RADIO_INTENSET_ ## EVENT_NAME ## _Enabled << RADIO_INTENSET_ ## EVENT_NAME ## _Pos) & RADIO_INTENSET_ ## EVENT_NAME ## _Msk)

    u32 intenset = 0;

    switch (radioEvent)
    {
        case RadioEvent::DISABLED:
            intenset |= FRUITY_HAL_RADIO_INTENSET(DISABLED);
            break;
    }

    NRF_RADIO->INTENSET = intenset;

#undef FRUITY_HAL_RADIO_INTENSET
#endif
}

void FruityHal::RadioMaskEvent(RadioEvent radioEvent)
{
#if defined(SIM_ENABLED)
    switch (radioEvent)
    {
        case RadioEvent::DISABLED:
            NRF_RADIO->EVENTS_DISABLED_MASKED = true;
            break;
        
        default:
            SIMEXCEPTION(IllegalArgumentException);
    }
#else
#define FRUITY_HAL_RADIO_INTENCLR(EVENT_NAME) \
    ((RADIO_INTENCLR_ ## EVENT_NAME ## _Enabled << RADIO_INTENCLR_ ## EVENT_NAME ## _Pos) & RADIO_INTENCLR_ ## EVENT_NAME ## _Msk)

    u32 intenclr = 0;

    switch (radioEvent)
    {
        case RadioEvent::DISABLED:
            intenclr |= FRUITY_HAL_RADIO_INTENCLR(DISABLED);
            break;
    }

    NRF_RADIO->INTENCLR = intenclr;

#undef FRUITY_HAL_RADIO_INTENCLR
#endif
}

#define FRUITY_HAL_RADIO_CHECK_EVENT(EVENT_NAME)                           \
    static_cast<bool>(NRF_RADIO->EVENTS_ ## EVENT_NAME != 0)
#define FRUITY_HAL_RADIO_CLEAR_EVENT(EVENT_NAME) \
    NRF_RADIO->EVENTS_ ## EVENT_NAME = 0

bool FruityHal::RadioCheckEvent(RadioEvent radioEvent)
{
    switch (radioEvent)
    {
        case RadioEvent::DISABLED:
            return FRUITY_HAL_RADIO_CHECK_EVENT(DISABLED);

#if defined(SIM_ENABLED)
        default:
            SIMEXCEPTION(IllegalArgumentException);
#endif
    }

    return false;
}

void FruityHal::RadioClearEvent(RadioEvent radioEvent)
{
    switch (radioEvent)
    {
        case RadioEvent::DISABLED:
            FRUITY_HAL_RADIO_CLEAR_EVENT(DISABLED);
            break;

#if defined(SIM_ENABLED)
        default:
            SIMEXCEPTION(IllegalArgumentException);
#endif
    }
}

bool FruityHal::RadioCheckAndClearEvent(RadioEvent radioEvent)
{
    const bool result = RadioCheckEvent(radioEvent);
    RadioClearEvent(radioEvent);
    return result;
}

#undef FRUITY_HAL_RADIO_CHECK_EVENT
#undef FRUITY_HAL_RADIO_CLEAR_EVENT

void FruityHal::RadioTriggerTask(RadioTask radioTask)
{
#define FRUITY_HAL_RADIO_TRIGGER_TASK(TASK_NAME) \
    NRF_RADIO->TASKS_ ## TASK_NAME = 1

    switch (radioTask)
    {
        case RadioTask::DISABLE:
            FRUITY_HAL_RADIO_TRIGGER_TASK(DISABLE);
            break;

        case RadioTask::TXEN:
            FRUITY_HAL_RADIO_TRIGGER_TASK(TXEN);
            break;

#if defined(SIM_ENABLED)    
        default:
            SIMEXCEPTION(IllegalArgumentException);
            break;
#endif
    }

#undef FRUITY_HAL_RADIO_TRIGGER_TASK
}

void FruityHal::RadioChooseBleAdvertisingChannel(unsigned channelIndex)
{
    if (channelIndex < 37 || channelIndex > 39)
    {
        SIMEXCEPTION(IllegalArgumentException);
        return; // assert?
    }

#if !defined(SIM_ENABLED)
    constexpr struct {
        u32 whitening;
        u32 frequency;
    } advChannelConfigurations[] = {
        {37, 2}, {38, 26}, {39, 80}
    };
    const auto &channel = advChannelConfigurations[channelIndex - 37];

    // Radio channel frequency (bits 0..6, range 0-100) and channel map (bit 8).
    NRF_RADIO->FREQUENCY = channel.frequency;

    // Initial value for data whitening (bit-mixing to reduce DC-bias in the RF-signal).
    NRF_RADIO->DATAWHITEIV = channel.whitening;
#endif
}

signed FruityHal::RadioChooseTxPowerHint(signed txPowerHint, bool dryRun)
{
#if defined(SIM_ENABLED) || defined(NRF52) || defined(NRF52840)
    // Lookup table of supported transmission powers (in dBm) and the
    // corresponding value of the TXPOWER register of the radio peripheral.
    // IMPORTANT: Keep the lookup-table in ascending order.
    static constexpr struct {
        i8 txPower;
        u8 registerValue;
    } supportedValues[] = {
        {-40, RADIO_TXPOWER_TXPOWER_Neg40dBm},
        {-20, RADIO_TXPOWER_TXPOWER_Neg20dBm},
        {-16, RADIO_TXPOWER_TXPOWER_Neg16dBm},
        {-12, RADIO_TXPOWER_TXPOWER_Neg12dBm},
        {-8, RADIO_TXPOWER_TXPOWER_Neg8dBm},
        {-4, RADIO_TXPOWER_TXPOWER_Neg4dBm},
        {0, RADIO_TXPOWER_TXPOWER_0dBm},
#if defined(NRF52840)
        {2, RADIO_TXPOWER_TXPOWER_Pos2dBm},
#endif
        {3, RADIO_TXPOWER_TXPOWER_Pos3dBm},
        {4, RADIO_TXPOWER_TXPOWER_Pos4dBm},
#if defined(NRF52840)
        {5, RADIO_TXPOWER_TXPOWER_Pos5dBm},
        {6, RADIO_TXPOWER_TXPOWER_Pos6dBm},
        {7, RADIO_TXPOWER_TXPOWER_Pos7dBm},
        {8, RADIO_TXPOWER_TXPOWER_Pos8dBm},
#endif
    };

    static constexpr size_t supportedValueCount = sizeof(supportedValues) / sizeof(supportedValues[0]);

    auto result = [txPowerHint] {
        for (size_t index = 0; index < supportedValueCount; ++index)
        {
            // Round the hint up to the next supported value.
            if (txPowerHint <= supportedValues[index].txPower)
                return supportedValues[index];
        }

        // The hint was smaller than the minimum, here we round up.
        return supportedValues[supportedValueCount - 1];
    }();

    if (!dryRun)
    {
        // Transmission power ranges from -40dBm to +8dBm.
        NRF_RADIO->TXPOWER = (result.registerValue << RADIO_TXPOWER_TXPOWER_Pos);
    }

    return result.txPower;
#else
    return 0;
#endif
}

void FruityHal::RadioHandleBleAdvTxStart(u8 *packet)
{
    // For reference see the following example for nRF51 Series SoCs from Nordic
    // https://github.com/NordicPlayground/nRF51-multi-role-conn-observer-advertiser
    //
#if !defined(SIM_ENABLED)
    // Power-cycle the radio, this resets the peripheral and registersto its initial state.
    NRF_RADIO->POWER = 1;
#endif

    RadioClearEvent(RadioEvent::DISABLED);
    RadioChooseBleAdvertisingChannel(37);
    RadioChooseTxPowerHint(0);

    // Set the packet pointer for radio tx/rx.
    NRF_RADIO->PACKETPTR = (u32)reinterpret_cast<uintptr_t>(packet);

#if !defined(SIM_ENABLED)
    // For the POC we use 1Mbit/s BLE (just like the example ...).
    NRF_RADIO->MODE = (RADIO_MODE_MODE_Ble_1Mbit << RADIO_MODE_MODE_Pos);

    // Setup the access address used on-air.
    // TODO: refactor into separate functions that handle the mapping
    //       between logical addresses and the base / prefix pairs.
    NRF_RADIO->PREFIX0 = 0x0000008e;
    NRF_RADIO->BASE0 = 0x89bed600;
    NRF_RADIO->TXADDRESS = 0; // logical address from 0-7
    NRF_RADIO->RXADDRESSES = 0; // POC does not receive (bits 0-7 clear)

    // Setup packet configuration (on-air LENGTH #bits, S0 #bytes, S1 #bytes, ...)
    // TODO: refactor into separate functions
    NRF_RADIO->PCNF0 = (
            ((1u << RADIO_PCNF0_S0LEN_Pos) & RADIO_PCNF0_S0LEN_Msk)
        |   ((2u << RADIO_PCNF0_S1LEN_Pos) & RADIO_PCNF0_S1LEN_Msk)
        |   ((6u << RADIO_PCNF0_LFLEN_Pos) & RADIO_PCNF0_LFLEN_Msk)
    );
    constexpr auto endian = RADIO_PCNF1_ENDIAN_Little;
    constexpr auto whiteen = RADIO_PCNF1_WHITEEN_Enabled;
    NRF_RADIO->PCNF1 = (
            ((37u     << RADIO_PCNF1_MAXLEN_Pos) & RADIO_PCNF1_MAXLEN_Msk)
        |   ((0u      << RADIO_PCNF1_STATLEN_Pos) & RADIO_PCNF1_STATLEN_Msk)
        |   ((3u      << RADIO_PCNF1_BALEN_Pos) & RADIO_PCNF1_BALEN_Msk)
        |   ((endian  << RADIO_PCNF1_ENDIAN_Pos) & RADIO_PCNF1_ENDIAN_Msk)
        |   ((whiteen << RADIO_PCNF1_WHITEEN_Pos) & RADIO_PCNF1_WHITEEN_Msk)
    );

    // Configure shortcuts through the radios state-transitions.
    NRF_RADIO->SHORTS = (
            ((1 << RADIO_SHORTS_READY_START_Pos) & RADIO_SHORTS_READY_START_Msk)
        |   ((1 << RADIO_SHORTS_END_DISABLE_Pos) & RADIO_SHORTS_END_DISABLE_Msk)
    );

    // Configure the checksum computation.
    NRF_RADIO->CRCCNF = (
            (RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos)
        |   (RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos)
    );
    NRF_RADIO->CRCINIT = 0x00555555; // CRC initial value
    NRF_RADIO->CRCPOLY = 0x0000065b; // CRC polynomial function

    // Configure interframe spacing in us. This is the time between the
    // last and first bit of two consecutive packets.
    NRF_RADIO->TIFS = 150;

    NVIC_EnableIRQ(RADIO_IRQn);
#endif
}

#endif // IS_ACTIVE(TIMESLOT)

/// Implements fix for anomaly 87 (CPU: Unexpected wake from System ON Idle
/// when using FPU). See [1] for details.
///
/// This function should be called instead of sd_app_evt_wait.
///
/// [1] https://infocenter.nordicsemi.com/topic/errata_nRF52832_EngC/ERR/nRF52832/EngineeringC/latest/anomaly_832_87.html?cp=4_2_1_2_1_24
static uint32_t sdAppEvtWaitAnomaly87()
{
#if !defined(SIM_ENABLED)
    CRITICAL_REGION_ENTER();
    __set_FPSCR(__get_FPSCR() & ~(0x0000009F));
    (void) __get_FPSCR();
    NVIC_ClearPendingIRQ(FPU_IRQn);
    CRITICAL_REGION_EXIT();
#endif

    return sd_app_evt_wait();
}

#if IS_ACTIVE(ASSET_MODULE)
//Both twiTurnOnAnomaly89() and twiTurnOffAnomaly89() is only added if ACTIVATE_ASSET_MODULE is active because currently
//twi is only activated for ASSET_MODULE so to avoid the cpp warning of unused functions. Tracked in BR-2082.
#if !defined(SIM_ENABLED)
/// Implements fix for anomaly 89 (CPU:Consumes static current
/// when using GPIOTE and TWI togther
/// https://infocenter.nordicsemi.com/topic/errata_nRF52832_EngC/ERR/nRF52832/EngineeringC/latest/anomaly_832_89.html
static void twiTurnOnAnomaly89()
{
    switch (TWI_INSTANCE_ID)
    {
        case 0:
            // The intermediate read is required due to the issue explained in the
            // [Application Note 321 ARM Cortex-M Programming Guide to Memory Barrier Instructions](https://developer.arm.com/documentation/dai0321/a/)
            // on section 3.6, page 18, look for 'The memory barrier instructions DMB and DSB'.
            // In a nutshell it flushes a write-cache internal to the CPU which could swallow
            // the first write.
            *(volatile uint32_t *)(0x40003000 + 0xFFC) = 0;
            *(volatile uint32_t *)(0x40003000 + 0xFFC);
            *(volatile uint32_t *)(0x40003000 + 0xFFC) = 1;
            break;
        case 1:
            *(volatile uint32_t *)(0x40004000 + 0xFFC) = 0;
            *(volatile uint32_t *)(0x40004000 + 0xFFC);
            *(volatile uint32_t *)(0x40004000 + 0xFFC) = 1;
            break;
        default:
            logt("ERROR", "Invalid TWI instance");
    }
}
/// Implements fix for anomaly 89 (CPU:Consumes static current
/// when using GPIOTE and TWI togther
/// https://infocenter.nordicsemi.com/topic/errata_nRF52832_EngC/ERR/nRF52832/EngineeringC/latest/anomaly_832_89.html
static void twiTurnOffAnomaly89()
{
    switch (TWI_INSTANCE_ID)
    {
        case 0:
            *(volatile uint32_t *)(0x40003000 + 0xFFC) = 0;
            break;
        case 1:
            *(volatile uint32_t *)(0x40004000 + 0xFFC) = 0;
            break;
        default:
            logt("ERROR", "Invalid TWI instance");
    }
}
#endif //!defined(SIM_ENABLED)
#endif //IS_ACTIVE(ASSET_MODULE)