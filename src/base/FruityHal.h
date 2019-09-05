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

typedef void(*SystemEventHandler) (u32 systemEvent);
typedef void(*TimerEventHandler) (uint16_t passedTimeDs);
typedef void(*ButtonEventHandler) (uint8_t buttonId, u32 buttonHoldTime);
typedef void(*UartEventHandler) (void);
typedef void(*AppErrorHandler) (u32 error_code);
typedef void(*StackErrorHandler) (u32 id, u32 pc, u32 info);
typedef void(*HardfaultHandler) (stacked_regs_t* stack);
typedef void(*DBDiscoveryHandler) (ble_db_discovery_evt_t * p_dbEvent);
typedef void(*EventLooperHandler) (void);

#define _________________GAP_DEFINITIONS______________________

/**@brief GAP Address types */
#define FH_BLE_GAP_ADDR_TYPE_PUBLIC                        0x00 /**< Public address. */
#define FH_BLE_GAP_ADDR_TYPE_RANDOM_STATIC                 0x01 /**< Random static address. */
#define FH_BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE     0x02 /**< Random private resolvable address. */
#define FH_BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_NON_RESOLVABLE 0x03 /**< Random private non-resolvable address. */

/**@brief GAP Address */
#define FH_BLE_SIZEOF_GAP_ADDR (7)
#define FH_BLE_GAP_ADDR_LEN (6)
struct fh_ble_gap_addr_t
{
public:
  uint8_t addr_type;       /**< See FH_BLE_GAP_ADDR_TYPES. */
  uint8_t addr[FH_BLE_GAP_ADDR_LEN]; /**< 48-bit address, LSB format. */
};

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


enum class GapAdvType : u8
{
#if SDK == 15
	ADV_IND         = 0x01,
	ADV_DIRECT_IND  = 0x03,
	ADV_SCAN_IND    = 0x04,
	ADV_NONCONN_IND = 0x05,
#else
	ADV_IND         = 0x00,
	ADV_DIRECT_IND  = 0x01,
	ADV_SCAN_IND    = 0x02,
	ADV_NONCONN_IND = 0x03,
#endif
};


/**@brief GAP advertising parameters. */
struct fh_ble_gap_adv_params_t
{
	GapAdvType               type;
	uint16_t                 interval;             /**< Advertising interval between 0x0020 and 0x4000 in 0.625 ms units (20 ms to 10.24 s), see @ref BLE_GAP_ADV_INTERVALS.
	                                                 - If type equals @ref BLE_GAP_ADV_TYPE_ADV_DIRECT_IND, this parameter must be set to 0 for high duty cycle directed advertising.
	                                                 - If type equals @ref BLE_GAP_ADV_TYPE_ADV_DIRECT_IND, set @ref BLE_GAP_ADV_INTERVAL_MIN <= interval <= @ref BLE_GAP_ADV_INTERVAL_MAX for low duty cycle advertising.*/
	uint16_t                 timeout;              /**< Advertising timeout between 0x0001 and 0x3FFF in seconds, 0x0000 disables timeout. See also @ref BLE_GAP_ADV_TIMEOUT_VALUES. If type equals @ref BLE_GAP_ADV_TYPE_ADV_DIRECT_IND, this parameter must be set to 0 for High duty cycle directed advertising. */
	fh_ble_gap_adv_ch_mask_t channel_mask;         /**< Advertising channel mask. See @ref ble_gap_adv_ch_mask_t. */
};


#define ______________________EVENT_DEFINITIONS_______________________

class BleEvent
{
protected:
	explicit BleEvent(void* evt);
#ifdef SIM_ENABLED //Unfortunatly a virtual destructor is too expensive for the real firmware.
	virtual ~BleEvent();
#endif
};

class GapEvent : public BleEvent 
{
protected:
	explicit GapEvent(void* evt);
public:
	u16 getConnectionHandle() const;
};

class GapConnParamUpdateEvent : public GapEvent 
{
public:
	explicit GapConnParamUpdateEvent(void* evt);
	u16 getMaxConnectionInterval() const;
};

class GapRssiChangedEvent : public GapEvent
{
public:
	explicit GapRssiChangedEvent(void* evt);
	i8 getRssi() const;
};

class GapAdvertisementReportEvent : public GapEvent
{
#if IS_ACTIVE(FAKE_NODE_POSITIONS)
	i8 fakeRssi;
	bool fakeRssiSet = false;
#endif
public:
	explicit GapAdvertisementReportEvent(void* evt);
	i8 getRssi() const;
	const u8* getData() const;
	u32 getDataLength() const;
	const u8* getPeerAddr() const;
	u8 getPeerAddrType() const;
	bool isConnectable() const;

#if IS_ACTIVE(FAKE_NODE_POSITIONS)
	void setFakeRssi(i8 rssi);
#endif
};

enum class GapRole : u8 {
	INVALID    = 0,
	PERIPHERAL = 1,
	CENTRAL    = 2
};

class GapConnectedEvent : public GapEvent
{
public:
	explicit GapConnectedEvent(void* evt);
	GapRole getRole() const;
	const u8* getPeerAddr() const;
	u8 getPeerAddrType() const;
	u16 getMinConnectionInterval() const;
};

class GapDisconnectedEvent : public GapEvent
{
public:
	explicit GapDisconnectedEvent(void* evt);
	u8 getReason() const;
};

enum class GapTimeoutSource : u8
{
	ADVERTISING      = 0,
	SECURITY_REQUEST = 1,
	SCAN             = 2,
	CONNECTION       = 3
};

class GapTimeoutEvent : public GapEvent
{
public:
	explicit GapTimeoutEvent(void* evt);
	GapTimeoutSource getSource() const;
};

class GapSecurityInfoRequestEvent : public GapEvent
{
public:
	explicit GapSecurityInfoRequestEvent(void* evt);
};

enum class SecurityMode : u8
{
	NO_PERMISSION = 0,
	ONE = 1,
	TWO = 2,
};

enum class SecurityLevel : u8
{
	NO_PERMISSION = 0,
	//These are the valid security levels.
	ONE   = 1,
	TWO   = 2,
	THREE = 3,
	FOUR  = 4
};

class GapConnectionSecurityUpdateEvent : public GapEvent
{
public:
	explicit GapConnectionSecurityUpdateEvent(void* evt);
	u8 getKeySize() const;
	SecurityLevel getSecurityLevel() const;
	SecurityMode getSecurityMode() const;
};

class GattcEvent : public BleEvent
{
public:
	explicit GattcEvent(void* evt);
	u16 getConnectionHandle() const;
	u16 getGattStatus() const;
};

class GattcWriteResponseEvent : public GattcEvent
{
public:
	explicit GattcWriteResponseEvent(void* evt);
};

class GattcTimeoutEvent : public GattcEvent
{
public:
	explicit GattcTimeoutEvent(void* evt);
};

class GattDataTransmittedEvent : public BleEvent /* Note: This is not a Gatt event because of implementation changes in the Nordic SDK. */
{
public:
	explicit GattDataTransmittedEvent(void* evt);

	u16 getConnectionHandle() const;
	bool isConnectionHandleValid() const;
	u32 getCompleteCount() const;
};

class GattsWriteEvent : public BleEvent
{
public:
	explicit GattsWriteEvent(void* evt);

	u16 getAttributeHandle() const;
	bool isWriteRequest() const;
	u16 getLength() const;
	u16 getConnectionHandle() const;
	u8* getData() const;
};

class GattcHandleValueEvent : public GattcEvent
{
public:
	explicit GattcHandleValueEvent(void* evt);

	u16 getHandle() const;
	u16 getLength() const;
	u8* getData() const;

};


namespace FruityHal
{
	constexpr u32 SUCCESS = 0;
	enum class GeneralHardwareError : u32 {
		SUCCESS = FruityHal::SUCCESS,
		//Variables of this type may have any u32 value! Their interpretation is hardware dependent.
		HIGHEST_POSSIBLE_ERROR = 0xFFFFFFFF,
	};
	enum class HciErrorCode : u8 {
		SUCCESS                                     = FruityHal::SUCCESS, //0
		UNKNOWN_BTLE_COMMAND                        = 0x01, //1
		UNKNOWN_CONNECTION_IDENTIFIER               = 0x02, //2
		AUTHENTICATION_FAILURE                      = 0x05, //5
		PIN_OR_KEY_MISSING                          = 0x06, //6
		MEMORY_CAPACITY_EXCEEDED                    = 0x07, //7
		CONNECTION_TIMEOUT                          = 0x08, //8
		COMMAND_DISALLOWED                          = 0x0C, //12
		INVALID_BTLE_COMMAND_PARAMETERS             = 0x12, //18
		REMOTE_USER_TERMINATED_CONNECTION           = 0x13, //19
		REMOTE_DEV_TERMINATION_DUE_TO_LOW_RESOURCES = 0x14, //20
		REMOTE_DEV_TERMINATION_DUE_TO_POWER_OFF     = 0x15, //21
		LOCAL_HOST_TERMINATED_CONNECTION            = 0x16, //22
		UNSUPPORTED_REMOTE_FEATURE                  = 0x1A, //26
		INVALID_LMP_PARAMETERS                      = 0x1E, //30
		UNSPECIFIED_ERROR                           = 0x1F, //31
		LMP_RESPONSE_TIMEOUT                        = 0x22, //34
		LMP_PDU_NOT_ALLOWED                         = 0x24, //36
		INSTANT_PASSED                              = 0x28, //40
		PAIRING_WITH_UNIT_KEY_UNSUPPORTED           = 0x29, //41
		DIFFERENT_TRANSACTION_COLLISION             = 0x2A, //42
		CONTROLLER_BUSY                             = 0x3A, //58
		CONN_INTERVAL_UNACCEPTABLE                  = 0x3B, //59
		DIRECTED_ADVERTISER_TIMEOUT                 = 0x3C, //60
		CONN_TERMINATED_DUE_TO_MIC_FAILURE          = 0x3D, //61
		CONN_FAILED_TO_BE_ESTABLISHED               = 0x3E, //62
	};

	// ######################### Ble Stack and Event Handling ############################
	GeneralHardwareError BleStackInit();
	void EventLooper();
	void DispatchBleEvents(void* evt);

	// ######################### GAP ############################
	u32 BleGapAddressGet(fh_ble_gap_addr_t *address);
	u32 BleGapAddressSet(fh_ble_gap_addr_t const *address);

	u32 BleGapConnect(fh_ble_gap_addr_t const *peerAddress, fh_ble_gap_scan_params_t const *scanParams, fh_ble_gap_conn_params_t const *connectionParams);
	u32 ConnectCancel();
	u32 Disconnect(uint16_t conn_handle, FruityHal::HciErrorCode hci_status_code);

#if (SDK == 15)
	u32 BleGapScanStart(fh_ble_gap_scan_params_t const *scanParams, u8 * p_scanData);
#else
	u32 BleGapScanStart(fh_ble_gap_scan_params_t const *scanParams);
#endif
	u32 BleGapScanStop();

#if (SDK == 15)
	u32 BleGapAdvStart(u8 advHandle);
	u32 BleGapAdvDataSet(u8 * p_advHandle, fh_ble_gap_adv_params_t const * p_advParams, const uint8_t *advData, uint8_t advDataLength, const uint8_t *scanData, uint8_t scanDataLength);
	u32 BleGapAdvStop(u8 advHandle);
#else
	u32 BleGapAdvStart(fh_ble_gap_adv_params_t const *advParams);
	u32 BleGapAdvDataSet(const uint8_t *advData, uint8_t advDataLength, const uint8_t *scanData, uint8_t scanDataLength);
	u32 BleGapAdvStop();
#endif

	u32 BleTxPacketCountGet(uint16_t connectionHandle, uint8_t* count);

	// ######################### GATT ############################
	u32 DiscovereServiceInit(DBDiscoveryHandler dbEventHandler);
	u32 DiscoverService(u16 connHandle, const ble_uuid_t& p_uuid, ble_db_discovery_t * p_discoveredServices);

	// ######################### Utility ############################

	u32 InitializeButtons();

	// ######################### Timers ############################

	u32 InitTimers();
	u32 StartTimers();
	u32 GetRtc();
	u32 GetRtcDifference(u32 ticksTo, u32 ticksFrom);

	// ######################### Utility ############################

	void SystemReset();
	RebootReason GetRebootReason();
	u32 ClearRebootReason();
	void StartWatchdog(bool safeBoot);
	void FeedWatchdog();
	u32 GetBootloaderVersion();
	void DelayUs(u32 delayMicroSeconds);
	void DelayMs(u32 delayMs);

	// ######################### Temporary conversion ############################
	//These functions are temporary until event handling is implemented in the HAL and events
	//are generated by the HAL
	ble_gap_addr_t Convert(const fh_ble_gap_addr_t* address);
	fh_ble_gap_addr_t Convert(const ble_gap_addr_t* p_addr);

	bool setRetentionRegisterTwo(u8 val);
	void disableHardwareDfuBootloader();
	void disableUart();
	void enableUart();
	void enableUartReadInterrupt();
	bool checkAndHandleUartTimeout();
	u32 checkAndHandleUartError();
	bool readUartByte(char* outByte);

	u32 getMasterBootRecordSize();
	u32 getSoftDeviceSize();
	BleStackType GetBleStackType();
	void bleStackErrorHandler(u32 id, u32 info);


	//Functions for resolving error codes
	const char* getBleEventNameString(u16 bleEventId);
	const char* getGattStatusErrorString(u16 gattStatusCode);
	const char* getGeneralErrorString(GeneralHardwareError nrfErrorCode);
	const char* getHciErrorString(FruityHal::HciErrorCode hciErrorCode);
	const char* getErrorLogErrorType(ErrorTypes type);
	const char* getErrorLogCustomError(CustomErrorTypes type);
	const char* getErrorLogRebootReason(RebootReason type);
	const char* getErrorLogError(ErrorTypes type, u32 code);

	u32* getUicrDataPtr();
}

extern "C" {
	void UART0_IRQHandler(void);
}
