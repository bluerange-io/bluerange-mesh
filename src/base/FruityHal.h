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
This is meant as a small HAL Layer for abstracting some platform specific code which should make
things easier when porting to other platforms.
It is clearly meant as a work in progress.
*/

#pragma once


//TODO: types.h should not include references to nrf sdk
#include <types.h>

#define _________________GAP_DEFINITIONS______________________

/**@brief GAP Address types */
#define FH_BLE_GAP_ADDR_TYPE_PUBLIC                        0x00 /**< Public address. */
#define FH_BLE_GAP_ADDR_TYPE_RANDOM_STATIC                 0x01 /**< Random static address. */
#define FH_BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE     0x02 /**< Random private resolvable address. */
#define FH_BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_NON_RESOLVABLE 0x03 /**< Random private non-resolvable address. */

/**@brief GAP Address */
#define FH_BLE_SIZEOF_GAP_ADDR (7)
#define FH_BLE_GAP_ADDR_LEN (6)
typedef struct
{
  uint8_t addr_type;       /**< See FH_BLE_GAP_ADDR_TYPES. */
  uint8_t addr[FH_BLE_GAP_ADDR_LEN]; /**< 48-bit address, LSB format. */
} fh_ble_gap_addr_t;

/**@brief GAP scanning parameters. */
typedef struct
{
  uint16_t interval;            /**< Scan interval between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
  uint16_t window;              /**< Scan window between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
  uint16_t timeout;             /**< Scan timeout between 0x0001 and 0xFFFF in seconds, 0x0000 disables timeout. */
} fh_ble_gap_scan_params_t;

/**@brief GAP connection parameters. */
typedef struct
{
	uint16_t min_conn_interval;         /**< Minimum Connection Interval in 1.25 ms units, see @ref BLE_GAP_CP_LIMITS.*/
	uint16_t max_conn_interval;         /**< Maximum Connection Interval in 1.25 ms units, see @ref BLE_GAP_CP_LIMITS.*/
	uint16_t slave_latency;             /**< Slave Latency in number of connection events, see @ref BLE_GAP_CP_LIMITS.*/
	uint16_t conn_sup_timeout;          /**< Connection Supervision Timeout in 10 ms units, see @ref BLE_GAP_CP_LIMITS.*/
} fh_ble_gap_conn_params_t;

/**@brief Channel mask for RF channels used in advertising. */
typedef struct
{
  uint8_t ch_37_off : 1;  /**< Setting this bit to 1 will turn off advertising on channel 37 */
  uint8_t ch_38_off : 1;  /**< Setting this bit to 1 will turn off advertising on channel 38 */
  uint8_t ch_39_off : 1;  /**< Setting this bit to 1 will turn off advertising on channel 39 */
} fh_ble_gap_adv_ch_mask_t;


/**@brief GAP advertising parameters. */
typedef struct
{
  uint8_t               type;                 /**< See @ref BLE_GAP_ADV_TYPES. */
  uint16_t              interval;             /**< Advertising interval between 0x0020 and 0x4000 in 0.625 ms units (20 ms to 10.24 s), see @ref BLE_GAP_ADV_INTERVALS.
                                                   - If type equals @ref BLE_GAP_ADV_TYPE_ADV_DIRECT_IND, this parameter must be set to 0 for high duty cycle directed advertising.
                                                   - If type equals @ref BLE_GAP_ADV_TYPE_ADV_DIRECT_IND, set @ref BLE_GAP_ADV_INTERVAL_MIN <= interval <= @ref BLE_GAP_ADV_INTERVAL_MAX for low duty cycle advertising.*/
  uint16_t              timeout;              /**< Advertising timeout between 0x0001 and 0x3FFF in seconds, 0x0000 disables timeout. See also @ref BLE_GAP_ADV_TIMEOUT_VALUES. If type equals @ref BLE_GAP_ADV_TYPE_ADV_DIRECT_IND, this parameter must be set to 0 for High duty cycle directed advertising. */
  fh_ble_gap_adv_ch_mask_t channel_mask;         /**< Advertising channel mask. See @ref ble_gap_adv_ch_mask_t. */
} fh_ble_gap_adv_params_t;


#define ______________________EVENT_DEFINITIONS_______________________

typedef ble_evt_t fh_ble_evt_t;

typedef void (*BleEventHandler) (ble_evt_t& bleEvent);
typedef void (*SystemEventHandler) (uint32_t systemEvent);
typedef void (*TimerEventHandler) (uint16_t passedTimeDs);
typedef void (*ButtonEventHandler) (uint8_t buttonId, uint32_t buttonHoldTime);
typedef void (*UartEventHandler) (void);
typedef void (*AppErrorHandler) (uint32_t error_code);
typedef void (*StackErrorHandler) (uint32_t id, uint32_t pc, uint32_t info);
typedef void (*HardfaultHandler) (stacked_regs_t* stack);
typedef void (*DBDiscoveryHandler) (ble_db_discovery_evt_t * p_dbEvent);

namespace FruityHal
{
	extern BleEventHandler bleEventHandler;
	extern SystemEventHandler systemEventHandler;
	extern TimerEventHandler timerEventHandler;
	extern ButtonEventHandler buttonEventHandler;
	extern AppErrorHandler appErrorHandler;
	extern StackErrorHandler stackErrorHandler;
	extern HardfaultHandler hardfaultHandler;
#ifdef SIM_ENABLED
	extern DBDiscoveryHandler dbDiscoveryHandler;
#endif

	// ######################### Ble Stack and Event Handling ############################
	uint32_t SetEventHandlers(
			BleEventHandler   bleEventHandler,   SystemEventHandler systemEventHandler,
			TimerEventHandler timerEventHandler, ButtonEventHandler buttonEventHandler,
			AppErrorHandler   appErrorHandler,   StackErrorHandler  stackErrorHandler, 
			HardfaultHandler  hardfaultHandler);
	uint32_t BleStackInit();
	void EventLooper();
	void SetUartHandler(UartEventHandler uartEventHandler);
	UartEventHandler GetUartHandler();

	// ######################### GAP ############################
	uint32_t BleGapAddressGet(fh_ble_gap_addr_t *address);
	uint32_t BleGapAddressSet(fh_ble_gap_addr_t const *address);

	uint32_t BleGapConnect(fh_ble_gap_addr_t const *peerAddress, fh_ble_gap_scan_params_t const *scanParams, fh_ble_gap_conn_params_t const *connectionParams);
	uint32_t ConnectCancel();
	uint32_t Disconnect(uint16_t conn_handle, uint8_t hci_status_code);

	uint32_t BleGapScanStart(fh_ble_gap_scan_params_t const *scanParams);
	uint32_t BleGapScanStop();

	uint32_t BleGapAdvStart(fh_ble_gap_adv_params_t const *advParams);
	uint32_t BleGapAdvDataSet(const uint8_t *advData, uint8_t advDataLength, const uint8_t *scanData, uint8_t scanDataLength);
	uint32_t BleGapAdvStop();

	uint32_t BleTxPacketCountGet(uint16_t connectionHandle, uint8_t* count);
	
	// ######################### GATT ############################
	uint32_t DiscovereServiceInit(DBDiscoveryHandler dbEventHandler);
	uint32_t DiscoverService(u16 connHandle, const ble_uuid_t& p_uuid, ble_db_discovery_t * p_discoveredServices);

	// ######################### Utility ############################

	uint32_t InitializeButtons();

	// ######################### Timers ############################

	uint32_t StartTimers();
	uint32_t GetRtc();
	uint32_t GetRtcDifference(uint32_t ticksTo, uint32_t ticksFrom);

	// ######################### Utility ############################

	void SystemReset();
	uint8_t GetRebootReason();
	uint32_t ClearRebootReason();
	void StartWatchdog();
	void FeedWatchdog();
	uint32_t GetBootloaderVersion();
	void DelayMs(u32 delayMs);

	// ######################### Temporary conversion ############################
	//These functions are temporary until event handling is implemented in the HAL and events
	//are generated by the HAL
	ble_gap_addr_t Convert(const fh_ble_gap_addr_t* address);
	fh_ble_gap_addr_t Convert(const ble_gap_addr_t* p_addr);

};

extern "C" {
	void UART0_IRQHandler(void);
}
