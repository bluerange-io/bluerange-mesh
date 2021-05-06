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

#include "FruityHal.h"
#include <RuuviWeatherModule.h>
#include <StatusReporterModule.h>
#include <GlobalState.h>
#include <Logger.h>
#include <Utility.h>
#include <IoModule.h>

#if defined(SIM_ENABLED) || IS_ACTIVE(DRIVER_BME280)
#define RUUVI_WEATHER_MODULE_HAS_BME280 1
#if !defined(SIM_ENABLED)
#include <bme280.h>
#endif
#endif

#include <limits>

#if IS_ACTIVE(RUUVI_WEATHER_MODULE) && IS_ACTIVE(TIMESLOT)

namespace
{
    /// The initially requested length of timeslots in µs.
    /// In general, the longer this value is, the harder it is for the
    /// SoftDevice to schedule the timeslot.
    constexpr u32 DEFAULT_INITIAL_TIMESLOT_LENGTH_US = 2000;

    /// Recommended interval in µs between advertisments for increased
    /// discoverability by apple devices. See "Accessory Design Guidelines
    /// for Apple Devices" (Release R13), section 36.5 (p. 143).
    constexpr u32 DEFAULT_NORMAL_TIMESLOT_DISTANCE_US = 1022500;

    /// The interval between sensor measurements (and mesh broadcasts).
    constexpr u16 DEFAULT_SENSOR_MEASUREMENT_INTERVAL_DS = 300;

    /// The maximum number of advertisments sent per slot before the slot is
    /// marked inactive.
    constexpr u8 MAX_ADVERTISMENTS_PER_SLOT = 3;

    void systemEventHandlerTrampoline(
            FruityHal::SystemEvents systemEvent,
            void *userData);

    FruityHal::RadioCallbackAction radioCallbackTrampoline(
            FruityHal::RadioCallbackSignalType signalType,
            void *userData);
}

RuuviWeatherModule::RuuviWeatherModule()
    : Module(RUUVI_WEATHER_MODULE_ID, "ruuvi_weather")
{
    // Setup the callback for radio-related system events and the radio
    // signal callback.
    Timeslot::GetInstance().SetRadioSystemEventHandler(systemEventHandlerTrampoline, this);
    Timeslot::GetInstance().SetRadioSignalCallback(radioCallbackTrampoline, this);

    //Enable the logtag for our vendor module template
    Logger::GetInstance().EnableTag("RUUVI");

    //Save configuration to base class variables
    //sizeof configuration must be a multiple of 4 bytes
    vendorConfigurationPointer = &configuration;
    configurationLength = sizeof(RuuviWeatherModuleConfiguration);

    //Set defaults
    ResetToDefaultConfiguration();
}

void RuuviWeatherModule::ResetToDefaultConfiguration()
{
    //Set default configuration values
    configuration.moduleId = vendorModuleId;
    configuration.moduleActive = true;
    configuration.moduleVersion = RUUVI_WEATHER_MODULE_CONFIG_VERSION;

    configuration.sensorMeasurementIntervalDs = DEFAULT_SENSOR_MEASUREMENT_INTERVAL_DS;

    #if defined(NRF52840)
    configuration.advertiserTxPower = FruityHal::RadioChooseTxPowerHint(+8, true);
    #else
    configuration.advertiserTxPower = FruityHal::RadioChooseTxPowerHint(+4, true);
    #endif
    configuration.advertiserEnabled = true;

    //This line allows us to have different configurations of this module depending on the featureset
    SET_FEATURESET_CONFIGURATION_VENDOR(&configuration, this);
}

void RuuviWeatherModule::ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength)
{
    // VendorModuleConfiguration* newConfig = (VendorModuleConfiguration*)migratableConfig;

    //Version migration can be added here, e.g. if module has version 2 and config is version 1
    // if(newConfig != nullptr && newConfig->moduleVersion == 1){/* ... */};

    // Find the index of the StatusReporterModule (if active) in order to use it for battery measurements.
    statusReporterModule = static_cast<StatusReporterModule *>(
        GS->node.GetModuleById(ModuleId::STATUS_REPORTER_MODULE)
    );

    // Initialize the sensor and measure once such that only initialized
    // values are read back.
    if (configuration.moduleActive)
    {
        if (bme280.Initialize() != ErrorType::SUCCESS)
        {
            logt("RUUVI", "BME280 sensor could not be initialized");
            return;
        }

        if (bme280.MeasureOnce() != ErrorType::SUCCESS)
        {
            logt("RUUVI", "initial measurement from BME280 sensor could not be performed");
            return;
        }
    }

    //Do additional initialization upon loading the config

    //Start the Module...
}

void RuuviWeatherModule::TimerEventHandler(u16 passedTimeDs)
{
    bme280.TimerEventHandler(passedTimeDs);

    [this] {
        const auto currentTime = GS->appTimerDs;

        if (currentTime - lastMeasurementAppTimer <= configuration.sensorMeasurementIntervalDs)
            return;

        // read the sensor data
        Bme280Sensor::Data data;
        {
            const auto err = bme280.ReadData(data);

            // if the read failed due to the sensor being in the wrong state
            // or no valid measurement being present, skip restarting the
            // measurement
            if (err == ErrorType::FORBIDDEN || err == ErrorType::BUSY)
                return;

            // in all other cases restart the measurement
            (void)bme280.MeasureOnce();
            lastMeasurementAppTimer = currentTime;

            // if the measurement failed for any other reason skip sending
            // a message with potentially invalid data
            if (err != ErrorType::SUCCESS)
                return;
        }

        // fill in the message
        AdvertiseRuuviRawV2MessageV1 msg;

        // BME280: Unsigned in 0.01  × [°C]
        // Ruuvi:  Unsigned in 0.005 × [°C]
        msg.temperature = [] (i32 input) -> i16 {
            constexpr i32 maxInput = std::numeric_limits<i16>::max() / 2;
            constexpr i32 minInput = std::numeric_limits<i16>::min() / 2;
            constexpr i16 invalidTemperature = std::numeric_limits<i16>::min();

            if (input < minInput || input > maxInput)
                return invalidTemperature;
            else
                return input * 2;
        }(data.temperature);

        // BME280: Fixed-Point Q22.10 in [%] (relative humidity)
        // Ruuvi:  Unsigned in  0.0025 × [%] (relative humidity)
        msg.relativeHumidity = [] (u32 input) -> u16 {
            constexpr u32 maxResult       = 40000u; // 100% relative humidity
            constexpr u16 invalidHumidity = 0xFFFF;

            const u32   integer  = input >> 10;
            const float fraction = (input & 1023u) * (1.f / 1024.f);
            const u32   result   = integer * 400 + fraction / 0.0025f;

            if (result > maxResult)
                return invalidHumidity;
            else
                return result;
        }(data.humidity);

        // BME280: Fixed-Point Q24.8 in [Pa]
        // Ruuvi:  Unsigned in [Pa] with 0 <=> 50000 Pa
        msg.athmosphericPressure = [] (u32 input) -> u16 {
            constexpr u32 offset          = 50000;
            constexpr u16 invalidPressure = 0xFFFF;

            // remove fractional part
            const u32 shifted = (input >> 8);

            // offset the value accordingly
            if (shifted < offset)
                return invalidPressure;
            else
                return shifted - offset;
        }(data.pressure);

        // StatusReporterModule: Unsigned (8bit) in [dV]
        // Ruuvi:                Unsigned (11bit) in [mV] with 0 <=> 1.6V
        msg.batteryVoltage = [] (u8 input) -> u16 {
            constexpr u8 offset = 16u;
            constexpr u16 invalidVoltage = 2047u;

            // Protect against underflow.
            if (input < offset)
                return invalidVoltage;

            // Protect against overflow.
            if (input - offset > 20)
                return invalidVoltage;

            // Convert from 'dV above 1.6V' to 'mV above 1.6V'.
            return (input - offset) * 100;
        }(statusReporterModule ? statusReporterModule->GetBatteryVoltage() : 0xFF);

        // fill and post-increment the measurement sequence number (wrap-around)
        msg.measurementSequenceNumber = measurementSequenceNumber++;

        // fill in the BLE GAP address for the measurement
        msg.sensorBleGapAddress = FruityHal::GetBleGapAddress();

        // send the broadcast message to all nodes
        SendModuleActionMessage(
            MessageType::MODULE_TRIGGER_ACTION, // message type
            NODE_ID_BROADCAST,                  // destination node id
            (u8)TriggerActionType::ADVERTISE_RUUVI_RAW_V2_MESSAGE_V1,    
            0,                                  // request handle
            (const u8*)&msg,                    // data pointer
            sizeof(msg),                        // data length
            false                               // reliable
        );

        logt("RUUVI", "sensor data sent to node %u", (u32)NODE_ID_BROADCAST);
    }();

    // If advertising is requested, start the timeslot.
    if (configuration.advertiserEnabled && checkForAdvertisableSlots)
    {
        if (!Timeslot::GetInstance().IsSessionOpen())
        {
            Timeslot::GetInstance().OpenSession();
        }

        if (!timeslotInProgress)
        {
            Timeslot::GetInstance().MakeInitialRequest(DEFAULT_INITIAL_TIMESLOT_LENGTH_US);
        }
    }
}

#ifdef TERMINAL_ENABLED
TerminalCommandHandlerReturnType RuuviWeatherModule::TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize)
{
    if (commandArgsSize >= 3 && TERMARGS(0, "action") && TERMARGS(2, moduleName))
    {
        const NodeId destinationNode = Utility::TerminalArgumentToNodeId(commandArgs[1]);

        if (commandArgsSize < 4)
        {
            return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
        }        

        if (TERMARGS(3, "advertiser"))
        {
            ConfigureAdvertiserMessage msg{};

            bool sendMsg = false;

            if (commandArgsSize < 5)
            {
                return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
            }

            if (TERMARGS(4, "enable"))
            {
                sendMsg = true;
                msg.advertiserEnabledUsed = true;
                msg.advertiserEnabled = true;
            }
            else if (TERMARGS(4, "disable"))
            {
                sendMsg = true;
                msg.advertiserEnabledUsed = true;
                msg.advertiserEnabled = false;
            }
            else if (TERMARGS(4, "txPower"))
            {
                if (commandArgsSize < 6)
                {
                    return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
                }

                bool parseError = false;
                const auto txPower = Utility::StringToI8(commandArgs[5], &parseError);

                if (parseError)
                {
                    logt("RUUVI", "error parsing transmission power '%s'", commandArgs[5]);
                    return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                }

                msg.txPowerUsed = true;
                msg.txPower = txPower;

                sendMsg = true;
            }

            if (sendMsg)
            {
                // Send the configuration message to the destination. 
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION, // message type
                    destinationNode,                    // destination node id
                    (u8)TriggerActionType::CONFIGURE_ADVERTISER,
                    0,                                  // request handle
                    (const u8*)&msg,                    // data pointer
                    sizeof(msg),                        // data length
                    false                               // reliable
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }
        }

        return TerminalCommandHandlerReturnType::UNKNOWN;
    }

    //Must be called to allow the module to get and set the config
    return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif

void RuuviWeatherModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader)
{
    //Must call superclass for handling
    Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

    if (packetHeader->messageType == MessageType::MODULE_TRIGGER_ACTION)
    {
        auto *packet = (const ConnPacketModuleVendor *)packetHeader;

        if (packet->moduleId == vendorModuleId)
        {
            const auto actionType = (TriggerActionType)packet->actionType;

            if (actionType == TriggerActionType::ADVERTISE_RUUVI_RAW_V2_MESSAGE_V1 && configuration.advertiserEnabled)
            {
                const auto *msg = (const AdvertiseRuuviRawV2MessageV1 *)packet->data;

                auto *slot = ChooseSlotForMeasurementFrom(msg->sensorBleGapAddress);
                if (slot)
                {
                    logt("RUUVI", "sensor data received from node %u", (u32)packet->header.sender);

                    // Encode the measurement in RAWv2 data format.
                    EncodeRawV2(slot->pdu.data(), *msg);
                    // Start advertising the slot.
                    slot->isSlotAdvertising = true;
                    slot->advertismentCounter = 0;
                    // Start to advertise if we weren't.
                    checkForAdvertisableSlots = true;
                }
            }

            if (actionType == TriggerActionType::CONFIGURE_ADVERTISER)
            {
                const auto *msg = (const ConfigureAdvertiserMessage *)packet->data;

                if (msg->advertiserEnabledUsed)
                {
                    configuration.advertiserEnabled = msg->advertiserEnabled;
                    logt("RUUVI", "advertiser %s by node %u", msg->advertiserEnabled ? "enabled" : "disabled", (u32)packet->header.sender);
                }

                if (msg->txPowerUsed)
                {
                    configuration.advertiserTxPower = FruityHal::RadioChooseTxPowerHint(msg->txPower, true);
                    logt("RUUVI", "advertiser transmission power set to %d by node %u", (i32)configuration.advertiserTxPower, (u32)packet->header.sender);
                }

                SendModuleActionMessage(
                    MessageType::MODULE_ACTION_RESPONSE,           // message type
                    packet->header.sender,                         // destination node id
                    (u8)ActionResponseType::ADVERTISER_CONFIGURED, // action type
                    packet->requestHandle,                         // request handle
                    nullptr,                                       // data pointer
                    0,                                             // data length
                    false                                          // reliable
                );
            }
        }
    }
    else if (packetHeader->messageType == MessageType::MODULE_ACTION_RESPONSE)
    {
        auto *packet = (const ConnPacketModuleVendor *)packetHeader;

        if (packet->moduleId == vendorModuleId)
        {
            const auto actionType = (ActionResponseType)packet->actionType;

            if (actionType == ActionResponseType::ADVERTISER_CONFIGURED)
            {
                logt("RUUVI", "advertiser on node %u configured", (u32)packetHeader->sender);
            }
        }
    }
}

void RuuviWeatherModule::HandleRadioSystemEvent(FruityHal::SystemEvents systemEvent)
{
    switch (systemEvent)
    {
        case FruityHal::SystemEvents::RADIO_SESSION_IDLE:
            timeslotInProgress = false;
            Timeslot::GetInstance().CloseSession();
            break;

        case FruityHal::SystemEvents::RADIO_BLOCKED:
            timeslotInProgress = false;
            // The next timeslot will be started from the timer or the receive
            // handler if neccessary.
            break;

        case FruityHal::SystemEvents::RADIO_CANCELED:
            timeslotInProgress = false;
            // The next timeslot will be started from the timer or the receive
            // handler if neccessary.
            break;

        case FruityHal::SystemEvents::RADIO_SIGNAL_CALLBACK_INVALID_RETURN:
        case FruityHal::SystemEvents::RADIO_SESSION_CLOSED:
        default:
            break;
    }
}

FruityHal::RadioCallbackAction RuuviWeatherModule::HandleRadioSignal(FruityHal::RadioCallbackSignalType signalType)
{
    // WARNING: This function is called in an interrupt handler of the
    //          highest priority! This means in particular that no
    //          SoftDevice functions must be called, otherwise the
    //          firmware will hang.

    switch (signalType)
    {
        case FruityHal::RadioCallbackSignalType::START:
            timeslotInProgress = true;

#if defined(SIM_ENABLED)
            logt("RUUVI", "handling radio signal START");
#endif

            // Update the transmission buffer with an advertising packet.
            {
                // Select the transmission slot.
                auto * slot = ChooseSlotForTransmission();
                if (!slot)
                {
                    // If there is no active advertisment slot, deactivate
                    // advertising and end the timeslot.
                    checkForAdvertisableSlots = false;
                    return FruityHal::RadioCallbackAction::END;
                }

                // Copy the PDU to the transmission buffer.
                CheckedMemcpy(transmissionBuffer.data(), slot->pdu.data(), slot->pdu.size());

                // Increment the advertisment counter of the slot.
                ++slot->advertismentCounter;

                // If the maximum number of advertisments was reached,
                // deactivate advertisment for this slot.
                if (slot->advertismentCounter >= MAX_ADVERTISMENTS_PER_SLOT)
                {
                    slot->isSlotAdvertising = false;
                }
            }
            
            #ifdef NRF52840
            GS->ledRed.On();
            #endif

            // Configure the radio and make sure the DISBALED event is unmasked.
            FruityHal::RadioHandleBleAdvTxStart(transmissionBuffer.data());
            FruityHal::RadioChooseTxPowerHint(configuration.advertiserTxPower);
            FruityHal::RadioUnmaskEvent(FruityHal::RadioEvent::DISABLED);
            FruityHal::RadioClearEvent(FruityHal::RadioEvent::DISABLED);
            FruityHal::RadioTriggerTask(FruityHal::RadioTask::DISABLE);

            // The advertiser starts by transmitting on channel 37.
            advertiserState = AdvertiserState::TX_ADV_37_SEND;
            break;

        case FruityHal::RadioCallbackSignalType::RADIO:
#if defined(SIM_ENABLED)
            logt("RUUVI", "handling radio signal RADIO");
#endif

            if (FruityHal::RadioCheckAndClearEvent(FruityHal::RadioEvent::DISABLED))
            {
                // The RADIO signal is triggered on peripheral events. In this
                // example we configured the peripheral to only generate the
                // DISABLED signal and to automatically DISABLE after the
                // transmission finished (see the RADIO peripheral
                // documentation under shortcuts).

#if defined(SIM_ENABLED)
            logt("RUUVI", "handling radio signal RADIO due to radio event DISABLED");
#endif

                switch (advertiserState)
                {
                    case AdvertiserState::TX_ADV_37_SEND:
                        FruityHal::RadioChooseBleAdvertisingChannel(37);
                        FruityHal::RadioTriggerTask(FruityHal::RadioTask::TXEN);
                        FruityHal::RadioUnmaskEvent(FruityHal::RadioEvent::DISABLED);
                        advertiserState = AdvertiserState::TX_ADV_38_SEND;
                        break;
                    
                    case AdvertiserState::TX_ADV_38_SEND:
                        FruityHal::RadioChooseBleAdvertisingChannel(38);
                        FruityHal::RadioTriggerTask(FruityHal::RadioTask::TXEN);
                        FruityHal::RadioUnmaskEvent(FruityHal::RadioEvent::DISABLED);
                        advertiserState = AdvertiserState::TX_ADV_39_SEND;
                        break;

                    case AdvertiserState::TX_ADV_39_SEND:
                        FruityHal::RadioChooseBleAdvertisingChannel(39);
                        FruityHal::RadioTriggerTask(FruityHal::RadioTask::TXEN);
                        FruityHal::RadioUnmaskEvent(FruityHal::RadioEvent::DISABLED);
                        advertiserState = AdvertiserState::TX_DONE;
                        break;

                    case AdvertiserState::TX_DONE:
                        #ifdef NRF52840
                        GS->ledRed.Off();
                        #endif

                        // Schedule the next timeslot automatically if there
                        // are still more data points to advertise or end it.
                        if (checkForAdvertisableSlots)
                        {
                            FruityHal::TimeslotConfigureNextEventNormal(
                                DEFAULT_INITIAL_TIMESLOT_LENGTH_US,
                                DEFAULT_NORMAL_TIMESLOT_DISTANCE_US
                            );
                            return FruityHal::RadioCallbackAction::REQUEST_AND_END;
                        }
                        else
                        {
                            return FruityHal::RadioCallbackAction::END;
                        }
                }
            }

            break;

        case FruityHal::RadioCallbackSignalType::TIMER0:
        case FruityHal::RadioCallbackSignalType::EXTEND_SUCCEEDED:
        case FruityHal::RadioCallbackSignalType::EXTEND_FAILED:
        default:
            break;
    }

    return FruityHal::RadioCallbackAction::NONE;
}

RuuviWeatherModule::AdvertismentSlot * RuuviWeatherModule::ChooseSlotForMeasurementFrom(const FruityHal::BleGapAddr &address)
{
    AdvertismentSlot * byAddress = nullptr;
    AdvertismentSlot * byCounter = nullptr;
    AdvertismentSlot * byAdvertisingState = nullptr;

    for (auto &slot : advertismentSlots)
    {
        // Cache any slot with matching address.
        if (slot.HasAdvertiserAddress(address))
        {
            byAddress = &slot;
        }

        // Cache any slot that is not advertising.
        if (!slot.isSlotAdvertising)
        {
            byAdvertisingState = &slot;
        }

        // Cache any slot that has been advertised the most.
        if (!byCounter || slot.advertismentCounter > byCounter->advertismentCounter)
        {
            byCounter = &slot;
        }
    }

    auto *selected = [byAddress, byCounter, byAdvertisingState] () -> AdvertismentSlot * {
        // Always reuse the slot with matching address if set.
        if (byAddress)
        {
            return byAddress;
        }

        // Otherwise use an unused slot.
        if (byAdvertisingState)
        {
            return byAdvertisingState;
        }

        // Otherwise use the most advertised slot.
        return byCounter;
    }();

    return selected;
}

RuuviWeatherModule::AdvertismentSlot * RuuviWeatherModule::ChooseSlotForTransmission()
{
    AdvertismentSlot * byCounter = nullptr;

    for (auto &slot : advertismentSlots)
    {
        // Skip slots that are not advertising.
        if (!slot.isSlotAdvertising)
        {
            continue;
        }

        // Cache any slot that has been advertised the least.
        if (!byCounter || slot.advertismentCounter < byCounter->advertismentCounter)
        {
            byCounter = &slot;
        }
    }

    return byCounter;
}

void RuuviWeatherModule::EncodeRawV2(u8 *pdu, const AdvertiseRuuviRawV2MessageV1 &msg)
{
    u8 packetLengthFieldIndex, packetLengthStartIndex;
    u8 offset = 0;

    enum class PduType : u8
    {
        // ADV_IND         = 0x0,
        // ADV_DIRECT_IND  = 0x1,
        ADV_NONCONN_IND = 0x2,
        // SCAN_REQ        = 0x3,
        // SCAN_RSP        = 0x4,
        // CONNECT_REQ     = 0x5,
        // ADV_SCAN_IND    = 0x6,
    };

    const auto &address = msg.sensorBleGapAddress;
    const bool publicAddress = address.addr_type == FruityHal::BleGapAddrType::PUBLIC;

    const u8 pduType = (
            (u8)PduType::ADV_NONCONN_IND    // PDU Type
        |   (publicAddress ? 0 : (1 << 6))  // TxAdd
        |   (0)                             // RxAdd
    );

    // Configure the PDU Header.
    // The exact mapping from this structure to the on-air packet layout is
    // documented in e.g. "nRF52832 Product Specification / RADIO / Packet
    // configuration" or the corresponding secion of the nRF52840 series SOCs.
    pdu[offset++] = pduType;   //     S0 (1 byte) - PDU type
    packetLengthFieldIndex = offset;
    pdu[offset++] = 0;         // LENGTH (6 bits) - Packet length
    pdu[offset++] = 0x00;      //     S1 (2 bits) - Reserved - only present in RAM
    packetLengthStartIndex = offset;

    // Configure the PDU payload (device address)
    auto * const pduAddress = &pdu[offset];
    for (int index = 0; index < 6; ++index)
    {
        pdu[offset++] = address.addr[index];
    }
    if (!publicAddress)
    {
        // HACK: ensure the address is a valid random static address - FIX THIS NOW
        pduAddress[5] |= 0xC0;
    }

    // Configure PDU GAP Advertising Flags and Custom Manufacturer Header
    pdu[offset++] = 0x02; // Length
    pdu[offset++] = 0x01; // ...
    pdu[offset++] = FH_BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE | FH_BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;

    u8 cmdLengthFieldIndex, cmdLengthStartIndex;

    // Configure PDU Custom Manufacturer Data (Header)
    cmdLengthFieldIndex = offset;
    pdu[offset++] = 0x1B; // Length
    cmdLengthStartIndex = offset;
    pdu[offset++] = 0xFF; // Custom Manufacturer Data
    pdu[offset++] = RUUVI_COMPANY_ID_LSB; // Company ID
    pdu[offset++] = RUUVI_COMPANY_ID_MSB; // Company ID

    // Increments the pdu offset and returns a pointer into the buffer with
    // the previous offset.
    const auto pduNext = [&offset, pdu] (u32 increment) {
        const auto pointer = &pdu[offset];
        offset += increment;
        return pointer;
    };

    const auto encodedTxPower = [this] () -> u8 {
        constexpr u8 invalidTxPower = 31u;

        // Prevent underflow.
        if (configuration.advertiserTxPower < -40)
            return invalidTxPower;

        // Prevent overflow.
        if (configuration.advertiserTxPower > 30)
            return invalidTxPower;

        return (configuration.advertiserTxPower + 40) / 2;
    }();

    using Utility::WriteBE;
    using Utility::WriteLE;

    // Configure the PDU payload (data)
    pdu[offset++] = 0x05; // Data Format 5
    WriteBE<u16>(pduNext(2), msg.temperature);
    WriteBE<u16>(pduNext(2), msg.relativeHumidity);
    WriteBE<u16>(pduNext(2), msg.athmosphericPressure);
    WriteBE<u16>(pduNext(2), 0x8000); // acceleration x (n.a.)
    WriteBE<u16>(pduNext(2), 0x8000); // acceleration y (n.a.)
    WriteBE<u16>(pduNext(2), 0x8000); // acceleration z (n.a.)
    WriteBE<u16>(pduNext(2),
            (msg.batteryVoltage & 2047u) << 5u
        |   (encodedTxPower & 31u)
    );
    WriteBE<u8>(pduNext(1), 0xFF); // movement counter (n.a.)
    WriteBE<u16>(pduNext(2), msg.measurementSequenceNumber);

    // MAC address
    {
        auto * const msgAddress = &pdu[offset];
        for (int index = 5; index >= 0; --index)
        {
            pdu[offset++] = address.addr[index];
        }
        if (!publicAddress)
        {
            // HACK: ensure the address is a valid random static address
            msgAddress[5] |= 0xC0;
        }
    }

    // Configure the length fields.
    pdu[packetLengthFieldIndex] = offset - packetLengthStartIndex;
    pdu[cmdLengthFieldIndex]    = offset - cmdLengthStartIndex;
}

bool RuuviWeatherModule::AdvertismentSlot::HasAdvertiserAddress(const FruityHal::BleGapAddr & address) const
{
    // TxAdd == 0: Public, TxAdd == 1: Random
    const bool publicAddress = !((pdu[0] >> 6u) & 0x1);
    if (publicAddress)
    {
        if (address.addr_type != FruityHal::BleGapAddrType::PUBLIC)
        {
            return false;
        }
    }
    else
    {
        if (address.addr_type == FruityHal::BleGapAddrType::PUBLIC)
        {
            return false;
        }
    }

    for (unsigned index = 0; index < 6; ++index)
    {
        if (pdu[3 + index] != address.addr[index])
        {
            return false;
        }
    }

    return true;
}

ErrorType RuuviWeatherModule::Bme280Sensor::Initialize()
{
    if (state != State::DISABLED)
    {
        logt("RUUVI", "BME280 must be DISABLED to Enable()");
        return ErrorType::FORBIDDEN;
    }

    // make sure custom pinsets are available
    const auto getCustomPinset = Boardconf::GetInstance().getCustomPinset;
    if (!getCustomPinset)
    {
        logt(
            "RUUVI",
            "error: no custom pinsets available (potential "
            "mismatch between board id / board type and "
            "featureset)"
        );
        return ErrorType::NOT_SUPPORTED;
    }

    // load the pinset for the BME280 sensor
    Bme280Pins pins;
    pins.pinsetIdentifier = PinsetIdentifier::BME280;
    getCustomPinset(&pins);

    if (pins.ssPin == -1)
    {
        logt(
            "RUUVI",
            "error: BME280 slave-select pin not set"
        );
        return ErrorType::NOT_SUPPORTED;
    }

    // if required, enable the sensor
    if (pins.sensorEnablePin != -1) {
        FruityHal::GpioConfigureOutput(pins.sensorEnablePin);

        if (pins.sensorEnablePinActiveHigh)
            FruityHal::GpioPinClear(pins.sensorEnablePin);
        else
            FruityHal::GpioPinSet(pins.sensorEnablePin);

        // allow the sensor to turn on
        FruityHal::DelayUs(200);
    }

    if (!FruityHal::SpiIsInitialized())
    {
        FruityHal::SpiInit(
            pins.sckPin,
            pins.misoPin,
            pins.mosiPin
        );
    }

    // TODO: actually compute the measurement time based on the settings below
    //       (see section 9.B Appendix in BME280 Data sheet)
    measurementTimeMs = 200;

#ifdef RUUVI_WEATHER_MODULE_HAS_BME280
    u32 err = 0;

    // initialize the sensor
    if ((err = bme280_init(pins.ssPin)))
    {
        logt("RUUVI", "bme_init(...) error %x", err);
        return ErrorType::INTERNAL;
    }

    // give the sensor time to initialize
    FruityHal::DelayMs(10);

    // set the sensor into sleeping mode
    bme280_set_mode_assert(BME280_MODE_SLEEP);

    err = BME280_RET_OK;

    // TODO: review and change configuration
    // TODO: check if this should be refactored into either the Measure...()
    //       member functions or separate functions

    // configure oversampling for each measurement
    err |= bme280_set_oversampling_press(BME280_OVERSAMPLING_2);
    err |= bme280_set_oversampling_temp(BME280_OVERSAMPLING_2);
    err |= bme280_set_oversampling_hum(BME280_OVERSAMPLING_2);

    // configure the iir (low-pass) filter to reduce noise in pressure and
    // temperature values
    err |= bme280_set_iir(BME280_IIR_8);

    // configure the sampling interval for normal mode
    err |= bme280_set_interval(BME280_STANDBY_1000_MS);

    if (err != BME280_RET_OK)
    {
        logt("RUUVI", "bme_set_...(...) combined error %x", err);
        return ErrorType::INTERNAL;
    }

    state = State::INITIALIZED;

    return ErrorType::SUCCESS;
#else
    logt("RUUVI", "featureset does not support the BME280 sensor");
    return ErrorType::NOT_SUPPORTED;
#endif
}

ErrorType RuuviWeatherModule::Bme280Sensor::MeasureContinuously()
{
    if (!(state == State::INITIALIZED || state == State::SLEEPING))
    {
        logt("RUUVI", "BME280 must be SLEEPING to MeasureContinuously()");
        return ErrorType::FORBIDDEN;
    }

    return DoMeasure(false);
}

ErrorType RuuviWeatherModule::Bme280Sensor::MeasureOnce()
{
    if (!(state == State::INITIALIZED || state == State::SLEEPING || state == State::MEASURING_CONTINUOUSLY))
    {
        logt("RUUVI", "BME280 must be SLEEPING or MEASURING_CONTINUOUSLY to MeasureOnce()");
        return ErrorType::FORBIDDEN;
    }

    logt("RUUVI", "BME280 measure once");

    return DoMeasure(true);
}

ErrorType RuuviWeatherModule::Bme280Sensor::ReadData(Data &data)
{
    if (!(state == State::SLEEPING || state == State::MEASURING_CONTINUOUSLY))
        return ErrorType::FORBIDDEN;

    if (timeLeftMs > 0)
        return ErrorType::BUSY;

#ifdef RUUVI_WEATHER_MODULE_HAS_BME280
    const auto ret = bme280_read_measurements();
    if (ret != BME280_RET_OK)
    {
        logt("RUUVI", "bme280_read_mesaurements error %x", (u32)ret);
        return ErrorType::INTERNAL;
    }

    data.humidity = bme280_get_humidity();
    data.pressure = bme280_get_pressure();
    data.temperature = bme280_get_temperature();

    return ErrorType::SUCCESS;
#else
    return ErrorType::NOT_SUPPORTED;
#endif
}

void RuuviWeatherModule::Bme280Sensor::TimerEventHandler(u32 passedTimeDs)
{
    if (!(state == State::MEASURING_CONTINUOUSLY || state == State::MEASURING_ONCE))
        return;

    const auto passedTimeMs = passedTimeDs * 100;

    if (timeLeftMs <= passedTimeMs)
    {
        logt("RUUVI", "BME280 data available");
        timeLeftMs = 0;

        if (state == State::MEASURING_ONCE)
            state = State::SLEEPING;
    }
    else
    {
        timeLeftMs -= passedTimeMs;
    }
}

ErrorType RuuviWeatherModule::Bme280Sensor::DoMeasure(bool once)
{
#ifdef RUUVI_WEATHER_MODULE_HAS_BME280
    bme280_set_mode_assert(once ? BME280_MODE_FORCED : BME280_MODE_NORMAL);

    u8 data1 = bme280_read_reg(BME280REG_CTRL_MEAS);
    logt("RUUVI", "BME280_CTRL_MEAS: %x", (u32)data1);

    timeLeftMs = measurementTimeMs;
    state = once ? State::MEASURING_ONCE : State::MEASURING_CONTINUOUSLY;

    return ErrorType::SUCCESS;
#else
    return ErrorType::NOT_SUPPORTED;
#endif
}

namespace
{
    void systemEventHandlerTrampoline(FruityHal::SystemEvents systemEvent, void *userData)
    {
        auto *module = static_cast<RuuviWeatherModule *>(userData);
        return module->HandleRadioSystemEvent(systemEvent);
    }

    FruityHal::RadioCallbackAction radioCallbackTrampoline(FruityHal::RadioCallbackSignalType signalType, void *userData)
    {
        auto *module = static_cast<RuuviWeatherModule *>(userData);
        return module->HandleRadioSignal(signalType);
    }
}

#endif // IS_ACTIVE(RUUVI_WEATHER_MODULE) && IS_ACTIVE(TIMESLOT)
