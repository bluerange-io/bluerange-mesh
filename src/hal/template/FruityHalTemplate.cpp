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
 * This is the HAL template with empty implementation
 */

#include "FruityHal.h"
#include <FmTypes.h>


// ######################### CLASS ############################
FruityHal::GapConnParamUpdateEvent::GapConnParamUpdateEvent(void const* _evt)
    :GapEvent(_evt) {}
FruityHal::GapEvent::GapEvent(void const* _evt)
    : BleEvent(_evt) {}
u16 FruityHal::GapEvent::GetConnectionHandle() const { return 0; } 
u16 FruityHal::GapConnParamUpdateEvent::GetMaxConnectionInterval() const { return 0; } 
FruityHal::GapRssiChangedEvent::GapRssiChangedEvent(void const* _evt)
    :GapEvent(_evt) {}
i8 FruityHal::GapRssiChangedEvent::GetRssi() const { return 0; }
FruityHal::GapAdvertisementReportEvent::GapAdvertisementReportEvent(void const* _evt)
    :GapEvent(_evt) {}
i8 FruityHal::GapAdvertisementReportEvent::GetRssi() const { return 0; }
const u8 * FruityHal::GapAdvertisementReportEvent::GetData() const { return 0; }
u32 FruityHal::GapAdvertisementReportEvent::GetDataLength() const { return 0; } 
FruityHal::BleGapAddrBytes FruityHal::GapAdvertisementReportEvent::GetPeerAddr() const { return FruityHal::BleGapAddrBytes(); }
FruityHal::BleGapAddrType FruityHal::GapAdvertisementReportEvent::GetPeerAddrType() const { return FruityHal::BleGapAddrType::PUBLIC; }
bool FruityHal::GapAdvertisementReportEvent::IsConnectable() const { return false; } 
FruityHal::BleEvent::BleEvent(void const* _evt) {}
FruityHal::GapConnectedEvent::GapConnectedEvent(void const* _evt)
    :GapEvent(_evt) {}
FruityHal::GapRole FruityHal::GapConnectedEvent::GetRole() const { return FruityHal::GapRole::INVALID; }
u8 FruityHal::GapConnectedEvent::GetPeerAddrType() const { return 0; } 
u16 FruityHal::GapConnectedEvent::GetMinConnectionInterval() const { return 0; } 
FruityHal::BleGapAddrBytes FruityHal::GapConnectedEvent::GetPeerAddr() const { return FruityHal::BleGapAddrBytes(); }
FruityHal::GapDisconnectedEvent::GapDisconnectedEvent(void const* _evt)
    : GapEvent(_evt) {}
FruityHal::BleHciError FruityHal::GapDisconnectedEvent::GetReason() const { return FruityHal::BleHciError::SUCCESS; }
FruityHal::GapTimeoutEvent::GapTimeoutEvent(void const* _evt)
    : GapEvent(_evt) {}
FruityHal::GapTimeoutSource FruityHal::GapTimeoutEvent::GetSource() const  { return FruityHal::GapTimeoutSource::INVALID; }
FruityHal::GapSecurityInfoRequestEvent::GapSecurityInfoRequestEvent(void const* _evt)
    : GapEvent(_evt) {}
FruityHal::GapConnectionSecurityUpdateEvent::GapConnectionSecurityUpdateEvent(void const* _evt)
    : GapEvent(_evt) {}
u8 FruityHal::GapConnectionSecurityUpdateEvent::GetKeySize() const { return 0; } 
FruityHal::SecurityLevel FruityHal::GapConnectionSecurityUpdateEvent::GetSecurityLevel() const { return FruityHal::SecurityLevel::NO_PERMISSION; }
FruityHal::SecurityMode FruityHal::GapConnectionSecurityUpdateEvent::GetSecurityMode() const { return FruityHal::SecurityMode::NO_PERMISSION; }
FruityHal::GattcEvent::GattcEvent(void const* _evt)
    : BleEvent(_evt) {}
u16 FruityHal::GattcEvent::GetConnectionHandle() const { return 0; } 
FruityHal::BleGattEror FruityHal::GattcEvent::GetGattStatus() const { return FruityHal::BleGattEror::SUCCESS; }
FruityHal::GattcWriteResponseEvent::GattcWriteResponseEvent(void const* _evt)
    : GattcEvent(_evt) {}
FruityHal::GattcTimeoutEvent::GattcTimeoutEvent(void const* _evt)
    : GattcEvent(_evt) {}
FruityHal::GattDataTransmittedEvent::GattDataTransmittedEvent(void const* _evt)
    :BleEvent(_evt) {}
u16 FruityHal::GattDataTransmittedEvent::GetConnectionHandle() const { return 0; } 
bool FruityHal::GattDataTransmittedEvent::IsConnectionHandleValid() const { return false; } 
u32 FruityHal::GattDataTransmittedEvent::GetCompleteCount() const { return 0; } 
FruityHal::GattsWriteEvent::GattsWriteEvent(void const* _evt)
    : BleEvent(_evt) {}
u16 FruityHal::GattsWriteEvent::GetAttributeHandle() const { return 0; } 
bool FruityHal::GattsWriteEvent::IsWriteRequest() const { return false; } 
u16 FruityHal::GattsWriteEvent::GetLength() const { return 0; } 
u16 FruityHal::GattsWriteEvent::GetConnectionHandle() const { return 0; } 
u8 const * FruityHal::GattsWriteEvent::GetData() const { return 0; } 
FruityHal::GattcHandleValueEvent::GattcHandleValueEvent(void const* _evt)
    :GattcEvent(_evt) {}
u16 FruityHal::GattcHandleValueEvent::GetHandle() const { return 0; } 
u16 FruityHal::GattcHandleValueEvent::GetLength() const { return 0; } 
u8 const * FruityHal::GattcHandleValueEvent::GetData() const { return 0; } 


// ######################### Ble Stack and Event Handling ############################
ErrorType FruityHal::BleStackInit(){ return ErrorType::SUCCESS; }
void FruityHal::BleStackDeinit(){ }
void FruityHal::EventLooper(){ }
u16 FruityHal::GetEventBufferSize(){ return 0; }
void FruityHal::DispatchBleEvents(void const * evt){ }

// ######################### GAP ############################
FruityHal::BleGapAddr FruityHal::GetBleGapAddress(){ return FruityHal::BleGapAddr(); }
ErrorType FruityHal::SetBleGapAddress(BleGapAddr const &address){ return ErrorType::SUCCESS; }

ErrorType FruityHal::BleGapConnect(BleGapAddr const &peerAddress, BleGapScanParams const &scanParams, BleGapConnParams const &connectionParams){ return ErrorType::SUCCESS; }
ErrorType FruityHal::ConnectCancel(){ return ErrorType::SUCCESS; }
ErrorType FruityHal::Disconnect(u16 conn_handle, FruityHal::BleHciError hci_status_code){ return ErrorType::SUCCESS; }

ErrorType FruityHal::BleGapScanStart(BleGapScanParams const &scanParams){ return ErrorType::SUCCESS; }
ErrorType FruityHal::BleGapScanStop(){ return ErrorType::SUCCESS; }

ErrorType FruityHal::BleGapAdvStart(u8 * advHandle, BleGapAdvParams const &advParams){ return ErrorType::SUCCESS; }
ErrorType FruityHal::BleGapAdvDataSet(u8 * p_advHandle, u8 *advData, u8 advDataLength, u8 *scanData, u8 scanDataLength){ return ErrorType::SUCCESS; }
ErrorType FruityHal::BleGapAdvStop(u8 advHandle){ return ErrorType::SUCCESS; }

ErrorType FruityHal::BleTxPacketCountGet(u16 connectionHandle, u8* count){ return ErrorType::SUCCESS; }

ErrorType FruityHal::BleGapNameSet(const BleGapConnSecMode & mode, u8 const * p_dev_name, u16 len){ return ErrorType::SUCCESS; }
ErrorType FruityHal::BleGapAppearance(BleAppearance appearance){ return ErrorType::SUCCESS; }

ErrorType FruityHal::BleGapConnectionParamsUpdate(u16 conn_handle, BleGapConnParams const & params){ return ErrorType::SUCCESS; }
ErrorType FruityHal::BleGapConnectionPreferredParamsSet(BleGapConnParams const & params){ return ErrorType::SUCCESS; }

ErrorType FruityHal::BleGapDataLengthExtensionRequest(u16 connHandle){ return ErrorType::SUCCESS; }

ErrorType FruityHal::BleGapSecInfoReply(u16 conn_handle, BleGapEncInfo * p_info, u8 * p_id_info, u8 * p_sign_info){ return ErrorType::SUCCESS; }

ErrorType FruityHal::BleGapEncrypt(u16 conn_handle, BleGapMasterId const & master_id, BleGapEncInfo const & enc_info){ return ErrorType::SUCCESS; }

ErrorType FruityHal::BleGapRssiStart(u16 conn_handle, u8 threshold_dbm, u8 skip_count){ return ErrorType::SUCCESS; }
ErrorType FruityHal::BleGapRssiStop(u16 conn_handle){ return ErrorType::SUCCESS; }

// ######################### GATT ############################
ErrorType FruityHal::DiscovereServiceInit(DBDiscoveryHandler dbEventHandler){ return ErrorType::SUCCESS; }
ErrorType FruityHal::DiscoverService(u16 connHandle, const BleGattUuid &p_uuid){ return ErrorType::SUCCESS; }
bool FruityHal::DiscoveryIsInProgress(){ return true; }

ErrorType FruityHal::BleGattSendNotification(u16 connHandle, BleGattWriteParams & params){ return ErrorType::SUCCESS; }
ErrorType FruityHal::BleGattWrite(u16 connHandle, BleGattWriteParams const & params){ return ErrorType::SUCCESS; }

ErrorType FruityHal::BleUuidVsAdd(u8 const * p_vs_uuid, u8 * p_uuid_type){ return ErrorType::SUCCESS; }
ErrorType FruityHal::BleGattServiceAdd(BleGattSrvcType type, BleGattUuid const & p_uuid, u16 * p_handle){ return ErrorType::SUCCESS; }
ErrorType FruityHal::BleGattCharAdd(u16 service_handle, BleGattCharMd const & char_md, BleGattAttribute const & attr_char_value, BleGattCharHandles & handles){ return ErrorType::SUCCESS; }

ErrorType FruityHal::BleGattMtuExchangeRequest(u16 connHandle, u16 clientRxMtu){ return ErrorType::SUCCESS; }
u32 FruityHal::BleGattGetMaxMtu(){ return 0; }

// ######################### Radio ############################
ErrorType FruityHal::RadioSetTxPower(i8 tx_power, TxRole role, u16 handle){ return ErrorType::SUCCESS; }

// ######################### Utility ############################

ErrorType FruityHal::WaitForEvent(){ return ErrorType::SUCCESS; }
ErrorType FruityHal::InitializeButtons(){ return ErrorType::SUCCESS; }
ErrorType FruityHal::GetRandomBytes(u8 * p_data, u8 len){ return ErrorType::SUCCESS; }

// ######################### Timers ############################
ErrorType FruityHal::InitTimers(){ return ErrorType::SUCCESS; }
ErrorType FruityHal::StartTimers(){ return ErrorType::SUCCESS; }
u32 FruityHal::GetRtcMs(){ return 0; }
u32 FruityHal::GetRtcDifferenceMs(u32 nowTimeMs, u32 previousTimeMs){ return 0; }
ErrorType FruityHal::CreateTimer(swTimer &timer, bool repeated, TimerHandler handler){ return ErrorType::SUCCESS; }
ErrorType FruityHal::StartTimer(swTimer timer, u32 timeoutMs){ return ErrorType::SUCCESS; }
ErrorType FruityHal::StopTimer(swTimer timer){ return ErrorType::SUCCESS; }

// ######################### Utility ############################

void FruityHal::SystemReset(){ }
void FruityHal::SystemReset(bool softdeviceEnabled){ }
void FruityHal::SystemEnterOff(bool softdeviceEnabled){ }
RebootReason FruityHal::GetRebootReason(){ return RebootReason::UNKNOWN; }
ErrorType FruityHal::ClearRebootReason(){ return ErrorType::SUCCESS; }
void FruityHal::StartWatchdog(bool safeBoot){ }
void FruityHal::FeedWatchdog(){ }
u32 FruityHal::GetBootloaderVersion(){ return 0; }
u32 FruityHal::GetBootloaderAddress(){ return 0; }
void FruityHal::ActivateBootloaderOnReset(){ }
void FruityHal::DelayUs(u32 delayMicroSeconds){ }
void FruityHal::DelayMs(u32 delayMs){ }
void FruityHal::EcbEncryptBlock(const u8 * p_key, const u8 * p_clearText, u8 * p_cipherText){ }
u8 FruityHal::ConvertPortToGpio(u8 port, u8 pin){ return 0; }

// ######################### FLASH ############################

ErrorType FruityHal::FlashPageErase(u32 page){ return ErrorType::SUCCESS; }
ErrorType FruityHal::FlashWrite(u32 * p_addr, u32 * p_data, u32 len){ return ErrorType::SUCCESS; }

// ######################### NVIC ############################

void FruityHal::NvicEnableIRQ(u32 irqType) { }
void FruityHal::NvicDisableIRQ(u32 irqType) { }
void FruityHal::NvicSetPriorityIRQ(u32 irqType, u8 level) { }
void FruityHal::NvicClearPendingIRQ(u32 irqType) { }

// ######################### SERIAL COMMUNICATION ############################

// i2c
ErrorType FruityHal::TwiInit(i32 sclPin, i32 sdaPin) { return ErrorType::SUCCESS; }
ErrorType FruityHal::TwiRegisterWrite(u8 slaveAddress, u8 const * pTransferData, u8 length) { return ErrorType::SUCCESS; }
ErrorType FruityHal::TwiRegisterRead(u8 slaveAddress, u8 reg, u8 * pReceiveData, u8 length) { return ErrorType::SUCCESS; }
ErrorType FruityHal::TwiRead(u8 slaveAddress, u8 * pReceiveData, u8 length) { return ErrorType::SUCCESS; }
bool FruityHal::TwiIsInitialized(void) { return false; }
void FruityHal::TwiGpioAddressPinSetAndWait(bool high, i32 sdaPin) { }

//spi
void FruityHal::SpiInit(i32 sckPin, i32 misoPin, i32 mosiPin) { }
bool FruityHal::SpiIsInitialized(void) { return false; }
ErrorType FruityHal::SpiTransfer(u8* const p_toWrite, u8 count, u8* const p_toRead, i32 slaveSelectPin) { return ErrorType::SUCCESS; }
void FruityHal::SpiConfigureSlaveSelectPin(i32 pin) { }

// ######################### GPIO ############################
void FruityHal::GpioConfigureOutput(u32 pin){ }
void FruityHal::GpioConfigureInput(u32 pin, GpioPullMode mode){ }
void FruityHal::GpioConfigureInputSense(u32 pin, GpioPullMode mode, GpioSenseMode sense){ }
void FruityHal::GpioConfigureDefault(u32 pin){ }
void FruityHal::GpioPinSet(u32 pin){ }
void FruityHal::GpioPinClear(u32 pin){ }
void FruityHal::GpioPinToggle(u32 pin){ }
u32 FruityHal::GpioPinRead(u32 pin){ return 0; }
ErrorType FruityHal::GpioConfigureInterrupt(u32 pin, GpioPullMode mode, GpioTransistion trigger, GpioInterruptHandler handler){ return ErrorType::SUCCESS; }

// ######################### ADC ############################

ErrorType FruityHal::AdcInit(AdcEventHandler){ return ErrorType::SUCCESS; }
void FruityHal::AdcUninit(){ }
ErrorType FruityHal::AdcConfigureChannel(u32 pin, AdcReference reference, AdcResoultion resolution, AdcGain gain){ return ErrorType::SUCCESS; }
ErrorType FruityHal::AdcSample(i16 & buffer, u8 len){ return ErrorType::SUCCESS; } // triggers non-blocking convertion which end will be reported by calling AdcEventHandler
u8 FruityHal::AdcConvertSampleToDeciVoltage(u32 sample){ return 0; }
u8 FruityHal::AdcConvertSampleToDeciVoltage(u32 sample, u16 voltageDivider){ return 0; }

// ######################### Temporary conversion ############################
//These functions are temporary until event handling is implemented in the HAL and events
//are generated by the HAL

bool FruityHal::SetRetentionRegisterTwo(u8 val){ return false; }
void FruityHal::DisableHardwareDfuBootloader(){ }
void FruityHal::DisableUart(){ }
void FruityHal::EnableUart(bool promptAndEchoMode){ }
bool FruityHal::CheckAndHandleUartTimeout(){ return false; }
u32 FruityHal::CheckAndHandleUartError(){ return 0; }
void FruityHal::UartHandleError(u32 error){ }
bool FruityHal::UartCheckInputAvailable(){ return false; }
FruityHal::UartReadCharBlockingResult FruityHal::UartReadCharBlocking(){ UartReadCharBlockingResult ret; ret.didError = false; return ret; }
void FruityHal::UartPutStringBlockingWithTimeout(const char* message){ }
void FruityHal::UartEnableReadInterrupt(){ }
bool FruityHal::IsUartErroredAndClear(){ return false; }
bool FruityHal::IsUartTimedOutAndClear(){ return false; }
FruityHal::UartReadCharResult FruityHal::UartReadChar(){ UartReadCharResult ret; ret.hasNewChar = false; return ret; }

u32 FruityHal::GetMasterBootRecordSize(){ return 0; }
u32 FruityHal::GetSoftDeviceSize(){ return 0; }
u32 FruityHal::GetSoftDeviceVersion(){ return 0; }
BleStackType FruityHal::GetBleStackType(){ return BleStackType::INVALID; }
void FruityHal::BleStackErrorHandler(u32 id, u32 info){ }

ErrorType FruityHal::GetDeviceConfiguration(DeviceConfiguration & config){ return ErrorType::SUCCESS; }
u32 * FruityHal::GetUserMemoryAddress(){ return 0; }
u32 * FruityHal::GetDeviceMemoryAddress(){ return 0; }
void FruityHal::GetCustomerData(u32 * p_data, u8 len) {};
void FruityHal::WriteCustomerData(u32 * p_data, u8 len) {};
u32 FruityHal::GetBootloaderSettingsAddress(){ return 0; }
u32 FruityHal::GetCodePageSize(){ return 0; }
u32 FruityHal::GetCodeSize(){ return 0; }
u32 FruityHal::GetDeviceId(){ return 0; }
void FruityHal::GetDeviceAddress(u8 * p_address){ }