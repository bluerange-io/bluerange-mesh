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

#define SCAN_MODULE_CONFIG_VERSION 1

#include <ScanningModule.h>


#ifdef ACTIVATE_SCANNING_MODULE

#include <Logger.h>
#include <ScanController.h>
#include <Utility.h>
#include <Node.h>
#include <stdlib.h>

#ifdef ACTIVATE_ASSET_MODULE
#include <AssetModule.h>
#endif


//This module scans for specific messages and reports them back
//This implementation is currently very basic and should just illustrate how
//such functionality could be implemented

ScanningModule::ScanningModule() :
		Module(moduleID::SCANNING_MODULE_ID, "scan")
{
	moduleVersion = SCAN_MODULE_CONFIG_VERSION;

	//Register callbacks n' stuff

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(ScanningModuleConfiguration);

	//Initialize scanFilters as empty
	for (int i = 0; i < SCAN_FILTER_NUMBER; i++)
	{
		scanFilters[i].active = 0;
	}

	//resetAssetTrackingTable();

	//Set defaults
	ResetToDefaultConfiguration();
}

void ScanningModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = true;
	configuration.moduleVersion = SCAN_MODULE_CONFIG_VERSION;

	//Set additional config values...
	configuration.assetReportingIntervalDs = 0;
	configuration.groupedReportingIntervalDs = 0;
	configuration.groupedPacketRssiThreshold = -70;

	//TODO: This is for testing only
	scanFilterEntry filter;

	filter.grouping = GroupingType::GROUP_BY_ADDRESS;
	filter.address.addr_type = 0xFF;
	filter.advertisingType = 0xFF;
	filter.minRSSI = -100;
	filter.maxRSSI = 100;

	SET_FEATURESET_CONFIGURATION(&configuration);
}

void ScanningModule::ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength)
{
	//Do additional initialization upon loading the config

	// Reset address pointer to the beginning of the address table
//	resetAddressTable();
//	resetTotalRSSIsPerAddress();
//	resetTotalMessagesPerAdress();

	totalMessages = 0;
	totalRSSI = 0;

	assetPackets.zeroData();

	//Start the Module...
}

#ifdef TERMINAL_ENABLED
bool ScanningModule::TerminalCommandHandler(char* commandArgs[], u8 commandArgsSize)
{
	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif

void ScanningModule::TimerEventHandler(u16 passedTimeDs)
{
	if(SHOULD_IV_TRIGGER(GS->appTimerDs, passedTimeDs, configuration.groupedReportingIntervalDs))
	{

		//Send grouped packets
//		SendReport();
//
//		resetAddressTable();
//		resetTotalRSSIsPerAddress();
//		resetTotalMessagesPerAdress();

		totalMessages = 0;
		totalRSSI = 0;
	}

	if(SHOULD_IV_TRIGGER(GS->appTimerDs, passedTimeDs, configuration.assetReportingIntervalDs)){
		//Send asset tracking packets
		SendTrackedAssets();
//		resetAssetTrackingTable();
	}
}

void ScanningModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader)
{
	//Must call superclass for handling
	Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

	if (packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION)
	{
		connPacketModule* packet = (connPacketModule*) packetHeader;

		//Check if our module is meant and we should trigger an action
		if (packet->moduleId == moduleId)
		{
			ScanModuleMessages actionType = (ScanModuleMessages)packet->actionType;
			//It's a LED message
			if (actionType == ScanModuleMessages::TOTAL_SCANNED_PACKETS)
			{

				u32 totalDevices;
				u32 totalRSSI;
				memcpy(&totalDevices, packet->data + 0, 4);
				memcpy(&totalRSSI, packet->data + 4, 4);

				logjson("SCANMOD", "{\"nodeId\":%d,\"type\":\"sum_packets\",\"module\":%d,\"packets\":%u,\"rssi\":%d}" SEP, packet->header.sender, moduleId, totalDevices, totalRSSI);
			}
		}
	}
	else if(packetHeader->messageType == MESSAGE_TYPE_ASSET_V2)
	{
		ScanModuleTrackedAssetsV2Message* packet = (ScanModuleTrackedAssetsV2Message*) packetHeader;

		ReceiveTrackedAssets(sendData, packet);
	}
}


void ScanningModule::BleEventHandler(const ble_evt_t& bleEvent)
{
	if (!configuration.moduleActive) return;

#ifndef SAVE_SPACE_GW_1
	switch (bleEvent.header.evt_id)
	{
		case BLE_GAP_EVT_ADV_REPORT:
		{
			HandleAssetV1Packets(bleEvent);

			HandleAssetV2Packets(bleEvent);

//			HandleOtherStuff();
		}
	}
#endif
}

/* HandleOtherStuff() ...
 *
 * //Only parse advertising packets and not scan response packets
			if (bleEvent->evt.gap_evt.params.adv_report.scan_rsp != 1)
			{
				u8 advertisingType = bleEvent->evt.gap_evt.params.adv_report.type;
				u8* data = bleEvent->evt.gap_evt.params.adv_report.data;
				u8 dataLength = bleEvent->evt.gap_evt.params.adv_report.dlen;
				ble_gap_addr_t* address = &bleEvent->evt.gap_evt.params.adv_report.peer_addr;
				i8 rssi = bleEvent->evt.gap_evt.params.adv_report.rssi;

				// If advertise data is sent by a mobile device
				if (advertiseDataWasSentFromMobileDevice(data, dataLength))
				{
					// Only consider mobile devices that are within a certain range...
					if (rssi > configuration.groupedPacketRssiThreshold)
					{
						totalMessages++;
						totalRSSI += rssi;
						//logt("SCANMOD", "RSSI: %d", rssi);
						// Save address in addressTable if address has not already been tracked before.
						if (!addressAlreadyTracked(address->addr))
						{
							// if more than NUM_ADDRESSES_TRACKED have already been tracked
							// do not track this address
							if (addressPointer < NUM_ADDRESSES_TRACKED)
							{
								memcpy(&addresses[addressPointer], &(address->addr), BLE_GAP_ADDR_LEN);
								addressPointer++;
							}
						}
						// Update RSSI for the mobile device
						updateTotalRssiAndTotalMessagesForDevice(rssi, address->addr);
					}
				}

				u16 assetId = 0;

				if(isAssetTrackingData(data, dataLength)){
					//Extract the id from the packet
					memcpy(&assetId, data + 8, 2);
				}
				else if(isAssetTrackingDataFromiOSDeviceInForegroundMode(data, dataLength))
				{
					logt("SCANMOD", "ios AT");
					//Use id 7 for all iOS packets, just because
					assetId = 7;
				}
				//TODO addTrackedAsset(assetId, rssi);


				//logt("SCAN", "Other packet, rssi:%d, dataLength:%d", rssi, dataLength);

				for (int i = 0; i < SCAN_FILTER_NUMBER; i++)
				{
					if (scanFilters[i].active)
					{
						//If address type is
						if (scanFilters[i].address.addr_type == 0xFF || scanFilters[i].address.addr_type == address->addr_type)
						{
							if (scanFilters[i].advertisingType == 0xFF || scanFilters[i].advertisingType == advertisingType)
							{
								if (scanFilters[i].minRSSI <= rssi && scanFilters[i].maxRSSI >= rssi)
								{

									if (scanFilters[i].grouping == GROUP_BY_ADDRESS)
									{
										for (int i = 0; i < SCAN_BUFFERS_SIZE; i++)
										{

										}
									}
									else if (scanFilters[i].grouping == NO_GROUPING)
									{
										logt("SCAN", "sending");

										//FIXME: Legacy packet structure, should use module message
										connPacketAdvInfo data;
										data.header.messageType = MESSAGE_TYPE_ADVINFO;
										data.header.sender = node->configuration.nodeId;
										data.header.receiver = NODE_ID_BROADCAST; //Only send if sink available

										memcpy(&data.payload.peerAddress, address->addr, 6);
										data.payload.packetCount = 1;
										data.payload.inverseRssiSum = -rssi;

										cm->SendMeshMessage((u8*) &data, SIZEOF_CONN_PACKET_ADV_INFO, DeliveryPriority::LOW, false);

									}

									logt("SCAN", "Packet filtered, rssi:%d, dataLength:%d addr:%02X:%02X:%02X:%02X:%02X:%02X", rssi, dataLength, address->addr[0], address->addr[1], address->addr[2], address->addr[3], address->addr[4], address->addr[5]);
								}
							}
						}
						break;
					}
				}
 *
 * */

#define _______________________ASSET_V2______________________

#ifndef SAVE_SPACE_GW_1
//This function checks whether we received an assetV1 packet and handles it
//This is the old packet format which contains less information and must be migrated
void ScanningModule::HandleAssetV1Packets(const ble_evt_t& bleEvent)
{
	 advPacketAssetV1* packet = (advPacketAssetV1*)bleEvent.evt.gap_evt.params.adv_report.data;

	//Check if the advertising packet is an asset packet
	if (
			bleEvent.evt.gap_evt.params.adv_report.dlen >= SIZEOF_ADV_PACKET_ASSET_V1
			&& packet->header.flags.len == SIZEOF_ADV_STRUCTURE_FLAGS-1
			&& packet->header.manufacturer.type == 0xFF
			&& packet->header.manufacturer.companyIdentifier == COMPANY_IDENTIFIER
			&& packet->header.messageType == MESSAGE_TYPE_ASSET_V1
	){
		char serial[6];
		Utility::GenerateBeaconSerialForIndex(packet->assetId, serial);
		logt("SCANMOD", "RX ASSETV1 ADV: serial %s", serial);

		advPacketAssetServiceData assetPacket;
		memset(&assetPacket, 0x00, sizeof(advPacketAssetServiceData));
		assetPacket.serialNumberIndex = packet->assetId;
		assetPacket.advertisingChannel = 0;
		assetPacket.direction = 0xFF;
		assetPacket.pressure = 0xFFFF;
		assetPacket.speed = 0xFF;

		//Adds the asset packet to our buffer
		addTrackedAsset(&assetPacket, bleEvent.evt.gap_evt.params.adv_report.rssi);
	}
}

//This function checks whether we received an assetV2 packet
void ScanningModule::HandleAssetV2Packets(const ble_evt_t& bleEvent)
{
	advPacketServiceAndDataHeader* packet = (advPacketServiceAndDataHeader*)bleEvent.evt.gap_evt.params.adv_report.data;
	advPacketAssetServiceData* assetPacket = (advPacketAssetServiceData*)&packet->data;

	//Check if the advertising packet is an asset packet
	if (
			bleEvent.evt.gap_evt.params.adv_report.dlen >= SIZEOF_ADV_STRUCTURE_ASSET_SERVICE_DATA
			&& packet->flags.len == SIZEOF_ADV_STRUCTURE_FLAGS-1
			&& packet->uuid.len == SIZEOF_ADV_STRUCTURE_UUID16-1
			&& packet->data.type == BLE_GAP_AD_TYPE_SERVICE_DATA
			&& packet->data.uuid == SERVICE_DATA_SERVICE_UUID16
			&& packet->data.messageType == SERVICE_DATA_MESSAGE_TYPE_ASSET
	){
		char serial[6];
		Utility::GenerateBeaconSerialForIndex(assetPacket->serialNumberIndex, serial);
		logt("SCANMOD", "RX ASSETV2 ADV: serial %s, pressure %u, speed %u, temp %u, humid %u, cn %u, rssi %d",
				serial,
				assetPacket->pressure,
				assetPacket->speed,
				assetPacket->temperature,
				assetPacket->humidity,
				assetPacket->advertisingChannel,
				bleEvent.evt.gap_evt.params.adv_report.rssi);


		//Adds the asset packet to our buffer
		addTrackedAsset(assetPacket, bleEvent.evt.gap_evt.params.adv_report.rssi);

	}
}

/**
 * Finds a free slot in our buffer of asset packets and adds the packet
 */
bool ScanningModule::addTrackedAsset(advPacketAssetServiceData* packet, i8 rssi){
	if(packet->serialNumberIndex == 0) return false;

	rssi = -rssi; //Make rssi positive

	if(rssi < 10 || rssi > 90) return false; //filter out wrong rssis

	scannedAssetTrackingPacket* slot = nullptr;

	//Look for an old entry of this asset or a free space
	//Because we fill this buffer from the beginning, we can use the first slot that is empty
	for(int i = 0; i<ASSET_PACKET_BUFFER_SIZE; i++){
		if(assetPackets[i].serialNumberIndex == packet->serialNumberIndex || assetPackets[i].serialNumberIndex == 0){
			slot = &assetPackets[i];
			break;
		}
	}

	//If a slot was found, add the packet
	if(slot != nullptr){
		u16 slotNum = ((u32)slot - (u32)assetPackets.getRaw()) / sizeof(scannedAssetTrackingPacket);
		logt("SCANMOD", "Tracked packet %u in slot %u", packet->serialNumberIndex, slotNum);

		//Clean up first, if we overwrite another assetId
		if(slot->serialNumberIndex != packet->serialNumberIndex){
			slot->serialNumberIndex = packet->serialNumberIndex;
			slot->count = 0;
			slot->rssi37 = slot->rssi38 = slot->rssi39 = UINT8_MAX;
		}
		//If the count is at its max, we reset the rssi
		if(slot->count == UINT8_MAX){
			slot->count = 0;
			slot->rssi37 = slot->rssi38 = slot->rssi39 = UINT8_MAX;
		}

		slot->serialNumberIndex = packet->serialNumberIndex;
		slot->count++;
		//Channel 0 means that we have no channel data, add it to all rssi channels
		if(packet->advertisingChannel == 0 && rssi < slot->rssi37){
			slot->rssi37 = (u16) rssi;
			slot->rssi38 = (u16) rssi;
			slot->rssi39 = (u16) rssi;
		}
		if(packet->advertisingChannel == 1 && rssi < slot->rssi37) slot->rssi37 = (u16) rssi;
		if(packet->advertisingChannel == 2 && rssi < slot->rssi38) slot->rssi38 = (u16) rssi;
		if(packet->advertisingChannel == 3 && rssi < slot->rssi39) slot->rssi39 = (u16) rssi;
		slot->direction = packet->direction;
		slot->pressure = packet->pressure;
		slot->speed = packet->speed;

		return true;
	}
	return false;
}
#endif

/**
 * Sends out all tracked assets from our buffer and resets the buffer
 */

//FIXME: rssi threshold must be used somewhere, apply when receiving packet?
//FIXME: do we average packets or do we just take the best rssi

void ScanningModule::SendTrackedAssets()
{
#ifndef SAVE_SPACE_GW_1
	//Find out how many assets were tracked
	u8 count = 0;
	for(int i=0; i<ASSET_PACKET_BUFFER_SIZE; i++){
		if(assetPackets[i].serialNumberIndex == 0) break;
		count++;
	}

	if(count == 0) return;

	u16 messageLength = SIZEOF_CONN_PACKET_HEADER + SIZEOF_SCAN_MODULE_TRACKED_ASSET_V2 * count;

	//Allocate a buffer big enough and fill the packet
	DYNAMIC_ARRAY(buffer, messageLength);
	ScanModuleTrackedAssetsV2Message* message = (ScanModuleTrackedAssetsV2Message*) buffer;

	message->header.messageType = MESSAGE_TYPE_ASSET_V2;
	message->header.sender = GS->node->configuration.nodeId;
	message->header.receiver = NODE_ID_BROADCAST; //FIXME: only to sink

	for(int i=0; i<count; i++){
		message->trackedAssets[i].assetId = assetPackets[i].serialNumberIndex;
		message->trackedAssets[i].rssi37 = assetPackets[i].rssi37;
		message->trackedAssets[i].rssi38 = assetPackets[i].rssi38;
		message->trackedAssets[i].rssi39 = assetPackets[i].rssi39;

		if(assetPackets[i].speed == 0xFF) message->trackedAssets[i].speed = 0xF;
		else if(assetPackets[i].speed > 140) message->trackedAssets[i].speed = 14;
		else if(assetPackets[i].speed == 1) message->trackedAssets[i].speed = 1;
		else message->trackedAssets[i].speed = assetPackets[i].speed / 10;

		message->trackedAssets[i].direction = assetPackets[i].direction / 16; //TODO: convert meaningful

		if(assetPackets[i].pressure == 0xFFFF) message->trackedAssets[i].pressure = 0xFF;
		else message->trackedAssets[i].pressure = (u8)(assetPackets[i].pressure % 250); //Will wrap, which is ok (we still have a relative pressure, but not the absolute one, mod 250 to reserve 0xFF for not available)
	}

	//Send the packet as a non-module message to save some bytes in the header
	GS->cm->SendMeshMessage(
			buffer,
			(u8)messageLength,
			DeliveryPriority::LOW,
			false);

	//Clear the buffer
	assetPackets.zeroData();
#endif
}

void ScanningModule::ReceiveTrackedAssets(BaseConnectionSendData* sendData, ScanModuleTrackedAssetsV2Message* packet) const
{
	u8 count = (sendData->dataLength - SIZEOF_CONN_PACKET_HEADER)  / SIZEOF_SCAN_MODULE_TRACKED_ASSET_V2;

	logjson("SCANMOD", "{\"nodeId\":%d,\"type\":\"tracked_assets\",\"assets\":[", packet->header.sender);

	for(int i=0; i<count; i++){
		trackedAssetV2* assetData = packet->trackedAssets + i;

		i8 speed = assetData->speed == 0xF ? -1 : assetData->speed;
		i8 direction = assetData->direction == 0xF ? -1 : assetData->direction;
		i16 pressure = assetData->pressure == 0xFF ? -1 : assetData->pressure; //(taken %250 to exclude 0xFF)

		if(i != 0) logjson("SCANMOD", ",");
		logjson("SCANMOD", "{\"id\":%u,\"rssi1\":%d,\"rssi2\":%d,\"rssi3\":%d,\"speed\":%d,\"direction\":%d,\"pressure\":%d}",
				assetData->assetId,
				assetData->rssi37,
				assetData->rssi38,
				assetData->rssi39,
				speed,
				direction,
				pressure);

		//logt("SCANMOD", "MESH RX id: %u, rssi %u, speed %u", assetData->assetId, assetData->rssi, assetData->speed);
	}

	logjson("SCANMOD", "]}" SEP);
}

#define ________________OTHER________________________

////This function is used to set a number of filters that all non-mesh advertising packets
////Will be evaluated against. If they pass the filter, they are reported
//bool ScanningModule::setScanFilter(scanFilterEntry* filter)
//{
//	for (int i = 0; i < SCAN_FILTER_NUMBER; i++)
//	{
//		if (!scanFilters[i].active)
//		{
//			memcpy(scanFilters + i, filter, sizeof(scanFilterEntry));
//			scanFilters[i].active = 1;
//
//			return true;
//		}
//	}
//	return false;
//}
//
//void ScanningModule::SendReport()
//{
//	// The number of different addresses indicates the number of devices
//	// that have been scanned during the last time slot.
//	u32 totalDevices = addressPointer;
//	u32 totalRSSI = computeTotalRSSI();
//
//	// Log the address table
//	logt("SCANMOD", "GAP address  |  Mean rssi  |  Total messages");
//	for (int i = 0; i < addressPointer; i++)
//	{
//		uint8_t* address = addresses[i];
//		u32 meanRSSI = totalRSSIsPerAddress[i] / totalMessagesPerAdress[i];
//		u32 totalMessages = totalMessagesPerAdress[i];
//		logt("SCANMOD", "0x%x  |  %d  |  %d", address, meanRSSI, totalMessages);
//	}
//
//	logt("SCANMOD", "Total devices:%d, avgRSSI:%d", totalDevices, totalRSSI);
//	if (totalDevices > 0)
//	{
//		connPacketModule data;
//		data.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
//		data.header.sender = node->configuration.nodeId;
//		data.header.receiver = NODE_ID_BROADCAST; //Only send if sink available
//
//		data.moduleId = moduleId;
//		data.actionType = ScanModuleMessages::TOTAL_SCANNED_PACKETS;
//
//		//Insert total messages and totalRSSI
//		memcpy(data.data + 0, &totalDevices, 4);
//		memcpy(data.data + 4, &totalRSSI, 4);
//
//		SendModuleActionMessage(
//			MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
//			NODE_ID_BROADCAST,
//			ScanModuleMessages::TOTAL_SCANNED_PACKETS,
//			0,
//			(u8*)&data,
//			SIZEOF_CONN_PACKET_MODULE + 8,
//			false
//		);
//	}
//}
//
//
//
//
//
//bool ScanningModule::isAssetTrackingData(u8* data, u8 dataLength){
//	//02 01 06 XX FF 4D 02 02
//	if(data[4] == 0xFF && data[5] == 0x4D && data[6] == 0x02){
//		//Identifier for telemetry packet v1
//		if(data[7] == 0x02){
//			if(!(data[8] == 0 && data[9] == 0 && data[10] == 0 && data[11] == 0)){
//				return true;
//			}
//		}
//	}
//	return false;
//}
//
//bool ScanningModule::advertiseDataWasSentFromMobileDevice(u8* data, u8 dataLength)
//{
//	return advertiseDataFromAndroidDevice(data, dataLength) || advertiseDataFromiOSDeviceInBackgroundMode(data, dataLength) || advertiseDataFromiOSDeviceInForegroundMode(data, dataLength) || advertiseDataFromBeaconWithDifferentNetworkId(data, dataLength);
//}
//
//bool ScanningModule::advertiseDataFromAndroidDevice(u8* data, u8 dataLength)
//{
//	// advertising data -> manufacturer specific data
//	// = "41 6E 64 72 6F 69 64" = "Android"
//	if (data[7] == 0x41 && 	// A
//			data[8] == 0x6e && 	// n
//			data[9] == 0x64 && 	// d
//			data[10] == 0x72 && 	// r
//			data[11] == 0x6f && 	// o
//			data[12] == 0x69 && 	// i
//			data[13] == 0x64		// d)
//					)
//	{
//		//logt("SCANMOD", "Android device advertising.");
//		return true;
//	}
//	else
//	{
//		return false;
//	}
//}
//
//bool ScanningModule::advertiseDataFromiOSDeviceInBackgroundMode(u8* data, u8 dataLength)
//{
//	// advertising data -> manufacturer specific data
//	// starts with "4c 00 01"
//	if (data[5] == 0x4c && data[6] == 0x00 && data[7] == 0x01)
//	{
//		//logt("SCANMOD", "iOS device advertising (background).");
//		return true;
//	}
//	else
//	{
//		return false;
//	}
//}
//
//bool ScanningModule::advertiseDataFromiOSDeviceInForegroundMode(u8* data, u8 dataLength)
//{
//	// https://devzone.nordicsemi.com/documentation/nrf51/4.3.0/html/group___b_l_e___g_a_p___a_d___t_y_p_e___d_e_f_i_n_i_t_i_o_n_s.html
//	// advertising data -> local name
//	// = "iOS" = "69 4F 53"
//	int i = 0;
//	while (i < dataLength)
//	{
//		if (data[i + 1] == BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME)
//		{
//			if (data[i + 2] == 0x69 &&	// i
//					data[i + 3] == 0x4f &&	// O
//					data[i + 4] == 0x53		// S
//							)
//			{
//				//logt("SCANMOD", "iOS device advertising (foreground).");
//				return true;
//			}
//		}
//		i += data[i] + 1;
//	}
//	return false;
//}
//
//bool ScanningModule::isAssetTrackingDataFromiOSDeviceInForegroundMode(u8* data, u8 dataLength)
//{
//	// https://devzone.nordicsemi.com/documentation/nrf51/4.3.0/html/group___b_l_e___g_a_p___a_d___t_y_p_e___d_e_f_i_n_i_t_i_o_n_s.html
//	// advertising data -> local name
//	// = "iOS" = "69 4F 53"
//	int i = 0;
//	while (i < dataLength)
//	{
//		if (data[i + 1] == BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME)
//		{
//			if (data[i + 2] == 0x61 &&	// a
//					data[i + 3] == 0x74 &&	// t
//					data[i + 4] == 0x69	&&	// i
//					data[i + 5] == 0x4f	&&	// O
//					data[i + 6] == 0x53		// S
//							)
//			{
//				//logt("SCANMOD", "iOS device advertising (foreground).");
//				return true;
//			}
//		}
//		i += data[i] + 1;
//	}
//	return false;
//}
//
//bool ScanningModule::advertiseDataFromBeaconWithDifferentNetworkId(u8 *data, u8 dataLength)
//{
//	// advertising data -> manufacturer specific data
//	// starts with "42 63 6e" = "Bcn"
//	if (data[5] == 0x42 && data[6] == 0x63 && data[7] == 0x6e)
//	{
//		logt("SCANMOD", "Beacon advertising.");
//		return true;
//	}
//	else
//	{
//		return false;
//	}
//}
//
//void ScanningModule::resetAssetTrackingTable(){
//	memset(assetPackets, 0x00, sizeof(assetPackets));
//}
//
//bool ScanningModule::addressAlreadyTracked(uint8_t* address)
//{
//	for (int i = 0; i < addressPointer; i++)
//	{
//		if (memcmp(&(addresses[i]), address, BLE_GAP_ADDR_LEN) == 0)
//		{
//			return true;
//		}
//	}
//	return false;
//}
//
//void ScanningModule::resetAddressTable()
//{
//	addressPointer = 0;
//}
//
//void ScanningModule::resetTotalRSSIsPerAddress()
//{
//	for (int i = 0; i < NUM_ADDRESSES_TRACKED; i++)
//	{
//		totalRSSIsPerAddress[i] = 0;
//	}
//}
//
//void ScanningModule::resetTotalMessagesPerAdress()
//{
//	for (int i = 0; i < NUM_ADDRESSES_TRACKED; i++)
//	{
//		totalMessagesPerAdress[i] = 0;
//	}
//}
//
//void ScanningModule::updateTotalRssiAndTotalMessagesForDevice(i8 rssi, uint8_t* address)
//{
//	for (int i = 0; i < addressPointer; i++)
//	{
//		if (memcmp(&(addresses[i]), address, BLE_GAP_ADDR_LEN) == 0)
//		{
//			totalRSSIsPerAddress[i] += (u32) (-rssi);
//			totalMessagesPerAdress[i]++;
//		}
//	}
//}
//
//u32 ScanningModule::computeTotalRSSI()
//{
//	// Special case: If no devices have been found,
//	// totalRSSI = 0 should be returned.
//	if (addressPointer == 0)
//	{
//		return 0;
//	}
//
//	// Compute the mean of all RSSI values for each address.
//	DYNAMIC_ARRAY(meanRSSIsPerAddressBuffer, addressPointer);
//	u32* meanRSSIsPerAddress = (u32*) meanRSSIsPerAddressBuffer;
//
//	for (int i = 0; i < addressPointer; i++)
//	{
//		meanRSSIsPerAddress[i] = totalRSSIsPerAddress[i] / totalMessagesPerAdress[i];
//	}
//
//	// Sum up all the RSSI values to get a value that
//	// indicates the mobile device density around the node.
//	u32 totalRSSI = 0;
//	for (int i = 0; i < addressPointer; i++)
//	{
//		totalRSSI += meanRSSIsPerAddress[i];
//	}
//
//	// Return the totalRSSI
//	return totalRSSI;
//}
//
////currently not used
//u32 bleParseAdvData(u8 type, sizedData* advData, sizedData* p_typedata)
//{
//	uint32_t index = 0;
//	uint8_t * p_data;
//
//	p_data = advData->data;
//
//	while (index < advData->length)
//	{
//		uint8_t field_length = p_data[index];
//		uint8_t field_type = p_data[index + 1];
//
//		if (field_type == type)
//		{
//			p_typedata->data = &p_data[index + 2];
//			p_typedata->length = field_length - 1;
//			return NRF_SUCCESS;
//		}
//		index += field_length + 1;
//	}
//	return NRF_ERROR_NOT_FOUND;
//}

#endif
