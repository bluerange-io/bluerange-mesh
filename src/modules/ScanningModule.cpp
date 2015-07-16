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

ScanningModule::ScanningModule(u16 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot) :
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
	if (configuration.version == 1)
	{/* ... */
	};

	//Do additional initialization upon loading the config


	//Start the Module...
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

}

void ScanningModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = false;
	configuration.version = 1;

	//Set additional config values...

	//TODO: This is for testing only
	scanFilterEntry filter;

	filter.grouping = groupingType::NO_GROUPING;
	filter.address.addr_type = 0xFF;
	filter.advertisingType = 0xFF;
	filter.minRSSI = -100;
	filter.maxRSSI = 100;

	setScanFilter(&filter);

}

void ScanningModule::BleEventHandler(ble_evt_t* bleEvent)
{
	if(!configuration.moduleActive) return;

	switch (bleEvent->header.evt_id)
	{
		case BLE_GAP_EVT_ADV_REPORT:
		{

			//Do not handle mesh packets...
			advPacketHeader* packetHeader = (advPacketHeader*) bleEvent->evt.gap_evt.params.adv_report.data;
			if (
					bleEvent->evt.gap_evt.params.adv_report.dlen >= SIZEOF_ADV_PACKET_HEADER
					&& packetHeader->manufacturer.companyIdentifier == COMPANY_IDENTIFIER
					&& packetHeader->meshIdentifier == MESH_IDENTIFIER
					&& packetHeader->networkId == Node::getInstance()->persistentConfig.networkId
				)
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
};

void ScanningModule::NodeStateChangedHandler(discoveryState newState)
{
	if(newState == discoveryState::BACK_OFF){
		ScanController::SetScanState(scanState::SCAN_STATE_HIGH);
	} else {
		//TODO: disable scanning before node is active again
	}
}

bool ScanningModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	//Must be called to allow the module to get and set the config
	Module::TerminalCommandHandler(commandName, commandArgs);

	//React on commands, return true if handled, false otherwise

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
