/**
 OS_LICENSE_PLACEHOLDER
 */

#include <Logger.h>
#include <ScanController.h>
#include <ScanningModule.h>
#include <Utility.h>
#include <Storage.h>
#include <Node.h>

extern "C"
{
}

//This module scans for specific messages and reports them back
//This implementation is currently very basic and should just illustrate how
//such functionality could be implemented

ScanningModule::ScanningModule(u8 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot) :
		Module(moduleId, node, cm, name, storageSlot)
{
	//Register callbacks n' stuff
	Logger::getInstance().enableTag("SCANMOD");

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(ScanningModuleConfiguration);

	//Initialize scanFilters as empty
	for (int i = 0; i < SCAN_FILTER_NUMBER; i++)
	{
		scanFilters[i].active = 0;
	}

	//Start module configuration loading
	LoadModuleConfiguration();
}

void ScanningModule::ConfigurationLoadedHandler()
{
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if (configuration.moduleVersion == 1)
	{/* ... */
	};

	//Do additional initialization upon loading the config

	// Reset address pointer to the beginning of the address table
	resetAddressTable();
	resetTotalRSSIsPerAddress();
	resetTotalMessagesPerAdress();

	totalMessages = 0;
	totalRSSI = 0;

	//Start the Module...
}

void ScanningModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = true;
	configuration.moduleVersion = 1;

	//Set additional config values...
	configuration.reportingIntervalMs = 20 * 1000;

	//TODO: This is for testing only
	scanFilterEntry filter;

	filter.grouping = groupingType::GROUP_BY_ADDRESS;
	filter.address.addr_type = 0xFF;
	filter.advertisingType = 0xFF;
	filter.minRSSI = -100;
	filter.maxRSSI = 100;

	setScanFilter(&filter);

}

//This function is used to set a number of filters that all non-mesh advertising packets
//Will be evaluated against. If they pass the filter, they are reported
bool ScanningModule::setScanFilter(scanFilterEntry* filter)
{
	for (int i = 0; i < SCAN_FILTER_NUMBER; i++)
	{
		if (!scanFilters[i].active)
		{
			memcpy(scanFilters + i, filter, sizeof(scanFilterEntry));
			scanFilters[i].active = 1;

			return true;
		}
		return false;
	}
}

void ScanningModule::TimerEventHandler(u16 passedTime, u32 appTimer)
{
	//Do stuff on timer...
	if (configuration.reportingIntervalMs != 0 && node->appTimerMs - lastReportingTimerMs > configuration.reportingIntervalMs)
	{

		SendReport();

		resetAddressTable();
		resetTotalRSSIsPerAddress();
		resetTotalMessagesPerAdress();

		totalMessages = 0;
		totalRSSI = 0;

		lastReportingTimerMs = node->appTimerMs;
	}

}

void ScanningModule::SendReport()
{
	// The number of different addresses indicates the number of devices
	// that have been scanned during the last time slot.
	u32 totalDevices = addressPointer;
	u32 totalRSSI = computeTotalRSSI();

	// Log the address table
	logt("SCANMOD", "GAP address  |  Mean rssi  |  Total messages");
	for (int i = 0; i < addressPointer; i++)
	{
		uint8_t* address = addresses[i];
		u32 meanRSSI = totalRSSIsPerAddress[i] / totalMessagesPerAdress[i];
		u32 totalMessages = totalMessagesPerAdress[i];
		logt("SCANMOD", "0x%x  |  %d  |  %d", address, meanRSSI, totalMessages);
	}

	logt("SCANMOD", "Total devices:%d, avgRSSI:%d", totalDevices, totalRSSI);
	if (totalDevices > 0)
	{
		connPacketModule data;
		data.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
		data.header.sender = node->persistentConfig.nodeId;
		data.header.receiver = NODE_ID_BROADCAST; //Only send if sink available

		data.moduleId = moduleId;
		data.actionType = ScanModuleMessages::TOTAL_SCANNED_PACKETS;

		//Insert total messages and totalRSSI
		memcpy(data.data + 0, &totalDevices, 4);
		memcpy(data.data + 4, &totalRSSI, 4);

		cm->SendMessageToReceiver(NULL, (u8*) &data, SIZEOF_CONN_PACKET_MODULE + 8, false);
	}
}

void ScanningModule::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	//Must call superclass for handling
	Module::ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);

	if (packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION)
	{
		connPacketModule* packet = (connPacketModule*) packetHeader;

		//Check if our module is meant and we should trigger an action
		if (packet->moduleId == moduleId)
		{
			//It's a LED message
			if (packet->actionType == ScanModuleMessages::TOTAL_SCANNED_PACKETS)
			{

				u32 totalDevices;
				u32 totalRSSI;
				memcpy(&totalDevices, packet->data + 0, 4);
				memcpy(&totalRSSI, packet->data + 4, 4);

				uart("SCANMOD", "{\"nodeId\":%d, \"type\":\"sum_packets\", \"module\":%d, \"packets\":%u, \"rssi\":%d}" SEP, packet->header.sender, moduleId, totalDevices, totalRSSI);
			}
		}
	}
}

void ScanningModule::BleEventHandler(ble_evt_t* bleEvent)
{
	if (!configuration.moduleActive) return;

	switch (bleEvent->header.evt_id)
	{
		case BLE_GAP_EVT_ADV_REPORT:
		{

			//Do not handle mesh packets...
			advPacketHeader* packetHeader = (advPacketHeader*) bleEvent->evt.gap_evt.params.adv_report.data;
			if (bleEvent->evt.gap_evt.params.adv_report.dlen >= SIZEOF_ADV_PACKET_HEADER && packetHeader->manufacturer.companyIdentifier == COMPANY_IDENTIFIER && packetHeader->meshIdentifier == MESH_IDENTIFIER && packetHeader->networkId == Node::getInstance()->persistentConfig.networkId)
			{
				break;
			}

			//Only parse advertising packets and not scan response packets
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
					// Only consider mobile devices that are at most 5 meters away...
					if (rssi > RSSI_THRESHOLD)
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
										data.header.sender = node->persistentConfig.nodeId;
										data.header.receiver = NODE_ID_BROADCAST; //Only send if sink available

										memcpy(&data.payload.peerAddress, address->addr, 6);
										data.payload.packetCount = 1;
										data.payload.inverseRssiSum = -rssi;

										cm->SendMessageToReceiver(NULL, (u8*) &data, SIZEOF_CONN_PACKET_ADV_INFO, false);

									}

									logt("SCAN", "Packet filtered, rssi:%d, dataLength:%d addr:%02X:%02X:%02X:%02X:%02X:%02X", rssi, dataLength, address->addr[0], address->addr[1], address->addr[2], address->addr[3], address->addr[4], address->addr[5]);
								}
							}
						}
						break;
					}
				}
			}
		}
	}
}
;

void ScanningModule::NodeStateChangedHandler(discoveryState newState)
{
	if (newState == discoveryState::BACK_OFF)
	{
		ScanController::SetScanState(scanState::SCAN_STATE_LOW);
	}
	else
	{
		//TODO: disable scanning before node is active again
	}
}

bool ScanningModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	//React on commands, return true if handled, false otherwise

	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandName, commandArgs);
}

bool ScanningModule::advertiseDataWasSentFromMobileDevice(u8* data, u8 dataLength)
{
	return advertiseDataFromAndroidDevice(data, dataLength) || advertiseDataFromiOSDeviceInBackgroundMode(data, dataLength) || advertiseDataFromiOSDeviceInForegroundMode(data, dataLength) || advertiseDataFromBeaconWithDifferentNetworkId(data, dataLength);
}

bool ScanningModule::advertiseDataFromAndroidDevice(u8* data, u8 dataLength)
{
	// advertising data -> manufacturer specific data
	// = "41 6E 64 72 6F 69 64" = "Android"
	if (data[7] == 0x41 && 	// A
			data[8] == 0x6e && 	// n
			data[9] == 0x64 && 	// d
			data[10] == 0x72 && 	// r
			data[11] == 0x6f && 	// o
			data[12] == 0x69 && 	// i
			data[13] == 0x64		// d)
					)
	{
		//logt("SCANMOD", "Android device advertising.");
		return true;
	}
	else
	{
		return false;
	}
}

bool ScanningModule::advertiseDataFromiOSDeviceInBackgroundMode(u8* data, u8 dataLength)
{
	// advertising data -> manufacturer specific data
	// starts with "4c 00 01"
	if (data[5] == 0x4c && data[6] == 0x00 && data[7] == 0x01)
	{
		//logt("SCANMOD", "iOS device advertising (background).");
		return true;
	}
	else
	{
		return false;
	}
}

bool ScanningModule::advertiseDataFromiOSDeviceInForegroundMode(u8* data, u8 dataLength)
{
	// https://devzone.nordicsemi.com/documentation/nrf51/4.3.0/html/group___b_l_e___g_a_p___a_d___t_y_p_e___d_e_f_i_n_i_t_i_o_n_s.html
	// advertising data -> local name
	// = "iOS" = "69 4F 53"
	int i = 0;
	while (i < dataLength)
	{
		if (data[i + 1] == BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME)
		{
			if (data[i + 2] == 0x69 &&	// i
					data[i + 3] == 0x4f &&	// O
					data[i + 4] == 0x53		// S
							)
			{
				//logt("SCANMOD", "iOS device advertising (foreground).");
				return true;
			}
		}
		i += data[i] + 1;
	}
	return false;
}

bool ScanningModule::advertiseDataFromBeaconWithDifferentNetworkId(u8 *data, u8 dataLength)
{
	// advertising data -> manufacturer specific data
	// starts with "42 63 6e" = "Bcn"
	if (data[5] == 0x42 && data[6] == 0x63 && data[7] == 0x6e)
	{
		logt("SCANMOD", "Beacon advertising.");
		return true;
	}
	else
	{
		return false;
	}
}

bool ScanningModule::addressAlreadyTracked(uint8_t* address)
{
	for (int i = 0; i < addressPointer; i++)
	{
		if (memcmp(&(addresses[i]), address, BLE_GAP_ADDR_LEN) == 0)
		{
			return true;
		}
	}
	return false;
}

void ScanningModule::resetAddressTable()
{
	addressPointer = 0;
}

void ScanningModule::resetTotalRSSIsPerAddress()
{
	for (int i = 0; i < NUM_ADDRESSES_TRACKED; i++)
	{
		totalRSSIsPerAddress[i] = 0;
	}
}

void ScanningModule::resetTotalMessagesPerAdress()
{
	for (int i = 0; i < NUM_ADDRESSES_TRACKED; i++)
	{
		totalMessagesPerAdress[i] = 0;
	}
}

void ScanningModule::updateTotalRssiAndTotalMessagesForDevice(i8 rssi, uint8_t* address)
{
	for (int i = 0; i < addressPointer; i++)
	{
		if (memcmp(&(addresses[i]), address, BLE_GAP_ADDR_LEN) == 0)
		{
			totalRSSIsPerAddress[i] += (u32) (-rssi);
			totalMessagesPerAdress[i]++;
		}
	}
}

u32 ScanningModule::computeTotalRSSI()
{
	// Special case: If no devices have been found,
	// totalRSSI = 0 should be returned.
	if (addressPointer == 0)
	{
		return 0;
	}

	// Compute the mean of all RSSI values for each address.
	u32 meanRSSIsPerAddress[addressPointer];
	for (int i = 0; i < addressPointer; i++)
	{
		meanRSSIsPerAddress[i] = totalRSSIsPerAddress[i] / totalMessagesPerAdress[i];
	}

	// Sum up all the RSSI values to get a value that
	// indicates the mobile device density around the node.
	u32 totalRSSI = 0;
	for (int i = 0; i < addressPointer; i++)
	{
		totalRSSI += meanRSSIsPerAddress[i];
	}

	// Return the totalRSSI
	return totalRSSI;
}

//currently not used
u32 bleParseAdvData(u8 type, sizedData* advData, sizedData* p_typedata)
{
	uint32_t index = 0;
	uint8_t * p_data;

	p_data = advData->data;

	while (index < advData->length)
	{
		uint8_t field_length = p_data[index];
		uint8_t field_type = p_data[index + 1];

		if (field_type == type)
		{
			p_typedata->data = &p_data[index + 2];
			p_typedata->length = field_length - 1;
			return NRF_SUCCESS;
		}
		index += field_length + 1;
	}
	return NRF_ERROR_NOT_FOUND;
}

