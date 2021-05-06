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

#include "FruityHalBleGap.h"
#include "Utility.h"
#include <Module.h>
#include <GlobalState.h>
#include <type_traits>

#if IS_ACTIVE(RUUVI_WEATHER_MODULE) && IS_ACTIVE(TIMESLOT)

class StatusReporterModule;

/**
 * The RuuviWeatherModule provides a way to send sensor data to the Ruuvi
 * Station apps on iOS and Android.
 */

/// Registered company id of Ruuvi Innovations Ltd. (MSB).
constexpr u8 RUUVI_COMPANY_ID_MSB = 0x04;

/// Registered company id of Ruuvi Innovations Ltd. (LSB).
constexpr u8 RUUVI_COMPANY_ID_LSB = 0x99;

//Ruuvi Innovations Ltd., Module 1
constexpr VendorModuleId RUUVI_WEATHER_MODULE_ID = GET_VENDOR_MODULE_ID(
  (RUUVI_COMPANY_ID_MSB << 8) | RUUVI_COMPANY_ID_LSB, 1
);

constexpr u8 RUUVI_WEATHER_MODULE_CONFIG_VERSION = 1;

#pragma pack(push)
#pragma pack(1)
struct RuuviWeatherModuleConfiguration : VendorModuleConfiguration {
    /// Interval at which sensor data is read in deci-seconds.
    u16 sensorMeasurementIntervalDs;
    /// Transmission power in dBm used for sensor advertisments.
    i8 advertiserTxPower;
    /// If true, transmit sensor advertisments.
    bool advertiserEnabled : 1;

    u8 padding0 : 7;
};
static_assert(
    sizeof(RuuviWeatherModuleConfiguration) % 4 == 0,
    "size of the module configuration must be a multiple of 4"
);
#pragma pack(pop)

class RuuviWeatherModule : public Module
{
    enum class AdvertiserState : u8
    {
        TX_ADV_37_SEND,
        TX_ADV_38_SEND,
        TX_ADV_39_SEND,
        TX_DONE,
    };

    enum class TriggerActionType : u8
    {
        ADVERTISE_RUUVI_RAW_V2_MESSAGE_V1 = 1,

        CONFIGURE_ADVERTISER = 2,
    };

    enum class ActionResponseType : u8
    {
        ADVERTISER_CONFIGURED = 2,
    };

    #pragma pack(push)
    #pragma pack(1)

    /// Mesh message with the measured fields for a RAWv2 (data format 5)
    /// beacon advertisment.
    /// Message version 1.
    struct AdvertiseRuuviRawV2MessageV1
    {
        /// The BLE GAP address of the sensor node. Used to advertise on
        /// behalf of the sensor node.
        FruityHal::BleGapAddr sensorBleGapAddress;

        /// Incremented each time a measurement is taken. Used to de-duplicate
        /// measurements on the client-side.
        u16 measurementSequenceNumber;

        /// Temperature in 0.005°C steps.
        i16 temperature;

        /// Relative humidity in 0.0025% steps.
        u16 relativeHumidity;

        /// Athmospheric pressure in Pa above 50000 Pa.
        u16 athmosphericPressure;

        /// Battery voltage in mV above 1.6V.
        u16 batteryVoltage : 11;

        u16 padding0 : 5;
    };
    static constexpr size_t ADVERTISE_RUUVI_RAW_V2_MESSAGE_V1_SIZE = 17;
    STATIC_ASSERT_SIZE(AdvertiseRuuviRawV2MessageV1, ADVERTISE_RUUVI_RAW_V2_MESSAGE_V1_SIZE);

    /// Mesh message to configure the advertiser for Ruuvi sensor data.
    struct ConfigureAdvertiserMessage
    {
        /// If true, use the advertiserEnabled value to configure the destination. 
        bool advertiserEnabledUsed : 1;
        bool txPowerUsed : 1;
        /// Padding to 16 bits to allow extension of this message to 16 settings.
        u8 padding0 : 6;
        u8 padding1;

        /// True if the advertiser is enabled.
        bool advertiserEnabled : 1;
        /// Padding to 16 bits to allow extension of this message to 16 flags.
        u8 padding2 : 7;
        u8 padding3;

        /// Transmission power in dBm, the ususal range is between -40 dBm and +8 dBm.
        i8 txPower;
    };
    static constexpr size_t CONFIGURE_ADVERTISER_MESSAGE_SIZE = 5;
    STATIC_ASSERT_SIZE(ConfigureAdvertiserMessage, CONFIGURE_ADVERTISER_MESSAGE_SIZE);

    #pragma pack(pop)

    struct AdvertismentSlot
    {
        bool isSlotAdvertising : 1;
        u8 advertismentCounter : 7;
        std::array<u8, 40> pdu;

        bool HasAdvertiserAddress(const FruityHal::BleGapAddr & address) const;
    };

    /// Wrapper type for the BME280 sensor. This class is private for now,
    /// since it is currently only used here and does not add
    /// functionality. This should eventually be replaced with a generic
    /// driver model.
    class Bme280Sensor
    {
    public:
        enum class State : u8
        {
            DISABLED,
            INITIALIZED,
            SLEEPING,
            MEASURING_CONTINUOUSLY,
            MEASURING_ONCE,
        };

        struct Data
        {
            i32 temperature;
            u32 pressure;
            u32 humidity;
        };

    public:
        Bme280Sensor() = default;

        State GetState() const { return state; }

        ErrorType Initialize();
        ErrorType MeasureContinuously();
        ErrorType MeasureOnce();

        ErrorType ReadData(Data &data);

        /// Ensure state changes due to passed time. Should be called in a
        /// modules TimerEventHandler.
        void TimerEventHandler(u32 passedTimeDs);

    private:
        ErrorType DoMeasure(bool once);

    private:
        State state             = State::DISABLED;
        u32   timeLeftMs        = 0;
        u32   measurementTimeMs = 0;
    };

private:
    constexpr static size_t MAX_SLOTS = 8;
    std::array<AdvertismentSlot, MAX_SLOTS> advertismentSlots = {};
    std::array<u8, 64> transmissionBuffer;

    bool checkForAdvertisableSlots = false;
    bool timeslotInProgress = false;
    AdvertiserState advertiserState = AdvertiserState::TX_DONE;

    Bme280Sensor bme280;
    u32 lastMeasurementAppTimer = 0;

    StatusReporterModule *statusReporterModule = nullptr;

    u16 measurementSequenceNumber = 0;

public:
    DECLARE_CONFIG_AND_PACKED_STRUCT(RuuviWeatherModuleConfiguration);

    RuuviWeatherModule();

    void ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength) override final;

    void ResetToDefaultConfiguration() override final;

    void TimerEventHandler(u16 passedTimeDs) override final;

    void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader) override;

    #ifdef TERMINAL_ENABLED
    TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override final;
    #endif

    /// This function is registered as a callback for Timeslot system events.
    void HandleRadioSystemEvent(FruityHal::SystemEvents systemEvent);

    /// This function is registered as a callback for radio signals (interrupt
    /// handler).
    FruityHal::RadioCallbackAction HandleRadioSignal(FruityHal::RadioCallbackSignalType signalType);

private:
    /// Choose a advertisment slot for an incoming measurement.
    /// This function prioritized slots that have been used for the specified
    /// address before any other slot.
    AdvertismentSlot* ChooseSlotForMeasurementFrom(const FruityHal::BleGapAddr& address);

    /// Choose a advertisment slot for an outgoing transmission (BLE
    /// advertisment packet). This function prioritized slots that have the
    /// smalled advertisment counter.
    AdvertismentSlot* ChooseSlotForTransmission();

    /// Encode an incoming measurement as a protocol data unit (PDU) suitable
    /// for transmission. The sensor data is encoded in the Ruuvi data format
    /// RAWv5.
    void EncodeRawV2(u8* pdu, const AdvertiseRuuviRawV2MessageV1& msg);
};

#endif // IS_ACTIVE(RUUVI_WEATHER_MODULE) && IS_ACTIVE(TIMESLOT)
