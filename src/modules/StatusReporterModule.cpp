/**

Copyright (c) 2014-2017 "M-Way Solutions GmbH"
FruityMesh - Bluetooth Low Energy mesh protocol [http://mwaysolutions.com/]

This file is part of FruityMesh

FruityMesh is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <Logger.h>
#include <StatusReporterModule.h>
#include <Utility.h>
#include <Node.h>
#include <Config.h>

extern "C"{
#include <app_error.h>
#include <stdlib.h>
}

#ifdef ACTIVATE_STATUS_REPORTER_MODULE

StatusReporterModule::StatusReporterModule(u8 moduleId, Node* node, ConnectionManager* cm, const char* name)
	: Module(moduleId, node, cm, name)
{
	moduleVersion = 1;

	//Register callbacks n' stuff

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(StatusReporterModuleConfiguration);

	//Start module configuration loading
	LoadModuleConfiguration();
}

void StatusReporterModule::ConfigurationLoadedHandler()
{
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if(configuration.moduleVersion == this->moduleVersion){/* ... */};

	//Start the Module...

}


void StatusReporterModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = false;
	configuration.moduleVersion = 1;

	configuration.statusReportingIntervalDs = SEC_TO_DS(80);
	configuration.connectionReportingIntervalDs = 0;
	configuration.connectionRSSISamplingMode = RSSISampingModes::RSSI_SAMLING_HIGH;
	configuration.advertisingRSSISamplingMode = RSSISampingModes::RSSI_SAMLING_HIGH;
	configuration.nearbyReportingIntervalDs = 0;
	configuration.deviceInfoReportingIntervalDs = SEC_TO_DS(100);

	memset(nodeMeasurements, 0x00, sizeof(nodeMeasurements));

	//Set additional config values...


}

void StatusReporterModule::TimerEventHandler(u16 passedTimeDs, u32 appTimerDs)
{
	//Device Info
	if(SHOULD_IV_TRIGGER(node->appTimerDs+node->appTimerRandomOffsetDs, passedTimeDs, configuration.deviceInfoReportingIntervalDs)){
		SendDeviceInfo(NODE_ID_BROADCAST, MESSAGE_TYPE_MODULE_ACTION_RESPONSE);
	}
	//Status
	if(SHOULD_IV_TRIGGER(node->appTimerDs+node->appTimerRandomOffsetDs, passedTimeDs, configuration.statusReportingIntervalDs)){
		SendStatus(NODE_ID_BROADCAST, MESSAGE_TYPE_MODULE_ACTION_RESPONSE);
	}
	//Connections
	if(SHOULD_IV_TRIGGER(node->appTimerDs+node->appTimerRandomOffsetDs, passedTimeDs, configuration.connectionReportingIntervalDs)){
		SendAllConnections(NODE_ID_BROADCAST, MESSAGE_TYPE_MODULE_GENERAL);
	}
	//Nearby Nodes
	if(SHOULD_IV_TRIGGER(node->appTimerDs+node->appTimerRandomOffsetDs, passedTimeDs, configuration.nearbyReportingIntervalDs)){
		SendNearbyNodes(NODE_ID_BROADCAST, MESSAGE_TYPE_MODULE_ACTION_RESPONSE);
	}
//	//ErrorLog
//	if(SHOULD_IV_TRIGGER(node->appTimerDs+node->appTimerRandomOffsetDs, passedTimeDs, SEC_TO_DS(4))){
//		SendErrors(0);
//	}
}


//This method sends the node's status over the network
void StatusReporterModule::SendStatus(nodeID toNode, u8 messageType)
{
	MeshConnections conn = GS->cm->GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_IN);

	StatusReporterModuleStatusMessage data;

	data.batteryInfo = node->GetBatteryRuntime();
	data.clusterSize = node->clusterSize;
	data.connectionLossCounter = (u8) node->connectionLossCounter; //TODO: connectionlosscounter is random at the moment, and the u8 will wrap
	data.freeIn = cm->freeMeshInConnections;
	data.freeOut = cm->freeMeshOutConnections;
	data.inConnectionPartner = conn.connections[0] == NULL ? 0 : conn.connections[0]->partnerId;
	data.inConnectionRSSI = conn.connections[0] == NULL ? 0 : conn.connections[0]->rssiAverage;
	data.initializedByGateway = node->initializedByGateway;

	SendModuleActionMessage(
		messageType,
		toNode,
		StatusModuleActionResponseMessages::STATUS,
		0,
		(u8*)&data,
		SIZEOF_STATUS_REPORTER_MODULE_STATUS_MESSAGE,
		false
	);
}

//Message type can be either MESSAGE_TYPE_MODULE_ACTION_RESPONSE or MESSAGE_TYPE_MODULE_GENERAL
void StatusReporterModule::SendDeviceInfo(nodeID toNode, u8 messageType)
{
	StatusReporterModuleDeviceInfoMessage data;

	data.manufacturerId = Config->manufacturerId;
	data.deviceType = node->persistentConfig.deviceType;
	memcpy(data.chipId, (u8*)NRF_FICR->DEVICEADDR, 8);
	memcpy(data.serialNumber, Config->serialNumber, NODE_SERIAL_NUMBER_LENGTH);
	FruityHal::BleGapAddressGet(&data.accessAddress);
	data.nodeVersion = fruityMeshVersion;
	data.networkId = node->persistentConfig.networkId;
	data.dBmRX = Boardconfig->dBmRX;
	data.dBmTX = node->persistentConfig.dBmTX;
	data.calibratedTX = Boardconfig->calibratedTX;

	SendModuleActionMessage(
		messageType,
		toNode,
		StatusModuleActionResponseMessages::DEVICE_INFO,
		0,
		(u8*)&data,
		SIZEOF_STATUS_REPORTER_MODULE_DEVICE_INFO_MESSAGE,
		false
	);
}

void StatusReporterModule::SendNearbyNodes(nodeID toNode, u8 messageType)
{
	u16 numMeasurements = 0;
	for(int i=0; i<NUM_NODE_MEASUREMENTS; i++){
		if(nodeMeasurements[i].nodeId != 0) numMeasurements++;
	}

	u8 packetSize = (u8)(numMeasurements * 3);
	DYNAMIC_ARRAY(buffer, packetSize);

	u16 j = 0;
	for(int i=0; i<NUM_NODE_MEASUREMENTS; i++)
	{
		if(nodeMeasurements[i].nodeId != 0){
			nodeID sender = nodeMeasurements[i].nodeId;
			i8 rssi = (i8)(nodeMeasurements[i].rssiSum / nodeMeasurements[i].packetCount);

			memcpy(buffer + j*3 + 0, &sender, 2);
			memcpy(buffer + j*3 + 2, &rssi, 1);

			j++;
		}
	}

	//Clear node measurements
	memset(nodeMeasurements, 0x00, sizeof(nodeMeasurements));

	SendModuleActionMessage(
		messageType,
		toNode,
		StatusModuleActionResponseMessages::NEARBY_NODES,
		0,
		buffer,
		packetSize,
		false
	);
}


//This method sends information about the current connections over the network
void StatusReporterModule::SendAllConnections(nodeID toNode, u8 messageType)
{
	StatusReporterModuleConnectionsMessage message;
	memset(&message, 0x00, sizeof(StatusReporterModuleConnectionsMessage));

	MeshConnections conn = GS->cm->GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);

	u8* buffer = (u8*)&message;
	for(u32 i=0; i<conn.count; i++){
		memcpy(buffer + i*3, &conn.connections[i]->partnerId, 2);
		i8 avgRssi = conn.connections[i]->GetAverageRSSI();
		memcpy(buffer + i*3 + 2, &avgRssi, 1);
	}

	SendModuleActionMessage(
		MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
		NODE_ID_BROADCAST,
		StatusModuleActionResponseMessages::ALL_CONNECTIONS,
		0,
		(u8*)&message,
		SIZEOF_STATUS_REPORTER_MODULE_CONNECTIONS_MESSAGE,
		false
	);
}

void StatusReporterModule::SendRebootReason(nodeID toNode)
{
	SendModuleActionMessage(
		MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
		toNode,
		StatusModuleActionResponseMessages::REBOOT_REASON,
		0,
		(u8*)GS->ramRetainStructPtr,
		SIZEOF_RAM_RETAIN_STRUCT - sizeof(u32), //crc32 not needed
		false
	);
}
void StatusReporterModule::SendErrors(nodeID toNode){
	StatusReporterModuleErrorLogEntryMessage data;
	for(int i=0; i< Logger::getInstance()->errorLogPosition; i++){
		data.errorType = Logger::getInstance()->errorLog[i].errorType;
		data.extraInfo = Logger::getInstance()->errorLog[i].extraInfo;
		data.errorCode = Logger::getInstance()->errorLog[i].errorCode;
		data.timestamp = Logger::getInstance()->errorLog[i].timestamp;

		SendModuleActionMessage(
			MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
			toNode,
			StatusModuleActionResponseMessages::ERROR_LOG_ENTRY,
			0,
			(u8*)&data,
			SIZEOF_STATUS_REPORTER_MODULE_ERROR_LOG_ENTRY_MESSAGE,
			false
		);
	}

	//Reset the error log
	Logger::getInstance()->errorLogPosition = 0;
}

void StatusReporterModule::StartConnectionRSSIMeasurement(MeshConnection* connection){
	u32 err = 0;

	if (connection->isConnected())
	{
		//Reset old values
		connection->rssiSamplesNum = 0;
		connection->rssiSamplesSum = 0;

		err = sd_ble_gap_rssi_start(connection->connectionHandle, 0, 0);
		if(err == NRF_ERROR_INVALID_STATE || err == BLE_ERROR_INVALID_CONN_HANDLE){
			//Both errors are due to a disconnect and we can simply ignore them
		} else {
			APP_ERROR_CHECK(err); //OK
		}

		logt("STATUSMOD", "RSSI measurement started for connection %u with code %u", connection->connectionId, err);
	}
}

void StatusReporterModule::StopConnectionRSSIMeasurement(MeshConnection* connection){
	u32 err = 0;

	if (connection->isConnected())
	{
		err = sd_ble_gap_rssi_stop(connection->connectionHandle);
		if(err == NRF_ERROR_INVALID_STATE || err == BLE_ERROR_INVALID_CONN_HANDLE){
			//Both errors are due to a disconnect and we can simply ignore them
		} else {
			APP_ERROR_CHECK(err); //OK
		}

		logt("STATUSMOD", "RSSI measurement stopped for connection %u with code %u", connection->connectionId, err);
	}
}


//This handler receives all ble events and can act on them
void StatusReporterModule::BleEventHandler(ble_evt_t* bleEvent){

	//New RSSI measurement for connection received
	if(bleEvent->header.evt_id == BLE_GAP_EVT_RSSI_CHANGED)
	{
		BaseConnection* connection = cm->GetConnectionFromHandle(bleEvent->evt.gap_evt.conn_handle);
		if (connection != NULL) {
			i8 rssi = bleEvent->evt.gap_evt.params.rssi_changed.rssi;

			connection->rssiSamplesNum++;
			connection->rssiSamplesSum += rssi;

			if (connection->rssiSamplesNum > 50) {
				connection->rssiAverage = connection->rssiSamplesSum / connection->rssiSamplesNum;

				connection->rssiSamplesNum = 0;
				connection->rssiSamplesSum = 0;

				//logt("STATUSMOD", "New RSSI average %d", connection->rssiAverage);
			}
		}
	} else if(bleEvent->header.evt_id == BLE_GAP_EVT_ADV_REPORT){

		u8* data = bleEvent->evt.gap_evt.params.adv_report.data;
		u16 dataLength = bleEvent->evt.gap_evt.params.adv_report.dlen;

		advPacketHeader* packetHeader = (advPacketHeader*) data;

		switch (packetHeader->messageType)
		{
			case MESSAGE_TYPE_JOIN_ME_V0:
				if (dataLength == SIZEOF_ADV_PACKET_JOIN_ME)
				{
					advPacketJoinMeV0* packet = (advPacketJoinMeV0*) data;

					bool found = false;

					for(int i=0; i<NUM_NODE_MEASUREMENTS; i++){
						if(nodeMeasurements[i].nodeId == packet->payload.sender){
							if(nodeMeasurements[i].packetCount == UINT16_MAX){
								nodeMeasurements[i].packetCount = 0;
								nodeMeasurements[i].rssiSum = 0;
							}
							nodeMeasurements[i].packetCount++;
							nodeMeasurements[i].rssiSum += bleEvent->evt.gap_evt.params.adv_report.rssi;
							found = true;
							break;
						}
					}
					if(!found){
						for(int i=0; i<NUM_NODE_MEASUREMENTS; i++){
							if(nodeMeasurements[i].nodeId == 0){
								nodeMeasurements[i].nodeId = packet->payload.sender;
								nodeMeasurements[i].packetCount = 1;
								nodeMeasurements[i].rssiSum = bleEvent->evt.gap_evt.params.adv_report.rssi;

								break;
							}
						}
					}

				}
		}

	} else if(bleEvent->header.evt_id == BLE_GATTC_EVT_TIMEOUT){
		SendModuleActionMessage(
				MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
				0,
				StatusModuleActionResponseMessages::DISCONNECT_REASON,
				0,
				NULL,
				0,
				false
			);


	} else if(bleEvent->header.evt_id == BLE_GAP_EVT_DISCONNECTED){

	}
}

bool StatusReporterModule::TerminalCommandHandler(std::string commandName, std::vector<std::string> commandArgs)
{
	if(commandName == "rebootreason")
	{
		SendRebootReason(0);
		return true;
	}
	else if(commandName == "rssistart")
	{
		MeshConnections conn = GS->cm->GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);
		for(u32 i=0; i<conn.count; i++){
			StartConnectionRSSIMeasurement(conn.connections[i]);
		}

		return true;
	}
	else if(commandName == "rssistop")
	{
		MeshConnections conn = GS->cm->GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);
			for(u32 i=0; i<conn.count; i++){
			StopConnectionRSSIMeasurement(conn.connections[i]);
		}

		return true;
	}

	//React on commands, return true if handled, false otherwise
	if(commandArgs.size() >= 2 && commandArgs[1] == moduleName)
	{
		if(commandName == "action")
		{
			//Rewrite "this" to our own node id, this will actually build the packet
			//But reroute it to our own node
			nodeID destinationNode = (commandArgs[0] == "this") ? node->persistentConfig.nodeId : atoi(commandArgs[0].c_str());

			if(commandArgs.size() == 3 && commandArgs[2] == "get_status")
			{
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					StatusModuleTriggerActionMessages::GET_STATUS,
					0,
					NULL,
					0,
					false
				);

				return true;
			}
			else if(commandArgs.size() == 3 && commandArgs[2] == "get_device_info")
			{
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					StatusModuleTriggerActionMessages::GET_DEVICE_INFO,
					0,
					NULL,
					0,
					false
				);

				return true;
			}
			else if(commandArgs.size() == 3 && commandArgs[2] == "get_connections")
			{
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					StatusModuleTriggerActionMessages::GET_ALL_CONNECTIONS,
					0,
					NULL,
					0,
					false
				);

				return true;
			}
			else if(commandArgs.size() == 3 && commandArgs[2] == "get_nearby")
			{
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					StatusModuleTriggerActionMessages::GET_NEARBY_NODES,
					0,
					NULL,
					0,
					false
				);

				return true;
			}
			else if(commandArgs.size() == 3 && commandArgs[2] == "set_init")
			{
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					StatusModuleTriggerActionMessages::SET_INITIALIZED,
					0,
					NULL,
					0,
					false
				);

				return true;
			}
			else if(commandArgs.size() == 3 && commandArgs[2] == "keep_alive")
			{
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					StatusModuleTriggerActionMessages::SET_KEEP_ALIVE,
					0,
					NULL,
					0,
					false
				);

				return true;
			}
			else if(commandArgs.size() == 3 && commandArgs[2] == "get_errors")
			{
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					StatusModuleTriggerActionMessages::GET_ERRORS,
					0,
					NULL,
					0,
					false
				);

				return true;
			}
			else if(commandArgs.size() == 3 && commandArgs[2] == "get_rebootreason")
			{
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					StatusModuleTriggerActionMessages::GET_REBOOT_REASON,
					0,
					NULL,
					0,
					false
				);

				return true;
			}
		}
	}

	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandName, commandArgs);
}

void StatusReporterModule::MeshMessageReceivedHandler(MeshConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader)
{
	//Must call superclass for handling
	Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){

			//We were queried for our status
			if(packet->actionType == StatusModuleTriggerActionMessages::GET_STATUS)
			{
				SendStatus(packet->header.sender, MESSAGE_TYPE_MODULE_ACTION_RESPONSE);

			}//We were queried for our device info
			else if(packet->actionType == StatusModuleTriggerActionMessages::GET_DEVICE_INFO)
			{
				SendDeviceInfo(packet->header.sender, MESSAGE_TYPE_MODULE_ACTION_RESPONSE);

			}
			//We were queried for our connections
			else if(packet->actionType == StatusModuleTriggerActionMessages::GET_ALL_CONNECTIONS)
			{
				StatusReporterModule::SendAllConnections(packetHeader->sender, MESSAGE_TYPE_MODULE_ACTION_RESPONSE);
			}
			//We were queried for nearby nodes (nodes in the join_me buffer)
			else if(packet->actionType == StatusModuleTriggerActionMessages::GET_NEARBY_NODES)
			{
				StatusReporterModule::SendNearbyNodes(packetHeader->sender, MESSAGE_TYPE_MODULE_ACTION_RESPONSE);
			}
			//We should set ourselves initialized
			else if(packet->actionType == StatusModuleTriggerActionMessages::SET_INITIALIZED)
			{
				node->initializedByGateway = true;
				GS->ramRetainStructPtr->rebootReason = 0;
				FruityHal::ClearRebootReason();

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
					packet->header.sender,
					StatusModuleActionResponseMessages::SET_INITIALIZED_RESULT,
					0,
					NULL,
					0,
					false
				);
			}
			else if(packet->actionType == StatusModuleTriggerActionMessages::SET_KEEP_ALIVE)
			{
				FruityHal::FeedWatchdog();
			}
			//Send back the errors
			else if(packet->actionType == StatusModuleTriggerActionMessages::GET_ERRORS)
			{
				SendErrors(packet->header.sender);
			}
			//Send back the reboot reason
			else if(packet->actionType == StatusModuleTriggerActionMessages::GET_REBOOT_REASON)
			{
				SendRebootReason(packet->header.sender);
			}
		}
	}

	//Parse Module responses
	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_ACTION_RESPONSE){

		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId)
		{
			//Somebody reported its connections back
			if(packet->actionType == StatusModuleActionResponseMessages::ALL_CONNECTIONS)
			{
				StatusReporterModuleConnectionsMessage* packetData = (StatusReporterModuleConnectionsMessage*) (packet->data);
				logjson("STATUSMOD", "{\"type\":\"connections\",\"nodeId\":%d,\"module\":%d,\"partners\":[%d,%d,%d,%d],\"rssiValues\":[%d,%d,%d,%d]}" SEP, packet->header.sender, moduleId, packetData->partner1, packetData->partner2, packetData->partner3, packetData->partner4, packetData->rssi1, packetData->rssi2, packetData->rssi3, packetData->rssi4);
			}
			else if(packet->actionType == StatusModuleActionResponseMessages::DEVICE_INFO)
			{
				//Print packet to console
				StatusReporterModuleDeviceInfoMessage* data = (StatusReporterModuleDeviceInfoMessage*) (packet->data);

				u8* addr = data->accessAddress.addr;

				logjson("STATUSMOD", "{\"nodeId\":%u,\"type\":\"device_info\",\"module\":%d,", packet->header.sender, moduleId);
				logjson("STATUSMOD", "\"dBmRX\":%d,\"dBmTX\":%d,\"calibratedTX\":%d,", data->dBmRX, data->dBmTX, data->calibratedTX);
				logjson("STATUSMOD", "\"deviceType\":%u,\"manufacturerId\":%u,", data->deviceType, data->manufacturerId);
				logjson("STATUSMOD", "\"networkId\":%u,\"nodeVersion\":%u,", data->networkId, data->nodeVersion);
				logjson("STATUSMOD", "\"chipId\":\"%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\",", data->chipId[0], data->chipId[1], data->chipId[2], data->chipId[3], data->chipId[4], data->chipId[5], data->chipId[6], data->chipId[7]);
				logjson("STATUSMOD", "\"serialNumber\":\"%.*s\",\"accessAddress\":\"%02X:%02X:%02X:%02X:%02X:%02X\"", NODE_SERIAL_NUMBER_LENGTH, data->serialNumber, addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
				logjson("STATUSMOD", "}" SEP);

			}
			else if(packet->actionType == StatusModuleActionResponseMessages::STATUS)
			{
				//Print packet to console
				StatusReporterModuleStatusMessage* data = (StatusReporterModuleStatusMessage*) (packet->data);

				logjson("STATUSMOD", "{\"nodeId\":%u,\"type\":\"status\",\"module\":%d,", packet->header.sender, moduleId);
				logjson("STATUSMOD", "\"batteryInfo\":%u,\"clusterSize\":%u,", data->batteryInfo, data->clusterSize);
				logjson("STATUSMOD", "\"connectionLossCounter\":%u,\"freeIn\":%u,", data->connectionLossCounter, data->freeIn);
				logjson("STATUSMOD", "\"freeOut\":%u,\"inConnectionPartner\":%u,", data->freeOut, data->inConnectionPartner);
				logjson("STATUSMOD", "\"inConnectionRSSI\":%d, \"initialized\":%u", data->inConnectionRSSI, data->initializedByGateway);
				logjson("STATUSMOD", "}" SEP);
			}
			else if(packet->actionType == StatusModuleActionResponseMessages::NEARBY_NODES)
			{
				//Print packet to console
				logjson("STATUSMOD", "{\"nodeId\":%u,\"type\":\"nearby_nodes\",\"module\":%u,\"nodes\":[", packet->header.sender, moduleId);

				u16 nodeCount = (sendData->dataLength - SIZEOF_CONN_PACKET_MODULE) / 3;
				bool first = true;
				for(int i=0; i<nodeCount; i++){
					u16 nodeId;
					i8 rssi;
					//TODO: Find a nicer way to access unaligned data in packets
					memcpy(&nodeId, packet->data + i*3+0, 2);
					memcpy(&rssi, packet->data + i*3+2, 1);
					if(!first){
						logjson("STATUSMOD", ",");
					}
					logjson("STATUSMOD", "{\"nodeId\":%u,\"rssi\":%d}", nodeId, rssi);
					first = false;
				}

				logjson("STATUSMOD", "]}" SEP);
			}
			else if(packet->actionType == StatusModuleActionResponseMessages::SET_INITIALIZED_RESULT)
			{
				logjson("STATUSMOD", "{\"type\":\"set_init_result\",\"nodeId\":%u,\"module\":%u}" SEP, packet->header.sender, moduleId);
			}
			else if(packet->actionType == StatusModuleActionResponseMessages::ERROR_LOG_ENTRY)
			{
				StatusReporterModuleErrorLogEntryMessage* data = (StatusReporterModuleErrorLogEntryMessage*) (packet->data);

				logjson("STATUSMOD", "{\"type\":\"error_log_entry\",\"nodeId\":%u,\"module\":%u,", packet->header.sender, moduleId);
				logjson("STATUSMOD", "\"errType\":%u,\"code\":%u,\"extra\":%u,\"time\":%u", data->errorType, data->errorCode, data->extraInfo, data->timestamp);
				logjson("STATUSMOD", "}" SEP);
			}
			else if(packet->actionType == StatusModuleActionResponseMessages::REBOOT_REASON)
			{
				RamRetainStruct* data = (RamRetainStruct*) (packet->data);

				logjson("STATUSMOD", "{\"type\":\"reboot_reason\",\"nodeId\":%u,\"module\":%u,", packet->header.sender, moduleId);
				logjson("STATUSMOD", "\"reason\":%u,\"code1\":%u,\"code2\":%u,\"code3\":%u,\"stack\":[", data->rebootReason, data->code1, data->code2, data->code3);
				for(u8 i=0; i<data->stacktraceSize; i++){
					logjson("STATUSMOD", (i < data->stacktraceSize-1) ? "%x," : "%x", data->stacktrace[i]);
				}
				logjson("STATUSMOD", "]}" SEP);
			}
		}
	}
}

void StatusReporterModule::MeshConnectionChangedHandler(MeshConnection* connection)
{
	//New connection has just been made
	if(connection->handshakeDone()){
		//TODO: Implement low and medium rssi sampling with timer handler
		//TODO: disable and enable rssi sampling on existing connections
		if(Config->enableConnectionRSSIMeasurement){
			if(configuration.connectionRSSISamplingMode == RSSISampingModes::RSSI_SAMLING_HIGH){
				StartConnectionRSSIMeasurement(connection);
			}
		}
	}
}

void StatusReporterModule::ButtonHandler(u8 buttonId, u32 holdTime)
{

}

#endif
