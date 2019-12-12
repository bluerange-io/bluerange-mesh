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
 * The status reporter module is responsible for measuring battery, connections,
 * etc... and report them back to a sink
 */

#pragma once

#include <Module.h>
#include <Logger.h>

#include <Terminal.h>

#if defined(NRF51)
extern "C"{
#include <nrf_drv_adc.h>
}
#endif
#if defined(NRF52)
#include <nrf_drv_saadc.h>
#endif

#define BATTERY_SAMPLES_IN_BUFFER 					1//Number of SAADC samples in RAM before returning a SAADC event. For low power SAADC set this constant to 1. Otherwise the EasyDMA will be enabled for an extended time which consumes high current.

#if defined(NRF51) || defined(SIM_ENABLED)
constexpr int REF_VOLTAGE_IN_MILLIVOLTS            = 1200;
#define RESULT_IN_DECI_VOLTS(ADC_VALUE)    			((((ADC_VALUE) * REF_VOLTAGE_IN_MILLIVOLTS) / 1024))
#endif


#if defined(NRF52)
constexpr double REF_VOLTAGE_INTERNAL_IN_MILLI_VOLTS			= 600; // Maximum Internal Reference Voltage
constexpr double VOLTAGE_DIVIDER_INTERNAL_IN_MILLI_VOLTS		= 166; //Internal voltage divider
constexpr double ADC_RESOLUTION_8BIT							= 256;

#define RESULT_IN_DECI_VOLTS(ADC_VALUE)     		((ADC_VALUE*REF_VOLTAGE_INTERNAL_IN_MILLI_VOLTS)/(VOLTAGE_DIVIDER_INTERNAL_IN_MILLI_VOLTS*ADC_RESOLUTION_8BIT))*10

constexpr double REF_VOLTAGE_EXTERNAL_IN_MILLI_VOLTS		= 825; // Maximum Internal Reference Voltage
constexpr double VOLTAGE_GAIN_IN_MILLI_VOLTS				= 200; //Internal voltage divider
constexpr double ADC_RESOLUTION_10BIT						= 1023;

#define RESULT_IN_DECI_VOLTS_VOLTAGE_DIV(ADC_VALUE, VOLTAGE_DIV)   ( (ADC_VALUE)* (REF_VOLTAGE_EXTERNAL_IN_MILLI_VOLTS/VOLTAGE_GAIN_IN_MILLI_VOLTS) * (1/ADC_RESOLUTION_10BIT) * (VOLTAGE_DIV))
#endif

enum class RSSISamplingModes : u8 {
	NONE = 0,
	LOW = 1,
	MEDIUM = 2,
	HIGH = 3
};

#pragma pack(push, 1)
//Module configuration that is saved persistently
struct StatusReporterModuleConfiguration: ModuleConfiguration
{
		u16 connectionReportingIntervalDs;
		u16 statusReportingIntervalDs;
		u16 nearbyReportingIntervalDs;
		u16 deviceInfoReportingIntervalDs;
		LiveReportTypes liveReportingState;
		//Insert more persistent config values here
};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
typedef struct
{
	NodeId partner1;
	i8 rssi1;
	NodeId partner2;
	i8 rssi2;
	NodeId partner3;
	i8 rssi3;
	NodeId partner4;
	i8 rssi4;

} StatusReporterModuleConnectionsMessage;
STATIC_ASSERT_SIZE(StatusReporterModuleConnectionsMessage, 12);
#pragma pack(pop)

class StatusReporterModule: public Module
{
public:
		
		static constexpr RSSISamplingModes connectionRSSISamplingMode = RSSISamplingModes::HIGH;
		static constexpr u32 batteryMeasurementIntervalDs = SEC_TO_DS(6*60*60);

		enum class StatusModuleTriggerActionMessages : u8
		{
			SET_LED = 0,
			GET_STATUS = 1,
			//GET_DEVICE_INFO = 2, removed as of 17.05.2019
			GET_ALL_CONNECTIONS = 3,
			GET_NEARBY_NODES = 4,
			SET_INITIALIZED = 5,
			GET_ERRORS = 6,
			GET_REBOOT_REASON = 8,
			SET_KEEP_ALIVE = 9,
			GET_DEVICE_INFO_V2 = 10,
			SET_LIVEREPORTING = 11,
		};

		enum class StatusModuleActionResponseMessages : u8
		{
			SET_LED_RESULT = 0,
			STATUS = 1,
			//DEVICE_INFO = 2, removed as of 17.05.2019
			ALL_CONNECTIONS = 3,
			NEARBY_NODES = 4,
			SET_INITIALIZED_RESULT = 5,
			ERROR_LOG_ENTRY = 6,
			//DISCONNECT_REASON = 7, removed as of 21.05.2019
			REBOOT_REASON = 8,
			DEVICE_INFO_V2 = 10,
		};

		enum class StatusModuleGeneralMessages : u8
		{
			LIVE_REPORT = 1
		};

private:

		//####### Module specific message structs (these need to be packed)
		#pragma pack(push)
		#pragma pack(1)

		typedef struct
			{
				NodeId nodeId;
				i32 rssiSum;
				u16 packetCount;
			} nodeMeasurement;

			//This message delivers non- (or not often)changing information
			static constexpr int SIZEOF_STATUS_REPORTER_MODULE_DEVICE_INFO_V2_MESSAGE = (37);
			typedef struct
			{
				u16 manufacturerId;
				u32 serialNumberIndex;
				u8 chipId[8];
				FruityHal::BleGapAddr accessAddress;
				NetworkId networkId;
				u32 nodeVersion;
				i8 dBmRX;
				i8 dBmTX;
				DeviceType deviceType;
				i8 calibratedTX;
				NodeId chipGroupId;
				NodeId featuresetGroupId;
				u16 bootloaderVersion;

			} StatusReporterModuleDeviceInfoV2Message;
			STATIC_ASSERT_SIZE(StatusReporterModuleDeviceInfoV2Message, 37);

			//This message delivers often changing information and info about the incoming connection
			static constexpr int SIZEOF_STATUS_REPORTER_MODULE_STATUS_MESSAGE = 9;
			typedef struct
			{
				ClusterSize clusterSize;
				NodeId inConnectionPartner;
				i8 inConnectionRSSI;
				u8 freeIn : 2;
				u8 freeOut : 6;
				u8 batteryInfo;
				u8 connectionLossCounter; //Connection losses since reboot
				u8 initializedByGateway : 1; //Set to 0 if node has been resetted and does not know its configuration

			} StatusReporterModuleStatusMessage;
			STATIC_ASSERT_SIZE(StatusReporterModuleStatusMessage, 9);

			//Used for sending error logs through the mesh
			static constexpr int SIZEOF_STATUS_REPORTER_MODULE_ERROR_LOG_ENTRY_MESSAGE = 12;
			typedef struct
			{
				u32 errorType : 8; //Workaround necessary for packing, should be of type ErrorTypes
				u32 timestamp : 24; //This should be u32, but not enough space in that message, so it will wrap after 20 days
				u32 extraInfo;
				u32 errorCode;

			} StatusReporterModuleErrorLogEntryMessage;
			STATIC_ASSERT_SIZE(StatusReporterModuleErrorLogEntryMessage, 12);

			static constexpr int SIZEOF_STATUS_REPORTER_MODULE_LIVE_REPORT_MESSAGE = 9;
			typedef struct
			{
				u8 reportType;
				u32 extra;
				u32 extra2;
			} StatusReporterModuleLiveReportMessage;
			STATIC_ASSERT_SIZE(StatusReporterModuleLiveReportMessage, 9);

		#pragma pack(pop)

		//####### Module messages end

		static constexpr int NUM_NODE_MEASUREMENTS = 20;
		nodeMeasurement nodeMeasurements[NUM_NODE_MEASUREMENTS];

		u8 batteryVoltageDv; //in decivolts
		bool isADCInitialized;
		u8 number_of_adc_channels;
#if defined(NRF51)
		nrf_drv_adc_channel_t adc_channel_config;
		nrf_adc_value_t m_buffer[BATTERY_SAMPLES_IN_BUFFER];
#endif
#if defined(NRF52)
		nrf_drv_saadc_config_t saadc_config;
		nrf_saadc_channel_config_t channel_config;
		nrf_saadc_value_t m_buffer[BATTERY_SAMPLES_IN_BUFFER];
#endif

		void SendStatus(NodeId toNode, u8 requestHandle, MessageType messageType) const;
		void SendDeviceInfoV2(NodeId toNode, u8 requestHandle, MessageType messageType) const;
		void SendNearbyNodes(NodeId toNode, u8 requestHandle, MessageType messageType);
		void SendAllConnections(NodeId toNode, u8 requestHandle, MessageType messageType) const;
		void SendErrors(NodeId toNode, u8 requestHandle) const;
		void SendRebootReason(NodeId toNode, u8 requestHandle) const;

		void StartConnectionRSSIMeasurement(MeshConnection& connection) const;
		void StopConnectionRSSIMeasurement(const MeshConnection& connection) const;

		void initBatteryVoltageADC();
		void BatteryVoltageADC();

		void convertADCtoVoltage(i16 * buffer, u16 size);

	public:

		static constexpr int SIZEOF_STATUS_REPORTER_MODULE_CONNECTIONS_MESSAGE = 12;


		DECLARE_CONFIG_AND_PACKED_STRUCT(StatusReporterModuleConfiguration);

		StatusReporterModule();

		void ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength) override;

		void ResetToDefaultConfiguration() override;

		void TimerEventHandler(u16 passedTimeDs) override;

		#ifdef TERMINAL_ENABLED
		TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override;
		#endif

		void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader) override;

		void GapAdvertisementReportEventHandler(const FruityHal::GapAdvertisementReportEvent& advertisementReportEvent) override;

		void MeshConnectionChangedHandler(MeshConnection& connection) override;

		void SendLiveReport(LiveReportTypes type, u16 requestHandle, u32 extra, u32 extra2) const;

		u8 GetBatteryVoltage() const;

		u16 ExternalVoltageDividerDv(u32 Resistor1, u32 Resistor2);
};

