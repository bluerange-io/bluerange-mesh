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



#include <StatusReporterModule.h>

#include <Logger.h>
#include <Utility.h>
#include <Node.h>
#include <Config.h>
#include <GlobalState.h>
#include <cstdlib>

constexpr u8 STATUS_REPORTER_MODULE_CONFIG_VERSION = 2;
constexpr u16 STATUS_REPORTER_MODULE_MAX_HOPS = NODE_ID_HOPS_BASE + NODE_ID_HOPS_BASE_SIZE - 1;

StatusReporterModule::StatusReporterModule()
	: Module(ModuleId::STATUS_REPORTER_MODULE, "status")
{
	isADCInitialized = false;
	this->batteryVoltageDv = 0;
	number_of_adc_channels = 0;
	//Register callbacks n' stuff
	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(StatusReporterModuleConfiguration);

	//Set defaults
	ResetToDefaultConfiguration();
}

void StatusReporterModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = true;
	configuration.moduleVersion = STATUS_REPORTER_MODULE_CONFIG_VERSION;
	configuration.statusReportingIntervalDs = 0;
	configuration.connectionReportingIntervalDs = 0;
	configuration.nearbyReportingIntervalDs = 0;
	configuration.deviceInfoReportingIntervalDs = 0;
	configuration.liveReportingState = LiveReportTypes::LEVEL_INFO;

	CheckedMemset(nodeMeasurements, 0x00, sizeof(nodeMeasurements));

	SET_FEATURESET_CONFIGURATION(&configuration, this);
}

void StatusReporterModule::ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength)
{
	//Start the Module...

}

void StatusReporterModule::TimerEventHandler(u16 passedTimeDs)
{
	//Device Info
	if(SHOULD_IV_TRIGGER(GS->appTimerDs+GS->appTimerRandomOffsetDs, passedTimeDs, configuration.deviceInfoReportingIntervalDs)){
		SendDeviceInfoV2(NODE_ID_BROADCAST, 0, MessageType::MODULE_ACTION_RESPONSE);
	}
	//Status
	if(SHOULD_IV_TRIGGER(GS->appTimerDs+GS->appTimerRandomOffsetDs, passedTimeDs, configuration.statusReportingIntervalDs)){
		SendStatus(NODE_ID_BROADCAST, 0, MessageType::MODULE_ACTION_RESPONSE);
	}
	//Connections
	if(SHOULD_IV_TRIGGER(GS->appTimerDs+GS->appTimerRandomOffsetDs, passedTimeDs, configuration.connectionReportingIntervalDs)){
		SendAllConnections(NODE_ID_BROADCAST, 0, MessageType::MODULE_GENERAL);
	}
	//Nearby Nodes
	if(SHOULD_IV_TRIGGER(GS->appTimerDs+GS->appTimerRandomOffsetDs, passedTimeDs, configuration.nearbyReportingIntervalDs)){
		SendNearbyNodes(NODE_ID_BROADCAST, 0, MessageType::MODULE_ACTION_RESPONSE);
	}
	//BatteryMeasurement (measure short after reset and then priodically)
	if( (GS->appTimerDs < SEC_TO_DS(40) && Boardconfig->batteryAdcInputPin != -1 )
		|| SHOULD_IV_TRIGGER(GS->appTimerDs, passedTimeDs, batteryMeasurementIntervalDs)){
		BatteryVoltageADC();
	}
//	//ErrorLog
//	if(SHOULD_IV_TRIGGER(node->appTimerDs+node->appTimerRandomOffsetDs, passedTimeDs, SEC_TO_DS(4))){
//		SendErrors(0);
//	}
}

//This method sends the node's status over the network
void StatusReporterModule::SendStatus(NodeId toNode, u8 requestHandle, MessageType messageType) const
{
	MeshConnections conn = GS->cm.GetMeshConnections(ConnectionDirection::DIRECTION_IN);
	MeshConnection* inConnection = nullptr;

	for (u32 i = 0; i < conn.count; i++) {
		if (conn.connections[i]->handshakeDone()) {
			inConnection = conn.connections[i];
		}
	}

	StatusReporterModuleStatusMessage data;

	data.batteryInfo = GetBatteryVoltage();
	data.clusterSize = GS->node.clusterSize;
	data.connectionLossCounter = (u8) GS->node.connectionLossCounter; //TODO: connectionlosscounter is random at the moment, and the u8 will wrap
	data.freeIn = GS->cm.freeMeshInConnections;
	data.freeOut = GS->cm.freeMeshOutConnections;
	data.inConnectionPartner = inConnection == nullptr ? 0 : inConnection->partnerId;
	data.inConnectionRSSI = inConnection == nullptr ? 0 : inConnection->GetAverageRSSI();
	data.initializedByGateway = GS->node.initializedByGateway;

	SendModuleActionMessage(
		messageType,
		toNode,
		(u8)StatusModuleActionResponseMessages::STATUS,
		requestHandle,
		(u8*)&data,
		SIZEOF_STATUS_REPORTER_MODULE_STATUS_MESSAGE,
		false
	);
}

//Message type can be either MESSAGE_TYPE_MODULE_ACTION_RESPONSE or MESSAGE_TYPE_MODULE_GENERAL
void StatusReporterModule::SendDeviceInfoV2(NodeId toNode, u8 requestHandle, MessageType messageType) const
{
	StatusReporterModuleDeviceInfoV2Message data;

	data.manufacturerId = RamConfig->manufacturerId;
	data.deviceType = GET_DEVICE_TYPE();
	FruityHal::GetDeviceAddress(data.chipId);
	for (u32 i = 0; i < sizeof(data.chipId); i++)
	{
		// Not CheckedMemcpy, as DEVICEADDR is volatile.
		data.chipId[i] = ((const volatile u8*)NRF_FICR->DEVICEADDR)[i];
	}
	data.serialNumberIndex = RamConfig->GetSerialNumberIndex();
	FruityHal::BleGapAddressGet(&data.accessAddress);
	data.nodeVersion = GS->config.getFruityMeshVersion();
	data.networkId = GS->node.configuration.networkId;
	data.dBmRX = Boardconfig->dBmRX;
	data.dBmTX = Conf::defaultDBmTX;
	data.calibratedTX = Boardconfig->calibratedTX;
	data.chipGroupId = GS->config.fwGroupIds[0];
	data.featuresetGroupId = GS->config.fwGroupIds[1];
	data.bootloaderVersion = (u16)FruityHal::GetBootloaderVersion();

	SendModuleActionMessage(
		messageType,
		toNode,
		(u8)StatusModuleActionResponseMessages::DEVICE_INFO_V2,
		requestHandle,
		(u8*)&data,
		SIZEOF_STATUS_REPORTER_MODULE_DEVICE_INFO_V2_MESSAGE,
		false
	);
}

void StatusReporterModule::SendNearbyNodes(NodeId toNode, u8 requestHandle, MessageType messageType)
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
			NodeId sender = nodeMeasurements[i].nodeId;
			i8 rssi = (i8)(nodeMeasurements[i].rssiSum / nodeMeasurements[i].packetCount);

			CheckedMemcpy(buffer + j*3 + 0, &sender, 2);
			CheckedMemcpy(buffer + j*3 + 2, &rssi, 1);

			j++;
		}
	}

	//Clear node measurements
	CheckedMemset(nodeMeasurements, 0x00, sizeof(nodeMeasurements));

	SendModuleActionMessage(
		messageType,
		toNode,
		(u8)StatusModuleActionResponseMessages::NEARBY_NODES,
		requestHandle,
		buffer,
		packetSize,
		false
	);
}


//This method sends information about the current connections over the network
void StatusReporterModule::SendAllConnections(NodeId toNode, u8 requestHandle, MessageType messageType) const
{
	StatusReporterModuleConnectionsMessage message;
	CheckedMemset(&message, 0x00, sizeof(StatusReporterModuleConnectionsMessage));

	MeshConnections connIn = GS->cm.GetMeshConnections(ConnectionDirection::DIRECTION_IN);
	MeshConnections connOut = GS->cm.GetMeshConnections(ConnectionDirection::DIRECTION_OUT);

	u8* buffer = (u8*)&message;

	if(connIn.count > 0){
		CheckedMemcpy(buffer, &connIn.connections[0]->partnerId, 2);
		i8 avgRssi = connIn.connections[0]->GetAverageRSSI();
		CheckedMemcpy(buffer + 2, &avgRssi, 1);
	}

	for(u32 i=0; i<connOut.count; i++){
		CheckedMemcpy(buffer + (i+1)*3, &connOut.connections[i]->partnerId, 2);
		i8 avgRssi = connOut.connections[i]->GetAverageRSSI();
		CheckedMemcpy(buffer + (i+1)*3 + 2, &avgRssi, 1);
	}

	SendModuleActionMessage(
		MessageType::MODULE_ACTION_RESPONSE,
		NODE_ID_BROADCAST,
		(u8)StatusModuleActionResponseMessages::ALL_CONNECTIONS,
		requestHandle,
		(u8*)&message,
		SIZEOF_STATUS_REPORTER_MODULE_CONNECTIONS_MESSAGE,
		false
	);
}

void StatusReporterModule::SendRebootReason(NodeId toNode, u8 requestHandle) const
{
	SendModuleActionMessage(
		MessageType::MODULE_ACTION_RESPONSE,
		toNode,
		(u8)StatusModuleActionResponseMessages::REBOOT_REASON,
		requestHandle,
		(u8*)&(GS->ramRetainStructPreviousBoot),
		sizeof(RamRetainStruct) - sizeof(u32), //crc32 not needed
		false
	);
}
void StatusReporterModule::SendErrors(NodeId toNode, u8 requestHandle) const{

	//Log another error so that we know the uptime of the node when the errors were requested
	GS->logger.logCustomError(CustomErrorTypes::INFO_ERRORS_REQUESTED, GS->logger.errorLogPosition);

	StatusReporterModuleErrorLogEntryMessage data;
	for(int i=0; i< GS->logger.errorLogPosition; i++){
		data.errorType = (u8)GS->logger.errorLog[i].errorType;
		data.extraInfo = GS->logger.errorLog[i].extraInfo;
		data.errorCode = GS->logger.errorLog[i].errorCode;
		data.timestamp = GS->logger.errorLog[i].timestamp;

		SendModuleActionMessage(
			MessageType::MODULE_ACTION_RESPONSE,
			toNode,
			(u8)StatusModuleActionResponseMessages::ERROR_LOG_ENTRY,
			requestHandle,
			(u8*)&data,
			SIZEOF_STATUS_REPORTER_MODULE_ERROR_LOG_ENTRY_MESSAGE,
			false
		);
	}

	//Reset the error log
	GS->logger.errorLogPosition = 0;
}


void StatusReporterModule::SendLiveReport(LiveReportTypes type, u16 requestHandle, u32 extra, u32 extra2) const
{
	//Live reporting states are off=0, error=50, warn=100, info=150, debug=200
	if (type > configuration.liveReportingState) return;

	StatusReporterModuleLiveReportMessage data;

	data.reportType = (u8)type;
	data.extra = extra;
	data.extra2 = extra2;

	SendModuleActionMessage(
		MessageType::MODULE_GENERAL,
		NODE_ID_BROADCAST, //TODO: Could use gateway
		(u8)StatusModuleGeneralMessages::LIVE_REPORT,
		requestHandle,
		(u8*)&data,
		SIZEOF_STATUS_REPORTER_MODULE_LIVE_REPORT_MESSAGE,
		false
	);
}

void StatusReporterModule::StartConnectionRSSIMeasurement(MeshConnection& connection) const{
	u32 err = 0;

	if (connection.isConnected())
	{
		//Reset old values
		connection.lastReportedRssi = 0;
		connection.rssiAverageTimes1000 = 0;

		//Both possible errors are due to a disconnect and we can simply ignore them
		FruityHal::BleGapRssiStart(connection.connectionHandle, 2, 7);
		
		logt("STATUSMOD", "RSSI measurement started for connection %u with code %u", connection.connectionId, err);
	}
}

void StatusReporterModule::StopConnectionRSSIMeasurement(const MeshConnection& connection) const{
	u32 err = 0;

	if (connection.isConnected())
	{
		//Both possible errors are due to a disconnect and we can simply ignore them
		FruityHal::BleGapRssiStop(connection.connectionHandle);

		logt("STATUSMOD", "RSSI measurement stopped for connection %u with code %u", connection.connectionId, err);
	}
}


void StatusReporterModule::GapAdvertisementReportEventHandler(const FruityHal::GapAdvertisementReportEvent & advertisementReportEvent)
{
	const u8* data = advertisementReportEvent.getData();
	u16 dataLength = advertisementReportEvent.getDataLength();

	const advPacketHeader* packetHeader = (const advPacketHeader*)data;

	switch (packetHeader->messageType)
	{
	case MESSAGE_TYPE_JOIN_ME_V0:
		if (dataLength == SIZEOF_ADV_PACKET_JOIN_ME)
		{
			const advPacketJoinMeV0* packet = (const advPacketJoinMeV0*)data;

			bool found = false;

			for (int i = 0; i < NUM_NODE_MEASUREMENTS; i++) {
				if (nodeMeasurements[i].nodeId == packet->payload.sender) {
					if (nodeMeasurements[i].packetCount == UINT16_MAX) {
						nodeMeasurements[i].packetCount = 0;
						nodeMeasurements[i].rssiSum = 0;
					}
					nodeMeasurements[i].packetCount++;
					nodeMeasurements[i].rssiSum += advertisementReportEvent.getRssi();
					found = true;
					break;
				}
			}
			if (!found) {
				for (int i = 0; i < NUM_NODE_MEASUREMENTS; i++) {
					if (nodeMeasurements[i].nodeId == 0) {
						nodeMeasurements[i].nodeId = packet->payload.sender;
						nodeMeasurements[i].packetCount = 1;
						nodeMeasurements[i].rssiSum = advertisementReportEvent.getRssi();

						break;
					}
				}
			}

		}
	}
}

#ifdef TERMINAL_ENABLED
TerminalCommandHandlerReturnType StatusReporterModule::TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize)
{
	//React on commands, return true if handled, false otherwise
	if(commandArgsSize >= 4 && TERMARGS(2, moduleName))
	{
		if(TERMARGS(0, "action"))
		{
			NodeId destinationNode = Utility::TerminalArgumentToNodeId(commandArgs[1]);

			if(TERMARGS(3, "get_status"))
			{
				SendModuleActionMessage(
					MessageType::MODULE_TRIGGER_ACTION,
					destinationNode,
					(u8)StatusModuleTriggerActionMessages::GET_STATUS,
					0,
					nullptr,
					0,
					false
				);

				return TerminalCommandHandlerReturnType::SUCCESS;
			}
			else if(TERMARGS(3,"get_device_info"))
			{
				SendModuleActionMessage(
					MessageType::MODULE_TRIGGER_ACTION,
					destinationNode,
					(u8)StatusModuleTriggerActionMessages::GET_DEVICE_INFO_V2,
					0,
					nullptr,
					0,
					false
				);

				return TerminalCommandHandlerReturnType::SUCCESS;
			}
			else if(TERMARGS(3,"get_connections"))
			{
				SendModuleActionMessage(
					MessageType::MODULE_TRIGGER_ACTION,
					destinationNode,
					(u8)StatusModuleTriggerActionMessages::GET_ALL_CONNECTIONS,
					0,
					nullptr,
					0,
					false
				);

				return TerminalCommandHandlerReturnType::SUCCESS;
			}
			else if(TERMARGS(3, "get_nearby"))
			{
				SendModuleActionMessage(
					MessageType::MODULE_TRIGGER_ACTION,
					destinationNode,
					(u8)StatusModuleTriggerActionMessages::GET_NEARBY_NODES,
					0,
					nullptr,
					0,
					false
				);

				return TerminalCommandHandlerReturnType::SUCCESS;
			}
			else if(TERMARGS(3,"set_init"))
			{
				SendModuleActionMessage(
					MessageType::MODULE_TRIGGER_ACTION,
					destinationNode,
					(u8)StatusModuleTriggerActionMessages::SET_INITIALIZED,
					0,
					nullptr,
					0,
					false
				);

				return TerminalCommandHandlerReturnType::SUCCESS;
			}
			else if(TERMARGS(3, "keep_alive"))
			{
				// Sink routing self checking mechanism requires keep_alive message to be hops based.
				destinationNode = STATUS_REPORTER_MODULE_MAX_HOPS;
				SendModuleActionMessage(
					MessageType::MODULE_TRIGGER_ACTION,
					destinationNode,
					(u8)StatusModuleTriggerActionMessages::SET_KEEP_ALIVE,
					0,
					nullptr,
					0,
					false
				);

				return TerminalCommandHandlerReturnType::SUCCESS;
			}
			else if(TERMARGS(3, "get_errors"))
			{
				SendModuleActionMessage(
					MessageType::MODULE_TRIGGER_ACTION,
					destinationNode,
					(u8)StatusModuleTriggerActionMessages::GET_ERRORS,
					0,
					nullptr,
					0,
					false
				);

				return TerminalCommandHandlerReturnType::SUCCESS;
			}
			else if(TERMARGS(3 ,"livereports")){
					if (commandArgsSize < 5) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
					//Enables or disables live reporting of connection establishments
					u8 liveReportingState = Utility::StringToU8(commandArgs[4]);

					SendModuleActionMessage(
						MessageType::MODULE_TRIGGER_ACTION,
						destinationNode,
						(u8)StatusModuleTriggerActionMessages::SET_LIVEREPORTING,
						0,
						&liveReportingState,
						1,
						false
					);

					return TerminalCommandHandlerReturnType::SUCCESS;
				}
			else if(TERMARGS(3, "get_rebootreason"))
			{
				SendModuleActionMessage(
					MessageType::MODULE_TRIGGER_ACTION,
					destinationNode,
					(u8)StatusModuleTriggerActionMessages::GET_REBOOT_REASON,
					0,
					nullptr,
					0,
					false
				);

				return TerminalCommandHandlerReturnType::SUCCESS;
			}
		}
	}

	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif

void StatusReporterModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader)
{
	//Must call superclass for handling
	Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

	if(packetHeader->messageType == MessageType::MODULE_TRIGGER_ACTION){
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){

			//We were queried for our status
			StatusModuleTriggerActionMessages actionType = (StatusModuleTriggerActionMessages)packet->actionType;
			if(actionType == StatusModuleTriggerActionMessages::GET_STATUS)
			{
				SendStatus(packet->header.sender, packet->requestHandle, MessageType::MODULE_ACTION_RESPONSE);

			}
			//We were queried for our device info v2
			else if(actionType == StatusModuleTriggerActionMessages::GET_DEVICE_INFO_V2)
			{
				SendDeviceInfoV2(packet->header.sender, packet->requestHandle, MessageType::MODULE_ACTION_RESPONSE);

			}
			//We were queried for our connections
			else if(actionType == StatusModuleTriggerActionMessages::GET_ALL_CONNECTIONS)
			{
				StatusReporterModule::SendAllConnections(packetHeader->sender, packet->requestHandle, MessageType::MODULE_ACTION_RESPONSE);
			}
			//We were queried for nearby nodes (nodes in the join_me buffer)
			else if(actionType == StatusModuleTriggerActionMessages::GET_NEARBY_NODES)
			{
				StatusReporterModule::SendNearbyNodes(packetHeader->sender, packet->requestHandle, MessageType::MODULE_ACTION_RESPONSE);
			}
			//We should set ourselves initialized
			else if(actionType == StatusModuleTriggerActionMessages::SET_INITIALIZED)
			{
				GS->node.initializedByGateway = true;

				SendModuleActionMessage(
					MessageType::MODULE_ACTION_RESPONSE,
					packet->header.sender,
					(u8)StatusModuleActionResponseMessages::SET_INITIALIZED_RESULT,
					packet->requestHandle,
					nullptr,
					0,
					false
				);
			}
			else if(actionType == StatusModuleTriggerActionMessages::SET_KEEP_ALIVE)
			{
				FruityHal::FeedWatchdog();
				if (connection != nullptr)
				{
					u16 receivedHopsToSink = STATUS_REPORTER_MODULE_MAX_HOPS - packetHeader->receiver;
					MeshConnection * meshconn = GS->cm.GetMeshConnectionToPartner(connection->partnerId);
					if (meshconn != nullptr)
					{
						//meshconn can be nullptr if the connection for example is a mesh access connection.
						u16 hopsToSink = meshconn->getHopsToSink();

						if (receivedHopsToSink != hopsToSink)
						{
							GS->logger.logCustomError(CustomErrorTypes::FATAL_INCORRECT_HOPS_TO_SINK, (receivedHopsToSink << 16) | hopsToSink);
							logt("DEBUGMOD", "FATAL receivedHopsToSink: %d", receivedHopsToSink);
							logt("DEBUGMOD", "FATAL getHopsToSink: %d", hopsToSink);
							meshconn->setHopsToSink(receivedHopsToSink);
						}
					}
				}
			}
			//Send back the errors
			else if(actionType == StatusModuleTriggerActionMessages::GET_ERRORS)
			{
				SendErrors(packet->header.sender, packet->requestHandle);
			}
			//Configures livereporting
			else if(actionType == StatusModuleTriggerActionMessages::SET_LIVEREPORTING)
			{
				configuration.liveReportingState = (LiveReportTypes)packet->data[0];
				logt("DEBUGMOD", "LiveReporting is now %u", (u32)configuration.liveReportingState);
			}
			//Send back the reboot reason
			else if(actionType == StatusModuleTriggerActionMessages::GET_REBOOT_REASON)
			{
				SendRebootReason(packet->header.sender, packet->requestHandle);
			}
		}
	}

	//Parse Module responses
	if(packetHeader->messageType == MessageType::MODULE_ACTION_RESPONSE){

		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId)
		{
			StatusModuleActionResponseMessages actionType = (StatusModuleActionResponseMessages)packet->actionType;
			//Somebody reported its connections back
			if(actionType == StatusModuleActionResponseMessages::ALL_CONNECTIONS)
			{
				StatusReporterModuleConnectionsMessage* packetData = (StatusReporterModuleConnectionsMessage*) (packet->data);
				logjson("STATUSMOD", "{\"type\":\"connections\",\"nodeId\":%d,\"module\":%d,\"partners\":[%d,%d,%d,%d],\"rssiValues\":[%d,%d,%d,%d]}" SEP, packet->header.sender, (u32)moduleId, packetData->partner1, packetData->partner2, packetData->partner3, packetData->partner4, packetData->rssi1, packetData->rssi2, packetData->rssi3, packetData->rssi4);
			}
			else if(actionType == StatusModuleActionResponseMessages::DEVICE_INFO_V2)
			{
				//Print packet to console
				StatusReporterModuleDeviceInfoV2Message* data = (StatusReporterModuleDeviceInfoV2Message*) (packet->data);

				u8* addr = data->accessAddress.addr;

				char serialBuffer[NODE_SERIAL_NUMBER_LENGTH + 1];
				Utility::GenerateBeaconSerialForIndex(data->serialNumberIndex, serialBuffer);

				logjson("STATUSMOD", "{\"nodeId\":%u,\"type\":\"device_info\",\"module\":%d,", packet->header.sender, (u32)moduleId);
				logjson("STATUSMOD", "\"dBmRX\":%d,\"dBmTX\":%d,\"calibratedTX\":%d,", data->dBmRX, data->dBmTX, data->calibratedTX);
				logjson("STATUSMOD", "\"deviceType\":%u,\"manufacturerId\":%u,", (u32)data->deviceType, data->manufacturerId);
				logjson("STATUSMOD", "\"networkId\":%u,\"nodeVersion\":%u,", data->networkId, data->nodeVersion);
				logjson("STATUSMOD", "\"chipId\":\"%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\",", data->chipId[0], data->chipId[1], data->chipId[2], data->chipId[3], data->chipId[4], data->chipId[5], data->chipId[6], data->chipId[7]);
				logjson("STATUSMOD", "\"serialNumber\":\"%s\",\"accessAddress\":\"%02X:%02X:%02X:%02X:%02X:%02X\",", serialBuffer, addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
				logjson("STATUSMOD", "\"groupIds\":[%u,%u],\"blVersion\":%u", data->chipGroupId, data->featuresetGroupId, data->bootloaderVersion);
				logjson("STATUSMOD", "}" SEP);

			}
			else if(actionType == StatusModuleActionResponseMessages::STATUS)
			{
				//Print packet to console
				StatusReporterModuleStatusMessage* data = (StatusReporterModuleStatusMessage*) (packet->data);

				logjson("STATUSMOD", "{\"nodeId\":%u,\"type\":\"status\",\"module\":%d,", packet->header.sender, (u32)moduleId);
				logjson("STATUSMOD", "\"batteryInfo\":%u,\"clusterSize\":%u,", data->batteryInfo, data->clusterSize);
				logjson("STATUSMOD", "\"connectionLossCounter\":%u,\"freeIn\":%u,", data->connectionLossCounter, data->freeIn);
				logjson("STATUSMOD", "\"freeOut\":%u,\"inConnectionPartner\":%u,", data->freeOut, data->inConnectionPartner);
				logjson("STATUSMOD", "\"inConnectionRSSI\":%d, \"initialized\":%u", data->inConnectionRSSI, data->initializedByGateway);
				logjson("STATUSMOD", "}" SEP);
			}
			else if(actionType == StatusModuleActionResponseMessages::NEARBY_NODES)
			{
				//Print packet to console
				logjson("STATUSMOD", "{\"nodeId\":%u,\"type\":\"nearby_nodes\",\"module\":%u,\"nodes\":[", packet->header.sender, (u32)moduleId);

				u16 nodeCount = (sendData->dataLength - SIZEOF_CONN_PACKET_MODULE) / 3;
				bool first = true;
				for(int i=0; i<nodeCount; i++){
					u16 nodeId;
					i8 rssi;
					//TODO: Find a nicer way to access unaligned data in packets
					CheckedMemcpy(&nodeId, packet->data + i*3+0, 2);
					CheckedMemcpy(&rssi, packet->data + i*3+2, 1);
					if(!first){
						logjson("STATUSMOD", ",");
					}
					logjson("STATUSMOD", "{\"nodeId\":%u,\"rssi\":%d}", nodeId, rssi);
					first = false;
				}

				logjson("STATUSMOD", "]}" SEP);
			}
			else if(actionType == StatusModuleActionResponseMessages::SET_INITIALIZED_RESULT)
			{
				logjson("STATUSMOD", "{\"type\":\"set_init_result\",\"nodeId\":%u,\"module\":%u}" SEP, packet->header.sender, (u32)moduleId);
			}
			else if(actionType == StatusModuleActionResponseMessages::ERROR_LOG_ENTRY)
			{
				StatusReporterModuleErrorLogEntryMessage* data = (StatusReporterModuleErrorLogEntryMessage*) (packet->data);

				logjson("STATUSMOD", "{\"type\":\"error_log_entry\",\"nodeId\":%u,\"module\":%u,", packet->header.sender, (u32)moduleId);

				//As the time is currently only 3 byte, use this formula to get the current unix timestamp in UTC: now()  - (now() % (2^24)) + timestamp
				logjson("STATUSMOD", "\"errType\":%u,\"code\":%u,\"extra\":%u,\"time\":%u", (u32)data->errorType, data->errorCode, data->extraInfo, data->timestamp);
#if IS_INACTIVE(GW_SAVE_SPACE)
				logjson("STATUSMOD", ",\"typeStr\":\"%s\",\"codeStr\":\"%s\"", Logger::getErrorLogErrorType((LoggingError)data->errorType), Logger::getErrorLogError((LoggingError)data->errorType, data->errorCode));
#endif
				logjson("STATUSMOD", "}" SEP);
			}
			else if(actionType == StatusModuleActionResponseMessages::REBOOT_REASON)
			{
				RamRetainStruct* data = (RamRetainStruct*) (packet->data);

				logjson("STATUSMOD", "{\"type\":\"reboot_reason\",\"nodeId\":%u,\"module\":%u,", packet->header.sender, (u32)moduleId);
				logjson("STATUSMOD", "\"reason\":%u,\"code1\":%u,\"code2\":%u,\"code3\":%u,\"stack\":[", (u32)data->rebootReason, data->code1, data->code2, data->code3);
				for(u8 i=0; i<data->stacktraceSize; i++){
					logjson("STATUSMOD", (i < data->stacktraceSize-1) ? "%x," : "%x", data->stacktrace[i]);
				}
				logjson("STATUSMOD", "]}" SEP);
			}
		}
	}

	//Parse Module general messages
	if(packetHeader->messageType == MessageType::MODULE_GENERAL){

		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId)
		{
			StatusModuleGeneralMessages actionType = (StatusModuleGeneralMessages)packet->actionType;
			//Somebody reported its connections back
			if(actionType == StatusModuleGeneralMessages::LIVE_REPORT)
			{
				StatusReporterModuleLiveReportMessage* packetData = (StatusReporterModuleLiveReportMessage*) (packet->data);
				logjson("STATUSMOD", "{\"type\":\"live_report\",\"nodeId\":%d,\"module\":%d,\"code\":%u,\"extra\":%u,\"extra2\":%u}" SEP, packet->header.sender, (u32)moduleId, packetData->reportType, packetData->extra, packetData->extra2);
			}
		}
	}
}

void StatusReporterModule::MeshConnectionChangedHandler(MeshConnection& connection)
{
	//New connection has just been made
	if(connection.handshakeDone()){
		//TODO: Implement low and medium rssi sampling with timer handler
		//TODO: disable and enable rssi sampling on existing connections
		if(Conf::enableConnectionRSSIMeasurement){
			if(connectionRSSISamplingMode == connectionRSSISamplingMode){
				StartConnectionRSSIMeasurement(connection);
			}
		}
	}
}

#define _____________BATTERY_MEASUREMENT_________________

void StatusReporterModule::AdcEventHandler()
{
	StatusReporterModule * p_statusReporterModule = (StatusReporterModule *)GS->node.GetModuleById(ModuleId::STATUS_REPORTER_MODULE);
	if (p_statusReporterModule == nullptr) return;
	p_statusReporterModule->convertADCtoVoltage();
	FruityHal::AdcUninit();
}

void StatusReporterModule::initBatteryVoltageADC() {
#if IS_ACTIVE(BATTERY_MEASUREMENT)
	//Do not initialize battery checking if board does not support it
	if(Boardconfig->batteryAdcInputPin == -1 || isADCInitialized){
		return;
	}
	ErrorType error = FruityHal::AdcInit(AdcEventHandler);
	APP_ERROR_CHECK((u32)error);

	u32 pin = Boardconfig->batteryAdcInputPin;
	if(Boardconfig->batteryAdcInputPin == -2) 
	{
		// Battery input
		pin = 0xFF;
	}
#if FEATURE_AVAILABLE(ADC_INTERNAL_MEASUREMENT)
	if(Boardconfig->batteryAdcInputPin == -2) {
		FruityHal::AdcConfigureChannel(pin, 
		                               FruityHal::AdcReference::ADC_REFERENCE_0_6V, 
		                               FruityHal::AdcResoultion::ADC_10_BIT, 
		                               FruityHal::AdcGain::ADC_GAIN_1_6);
	}
	else
	{
		FruityHal::AdcConfigureChannel(pin, 
		                               FruityHal::AdcReference::ADC_REFERENCE_1_4_POWER_SUPPLY, 
		                               FruityHal::AdcResoultion::ADC_10_BIT, 
		                               FruityHal::AdcGain::ADC_GAIN_1_5);
	}
#else
	FruityHal::AdcConfigureChannel(pin, 
	                               FruityHal::AdcReference::ADC_REFERENCE_1_2V, 
	                               FruityHal::AdcResoultion::ADC_8_BIT, 
	                               FruityHal::AdcGain::ADC_GAIN_1);
#endif // FEATURE_AVAILABLE(ADC_INTERNAL_MEASUREMENT)
	isADCInitialized = true;
#endif
}
void StatusReporterModule::BatteryVoltageADC(){
#if IS_ACTIVE(BATTERY_MEASUREMENT)
	initBatteryVoltageADC();
	//Check if initialization did work
	if(!isADCInitialized || Boardconfig->batteryAdcInputPin == -1) return;

#ifndef SIM_ENABLED
	if (Boardconfig->batteryAdcInputPin >= 0) {
		FruityHal::GpioConfigureOutput(Boardconfig->batteryMeasurementEnablePin);
		FruityHal::GpioPinSet(Boardconfig->batteryMeasurementEnablePin);
	}
	
	ErrorType err = FruityHal::AdcSample(*m_buffer, 1);
	APP_ERROR_CHECK((u32)err);

	isADCInitialized = false;
#endif
#endif
}

void StatusReporterModule::convertADCtoVoltage()
{
#if IS_ACTIVE(BATTERY_MEASUREMENT)
	u32 adc_sum_value = 0;
	for (u16 i = 0; i < BATTERY_SAMPLES_IN_BUFFER; i++) {
		//Buffer implemented for future use
		adc_sum_value += m_buffer[i];               //Sum all values in ADC buffer
	}

#if FEATURE_AVAILABLE(ADC_INTERNAL_MEASUREMENT)
	if (Boardconfig->batteryAdcInputPin >= 0 && Boardconfig->voltageDividerR1,Boardconfig->voltageDividerR2 > 0){
		u16 voltageDividerDv = ExternalVoltageDividerDv(Boardconfig->voltageDividerR1, Boardconfig->voltageDividerR2);
		batteryVoltageDv = FruityHal::AdcConvertSampleToDeciVoltage(adc_sum_value / BATTERY_SAMPLES_IN_BUFFER, voltageDividerDv);
	}
	else {
		batteryVoltageDv = FruityHal::AdcConvertSampleToDeciVoltage(adc_sum_value / BATTERY_SAMPLES_IN_BUFFER); //Transform the average ADC value into decivolts value
	}
#else
	batteryVoltageDv = FruityHal::AdcConvertSampleToDeciVoltage(adc_sum_value / BATTERY_SAMPLES_IN_BUFFER);
#endif // FEATURE_AVAILABLE(ADC_INTERNAL_MEASUREMENT)

#endif
}

u8 StatusReporterModule::GetBatteryVoltage() const
{
	return batteryVoltageDv;
}

u16 StatusReporterModule::ExternalVoltageDividerDv(u32 Resistor1, u32 Resistor2)
{
	u16 voltageDividerDv = u16(((double(Resistor1 + Resistor2)) / double(Resistor2)) * 10);
	return voltageDividerDv;
}
