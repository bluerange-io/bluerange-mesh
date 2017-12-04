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
This is meant as a small HAL Layer for abstracting some platform specific code which should make
things easier when porting to other platforms.
It is clearly meant as a work in progress.
*/

#pragma once


#include <stdint.h>
#include <stdbool.h>

extern "C"{
//TODO: Temporary inclusion until gap converts everything
#include <ble.h>
#include <ble_gap.h>
}

#define _________________GAP_DEFINITIONS______________________

/**@brief GAP Address types */
#define FH_BLE_GAP_ADDR_TYPE_PUBLIC                        0x00 /**< Public address. */
#define FH_BLE_GAP_ADDR_TYPE_RANDOM_STATIC                 0x01 /**< Random static address. */
#define FH_BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE     0x02 /**< Random private resolvable address. */
#define FH_BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_NON_RESOLVABLE 0x03 /**< Random private non-resolvable address. */

/**@brief GAP Address */
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

typedef void (*BleEventHandler) (ble_evt_t* bleEvent);
typedef void (*SystemEventHandler) (uint32_t systemEvent);
typedef void (*TimerEventHandler) (uint16_t passedTime, uint32_t appTimer);
typedef void (*ButtonEventHandler) (uint8_t buttonId, uint32_t buttonHoldTime);

class FruityHal
{
	public:
	static BleEventHandler bleEventHandler;
	static SystemEventHandler systemEventHandler;
	static TimerEventHandler timerEventHandler;

	// ######################### Ble Stack and Event Handling ############################
	static uint32_t BleStackInit(BleEventHandler bleEventHandler, SystemEventHandler systemEventHandler, TimerEventHandler timerEventHandler, ButtonEventHandler buttonEventHandler);
	static void EventLooper(BleEventHandler bleEventHandler, SystemEventHandler systemEventHandler, TimerEventHandler timerEventHandler, ButtonEventHandler buttonEventHandler );

	// ######################### GAP ############################
	static uint32_t BleGapAddressGet(fh_ble_gap_addr_t *address);
	static uint32_t BleGapAddressSet(fh_ble_gap_addr_t const *address);

	static uint32_t BleGapConnect(fh_ble_gap_addr_t const *peerAddress, fh_ble_gap_scan_params_t const *scanParams, fh_ble_gap_conn_params_t const *connectionParams);

	static uint32_t BleGapScanStart(fh_ble_gap_scan_params_t const *scanParams);
	static uint32_t BleGapScanStop();

	static uint32_t BleGapAdvStart(fh_ble_gap_adv_params_t const *advParams);
	static uint32_t BleGapAdvStop();

	static uint32_t BleTxPacketCountGet(uint16_t connectionHandle, uint8_t* count);

	// ######################### Utility ############################

	static uint32_t StartTimers();
	static uint32_t GetRtc();
	static uint32_t GetRtcDifference(uint32_t ticksTo, uint32_t ticksFrom);
	static void SystemReset();
	static uint8_t GetRebootReason();
	static uint32_t ClearRebootReason();
	static void StartWatchdog();
	static void FeedWatchdog();

	// ######################### Temporary conversion ############################
	//These functions are temporary until event handling is implemented in the HAL and events
	//are generated by the HAL
	static ble_gap_addr_t Convert(const fh_ble_gap_addr_t* address);
	static fh_ble_gap_addr_t Convert(const ble_gap_addr_t* p_addr);
};
