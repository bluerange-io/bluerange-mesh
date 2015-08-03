/**
 OS_LICENSE_PLACEHOLDER
 */

#include <Config.h>

#include <Node.h>
#include <LedWrapper.h>
#include <Connection.h>
#include <SimpleBuffer.h>
#include <AdvertisingController.h>
#include <GATTController.h>
#include <ConnectionManager.h>
#include <ScanController.h>
#include <Utility.h>
#include <Logger.h>
#include <TestModule.h>
#include <DFUModule.h>
#include <StatusReporterModule.h>
#include <AdvertisingModule.h>
#include <ScanningModule.h>

extern "C"
{
#include <stdlib.h>
#include <nrf_soc.h>
#include <app_error.h>
}

//Buffer that keeps a predefined number of join me packets
#define JOIN_ME_PACKET_BUFFER_MAX_ELEMENTS 10
joinMeBufferPacket raw_joinMePacketBuffer[JOIN_ME_PACKET_BUFFER_MAX_ELEMENTS];

#define MAX_JOIN_ME_PACKET_AGE_MS (15 * 1000)

Node* Node::instance;
ConnectionManager* Node::cm;

bool Node::lookingForInvalidStateErrors = false;

Node::Node(networkID networkId)
{
	//Initialize variables
	instance = this;

	ackFieldDebugCopy = 0;
	this->clusterId = 0;
	this->clusterSize = 1;


	this->noNodesFoundCounter = 0;
	this->passsedTimeSinceLastTimerHandler = 0;

	this->outputRawData = false;

	this->lastRadioActiveCountResetTimerMs = 0;
	this->radioActiveCount = 0;

	//Set the current state and its timeout
	currentStateTimeoutMs = 0;
	currentDiscoveryState = discoveryState::BOOTUP;
	nextDiscoveryState = discoveryState::INVALID_STATE;
	this->appTimerMs = 0;
	this->lastDecisionTimeMs = 0;

	LedRed = new LedWrapper(BSP_LED_0, false);
	LedGreen = new LedWrapper(BSP_LED_1, false);
	LedBlue = new LedWrapper(BSP_LED_2, false);

	LedRed->Off();
	LedGreen->Off();
	LedBlue->Off();

	//Register terminal listener
	Terminal::AddTerminalCommandListener(this);

	currentLedMode = LED_MODE_CONNECTIONS;


	//Receive ConnectionManager events
	cm = ConnectionManager::getInstance();
	cm->setConnectionManagerCallback(this);


	//Initialize all Modules
	//Module ids start with 1, this id is also used for saving persistent
	//module configurations with the Storage class
	//Module ids must persist when nodes are updated to guearantee that the
	//same module receives the same storage slot
	activeModules[0] = new TestModule(moduleID::TEST_MODULE_ID, this, cm, "TEST", 1);
	//activeModules[1] = new DFUModule((moduleID::DFU_MODULE_ID, this, cm, "DFU", 2);
	activeModules[2] = new StatusReporterModule(moduleID::STATUS_REPORTER_MODULE_ID, this, cm, "STATUS", 3);
	activeModules[3] = new AdvertisingModule(moduleID::ADVERTISING_MODULE_ID, this, cm, "ADV", 4);
	activeModules[4] = new ScanningModule(moduleID::SCANNING_MODULE_ID, this, cm, "SCAN", 5);


	//Register a pre/post transmit hook for radio events
	//ble_radio_notification_init(NRF_APP_PRIORITY_HIGH, NRF_RADIO_NOTIFICATION_DISTANCE_1740US, radioNotificationHook);

	joinMePacketBuffer = new SimpleBuffer((u8*) raw_joinMePacketBuffer, sizeof(joinMeBufferPacket) * JOIN_ME_PACKET_BUFFER_MAX_ELEMENTS, sizeof(joinMeBufferPacket));

	//Load Node configuration from slot 0
	if(Config->ignorePersistentConfigurationOnBoot){
		persistentConfig.version = 0xFF;
		ConfigurationLoadedHandler();
	} else {
		Storage::getInstance().QueuedRead((u8*) &persistentConfig, sizeof(NodeConfiguration), 0, this);
	}

}

void Node::ConfigurationLoadedHandler()
{
	u32 err;


	//If config is unset, set to default
	if (persistentConfig.version == 0xFF)
	{
		logt("NODE", "default config set");
		persistentConfig.version = 0;
		persistentConfig.connectionLossCounter = 0;
		persistentConfig.networkId = Config->meshNetworkIdentifier;
		persistentConfig.reserved = 0;

		//Get an id for our testdevices when not working with persistent storage
		InitWithTestDeviceSettings();
	}
	else
	{
		logt("NODE", "Config loaded nodeId:%X, connLossCount:%X, netowrkId:%d, reserved:%d", persistentConfig.nodeId, persistentConfig.connectionLossCounter, persistentConfig.networkId, persistentConfig.reserved);
	}



	clusterId = this->GenerateClusterID();

	//Set the BLE address so that we have the same on every startup
	err = sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &persistentConfig.nodeAddress);
	APP_ERROR_CHECK(err);

	//Init softdevice and c libraries
	ScanController::Initialize();
	AdvertisingController::Initialize(persistentConfig.networkId);

	//Fill JOIN_ME packet with data
	this->UpdateJoinMePacket(NULL);

	//Print configuration and start node
	logt("NODE", "Config loaded nodeId:%d, connLossCount:%02X, reserved:%d", persistentConfig.nodeId, persistentConfig.connectionLossCounter, persistentConfig.reserved);

	//Go to Discovery
	ChangeState(discoveryState::DISCOVERY);
}

/*
 #########################################################################################################
 ### Connections and Handlers
 #########################################################################################################
 */
#define ________________CONNECTION___________________
#pragma region connections

//Is called as soon as a connection is connected, before the handshake
void Node::ConnectionSuccessfulHandler(ble_evt_t* bleEvent)
{
	logt("NODE", "Connection initiated");

	//We are leaving the discoveryState::CONNECTING state
	ChangeState(discoveryState::HANDSHAKE);

	//TODO: manage the timeout for the handshake
}

//Is called after a connection has ended its handshake
void Node::HandshakeDoneHandler(Connection* connection)
{
	logt("NODE", "Handshake done");
	//Go back to Discovery
	ChangeState(discoveryState::DISCOVERY);

}

//If we wanted to connect but our connection timed out (only outgoing connections)
void Node::ConnectionTimeoutHandler(ble_evt_t* bleEvent)
{
	logt("NODE", "Connection Timeout");

	//We are leaving the discoveryState::CONNECTING state
	ChangeState(discoveryState::DISCOVERY);
}

void Node::DisconnectionHandler(ble_evt_t* bleEvent)
{
	this->persistentConfig.connectionLossCounter++;

	Connection* connection = cm->GetConnectionFromHandle(bleEvent->evt.gap_evt.conn_handle);

	if (connection->handshakeDone)
	{
		//CASE 1: if this is the smaller cluster then we have to get a new clusterID
		if (clusterSize - connection->connectedClusterSize < connection->connectedClusterSize || (clusterSize - connection->connectedClusterSize == connection->connectedClusterSize && persistentConfig.nodeId < connection->partnerId))
		{
			this->clusterId = GenerateClusterID();

			logt("HANDSHAKE", "ClusterSize Change from %d to %d", this->clusterSize, this->clusterSize - connection->connectedClusterSize);

			this->clusterSize -= connection->connectedClusterSize;

			//Inform the rest of the cluster of our new ID and size
			connPacketClusterInfoUpdate packet;

			packet.header.messageType = MESSAGE_TYPE_CLUSTER_INFO_UPDATE;
			packet.header.sender = this->persistentConfig.nodeId;
			packet.header.receiver = 0;

			packet.payload.currentClusterId = connection->connectedClusterId;
			packet.payload.newClusterId = this->clusterId;
			packet.payload.clusterSizeChange = -connection->connectedClusterSize;



			logt("HANDSHAKE", "OUT => %d CLUSTER_INFO_UPDATE sizeChange:%d", connection->partnerId, packet.payload.clusterSizeChange);

			//Send message to all other connections and update the hops to sink accordingly
			for(int i=0; i<Config->meshMaxConnections; i++){
				if(cm->connections[i] == connection || !cm->connections[i]->handshakeDone) continue;
				packet.payload.hopsToSink = cm->GetHopsToShortestSink(cm->connections[i]);
				cm->SendMessage(cm->connections[i], (u8*) &packet, SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE, true);
			}

			//CASE 2: But we might also be the bigger cluster, in this case, we keep our clusterID
		}
		else
		{
			logt("HANDSHAKE", "ClusterSize Change from %d to %d", this->clusterSize, this->clusterSize - connection->connectedClusterSize);

			this->clusterSize -= connection->connectedClusterSize;

			// Inform the rest of the cluster of our new size
			connPacketClusterInfoUpdate packet;

			packet.header.messageType = MESSAGE_TYPE_CLUSTER_INFO_UPDATE;
			packet.header.sender = this->persistentConfig.nodeId;
			packet.header.receiver = 0;

			packet.payload.currentClusterId = connection->connectedClusterId;
			packet.payload.newClusterId = 0;
			packet.payload.clusterSizeChange = -connection->connectedClusterSize;

			logt("HANDSHAKE", "OUT => %d CLUSTER_INFO_UPDATE sizeChange:%d", connection->partnerId, packet.payload.clusterSizeChange);

			//Send message to all other connections and update the hops to sink accordingly
			for(int i=0; i<Config->meshMaxConnections; i++){
				if(cm->connections[i] == connection || !cm->connections[i]->handshakeDone) continue;
				packet.payload.hopsToSink = cm->GetHopsToShortestSink(cm->connections[i]);
				cm->SendMessage(cm->connections[i], (u8*) &packet, SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE, true);
			}

		}
		//Handshake had not yet finished, not much to do
	}
	else
	{

	}

	//In either case, we must update our advertising packet
	UpdateJoinMePacket(NULL);

	//Go to discovery mode, and force high mode
	noNodesFoundCounter = 0;
	ChangeState(discoveryState::DISCOVERY);
}

//All incoming messages over a connection go here if they are not part of the connection handshake
void Node::UpdateClusterInfo(Connection* connection, connPacketClusterInfoUpdate* packet)
{
	//Update hops to sink
	//Another sink may have joined or left the network, update this
	//FIXME: race conditions can cause this to work incorrectly...
	connection->hopsToSink = packet->payload.hopsToSink > -1 ? packet->payload.hopsToSink + 1 : -1;

	//Update size
	if (packet->payload.clusterSizeChange != 0)
	{

		logt("HANDSHAKE", "ClusterSize Change from %d to %d", this->clusterSize, this->clusterSize + packet->payload.clusterSizeChange);

		this->clusterSize += packet->payload.clusterSizeChange;

		//Also on the connection arm
		connection->connectedClusterSize += packet->payload.clusterSizeChange;


		//Update advertisement packets
		this->UpdateJoinMePacket(NULL);
	}

	//PART 1: We belong to the same cluster
	if (packet->payload.currentClusterId == this->clusterId)
	{
		//Part 1A: The cluster ID should be updated and the previous clusterID does match
		if (packet->payload.newClusterId != 0)
		{

			this->clusterId = packet->payload.newClusterId;
			//Also on the connection arm
			connection->connectedClusterId = packet->payload.newClusterId;

			//Update advertisement packets
			this->UpdateJoinMePacket(NULL);
		}

	}
	//PART 2: We do not have the same clusterID, we should probably not update ours. Better disconnect
	else
	{
		//connection.disconnect(true);
	}
}

void Node::messageReceivedCallback(connectionPacket* inPacket)
{
	Connection* connection = cm->GetConnectionFromHandle(inPacket->connectionHandle);
	u8* data = inPacket->data;
	u16 dataLength = inPacket->dataLength;

	connPacketHeader* packetHeader = (connPacketHeader*) data;

	//If the packet is a handshake packet it will not be forwarded to the node but will be
	//handled in the connection. All other packets go here for further processing
	switch (packetHeader->messageType)
	{
		case MESSAGE_TYPE_CLUSTER_INFO_UPDATE:
			if (dataLength == SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE)
			{
				connPacketClusterInfoUpdate* packet = (connPacketClusterInfoUpdate*) data;
				logt("HANDSHAKE", "IN <= %d CLUSTER_INFO_UPDATE clstID:%d, newClstId:%d, sizeChange:%d, hop:%d", connection->partnerId, packet->payload.currentClusterId, packet->payload.newClusterId, packet->payload.clusterSizeChange, packet->payload.hopsToSink);
				UpdateClusterInfo(connection, packet);

			}
			break;

		case MESSAGE_TYPE_DATA_1:
			if (dataLength >= SIZEOF_CONN_PACKET_DATA_1)
			{
				connPacketData1* packet = (connPacketData1*) data;

				logt("DATA", "IN <= %d ################## Got Data packet %d:%d:%d (len:%d) ##################", connection->partnerId, packet->payload.data[0], packet->payload.data[1], packet->payload.data[2], inPacket->dataLength);

				//tracef("data is %u/%u/%u\n\r", packet->payload.data[0], packet->payload.data[1], packet->payload.data[2]);
			}
			break;

		case MESSAGE_TYPE_DATA_2:
			if (dataLength == SIZEOF_CONN_PACKET_DATA_2)
			{
				connPacketData2* packet = (connPacketData2*) data;

				//Update our scan response with the data from this campaign
				this->UpdateScanResponsePacket(packet->payload.data, packet->payload.length);
				logt("DATA", "IN <= %d ################## Got Data 2 packet %c ##################", connection->partnerId, packet->payload.data[0]);
			}
			break;

		case MESSAGE_TYPE_ADVINFO:
			if (dataLength == SIZEOF_CONN_PACKET_ADV_INFO)
			{
				connPacketAdvInfo* packet = (connPacketAdvInfo*) data;

				uart("ADVINFO", "{\"sender\":\"%d\",\"addr\":\"%x:%x:%x:%x:%x:%x\",\"count\":%d,\"rssiSum\":%d}", packet->header.sender, packet->payload.peerAddress[0], packet->payload.peerAddress[1], packet->payload.peerAddress[2], packet->payload.peerAddress[3], packet->payload.peerAddress[4], packet->payload.peerAddress[5], packet->payload.packetCount, packet->payload.inverseRssiSum);

			}
			break;
	}

	//Now we must pass the message to all of our modules for further processing
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(activeModules[i] != 0){
			activeModules[i]->ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);
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
void Node::UpdateJoinMePacket(joinMeBufferPacket* ackCluster)
{

	//Build a JOIN_ME packet and set it in the advertisement data
	advPacketPayloadJoinMeV0 packet;

	packet.sender = this->persistentConfig.nodeId;
	packet.clusterId = this->clusterId;
	packet.clusterSize = this->clusterSize;
	packet.freeInConnections = cm->freeInConnections;
	packet.freeOutConnections = cm->freeOutConnections;
	packet.ackField = 0;
	packet.version = 0;
	packet.meshWriteHandle = GATTController::getMeshWriteHandle();

	if (ackCluster != NULL)
	{
		packet.clusterSize = ackCluster->payload.clusterSize;
		packet.ackField = ackCluster->payload.sender;

		ackFieldDebugCopy = ackCluster->payload.ackField;
	}

	sizedData data;
	data.data = (u8*) &packet;
	data.length = SIZEOF_ADV_PACKET_PAYLOAD_JOIN_ME_V0;

	logt("JOIN", "JOIN_ME updated clusterId:%d, clusterSize:%d, freeIn:%d, freeOut:%d, handle:%d, ack:%d", packet.clusterId, packet.clusterSize, packet.freeInConnections, packet.freeOutConnections, packet.meshWriteHandle, packet.ackField);

	//Broadcast connectable advertisement if we have a free inConnection, otherwise, we can only act as master
	if (!cm->inConnection->isConnected) AdvertisingController::UpdateAdvertisingData(MESSAGE_TYPE_JOIN_ME, &data, true);
	else AdvertisingController::UpdateAdvertisingData(MESSAGE_TYPE_JOIN_ME, &data, false);
}

void Node::UpdateScanResponsePacket(u8* newData, u8 length)
{

	sizedData data;
	data.data = newData;
	data.length = length;

	AdvertisingController::SetScanResponse(&data);

}

//STEP 3: After collecting all available clusters, we want to connect to the best cluster that is available
//If the other clusters were not good and we have something better, we advertise it.
Node::decisionResult Node::DetermineBestClusterAvailable(void)
{
	//If no clusters have been advertised since the first time, there is no work to do
	if (joinMePacketBuffer->_numElements == 0)
	{
		logt("DISCOVERY", "No other nodes discovered");
		return Node::DECISION_NO_NODES_FOUND;
	}

	//TODO: Kill packets that are timed out (add a lastReceivedTimer to the packets and overwrite old ones)

	u32 bestScore = 0;
	joinMeBufferPacket* bestCluster = NULL;
	bool advertiseMyCluster = false;
	joinMeBufferPacket* packet = NULL;

	//Determine the best Cluster to connect to as a master
	if (cm->freeOutConnections > 0)
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
			if (packet->payload.clusterId != this->clusterId) advertiseMyCluster = true;
		}

		//Now, if we want to be a master in the connection, we simply answer the ad packet that
		//informs us about that cluster
		if (bestCluster != NULL)
		{

			ble_gap_addr_t address;
			address.addr_type = bestCluster->bleAddressType;
			memcpy(address.addr, bestCluster->bleAddress, BLE_GAP_ADDR_LEN);

			cm->ConnectAsMaster(bestCluster->payload.sender, &address, bestCluster->payload.meshWriteHandle);

			//Clear the buffer because it will have changed now
			joinMePacketBuffer->Clean();

			return Node::DECISION_CONNECT_AS_MASTER;
		}
	}

	//If no good cluster could be found (all are bigger than mine)
	//Find the best cluster that should connect to us (we as slave)
	packet = NULL;
	for (int i = 0; i < joinMePacketBuffer->_numElements; i++)
	{
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
		logt("DISCOVERY", "Other clusters are bigger, we are going to be a slave");

		//CASE 1: The ack field is already set to our id, we can reach each other
		//Kill connections and broadcast our preferred partner with the ack field
		//so that he connects to us
		if (bestCluster->payload.ackField == this->persistentConfig.nodeId)
		{
			cm->DisconnectOtherConnections(NULL);

			UpdateJoinMePacket(bestCluster);
		}
		//CASE 2: The ack field is not set to our id, set our ack field to his id
		//And wait for him to confirm that he can reach us
		else
		{
			UpdateJoinMePacket(bestCluster);
		}

		//Clear the buffer because it will have changed now
		joinMePacketBuffer->Clean();

		return Node::DECISION_CONNECT_AS_SLAVE;
	}

	logt("DISCOVERY", "no cluster found");

	return Node::DECISION_NO_NODES_FOUND;
}

//Calculates the score for a cluster
//Connect to big clusters but big clusters must connect nodes that are not able 
u32 Node::CalculateClusterScoreAsMaster(joinMeBufferPacket* packet)
{

	//If the packet is too old, filter it out
	if (appTimerMs - packet->receivedTime > MAX_JOIN_ME_PACKET_AGE_MS) return 0;

	//If we are already connected to that cluster, the score is 0
	if (packet->payload.clusterId == this->clusterId) return 0;

	//If there are zero free in connections, we cannot connect as master
	if (packet->payload.freeInConnections == 0) return 0;

	//If his cluster is bigger, but only if it is not faked (when setting an ack)
	if (packet->payload.ackField == 0 && (packet->payload.clusterSize > this->clusterSize || (packet->payload.clusterSize == this->clusterSize && packet->payload.clusterId > this->clusterId)))
	{
		return 0;
	}

	//If the ack field is not 0 and set to a different nodeID than ours, somebody else wants to connect to him
	if (packet->payload.ackField != 0 && packet->payload.ackField != this->persistentConfig.nodeId)
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

	//Free in connections are best, free out connections are good as well
	//TODO: RSSI should be factored into the score as well, maybe battery runtime, device type, etc...
	return packet->payload.freeInConnections * 1000 + packet->payload.freeOutConnections * 100;
}

//If there are only bigger clusters around, we want to find the best
//And set its id in our ack field
u32 Node::CalculateClusterScoreAsSlave(joinMeBufferPacket* packet)
{

	//If the packet is too old, filter it out
	if (appTimerMs - packet->receivedTime > MAX_JOIN_ME_PACKET_AGE_MS) return 0;

	//If we are already connected to that cluster, the score is 0
	if (packet->payload.clusterId == this->clusterId) return 0;

	//If the ack field is set, we do not want to connect as slave
	if (packet->payload.ackField != 0) return 0;

	//He could not connect to us, leave him alone
	if (packet->payload.freeOutConnections == 0) return 0;

	//Choose the one with the biggest cluster size, if there are more, prefer the most outConnections
	return packet->payload.clusterSize * 1000 + packet->payload.freeOutConnections;
}

//All advertisement packets are received here if they are valid
void Node::AdvertisementMessageHandler(ble_evt_t* bleEvent)
{

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
		case MESSAGE_TYPE_JOIN_ME:
			if (dataLength == SIZEOF_ADV_PACKET_JOIN_ME)
			{

				advPacketJoinMeV0* packet = (advPacketJoinMeV0*) data;

				//Ignore advertising packets from the same cluster
				if (packet->payload.clusterId == clusterId) return;

				//logt("SCAN", "JOIN_ME: sender:%d, clusterId:%d, clusterSize:%d, freeIn:%d, freeOut:%d, ack:%d", packet->payload.sender, packet->payload.clusterId, packet->payload.clusterSize, packet->payload.freeInConnections, packet->payload.freeOutConnections, packet->payload.ackField);

				//Look through the buffer, if this node is already in our packet buffer
				joinMeBufferPacket* targetPacket = NULL;
				for (int i = 0; i < joinMePacketBuffer->_numElements; i++)
				{
					joinMeBufferPacket* joinMePacket = (joinMeBufferPacket*) joinMePacketBuffer->PeekItemAt(i);

					if (packet->payload.sender == joinMePacket->payload.sender)
					{
						targetPacket = joinMePacket;
						break;
					}

				};

				//If there is no previous packet from that node in the buffer, add one to the buffer
				if (targetPacket == NULL)
				{
					targetPacket = (joinMeBufferPacket*) joinMePacketBuffer->Reserve();
				}

				//Now, we have the space for our packet and we fill it with the latest information
				if (targetPacket != NULL)
				{
					memcpy(targetPacket->bleAddress, bleEvent->evt.gap_evt.params.connected.peer_addr.addr, BLE_GAP_ADDR_LEN);
					targetPacket->bleAddressType = bleEvent->evt.gap_evt.params.connected.peer_addr.addr_type;

					targetPacket->payload.clusterId = packet->payload.clusterId;
					targetPacket->payload.clusterSize = packet->payload.clusterSize;
					targetPacket->payload.freeInConnections = packet->payload.freeInConnections;
					targetPacket->payload.freeOutConnections = packet->payload.freeOutConnections;
					targetPacket->payload.sender = packet->payload.sender;
					targetPacket->payload.meshWriteHandle = packet->payload.meshWriteHandle;
					targetPacket->payload.ackField = packet->payload.ackField;
					targetPacket->connectable = bleEvent->evt.gap_evt.params.adv_report.type;
					targetPacket->rssi = bleEvent->evt.gap_evt.params.adv_report.rssi;
					targetPacket->receivedTime = appTimerMs;
				}
			}
			break;
	}

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
	if (currentDiscoveryState == newState || stateMachineDisabled) return;

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

		ChangeState(discoveryState::DISCOVERY_HIGH);
	}
	else if (newState == discoveryState::DISCOVERY_HIGH)
	{
		logt("STATES", "-- DISCOVERY HIGH --");
		AdvertisingController::SetAdvertisingState(ADV_STATE_HIGH);
		ScanController::SetScanState(SCAN_STATE_HIGH);

		currentStateTimeoutMs = Config->meshStateTimeoutHigh;
		nextDiscoveryState = discoveryState::DECIDING;
	}
	else if (newState == discoveryState::DISCOVERY_LOW)
	{
		logt("STATES", "-- DISCOVERY LOW --");
		AdvertisingController::SetAdvertisingState(ADV_STATE_LOW);
		ScanController::SetScanState(SCAN_STATE_LOW);

		currentStateTimeoutMs = Config->meshStateTimeoutLow;
		nextDiscoveryState = discoveryState::DECIDING;
	}
	else if (newState == discoveryState::DECIDING)
	{
		logt("STATES", "-- DECIDING --");

		//Disable scanning and advertising first
		AdvertisingController::SetAdvertisingState(ADV_STATE_OFF);
		ScanController::SetScanState(SCAN_STATE_OFF);

		Node::decisionResult decision = DetermineBestClusterAvailable();

		nextDiscoveryState = discoveryState::INVALID_STATE;

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
	}
	else if (newState == discoveryState::BACK_OFF)
	{
		logt("STATES", "-- BACK OFF --");
		AdvertisingController::SetAdvertisingState(ADV_STATE_OFF);
		ScanController::SetScanState(SCAN_STATE_OFF);

		currentStateTimeoutMs = (Utility::GetRandomInteger() % Config->meshStateTimeoutBackOff + Config->meshStateTimeoutBackOffVariance); // 5 - 8 sec
		nextDiscoveryState = discoveryState::DISCOVERY;
	}
	else if (newState == discoveryState::CONNECTING)
	{
		logt("STATES", "-- CONNECT_AS_MASTER --");
		AdvertisingController::SetAdvertisingState(ADV_STATE_OFF);
		ScanController::SetScanState(SCAN_STATE_OFF);

		//Connection will be terminated by connection procedure itself
		//This might be a timeout, or a success
		//Which will call the Handshake state
		//But we will set a high timeout in case anything fails
		currentStateTimeoutMs = 30 * 1000;
		nextDiscoveryState = discoveryState::DECIDING;
	}
	else if (newState == discoveryState::HANDSHAKE)
	{
		logt("STATES", "-- HANDSHAKE --");
		AdvertisingController::SetAdvertisingState(ADV_STATE_OFF);
		ScanController::SetScanState(SCAN_STATE_OFF);

		nextDiscoveryState = discoveryState::INVALID_STATE;

	}
	else if (newState == discoveryState::DISCOVERY_OFF)
	{
		logt("STATES", "-- DISCOVERY OFF --");
		AdvertisingController::SetAdvertisingState(ADV_STATE_OFF);
		ScanController::SetScanState(SCAN_STATE_OFF);

		nextDiscoveryState = discoveryState::INVALID_STATE;

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

void Node::TimerTickHandler(u16 timerMs)
{
	passsedTimeSinceLastTimerHandler = timerMs;

	appTimerMs += timerMs;
	currentStateTimeoutMs -= timerMs;

	//FIXME: Used for debugging invalid state errors, reboot the nodes until an error occurs
	if (lookingForInvalidStateErrors)
	{
		if (appTimerMs > 25000)
		{
			NVIC_SystemReset();
		}
	}

	//logt("TIMER", "Tick, appTimer %d, stateTimeout:%d, lastDecisionTime:%d, state:%d=>%d", appTimerMs, currentStateTimeoutMs, lastDecisionTimeMs, currentDiscoveryState, nextDiscoveryState);

	//Check if we should switch states because of timeouts
	if (nextDiscoveryState != INVALID_STATE && currentStateTimeoutMs <= 0)
	{

		//Go to the next state
		ChangeState(nextDiscoveryState);
	}

	//FIXME: there should be a handshake timeout

	if (currentLedMode == LED_MODE_CONNECTIONS)
	{
		//Now we test for blinking lights
		u8 countHandshake = (cm->inConnection->handshakeDone ? 1 : 0) + (cm->outConnections[0]->handshakeDone ? 1 : 0) + (cm->outConnections[1]->handshakeDone ? 1 : 0) + (cm->outConnections[2]->handshakeDone ? 1 : 0);
		u8 countConnected = (cm->inConnection->isConnected ? 1 : 0) + (cm->outConnections[0]->isConnected ? 1 : 0) + (cm->outConnections[1]->isConnected ? 1 : 0) + (cm->outConnections[2]->isConnected ? 1 : 0);

		//Check if we want to switch one off
		if (appTimerMs - LedRed->lastStateChangeMs >= 300) LedRed->Off();
		if (appTimerMs - LedGreen->lastStateChangeMs >= 300) LedGreen->Off();
		if (appTimerMs - LedBlue->lastStateChangeMs >= 300) LedBlue->Off();

		if (appTimerMs % 2000 == 0 || appTimerMs % 2000 == 500 || appTimerMs % 2000 == 1000 || appTimerMs % 2000 == 1500)
		{
			u8 id = (appTimerMs % 2000) / 500;

			if (countHandshake == 0 && countConnected == 0)
			{
				LedRed->On();
				LedRed->lastStateChangeMs = appTimerMs;
			}
			if (countConnected > id)
			{
				LedGreen->On();
				LedGreen->lastStateChangeMs = appTimerMs;
			}
			if (countHandshake > id)
			{
				LedBlue->On();
				LedBlue->lastStateChangeMs = appTimerMs;
			}
		}
	}
}

#pragma endregion States

/*
 #########################################################################################################
 ### Radio
 #########################################################################################################
 */
#define ________________RADIO___________________

void radioNotificationHook(bool radio_active)
{
	//Node_RadioEventHandler(radio_active);
}

void Node::RadioEventHandler(bool radioActive)
{
	//Let's do some logging
	if (radioActive) radioActiveCount++;

	if (currentLedMode == LED_MODE_RADIO)
	{
		if (radioActive) LedRed->On();
		else LedRed->Off();
	}

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
 ### Qos
 #########################################################################################################
 */
#define ________________QOS___________________



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
	clusterID newId = this->persistentConfig.nodeId + this->persistentConfig.connectionLossCounter << 16;

	logt("NODE", "New cluster id generated %d", newId);
	return newId;
}

void Node::PrintStatus(void)
{
	trace("**************\n\r");
	trace("This is Node %d in clusterId:%d with clusterSize:%d, type:%d\n\r", this->persistentConfig.nodeId, this->clusterId, this->clusterSize, this->persistentConfig.deviceType);
	trace("Ack Field:%d, ChipId:%u\n\r", ackFieldDebugCopy, NRF_FICR->DEVICEID[1]);

	ble_gap_addr_t p_addr;
	sd_ble_gap_address_get(&p_addr);
	char addrString[20];
	Logger::getInstance().convertBufferToHexString(p_addr.addr, 6, addrString);

	trace("GAP Addr is %s\n\r\n\r", addrString);

	//Print connection info
	trace("CONNECTIONS (freeIn:%d, freeOut:%d, pendingPackets:%d, txBuf:%d\n\r", cm->freeInConnections, cm->freeOutConnections, cm->pendingPackets, cm->txBufferFreeCount);
	cm->inConnection->PrintStatus();
	for (int i = 0; i < Config->meshMaxOutConnections; i++)
	{
		cm->outConnections[i]->PrintStatus();
	}
}

void Node::PrintBufferStatus(void)
{
	//Print JOIN_ME buffer
	trace("\n\rJOIN_ME Buffer:\n\r");
	joinMeBufferPacket* packet;
	for (int i = 0; i < joinMePacketBuffer->_numElements; i++)
	{
		packet = (joinMeBufferPacket*) joinMePacketBuffer->PeekItemAt(i);
		trace("=> %d, clusterId:%d, clusterSize:%d, freeIn:%d, freeOut:%d, writeHandle:%d, ack:%d", packet->payload.sender, packet->payload.clusterId, packet->payload.clusterSize, packet->payload.freeInConnections, packet->payload.freeOutConnections, packet->payload.meshWriteHandle, packet->payload.ackField);
		if (packet->connectable == BLE_GAP_ADV_TYPE_ADV_IND)
		trace(" ADV_IND\n\r");
		else if (packet->connectable == BLE_GAP_ADV_TYPE_ADV_NONCONN_IND)
		trace(" NON_CONN\n\r");
		else
		trace(" OTHER\n\r");
	}

	trace("**************\n\r");
}

void Node::PrintSingleLineStatus(void)
{
	trace("NodeId: %d, clusterId:%d, clusterSize:%d (%d:%d, %d:%d, %d:%d, %d:%d)\n\r", persistentConfig.nodeId, clusterId, clusterSize, cm->inConnection->partnerId, cm->inConnection->connectedClusterSize, cm->outConnections[0]->partnerId, cm->outConnections[0]->connectedClusterSize, cm->outConnections[1]->partnerId, cm->outConnections[1]->connectedClusterSize, cm->outConnections[2]->partnerId,
			cm->outConnections[2]->connectedClusterSize);
}

void Node::UartGetStatus()
{
	ble_gap_addr_t p_addr;
	sd_ble_gap_address_get(&p_addr);

	char mac[18];
	sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", p_addr.addr[5], p_addr.addr[4], p_addr.addr[3], p_addr.addr[2], p_addr.addr[1], p_addr.addr[0]);

	uart("STATUS", "{\"module\":30, \"type\":\"response\", \"msgType\":\"status\", \"nodeId\":%d, \"mac\":\"%s\", \"clusterId\":%d, \"clusterSize\":%d, \"freeIn\":%d, \"freeOut\":%d}", persistentConfig.nodeId, mac, clusterId, clusterSize, cm->freeInConnections, cm->freeOutConnections);
}


/*
 #########################################################################################################
 ### Terminal Methods
 #########################################################################################################
 */

bool Node::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	/************* SYSTEM ***************/
	if (commandName == "RESET")
	{
		sd_nvic_SystemReset();
	}
	else if (commandName == "STARTTERM")
	{
		Terminal::promptAndEchoMode = true;
	}
	else if (commandName == "STOPTERM")
	{
		Terminal::promptAndEchoMode = false;
		Logger::getInstance().disableAll();
	}
	/************* NODE ***************/
	else if (commandName == "STATUS")
	{
		PrintStatus();
	}
	else if (commandName == "BUFFERSTAT")
	{
		PrintBufferStatus();
	}
	else if (commandName == "STAT")
	{
		PrintSingleLineStatus();
	}

	else if (commandName == "DATA")
	{
		nodeID receiverId = 0;
		if(commandArgs.size() > 0){
			if(commandArgs[0] == "sink") receiverId = NODE_ID_SHORTEST_SINK;
			else if(commandArgs[0] == "hop") receiverId = NODE_ID_HOPS_BASE + 1;
			else receiverId = NODE_ID_BROADCAST;
		}

		//Send data over all connections
		connPacketData1 data;
		data.header.messageType = MESSAGE_TYPE_DATA_1;
		data.header.sender = persistentConfig.nodeId;
		data.header.receiver = receiverId;

		data.payload.length = 7;
		data.payload.data[0] = 1;
		data.payload.data[1] = 3;
		data.payload.data[2] = 3;

		bool reliable = (commandArgs.size() == 0) ? false : true;

		cm->SendMessageToReceiver(NULL, (u8*) &data, SIZEOF_CONN_PACKET_DATA_1, reliable);
	}
	else if(commandName == "DATAL")
	{
		//Send some large data that is split over messages
		const u8 dataLength = 45;
		u8 _packet[dataLength];
		connPacketHeader* packet = (connPacketHeader*)_packet;
		packet->messageType = MESSAGE_TYPE_DATA_1;
		packet->receiver = 0;
		packet->sender = persistentConfig.nodeId;

		for(u32 i=0; i< dataLength-5; i++){
			_packet[i+5] = i+1;
		}

		cm->SendMessageToReceiver(NULL, _packet, dataLength, true);


	}
	else if (commandName == "LOSS")
	{
		//Simulate connection loss
		this->persistentConfig.connectionLossCounter++;
		clusterId = this->GenerateClusterID();
		this->UpdateJoinMePacket(NULL);

	}
	else if (commandName == "DISCOVERY")
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
	else if (commandName == "DISCOVER")
	{
		ChangeState(discoveryState::DISCOVERY_HIGH);
	}
	else if (commandName == "DISCOVERYLOW")
	{
		noNodesFoundCounter = 50;

	}
	//Trigger this to save the current node configuration
	else if (commandName == "SAVENODE")
	{
		persistentConfig.reserved++;
		Storage::getInstance().QueuedWrite((u8*) &persistentConfig, sizeof(NodeConfiguration), 0, this);

	}
	else if (commandName == "STOP")
	{
		DisableStateMachine(true);
	}
	else if (commandName == "START")
	{
		DisableStateMachine(false);
	}
	else if (commandName == "BREAK")
	{
		Config->breakpointToggleActive = !Config->breakpointToggleActive;
	}
	else if (commandName == "CONNECT")
	{

		for (int i = 0; i <NUM_TEST_DEVICES ; i++)
		{
			if (strcmp(commandArgs[0].c_str(), testDevices[i].name) == 0)
			{
				trace("Trying to connecting to node %s", testDevices[i].name);

				cm->ConnectAsMaster(testDevices[i].id, &testDevices[i].addr, 14);

			}
		}

	}
	else if (commandName == "DISCONNECT")
	{
		if (commandArgs.size() > 0)
		{
			u8 connectionNumber = atoi(commandArgs[0].c_str());

			cm->connections[connectionNumber]->Disconnect();
		}
	}

	else if (commandName == "HEAP")
	{

		Utility::CheckFreeHeap();

		return true;

	}
	else if (commandName == "MODULES")
	{

		for(u32 i=0; i<MAX_MODULE_COUNT; i++)
		{
			if(activeModules[i] != NULL) log("Module: %s", activeModules[i]->moduleName);
		}

		return true;

	}
	else if (commandName == "YOUSINK")
	{

		this->persistentConfig.deviceType = deviceTypes::DEVICE_TYPE_SINK;

		return true;

	}

	/************* UART COMMANDS ***************/
	else if (commandName == "UART_GET_STATUS")
	{
		UartGetStatus();
	}
	else if (commandName == "UART_SET_CAMPAIGN")
	{
		if (commandArgs.size() > 0){
			AdvertisingController::SetScanResponseData(this, commandArgs[0]);
		} else {
			uart_error(Logger::ARGUMENTS_WRONG);
		}
	}
	else
	{
		return false;
	}
	return true;
}

//IDs of test nodes
Node::testDevice Node::testDevices[NUM_TEST_DEVICES] = {

		{ 1650159794, 45, DEVICE_TYPE_SINK, "045", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x45, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 2267790660, 72, DEVICE_TYPE_STATIC, "072", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x72, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 931144702, 458, DEVICE_TYPE_STATIC, "458", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x58, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 1952379473, 635, DEVICE_TYPE_STATIC, "635", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x35, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 3505517882, 847, DEVICE_TYPE_STATIC, "847", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x47, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 0xFFFF, 667, DEVICE_TYPE_STATIC, "667", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x67, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 1812994605, 304, DEVICE_TYPE_STATIC, "304", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x04, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 449693942, 493, DEVICE_TYPE_STATIC, "493", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x93, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 3062062265, 309, DEVICE_TYPE_STATIC, "309", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x09, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 1040859205, 880, DEVICE_TYPE_STATIC, "880", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x80, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } }

	};

void Node::InitWithTestDeviceSettings()
{
	u8 found = 0;

	//Find our testDevice
	for(u32 i=0; i<NUM_TEST_DEVICES; i++){
		if(testDevices[i].chipID == NRF_FICR->DEVICEID[1])
		{
			persistentConfig.nodeId = testDevices[i].id;
			persistentConfig.deviceType = testDevices[i].deviceType;
			memcpy(&persistentConfig.nodeAddress, &testDevices[i].addr, sizeof(ble_gap_addr_t));
			found = 1;

			break;
		}
	}
	if(!found)
	{
		logt("ERROR", "ChipId:%u did not match any testDevice, assigning random one...", NRF_FICR->DEVICEID[1]);

		//Generate a "random" id between 1 and 15001
		persistentConfig.nodeId = (nodeID)NRF_FICR->DEVICEID[1] % 15000 + 1;
		persistentConfig.deviceType = deviceTypes::DEVICE_TYPE_STATIC;
		sd_ble_gap_address_get(&persistentConfig.nodeAddress);

	}
}
/* EOF */
