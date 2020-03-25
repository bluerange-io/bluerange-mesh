////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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
	enum class SystemEvents
	{
		HIGH_FREQUENCY_CLOCK_STARTED,
		POWER_FAILURE_WARNING,
		FLASH_OPERATION_SUCCESS,
		FLASH_OPERATION_ERROR,
		RADIO_BLOCKED,
		RADIO_CANCELED,
		RADIO_SIGNAL_CALLBACK_INVALID_RETURN,
		RADIO_SESSION_IDLE,
		RADIO_SESSION_CLOSED,
		UNKOWN_EVENT,
		NUMBER_OF_EVTS
	};
	
	typedef void(*SystemEventHandler) (SystemEvents systemEvent);
	typedef void(*TimerEventHandler) (u16 passedTimeDs);
	typedef void(*ButtonEventHandler) (u8 buttonId, u32 buttonHoldTime);
	typedef void(*UartEventHandler) (void);
	typedef void(*AppErrorHandler) (u32 error_code);
	typedef void(*StackErrorHandler) (u32 id, u32 pc, u32 info);
	typedef void(*HardfaultHandler) (stacked_regs_t* stack);
	typedef void(*DBDiscoveryHandler) (BleGattDBDiscoveryEvent * p_dbEvent);
	typedef void(*EventLooperHandler) (void);

	#define ______________________EVENT_DEFINITIONS_______________________

	class BleEvent
	{
	protected:
		explicit BleEvent(void const * evt);
	#ifdef SIM_ENABLED //Unfortunatly a virtual destructor is too expensive for the real firmware.
		virtual ~BleEvent();
	#endif
	};

	class GapEvent : public BleEvent 
	{
	protected:
		explicit GapEvent(void const * evt);
	public:
		u16 getConnectionHandle() const;
	};

	class GapConnParamUpdateEvent : public GapEvent 
	{
	public:
		explicit GapConnParamUpdateEvent(void const * evt);
		u16 getMaxConnectionInterval() const;
	};

	class GapRssiChangedEvent : public GapEvent
	{
	public:
		explicit GapRssiChangedEvent(void const * evt);
		i8 getRssi() const;
	};

	class GapAdvertisementReportEvent : public GapEvent
	{
	public:
		explicit GapAdvertisementReportEvent(void const * evt);
		i8 getRssi() const;
		const u8* getData() const;
		u32 getDataLength() const;
		const u8* getPeerAddr() const;
		BleGapAddrType getPeerAddrType() const;
		bool isConnectable() const;
	};

	enum class GapRole : u8 {
		INVALID    = 0,
		PERIPHERAL = 1,
		CENTRAL    = 2
	};

	class GapConnectedEvent : public GapEvent
	{
	public:
		explicit GapConnectedEvent(void const * evt);
		GapRole getRole() const;
		const u8* getPeerAddr() const;
		u8 getPeerAddrType() const;
		u16 getMinConnectionInterval() const;
	};

	class GapDisconnectedEvent : public GapEvent
	{
	public:
		explicit GapDisconnectedEvent(void const * evt);
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
		explicit GapTimeoutEvent(void const * evt);
		GapTimeoutSource getSource() const;
	};

	class GapSecurityInfoRequestEvent : public GapEvent
	{
	public:
		explicit GapSecurityInfoRequestEvent(void const * evt);
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
		explicit GapConnectionSecurityUpdateEvent(void const * evt);
		u8 getKeySize() const;
		SecurityLevel getSecurityLevel() const;
		SecurityMode getSecurityMode() const;
	};

	class GattcEvent : public BleEvent
	{
	public:
		explicit GattcEvent(void const * evt);
		u16 getConnectionHandle() const;
		FruityHal::BleGattEror getGattStatus() const;
	};

	class GattcWriteResponseEvent : public GattcEvent
	{
	public:
		explicit GattcWriteResponseEvent(void const * evt);
	};

	class GattcTimeoutEvent : public GattcEvent
	{
	public:
		explicit GattcTimeoutEvent(void const * evt);
	};

	class GattDataTransmittedEvent : public BleEvent /* Note: This is not a Gatt event because of implementation changes in the Nordic SDK. */
	{
	public:
		explicit GattDataTransmittedEvent(void const * evt);

		u16 getConnectionHandle() const;
		bool isConnectionHandleValid() const;
		u32 getCompleteCount() const;
	};

	class GattsWriteEvent : public BleEvent
	{
	public:
		explicit GattsWriteEvent(void const * evt);

		u16 getAttributeHandle() const;
		bool isWriteRequest() const;
		u16 getLength() const;
		u16 getConnectionHandle() const;
		u8 const * getData() const;
	};

	class GattcHandleValueEvent : public GattcEvent
	{
	public:
		explicit GattcHandleValueEvent(void const * evt);

		u16 getHandle() const;
		u16 getLength() const;
		u8 const * getData() const;

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

	enum class ClockSource : u8 {
		CLOCK_SOURCE_RC     = 0, 
		CLOCK_SOURCE_XTAL   = 1, 
		CLOCK_SOURCE_SYNTH  = 2
	};

	enum class GpioPullMode {
		GPIO_PIN_NOPULL			= 0,
		GPIO_PIN_PULLDOWN		= 1,
		GPIO_PIN_PULLUP			= 2
	};

	enum class GpioTransistion {
		GPIO_TRANSITION_TOGGLE      = 0,
		GPIO_TRANSITION_LOW_TO_HIGH = 1,
		GPIO_TRANSITION_HIGH_TO_LOW = 2
	};

	enum class AdcGain {
		ADC_GAIN_1_6  = 0,
		ADC_GAIN_1_5  = 1,
		ADC_GAIN_1_4  = 2,
		ADC_GAIN_1_3  = 3,
		ADC_GAIN_1_2  = 4,
		ADC_GAIN_2_3  = 5,
		ADC_GAIN_1    = 6,
		ADC_GAIN_2    = 7,
		ADC_GAIN_4    = 8,
	};

	enum class AdcResoultion {
		ADC_8_BIT		= 0,
		ADC_10_BIT	= 1,
	};

	enum class AdcReference {
		ADC_REFERENCE_0_6V              = 0,
		ADC_REFERENCE_1_2V              = 1,
		ADC_REFERENCE_1_2_POWER_SUPPLY  = 2,
		ADC_REFERENCE_1_3_POWER_SUPPLY  = 3,
		ADC_REFERENCE_1_4_POWER_SUPPLY  = 4,
	};

	enum class UartBaudrate {
		BAUDRATE_1M,
		BAUDRATE_115200,
		BAUDRATE_57600,
	};

	// ######################### Ble Stack and Event Handling ############################
	ErrorType BleStackInit();
	void BleStackDeinit();
	void EventLooper();
	u16 GetEventBufferSize();
	void DispatchBleEvents(void const * evt);
	void SetPendingEventIRQ();

	// ######################### GAP ############################
	u32 BleGapAddressGet(BleGapAddr *address);
	u32 BleGapAddressSet(BleGapAddr const *address);

	ErrorType BleGapConnect(BleGapAddr const *peerAddress, BleGapScanParams const *scanParams, BleGapConnParams const *connectionParams);
	u32 ConnectCancel();
	ErrorType Disconnect(u16 conn_handle, FruityHal::BleHciError hci_status_code);

	ErrorType BleGapScanStart(BleGapScanParams const *scanParams);
	ErrorType BleGapScanStop();

	ErrorType BleGapAdvStart(u8 *advHandle, BleGapAdvParams const *advParams);
	ErrorType BleGapAdvDataSet(u8 * p_advHandle, u8 *advData, u8 advDataLength, u8 *scanData, u8 scanDataLength);
	ErrorType BleGapAdvStop(u8 advHandle);

	ErrorType BleTxPacketCountGet(u16 connectionHandle, u8* count);

	u32 BleGapNameSet(BleGapConnSecMode & mode, u8 const * p_dev_name, u16 len);
	u32 BleGapAppearance(BleAppearance appearance);

	ErrorType BleGapConnectionParamsUpdate(u16 conn_handle, BleGapConnParams const & params);
	u32 BleGapConnectionPreferredParamsSet(BleGapConnParams const & params);

	u32 BleGapDataLengthExtensionRequest(u16 connHandle);

	u32 BleGapSecInfoReply(u16 conn_handle, BleGapEncInfo * p_info, u8 * p_id_info, u8 * p_sign_info);

	u32 BleGapEncrypt(u16 conn_handle, BleGapMasterId const & master_id, BleGapEncInfo const & enc_info);

	u32 BleGapRssiStart(u16 conn_handle, u8 threshold_dbm, u8 skip_count);
	u32 BleGapRssiStop(u16 conn_handle);

	// ######################### GATT ############################
	u32 DiscovereServiceInit(DBDiscoveryHandler dbEventHandler);
	u32 DiscoverService(u16 connHandle, const BleGattUuid& p_uuid);
	bool DiscoveryIsInProgress();

	u32 BleGattSendNotification(u16 connHandle, BleGattWriteParams & params);
	u32 BleGattWrite(u16 connHandle, BleGattWriteParams const & params);

	u32 BleUuidVsAdd(u8 const * p_vs_uuid, u8 * p_uuid_type);
	u32 BleGattServiceAdd(BleGattSrvcType type, BleGattUuid const & p_uuid, u16 * p_handle);
	u32 BleGattCharAdd(u16 service_handle, BleGattCharMd const & char_md, BleGattAttribute const & attr_char_value, BleGattCharHandles & handles);

	u32 BleGattGetMaxMtu();
	u32 BleGattMtuExchangeRequest(u16 connHandle, u16 clientRxMtu);
	u32 BleGattMtuExchangeReply(u16 connHandle, u16 clientRxMtu);
	
	// ######################### Radio ############################
	ErrorType RadioSetTxPower(i8 tx_power, TxRole role, u16 handle);


	// ######################### Bootloader ############################
	u32 GetBootloaderVersion();
	u32 GetBootloaderAddress();
	ErrorType ActivateBootloaderOnReset();

	// ######################### Utility ############################

	ErrorType WaitForEvent();
	u32 InitializeButtons();
	ErrorType GetRandomBytes(u8 * p_data, u8 len);
	u32 ClearGeneralPurposeRegister(u32 gpregId, u32 mask);
	u32 WriteGeneralPurposeRegister(u32 gpregId, u32 mask);
	bool setRetentionRegisterTwo(u8 val);

	// ######################### Timers ############################

	typedef void(*TimerHandler)(void * p_context);
	typedef u32* swTimer;
	u32 InitTimers();
	u32 StartTimers();
	u32 GetRtcMs();
	u32 GetRtcDifferenceMs(u32 nowTimeMs, u32 previousTimeMs);
	ErrorType CreateTimer(swTimer &timer, bool repeated, TimerHandler handler);
	ErrorType StartTimer(swTimer timer, u32 timeoutMs);
	ErrorType StopTimer(swTimer timer);

	// ######################### Utility ############################

	void SystemReset();
	void SystemReset(bool softdeviceEnabled);
	RebootReason GetRebootReason();
	u32 ClearRebootReason();
	void StartWatchdog(bool safeBoot);
	void FeedWatchdog();
	void DelayUs(u32 delayMicroSeconds);
	void DelayMs(u32 delayMs);
	u32 EcbEncryptBlock(const u8 * p_key, const u8 * p_clearText, u8 * p_cipherText);
	u8 ConvertPortToGpio(u8 port, u8 pin);

	// ######################### FLASH ############################

	ErrorType FlashPageErase(u32 page);
	ErrorType FlashWrite(u32 * p_addr, u32 * p_data, u32 len);

	// ######################### NVIC ############################

	void nvicEnableIRQ(u32 irqType);
	void nvicDisableIRQ(u32 irqType);
	void nvicSetPriorityIRQ(u32 irqType, u8 level);
	void nvicClearPendingIRQ(u32 irqType);

	// ######################### SERIAL COMMUNICATION ############################

	// i2c
	ErrorType twi_init(i32 sclPin, i32 sdaPin );
	void twi_uninit(i32 sclPin, i32 sdaPin);
	ErrorType twi_registerWrite(u8 slaveAddress, u8 const * pTransferData, u8 length);
	ErrorType twi_registerRead(u8 slaveAddress, u8 reg, u8 * pReceiveData, u8 length);
	ErrorType twi_read(u8 slaveAddress, u8 * pReceiveData, u8 length);
	bool twi_isInitialized(void);
	void twi_gpio_address_pin_set_and_wait(bool high, i32 sdaPin);

	//spi
	void spi_init(i32 sckPin, i32 misoPin, i32 mosiPin);
	bool spi_isInitialized(void);
	ErrorType spi_transfer(u8* const p_toWrite, u8 count, u8* const p_toRead, i32 slaveSelectPin);
	void spi_configureSlaveSelectPin(i32 pin);

	// ######################### GPIO ############################
	void GpioConfigureOutput(u32 pin);
	void GpioConfigureInput(u32 pin, GpioPullMode mode);
	void GpioConfigureDefault(u32 pin);
	void GpioPinSet(u32 pin);
	void GpioPinClear(u32 pin);
	void GpioPinToggle(u32 pin);
	typedef void (*GpioInterruptHandler)(u32 pin, GpioTransistion transistion);
	ErrorType GpioConfigureInterrupt(u32 pin, GpioPullMode mode, GpioTransistion trigger, GpioInterruptHandler handler);

	// ######################### ADC ############################

	typedef void (*AdcEventHandler)(void);
	ErrorType AdcInit(AdcEventHandler);
	void AdcUninit();
	ErrorType AdcConfigureChannel(u32 pin, AdcReference reference, AdcResoultion resolution, AdcGain gain);
	ErrorType AdcSample(i16 & buffer, u8 len); // triggers non-blocking convertion which end will be reported by calling AdcEventHandler
	u8 AdcConvertSampleToDeciVoltage(u32 sample);
	u8 AdcConvertSampleToDeciVoltage(u32 sample, u16 voltageDivider);

	// ################# USB CDC (Virtual Com Port) #############
	void VirtualComInitBeforeStack();
	void VirtualComInitAfterStack(void (*portEventHandler)(bool));
	void VirtualComProcessEvents();
	ErrorType VirtualComCheckAndProcessLine(u8* buffer, u16 bufferLength);
	void VirtualComWriteData(const u8* data, u16 dataLength);

	// ######################### UART ############################
	void disableHardwareDfuBootloader();
	void disableUart();
	void EnableUart(bool promptAndEchoMode);
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

	ErrorType getDeviceConfiguration(DeviceConfiguration & config);
	u32 * GetUserMemoryAddress();
	u32 * GetDeviceMemoryAddress();
	void GetCustomerData(u32 * p_data, u8 len);
	void WriteCustomerData(u32 * p_data, u8 len);
	u32 GetBootloaderSettingsAddress();
	u32 GetCodePageSize();
	u32 GetCodeSize();
	u32 GetDeviceId();
	void GetDeviceAddress(u8 * p_address);

	u32 GetHalMemorySize();
}

extern "C" {
	void UART0_IRQHandler(void);
}
