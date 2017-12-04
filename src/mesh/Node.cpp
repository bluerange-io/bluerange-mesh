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

#include <Config.h>

#include <Node.h>
#include <LedWrapper.h>
#include <SimpleBuffer.h>
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
#include <IoModule.h>
#include <MeshConnection.h>
#include <conn_packets.h>

#ifdef ACTIVATE_DFU_MODULE
#include <DFUModule.h>
#endif

#ifdef ACTIVATE_CLC_MODULE
#include <ClcModule.h>
#endif

extern "C"
{
#include <time.h>
#include <stdlib.h>
#include <nrf_soc.h>
#include <app_error.h>
#include <app_timer.h>
#include <ble_hci.h>
#ifndef SIM_ENABLED
#include <nrf_nvic.h>
#endif
}


Node::Node()
{

}

void Node::Initialize()
{
	LoadDefaults();

	Conf::LoadSettingsFromFlash(moduleID::NODE_ID, &persistentConfig, sizeof(NodeConfiguration));

	//Initialize variables
	GS->node = this;

	this->clusterId = 0;
	this->clusterSize = 1;

	memset(this->raw_joinMePacketBuffer, 0x00, sizeof(raw_joinMePacketBuffer));

	this->currentAckId = 0;

	this->noNodesFoundCounter = 0;
	this->passsedTimeSinceLastTimerHandlerDs = 0;

	this->outputRawData = false;

	this->radioActiveCount = 0;

	meshAdvJobHandle = NULL;

	globalTimeSec = 0;
	globalTimeRemainderTicks = 0;
	previousRtcTicks = 0;

	//Set the current state and its timeout
	currentStateTimeoutDs = 0;
	currentDiscoveryState = discoveryState::BOOTUP;
	nextDiscoveryState = discoveryState::INVALID_STATE;
	this->appTimerDs = 0;
	this->lastDecisionTimeDs = 0;
	this->appTimerRandomOffsetDs = 0;

	initializedByGateway = false;

	//Register terminal listener
	Terminal::getInstance()->AddTerminalCommandListener(this);

	//Receive ConnectionManager events
	GS->cm = ConnectionManager::getInstance();

	ConfigurationLoadedHandler();

	//Initialize all Modules
	//Module ids start with 1, this id is also used for saving persistent
	//module configurations with the Storage class
	//Module ids must persist when nodes are updated to guearantee that the
	//same module receives the same storage slot
#ifdef ACTIVATE_DEBUG_MODULE
	activeModules[0] = new DebugModule(moduleID::DEBUG_MODULE_ID, this, GS->cm, "debug");
#endif
#ifdef ACTIVATE_DFU_MODULE
	activeModules[1] = new DFUModule(moduleID::DFU_MODULE_ID, this, GS->cm, "dfu");
#endif
#ifdef ACTIVATE_STATUS_REPORTER_MODULE
	activeModules[2] = new StatusReporterModule(moduleID::STATUS_REPORTER_MODULE_ID, this, GS->cm, "status");
#endif
#ifdef ACTIVATE_ADVERTISING_MODULE
	activeModules[3] = new AdvertisingModule(moduleID::ADVERTISING_MODULE_ID, this, GS->cm, "adv");
#endif
#ifdef ACTIVATE_SCANNING_MODULE
	activeModules[4] = new ScanningModule(moduleID::SCANNING_MODULE_ID, this, GS->cm, "scan");
#endif
#ifdef ACTIVATE_ENROLLMENT_MODULE
	activeModules[5] = new EnrollmentModule(moduleID::ENROLLMENT_MODULE_ID, this, GS->cm, "enroll");
#endif
#ifdef ACTIVATE_IO_MODULE
	activeModules[6] = new IoModule(moduleID::IO_MODULE_ID, this, GS->cm, "io");
#endif
#ifdef ACTIVATE_CLC_MODULE
	activeModules[7] = new ClcModule(moduleID::CLC_MODULE_ID, this, GS->cm, "clc");
#endif

	joinMePacketBuffer = new SimpleBuffer((u8*) raw_joinMePacketBuffer, sizeof(joinMeBufferPacket) * JOIN_ME_PACKET_BUFFER_MAX_ELEMENTS, sizeof(joinMeBufferPacket));

	//Fills buffer with empty packets
	for(int i=0; i<JOIN_ME_PACKET_BUFFER_MAX_ELEMENTS; i++){
		joinMePacketBuffer->Reserve();
	}

	Logger::getInstance()->logError(Logger::errorTypes::CUSTOM, Logger::customErrorTypes::REBOOT, 0);

}

void Node::LoadDefaults()
{
	persistentConfig.moduleId = moduleID::NODE_ID;
	persistentConfig.moduleVersion = 1;
	persistentConfig.moduleActive = 1;

	//TODO: Do not copy settings to node
	persistentConfig.networkId = Config->meshNetworkIdentifier;
	memcpy(&persistentConfig.networkKey, &Config->meshNetworkKey, 16);
	persistentConfig.dBmTX = Config->defaultDBmTX;

	persistentConfig.nodeId = Config->defaultNodeId;
	persistentConfig.networkId = Config->meshNetworkIdentifier;

	persistentConfig.deviceType = Config->deviceType;
	persistentConfig.nodeId = Config->defaultNodeId;

	memcpy(&persistentConfig.nodeAddress, &Config->staticAccessAddress, sizeof(ble_gap_addr_t));

}

void Node::ConfigurationLoadedHandler()
{
	u32 err;

	//Check if some of the values are not set, use the ones from UICR
	if(persistentConfig.networkId == 0xFFFF) persistentConfig.networkId = Config->meshNetworkIdentifier;
	bool allBitsSet = true;
	for(int i=0; i<16; i++){
		if(persistentConfig.networkKey[i] != 0xFF){
			allBitsSet = false;
		}
	}
	if(allBitsSet) memcpy(&persistentConfig.networkKey, &Config->meshNetworkKey, 16);
	if(persistentConfig.nodeId == 0xFFFF) persistentConfig.nodeId = Config->defaultNodeId;
	if(persistentConfig.deviceType == 0xFF) persistentConfig.deviceType = Config->deviceType;


	//Random offset that can be used to disperse packets from different nodes over time
	this->appTimerRandomOffsetDs = (persistentConfig.nodeId % 100);

	//Change window title of the Terminal
	SetTerminalTitle();
	logt("NODE", "====> Node %u (%s) <====", persistentConfig.nodeId, Config->serialNumber);

	//Get a random number for the connection loss counter (hard on system start,...stat)
	connectionLossCounter = Utility::GetRandomInteger();

	clusterId = this->GenerateClusterID();

	//Set the BLE address so that we have the same on every startup, mostly for debugging
	if(persistentConfig.nodeAddress.addr_type != 0xFF){
		err = FruityHal::BleGapAddressSet(&persistentConfig.nodeAddress);
		if(err != NRF_SUCCESS){
			//Can be ignored and will not happen
		}
	}

	//Init softdevice and c libraries
	ScanController::getInstance();

	//Set preferred TX power
	err = sd_ble_gap_tx_power_set(persistentConfig.dBmTX);

	//Print configuration and start node
	logt("NODE", "Config loaded nodeId:%d, connLossCount:%u, networkId:%d", persistentConfig.nodeId, connectionLossCounter, persistentConfig.networkId);

	//Register the mesh service in the GATT table
	InitializeMeshGattService();

	//Remove Advertising job if it's been registered before
	AdvertisingController::getInstance()->RemoveJob(meshAdvJobHandle);

	if(persistentConfig.moduleActive){
		//Register Job with AdvertisingController
		AdvJob job = {
			AdvJobTypes::ADV_JOB_TYPE_SCHEDULED,
			5, //Slots
			0, //Delay
			MSEC_TO_UNITS(100, UNIT_0_625_MS), //AdvInterval
			0, //CurrentSlots
			0, //CurrentDelay
			BLE_GAP_ADV_TYPE_ADV_IND, //Advertising Mode
			{0}, //AdvData
			0, //AdvDataLength
			{0}, //ScanData
			0 //ScanDataLength
		};
		meshAdvJobHandle = AdvertisingController::getInstance()->AddJob(&job);
	}

	//Go to Discovery if node is active
	if(persistentConfig.moduleActive != 0){
		//Fill JOIN_ME packet with data
		this->UpdateJoinMePacket();

		ChangeState(discoveryState::DISCOVERY);
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
	characteristicMetadata.p_cccd_md = NULL;

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

//TODO: Currently not called
void Node::CharacteristicsDiscoveredHandler(ble_evt_t* bleEvent)
{
	u32 err = 0;

	//Service has been found
	if (bleEvent->evt.gattc_evt.params.char_disc_rsp.count > 0)
	{
		//Characteristics have been found (FIXME: more could be requested by calling discovery again see:https://devzone.nordicsemi.com/documentation/nrf51/4.3.0/html/group___b_l_e___g_a_t_t_c___c_h_a_r___d_i_s_c___m_s_c.html)
		if (bleEvent->evt.gattc_evt.params.char_disc_rsp.chars[0].uuid.uuid == MESH_SERVICE_CHARACTERISTIC_UUID && bleEvent->evt.gattc_evt.params.char_disc_rsp.chars[0].uuid.type == meshService.serviceUuid.type)
		{
			u16 characteristicHandle = bleEvent->evt.gattc_evt.params.char_disc_rsp.chars[0].handle_value;
			logt("C", "Found mesh write characteristic");
			BaseConnection* connection = GS->cm->GetConnectionFromHandle(bleEvent->evt.gattc_evt.conn_handle);
			if (connection != NULL)
			{
				connection->GATTHandleDiscoveredHandler(characteristicHandle);
			}
		}
	}
}

/*
 #########################################################################################################
 ### Connections and Handlers
 #########################################################################################################
 */
#define ________________CONNECTION___________________
#pragma region connections

//Is called as soon as a connection is connected, before the handshake
void Node::MeshConnectionConnectedHandler()
{
	logt("NODE", "Connection initiated");

	//We are leaving the discoveryState::CONNECTING state
	ChangeState(discoveryState::HANDSHAKE);

	//FIXME: The handshake needs some kind of timeout
}

//Is called after a connection has ended its handshake
void Node::HandshakeDoneHandler(MeshConnection* connection, bool completedAsWinner)
{
	logt("HANDSHAKE", "############ Handshake done (asWinner:%u) ###############", completedAsWinner);

	Logger::getInstance()->logError(Logger::errorTypes::CUSTOM, Logger::customErrorTypes::HANDSHAKE_DONE, connection->partnerId);

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
		outPacket.header.sender = persistentConfig.nodeId;
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

	connection->connectionState = ConnectionState::CONNECTION_STATE_HANDSHAKE_DONE;
	connection->connectionHandshakedTimestampDs = appTimerDs;

	//Call our lovely modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(activeModules[i] != NULL){
			activeModules[i]->MeshConnectionChangedHandler(connection);
		}
	}

	//Update our advertisement packet
	UpdateJoinMePacket();

	//Pass on the masterbit to someone if necessary
	HandOverMasterBitIfNecessary(connection);

	//Go back to Discovery
	ChangeState(discoveryState::DISCOVERY);
}

//TODO: part of the connection manager
void Node::HandshakeTimeoutHandler()
{
	logt("HANDSHAKE", "############ Handshake TIMEOUT/FAIL ###############");

	//Disconnect the hanging connection
	BaseConnections conn = GS->cm->GetBaseConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);
	for(int i=0; i<conn.count; i++){
		if(conn.connections[i]->isConnected() && !conn.connections[i]->handshakeDone()){
			u32 handshakeTimePassed = appTimerDs - conn.connections[i]->handshakeStartedDs;
			logt("HANDSHAKE", "Disconnecting conn %u, timePassed:%u", conn.connections[i]->connectionId, handshakeTimePassed);
			conn.connections[i]->Disconnect();
		}
	}

	//Go back to discovery
	ChangeState(discoveryState::DISCOVERY);
}

//TODO: part of the connection manager
//If we wanted to connect but our connection timed out (only outgoing connections)
void Node::MeshConnectingTimeoutHandler(ble_evt_t* bleEvent)
{
	logt("NODE", "Connecting Timeout");

	//We are leaving the discoveryState::CONNECTING state
	ChangeState(discoveryState::DISCOVERY);
}

void Node::MeshConnectionDisconnectedHandler(MeshConnection* connection)
{
	//TODO: If the local host disconnected this connection, it was already increased, we do not have to count the disconnect here
	this->connectionLossCounter++;

	//If the handshake was already done, this node was part of our cluster
	//If the local host terminated the connection, we do not count it as a cluster Size change
	if (
		connection->connectionStateBeforeDisconnection >= ConnectionState::CONNECTION_STATE_HANDSHAKE_DONE
	){
		//CASE 1: if our partner has the connection master bit, we must dissolve
		//It may happen rarely that the connection master bit was just passed over and that neither node has it
		//This will result in two clusters dissolving
		if (!connection->connectionMasterBit)
		{
			GS->cm->ForceDisconnectOtherMeshConnections(NULL);

			clusterSize = 1;
			clusterId = GenerateClusterID();

			UpdateJoinMePacket();

		}

		//CASE 2: If we have the master bit, we keep our clusterID (happens if we are the biggest cluster)
		else
		{
			logt("HANDSHAKE", "ClusterSize Change from %d to %d", this->clusterSize, this->clusterSize - connection->connectedClusterSize);

			this->clusterSize -= connection->connectedClusterSize;

			// Inform the rest of the cluster of our new size
			connPacketClusterInfoUpdate packet;
			memset((u8*)&packet, 0x00, sizeof(connPacketClusterInfoUpdate));

			packet.header.messageType = MESSAGE_TYPE_CLUSTER_INFO_UPDATE;
			packet.header.sender = this->persistentConfig.nodeId;
			packet.header.receiver = NODE_ID_BROADCAST;

			packet.payload.newClusterId = connection->connectedClusterId;
			packet.payload.clusterSizeChange = -connection->connectedClusterSize;

			SendClusterInfoUpdate(connection, &packet);

		}
		//Handshake had not yet finished, not much to do
	}
	else
	{

	}

	logjson("CLUSTER", "{\"type\":\"cluster_disconnect\",\"size\":%d}" SEP, clusterSize);

	//In either case, we must update our advertising packet
	UpdateJoinMePacket();

	//Pass on the masterbit to someone if necessary
	HandOverMasterBitIfNecessary(connection);

	//Go to discovery mode, and force high mode
	noNodesFoundCounter = 0;

	//At this point we can start discovery again if we are in a stable mesh
	//That has discovery disabled.
	if(currentDiscoveryState == discoveryState::DISCOVERY_OFF){
		ChangeState(discoveryState::DISCOVERY);
	}

	//If this connection was part of the current handshake, we trigger a handshake timeout/fail
	if (
		currentDiscoveryState == discoveryState::HANDSHAKE
		&& connection->connectionStateBeforeDisconnection == ConnectionState::CONNECTION_STATE_HANDSHAKING
	) {
		ChangeState(discoveryState::HANDSHAKE_TIMEOUT);
	}
}

//Handles incoming cluster info update
void Node::ReceiveClusterInfoUpdate(MeshConnection* connection, connPacketClusterInfoUpdate* packet)
{
	//Prepare cluster update packet for other connections
	connPacketClusterInfoUpdate outPacket;
	memset((u8*)&outPacket, 0x00, sizeof(connPacketClusterInfoUpdate));

	outPacket.header.messageType = MESSAGE_TYPE_CLUSTER_INFO_UPDATE;
	outPacket.header.receiver = NODE_ID_BROADCAST;
	outPacket.header.sender = persistentConfig.nodeId;

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

	//Update adverting packet
	this->UpdateJoinMePacket();

	//TODO: What happens if:
	/*
	 * We send a clusterid update and commit it in our connection arm
	 * The other one does the same at nearly the same time
	 * ID before was 3, A now has 2 and 2 on the connection arm, B has 4 and 4 on the connection arm
	 * Then both will not accept the new clusterID!!!
	 * What if the biggest id will always win?
	 */
}

void Node::HandOverMasterBitIfNecessary(MeshConnection* connection) {
	//If we have all masterbits, we can give 1 at max
	//We do this, if the connected cluster size is bigger than all the other connected cluster sizes summed together
	bool hasAllMasterBits = HasAllMasterBits();
	if (hasAllMasterBits) {
		MeshConnections conn = GS->cm->GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);
		for (u32 i = 0; i < conn.count; i++) {
			MeshConnection* c2 = conn.connections[i];
			if (c2->connectedClusterSize > clusterSize - c2->connectedClusterSize) {
				//Remove the masterbit from this connection
				connection->connectionMasterBit = 0;
				//Put the masterbit handover in the correct packet.
				c2->currentClusterInfoUpdatePacket.header.messageType = MESSAGE_TYPE_CLUSTER_INFO_UPDATE;
				c2->currentClusterInfoUpdatePacket.payload.connectionMasterBitHandover = 1;
			}
		}
	}
}

bool Node::HasAllMasterBits() {
	MeshConnections conn = GS->cm->GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);
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
void Node::SendClusterInfoUpdate(MeshConnection* ignoreConnection, connPacketClusterInfoUpdate* packet)
{
	MeshConnections conn = GS->cm->GetMeshConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);
	for (u32 i = 0; i < conn.count; i++) {
		if(!conn.connections[i]->isConnected() || conn.connections[i] == ignoreConnection) continue;

		packet->payload.hopsToSink = GS->cm->GetMeshHopsToShortestSink(conn.connections[i]);

		//Get the current packet
		connPacketClusterInfoUpdate* currentPacket = &(conn.connections[i]->currentClusterInfoUpdatePacket);

		//If another clusterUpdate message is about to be sent
		if(currentPacket->header.messageType == MESSAGE_TYPE_CLUSTER_INFO_UPDATE){
			logt("HANDSHAKE", "TO NODE %u Adding to clusterSize change:%d, id:%u, hops:%d", conn.connections[i]->partnerId, packet->payload.clusterSizeChange, packet->payload.newClusterId, packet->payload.hopsToSink);

			currentPacket->payload.clusterSizeChange += packet->payload.clusterSizeChange;
			currentPacket->payload.newClusterId = packet->payload.newClusterId; //TODO: we could intelligently choose our clusterID
			currentPacket->payload.hopsToSink = GS->cm->GetMeshHopsToShortestSink(conn.connections[i]);

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

//Constructs a simple trigger action message and can take aditional payload data
void Node::SendModuleActionMessage(u8 messageType, nodeID toNode, u8 actionType, u8 requestHandle, u8* additionalData, u16 additionalDataSize, bool reliable)
{
	DYNAMIC_ARRAY(buffer, SIZEOF_CONN_PACKET_MODULE + additionalDataSize);

	connPacketModule* outPacket = (connPacketModule*)buffer;
	outPacket->header.messageType = messageType;
	outPacket->header.sender = persistentConfig.nodeId;
	outPacket->header.receiver = toNode;

	outPacket->moduleId = moduleID::NODE_ID;
	outPacket->requestHandle = requestHandle;
	outPacket->actionType = actionType;

	if(additionalData != NULL && additionalDataSize > 0)
	{
		memcpy(&outPacket->data, additionalData, additionalDataSize);
	}

	GS->cm->SendMeshMessage(buffer, SIZEOF_CONN_PACKET_MODULE + additionalDataSize, DeliveryPriority::DELIVERY_PRIORITY_LOW, reliable);
}

void Node::MeshMessageReceivedHandler(MeshConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader)
{
	//If the packet is a handshake packet it will not be forwarded to the node but will be
	//handled in the connection. All other packets go here for further processing
	switch (packetHeader->messageType)
	{
		case MESSAGE_TYPE_CLUSTER_INFO_UPDATE:
			if (
					connection != NULL
					&& connection->connectionType == ConnectionTypes::CONNECTION_TYPE_FRUITYMESH
					&& sendData->dataLength >= SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE)
			{
				connPacketClusterInfoUpdate* packet = (connPacketClusterInfoUpdate*) packetHeader;
				logt("HANDSHAKE", "IN <= %d CLUSTER_INFO_UPDATE newClstId:%u, sizeChange:%d, hop:%d", connection->partnerId, packet->payload.newClusterId, packet->payload.clusterSizeChange, packet->payload.hopsToSink);
				ReceiveClusterInfoUpdate((MeshConnection*)connection, packet);

			}
			break;

		case MESSAGE_TYPE_DATA_1:
			if (sendData->dataLength >= SIZEOF_CONN_PACKET_DATA_1)
			{
				connPacketData1* packet = (connPacketData1*) packetHeader;
				nodeID partnerId = connection == NULL ? 0 : connection->partnerId;

				logt("DATA", "IN <= %d ################## Got Data packet %d:%d:%d (len:%d) ##################", partnerId, packet->payload.data[0], packet->payload.data[1], packet->payload.data[2], sendData->dataLength);

				//tracef("data is %u/%u/%u" EOL, packet->payload.data[0], packet->payload.data[1], packet->payload.data[2]);
			}
			break;

		case MESSAGE_TYPE_DATA_2:
			if (sendData->dataLength >= SIZEOF_CONN_PACKET_DATA_2)
			{
				connPacketData2* packet = (connPacketData2*) packetHeader;
				nodeID partnerId = connection == NULL ? 0 : connection->partnerId;

				logt("DATA", "IN <= %d ################## Got Data 2 packet %c ##################", partnerId, packet->payload.data[0]);
			}
			break;

		case MESSAGE_TYPE_DATA_3:
			{
				connPacketData3* packet = (connPacketData3*) packetHeader;

				char dataString[16*3];
				Logger::getInstance()->convertBufferToHexString(packet->payload.data, packet->payload.len, dataString, 15*3+1);

				logjson("NODE", "{\"module\":0,\"type\":\"tunnel\",\"nodeId\":%u,\"data\":\"%s\"}" SEP,
					packet->header.sender,
					dataString
				);
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

	}

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_CONFIG)
	{
		connPacketModule* packet = (connPacketModule*) packetHeader;


		if(packet->actionType == Module::ModuleConfigMessages::GET_MODULE_LIST)
		{
			SendModuleList(packet->header.sender, packet->requestHandle);

		}
		else if(packet->actionType == Module::ModuleConfigMessages::MODULE_LIST)
		{

			logjson("MODULE", "{\"nodeId\":%u,\"type\":\"module_list\",\"modules\":[", packet->header.sender);

			u16 moduleCount = (sendData->dataLength - SIZEOF_CONN_PACKET_MODULE) / 4;
			bool first = true;
			for(int i=0; i<moduleCount; i++){
				u8 moduleId = 0, version = 0, active = 0;
				memcpy(&moduleId, packet->data + i*4+0, 1);
				memcpy(&version, packet->data + i*4+2, 1);
				memcpy(&active, packet->data + i*4+3, 1);

				if(moduleId)
				{
					//comma seperator issue,....
					if(!first){
						logjson("MODULE", ",");
					}
					logjson("MODULE", "{\"id\":%u,\"version\":%u,\"active\":%u}", moduleId, version, active);

					first = false;
				}
			}
			logjson("MODULE", "]}" SEP);
		}
	}

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleID::NODE_ID){

			if(packet->actionType == NodeModuleTriggerActionMessages::SET_DISCOVERY){

				u8 ds = packet->data[0];

				if(ds == 0){
					ChangeState(discoveryState::DISCOVERY_OFF);
				} else {
					ChangeState(discoveryState::DISCOVERY_HIGH);
				}

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
					packetHeader->sender,
					NodeModuleActionResponseMessages::SET_DISCOVERY_RESULT,
					0,
					NULL,
					0,
					false
				);
			}
		}
	}

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_ACTION_RESPONSE){
			connPacketModule* packet = (connPacketModule*)packetHeader;

			//Check if our module is meant and we should trigger an action
			if(packet->moduleId == moduleID::NODE_ID){

				if(packet->actionType == NodeModuleActionResponseMessages::SET_DISCOVERY_RESULT){

					logjson("NODE", "{\"type\":\"set_discovery_result\",\"nodeId\":%d,\"module\":%d}" SEP, packetHeader->sender, moduleID::NODE_ID);
				}
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
void Node::UpdateJoinMePacket()
{
	if(!persistentConfig.moduleActive) return;

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
	advPacket->networkId = persistentConfig.networkId;
	advPacket->messageType = MESSAGE_TYPE_JOIN_ME_V0;

	//Build a JOIN_ME packet and set it in the advertisement data
	advPacketPayloadJoinMeV0* packet = (advPacketPayloadJoinMeV0*)(bufferPointer+SIZEOF_ADV_PACKET_HEADER);
	packet->sender = this->persistentConfig.nodeId;
	packet->clusterId = this->clusterId;
	packet->clusterSize = this->clusterSize;
	packet->freeMeshInConnections = GS->cm->freeMeshInConnections;
	packet->freeMeshOutConnections = GS->cm->freeMeshOutConnections;

	packet->batteryRuntime = GetBatteryRuntime();
	packet->txPower = persistentConfig.dBmTX;
	packet->deviceType = persistentConfig.deviceType;
	packet->hopsToSink = GS->cm->GetMeshHopsToShortestSink(NULL);
	packet->meshWriteHandle = meshService.sendMessageCharacteristicHandle.value_handle;

	if (currentAckId != 0)
	{
		packet->ackField = currentAckId;

	} else {
		packet->ackField = 0;
	}

	meshAdvJobHandle->advDataLength = SIZEOF_ADV_PACKET_HEADER + SIZEOF_ADV_PACKET_PAYLOAD_JOIN_ME_V0;

	logt("JOIN", "JOIN_ME updated clusterId:%x, clusterSize:%d, freeIn:%u, freeOut:%u, handle:%u, ack:%u", packet->clusterId, packet->clusterSize, packet->freeMeshInConnections, packet->freeMeshOutConnections, packet->meshWriteHandle, packet->ackField);

	AdvertisingController::getInstance()->RefreshJob(meshAdvJobHandle);
}

//STEP 3: After collecting all available clusters, we want to connect to the best cluster that is available
//If the other clusters were not good and we have something better, we advertise it.
Node::decisionResult Node::DetermineBestClusterAvailable(void)
{
	//If no clusters have been advertised since the first time, there is no work to do
	if (joinMePacketBuffer->_numElements == 0)
	{
		logt("DECISION", "No other nodes discovered");
		return Node::DECISION_NO_NODES_FOUND;
	}

	u32 bestScore = 0;
	joinMeBufferPacket* bestCluster = NULL;
	joinMeBufferPacket* packet = NULL;

	//Determine the best Cluster to connect to as a master
	if (GS->cm->freeMeshOutConnections > 0)
	{
		for (int i = 0; i < joinMePacketBuffer->_numElements; i++)
		{
			packet = (joinMeBufferPacket*) joinMePacketBuffer->PeekItemAt(i);

			u32 score = CalculateClusterScoreAsMaster(packet);
			if (score > bestScore)
			{
				bestScore = score;
				bestCluster = packet;
			}
		}

		//Now, if we want to be a master in the connection, we simply answer the ad packet that
		//informs us about that cluster
		if (bestCluster != NULL)
		{
			currentAckId = 0;

			fh_ble_gap_addr_t address;
			address.addr_type = bestCluster->bleAddressType;
			memcpy(address.addr, bestCluster->bleAddress, BLE_GAP_ADDR_LEN);

			GS->cm->ConnectAsMaster(bestCluster->payload.sender, &address, bestCluster->payload.meshWriteHandle);

			//We clear this packet from the buffer, because if the connetion fails, we want to try a different node
			memset(bestCluster, 0x00, sizeof(joinMeBufferPacket));

			//Clear the buffer because it will have changed now
			//joinMePacketBuffer->Clean();

			return Node::DECISION_CONNECT_AS_MASTER;
		}
	}

	//If no good cluster could be found (all are bigger than mine)
	//Find the best cluster that should connect to us (we as slave)
	packet = NULL;
	for (int i = 0; i < joinMePacketBuffer->_numElements; i++)
	{
		currentAckId = 0;

		packet = (joinMeBufferPacket*) joinMePacketBuffer->PeekItemAt(i);

		u32 score = CalculateClusterScoreAsSlave(packet);
		if (score > bestScore)
		{
			bestScore = score;
			bestCluster = packet;
		}
	}

	//Set our ack field to the best Cluster
	if (bestCluster != NULL)
	{
		logt("DECISION", "Other clusters are bigger, we are going to be a slave");

		currentAckId = bestCluster->payload.clusterId;

		//CASE 1: The ack field is already set to our cluster id, we can reach each other
		//Kill connections and broadcast our preferred partner with the ack field
		//so that he connects to us
		if (bestCluster->payload.ackField == clusterId && bestCluster->payload.clusterSize >= clusterSize)
		{
			GS->cm->ForceDisconnectOtherMeshConnections(NULL);

			clusterSize = 1;
			clusterId = GenerateClusterID();

			UpdateJoinMePacket();
		}
		//CASE 2: The ack field is not set to our id, set our ack field to his id
		//And wait for him to confirm that he can reach us
		else
		{
			UpdateJoinMePacket();
		}

		//Clear the buffer because it will have changed now
		//joinMePacketBuffer->Clean();

		return Node::DECISION_CONNECT_AS_SLAVE;
	}

	logt("DECISION", "no cluster found");

	return Node::DECISION_NO_NODES_FOUND;
}

#define STABLE_CONNECTION_RSSI_THRESHOLD -85

//Calculates the score for a cluster
//Connect to big clusters but big clusters must connect nodes that are not able 
u32 Node::CalculateClusterScoreAsMaster(joinMeBufferPacket* packet)
{

	//If the packet is too old, filter it out
	if (appTimerDs - packet->receivedTimeDs > MAX_JOIN_ME_PACKET_AGE_DS) return 0;

	//If we are already connected to that cluster, the score is 0
	if (packet->payload.clusterId == this->clusterId) return 0;

	//If there are zero free in connections, we cannot connect as master
	if (packet->payload.freeMeshInConnections == 0) return 0;

	//If his cluster is bigger, but only if it is not faked (when setting an ack)
	if (packet->payload.ackField == 0 && packet->payload.clusterSize >= this->clusterSize)
	{
		return 0;
	}

	//Connection should have a minimum of stability
	if(packet->rssi < STABLE_CONNECTION_RSSI_THRESHOLD) return 0;

	//If the ack field is not 0 and set to a different nodeID than ours, somebody else wants to connect to him
	if (packet->payload.ackField != 0 && packet->payload.ackField != this->clusterId)
	{
		//Override ack field if our clustersize is similar or bigger
		if (packet->payload.clusterSize <= this->clusterSize)
		{
		}
		else
		{
			return 0;
		}
	}
	u32 rssiScore = 100 + packet->rssi;

	//Free in connections are best, free out connections are good as well
	//TODO: RSSI should be factored into the score as well, maybe battery runtime, device type, etc...
	return packet->payload.freeMeshInConnections * 10000 + packet->payload.freeMeshOutConnections * 100 + rssiScore;
}

//If there are only bigger clusters around, we want to find the best
//And set its id in our ack field
u32 Node::CalculateClusterScoreAsSlave(joinMeBufferPacket* packet)
{

	//If the packet is too old, filter it out
	if (appTimerDs - packet->receivedTimeDs > MAX_JOIN_ME_PACKET_AGE_DS) return 0;

	//If we are already connected to that cluster, the score is 0
	if (packet->payload.clusterId == this->clusterId) return 0;

	//If the ack field is set, we do not want to connect as slave
	//if (packet->payload.ackField != 0/* && packet->payload.ackField != this->clusterId*/) return 0;

	//if (packet->payload.clusterSize < this->clusterSize) return 0;

	//He could not connect to us, leave him alone
	if (packet->payload.freeMeshOutConnections == 0) return 0;

	//Connection should have a minimum of stability
	if(packet->rssi < STABLE_CONNECTION_RSSI_THRESHOLD) return 0;

	u32 rssiScore = 100 + packet->rssi;

	//Choose the one with the biggest cluster size, if there are more, prefer the most outConnections
	return packet->payload.clusterSize * 10000 + packet->payload.freeMeshOutConnections * 100 + rssiScore;
}

//All advertisement packets are received here if they are valid
void Node::AdvertisementMessageHandler(ble_evt_t* bleEvent)
{
	if(!persistentConfig.moduleActive) return;

	u8* data = bleEvent->evt.gap_evt.params.adv_report.data;
	u16 dataLength = bleEvent->evt.gap_evt.params.adv_report.dlen;

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
				advPacketJoinMeV0* packet = (advPacketJoinMeV0*) data;

				logt("DISCOVERY", "JOIN_ME: sender:%u, clusterId:%x, clusterSize:%d, freeIn:%u, freeOut:%u, ack:%u", packet->payload.sender, packet->payload.clusterId, packet->payload.clusterSize, packet->payload.freeMeshInConnections, packet->payload.freeMeshOutConnections, packet->payload.ackField);

				//Look through the buffer and determine a space where we can put the packet in
				joinMeBufferPacket* targetBuffer = findTargetBuffer(packet);

				//Now, we have the space for our packet and we fill it with the latest information
				if (targetBuffer != NULL)
				{
					memcpy(targetBuffer->bleAddress, bleEvent->evt.gap_evt.params.connected.peer_addr.addr, BLE_GAP_ADDR_LEN);
					targetBuffer->bleAddressType = bleEvent->evt.gap_evt.params.connected.peer_addr.addr_type;

					targetBuffer->payload.clusterId = packet->payload.clusterId;
					targetBuffer->payload.clusterSize = packet->payload.clusterSize;
					targetBuffer->payload.freeMeshInConnections = packet->payload.freeMeshInConnections;
					targetBuffer->payload.freeMeshOutConnections = packet->payload.freeMeshOutConnections;
					targetBuffer->payload.sender = packet->payload.sender;
					targetBuffer->payload.meshWriteHandle = packet->payload.meshWriteHandle;
					targetBuffer->payload.ackField = packet->payload.ackField;
					targetBuffer->connectable = bleEvent->evt.gap_evt.params.adv_report.type;
					targetBuffer->rssi = bleEvent->evt.gap_evt.params.adv_report.rssi;
					targetBuffer->receivedTimeDs = appTimerDs;
				}
			}
			break;
	}

}

joinMeBufferPacket* Node::findTargetBuffer(advPacketJoinMeV0* packet)
{
	joinMeBufferPacket* targetBuffer = NULL;

	//First, look if a packet from this node is already in the buffer, if yes, we use this space
	for (int i = 0; i < joinMePacketBuffer->_numElements; i++)
	{
		targetBuffer = (joinMeBufferPacket*)joinMePacketBuffer->PeekItemAt(i);

		if (packet->payload.sender == targetBuffer->payload.sender)
		{
			logt("DISCOVERY", "Updated old buffer packet");
			return targetBuffer;
		}
	}
	targetBuffer = NULL;

	//Next, we look if there's an empty space
	for (int i = 0; i < joinMePacketBuffer->_numElements; i++)
	{
		targetBuffer = (joinMeBufferPacket*)joinMePacketBuffer->PeekItemAt(i);

		if(targetBuffer->payload.sender == 0)
		{
			logt("DISCOVERY", "Used empty space");
			return targetBuffer;
		}
	}
	targetBuffer = NULL;

	//Next, we can overwrite the oldest packet that we saved from our own cluster
	u32 oldestTimestamp = UINT32_MAX;
	for (int i = 0; i < joinMePacketBuffer->_numElements; i++)
	{
		joinMeBufferPacket* tmpPacket = (joinMeBufferPacket*) joinMePacketBuffer->PeekItemAt(i);

		if(tmpPacket->payload.clusterId == clusterId && tmpPacket->receivedTimeDs < oldestTimestamp){
			oldestTimestamp = tmpPacket->receivedTimeDs;
			targetBuffer = tmpPacket;
		}
	}

	if(targetBuffer != NULL){
		logt("DISCOVERY", "Overwrote one from our own cluster");
		return targetBuffer;
	}

	//If there's still no space, we overwrite the oldest packet that we received, this will not fail
	//TODO: maybe do not use oldest one but worst candidate?? Use clusterScore on all packets to find the least interesting
	u32 minScore = UINT32_MAX;
	for (int i = 0; i < joinMePacketBuffer->_numElements; i++)
	{
		joinMeBufferPacket* tmpPacket = (joinMeBufferPacket*) joinMePacketBuffer->PeekItemAt(i);

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
 ### Persistent configuration
 #########################################################################################################
 */
#define ________________CONFIGURATION___________________
#pragma region configuration

void Node::SaveConfiguration()
{
	logt("ERROR", "save node conf");
	Conf::SaveModuleSettingsToFlash(moduleID::NODE_ID, &persistentConfig, sizeof(NodeConfiguration), this, 0, NULL, 0);
}


#pragma endregion configuration


/*
 #########################################################################################################
 ### Advertising and Receiving advertisements
 #########################################################################################################
 */
#define ________________STATES___________________
#pragma region states

void Node::ChangeState(discoveryState newState)
{
	if (currentDiscoveryState == newState || stateMachineDisabled || !persistentConfig.moduleActive) return;

	discoveryState oldState = currentDiscoveryState;
	currentDiscoveryState = newState;

	//Check what we have to do to leave our old state

	//Now let's check what we do on entry of the new state
	if (newState == discoveryState::DISCOVERY)
	{
		nextDiscoveryState = discoveryState::INVALID_STATE;

		//Use Low instead of High discovery if no nodes have been found for a while
		if (noNodesFoundCounter < Config->discoveryHighToLowTransitionDuration)
		{
			ChangeState(discoveryState::DISCOVERY_HIGH);
		}
		else
		{
			ChangeState(discoveryState::DISCOVERY_LOW);
		}

		//FIXME: this disables discovery low mode
		ChangeState(discoveryState::DISCOVERY_HIGH);
	}
	else if (newState == discoveryState::DISCOVERY_HIGH)
	{
		currentStateTimeoutDs = Config->meshStateTimeoutHighDs;
		nextDiscoveryState = discoveryState::DECIDING;

		logt("STATES", "-- DISCOVERY HIGH --");
		//TODO: ADVREF AdvertisingController::getInstance()->SetAdvertisingState(ADV_STATE_HIGH);
		ScanController::getInstance()->SetScanState(SCAN_STATE_HIGH);

	}
	else if (newState == discoveryState::DISCOVERY_LOW)
	{
		currentStateTimeoutDs = Config->meshStateTimeoutLowDs;
		nextDiscoveryState = discoveryState::DECIDING;

		logt("STATES", "-- DISCOVERY LOW --");
		//TODO: ADVREF AdvertisingController::getInstance()->SetAdvertisingState(ADV_STATE_LOW);
		ScanController::getInstance()->SetScanState(SCAN_STATE_LOW);

	}
	else if (newState == discoveryState::DECIDING)
	{
		nextDiscoveryState = discoveryState::INVALID_STATE;

		logt("STATES", "-- DECIDING --");

		//Disable scanning first
		ScanController::getInstance()->SetScanState(SCAN_STATE_OFF);

		//Check if we want to reestablish a connection
//		int result = GS->cm->ReestablishConnections();
//
//		if(result == 1){
//			//as peripheral
//			ChangeState(discoveryState::DISCOVERY_HIGH);
//		} else if(result == 2){
//			ChangeState(discoveryState::CONNECTING);
//		} else if (result == 0){

			//Check if there is a good cluster
			Node::decisionResult decision = DetermineBestClusterAvailable();


			if (decision == Node::DECISION_NO_NODES_FOUND)
			{
				if (noNodesFoundCounter < 100) //Do not overflow
					noNodesFoundCounter++;
				ChangeState(discoveryState::BACK_OFF);
			}
			else if (decision == Node::DECISION_CONNECT_AS_MASTER)
			{
				ChangeState(discoveryState::CONNECTING);
				noNodesFoundCounter = 0;
			}
			else if (decision == Node::DECISION_CONNECT_AS_SLAVE)
			{
				noNodesFoundCounter = 0;
				ChangeState(discoveryState::DISCOVERY);
			}

//		}


	}
	else if (newState == discoveryState::BACK_OFF)
	{
		nextDiscoveryState = discoveryState::DISCOVERY;
		if(Config->meshStateTimeoutBackOffDs == 0) currentStateTimeoutDs = Config->meshStateTimeoutBackOffDs;
		else currentStateTimeoutDs = (Config->meshStateTimeoutBackOffDs + (Utility::GetRandomInteger() % Config->meshStateTimeoutBackOffVarianceDs)); // 5 - 8 sec

		logt("STATES", "-- BACK OFF --");
		ScanController::getInstance()->SetScanState(SCAN_STATE_OFF);
	}
	else if (newState == discoveryState::CONNECTING)
	{
		//Connection will be terminated by connection procedure itself
		//This might be a timeout, or a success
		//Which will call the Handshake state
		//But we will set a high timeout in case anything fails
		currentStateTimeoutDs = SEC_TO_DS(30);
		nextDiscoveryState = discoveryState::DECIDING;


		logt("STATES", "-- CONNECT_AS_MASTER --");
		ScanController::getInstance()->SetScanState(SCAN_STATE_OFF);

	}
	else if (newState == discoveryState::HANDSHAKE)
	{
		//Use a timeout that is high enough for the handshake to finish
		if(Config->meshHandshakeTimeoutDs == 0){
			currentStateTimeoutDs = discoveryState::INVALID_STATE;
		} else {
			currentStateTimeoutDs = Config->meshHandshakeTimeoutDs;
		}
		nextDiscoveryState = discoveryState::HANDSHAKE_TIMEOUT;


		logt("STATES", "-- HANDSHAKE --");
		//TODO: ADVREF AdvertisingController::getInstance()->SetAdvertisingState(ADV_STATE_OFF); ?? Should be disable advertising?

		ScanController::getInstance()->SetScanState(SCAN_STATE_OFF);


	}
	else if (newState == discoveryState::HANDSHAKE_TIMEOUT)
	{
		nextDiscoveryState = discoveryState::INVALID_STATE;

		logt("STATES", "-- HANDSHAKE TIMEOUT/FAIL --");
		HandshakeTimeoutHandler();


	}
	else if (newState == discoveryState::DISCOVERY_OFF)
	{
		nextDiscoveryState = discoveryState::INVALID_STATE;


		logt("STATES", "-- DISCOVERY OFF --");

		meshAdvJobHandle->slots = 0;
		AdvertisingController::getInstance()->RefreshJob(meshAdvJobHandle);

		ScanController::getInstance()->SetScanState(SCAN_STATE_OFF);

	}

	//Inform all modules of the new state
	//Dispatch event to all modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(activeModules[i] != 0 && activeModules[i]->configurationPointer->moduleActive){
			activeModules[i]->NodeStateChangedHandler(newState);
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

void Node::TimerTickHandler(u16 timerDs)
{
	//Update the app timer (The app timer has a drift when comparing it to the
	//config value in deciseconds because these do not convert nicely into ticks)
	appTimerDs += timerDs;
	currentStateTimeoutDs -= timerDs;

	UpdateGlobalTime();

	//Check if we should switch states because of timeouts
	if (nextDiscoveryState != INVALID_STATE && currentStateTimeoutDs <= 0)
	{
		//Go to the next state
		ChangeState(nextDiscoveryState);
	}
}

void Node::UpdateGlobalTime(){
	//Request the Realtimeclock counter
	u32 rtc1, passedTime;
	rtc1 = FruityHal::GetRtc();
	passedTime = FruityHal::GetRtcDifference(rtc1, previousRtcTicks);
	previousRtcTicks = rtc1;

	//Update the global time seconds and save the remainder for the next iteration
	passedTime += globalTimeRemainderTicks;
	globalTimeSec += passedTime / APP_TIMER_CLOCK_FREQ;
	globalTimeRemainderTicks = passedTime % APP_TIMER_CLOCK_FREQ;
}

#pragma endregion States

/*
 #########################################################################################################
 ### Radio
 #########################################################################################################
 */
#define ________________RADIO___________________

//This will get called before every packet that is sent and can be used to modify packets before sending
//

void Node::RadioEventHandler(bool radioActive)
{
	//Let's do some logging
	/*if (radioActive) radioActiveCount++;

	if (currentLedMode == LED_MODE_RADIO)
	{
		if (radioActive) LedRed->On();
		else LedRed->Off();
	}*/

	//if(radioActive) trace("1,");
	//else trace("0,");

	//trace(".");
	/*if (radioActive)
	 {
	 if (inConnection->isConnected || outConnections[0]->isConnected || outConnections[1]->isConnected || outConnections[2]->isConnected)
	 {
	 LedRed->On();
	 }
	 else
	 {
	 LedGreen->On();
	 }

	 }*/
}

/*
 #########################################################################################################
 ### Actions
 #########################################################################################################
 */
#define ________________ACTIONS___________________



/*
 #########################################################################################################
 ### Helper functions
 #########################################################################################################
 */
#define ________________HELPERS___________________

//Generates a new clusterID by using connectionLoss and the unique id of the node
clusterID Node::GenerateClusterID(void)
{
	//Combine connection loss and nodeId to generate a unique cluster id
	clusterID newId = this->persistentConfig.nodeId + (this->connectionLossCounter << 16);

	logt("NODE", "New cluster id generated %x", newId);
	return newId;
}

Module* Node::GetModuleById(moduleID id)
{
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(activeModules[i]->moduleId == id){
			return activeModules[i];
		}
	}
	return NULL;
}

void Node::PrintStatus(void)
{
	u32 err;

	trace("**************" EOL);
	SetTerminalTitle();
	trace("This is Node %u in clusterId:%u with clusterSize:%d, networkId:%u, version:%u" EOL, this->persistentConfig.nodeId, this->clusterId, this->clusterSize, persistentConfig.networkId, fruityMeshVersion);
	trace("Ack Field:%u, ChipIdA:%u, ChipIdB:%u, ConnectionLossCounter:%u, nodeType:%d" EOL, currentAckId, NRF_FICR->DEVICEID[0], NRF_FICR->DEVICEID[1], connectionLossCounter, this->persistentConfig.deviceType);

	fh_ble_gap_addr_t p_addr;
	err = FruityHal::BleGapAddressGet(&p_addr);
	APP_ERROR_CHECK(err); //OK
	trace("GAP Addr is %02X:%02X:%02X:%02X:%02X:%02X, serial:%s, netKey: %02X:%02X:%02X:%02X:...:%02X:%02X:%02X:%02X, state:%u" EOL EOL,
			p_addr.addr[5],
			p_addr.addr[4],
			p_addr.addr[3],
			p_addr.addr[2],
			p_addr.addr[1],
			p_addr.addr[0],
			Config->serialNumber,
			persistentConfig.networkKey[0],
			persistentConfig.networkKey[1],
			persistentConfig.networkKey[2],
			persistentConfig.networkKey[3],
			persistentConfig.networkKey[12],
			persistentConfig.networkKey[13],
			persistentConfig.networkKey[14],
			persistentConfig.networkKey[15],
			currentDiscoveryState);

	//Print connection info
	BaseConnections conn = GS->cm->GetBaseConnections(ConnectionDirection::CONNECTION_DIRECTION_INVALID);
	trace("CONNECTIONS %u (freeIn:%u, freeOut:%u, pendingPackets:%u" EOL, conn.count, GS->cm->freeMeshInConnections, GS->cm->freeMeshOutConnections, GS->cm->GetPendingPackets());
	for (u32 i = 0; i < conn.count; i++) {
		conn.connections[i]->PrintStatus();
	}
	trace("**************" EOL);
}

void Node::SetTerminalTitle()
{
#ifdef SET_TERMINAL_TITLE
	//Change putty terminal title
	if(Config->terminalPromptMode) trace("\033]0;Node %u (%s) ClusterSize:%d (%x), [%u, %u, %u, %u]\007", persistentConfig.nodeId, Config->serialNumber, clusterSize, clusterId, GS->cm->connections[0]->partnerId, GS->cm->connections[1]->partnerId, GS->cm->connections[2]->partnerId, GS->cm->connections[3]->partnerId);
#endif
}

void Node::PrintBufferStatus(void)
{
	//Print JOIN_ME buffer
	trace("JOIN_ME Buffer:" EOL);
	joinMeBufferPacket* packet;
	for (int i = 0; i < joinMePacketBuffer->_numElements; i++)
	{
		packet = (joinMeBufferPacket*) joinMePacketBuffer->PeekItemAt(i);
		trace("=> %d, clstId:%u, clstSize:%d, freeIn:%u, freeOut:%u, writeHndl:%u, ack:%u, rssi:%d, ageDs:%d", packet->payload.sender, packet->payload.clusterId, packet->payload.clusterSize, packet->payload.freeMeshInConnections, packet->payload.freeMeshOutConnections, packet->payload.meshWriteHandle, packet->payload.ackField, packet->rssi, appTimerDs - packet->receivedTimeDs);
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

bool Node::TerminalCommandHandler(std::string commandName, std::vector<std::string> commandArgs)
{

	//React on commands, return true if handled, false otherwise
	if(commandArgs.size() >= 2 && commandArgs[1] == "node")
	{
		if(commandName == "action")
		{
			//Rewrite "this" to our own node id, this will actually build the packet
			//But reroute it to our own node
			nodeID destinationNode = (commandArgs[0] == "this") ? persistentConfig.nodeId : atoi(commandArgs[0].c_str());

			if(commandArgs.size() == 4 && commandArgs[2] == "discovery")
			{
				u8 discoveryState = commandArgs[3] == "off" ? 0 : 1;

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					destinationNode,
					NodeModuleTriggerActionMessages::SET_DISCOVERY,
					0,
					&discoveryState,
					1,
					false
				);

				return true;
			}
		}
	}

	/************* SYSTEM ***************/
	if (commandName == "reset")
	{
		//Do not reboot in safe mode
		*GS->rebootMagicNumberPtr = REBOOT_MAGIC_NUMBER;
		FruityHal::SystemReset(); //OK
	}
	/************ TESTING ************/
//	else if (commandName == "sustain")
//	{
//		for(int i=0; i<4; i++){
//			if(GS->cm->connections[i]->isConnected()){
//				GS->cm->connections[i]->reestablishTimeSec = 40;
//				logt("ERROR", "sustain time set");
//			}
//		}
//	}
	/************* NODE ***************/
	//Get a full status of the node
	else if (commandName == "status")
	{
		PrintStatus();
	}
	//Print the JOIN_ME buffer
	else if (commandName == "bufferstat")
	{
		PrintBufferStatus();
	}
	//Broadcast some data over all connections
	else if (commandName == "data")
	{
		nodeID receiverId = 0;
		if(commandArgs.size() > 0){
			if(commandArgs[0] == "sink") receiverId = NODE_ID_SHORTEST_SINK;
			else if(commandArgs[0] == "hop") receiverId = NODE_ID_HOPS_BASE + 1;
			else receiverId = NODE_ID_BROADCAST;
		}

		connPacketData1 data;
		memset(&data, 0x00, sizeof(connPacketData1));

		data.header.messageType = MESSAGE_TYPE_DATA_1;
		data.header.sender = persistentConfig.nodeId;
		data.header.receiver = receiverId;

		data.payload.length = 7;
		data.payload.data[0] = 1;
		data.payload.data[1] = 3;
		data.payload.data[2] = 3;

		bool reliable = (commandArgs.size() == 0) ? false : true;

		GS->cm->SendMeshMessage((u8*) &data, SIZEOF_CONN_PACKET_DATA_1, DeliveryPriority::DELIVERY_PRIORITY_LOW, reliable);
	}
	//Send some large data that is split over a few messages
	else if(commandName == "datal")
	{
		bool reliable = (commandArgs.size() > 0 && commandArgs[0] == "r");

		const u8 dataLength = 145;
		u8 _packet[dataLength];
		connPacketHeader* packet = (connPacketHeader*)_packet;
		packet->messageType = MESSAGE_TYPE_DATA_1;
		packet->receiver = 0;
		packet->sender = persistentConfig.nodeId;

		for(u32 i=0; i< dataLength-5; i++){
			_packet[i+5] = i+1;
		}

		GS->cm->SendMeshMessage(_packet, dataLength, DeliveryPriority::DELIVERY_PRIORITY_LOW, reliable);
	}
	//Simulate connection loss which generates a new cluster id
	else if (commandName == "loss")
	{
		this->connectionLossCounter++;
		clusterId = this->GenerateClusterID();
		this->UpdateJoinMePacket();
	}
	//Set a timestamp for this node
	else if (commandName == "settime")
	{
		//Set the time for our node
		globalTimeSec = atoi(commandArgs[0].c_str());
		globalTimeRemainderTicks = 0;
	}
	//Display the time of this node
	else if(commandName == "gettime")
	{
		char timestring[80];
		Logger::getInstance()->convertTimestampToString(globalTimeSec, globalTimeRemainderTicks, timestring);

		trace("Time is currently %s" EOL, timestring);
	}
	//Generate a timestamp packet and send it to all other nodes
	else if (commandName == "sendtime")
	{
		connPacketUpdateTimestamp packet;

		packet.header.messageType = MESSAGE_TYPE_UPDATE_TIMESTAMP;
		packet.header.sender = persistentConfig.nodeId;
		packet.header.receiver = 0;

		//Data must not be filled because it is set in the fillTransmitBuffers method
		//Because it might still take some time from filling the buffer to sending the packet
		//We should use the radio event or estimate the sending based on the connetion parameters

		//It is then received and processed in the Connectionmanager::messageReceivedCallback

		GS->cm->SendMeshMessage((u8*)&packet, SIZEOF_CONN_PACKET_UPDATE_TIMESTAMP,DeliveryPriority::DELIVERY_PRIORITY_HIGH, true);

	}
	//Switch to another discovery mode
	else if (commandName == "discovery")
	{
		if (commandArgs.size() < 1 || commandArgs[0] == "high")
		{
			noNodesFoundCounter = 0;
			ChangeState(discoveryState::DISCOVERY_HIGH);
		}
		else if (commandArgs[0] == "low")
		{
			noNodesFoundCounter = 50;
			ChangeState(discoveryState::DISCOVERY_LOW);

		}
		else if (commandArgs[0] == "off")
		{
			ChangeState(discoveryState::DISCOVERY_OFF);
		}
	}
	//Stop the state machine
	else if (commandName == "stop")
	{
		DisableStateMachine(true);
	}
	//Start the state machine
	else if (commandName == "start")
	{
		DisableStateMachine(false);
	}
	//This variable can be used to toggle conditional breakpoints
	else if (commandName == "break")
	{
		Config->breakpointToggleActive = !Config->breakpointToggleActive;
	}
	//Try to connect to one of the nodes in the test devices array
	else if (commandName == "connect")
	{
#ifdef ENABLE_TEST_DEVICES
		for (int i = 0; i <NUM_TEST_DEVICES ; i++)
		{
			if (strtol(commandArgs[0].c_str(), NULL, 10) == Config->testDevices[i].id)
			{
				logt("NODE", "Trying to connecting to node %d", Config->testDevices[i].id);

				cm->ConnectAsMaster(Config->testDevices[i].id, &Config->testDevices[i].addr, 14);
			}
		}
#else
		//Allows us to connect to any node when giving the GAP Address
		nodeID partnerId = atoi(commandArgs[0].c_str());
		u8 buffer[6];
		Logger::getInstance()->parseHexStringToBuffer(commandArgs[1].c_str(), buffer, 6);
		fh_ble_gap_addr_t addr;
		addr.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
		addr.addr[0] = buffer[5];
		addr.addr[1] = buffer[4];
		addr.addr[2] = buffer[3];
		addr.addr[3] = buffer[2];
		addr.addr[4] = buffer[1];
		addr.addr[5] = buffer[0];

		//Using the same GATT handle as our own will probably work if our partner has the same implementation
		GS->cm->ConnectAsMaster(partnerId, &addr, meshService.sendMessageCharacteristicHandle.value_handle);
#endif
	}
	//Disconnect a connection by id (0-4)
	else if (commandName == "disconnect")
	{
		if (commandArgs.size() > 0)
		{
			u8 connectionId = atoi(commandArgs[0].c_str());

			GS->cm->allConnections[connectionId]->Disconnect();
		}
	}
	//tell the gap layer to loose a connection
	else if (commandName == "gap_disconnect")
	{
		if (commandArgs.size() > 0)
		{
			u8 connectionId = atoi(commandArgs[0].c_str());
			GAPController::getInstance()->disconnectFromPartner(GS->cm->allConnections[connectionId]->connectionHandle);
		}
	}
	else if(commandName == "update_iv")
	{
		nodeID nodeId = atoi(commandArgs[0].c_str());
		u16 newConnectionInterval = atoi(commandArgs[1].c_str());

		connPacketUpdateConnectionInterval packet;
		packet.header.messageType = MESSAGE_TYPE_UPDATE_CONNECTION_INTERVAL;
		packet.header.sender = persistentConfig.nodeId;
		packet.header.receiver = nodeId;

		packet.newInterval = newConnectionInterval;
		GS->cm->SendMeshMessage((u8*)&packet, SIZEOF_CONN_PACKET_UPDATE_CONNECTION_INTERVAL, DeliveryPriority::DELIVERY_PRIORITY_HIGH, false);
	}
	//Encrypt a connection by id
	else if (commandName == "security")
	{
		u16 connectionId = (u16) strtol(commandArgs[0].c_str(), NULL, 10);

		//Enable connection security
		GAPController::getInstance()->startEncryptingConnection(GS->cm->allConnections[connectionId]->connectionHandle);
	}
	//Configure the current node to be a data endpoint
	else if (commandName == "yousink")
	{
		this->persistentConfig.deviceType = deviceTypes::DEVICE_TYPE_SINK;
	}
	//Change nodeid of current node
	else if (commandName == "set_nodeid")
	{
		this->persistentConfig.nodeId = atoi(commandArgs[0].c_str());
	}
	/************* UART COMMANDS ***************/
	//Get the status information of this node
	else if(commandName == "get_plugged_in")
	{
		logjson("NODE", "{\"type\":\"plugged_in\",\"nodeId\":%u,\"serialNumber\":\"%s\"}" SEP, persistentConfig.nodeId, Config->serialNumber);
	}
	//Query all modules from any node
	else if((commandName == "get_modules") && commandArgs.size() == 1)
	{
		nodeID receiver = commandArgs[0] == "this" ? persistentConfig.nodeId : atoi(commandArgs[0].c_str());

		connPacketModule packet;
		packet.header.messageType = MESSAGE_TYPE_MODULE_CONFIG;
		packet.header.sender = persistentConfig.nodeId;
		packet.header.receiver = receiver;

		packet.moduleId = moduleID::NODE_ID;
		packet.requestHandle = 0;
		packet.actionType = Module::ModuleConfigMessages::GET_MODULE_LIST;

		GS->cm->SendMeshMessage((u8*) &packet, SIZEOF_CONN_PACKET_MODULE, DeliveryPriority::DELIVERY_PRIORITY_LOW, true);
	}
	else
	{
		return false;
	}
	return true;
}

void Node::ButtonHandler(u8 buttonId, u32 holdTimeDs)
{

}

inline void Node::SendModuleList(nodeID toNode, u8 requestHandle)
{
u8 buffer[SIZEOF_CONN_PACKET_MODULE + MAX_MODULE_COUNT*4];
		memset(buffer, 0, sizeof(buffer));

		connPacketModule* outPacket = (connPacketModule*)buffer;
		outPacket->header.messageType = MESSAGE_TYPE_MODULE_CONFIG;
		outPacket->header.sender = persistentConfig.nodeId;
		outPacket->header.receiver = toNode;

		outPacket->moduleId = moduleID::NODE_ID;
		outPacket->requestHandle = requestHandle;
		outPacket->actionType = Module::ModuleConfigMessages::MODULE_LIST;


		for(int i = 0; i<MAX_MODULE_COUNT; i++){
			if(activeModules[i] != NULL){
				//TODO: can we do this better? the data region is unaligned in memory
				memcpy(outPacket->data + i*4, &activeModules[i]->configurationPointer->moduleId, 2);
				memcpy(outPacket->data + i*4 + 2, &activeModules[i]->configurationPointer->moduleVersion, 1);
				memcpy(outPacket->data + i*4 + 3, &activeModules[i]->configurationPointer->moduleActive, 1);
			}
		}

		/*
		char* strbuffer[200];
		Logger::getInstance()->convertBufferToHexString(buffer, SIZEOF_CONN_PACKET_MODULE + MAX_MODULE_COUNT*4, (char*)strbuffer);
		logt("MODULE", "Sending: %s", strbuffer);
*/

		GS->cm->SendMeshMessage((u8*)outPacket, SIZEOF_CONN_PACKET_MODULE + MAX_MODULE_COUNT*4, DeliveryPriority::DELIVERY_PRIORITY_LOW, true);
}

u8 Node::GetBatteryRuntime()
{
	//TODO: implement, measurement can be done in here or sampled periodically
	//If measurement is done in here, we should save the last measurement and only update it after
	//some time has passed
	return 7;
}

/* EOF */
