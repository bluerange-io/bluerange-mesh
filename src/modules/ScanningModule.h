////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// ** BlueRange Mesh – Community Edition (CE)
// ** Copyright (c) 2015-2021 MWAY DIGITAL GmbH, Germany
// ** Copyright (c) 2021-2026 BlueRange GmbH, Germany
// **
// ** This file is part of BlueRange Mesh Community Edition (formerly known as
// ** FruityMesh).
// **
// ** BlueRange Mesh Community Edition is free software: you can redistribute it
// ** and/or modify it under the terms of the GNU General Public License as
// ** published by the Free Software Foundation, either version 3 of the
// ** License, or (at your option) any later version.
// **
// ** BlueRange Mesh Community Edition is distributed in the hope that it will
// ** be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// ** See the GNU General Public License for more details.
// **
// ** You should have received a copy of the GNU General Public License along
// ** with this program. If not, see https://www.gnu.org/licenses/.
// **
// ** IMPORTANT:
// ** Any modification, extension, or derivative work of this file MUST also be
// ** licensed under the GNU General Public License v3 or later and the complete
// ** corresponding source code MUST be made available.
// **
// ** Commercial Use:
// ** If you wish to use this software without the obligations of the GPLv3
// ** (including source code disclosure), a commercial license for
// ** BlueRange Mesh OEM Edition is required.
// **
// ** License violations automatically terminate your rights under this license
// ** and may result in legal action under applicable law.
// ** For further information please use the contact form at:
// ** https://bluerange.io/en/contact
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <array>
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
constexpr int ASSET_PACKET_RSSI_SEND_THRESHOLD = -88;

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
    std::array<u8, 31> byteMask;
    std::array<u8, 31> mandatory;

} scanFilterEntry;

#pragma pack(push, 1)
//Module configuration that is saved persistently
struct ScanningModuleConfiguration : ModuleConfiguration{
    //Insert more persistent config values here
};
#pragma pack(pop)

constexpr int SIZEOF_SCAN_MODULE_TRACKED_ASSET_LEGACY = 8;
/*
 * The ScanModule should provide filtering so that all nodes are able to scan
 * for advertising messages and broadcast them through the mesh. (not yet implemented)
 * This module should also allow to trigger certain tasks after specific
 * packets have been scanned.
 * Currently it is used for scanning asset packets.
 *
 */
class ScanningModule : public Module
{
private:
    static constexpr u16 groupedReportingIntervalDs = 0;
    /*
     * Filters could be:
     *     - group all filtered packets by address and sum their RSSI and count
     *     - scan for specific packets and send them back
     *     -
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

    std::array<scanFilterEntry, SCAN_FILTER_NUMBER> scanFilters;

    //For total message counting
    //u32 totalMessages;
    //i32 totalRSSI;

    // Addresses of active devices
    //uint8_t addressPointer;
    //std::array<SimpleArray<u8, FH_BLE_GAP_ADDR_LEN>, NUM_ADDRESSES_TRACKED> addresses;
    //std::array<u32, NUM_ADDRESSES_TRACKED> totalRSSIsPerAddress;
    //std::array<u32, NUM_ADDRESSES_TRACKED> totalMessagesPerAddress;

    u32 totalMessages;
    u32 totalRSSI;


    enum class ScanModuleMessages : u8 {
        //TOTAL_SCANNED_PACKETS=0,  //Removed as of 21.05.2019
        //ASSET_LEGACY_TRACKING_PACKET=1,  //Removed as of 24.10.2019
        ASSET_TRACKING_PACKET = 2,
    };

    //####### Module specific message structs (these need to be packed)
#pragma pack(push)
#pragma pack(1)

//Asset Message V2 (Legacy)
    typedef struct
    {
        u32 assetId : 24;    //Either part of the serialNumberIndex (old assets) or the nodeId
        i32 rssi37 : 8;        //Compilerhack! MSVC refuses to combine this bitfield with the previous one if we declare it as i8!
        i8 rssi38;
        i8 rssi39;
        u8 speed : 4;
        u8 hasFreeInConnection : 1;
        u8 interestedInConnection : 1;
        u8 hasSameNetworkId : 1;
        u8 reservedBits : 1;
        u8 pressure;
    } TrackedAssetLegacy;
    STATIC_ASSERT_SIZE(TrackedAssetLegacy, 8);

    typedef struct
    {
        ConnPacketHeader header;
        TrackedAssetLegacy trackedAssets[1];
    } ScanModuleTrackedAssetsLegacyMessage;
public:
    struct TrackedAssetMessage
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
        u8 positionValid : 1;
        u8 reservedBits : 3;
    };
    STATIC_ASSERT_SIZE(TrackedAssetMessage, 12);
    constexpr static u32 SIZEOF_TRACKED_ASSET_MESSAGE_WITH_CONN_PACKET_HEADER =  sizeof(ScanningModule::TrackedAssetMessage) + SIZEOF_CONN_PACKET_HEADER;
private:

    //Storage for Asset advertising packets
    struct ScannedAssetTrackingStorage
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
        u8 positionValid : 1;
        u8 reservedBits : 3;
    };

    std::array<ScannedAssetTrackingStorage, ASSET_PACKET_BUFFER_SIZE> assetPackets{};

    //####### End of Module specific messages
#pragma pack(pop)


//Asset packet handling
    void HandleAssetLegacyPackets(const FruityHal::GapAdvertisementReportEvent& advertisementReportEvent);
    void HandleAssetPackets(const FruityHal::GapAdvertisementReportEvent& advertisementReportEvent);
    bool AddTrackedAsset(const AdvPacketLegacyV2AssetServiceData* packet, i8 rssi);
    void ReceiveTrackedAssetsLegacy(BaseConnectionSendData* sendData, ScanModuleTrackedAssetsLegacyMessage const * packet) const;
    void ReceiveTrackedAssets(TrackedAssetMessage const * msg, u32 amount, NodeId sender) const;
    void RssiRunningAverageCalculationInPlace(RssiContainer &container, u8 advertisingChannel, i8 rssi);

    void SendTrackedAssets();


    static u8 ConvertServiceDataToMeshMessageSpeed(u8 serviceDataSpeed);
    u8 ConvertServiceDataToMeshMessagePressure(u16 serviceDataPressure);


public:
    u16 assetReportingIntervalDs = 0;

    ScanJob * p_scanJob;

    DECLARE_CONFIG_AND_PACKED_STRUCT(ScanningModuleConfiguration);

    ScanningModule();

    void ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength) override final;

    void ResetToDefaultConfiguration() override final;

    void TimerEventHandler(u16 passedTimeDs) override final;

    virtual void GapAdvertisementReportEventHandler(const FruityHal::GapAdvertisementReportEvent& advertisementReportEvent) override final;

    void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader) override final;

    //Priority
    virtual DeliveryPriority GetPriorityOfMessage(const u8* data, MessageLength size) override;

#ifdef TERMINAL_ENABLED
    TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override final;
#endif
};
