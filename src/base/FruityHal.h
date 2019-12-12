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
#include <PortFeatures.h>
#include <FruityHalError.h>
#include <FruityHalBleTypes.h>
#include <FruityHalBleGatt.h>
#include <FruityHalBleGap.h>

namespace FruityHal
{
typedef void(*SystemEventHandler) (u32 systemEvent);
typedef void(*TimerEventHandler) (u16 passedTimeDs);
typedef void(*ButtonEventHandler) (u8 buttonId, u32 buttonHoldTime);
typedef void(*UartEventHandler) (void);
typedef void(*AppErrorHandler) (u32 error_code);
typedef void(*StackErrorHandler) (u32 id, u32 pc, u32 info);
typedef void(*HardfaultHandler) (stacked_regs_t* stack);
typedef void(*DBDiscoveryHandler) (ble_db_discovery_evt_t * p_dbEvent);
typedef void(*EventLooperHandler) (void);

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
	BleGapAddrType getPeerAddrType() const;
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
	FruityHal::BleHciError getReason() const;
};

enum class GapTimeoutSource : u8
{
	ADVERTISING      = 0,
	SECURITY_REQUEST = 1,
	SCAN             = 2,
	CONNECTION       = 3,
	AUTH_PAYLOAD     = 4,
	INVALID          = 255
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
	FruityHal::BleGattEror getGattStatus() const;
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

struct UartReadCharBlockingResult
{
	bool didError = false;
	char c = '0';
};

struct UartReadCharResult
{
	bool hasNewChar = false;
	char c = '0';
};

	enum class TxRole : u8 {
		CONNECTION  = 0x00,  // connection
		ADVERTISING = 0x01,  // advertising
		SCAN_INIT   = 0x02   // scanning and connection initiator
	};

	enum class ClockAccuracy : u8 {
		CLOCK_ACCURACY_250_PPM = 0, //Default
		CLOCK_ACCURACY_500_PPM = 1,
		CLOCK_ACCURACY_150_PPM = 2,
		CLOCK_ACCURACY_100_PPM = 3,
		CLOCK_ACCURACY_75_PPM = 4,
		CLOCK_ACCURACY_50_PPM = 5,
		CLOCK_ACCURACY_30_PPM = 6,
		CLOCK_ACCURACY_20_PPM = 7,
		CLOCK_ACCURACY_10_PPM = 8,
		CLOCK_ACCURACY_5_PPM = 9,
		CLOCK_ACCURACY_2_PPM = 10,
		CLOCK_ACCURACY_1_PPM = 11,
	};

	// ######################### Ble Stack and Event Handling ############################
	ErrorType BleStackInit();
	void BleStackDeinit();
	void EventLooper();
	void DispatchBleEvents(void* evt);

	// ######################### GAP ############################
	u32 BleGapAddressGet(BleGapAddr *address);
	u32 BleGapAddressSet(BleGapAddr const *address);

	ErrorType BleGapConnect(BleGapAddr const *peerAddress, BleGapScanParams const *scanParams, BleGapConnParams const *connectionParams);
	u32 ConnectCancel();
	ErrorType Disconnect(u16 conn_handle, FruityHal::BleHciError hci_status_code);

	ErrorType BleGapScanStart(BleGapScanParams const *scanParams);
	ErrorType BleGapScanStop();

	ErrorType BleGapAdvStart(u8 advHandle, BleGapAdvParams const *advParams);
	ErrorType BleGapAdvDataSet(u8 * p_advHandle, BleGapAdvParams const * p_advParams, u8 *advData, u8 advDataLength, u8 *scanData, u8 scanDataLength);
	ErrorType BleGapAdvStop(u8 advHandle);

	ErrorType BleTxPacketCountGet(u16 connectionHandle, u8* count);

	u32 BleGapNameSet(BleGapConnSecMode & mode, u8 const * p_dev_name, u16 len);
	u32 BleGapAppearance(BleAppearance appearance);

	u32 BleGapConnectionParamsUpdate(u16 conn_handle, BleGapConnParams const & params);
	u32 BleGapConnectionPreferredParamsSet(BleGapConnParams const & params);

	u32 BleGapDataLengthExtensionRequest(u16 connHandle);

	u32 BleGapSecInfoReply(u16 conn_handle, BleGapEncInfo * p_info, u8 * p_id_info, u8 * p_sign_info);

	u32 BleGapEncrypt(u16 conn_handle, BleGapMasterId const & master_id, BleGapEncInfo const & enc_info);

	u32 BleGapRssiStart(u16 conn_handle, u8 threshold_dbm, u8 skip_count);
	u32 BleGapRssiStop(u16 conn_handle);

	// ######################### GATT ############################
	u32 DiscovereServiceInit(DBDiscoveryHandler dbEventHandler);
	u32 DiscoverService(u16 connHandle, const BleGattUuid& p_uuid, ble_db_discovery_t * p_discoveredServices);

	u32 BleGattSendNotification(u16 connHandle, BleGattWriteParams & params);
	u32 BleGattWrite(u16 connHandle, BleGattWriteParams const & params);

	u32 BleUuidVsAdd(u8 const * p_vs_uuid, u8 * p_uuid_type);
	u32 BleGattServiceAdd(BleGattSrvcType type, BleGattUuid const & p_uuid, u16 * p_handle);
	u32 BleGattCharAdd(u16 service_handle, BleGattCharMd const & char_md, BleGattAttribute const & attr_char_value, BleGattCharHandles & handles);

	u32 BleGattMtuExchangeRequest(u16 connHandle, u16 clientRxMtu);
	u32 BleGattMtuExchangeReply(u16 connHandle, u16 clientRxMtu);
	
	// ######################### Radio ############################
	ErrorType RadioSetTxPower(i8 tx_power, TxRole role, u16 handle);

	// ######################### Utility ############################

	ErrorType WaitForEvent();
	u32 InitializeButtons();
	ErrorType GetRandomBytes(u8 * p_data, u8 len);
	u32 ClearGeneralPurposeRegister(u32 gpregId, u32 mask);
	u32 WriteGeneralPurposeRegister(u32 gpregId, u32 mask);

	// ######################### Timers ############################

	typedef void(*TimerHandler)(void * p_context);
	typedef u32* swTimer;
	u32 InitTimers();
	u32 StartTimers();
	u32 GetRtcMs();
	u32 GetRtcDifferenceMs(u32 nowTimeMs, u32 previousTimeMs);
	ErrorType CreateTimer(swTimer timer, bool repeated, TimerHandler handler);
	ErrorType StartTimer(swTimer timer, u32 timeoutMs);
	ErrorType StopTimer(swTimer timer);

	// ######################### Utility ############################

	void SystemReset();
	RebootReason GetRebootReason();
	u32 ClearRebootReason();
	void StartWatchdog(bool safeBoot);
	void FeedWatchdog();
	u32 GetBootloaderVersion();
	void DelayUs(u32 delayMicroSeconds);
	void DelayMs(u32 delayMs);
	u32 EcbEncryptBlock(const u8 * p_key, const u8 * p_clearText, u8 * p_cipherText);

	// ######################### FLASH ############################

	ErrorType FlashPageErase(u32 page);
	ErrorType FlashWrite(u32 * p_addr, u32 * p_data, u32 len);

	// ######################### NVIC ############################

	void nvicEnableIRQ(u32 irqType);
	void nvicDisableIRQ(u32 irqType);
	void nvicSetPriorityIRQ(u32 irqType, u8 level);
	void nvicClearPendingIRQ(u32 irqType);

	// ######################### Temporary conversion ############################
	//These functions are temporary until event handling is implemented in the HAL and events
	//are generated by the HAL
	ble_gap_addr_t Convert(const BleGapAddr* address);
	BleGapAddr Convert(const ble_gap_addr_t* p_addr);

	bool setRetentionRegisterTwo(u8 val);
	void disableHardwareDfuBootloader();
	void disableUart();
	void EnableUart(bool promptAndEchoMode);
	void enableUartReadInterrupt();
	bool checkAndHandleUartTimeout();
	u32 checkAndHandleUartError();
	void UartHandleError(u32 error);
	bool UartCheckInputAvailable();
	UartReadCharBlockingResult UartReadCharBlocking();
	void UartPutStringBlockingWithTimeout(const char* message);
	void UartEnableReadInterrupt();
	bool IsUartErroredAndClear();
	bool IsUartTimedOutAndClear();
	UartReadCharResult UartReadChar();

	u32 getMasterBootRecordSize();
	u32 getSoftDeviceSize();
	u32 GetSoftDeviceVersion();
	BleStackType GetBleStackType();
	void bleStackErrorHandler(u32 id, u32 info);


	//Functions for resolving error codes
	const char* getBleEventNameString(u16 bleEventId);

	u32* getUicrDataPtr();
}

extern "C" {
	void UART0_IRQHandler(void);
}
