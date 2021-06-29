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

#pragma once


//TODO: types.h should not include references to nrf sdk
#include <FmTypes.h>
#include <PortFeatures.h>
#include <FruityHalError.h>
#include <FruityHalBleTypes.h>
#include <FruityHalBleGatt.h>
#include <FruityHalBleGap.h>

/*
 * This is the FruityMesh HAL Layer for abstracting platform specific 
 * code which makes things easier when porting to other platforms.
 */
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
    
    typedef void(*UartEventHandler) (void);
    typedef void(*AppErrorHandler) (u32 error_code);
    typedef void(*DBDiscoveryHandler) (BleGattDBDiscoveryEvent * p_dbEvent);
    typedef void(*ApplicationInterruptHandler) (void);
    typedef void(*MainContextHandler) (void);
    
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
        u16 GetConnectionHandle() const;
    };

    class GapConnParamUpdateEvent : public GapEvent 
    {
    public:
        explicit GapConnParamUpdateEvent(void const * evt);
        u16 GetMinConnectionInterval() const;
        u16 GetMaxConnectionInterval() const;
        u16 GetSlaveLatency() const;
        u16 GetConnectionSupervisionTimeout() const;
    };

    class GapConnParamUpdateRequestEvent : public GapEvent 
    {
    public:
        explicit GapConnParamUpdateRequestEvent(void const * evt);
        u16 GetMinConnectionInterval() const;
        u16 GetMaxConnectionInterval() const;
        u16 GetSlaveLatency() const;
        u16 GetConnectionSupervisionTimeout() const;
    };

    class GapRssiChangedEvent : public GapEvent
    {
    public:
        explicit GapRssiChangedEvent(void const * evt);
        i8 GetRssi() const;
    };

    class GapAdvertisementReportEvent : public GapEvent
    {
    public:
        explicit GapAdvertisementReportEvent(void const * evt);
        i8 GetRssi() const;
        const u8* GetData() const;
        u32 GetDataLength() const;
        BleGapAddrBytes GetPeerAddr() const;
        BleGapAddrType GetPeerAddrType() const;
        bool IsConnectable() const;
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
        GapRole GetRole() const;
        BleGapAddrBytes GetPeerAddr() const;
        u8 GetPeerAddrType() const;
        u16 GetMinConnectionInterval() const;
    };

    class GapDisconnectedEvent : public GapEvent
    {
    public:
        explicit GapDisconnectedEvent(void const * evt);
        FruityHal::BleHciError GetReason() const;
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
        GapTimeoutSource GetSource() const;
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
        u8 GetKeySize() const;
        SecurityLevel GetSecurityLevel() const;
        SecurityMode GetSecurityMode() const;
    };

    class GattcEvent : public BleEvent
    {
    public:
        explicit GattcEvent(void const * evt);
        u16 GetConnectionHandle() const;
        FruityHal::BleGattEror GetGattStatus() const;
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

        u16 GetConnectionHandle() const;
        bool IsConnectionHandleValid() const;
        u32 GetCompleteCount() const;
    };

    class GattsWriteEvent : public BleEvent
    {
    public:
        explicit GattsWriteEvent(void const * evt);

        u16 GetAttributeHandle() const;
        bool IsWriteRequest() const;
        u16 GetLength() const;
        u16 GetConnectionHandle() const;
        u8 const * GetData() const;
    };

    class GattcHandleValueEvent : public GattcEvent
    {
    public:
        explicit GattcHandleValueEvent(void const * evt);

        u16 GetHandle() const;
        u16 GetLength() const;
        u8 const * GetData() const;

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
        GPIO_PIN_NOPULL            = 0,
        GPIO_PIN_PULLDOWN        = 1,
        GPIO_PIN_PULLUP            = 2
    };

    enum class GpioSenseMode {
        GPIO_PIN_NOSENSE              = 0,
        GPIO_PIN_LOWSENSE             = 1,
        GPIO_PIN_HIGHSENSE            = 2
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
        ADC_8_BIT        = 0,
        ADC_10_BIT    = 1,
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
        BAUDRATE_38400,
    };

    // ######################### Ble Stack and Event Handling ############################
    ErrorType BleStackInit();
    void BleStackDeinit();
    void EventLooper();
    u16 GetEventBufferSize();
    void DispatchBleEvents(void const * evt);
    void SetPendingEventIRQ();

    // ######################### GAP ############################
    BleGapAddr GetBleGapAddress();
    ErrorType SetBleGapAddress(BleGapAddr const &address);

    ErrorType BleGapConnect(BleGapAddr const &peerAddress, BleGapScanParams const &scanParams, BleGapConnParams const &connectionParams);
    ErrorType ConnectCancel();
    ErrorType Disconnect(u16 connHandle, FruityHal::BleHciError hci_status_code);

    ErrorType BleGapScanStart(BleGapScanParams const &scanParams);
    ErrorType BleGapScanStop();

    ErrorType BleGapAdvStart(u8 *advHandle, BleGapAdvParams const &advParams);
    ErrorType BleGapAdvDataSet(u8 * p_advHandle, u8 *advData, u8 advDataLength, u8 *scanData, u8 scanDataLength);
    ErrorType BleGapAdvStop(u8 advHandle);

    ErrorType BleTxPacketCountGet(u16 connectionHandle, u8* count);

    ErrorType BleGapNameSet(const BleGapConnSecMode & mode, u8 const * p_devName, u16 len);
    ErrorType BleGapAppearance(BleAppearance appearance);

    ErrorType BleGapConnectionParamsUpdate(u16 conn_handle, BleGapConnParams const & params);
    ErrorType BleGapRejectConnectionParamsUpdate(u16 conn_handle);
    ErrorType BleGapConnectionPreferredParamsSet(BleGapConnParams const & params);

    ErrorType BleGapDataLengthExtensionRequest(u16 connHandle);

    ErrorType BleGapSecInfoReply(u16 connHandle, BleGapEncInfo * p_infoOut, u8 * p_id_info, u8 * p_sign_info);

    ErrorType BleGapEncrypt(u16 connHandle, BleGapMasterId const & masterId, BleGapEncInfo const & encInfo);

    ErrorType BleGapRssiStart(u16 connHandle, u8 thresholdDbm, u8 skipCount);
    ErrorType BleGapRssiStop(u16 connHandle);

    // ######################### GATT ############################
    ErrorType DiscovereServiceInit(DBDiscoveryHandler dbEventHandler);
    ErrorType DiscoverService(u16 connHandle, const BleGattUuid& p_uuid);
    bool DiscoveryIsInProgress();

    ErrorType BleGattSendNotification(u16 connHandle, BleGattWriteParams & params);
    ErrorType BleGattWrite(u16 connHandle, BleGattWriteParams const & params);

    ErrorType BleUuidVsAdd(u8 const * p_vsUuid, u8 * p_uuidType);
    ErrorType BleGattServiceAdd(BleGattSrvcType type, BleGattUuid const & p_uuid, u16 * p_handle);
    ErrorType BleGattCharAdd(u16 service_handle, BleGattCharMd const & charMd, BleGattAttribute const & attrCharValue, BleGattCharHandles & handles);

    u32 BleGattGetMaxMtu();
    ErrorType BleGattMtuExchangeRequest(u16 connHandle, u16 clientRxMtu);
    
    // ######################### Radio ############################
    ErrorType RadioSetTxPower(i8 txPower, TxRole role, u16 handle);


    // ######################### Bootloader ############################
    u32 GetBootloaderVersion();
    u32 GetBootloaderAddress();
    void ActivateBootloaderOnReset();

    // ######################### Utility ############################

    ErrorType WaitForEvent();
    ErrorType InitializeButtons();
    ErrorType GetRandomBytes(u8 * p_data, u8 len);
    bool SetRetentionRegisterTwo(u8 val);

    // ######################### Timers ############################

    typedef void(*TimerHandler)(void * p_context);
    typedef u32* swTimer;
    ErrorType InitTimers();
    ErrorType StartTimers();
    u32 GetRtcMs();
    u32 GetRtcDifferenceMs(u32 nowTimeMs, u32 previousTimeMs);
    ErrorType CreateTimer(swTimer &timer, bool repeated, TimerHandler handler);
    ErrorType StartTimer(swTimer timer, u32 timeoutMs);
    ErrorType StopTimer(swTimer timer);

    // ######################### Utility ############################

    void SystemReset();
    void SystemReset(bool softdeviceEnabled);
    void SystemEnterOff(bool softdeviceEnabled);
    RebootReason GetRebootReason();
    ErrorType ClearRebootReason();
    void StartWatchdog(bool safeBoot);
    void FeedWatchdog();
    void DelayUs(u32 delayMicroSeconds);
    void DelayMs(u32 delayMs);
    void EcbEncryptBlock(const u8 * p_key, const u8 * p_clearText, u8 * p_cipherText);
    u8 ConvertPortToGpio(u8 port, u8 pin);
    

    // ######################### FLASH ############################

    ErrorType FlashPageErase(u32 page);
    ErrorType FlashWrite(u32 * p_addr, u32 * p_data, u32 len);

    // ######################### NVIC ############################

    void NvicEnableIRQ(u32 irqType);
    void NvicDisableIRQ(u32 irqType);
    void NvicSetPriorityIRQ(u32 irqType, u8 level);
    void NvicClearPendingIRQ(u32 irqType);

    // ######################### SERIAL COMMUNICATION ############################

    // i2c
    ErrorType TwiInit(i32 sclPin, i32 sdaPin);
    ErrorType TwiRegisterWrite(u8 slaveAddress, u8 const * pTransferData, u8 length);
    ErrorType TwiRegisterRead(u8 slaveAddress, u8 reg, u8 * pReceiveData, u8 length);
    ErrorType TwiRead(u8 slaveAddress, u8 * pReceiveData, u8 length);
    bool TwiIsInitialized(void);
    void TwiGpioAddressPinSetAndWait(bool high, i32 sdaPin);

    //spi
    void SpiInit(i32 sckPin, i32 misoPin, i32 mosiPin);
    bool SpiIsInitialized(void);
    ErrorType SpiTransfer(u8* const p_toWrite, u8 count, u8* const p_toRead, i32 slaveSelectPin);
    void SpiConfigureSlaveSelectPin(i32 pin);

    // ######################### GPIO ############################
    void GpioConfigureOutput(u32 pin);
    void GpioConfigureInput(u32 pin, GpioPullMode mode);
    void GpioConfigureInputSense(u32 pin, GpioPullMode mode, GpioSenseMode sense);
    void GpioConfigureDefault(u32 pin);
    void GpioPinSet(u32 pin);
    void GpioPinClear(u32 pin);
    u32 GpioPinRead(u32 pin);
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
    void DisableHardwareDfuBootloader();
    void DisableUart();
    void EnableUart(bool promptAndEchoMode);
    bool CheckAndHandleUartTimeout();
    u32 CheckAndHandleUartError();
    void UartHandleError(u32 error);
    bool UartCheckInputAvailable();
    UartReadCharBlockingResult UartReadCharBlocking();
    void UartPutStringBlockingWithTimeout(const char* message);
    void UartEnableReadInterrupt();
    bool IsUartErroredAndClear();
    bool IsUartTimedOutAndClear();
    UartReadCharResult UartReadChar();

    u32 GetMasterBootRecordSize();
    u32 GetSoftDeviceSize();
    u32 GetSoftDeviceVersion();
    BleStackType GetBleStackType();
    void BleStackErrorHandler(u32 id, u32 info);

    ErrorType GetDeviceConfiguration(DeviceConfiguration & config);
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

    // ######################### Timeslot API ############################

    enum class RadioCallbackSignalType : u8
    {
        START,
        RADIO,
        TIMER0,
        EXTEND_SUCCEEDED,
        EXTEND_FAILED,
        UNKNOWN_SIGNAL_TYPE,
    };

    enum class RadioCallbackAction : u8
    {
        NONE,
        // TODO: EXTEND: not supported by the POC
        END,
        REQUEST_AND_END,
    };

    /// Open the timeslot session. This must be called before attempting to
    /// use any timeslot functionality.
    ErrorType TimeslotOpenSession();

    /// Request closing the timeslot session. The session can only be
    /// considered closed when the RADIO_SESSION_CLOSED event was received.
    void TimeslotCloseSession();

    /// Configures the request for the earliest possible timeslot of
    /// lengthMicroseconds length.
    ///
    /// The first timeslot request must always be configured using this
    /// function.
    void TimeslotConfigureNextEventEarliest(u32 lengthMicroseconds);

    /// Configures the request for the next timeslot of lengthMicroseconds
    /// length and a distance in time of distanceMicroseconds between the
    /// start of the current timeslot and the requested one.
    void TimeslotConfigureNextEventNormal(u32 lengthMicroseconds, u32 distanceMicroseconds);

    /// Request the configured timeslot.
    ErrorType TimeslotRequestNextEvent();
    
    // ######################### RADIO ############################

    /// Events that are generated by the radio peripheral.
    enum class RadioEvent
    {
        /// The radio peripheral has been disabled.
        DISABLED,
    };

    /// Enable generation of any particular event by the radio peripheral.
    void RadioUnmaskEvent(RadioEvent radioEvent);

    /// Disable generation of any particular event by the radio peripheral.
    void RadioMaskEvent(RadioEvent radioEvent);

    /// Checks if radioEvent was pending.
    bool RadioCheckEvent(RadioEvent radioEvent);

    /// Clears the pending state of radioEvent.
    void RadioClearEvent(RadioEvent radioEvent);

    /// Checks if radioEvent was pending and clears the pending state.
    bool RadioCheckAndClearEvent(RadioEvent radioEvent);

    /// Tasks of the radio peripheral that can be triggered from the firmware.
    enum class RadioTask
    {
        /// Disables the radio.
        DISABLE,
        
        /// Enables the radio in transmission mode.
        TXEN,
    };

    /// Triggers the task of the radio peripheral.
    /// Calling this function might generate an event.
    void RadioTriggerTask(RadioTask radioTask);

    /// Select the BLE advertising channel using the index as defined in the
    /// Bluetooth Core Specification 4.2, Volume 6, Part B, Section 1.4.1.
    /// Valid values for channelIndex are 37, 38 and 39.
    void RadioChooseBleAdvertisingChannel(unsigned channelIndex);

    /// Set the transmission power in dBm.
    ///
    /// Typically supported values range from -40dBm to +4dBm (nRF52832)
    /// or +8dBm (nRF52840) with varyingly sized steps inbetween.
    ///
    /// If dryRun is true this function does not write the value to the
    /// corresponding peripheral register.
    ///
    /// Returns:
    /// The actual transmission power used by the device is returned from this
    /// function and might be different from the specified hint. 
    /// NOTE: The specific rounding method is deliberately left unspecified.
    signed RadioChooseTxPowerHint(signed txPowerHint = 0, bool dryRun = false);

    /// Configure the radio for sending BLE advertisments. The PDU is
    /// specified by the packet pointer.
    void RadioHandleBleAdvTxStart(u8 *packet);
}

extern "C" {
    void UART0_IRQHandler(void);
}
