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
 * The ScanModule should provide filtering so that all nodes are able to scan
 * for advertising messages and broadcast them through the mesh.
 * This module should also allow to trigger certain tasks after specific
 * packets have been scanned.
 *
 */

#pragma once

#include <Module.h>

#ifdef ACTIVATE_SCANNING_MODULE

#ifdef ACTIVATE_ASSET_MODULE
#include <AssetModule.h>
#endif

#define SCAN_FILTER_NUMBER 1 //Number of filters that can be set
#define NUM_ADDRESSES_TRACKED 50

#define ASSET_PACKET_BUFFER_SIZE 30
#define ASSET_PACKET_RSSI_SEND_THRESHOLD -88

#define SCAN_BUFFERS_SIZE 10 //Max number of packets that are buffered

		enum class GroupingType : u8 {
			GROUP_BY_ADDRESS=1, 
			NO_GROUPING=2
		};

		typedef struct
		{
			u8 active;
			GroupingType grouping;
			fh_ble_gap_addr_t address;
			i8 minRSSI;
			i8 maxRSSI;
			u8 advertisingType;
			SimpleArray<u8, 31> byteMask;
			SimpleArray<u8, 31> mandatory;

		} scanFilterEntry;

#pragma pack(push, 1)
//Module configuration that is saved persistently
struct ScanningModuleConfiguration : ModuleConfiguration{
	u16 groupedReportingIntervalDs;
	u16 assetReportingIntervalDs;
	i8 groupedPacketRssiThreshold;
	//Insert more persistent config values here
};
#pragma pack(pop)

class ScanningModule: public Module
{
	private:

		/*
		 * Filters coud be:
		 * 	- group all filtered packets by address and sum their RSSI and count
		 * 	- scan for specific packets and send them back
		 * 	-
		 *
		 * */

		//Storage for advertising packets
		typedef struct
		{
			u32 serialNumberIndex;
			u8 rssi37;
			u8 rssi38;
			u8 rssi39;
			u8 count;
			u8 speed;
			u8 direction;
			u16 pressure;
		} scannedAssetTrackingPacket;

		SimpleArray<scannedAssetTrackingPacket, ASSET_PACKET_BUFFER_SIZE> assetPackets;

		typedef struct
		{
			fh_ble_gap_addr_t address;
			u32 rssiSum;
			u16 count;
		} scannedPacket;

		SimpleArray<scanFilterEntry, SCAN_FILTER_NUMBER> scanFilters;

		SimpleArray<scannedPacket, SCAN_BUFFERS_SIZE> groupedPackets;



		//For total message counting
		//u32 totalMessages;
		//i32 totalRSSI;

		// Addresses of active devices
		//uint8_t addressPointer;
		//SimpleArray<SimpleArray<u8, BLE_GAP_ADDR_LEN>, NUM_ADDRESSES_TRACKED> addresses;
		//SimpleArray<u32, NUM_ADDRESSES_TRACKED> totalRSSIsPerAddress;
		//SimpleArray<u32, NUM_ADDRESSES_TRACKED> totalMessagesPerAdress;

		u32 totalMessages;
		u32 totalRSSI;


		enum class ScanModuleMessages : u8{
			TOTAL_SCANNED_PACKETS=0,
			ASSET_TRACKING_PACKET=1
		};

		//####### Module specific message structs (these need to be packed)
		#pragma pack(push)
		#pragma pack(1)

		typedef struct
		{
			u16 assetId;
			i8 rssiAvg;
			u8 packetCount;
		} trackedAsset;

		#define SIZEOF_SCAN_MODULE_TRACKED_ASSETS_MESSAGE 12
		typedef struct
		{
				SimpleArray<trackedAsset, 3> trackedAssets;

		} ScanModuleTrackedAssetsMessage;
		STATIC_ASSERT_SIZE(ScanModuleTrackedAssetsMessage, 12);

		//Asset Message V2

		#define SIZEOF_SCAN_MODULE_TRACKED_ASSET_V2 8
		typedef struct
		{
			u32 assetId : 24;
			i32 rssi37 : 8;		//Compilerhack! MSVC refuses to combine this bitfield with the previous one if we declare it as i8!
			i8 rssi38;
			i8 rssi39;
			u8 speed : 4;
			u8 direction : 4;
			u8 pressure;
		} trackedAssetV2;
		STATIC_ASSERT_SIZE(trackedAssetV2, 8);

		typedef struct
		{
			connPacketHeader header;
			trackedAssetV2 trackedAssets[1];

		} ScanModuleTrackedAssetsV2Message;

		//####### End of Module specitic messages
		#pragma pack(pop)


		//Asset packet handling
		void HandleAssetV1Packets(const ble_evt_t& bleEvent);
		void HandleAssetV2Packets(const ble_evt_t& bleEvent);
		bool addTrackedAsset(advPacketAssetServiceData* packet, i8 rssi);
		void ReceiveTrackedAssets(BaseConnectionSendData* sendData, ScanModuleTrackedAssetsV2Message* packet) const;


		//Byte muss gesetzt sein, byte darf nicht gesetzt sein, byte ist egal
		bool setScanFilter(scanFilterEntry* filter);

		void SendReport();

		bool isAssetTrackingData(u8* data, u8 dataLength);
		void resetAssetTrackingTable();
		void SendTrackedAssets();
		bool isAssetTrackingDataFromiOSDeviceInForegroundMode(u8* data, u8 dataLength);

		bool advertiseDataWasSentFromMobileDevice(u8* data, u8 dataLength);
		bool advertiseDataFromAndroidDevice(u8* data, u8 dataLength);
		bool advertiseDataFromiOSDeviceInBackgroundMode(u8* data, u8 dataLength);
		bool advertiseDataFromiOSDeviceInForegroundMode(u8* data, u8 dataLength);
		bool advertiseDataFromBeaconWithDifferentNetworkId(u8 *data, u8 dataLength);

		bool addressAlreadyTracked(uint8_t* address);

		void resetAddressTable();
		void resetTotalRSSIsPerAddress();
		void resetTotalMessagesPerAdress();
		void updateTotalRssiAndTotalMessagesForDevice(i8 rssi, uint8_t* address);
		u32 computeTotalRSSI();




	public:
		DECLARE_CONFIG_AND_PACKED_STRUCT(ScanningModuleConfiguration);

		ScanningModule();

		void ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength) override;

		void ResetToDefaultConfiguration() override;

		void TimerEventHandler(u16 passedTimeDs) override;

		void BleEventHandler(const ble_evt_t& bleEvent) override;

		void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader) override;

		#ifdef TERMINAL_ENABLED
		bool TerminalCommandHandler(char* commandArgs[], u8 commandArgsSize) override;
		#endif
};

#endif
