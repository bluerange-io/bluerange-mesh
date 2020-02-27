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
 * The ScanModule should provide filtering so that all nodes are able to scan
 * for advertising messages and broadcast them through the mesh.
 * This module should also allow to trigger certain tasks after specific
 * packets have been scanned.
 *
 */

#pragma once

#include <Module.h>

#if IS_ACTIVE(ASSET_MODULE)
#ifndef GITHUB_RELEASE
#include <AssetModule.h>
#endif //GITHUB_RELEASE
#endif
#include <ScanController.h>

constexpr int SCAN_FILTER_NUMBER = 2;//Number of filters that can be set
constexpr int NUM_ADDRESSES_TRACKED = 50;

constexpr int ASSET_PACKET_BUFFER_SIZE = 30;
constexpr int ASSET_INS_PACKET_BUFFER_SIZE = 30;
constexpr int ASSET_PACKET_RSSI_SEND_THRESHOLD = -88;

constexpr int SCAN_BUFFERS_SIZE = 10; //Max number of packets that are buffered

enum class GroupingType : u8 {
	GROUP_BY_ADDRESS =1, 
	NO_GROUPING      =2,
};

typedef struct
{
	u8 active;
	GroupingType grouping;
	FruityHal::BleGapAddr address;
	i8 minRSSI;
	i8 maxRSSI;
	u8 advertisingType;
	SimpleArray<u8, 31> byteMask;
	SimpleArray<u8, 31> mandatory;

} scanFilterEntry;

#pragma pack(push, 1)
//Module configuration that is saved persistently
struct ScanningModuleConfiguration : ModuleConfiguration{
	//Insert more persistent config values here
};
#pragma pack(pop)

class ScanningModule : public Module
{
private:
	static constexpr u16 groupedReportingIntervalDs = 0;
	/*
	 * Filters coud be:
	 * 	- group all filtered packets by address and sum their RSSI and count
	 * 	- scan for specific packets and send them back
	 * 	-
	 *
	 * */

	struct RssiContainer
	{
		u8 rssi37;
		u8 rssi38;
		u8 rssi39;
		u8 count;
		u16 channelCount[3];
	};

	//Storage for advertising packets
	struct ScannedAssetTrackingStorage
	{
		RssiContainer rssiContainer;
		u32 serialNumberIndex;
		NodeId nodeId;
		u8 speed;
		u16 pressure;
		u8 hasFreeInConnection : 1;
		u8 interestedInConnection : 1;
		u8 hasSameNetworkId : 1;
		u8 reservedBits : 5;
	};

	SimpleArray<ScannedAssetTrackingStorage, ASSET_PACKET_BUFFER_SIZE> assetPackets;

	typedef struct
	{
		FruityHal::BleGapAddr address;
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


	enum class ScanModuleMessages : u8 {
		//TOTAL_SCANNED_PACKETS=0,  //Removed as of 21.05.2019
		//ASSET_TRACKING_PACKET=1,  //Removed as of 24.10.2019
		ASSET_INS_TRACKING_PACKET = 2,
	};

	//####### Module specific message structs (these need to be packed)
#pragma pack(push)
#pragma pack(1)

//Asset Message V2
	static constexpr int SIZEOF_SCAN_MODULE_TRACKED_ASSET_V2 = 8;
	typedef struct
	{
		u32 assetId : 24;	//Either part of the serialNumberIndex (old assets) or the nodeId
		i32 rssi37 : 8;		//Compilerhack! MSVC refuses to combine this bitfield with the previous one if we declare it as i8!
		i8 rssi38;
		i8 rssi39;
		u8 speed : 4;
		u8 hasFreeInConnection : 1;
		u8 interestedInConnection : 1;
		u8 hasSameNetworkId : 1;
		u8 reservedBits : 1;
		u8 pressure;
	} trackedAssetV2;
	STATIC_ASSERT_SIZE(trackedAssetV2, 8);

	typedef struct
	{
		connPacketHeader header;
		trackedAssetV2 trackedAssets[1];
	} ScanModuleTrackedAssetsV2Message;

	struct TrackedAssetInsMessage
	{
		NodeId assetNodeId;
		i8 rssi37;
		i8 rssi38;
		i8 rssi39;
		u8 batteryPower;
		u16 absolutePositionX;
		u16 absolutePositionY;
		u8 pressure;
		u8 moving : 1;
		u8 hasFreeInConnection : 1;
		u8 interestedInConnection : 1;
		u8 hasSameNetworkId : 1;
		u8 reservedBits : 4;
	};
	STATIC_ASSERT_SIZE(TrackedAssetInsMessage, 12);

	//Storage for INS advertising packets
	struct ScannedAssetInsTrackingStorage
	{
		RssiContainer rssiContainer;
		NodeId assetNodeId;
		u8 batteryPower;
		u16 absolutePositionX;
		u16 absolutePositionY;
		u16 pressure;
		u8 moving : 1;
		u8 hasFreeInConnection : 1;
		u8 interestedInConnection : 1;
		u8 hasSameNetworkId : 1;
		u8 reservedBits : 5;
	};

	SimpleArray<ScannedAssetInsTrackingStorage, ASSET_INS_PACKET_BUFFER_SIZE> assetInsPackets;

	//####### End of Module specitic messages
#pragma pack(pop)


//Asset packet handling
	void HandleAssetV2Packets(const FruityHal::GapAdvertisementReportEvent& advertisementReportEvent);
	void HandleAssetInsPackets(const FruityHal::GapAdvertisementReportEvent& advertisementReportEvent);
	bool addTrackedAsset(const advPacketAssetServiceData* packet, i8 rssi);
	bool addTrackedAssetIns(const advPacketAssetInsServiceData* packet, i8 rssi);
	void ReceiveTrackedAssets(BaseConnectionSendData* sendData, ScanModuleTrackedAssetsV2Message const * packet) const;
	void ReceiveTrackedAssetsIns(TrackedAssetInsMessage const * msg, u32 amount, NodeId sender) const;
	void RssiRunningAverageCalculationInPlace(RssiContainer &container, u8 advertisingChannel, i8 rssi);

	//Byte muss gesetzt sein, byte darf nicht gesetzt sein, byte ist egal
	bool setScanFilter(scanFilterEntry* filter);

	void SendReport();

	bool isAssetTrackingData(u8* data, u8 dataLength);
	void resetAssetTrackingTable();
	void SendTrackedAssets();
	void SendTrackedAssetsIns();
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

	static u8 ConvertServiceDataToMeshMessageSpeed(u8 serviceDataSpeed);
	u8 ConvertServiceDataToMeshMessagePressure(u16 serviceDataPressure);


public:
	u16 assetReportingIntervalDs = 0;

	ScanJob * p_scanJob;

	DECLARE_CONFIG_AND_PACKED_STRUCT(ScanningModuleConfiguration);

	ScanningModule();

	void ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength) override;

	void ResetToDefaultConfiguration() override;

	void TimerEventHandler(u16 passedTimeDs) override;

	virtual void GapAdvertisementReportEventHandler(const FruityHal::GapAdvertisementReportEvent& advertisementReportEvent) override;

	void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader const * packetHeader) override;

#ifdef TERMINAL_ENABLED
	TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override;
#endif
};

