/**

Copyright (c) 2014-2015 "M-Way Solutions GmbH"
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
#include <Storage.h>
#include <Node.h>
#include <Config.h>

extern "C"{
#include <app_error.h>
#include <stdlib.h>
}

StatusReporterModule::StatusReporterModule(u16 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot)
	: Module(moduleId, node, cm, name, storageSlot)
{
	//Register callbacks n' stuff
	Logger::getInstance().enableTag("STATUSMOD");

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(StatusReporterModuleConfiguration);

	lastConnectionReportingTimer = 0;
	lastStatusReportingTimer = 0;

	//Start module configuration loading
	LoadModuleConfiguration();
}

void StatusReporterModule::ConfigurationLoadedHandler()
{
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if(configuration.moduleVersion == 1){/* ... */};

	//Do additional initialization upon loading the config


	//Start the Module...

}



void StatusReporterModule::TimerEventHandler(u16 passedTime, u32 appTimer)
{
	//Every reporting interval, the node should send its connections
	if(configuration.connectionReportingIntervalMs != 0 && node->appTimerMs - lastConnectionReportingTimer > configuration.connectionReportingIntervalMs)
	{
		//Send connection info
		SendAllConnections(NODE_ID_BROADCAST, MESSAGE_TYPE_MODULE_GENERAL);

		lastConnectionReportingTimer = node->appTimerMs;
	}

	//Every reporting interval, the node should send its status
	if(configuration.statusReportingIntervalMs != 0 && node->appTimerMs - lastStatusReportingTimer > configuration.statusReportingIntervalMs)
	{
		//Send status
		SendStatus(NODE_ID_BROADCAST, MESSAGE_TYPE_MODULE_GENERAL);

		lastStatusReportingTimer = node->appTimerMs;
	}


}

void StatusReporterModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = false;
	configuration.moduleVersion = 1;

	lastConnectionReportingTimer = 0;
	lastStatusReportingTimer = 0;

	configuration.statusReportingIntervalMs = 0;
	configuration.connectionReportingIntervalMs = 30 * 1000;
	configuration.connectionRSSISamplingMode = RSSISampingModes::RSSI_SAMLING_HIGH;
	configuration.advertisingRSSISamplingMode = RSSISampingModes::RSSI_SAMLING_HIGH;

	//Set additional config values...

}

//This method sends the node's status over the network
void StatusReporterModule::SendStatus(nodeID toNode, u8 messageType)
{
	u16 packetSize = SIZEOF_CONN_PACKET_MODULE + SIZEOF_STATUS_REPORTER_MODULE_STATUS_MESSAGE;
		u8 buffer[packetSize];
		connPacketModule* outPacket = (connPacketModule*)buffer;
		outPacket->header.messageType = messageType;
		outPacket->header.receiver = toNode;
		outPacket->header.sender = node->persistentConfig.nodeId;
		outPacket->moduleId = moduleId;
		outPacket->actionType = StatusModuleActionResponseMessages::STATUS;

		StatusReporterModuleStatusMessage* outPacketData = (StatusReporterModuleStatusMessage*)(outPacket->data);

		outPacketData->batteryInfo = 0; //TODO
		outPacketData->clusterSize = node->clusterSize;
		outPacketData->connectionLossCounter = node->persistentConfig.connectionLossCounter; //TODO: connectionlosscounter is random at the moment
		outPacketData->freeIn = cm->freeInConnections;
		outPacketData->freeOut = cm->freeOutConnections;
		outPacketData->inConnectionPartner = cm->inConnection->partnerId;
		outPacketData->inConnectionRSSI = cm->inConnection->rssiAverage;

		cm->SendMessageToReceiver(NULL, buffer, SIZEOF_CONN_PACKET_MODULE + SIZEOF_STATUS_REPORTER_MODULE_STATUS_MESSAGE, false);
}

//Message type can be either MESSAGE_TYPE_MODULE_ACTION_RESPONSE or MESSAGE_TYPE_MODULE_GENERAL
void StatusReporterModule::SendDeviceInfo(nodeID toNode, u8 messageType)
{
	u16 packetSize = SIZEOF_CONN_PACKET_MODULE + SIZEOF_STATUS_REPORTER_MODULE_DEVICE_INFO_MESSAGE;
	u8 buffer[packetSize];
	connPacketModule* outPacket = (connPacketModule*)buffer;
	outPacket->header.messageType = messageType;
	outPacket->header.receiver = toNode;
	outPacket->header.sender = node->persistentConfig.nodeId;
	outPacket->moduleId = moduleId;
	outPacket->actionType = StatusModuleActionResponseMessages::DEVICE_INFO;

	StatusReporterModuleDeviceInfoMessage* outPacketData = (StatusReporterModuleDeviceInfoMessage*)(outPacket->data);

	outPacketData->manufacturerId = node->persistentConfig.manufacturerId;
	outPacketData->deviceType = node->persistentConfig.deviceType;
	memcpy(outPacketData->chipId, (u8*)NRF_FICR->DEVICEADDR, 8);
	memcpy(outPacketData->serialNumber, node->persistentConfig.serialNumber, SERIAL_NUMBER_LENGTH);
	sd_ble_gap_address_get(&outPacketData->accessAddress);
	outPacketData->nodeVersion = Config->firmwareVersion;
	outPacketData->networkId = node->persistentConfig.networkId;
	outPacketData->dBmRX = node->persistentConfig.dBmRX;
	outPacketData->dBmTX = node->persistentConfig.dBmTX;


	cm->SendMessageToReceiver(NULL, buffer, SIZEOF_CONN_PACKET_MODULE + SIZEOF_STATUS_REPORTER_MODULE_DEVICE_INFO_MESSAGE, false);
}

void StatusReporterModule::SendNearbyNodes(nodeID toNode, u8 messageType)
{
	u16 packetSize = SIZEOF_CONN_PACKET_MODULE + node->joinMePacketBuffer->_numElements * 3;
	u8 buffer[packetSize];
	connPacketModule* outPacket = (connPacketModule*)buffer;
	outPacket->header.messageType = messageType;
	outPacket->header.receiver = toNode;
	outPacket->header.sender = node->persistentConfig.nodeId;
	outPacket->moduleId = moduleId;
	outPacket->actionType = StatusModuleActionResponseMessages::NEARBY_NODES;

	for(int i=0; i<node->joinMePacketBuffer->_numElements; i++)
	{
		joinMeBufferPacket* packet = (joinMeBufferPacket*) node->joinMePacketBuffer->PeekItemAt(i);

		memcpy(outPacket->data + i*3 + 0, &packet->payload.sender, 2);
		memcpy(outPacket->data + i*3 + 2, &packet->rssi, 1);
	}

	cm->SendMessageToReceiver(NULL, buffer, packetSize, false);
}


//This method sends information about the current connections over the network
void StatusReporterModule::SendAllConnections(nodeID toNode, u8 messageType)
{
	//Build response and send
	u16 packetSize = SIZEOF_CONN_PACKET_MODULE + SIZEOF_STATUS_REPORTER_MODULE_CONNECTIONS_MESSAGE;
	u8 buffer[packetSize];
	connPacketModule* outPacket = (connPacketModule*)buffer;

	outPacket->header.messageType = MESSAGE_TYPE_MODULE_ACTION_RESPONSE;
	outPacket->header.receiver = NODE_ID_BROADCAST;
	outPacket->header.sender = node->persistentConfig.nodeId;

	outPacket->moduleId = moduleId;
	outPacket->actionType = StatusModuleActionResponseMessages::ALL_CONNECTIONS;

	StatusReporterModuleConnectionsMessage* outPacketData = (StatusReporterModuleConnectionsMessage*)(outPacket->data);

	outPacketData->partner1 = cm->connections[0]->partnerId;
	outPacketData->partner2 = cm->connections[1]->partnerId;
	outPacketData->partner3 = cm->connections[2]->partnerId;
	outPacketData->partner4 = cm->connections[3]->partnerId;

	outPacketData->rssi1 = cm->connections[0]->GetAverageRSSI();
	outPacketData->rssi2 = cm->connections[1]->GetAverageRSSI();
	outPacketData->rssi3 = cm->connections[2]->GetAverageRSSI();
	outPacketData->rssi4 = cm->connections[3]->GetAverageRSSI();


	cm->SendMessageToReceiver(NULL, buffer, packetSize, true);
}

void StatusReporterModule::StartConnectionRSSIMeasurement(Connection* connection){
	u32 err = 0;

	if (connection->isConnected)
	{
		//Reset old values
		connection->rssiSamplesNum = 0;
		connection->rssiSamplesSum = 0;

		err = sd_ble_gap_rssi_start(connection->connectionHandle, 0, 0);
		APP_ERROR_CHECK(err);

		logt("STATUSMOD", "RSSI measurement started for connection %u with code %u", connection->connectionId, err);
	}
}

void StatusReporterModule::StopConnectionRSSIMeasurement(Connection* connection){
	u32 err = 0;

	if (connection->isConnected)
	{
		err = sd_ble_gap_rssi_stop(connection->connectionHandle);
		APP_ERROR_CHECK(err);

		logt("STATUSMOD", "RSSI measurement stopped for connection %u with code %u", connection->connectionId, err);
	}
}


//This handler receives all ble events and can act on them
void StatusReporterModule::BleEventHandler(ble_evt_t* bleEvent){

	//New RSSI measurement for connection received
	if(bleEvent->header.evt_id == BLE_GAP_EVT_RSSI_CHANGED)
	{
		Connection* connection = cm->GetConnectionFromHandle(bleEvent->evt.gap_evt.conn_handle);
		i8 rssi = bleEvent->evt.gap_evt.params.rssi_changed.rssi;

		connection->rssiSamplesNum++;
		connection->rssiSamplesSum += rssi;

		if(connection->rssiSamplesNum > 50){
			connection->rssiAverage = connection->rssiSamplesSum / connection->rssiSamplesNum;

			connection->rssiSamplesNum = 0;
			connection->rssiSamplesSum = 0;

			//logt("STATUSMOD", "New RSSI average %d", connection->rssiAverage);
		}


	}
}
;

bool StatusReporterModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{

	if(commandName == "rssistart")
	{
		for (int i = 0; i < Config->meshMaxConnections; i++)
		{
			StartConnectionRSSIMeasurement(cm->connections[i]);
		}

		return true;
	}
	else if(commandName == "rssistop")
	{
		for (int i = 0; i < Config->meshMaxConnections; i++)
		{
			StopConnectionRSSIMeasurement(cm->connections[i]);
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
		}
	}

	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandName, commandArgs);
}


void StatusReporterModule::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	//Must call superclass for handling
	Module::ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);

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
				uart("STATUSMOD", "{\"type\":\"connections\",\"nodeId\":%d,\"module\":%d,\"partners\":[%d,%d,%d,%d],\"rssiValues\":[%d,%d,%d,%d]}" SEP, packet->header.sender, moduleId, packetData->partner1, packetData->partner2, packetData->partner3, packetData->partner4, packetData->rssi1, packetData->rssi2, packetData->rssi3, packetData->rssi4);
			}
			else if(packet->actionType == StatusModuleActionResponseMessages::DEVICE_INFO)
			{
				//Print packet to console
				StatusReporterModuleDeviceInfoMessage* data = (StatusReporterModuleDeviceInfoMessage*) (packet->data);

				u8* addr = data->accessAddress.addr;

				uart("STATUSMOD", "{\"nodeId\":%u,\"type\":\"device_info\",\"module\":%d,", packet->header.sender, moduleId);
				uart("STATUSMOD", "\"dBmRX\":%u,\"dBmTX\":%u,", data->dBmRX, data->dBmTX);
				uart("STATUSMOD", "\"deviceType\":%u,\"manufacturerId\":%u,", data->deviceType, data->manufacturerId);
				uart("STATUSMOD", "\"networkId\":%u,\"nodeVersion\":%u,", data->networkId, data->nodeVersion);
				uart("STATUSMOD", "\"chipId\":\"%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\",", data->chipId[0], data->chipId[1], data->chipId[2], data->chipId[3], data->chipId[4], data->chipId[5], data->chipId[6], data->chipId[7]);
				uart("STATUSMOD", "\"serialNumber\":\"%.*s\",\"accessAddress\":\"%02X:%02X:%02X:%02X:%02X:%02X\"", SERIAL_NUMBER_LENGTH, data->serialNumber, addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
				uart("STATUSMOD", "}" SEP);

			}
			else if(packet->actionType == StatusModuleActionResponseMessages::STATUS)
			{
				//Print packet to console
				StatusReporterModuleStatusMessage* data = (StatusReporterModuleStatusMessage*) (packet->data);

				uart("STATUSMOD", "{\"nodeId\":%u,\"type\":\"status\",\"module\":%d,", packet->header.sender, moduleId);
				uart("STATUSMOD", "\"batteryInfo\":%u,\"clusterSize\":%u,", data->batteryInfo, data->clusterSize);
				uart("STATUSMOD", "\"connectionLossCounter\":%u,\"freeIn\":%u,", data->connectionLossCounter, data->freeIn);
				uart("STATUSMOD", "\"freeOut\":%u,\"inConnectionPartner\":%u,", data->freeOut, data->inConnectionPartner);
				uart("STATUSMOD", "\"inConnectionRSSI\":%d", data->inConnectionRSSI);
				uart("STATUSMOD", "}" SEP);
			}
			else if(packet->actionType == StatusModuleActionResponseMessages::NEARBY_NODES)
			{
				//Print packet to console
				uart("STATUSMOD", "{\"nodeId\":%u,\"type\":\"nearby_nodes\",\"module\":%d,\"nodes\":[", packet->header.sender, moduleId);

				u16 nodeCount = (dataLength - SIZEOF_CONN_PACKET_MODULE) / 3;
				bool first = true;
				for(int i=0; i<nodeCount; i++){
					u16 nodeId;
					i8 rssi;
					//TODO: Find a nicer way to access unaligned data in packets
					memcpy(&nodeId, packet->data + i*3+0, 2);
					memcpy(&rssi, packet->data + i*3+2, 1);
					if(!first){
						uart("STATUSMOD", ",");
					}
					uart("STATUSMOD", "{\"nodeId\":%u,\"rssi\":%d}", nodeId, rssi);
					first = false;
				}

				uart("STATUSMOD", "]}" SEP);
			}
		}
	}
}

void StatusReporterModule::MeshConnectionChangedHandler(Connection* connection)
{
	//New connection has just been made
	if(connection->handshakeDone){
		//TODO: Implement low and medium rssi sampling with timer handler
		//TODO: disable and enable rssi sampling on existing connections
		if(Config->enableConnectionRSSIMeasurement){
			if(configuration.connectionRSSISamplingMode == RSSISampingModes::RSSI_SAMLING_HIGH){
				StartConnectionRSSIMeasurement(connection);
			}
		}
	}
}
