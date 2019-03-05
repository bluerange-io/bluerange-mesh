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

#include <Config.h>

#include <Node.h>
#include <LedWrapper.h>
#include <AdvertisingController.h>
#include <GAPController.h>
#include <GlobalState.h>
#include <GATTController.h>
#include <ConnectionManager.h>
#include <ScanController.h>
#include <Utility.h>
#include <Logger.h>
#include <StatusReporterModule.h>
#include <AdvertisingModule.h>
#include <DebugModule.h>
#include <ScanningModule.h>
#include <EnrollmentModule.h>
#include <MeshAccessModule.h>
#include <IoModule.h>
#include <MeshConnection.h>

#ifdef ACTIVATE_DFU_MODULE
#include <DfuModule.h>
#endif

#ifdef ACTIVATE_CLC_MODULE
#include <ClcModule.h>
#endif
#ifdef ACTIVATE_VS_MODULE
#include <VsModule.h>
#endif

#ifdef ACTIVATE_ENOCEAN_MODULE
#include <EnOceanModule.h>
#endif

#ifdef ACTIVATE_ASSET_MODULE
#include <AssetModule.h>
#endif

#ifdef ACTIVATE_EINK_MODULE
#include <EinkModule.h>
#endif

#ifdef ACTIVATE_MANAGEMENT_MODULE
#include <ManagementModule.h>
#endif

extern "C"
{
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <nrf_soc.h>
#include <app_error.h>
#include <app_timer.h>
#include <ble_hci.h>
#ifndef SIM_ENABLED
#include <nrf_nvic.h>
#endif
}

#define NODE_MODULE_CONFIG_VERSION 2

Node::Node()
	: Module(moduleID::NODE_ID, "node")
{
	moduleVersion = NODE_MODULE_CONFIG_VERSION;

	//Initialize variables
	GS->node = this;

	this->clusterId = 0;
	this->clusterSize = 1;

	this->currentAckId = 0;

	this->noNodesFoundCounter = 0;

	this->outputRawData = false;

	this->radioActiveCount = 0;

	meshAdvJobHandle = nullptr;

	rebootTimeDs = 0;

	//Set the current state and its timeout
	currentStateTimeoutDs = 0;
	currentDiscoveryState = discoveryState::DISCOVERY_OFF;
	nextDiscoveryState = discoveryState::INVALID_STATE;
	this->lastDecisionTimeDs = 0;

	initializedByGateway = false;
	
	joinMePackets.zeroData();

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(NodeConfiguration);

	//Load default configuration
	ResetToDefaultConfiguration();
}

void Node::ResetToDefaultConfiguration()
{
	configuration.moduleId = moduleID::NODE_ID;
	configuration.moduleVersion = NODE_MODULE_CONFIG_VERSION;
	configuration.moduleActive = true;

	configuration.dBmTX = Config->defaultDBmTX;

	//Load defaults from Config
	configuration.enrollmentState = RamConfig->defaultNetworkId != 0 ? EnrollmentState::ENROLLED : EnrollmentState::NOT_ENROLLED;
	configuration.deviceType = (deviceTypes)RamConfig->deviceType;
	configuration.nodeId = RamConfig->defaultNodeId;
	configuration.networkId = RamConfig->defaultNetworkId;
	memcpy(configuration.networkKey, RamConfig->defaultNetworkKey, 16);
	memcpy(configuration.userBaseKey, RamConfig->defaultUserBaseKey, 16);

	memcpy(&configuration.bleAddress, &RamConfig->staticAccessAddress, sizeof(ble_gap_addr_t));

	SET_FEATURESET_CONFIGURATION(&configuration);
}

void Node::ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength)
{
	u32 err;

	//We must now decide if we want to overwrite some unset persistent config values with defaults
	if(configuration.deviceType == 0) configuration.deviceType = (deviceTypes)RamConfig->deviceType;
	if(configuration.nodeId == 0) configuration.nodeId = RamConfig->defaultNodeId;
	if(configuration.networkId == 0) configuration.networkId = RamConfig->defaultNetworkId;
	if(Utility::CompareMem(0x00, configuration.networkKey, 16)){
		memcpy(configuration.networkKey, RamConfig->defaultNetworkKey, 16);
	}
	if(Utility::CompareMem(0x00, configuration.userBaseKey, 16)){
		memcpy(configuration.userBaseKey, RamConfig->defaultUserBaseKey, 16);
	}

	//Random offset that can be used to disperse packets from different nodes over time
	GS->appTimerRandomOffsetDs = (configuration.nodeId % 100);

	//Change window title of the Terminal
	SetTerminalTitle();
	logt("NODE", "====> Node %u (%s) <====", configuration.nodeId, RamConfig->serialNumber);

	//Get a random number for the connection loss counter (hard on system start,...stat)
	connectionLossCounter = 0;
	randomBootNumber = Utility::GetRandomInteger();

	clusterId = this->GenerateClusterID();

	//Set the BLE address so that we have the same on every startup, mostly for debugging
	if(configuration.bleAddress.addr_type != 0xFF){
		err = FruityHal::BleGapAddressSet(&configuration.bleAddress);
		if(err != NRF_SUCCESS){
			//Can be ignored and will not happen
		}
	}

	//Set preferred TX power
	err = sd_ble_gap_tx_power_set(configuration.dBmTX);

	//Print configuration and start node
	logt("NODE", "Config loaded nodeId:%d, connLossCount:%u, networkId:%d", configuration.nodeId, connectionLossCounter, configuration.networkId);

	//Register the mesh service in the GATT table
	InitializeMeshGattService();

	//Remove Advertising job if it's been registered before
	GS->advertisingController->RemoveJob(meshAdvJobHandle);

	if(configuration.moduleActive && configuration.networkId != 0){
		//Register Job with AdvertisingController
		AdvJob job = {
			AdvJobTypes::SCHEDULED,
			5, //Slots
			0, //Delay
			MSEC_TO_UNITS(100, UNIT_0_625_MS), //AdvInterval
			0, //AdvChannel
			0, //CurrentSlots
			0, //CurrentDelay
			BLE_GAP_ADV_TYPE_ADV_IND, //Advertising Mode
			{0}, //AdvData
			0, //AdvDataLength
			{0}, //ScanData
			0 //ScanDataLength
		};
		meshAdvJobHandle = GS->advertisingController->AddJob(job);
	}

	//Go to Discovery if node is active
	if(configuration.moduleActive != 0 && configuration.networkId != 0){
		//Fill JOIN_ME packet with data
		this->UpdateJoinMePacket();

		ChangeState(discoveryState::DISCOVERY_HIGH);
	}
}

void Node::RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength)
{

}

void Node::InitializeMeshGattService()
{
	u32 err = 0;

	//##### At first, we register our custom service
	//Add our Service UUID to the BLE stack for management
	ble_uuid128_t baseUUID128 = { MESH_SERVICE_BASE_UUID128 };
	err = sd_ble_uuid_vs_add(&baseUUID128, &meshService.serviceUuid.type);
	APP_ERROR_CHECK(err); //OK

	//Add the service
	err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &meshService.serviceUuid, &meshService.serviceHandle);
	APP_ERROR_CHECK(err); //OK

	//##### Now we need to add a characteristic to that service

	//BLE GATT Attribute Metadata http://developer.nordicsemi.com/nRF51_SDK/doc/7.1.0/s120/html/a00163.html
	//Read and write permissions, variable length, etc...
	ble_gatts_attr_md_t attributeMetadata;
	memset(&attributeMetadata, 0, sizeof(ble_gatts_attr_md_t));

	//If encryption is enabled, we want our mesh handle only to be accessable over an
	//encrypted connection with authentication
	if(Config->encryptionEnabled){
		BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&attributeMetadata.read_perm);
		BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&attributeMetadata.write_perm);
	}
	else
	{
		BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attributeMetadata.read_perm);
		BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attributeMetadata.write_perm);
	}

	attributeMetadata.vloc = BLE_GATTS_VLOC_STACK; //We currently have the value on the SoftDevice stack, we might port that to the application space
	attributeMetadata.rd_auth = 0;
	attributeMetadata.wr_auth = 0;
	attributeMetadata.vlen = 1; //Make it a variable length attribute

	//Characteristic metadata, whatever....
	ble_gatts_char_md_t characteristicMetadata;
	memset(&characteristicMetadata, 0, sizeof(ble_gatts_char_md_t));
	characteristicMetadata.char_props.read = 1; /*Reading value permitted*/
	characteristicMetadata.char_props.write = 1; /*Writing value with Write Request permitted*/
	characteristicMetadata.char_props.write_wo_resp = 1; /*Writing value with Write Command permitted*/
	characteristicMetadata.char_props.auth_signed_wr = 0; /*Writing value with Signed Write Command not permitted*/
	characteristicMetadata.char_props.notify = 1; /*Notications of value permitted*/
	characteristicMetadata.char_props.indicate = 0; /*Indications of value not permitted*/
	characteristicMetadata.p_cccd_md = nullptr;

	//Finally, the attribute
	ble_gatts_attr_t attribute;
	memset(&attribute, 0, sizeof(ble_gatts_attr_t));

	ble_uuid_t attributeUUID;
	attributeUUID.type = meshService.serviceUuid.type;
	attributeUUID.uuid = MESH_SERVICE_CHARACTERISTIC_UUID;

	attribute.p_uuid = &attributeUUID; /* The UUID of the Attribute*/
	attribute.p_attr_md = &attributeMetadata; /* The previously defined attribute Metadata */
	attribute.max_len = MESH_CHARACTERISTIC_MAX_LENGTH;
	attribute.init_len = 0;
	attribute.init_offs = 0;

	//Finally, add the characteristic
	err = sd_ble_gatts_characteristic_add(meshService.serviceHandle, &characteristicMetadata, &attribute, &meshService.sendMessageCharacteristicHandle);
	APP_ERROR_CHECK(err); //OK
}


/*
 #########################################################################################################
 ### Connections and Handlers
 #########################################################################################################
 */
#define ________________CONNECTION___________________
#pragma region connections

//Is called as soon as a connection is connected, before the handshake
void Node::MeshConnectionConnectedHandler() const
{
	logt("NODE", "Connection initiated");
}

//Is called after a connection has ended its handshake
void Node::HandshakeDoneHandler(MeshConnection* connection, bool completedAsWinner)
{
	logt("HANDSHAKE", "############ Handshake done (asWinner:%u) ###############", completedAsWinner);

	StatusReporterModule* statusMod = (StatusReporterModule*)GS->node->GetModuleById(moduleID::STATUS_REPORTER_MODULE_ID);
	if(statusMod != nullptr){
		statusMod->SendLiveReport(LiveReportTypes::MESH_CONNECTED, connection->partnerId, completedAsWinner);
	}

	GS->logger->logCount(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_HANDSHAKE_DONE);

	//We can now commit the changes that were part of the handshake
	//This node was the winner of the handshake and successfully acquired a new member
	if(completedAsWinner){
		//Update node data
		clusterSize += 1;
		connection->hopsToSink = connection->clusterAck1Packet.payload.hopsToSink < 0 ? -1 : connection->clusterAck1Packet.payload.hopsToSink + 1;

		logt("HANDSHAKE", "ClusterSize Change from %d to %d", clusterSize-1, clusterSize);

		//Update connection data
		connection->connectedClusterId = connection->clusterIDBackup;
		connection->partnerId = connection->clusterAck1Packet.header.sender;
		connection->connectedClusterSize = 1;

		//Broadcast cluster update to other connections
		connPacketClusterInfoUpdate outPacket;
		memset((u8*)&outPacket, 0x00, sizeof(connPacketClusterInfoUpdate));

		outPacket.header.messageType = MESSAGE_TYPE_CLUSTER_INFO_UPDATE;
		outPacket.header.sender = configuration.nodeId;
		outPacket.header.receiver = NODE_ID_BROADCAST;

		outPacket.payload.clusterSizeChange = 1;
		outPacket.payload.newClusterId = connection->clusterIDBackup;
		outPacket.payload.connectionMasterBitHandover = 0;

		logt("HANDSHAKE", "OUT => ALL MESSAGE_TYPE_CLUSTER_INFO_UPDATE clustChange:1");

		SendClusterInfoUpdate(connection, &outPacket);

	//This node was the looser of the Handshake and is now part of a newer bigger cluster
	} else {

		//The node that receives this message can not be connected to any other node
		//This is why we can set absolute values for the clusterSize
		connection->connectedClusterId = connection->clusterAck2Packet.payload.clusterId;
		connection->connectedClusterSize = connection->clusterAck2Packet.payload.clusterSize - 1; // minus myself

		//If any cluster updates are waiting, we delete them
		memset(&connection->currentClusterInfoUpdatePacket, 0x00, sizeof(connPacketClusterInfoUpdate));

		clusterId = connection->clusterAck2Packet.payload.clusterId;
		clusterSize = connection->clusterAck2Packet.payload.clusterSize; // The other node knows best

		connection->hopsToSink = connection->clusterAck2Packet.payload.hopsToSink < 0 ? -1 : connection->clusterAck2Packet.payload.hopsToSink + 1;

		logt("HANDSHAKE", "ClusterSize set to %d", clusterSize);
	}

	logjson("CLUSTER", "{\"type\":\"cluster_handshake\",\"winner\":%u,\"size\":%d}" SEP, completedAsWinner, clusterSize);

	logjson("SIM", "{\"type\":\"mesh_connect\",\"partnerId\":%u}" SEP, connection->partnerId);

	connection->connectionState = ConnectionState::HANDSHAKE_DONE;
	connection->connectionHandshakedTimestampDs = GS->appTimerDs;

	//Call our lovely modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(GS->activeModules[i] != nullptr && GS->activeModules[i]->configurationPointer->moduleActive){
			GS->activeModules[i]->MeshConnectionChangedHandler(*connection);
		}
	}

	//Enable discovery or prolong its state
	KeepHighDiscoveryActive();

	//Update our advertisement packet
	UpdateJoinMePacket();

	//Pass on the masterbit to someone if necessary
	HandOverMasterBitIfNecessary(connection);
}

//TODO: part of the connection manager
//void Node::HandshakeTimeoutHandler()
//{
//	logt("HANDSHAKE", "############ Handshake TIMEOUT/FAIL ###############");
//
//	//Disconnect the hanging connection
//	BaseConnections conn = GS->cm->GetBaseConnections(ConnectionDirection::INVALID);
//	for(int i=0; i<conn.count; i++){
//		if(conn.connections[i]->isConnected() && !conn.connections[i]->handshakeDone()){
//			u32 handshakeTimePassed = GS->appTimerDs - conn.connections[i]->handshakeStartedDs;
//			logt("HANDSHAKE", "Disconnecting conn %u, timePassed:%u", conn.connections[i]->connectionId, handshakeTimePassed);
//			conn.connections[i]->Disconnect();
//		}
//	}
//
//	//Go back to discovery
//	ChangeState(discoveryState::DISCOVERY);
//}


void Node::MeshConnectionDisconnectedHandler(ConnectionState connectionStateBeforeDisconnection, u8 hadConnectionMasterBit, i16 connectedClusterSize, u32 connectedClusterId)
{
	logt("NODE", "MeshConn Disconnected with previous state %u", connectionStateBeforeDisconnection);

	//TODO: If the local host disconnected this connection, it was already increased, we do not have to count the disconnect here
	this->connectionLossCounter++;

	//If the handshake was already done, this node was part of our cluster
	//If the local host terminated the connection, we do not count it as a cluster Size change
	if (
		connectionStateBeforeDisconnection >= ConnectionState::HANDSHAKE_DONE
	){
		//CASE 1: if our partner has the connection master bit, we must dissolve
		//It may happen rarely that the connection master bit was just passed over and that neither node has it
		//This will result in two clusters dissolving
		if (!hadConnectionMasterBit)
		{
			GS->cm->ForceDisconnectOtherMeshConnections(nullptr, AppDisconnectReason::PARTNER_HAS_MASTERBIT);

			clusterSize = 1;
			clusterId = GenerateClusterID();

		}

		//CASE 2: If we have the master bit, we keep our ClusterId (happens if we are the biggest cluster)
		else
		{
			logt("HANDSHAKE", "ClusterSize Change from %d to %d", this->clusterSize, this->clusterSize - connectedClusterSize);

			this->clusterSize -= connectedClusterSize;

			// Inform the rest of the cluster of our new size
			connPacketClusterInfoUpdate packet;
			memset((u8*)&packet, 0x00, sizeof(connPacketClusterInfoUpdate));

			packet.header.messageType = MESSAGE_TYPE_CLUSTER_INFO_UPDATE;
			packet.header.sender = configuration.nodeId;
			packet.header.receiver = NODE_ID_BROADCAST;

			packet.payload.newClusterId = connectedClusterId;
			packet.payload.clusterSizeChange = -connectedClusterSize;

			SendClusterInfoUpdate(nullptr, &packet);

		}

		logjson("CLUSTER", "{\"type\":\"cluster_disconnect\",\"size\":%d}" SEP, clusterSize);

	}
	//Handshake had not yet finished, not much to do
	else
	{

	}

	//Enable discovery or prolong its state
	KeepHighDiscoveryActive();

	//To be sure we do not have a clusterId clash if we are disconnected, we generate one if we are a single node, doesn't hurt
	if (clusterSize == 1) clusterId = GenerateClusterID();

	//In either case, we must update our advertising packet
	UpdateJoinMePacket();

	//Pass on the masterbit to someone if necessary
	HandOverMasterBitIfNecessary(nullptr);

	//Revert to discovery high
	noNodesFoundCounter = 0;
	ChangeState(discoveryState::DISCOVERY_HIGH); 
}

//Handles incoming cluster info update
void Node::ReceiveClusterInfoUpdate(MeshConnection* connection, connPacketClusterInfoUpdate* packet)
{
	//Check if next expected counter matches, if not, this clusterUpdate was a duplicate and we ignore it (might happen during reconnection)
	if (connection->nextExpectedClusterUpdateCounter == packet->payload.counter) {
		connection->nextExpectedClusterUpdateCounter++;
	}
	else {
		//TODO: Log an error to ram
		//This must not happen normally, only in rare cases where the connection is reestablished and the remote node receives a duplicate of the cluster update message
		SIMSTATCOUNT("ClusterUpdateCountMismatch");
		logt("ERROR", "Next expected ClusterUpdateCounter did not match");
		return;
	}

	//Prepare cluster update packet for other connections
	connPacketClusterInfoUpdate outPacket;
	memset((u8*)&outPacket, 0x00, sizeof(connPacketClusterInfoUpdate));

	outPacket.header.messageType = MESSAGE_TYPE_CLUSTER_INFO_UPDATE;
	outPacket.header.receiver = NODE_ID_BROADCAST;
	outPacket.header.sender = configuration.nodeId;

	outPacket.payload.clusterSizeChange = packet->payload.clusterSizeChange;

	//Update hops to sink
	//Another sink may have joined or left the network, update this
	//FIXME: race conditions can cause this to work incorrectly...

	if(packet->payload.clusterSizeChange != 0){
		logt("HANDSHAKE", "ClusterSize Change from %d to %d", this->clusterSize, this->clusterSize + packet->payload.clusterSizeChange);
		this->clusterSize += packet->payload.clusterSizeChange;
		connection->connectedClusterSize += packet->payload.clusterSizeChange;
	}

	connection->hopsToSink = packet->payload.hopsToSink > -1 ? packet->payload.hopsToSink + 1 : -1;

	//this->clusterId = packet->payload.newClusterId;
	outPacket.payload.newClusterId = packet->payload.newClusterId;

	//Now look if our partner has passed over the connection master bit
	if(packet->payload.connectionMasterBitHandover){
		connection->connectionMasterBit = 1;
	}

	//Pass on the masterbit to someone else if necessary
	HandOverMasterBitIfNecessary(connection);

	//hops to sink are updated in the send method
	//current cluster id is updated in the send method

	SendClusterInfoUpdate(connection, &outPacket);

	//Log Cluster change to UART
	logjson("CLUSTER", "{\"type\":\"cluster_update\",\"size\":%d,\"newId\":%u,\"masterBit\":%u}" SEP, clusterSize, clusterId, packet->payload.connectionMasterBitHandover);

	//Enable discovery or prolong its state
	KeepHighDiscoveryActive();

	//Update adverting packet
	this->UpdateJoinMePacket();

	//TODO: What happens if:
	/*
	 * We send a clusterid update and commit it in our connection arm
	 * The other one does the same at nearly the same time
	 * ID before was 3, A now has 2 and 2 on the connection arm, B has 4 and 4 on the connection arm
	 * Then both will not accept the new ClusterId!!!
	 * What if the biggest id will always win?
	 */
}

void Node::HandOverMasterBitIfNecessary(MeshConnection* connection)  const{
	//If we have all masterbits, we can give 1 at max
	//We do this, if the connected cluster size is bigger than all the other connected cluster sizes summed together
	bool hasAllMasterBits = HasAllMasterBits();
	if (hasAllMasterBits) {
		MeshConnections conn = GS->cm->GetMeshConnections(ConnectionDirection::INVALID);
		for (u32 i = 0; i < conn.count; i++) {
			MeshConnection* c2 = conn.connections[i];
			if (c2->connectedClusterSize > clusterSize - c2->connectedClusterSize) {
				//Remove the masterbit from this connection
				if(connection != nullptr) connection->connectionMasterBit = 0;
				//Put the masterbit handover in the correct packet.
				c2->currentClusterInfoUpdatePacket.header.messageType = MESSAGE_TYPE_CLUSTER_INFO_UPDATE;
				c2->currentClusterInfoUpdatePacket.payload.connectionMasterBitHandover = 1;
			}
		}
	}
}

bool Node::HasAllMasterBits() const {
	MeshConnections conn = GS->cm->GetMeshConnections(ConnectionDirection::INVALID);
	for (u32 i = 0; i < conn.count; i++) {
		MeshConnection* connection = conn.connections[i];
		//Connection must be handshaked, if yes check if we have its masterbit
		if (connection->handshakeDone() && !connection->connectionMasterBit) {
			return false;
		}
	}
	return true;
}



//Saves a cluster update for all connections (except the one that caused it)
//This update will then be sent by a connection as soon as the connection is ready (handshakeDone)
void Node::SendClusterInfoUpdate(MeshConnection* ignoreConnection, connPacketClusterInfoUpdate* packet) const
{
	MeshConnections conn = GS->cm->GetMeshConnections(ConnectionDirection::INVALID);
	for (u32 i = 0; i < conn.count; i++) {
		if(!conn.connections[i]->isConnected() || conn.connections[i] == ignoreConnection) continue;

		packet->payload.hopsToSink = GS->cm->GetMeshHopsToShortestSink(conn.connections[i]);

		//Get the current packet
		connPacketClusterInfoUpdate* currentPacket = &(conn.connections[i]->currentClusterInfoUpdatePacket);

		//If another clusterUpdate message is about to be sent
		if(currentPacket->header.messageType == MESSAGE_TYPE_CLUSTER_INFO_UPDATE){
			logt("HANDSHAKE", "TO NODE %u Adding to clusterSize change:%d, id:%u, hops:%d", conn.connections[i]->partnerId, packet->payload.clusterSizeChange, packet->payload.newClusterId, packet->payload.hopsToSink);

			currentPacket->payload.clusterSizeChange += packet->payload.clusterSizeChange;
			currentPacket->payload.newClusterId = packet->payload.newClusterId; //TODO: we could intelligently choose our ClusterId
			currentPacket->payload.hopsToSink = GS->cm->GetMeshHopsToShortestSink(conn.connections[i]);
			//=> The counter and maybe some other fields are set right before queuing the packet

			HandOverMasterBitIfNecessary(conn.connections[i]);

		//If no other clusterUpdate message is waiting to be sent
		} else {
			logt("HANDSHAKE", "TO NODE %u clusterSize change:%d, id:%u, hops:%d", conn.connections[i]->partnerId,  packet->payload.clusterSizeChange, packet->payload.newClusterId, packet->payload.hopsToSink);
			memcpy((u8*)currentPacket, (u8*)packet, sizeof(connPacketClusterInfoUpdate));
		}
	}
	//TODO: If we call fillTransmitBuffers after a timeout, they would accumulate more,...
	GS->cm->fillTransmitBuffers();
}

void Node::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader)
{
	//Must call superclass for handling
	Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

	//If the packet is a handshake packet it will not be forwarded to the node but will be
	//handled in the connection. All other packets go here for further processing
	switch (packetHeader->messageType)
	{
		case MESSAGE_TYPE_CLUSTER_INFO_UPDATE:
			if (
					connection != nullptr
					&& connection->connectionType == ConnectionTypes::CONNECTION_TYPE_FRUITYMESH
					&& sendData->dataLength >= SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE)
			{
				connPacketClusterInfoUpdate* packet = (connPacketClusterInfoUpdate*) packetHeader;
				logt("HANDSHAKE", "IN <= %d CLUSTER_INFO_UPDATE newClstId:%u, sizeChange:%d, hop:%d", connection->partnerId, packet->payload.newClusterId, packet->payload.clusterSizeChange, packet->payload.hopsToSink);
				ReceiveClusterInfoUpdate((MeshConnection*)connection, packet);

			}
			break;
#ifndef SAVE_SPACE_1
		case MESSAGE_TYPE_DATA_2:
			if (sendData->dataLength >= SIZEOF_CONN_PACKET_DATA_2)
			{
				connPacketData2* packet = (connPacketData2*) packetHeader;
				NodeId partnerId = connection == nullptr ? 0 : connection->partnerId;

				logt("DATA", "IN <= %d ################## Got Data 2 packet %c ##################", partnerId, packet->payload.data[0]);
			}
			break;
		case MESSAGE_TYPE_ADVINFO:
			if (sendData->dataLength >= SIZEOF_CONN_PACKET_ADV_INFO)
			{
				connPacketAdvInfo* packet = (connPacketAdvInfo*) packetHeader;

				logjson("ADVINFO", "{\"sender\":\"%d\",\"addr\":\"%x:%x:%x:%x:%x:%x\",\"count\":%d,\"rssiSum\":%d}" SEP, packet->header.sender, packet->payload.peerAddress[0], packet->payload.peerAddress[1], packet->payload.peerAddress[2], packet->payload.peerAddress[3], packet->payload.peerAddress[4], packet->payload.peerAddress[5], packet->payload.packetCount, packet->payload.inverseRssiSum);

			}
			break;

		case MESSAGE_TYPE_UPDATE_CONNECTION_INTERVAL:
			if(sendData->dataLength == SIZEOF_CONN_PACKET_UPDATE_CONNECTION_INTERVAL)
			{
				connPacketUpdateConnectionInterval* packet = (connPacketUpdateConnectionInterval*) packetHeader;

				GS->cm->SetMeshConnectionInterval(packet->newInterval);
			}
			break;
#endif

	}

#ifndef SAVE_SPACE_1
	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_CONFIG)
	{
		connPacketModule* packet = (connPacketModule*) packetHeader;

		if(packet->actionType == (u8)Module::ModuleConfigMessages::GET_MODULE_LIST)
		{
			SendModuleList(packet->header.sender, packet->requestHandle);

		}
		else if(packet->actionType == (u8)Module::ModuleConfigMessages::MODULE_LIST)
		{

			logjson("MODULE", "{\"nodeId\":%u,\"type\":\"module_list\",\"modules\":[", packet->header.sender);

			u16 moduleCount = (sendData->dataLength - SIZEOF_CONN_PACKET_MODULE) / 4;
			bool first = true;
			for(int i=0; i<moduleCount; i++){
				u8 moduleId = 0, version = 0, active = 0;
				memcpy(&moduleId, packet->data + i*4+0, 1);
				memcpy(&version, packet->data + i*4+2, 1);
				memcpy(&active, packet->data + i*4+3, 1);

				//comma seperator issue,....
				if(!first){
					logjson("MODULE", ",");
				}
				logjson("MODULE", "{\"id\":%u,\"version\":%u,\"active\":%u}", moduleId, version, active);

				first = false;
			}
			logjson("MODULE", "]}" SEP);
		}
	}
#endif

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleID::NODE_ID){

			if(packet->actionType == (u8)NodeModuleTriggerActionMessages::SET_DISCOVERY){

				u8 ds = packet->data[0];

				if(ds == 0){
					ChangeState(discoveryState::DISCOVERY_OFF);
				} else {
					ChangeState(discoveryState::DISCOVERY_HIGH);
				}

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
					packetHeader->sender,
					(u8)NodeModuleActionResponseMessages::SET_DISCOVERY_RESULT,
					0,
					nullptr,
					0,
					false
				);
			}

			else if (packet->actionType == (u8)NodeModuleTriggerActionMessages::RESET_NODE)
				{
					NodeModuleResetMessage* message = (NodeModuleResetMessage*)packet->data;
					logt("NODE", "Scheduled reboot in %u seconds", message->resetSeconds);
					Reboot(message->resetSeconds*10);
			}
		}
	}

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_ACTION_RESPONSE){
			connPacketModule* packet = (connPacketModule*)packetHeader;
			//Check if our module is meant and we should trigger an action
			if(packet->moduleId == moduleID::NODE_ID){

				if(packet->actionType == (u8)NodeModuleActionResponseMessages::SET_DISCOVERY_RESULT){

					logjson("NODE", "{\"type\":\"set_discovery_result\",\"nodeId\":%d,\"module\":%d}" SEP, packetHeader->sender, moduleID::NODE_ID);
				}
			}
		}

	if (packetHeader->messageType == MESSAGE_TYPE_MODULE_RAW_DATA) {
		const RawDataHeader* packet = (RawDataHeader*)packetHeader;
		//Check if our module is meant
		if (packet->moduleId == moduleId) {
			const RawDataActionType actionType = (const RawDataActionType)packet->actionType;
			if (actionType == RawDataActionType::START)
			{
				RawDataStart packet = *(const RawDataStart*)packetHeader;

				logjson("DEBUG",
					"{"
						"\"nodeId\":%u,"
						"\"type\":\"raw_data_start\","
						"\"module\":%u,"
						"\"numChunks\":%u,"
						"\"protocol\":%u,"
						"\"fmKeyId\":%u,"
						"\"requestHandle\":%u"
					"}" SEP,
					packet.header.connHeader.sender,
					moduleId,
					packet.numChunks,
					packet.protocolId,
					packet.fmKeyId,
					packet.header.requestHandle
				);
			}
			else if (actionType == RawDataActionType::START_RECEIVED)
			{
				RawDataStartReceived packet = *(const RawDataStartReceived*)packetHeader;

				logjson("DEBUG",
					"{"
						"\"nodeId\":%u,"
						"\"type\":\"raw_data_start_received\","
						"\"module\":%u,"
						"\"requestHandle\":%u"
					"}" SEP,
					packet.header.connHeader.sender,
					moduleId,
					packet.header.requestHandle
				);
			}
			else if (actionType == RawDataActionType::ERROR_T)
			{
				const RawDataError* packet = (const RawDataError*)packetHeader;
				logjson("DEBUG",
					"{"
						"\"nodeId\":%u,"
						"\"type\":\"raw_data_error\","
						"\"module\":%u,"
						"\"error\":%u,"
						"\"destination\":%u,"
						"\"requestHandle\":%u"
					"}" SEP,
					packet->header.connHeader.sender,
					moduleId,
					packet->type,
					packet->destination,
					packet->header.requestHandle
				);
			}
			else if (actionType == RawDataActionType::CHUNK)
			{
				const RawDataChunk* packet = (const RawDataChunk*)packetHeader;
				if (CHECK_MSG_SIZE(packet, packet->payload, 1, sendData->dataLength))
				{
					const u32 payloadLength = sendData->dataLength - sizeof(RawDataChunk) + 1;
					char payload[250];
					GS->logger->convertBufferToHexString(packet->payload, payloadLength, payload, sizeof(payload));

					logjson("DEBUG",
						"{"
							"\"nodeId\":%u,"
							"\"type\":\"raw_data_chunk\","
							"\"module\":%u,"
							"\"chunkId\":%u,"
							"\"payload\":\"%s\","
							"\"requestHandle\":%u"
						"}" SEP,
						packet->header.connHeader.sender,
						moduleId,
						packet->chunkId,
						payload,
						packet->header.requestHandle
					);
				}
				else
				{
					SIMEXCEPTION(PaketTooSmall);
				}
			}
			else if (actionType == RawDataActionType::REPORT)
			{
				const RawDataReport* packet = (const RawDataReport*)packetHeader;

				char missingsBuffer[200] = "[";
				bool successfulTransmission = true;
				for (u32 i = 0; i < sizeof(packet->missings) / sizeof(packet->missings[0]); i++)
				{
					if (packet->missings[i] != 0)
					{
						char singleMissingBuffer[50];
						snprintf(singleMissingBuffer, sizeof(singleMissingBuffer), "%u", packet->missings[i]);

						if (!successfulTransmission) 
						{
							strcat(missingsBuffer, ",");
						}
						strcat(missingsBuffer, singleMissingBuffer);

						successfulTransmission = false;
					}
				}

				strcat(missingsBuffer, "]");


				logjson("DEBUG",
					"{"
						"\"nodeId\":%u,"
						"\"type\":\"raw_data_report\","
						"\"module\":%u,"
						"\"missing\":%s,"
						"\"requestHandle\":%u"
					"}" SEP,
					packet->header.connHeader.sender,
					moduleId,
					missingsBuffer,
					packet->header.requestHandle
				);
			}
			else
			{
				SIMEXCEPTION(GotUnsupportedActionTypeException);
			}
		}
	}
	else if (packetHeader->messageType == MESSAGE_TYPE_MODULE_RAW_DATA_LIGHT) 
	{
		const RawDataLight* packet = (const RawDataLight*)packetHeader;
		if (CHECK_MSG_SIZE(packet, packet->payload, 1, sendData->dataLength))
		{
			const u32 payloadLength = sendData->dataLength - sizeof(RawDataLight) + 1;
			char payload[250];
			GS->logger->convertBufferToHexString(packet->payload, payloadLength, payload, sizeof(payload));

			logjson("DEBUG",
				"{"
				"\"nodeId\":%u,"
				"\"type\":\"raw_data_light\","
				"\"module\":%u,"
				"\"protocol\":%u,"
				"\"payload\":\"%s\","
				"\"requestHandle\":%u"
				"}" SEP,
				packet->connHeader.sender,
				moduleId,
				packet->protocolId,
				payload,
				packet->requestHandle
			);
		}
		else
		{
			SIMEXCEPTION(PaketTooSmall);
		}
	}
}

//Processes incoming CLUSTER_INFO_UPDATE packets
#pragma endregion Connection and Handlers
/*
 #########################################################################################################
 ### Advertising and Receiving advertisements
 #########################################################################################################
 */
#define ________________ADVERTISING___________________
#pragma region advertising

//Start to broadcast our own clusterInfo, set ackID if we want to have an ack or an ack response
void Node::UpdateJoinMePacket() const
{
	if(!configuration.moduleActive) return;

	SetTerminalTitle();

	u8* buffer = meshAdvJobHandle->advData;
	u8* bufferPointer = buffer;

	advPacketHeader* advPacket = (advPacketHeader*)bufferPointer;
	advPacket->flags.len = SIZEOF_ADV_STRUCTURE_FLAGS-1; //minus length field itself
	advPacket->flags.type = BLE_GAP_AD_TYPE_FLAGS;
	advPacket->flags.flags = BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;

	advPacket->manufacturer.len = (SIZEOF_ADV_STRUCTURE_MANUFACTURER + SIZEOF_ADV_PACKET_STUFF_AFTER_MANUFACTURER + SIZEOF_ADV_PACKET_PAYLOAD_JOIN_ME_V0) - 1;
	advPacket->manufacturer.type = BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA;
	advPacket->manufacturer.companyIdentifier = COMPANY_IDENTIFIER;

	advPacket->meshIdentifier = MESH_IDENTIFIER;
	advPacket->networkId = configuration.networkId;
	advPacket->messageType = MESSAGE_TYPE_JOIN_ME_V0;

	//Build a JOIN_ME packet and set it in the advertisement data
	advPacketPayloadJoinMeV0* packet = (advPacketPayloadJoinMeV0*)(bufferPointer+SIZEOF_ADV_PACKET_HEADER);
	packet->sender = configuration.nodeId;
	packet->clusterId = this->clusterId;
	packet->clusterSize = this->clusterSize;
	packet->freeMeshInConnections = GS->cm->freeMeshInConnections;
	packet->freeMeshOutConnections = GS->cm->freeMeshOutConnections;

	//A leaf only has one free in connection
	if(configuration.deviceType == DEVICE_TYPE_LEAF){
		if(GS->cm->freeMeshInConnections > 0) packet->freeMeshInConnections = 1;
		packet->freeMeshOutConnections = 0;
	}

	StatusReporterModule* statusMod = (StatusReporterModule*)this->GetModuleById(moduleID::STATUS_REPORTER_MODULE_ID);
	if(statusMod != nullptr){
		packet->batteryRuntime = statusMod->GetBatteryVoltage();
	} else {
		packet->batteryRuntime = 0;
	}

	packet->txPower = configuration.dBmTX;
	packet->deviceType = configuration.deviceType;
	packet->hopsToSink = GS->cm->GetMeshHopsToShortestSink(nullptr);
	packet->meshWriteHandle = meshService.sendMessageCharacteristicHandle.value_handle;

	if (currentAckId != 0)
	{
		packet->ackField = currentAckId;

	} else {
		packet->ackField = 0;
	}

//#ifdef ACTIVATE_ASSET_MODULE
//	//FIXME: we need another field to do this. maybe add another advjob?
//	packet->txPower = this->GetAccValue();
//	meshAdvJobHandle->advertisingInterval = this->GetAccAdvInterval();
//#endif
	meshAdvJobHandle->advDataLength = SIZEOF_ADV_PACKET_HEADER + SIZEOF_ADV_PACKET_PAYLOAD_JOIN_ME_V0;

	logt("JOIN", "JOIN_ME updated clusterId:%x, clusterSize:%d, freeIn:%u, freeOut:%u, handle:%u, ack:%u", packet->clusterId, packet->clusterSize, packet->freeMeshInConnections, packet->freeMeshOutConnections, packet->meshWriteHandle, packet->ackField);

	logjson("SIM", "{\"type\":\"update_joinme\",\"clusterId\":%u,\"clusterSize\":%d}" SEP, clusterId, clusterSize);

	//Stop advertising if we are already connected as a leaf. Necessary for EoModule
	if(configuration.deviceType == DEVICE_TYPE_LEAF && GS->cm->freeMeshInConnections == 0){
		meshAdvJobHandle->slots = 0;
	} else if(configuration.deviceType == DEVICE_TYPE_LEAF){
		meshAdvJobHandle->slots = 5;
	}

	GS->advertisingController->RefreshJob(meshAdvJobHandle);
}

//STEP 3: After collecting all available clusters, we want to connect to the best cluster that is available
//If the other clusters were not good and we have something better, we advertise it.
Node::DecisionStruct Node::DetermineBestClusterAvailable(void)
{
	DecisionStruct result = { DecisionResult::NO_NODES_FOUND, 0, 0};

	u32 bestScore = 0;
	joinMeBufferPacket* bestCluster = nullptr;
	joinMeBufferPacket* packet = nullptr;

	//Determine the best Cluster to connect to as a master
	if (GS->cm->freeMeshOutConnections > 0)
	{
		for (int i = 0; i < joinMePackets.length; i++)
		{
			packet = &joinMePackets[i];
			if (packet->payload.sender == 0) continue;

			u32 score = CalculateClusterScoreAsMaster(packet);
			if (score > bestScore)
			{
				bestScore = score;
				bestCluster = packet;
			}
		}

		//Now, if we want to be a master in the connection, we simply answer the ad packet that
		//informs us about that cluster
		if (bestCluster != nullptr)
		{
			currentAckId = 0;

			fh_ble_gap_addr_t address;
			address.addr_type = bestCluster->bleAddressType;
			memcpy(address.addr, bestCluster->bleAddress, BLE_GAP_ADDR_LEN);

			//Choose a different connection interval for leaf nodes
			u16 connectionIv = Config->meshMinConnectionInterval;
			if(bestCluster->payload.deviceType == DEVICE_TYPE_LEAF){
				connectionIv = MSEC_TO_UNITS(90, UNIT_1_25_MS);
			}

			GS->cm->ConnectAsMaster(bestCluster->payload.sender, &address, bestCluster->payload.meshWriteHandle, connectionIv);

			//Note the time that we tried to connect to this node so that we can blacklist it for some time if it does not work
			bestCluster->lastConnectAttemptDs = GS->appTimerDs;

			result.result = DecisionResult::CONNECT_AS_MASTER;
			result.preferredPartner = bestCluster->payload.sender;
			return result;
		}
	}

	//If no good cluster could be found (all are bigger than mine)
	//Find the best cluster that should connect to us (we as slave)
	packet = nullptr;
	for (int i = 0; i < joinMePackets.length; i++)
	{
		currentAckId = 0;

		packet = &joinMePackets[i];
		if (packet->payload.sender == 0) continue;

		u32 score = CalculateClusterScoreAsSlave(packet);
		if (score > bestScore)
		{
			bestScore = score;
			bestCluster = packet;
		}
	}

	//Set our ack field to the best cluster that we want to be a part of
	if (bestCluster != nullptr)
	{
		currentAckId = bestCluster->payload.clusterId;

		logt("DECISION", "Other clusters are bigger, we are going to be a slave of %u", currentAckId);

		//Check if we have a recently established connection and do not disconnect if yes bofore the handshake has not timed out
		bool freshConnectionAvailable = false;
		BaseConnections conns = GS->cm->GetBaseConnections(ConnectionDirection::INVALID);
		for(u32 i=0; i<conns.count; i++){
			BaseConnection* conn = GS->cm->allConnections[conns.connectionIndizes[i]];
			if (conn != nullptr) {
				if (conn->creationTimeDs + Config->meshHandshakeTimeoutDs > GS->appTimerDs) {
					freshConnectionAvailable = true;
					break;
				}
			}
		}

		if(!freshConnectionAvailable){
			GS->cm->ForceDisconnectOtherMeshConnections(nullptr, AppDisconnectReason::SHOULD_WAIT_AS_SLAVE);

			clusterSize = 1;
			clusterId = GenerateClusterID();

			UpdateJoinMePacket();
		}

		result.result = DecisionResult::CONNECT_AS_SLAVE;
		result.preferredPartner = bestCluster->payload.sender;
		return result;
	}

	logt("DECISION", "no cluster found");

	result.result = DecisionResult::NO_NODES_FOUND;
	return result;
}

//Calculates the score for a cluster
//Connect to big clusters but big clusters must connect nodes that are not able 
u32 Node::CalculateClusterScoreAsMaster(joinMeBufferPacket* packet) const
{

	//If the packet is too old, filter it out
	if (GS->appTimerDs - packet->receivedTimeDs > MAX_JOIN_ME_PACKET_AGE_DS) return 0;

	//If we are already connected to that cluster, the score is 0
	if (packet->payload.clusterId == this->clusterId) return 0;

	//If there are zero free in connections, we cannot connect as master
	if (packet->payload.freeMeshInConnections == 0) return 0;

	//If the other node wants to connect as a slave to another cluster, do not connect
	if (packet->payload.ackField != 0 && packet->payload.ackField != this->clusterId) return 0;

	//If the other cluster is bigger, we cannot connect as master
	if (packet->payload.clusterSize > this->clusterSize) return 0;

	//Check if we recently tried to connect to him and blacklist him for a short amount of time
	if (packet->lastConnectAttemptDs + SEC_TO_DS(Config->meshConnectingScanTimeout) + SEC_TO_DS(1) > GS->appTimerDs) {
		SIMSTATCOUNT("tempBlacklist");
		logt("NODE", "temporary blacklisting node %u", packet->payload.sender);
		return 0;
	}

	//Connection should have a minimum of stability
	if(packet->rssi < STABLE_CONNECTION_RSSI_THRESHOLD) return 0;

	u32 rssiScore = 100 + packet->rssi;

	//If we are a leaf node, we must not connect to anybody
	if(configuration.deviceType == DEVICE_TYPE_LEAF) return 0;

	//Free in connections are best, free out connections are good as well
	//TODO: RSSI should be factored into the score as well, maybe battery runtime, device type, etc...
	return (u32)(packet->payload.freeMeshInConnections) * 10000 + (u32)(packet->payload.freeMeshOutConnections) * 100 + rssiScore;
}

//If there are only bigger clusters around, we want to find the best
//And set its id in our ack field
u32 Node::CalculateClusterScoreAsSlave(joinMeBufferPacket* packet) const
{

	//If the packet is too old, filter it out
	if (GS->appTimerDs - packet->receivedTimeDs > MAX_JOIN_ME_PACKET_AGE_DS) return 0;

	//If we are already connected to that cluster, the score is 0
	if (packet->payload.clusterId == this->clusterId) return 0;

	//He could not connect to us, leave him alone
	if (packet->payload.freeMeshOutConnections == 0) return 0;

	//We will only be a slave of a bigger or equal cluster
	if (packet->payload.clusterSize < this->clusterSize) return 0;

	//Connection should have a minimum of stability
	if(packet->rssi < STABLE_CONNECTION_RSSI_THRESHOLD) return 0;

	u32 rssiScore = 100 + packet->rssi;

	//Choose the one with the biggest cluster size, if there are more, prefer the most outConnections
	return (u32)(packet->payload.clusterSize) * 10000 + (u32)(packet->payload.freeMeshOutConnections) * 100 + rssiScore;
}

//All advertisement packets are received here if they are valid
void Node::AdvertisementMessageHandler(ble_evt_t &bleEvent)
{
	if(!configuration.moduleActive) return;

	u8* data = bleEvent.evt.gap_evt.params.adv_report.data;
	u16 dataLength = bleEvent.evt.gap_evt.params.adv_report.dlen;

	advPacketHeader* packetHeader = (advPacketHeader*) data;

	//Print packet as hex
	/*char stringBuffer[100];
	 convertBufferToHexString(data, dataLength, stringBuffer);
	 tracel(stringBuffer);
	 tracelf("type %d length %d, JOIN ME LENGTH %d", packetHeader->messageType, dataLength, SIZEOF_ADV_PACKET_JOIN_ME);*/

	switch (packetHeader->messageType)
	{
		case MESSAGE_TYPE_JOIN_ME_V0:
			if (dataLength == SIZEOF_ADV_PACKET_JOIN_ME)
			{
				GS->logger->logCount(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::COUNT_JOIN_ME_RECEIVED);

				advPacketJoinMeV0* packet = (advPacketJoinMeV0*) data;

				logt("DISCOVERY", "JOIN_ME: sender:%u, clusterId:%x, clusterSize:%d, freeIn:%u, freeOut:%u, ack:%u", packet->payload.sender, packet->payload.clusterId, packet->payload.clusterSize, packet->payload.freeMeshInConnections, packet->payload.freeMeshOutConnections, packet->payload.ackField);

				//Look through the buffer and determine a space where we can put the packet in
				joinMeBufferPacket* targetBuffer = findTargetBuffer(packet);

				//Now, we have the space for our packet and we fill it with the latest information
				if (targetBuffer != nullptr)
				{
					memcpy(targetBuffer->bleAddress, bleEvent.evt.gap_evt.params.connected.peer_addr.addr, BLE_GAP_ADDR_LEN);
					targetBuffer->bleAddressType = bleEvent.evt.gap_evt.params.connected.peer_addr.addr_type;
					targetBuffer->connectable = bleEvent.evt.gap_evt.params.adv_report.type;
					targetBuffer->rssi = bleEvent.evt.gap_evt.params.adv_report.rssi;
					targetBuffer->receivedTimeDs = GS->appTimerDs;

					targetBuffer->payload = packet->payload;
				}
			}
			break;
	}

}

joinMeBufferPacket* Node::findTargetBuffer(advPacketJoinMeV0* packet)
{
	joinMeBufferPacket* targetBuffer = nullptr;

	//First, look if a packet from this node is already in the buffer, if yes, we use this space
	for (int i = 0; i < joinMePackets.length; i++)
	{
		targetBuffer = &joinMePackets[i];

		if (packet->payload.sender == targetBuffer->payload.sender)
		{
			logt("DISCOVERY", "Updated old buffer packet");
			return targetBuffer;
		}
	}
	targetBuffer = nullptr;

	//Next, we look if there's an empty space
	for (int i = 0; i < joinMePackets.length; i++)
	{
		targetBuffer = &(joinMePackets[i]);

		if(targetBuffer->payload.sender == 0)
		{
			logt("DISCOVERY", "Used empty space");
			return targetBuffer;
		}
	}
	targetBuffer = nullptr;

	//Next, we can overwrite the oldest packet that we saved from our own cluster
	u32 oldestTimestamp = UINT32_MAX;
	for (int i = 0; i < joinMePackets.length; i++)
	{
		joinMeBufferPacket* tmpPacket = &joinMePackets[i];

		if(tmpPacket->payload.clusterId == clusterId && tmpPacket->receivedTimeDs < oldestTimestamp){
			oldestTimestamp = tmpPacket->receivedTimeDs;
			targetBuffer = tmpPacket;
		}
	}

	if(targetBuffer != nullptr){
		logt("DISCOVERY", "Overwrote one from our own cluster");
		return targetBuffer;
	}

	//If there's still no space, we overwrite the oldest packet that we received, this will not fail
	//TODO: maybe do not use oldest one but worst candidate?? Use clusterScore on all packets to find the least interesting
	u32 minScore = UINT32_MAX;
	for (int i = 0; i < joinMePackets.length; i++)
	{
		joinMeBufferPacket* tmpPacket = &joinMePackets[i];

		u32 score = 0;
		if (packet->payload.clusterSize >= clusterSize) {
			score = CalculateClusterScoreAsMaster(tmpPacket);
		}
		else {
			score = CalculateClusterScoreAsSlave(tmpPacket);
		}

		if(score < minScore){
			minScore = score;
			targetBuffer = tmpPacket;
		}
	}

	logt("DISCOVERY", "Overwrote worst packet from different cluster");
	return targetBuffer;
}

#pragma endregion Advertising

/*
 #########################################################################################################
 ### Advertising and Receiving advertisements
 #########################################################################################################
 */
#define ________________STATES___________________
#pragma region states

void Node::ChangeState(discoveryState newState)
{
	if (currentDiscoveryState == newState || stateMachineDisabled || !configuration.moduleActive) return;

	currentDiscoveryState = newState;

	if (newState == discoveryState::DISCOVERY_HIGH)
	{
		logt("STATES", "-- DISCOVERY HIGH --");

		//Reset no nodes fount counter
		noNodesFoundCounter = 0;

		currentStateTimeoutDs = SEC_TO_DS((u32)Config->highToLowDiscoveryTimeSec);
		nextDiscoveryState = Config->highToLowDiscoveryTimeSec == 0 ? discoveryState::INVALID_STATE : discoveryState::DISCOVERY_LOW;

		//Reconfigure the advertising and scanning jobs
		meshAdvJobHandle->advertisingInterval = Config->meshAdvertisingIntervalHigh;
		meshAdvJobHandle->slots = 5;
		GS->advertisingController->RefreshJob(meshAdvJobHandle);
		GS->scanController->SetScanState(SCAN_STATE_HIGH);
	}
	else if (newState == discoveryState::DISCOVERY_LOW)
	{
		logt("STATES", "-- DISCOVERY LOW --");

		currentStateTimeoutDs = 0;
		nextDiscoveryState = discoveryState::INVALID_STATE;

		//Reconfigure the advertising and scanning jobs
		meshAdvJobHandle->advertisingInterval = Config->meshAdvertisingIntervalLow;
		GS->advertisingController->RefreshJob(meshAdvJobHandle);
		GS->scanController->SetScanState(SCAN_STATE_LOW);

	}
	else if (newState == discoveryState::DISCOVERY_OFF)
	{
		logt("STATES", "-- DISCOVERY OFF --");

		nextDiscoveryState = discoveryState::INVALID_STATE;

		meshAdvJobHandle->slots = 0;
		GS->advertisingController->RefreshJob(meshAdvJobHandle);
		GS->scanController->SetScanState(SCAN_STATE_OFF);

	}

	//Inform all modules of the new state
	//Dispatch event to all modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(GS->activeModules[i] != 0 && GS->activeModules[i]->configurationPointer->moduleActive){
			GS->activeModules[i]->NodeStateChangedHandler(newState);
		}
	}
}

void Node::DisableStateMachine(bool disable)
{
	stateMachineDisabled = disable;
}

//Convenience method for stopping all mesh activity
void Node::Stop(){
	ChangeState(discoveryState::DISCOVERY_OFF);
	DisableStateMachine(true);
}

void Node::TimerEventHandler(u16 passedTimeDs)
{
	currentStateTimeoutDs -= passedTimeDs;

	//Check if we should switch states because of timeouts
	if (nextDiscoveryState != INVALID_STATE && currentStateTimeoutDs <= 0)
	{
		//Go to the next state
		ChangeState(nextDiscoveryState);
	}

	//Count the nodes that are a good choice for connecting
	//TODO: We could use this snippet to connect immediately after enought nodes were collected
//	u8 numGoodNodesInBuffer = 0;
//	for (int i = 0; i < joinMePacketBuffer->_numElements; i++)
//	{
//		joinMeBufferPacket* packet = (joinMeBufferPacket*) joinMePacketBuffer->PeekItemAt(i);
//		u32 score = CalculateClusterScoreAsMaster(packet);
//		if (score > 0){
//			numGoodNodesInBuffer++;
//		}
//	}
//
//	if(numGoodNodesInBuffer >= Config->numNodesForDecision) ...

	//Check if there is a good cluster
	if(lastDecisionTimeDs + Config->maxTimeUntilDecisionDs < GS->appTimerDs){
		DecisionStruct decision = DetermineBestClusterAvailable();

		if (decision.result == Node::DecisionResult::NO_NODES_FOUND && noNodesFoundCounter < 100){
			noNodesFoundCounter++;
		} else if (decision.result == Node::DecisionResult::CONNECT_AS_MASTER || decision.result == Node::DecisionResult::CONNECT_AS_SLAVE){
			noNodesFoundCounter = 0;
		}
		lastDecisionTimeDs = GS->appTimerDs;

		StatusReporterModule* statusMod = (StatusReporterModule*)GS->node->GetModuleById(moduleID::STATUS_REPORTER_MODULE_ID);
		if(statusMod != nullptr){
			statusMod->SendLiveReport(LiveReportTypes::DECISION_RESULT, (u8)(decision.result), decision.preferredPartner);
		}
	}

	//Reboot if a time is set
	if(rebootTimeDs != 0 && rebootTimeDs < GS->appTimerDs){
		logt("NODE", "Resetting!");
		//Do not reboot in safe mode
		*GS->rebootMagicNumberPtr = REBOOT_MAGIC_NUMBER;
		FruityHal::SystemReset();
	}
}

void Node::KeepHighDiscoveryActive()
{
	//Reset the state in discovery high, if anything in the cluster configuration changed
	if(currentDiscoveryState == discoveryState::DISCOVERY_HIGH){
		currentStateTimeoutDs = Config->highToLowDiscoveryTimeSec;
	} else {
		ChangeState(discoveryState::DISCOVERY_HIGH);
	}
}

#pragma endregion States

/*
 #########################################################################################################
 ### Helper functions
 #########################################################################################################
 */
#define ________________HELPERS___________________

//Generates a new ClusterId by using connectionLoss and the unique id of the node
ClusterId Node::GenerateClusterID(void) const
{
	//Combine connection loss and nodeId to generate a unique cluster id
	ClusterId newId = configuration.nodeId + ((this->connectionLossCounter + randomBootNumber) << 16);

	logt("NODE", "New cluster id generated %x", newId);
	return newId;
}

bool Node::GetKey(u32 fmKeyId, u8* keyOut) const
{
	if(fmKeyId == FM_KEY_ID_NODE){
		memcpy(keyOut, RamConfig->nodeKey, 16);
		return true;
	} else if(fmKeyId == FM_KEY_ID_NETWORK){
		memcpy(keyOut, GS->node->configuration.networkKey, 16);
		return true;
	} else if(fmKeyId == FM_KEY_ID_ORGANIZATION){
		memcpy(keyOut, GS->node->configuration.organizationKey, 16);
		return true;
	} else if(fmKeyId >= FM_KEY_ID_USER_DERIVED_START && fmKeyId <= FM_KEY_ID_USER_DERIVED_END){
		//Construct some cleartext with the user id to construct the user key
		u8 cleartext[16];
		memset(cleartext, 0x00, 16);
		memcpy(cleartext, &fmKeyId, 4);

		Utility::Aes128BlockEncrypt(
				(Aes128Block*)cleartext,
				(Aes128Block*)GS->node->configuration.userBaseKey,
				(Aes128Block*)keyOut);

		return true;
	} else {
		return false;
	}
}

Module* Node::GetModuleById(moduleID id) const
{
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(GS->activeModules[i] != nullptr && GS->activeModules[i]->moduleId == id){
			return GS->activeModules[i];
		}
	}
	return nullptr;
}

void Node::PrintStatus(void) const
{
	u32 err;

	fh_ble_gap_addr_t p_addr;
	err = FruityHal::BleGapAddressGet(&p_addr);

	trace("**************" EOL);
	trace("Node %s (nodeId: %u) vers: %u, NodeKey: %02X:%02X:....:%02X:%02X" EOL EOL, RamConfig->serialNumber, configuration.nodeId, fruityMeshVersion,
			RamConfig->nodeKey[0], RamConfig->nodeKey[1], RamConfig->nodeKey[14], RamConfig->nodeKey[15]);
	SetTerminalTitle();
	trace("Mesh clusterSize:%u, clusterId:%u" EOL, clusterSize, clusterId);
	trace("Enrolled %u: networkId:%u, deviceType:%u, NetKey %02X:%02X:....:%02X:%02X, UserBaseKey %02X:%02X:....:%02X:%02X" EOL,
			configuration.enrollmentState, configuration.networkId, configuration.deviceType,
			configuration.networkKey[0], configuration.networkKey[1], configuration.networkKey[14], configuration.networkKey[15],
			configuration.userBaseKey[0], configuration.userBaseKey[1], configuration.userBaseKey[14], configuration.userBaseKey[15]);
	trace("Addr:%02X:%02X:%02X:%02X:%02X:%02X, ConnLossCounter:%u, AckField:%u, State: %u" EOL EOL,
			p_addr.addr[5], p_addr.addr[4], p_addr.addr[3], p_addr.addr[2], p_addr.addr[1], p_addr.addr[0],
			connectionLossCounter, currentAckId, currentDiscoveryState);

	//Print connection info
	BaseConnections conns = GS->cm->GetBaseConnections(ConnectionDirection::INVALID);
	trace("CONNECTIONS %u (freeIn:%u, freeOut:%u, pendingPackets:%u" EOL, conns.count, GS->cm->freeMeshInConnections, GS->cm->freeMeshOutConnections, GS->cm->GetPendingPackets());
	for (u32 i = 0; i < conns.count; i++) {
		BaseConnection *conn = GS->cm->allConnections[conns.connectionIndizes[i]];
		conn->PrintStatus();
	}
	trace("**************" EOL);
}

void Node::SetTerminalTitle() const
{
#ifdef SET_TERMINAL_TITLE
	//Change putty terminal title
	if(Config->terminalMode == TerminalMode::TERMINAL_PROMPT_MODE) trace("\033]0;Node %u (%s) ClusterSize:%d (%x), [%u, %u, %u, %u]\007",
			configuration.nodeId,
			RamConfig->serialNumber,
			clusterSize, clusterId,
			GS->cm->allConnections[0] != nullptr ? GS->cm->allConnections[0]->partnerId : 0,
			GS->cm->allConnections[1] != nullptr ? GS->cm->allConnections[1]->partnerId : 0,
			GS->cm->allConnections[2] != nullptr ? GS->cm->allConnections[2]->partnerId : 0,
			GS->cm->allConnections[3] != nullptr ? GS->cm->allConnections[3]->partnerId : 0);
#endif
}

void Node::PrintBufferStatus(void) const
{
	//Print JOIN_ME buffer
	trace("JOIN_ME Buffer:" EOL);
	for (int i = 0; i < joinMePackets.length; i++)
	{
		const joinMeBufferPacket* packet = &joinMePackets[i];
		trace("=> %d, clstId:%u, clstSize:%d, freeIn:%u, freeOut:%u, writeHndl:%u, ack:%u, rssi:%d, ageDs:%d", packet->payload.sender, packet->payload.clusterId, packet->payload.clusterSize, packet->payload.freeMeshInConnections, packet->payload.freeMeshOutConnections, packet->payload.meshWriteHandle, packet->payload.ackField, packet->rssi, GS->appTimerDs - packet->receivedTimeDs);
		if (packet->connectable == BLE_GAP_ADV_TYPE_ADV_IND)
		trace(" ADV_IND" EOL);
		else if (packet->connectable == BLE_GAP_ADV_TYPE_ADV_NONCONN_IND)
		trace(" NON_CONN" EOL);
		else
		trace(" OTHER" EOL);
	}

	trace("**************" EOL);
}


/*
 #########################################################################################################
 ### Terminal Methods
 #########################################################################################################
 */

#ifdef TERMINAL_ENABLED
bool Node::TerminalCommandHandler(char* commandArgs[], u8 commandArgsSize)
{
	//React on commands, return true if handled, false otherwise
	if(commandArgsSize >= 3 && TERMARGS(2 , "node"))
	{
		if(TERMARGS(0 ,"action"))
		{
			//Rewrite "this" to our own node id, this will actually build the packet
			//But reroute it to our own node
			NodeId destinationNode = (TERMARGS(1 ,"this")) ? configuration.nodeId : atoi(commandArgs[1]);

			if(commandArgsSize >= 5 && TERMARGS(3 ,"discovery"))
			{
				u8 discoveryState = (TERMARGS(4 , "off")) ? 0 : 1;

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					(u8)NodeModuleTriggerActionMessages::SET_DISCOVERY,
					0,
					&discoveryState,
					1,
					false
				);

				return true;
			}
			//Send a reset command to a node in the mesh, it will then reboot
			if(commandArgsSize > 3 && TERMARGS(3 ,"reset"))
			{
				NodeModuleResetMessage data;
				data.resetSeconds = commandArgsSize > 4 ? atoi(commandArgs[4]) : 10;

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					(u8)NodeModuleTriggerActionMessages::RESET_NODE,
					0,
					(u8*)&data,
					SIZEOF_NODE_MODULE_RESET_MESSAGE,
					false
				);

				return true;
			}
		}
	}

	/************* SYSTEM ***************/
	if (TERMARGS(0 ,"reset"))
	{
		//Do not reboot in safe mode
		*GS->rebootMagicNumberPtr = REBOOT_MAGIC_NUMBER;
		FruityHal::SystemReset(); //OK

		return true;
	}
	/************* NODE ***************/
	//Get a full status of the node
#ifndef SAVE_SPACE_GW_1
	else if (TERMARGS(0, "status"))
	{
		PrintStatus();

		return true;
	}
	//Allows us to send arbitrary mesh packets
	else if (TERMARGS(0, "rawsend") && commandArgsSize > 1){
		DYNAMIC_ARRAY(buffer, 200);
		u32 len = Logger::parseHexStringToBuffer(commandArgs[1], buffer, 200);

		//TODO: We could optionally allow to specify delivery priority and reliability

		GS->cm->SendMeshMessage(buffer, len, DeliveryPriority::LOW, false, true);

		return true;
	}
#endif
	else if (commandArgsSize >= 5 && commandArgsSize <= 6 && TERMARGS(0, "raw_data_light"))
	{
		//Command description
		//Index               0           1                2               3           4            5       
		//Name        raw_data_light [receiverId] [destinationModule] [protocolId] [payload] {requestHandle}
		//Type             string        u16              u8               u8      hexstring       u8       

		alignas(RawDataLight) u8 buffer[80];
		memset(&buffer, 0, sizeof(buffer));
		RawDataLight& paket = (RawDataLight&)buffer;

		if (commandArgsSize >= 6)
		{
			paket.requestHandle = atoi(commandArgs[5]);
		}

		paket.connHeader.messageType = MESSAGE_TYPE_MODULE_RAW_DATA_LIGHT;
		paket.connHeader.sender = configuration.nodeId;
		paket.connHeader.receiver = atoi(commandArgs[1]);

		paket.moduleId = atoi(commandArgs[2]);
		paket.protocolId = static_cast<RawDataProtocol>(atoi(commandArgs[3]));

		u32 payloadLength = GS->logger->parseHexStringToBuffer(commandArgs[4], paket.payload, sizeof(buffer) - sizeof(RawDataLight) + 1);

		//Let's do some sanity checks!
		if (payloadLength == 0)	//Nothing to send
			return false;

		GS->cm->SendMeshMessage(
			buffer,
			sizeof(RawDataLight) - 1 + payloadLength,
			DeliveryPriority::LOW,
			false);

		return true;
	}
	//Send some large data that is split over a few messages
	else if(commandArgsSize >= 5 && commandArgsSize <= 6 && TERMARGS(0, "raw_data_start"))
	{
		//Command description
		//Index            0              1                2               3           4             5
		//Name        raw_data_start [receiverId] [destinationModule] [numChunks] [protocolId] {requestHandle}
		//Type          string           u16              u8              u24          u8            u8

		RawDataStart paket;
		memset(&paket, 0, sizeof(paket));
		if (!createRawHeader(&paket.header, RawDataActionType::START, commandArgs, commandArgsSize >= 6 ? commandArgs[5] : nullptr))
			return false;

		paket.numChunks   = atoi(commandArgs[3]);
		paket.protocolId = (u32)static_cast<RawDataProtocol>(atoi(commandArgs[4]));

		//paket.reserved;    Leave zero

		GS->cm->SendMeshMessage(
			(u8*)&paket,
			sizeof(RawDataStart),
			DeliveryPriority::LOW,
			false);

		return true;
	}
	else if (commandArgsSize >= 5 && commandArgsSize <= 6 && TERMARGS(0, "raw_data_error"))
	{
		//Command description
		//Index               0            1               2                3           4              5
		//Name        raw_data_error [receiverId] [destinationModule] [errorCode] [destination] {requestHandle}
		//Type             string         u16             u8               u8          u8             u8
		
		//Let's do some sanity checks!
		if (atoi(commandArgs[1]) < 0 || atoi(commandArgs[1]) > 65535) //Receiver malformed
			return false;
		if (atoi(commandArgs[2]) < 0 || atoi(commandArgs[2]) > 255) //Destination malformed
			return false;
		if (atoi(commandArgs[3]) < 0 || atoi(commandArgs[3]) > 255) //error code malformed
			return false;
		if (atoi(commandArgs[4]) < 1 || atoi(commandArgs[4]) > 3) //destination malformed
			return false;
		if (commandArgsSize >= 6 && (atoi(commandArgs[5]) < 0 || atoi(commandArgs[5]) > 255)) //Request Handle malformed
			return false;

		u8 requestHandle = 0;
		if (commandArgsSize >= 6) 
		{
			requestHandle = atoi(commandArgs[5]);
		}
		sendRawError(atoi(commandArgs[1]), atoi(commandArgs[2]), (RawDataErrorType)atoi(commandArgs[3]), (RawDataErrorDestination)atoi(commandArgs[4]), requestHandle);

		return true;

	}
	else if (commandArgsSize >= 3 && commandArgsSize <= 4 && TERMARGS(0, "raw_data_start_received"))
	{
		//Command description
		//Index                  0                 1                2                 3
		//Name        raw_data_start_received [receiverId] [destinationModule] {requestHandle}
		//Type                string              u16              u8                 u8

		RawDataStartReceived paket;
		memset(&paket, 0, sizeof(paket));
		if (!createRawHeader(&paket.header, RawDataActionType::START_RECEIVED, commandArgs, commandArgsSize >= 4 ? commandArgs[3] : nullptr))
			return false;

		GS->cm->SendMeshMessage(
			(u8*)&paket,
			sizeof(RawDataStartReceived),
			DeliveryPriority::LOW,
			false);

		return true;
	}
	else if (commandArgsSize >= 5 && commandArgsSize <= 6 && TERMARGS(0, "raw_data_chunk"))
	{
		//Command description
		//Index               0           1                2              3         4            5       
		//Name        raw_data_chunk [receiverId] [destinationModule] [chunkId] [payload] {requestHandle}
		//Type             string        u16              u8             u24    hexstring       u8       

		alignas(RawDataChunk) u8 buffer[80];
		memset(&buffer, 0, sizeof(buffer));
		RawDataChunk& paket = (RawDataChunk&)buffer;
		if (!createRawHeader(&paket.header, RawDataActionType::CHUNK, commandArgs, commandArgsSize >= 6 ? commandArgs[5] : nullptr))
			return false;

		paket.chunkId = atoi(commandArgs[3]);
		//paket.reserved;    Leave zero

		u32 payloadLength = GS->logger->parseHexStringToBuffer(commandArgs[4], paket.payload, sizeof(buffer) - sizeof(RawDataChunk) + 1);

		//Let's do some sanity checks!
		if (payloadLength == 0)	//Nothing to send
			return false;
		if (((strlen(commandArgs[4]) + 1) / 3) > MAX_RAW_CHUNK_SIZE)	//Msg too long
			return false;

		GS->cm->SendMeshMessage(
			buffer,
			sizeof(RawDataChunk) - 1 + payloadLength,
			DeliveryPriority::LOW,
			false);

		return true;
	}
	else if (commandArgsSize >= 4 && commandArgsSize <= 5 && TERMARGS(0, "raw_data_report"))
	{
		RawDataReport paket;
		memset(&paket, 0, sizeof(paket));
		if (!createRawHeader(&paket.header, RawDataActionType::REPORT, commandArgs, commandArgsSize >= 5 ? commandArgs[4] : nullptr))
			return false;

		if (strcmp(commandArgs[3], "-") != 0) 
		{
			char *missings[sizeof(paket.missings) / sizeof(paket.missings[0])] = {};
			missings[0] = commandArgs[3];
			char* readPtr = commandArgs[3] + 1;
			int missingIndex = 1;
			while (*readPtr != '\0')
			{
				if (*readPtr == ',')
				{
					if (missingIndex == sizeof(paket.missings) / sizeof(paket.missings[0])) //Too many missings
					{
						return false;
					}
					*readPtr = '\0';
					missings[missingIndex] = readPtr + 1;
					missingIndex++;
				}
				readPtr++;
			}

			for (u32 i = 0; i < sizeof(paket.missings) / sizeof(paket.missings[0]); i++)
			{
				if (missings[i] != nullptr)
				{
					paket.missings[i] = atoi(missings[i]);
				}
			}
		}

		GS->cm->SendMeshMessage(
			(u8*)&paket,
			sizeof(RawDataReport),
			DeliveryPriority::LOW,
			false);

		return true;
	}
	//Set a timestamp for this node
	else if (TERMARGS(0, "settime") && commandArgsSize >= 2)
	{
		//Set the time for our node
		GS->globalTimeSec = atoi(commandArgs[1]);
		GS->globalTimeRemainderTicks = 0;
		GS->timeWasSet = true;

		return true;
	}
	//Display the time of this node
	else if(TERMARGS(0, "gettime"))
	{
		char timestring[80];
		GS->logger->convertTimestampToString(GS->globalTimeSec, GS->globalTimeRemainderTicks, timestring);

		if (GS->timeWasSet)
		{
			trace("Time is currently %s" EOL, timestring);		
		}
		else
		{
			trace("Time is currently not set: %s" EOL, timestring);	
		}
		return true;
	}
	//Generate a timestamp packet and send it to all other nodes
	else if (TERMARGS(0, "sendtime"))
	{
		connPacketUpdateTimestamp packet;

		packet.header.messageType = MESSAGE_TYPE_UPDATE_TIMESTAMP;
		packet.header.sender = configuration.nodeId;
		packet.header.receiver = 0;

		//Data must not be filled because it is set in the fillTransmitBuffers method
		//Because it might still take some time from filling the buffer to sending the packet
		//We should use the radio event or estimate the sending based on the connetion parameters

		//It is then received and processed in the Connectionmanager::messageReceivedCallback

		GS->cm->SendMeshMessage((u8*)&packet, SIZEOF_CONN_PACKET_UPDATE_TIMESTAMP,DeliveryPriority::HIGH, true);

		return true;

	}

	else if (TERMARGS(0, "startterm"))
	{
		Config->terminalMode = TerminalMode::TERMINAL_PROMPT_MODE;
		return true;
	}
	else if (TERMARGS(0, "stopterm"))
	{
		Config->terminalMode = TerminalMode::TERMINAL_JSON_MODE;
		return true;
	}


	/************* Debug commands ***************/
#ifndef SAVE_SPACE_1
	//Print the JOIN_ME buffer
	else if (TERMARGS(0, "bufferstat"))
	{
		PrintBufferStatus();
		return true;
	}
	//Send some large data that is split over a few messages
	else if(TERMARGS(0, "datal"))
	{
		bool reliable = (commandArgsSize > 1 && TERMARGS(1 ,"r"));

		const u8 dataLength = 145;
		u8 _packet[dataLength];
		connPacketHeader* packet = (connPacketHeader*)_packet;
		packet->messageType = MESSAGE_TYPE_DATA_1;
		packet->receiver = 0;
		packet->sender = configuration.nodeId;

		for(u32 i=0; i< dataLength-5; i++){
			_packet[i+5] = i+1;
		}

		GS->cm->SendMeshMessage(_packet, dataLength, DeliveryPriority::LOW, reliable);

		return true;
	}
#ifndef SAVE_SPACE_GW_1
	//Stop the state machine
	else if (TERMARGS(0, "stop"))
	{
		DisableStateMachine(true);
		logt("NODE", "Stopping state machine.");
		return true;
	}
	//Start the state machine
	else if (TERMARGS(0, "start"))
	{
		DisableStateMachine(false);
		logt("NODE", "Starting state machine.");

		return true;
	}
	//This variable can be used to toggle conditional breakpoints
	else if (TERMARGS(0, "break"))
	{
		Config->breakpointToggleActive = !Config->breakpointToggleActive;

		return true;
	}
#endif
	//Try to connect to one of the nodes in the test devices array
	else if (TERMARGS(0, "connect"))
	{
	if(commandArgsSize <= 2) return false;

#ifdef ENABLE_TEST_DEVICES
		for (int i = 0; i <NUM_TEST_DEVICES ; i++)
		{
			if (strtol(commandArgs[1], nullptr, 10) == Config->testDevices[i].id)
			{
				logt("NODE", "Trying to connecting to node %d", Config->testDevices[i].id);

				cm->ConnectAsMaster(Config->testDevices[i].id, &Config->testDevices[i].addr, 14);
			}
		}
#else
		//Allows us to connect to any node when giving the GAP Address
		NodeId partnerId = atoi(commandArgs[1]);
		u8 buffer[6];
		GS->logger->parseHexStringToBuffer(commandArgs[2], buffer, 6);
		fh_ble_gap_addr_t addr;
		addr.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
		addr.addr[0] = buffer[5];
		addr.addr[1] = buffer[4];
		addr.addr[2] = buffer[3];
		addr.addr[3] = buffer[2];
		addr.addr[4] = buffer[1];
		addr.addr[5] = buffer[0];

		//Using the same GATT handle as our own will probably work if our partner has the same implementation
		GS->cm->ConnectAsMaster(partnerId, &addr, meshService.sendMessageCharacteristicHandle.value_handle, MSEC_TO_UNITS(10, UNIT_1_25_MS));

		return true;
#endif
	}
#endif

#ifndef SAVE_SPACE_1
	//Disconnect a connection by its handle or all
	else if (TERMARGS(0, "disconnect"))
	{
		if(commandArgsSize <= 1) return false;
		if(TERMARGS(1 , "all")){
			GS->cm->ForceDisconnectAllConnections(AppDisconnectReason::USER_REQUEST);
		} else {
			BaseConnection* conn = GS->cm->GetConnectionFromHandle(atoi(commandArgs[1]));
			if(conn != nullptr){
				conn->DisconnectAndRemove();
			}
		}

		return true;
	}
	//tell the gap layer to loose a connection
	else if (TERMARGS(0, "gap_disconnect"))
	{
		if(commandArgsSize <= 1) return false;
		u8 connectionId = atoi(commandArgs[1]);
		if (connectionId < sizeof(GS->cm->allConnections) / sizeof(GS->cm->allConnections[0]) && GS->cm->allConnections[connectionId] != nullptr) {
			GS->gapController->disconnectFromPartner(GS->cm->allConnections[connectionId]->connectionHandle);
		}
		return true;
	}
	else if(TERMARGS(0, "update_iv"))
	{
		if(commandArgsSize <= 2) return false;

		NodeId nodeId = atoi(commandArgs[1]);
		u16 newConnectionInterval = atoi(commandArgs[2]);

		connPacketUpdateConnectionInterval packet;
		packet.header.messageType = MESSAGE_TYPE_UPDATE_CONNECTION_INTERVAL;
		packet.header.sender = GS->node->configuration.nodeId;
		packet.header.receiver = nodeId;

		packet.newInterval = newConnectionInterval;
		GS->cm->SendMeshMessage((u8*)&packet, SIZEOF_CONN_PACKET_UPDATE_CONNECTION_INTERVAL, DeliveryPriority::HIGH, false);

		return true;
	}
#endif
	/************* UART COMMANDS ***************/
	//Get the status information of this node
	else if(TERMARGS(0, "get_plugged_in"))
	{
		#ifndef ENABLE_FAKE_NODE_POSITIONS
			logjson("NODE", "{\"type\":\"plugged_in\",\"nodeId\":%u,\"serialNumber\":\"%s\"}" SEP, configuration.nodeId, RamConfig->serialNumber);
		#else
			u8 xM = 0;
			u8 yM = 0;

			//Get the record with all fake beacon positions
			sizedData data = GS->recordStorage->GetRecordData(RECORD_STORAGE_RECORD_ID_FAKE_NODE_POSITIONS);
			FakeNodePositionRecord* record = (FakeNodePositionRecord*) data.data;
			if(data.length != 0){

				//Get our own position and that of our partner
				FakeNodePositionRecordEntry* ownEntry = nullptr;

				fh_ble_gap_addr_t own_addr;
				FruityHal::BleGapAddressGet(&own_addr);

				for(u32 i=0; i<record->count; i++){
					 if(memcmp(&record->entries[i].addr, &own_addr, sizeof(ble_gap_addr_t)) == 0){
						ownEntry = record->entries + i;
						xM = ownEntry->xM;
						yM = ownEntry->yM;
					}
				}
			}

			logjson("NODE", "{\"type\":\"plugged_in\",\"nodeId\":%u,\"serialNumber\":\"%s\",\"xM\":%u,\"yM\":%u}" SEP, configuration.nodeId, RamConfig->serialNumber, xM, yM);
		#endif

		return true;
	}
#ifndef SAVE_SPACE_1
	//Query all modules from any node
	else if((TERMARGS(0, "get_modules")))
	{
		if(commandArgsSize <= 1) return false;

		NodeId receiver = (TERMARGS(1 ,"this")) ? configuration.nodeId : atoi(commandArgs[1]);

		connPacketModule packet;
		packet.header.messageType = MESSAGE_TYPE_MODULE_CONFIG;
		packet.header.sender = configuration.nodeId;
		packet.header.receiver = receiver;

		packet.moduleId = moduleID::NODE_ID;
		packet.requestHandle = 0;
		packet.actionType = (u8)Module::ModuleConfigMessages::GET_MODULE_LIST;

		GS->cm->SendMeshMessage((u8*) &packet, SIZEOF_CONN_PACKET_MODULE, DeliveryPriority::LOW, true);

		return true;
	}
#endif
#ifndef SAVE_SPACE_GW_1
	else if(TERMARGS(0, "sep")){
		trace(EOL);
		for(u32 i=0; i<80*5; i++){
			if(i%80 == 0) trace(EOL);
			trace("#");
		}
		trace(EOL);
		trace(EOL);
		return true;
	}
#endif

	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif

void Node::ButtonHandler(u8 buttonId, u32 holdTimeDs)
{

}

inline void Node::SendModuleList(NodeId toNode, u8 requestHandle) const
{
u8 buffer[SIZEOF_CONN_PACKET_MODULE + (MAX_MODULE_COUNT+1)*4];
		memset(buffer, 0, sizeof(buffer));

		connPacketModule* outPacket = (connPacketModule*)buffer;
		outPacket->header.messageType = MESSAGE_TYPE_MODULE_CONFIG;
		outPacket->header.sender = configuration.nodeId;
		outPacket->header.receiver = toNode;

		outPacket->moduleId = moduleID::NODE_ID;
		outPacket->requestHandle = requestHandle;
		outPacket->actionType = (u8)Module::ModuleConfigMessages::MODULE_LIST;

		for(int i = 0; i<MAX_MODULE_COUNT; i++){
			if(GS->activeModules[i] != nullptr){
				//TODO: can we do this better? the data region is unaligned in memory
				memcpy(outPacket->data + i*4, &GS->activeModules[i]->configurationPointer->moduleId, 2);
				memcpy(outPacket->data + i*4 + 2, &GS->activeModules[i]->configurationPointer->moduleVersion, 1);
				memcpy(outPacket->data + i*4 + 3, &GS->activeModules[i]->configurationPointer->moduleActive, 1);
			}
		}

		GS->cm->SendMeshMessage(
				(u8*)outPacket,
				SIZEOF_CONN_PACKET_MODULE + (MAX_MODULE_COUNT+1)*4,
				DeliveryPriority::LOW,
				true);
}

#ifdef ENABLE_FAKE_NODE_POSITIONS
void Node::modifyEventForFakePositions(ble_evt_t* bleEvent) const
{
	if(bleEvent->header.evt_id != BLE_GAP_EVT_ADV_REPORT) return;
	//TODO: Implement for connection rssi as well, but we need to get the partner address from our implementation

	//Get the record with all fake beacon positions
	sizedData data = GS->recordStorage->GetRecordData(RECORD_STORAGE_RECORD_ID_FAKE_NODE_POSITIONS);
	FakeNodePositionRecord* record = (FakeNodePositionRecord*) data.data;
	if(data.length == 0) return;

	//Get our own position and that of our partner
	FakeNodePositionRecordEntry* ownEntry = nullptr;
	FakeNodePositionRecordEntry* partnerEntry = nullptr;

	fh_ble_gap_addr_t own_addr;
	FruityHal::BleGapAddressGet(&own_addr);

	for(u32 i=0; i<record->count; i++){
		if (memcmp(&record->entries[i].addr, &bleEvent->evt.gap_evt.params.adv_report.peer_addr, sizeof(ble_gap_addr_t)) == 0){
			partnerEntry = record->entries + i;
		} else if(memcmp(&record->entries[i].addr, &own_addr, sizeof(ble_gap_addr_t)) == 0){
			ownEntry = record->entries + i;
		}
	}

	//If no data is available either about us or our partner, do not modify the event
	if(ownEntry == nullptr || partnerEntry == nullptr){
		return;
	}

	//Calculate the RSSI based on the distance
	double N = 2.5;
	double dist = sqrt(  pow((double)(ownEntry->xM) - (double)(partnerEntry->xM), (double)2) + pow((double)(ownEntry->yM) - (double)(partnerEntry->yM), (double)2)  );

	if(dist > 40){
		//logt("NODE", "Made BLE_GAP_EVT_ADV_REPORT invalid because of fake position.");
		//Modify the event so that it does not get processed
		bleEvent->header.evt_id = BLE_EVT_INVALID;
	} else {
		//Modify the event with the new rssi
		i8 rssi = (double)((i32)-40 + Config->defaultDBmTX) - log10(dist) * 10 * N;
		//logt("NODE", "Modified BLE_GAP_EVT_ADV_REPORT with rssi %d at distance %d", rssi, (int)dist);

		bleEvent->evt.gap_evt.params.adv_report.rssi = rssi;
	}

}
#endif

void Node::sendRawError(NodeId receiver, u8 moduleId, RawDataErrorType type, RawDataErrorDestination destination, u8 requestHandle) const
{
	RawDataError paket;
	memset(&paket, 0, sizeof(paket));

	paket.header.connHeader.messageType = MESSAGE_TYPE_MODULE_RAW_DATA;
	paket.header.connHeader.sender = configuration.nodeId;
	paket.header.connHeader.receiver = receiver;

	paket.header.moduleId = moduleId;
	paket.header.actionType = RawDataActionType::ERROR_T;
	paket.header.requestHandle = requestHandle;

	paket.type = type;
	paket.destination = destination;

	GS->cm->SendMeshMessage(
		(u8*)&paket,
		sizeof(RawDataError),
		DeliveryPriority::LOW,
		false);
}

bool Node::createRawHeader(RawDataHeader* outVal, RawDataActionType type, char* commandArgs[], char* requestHandle) const
{
	if (requestHandle != nullptr)
	{
		outVal->requestHandle = atoi(requestHandle);
	}

	outVal->connHeader.messageType = MESSAGE_TYPE_MODULE_RAW_DATA;
	outVal->connHeader.sender = configuration.nodeId;
	outVal->connHeader.receiver = atoi(commandArgs[1]);

	outVal->moduleId = atoi(commandArgs[2]);
	outVal->actionType = type;


	return true;
}

void Node::Reboot(u32 delayDs)
{
	rebootTimeDs = GS->appTimerDs + delayDs;
}

/* EOF */
