/**
 OS_LICENSE_PLACEHOLDER
 */

#include <Config.h>

#include <Node.h>
#include <LedWrapper.h>
#include <Connection.h>
#include <SimpleBuffer.h>
#include <AdvertisingController.h>
#include <GAPController.h>
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

#ifdef ACTIVATE_DFU_MODULE
#include <DFUModule.h>
#endif

extern "C"
{
#include <time.h>
#include <stdlib.h>
#include <nrf_soc.h>
#include <app_error.h>
#include <app_timer.h>
#include <ble_hci.h>
#include <ble_radio_notification.h>
}

//Buffer that keeps a predefined number of join me packets
#define JOIN_ME_PACKET_BUFFER_MAX_ELEMENTS 5
joinMeBufferPacket raw_joinMePacketBuffer[JOIN_ME_PACKET_BUFFER_MAX_ELEMENTS];

#define MAX_JOIN_ME_PACKET_AGE_MS (15 * 1000)

Node* Node::instance;
ConnectionManager* Node::cm;

Node::Node(networkID networkId)
{
	//Initialize variables
	instance = this;

	this->clusterId = 0;
	this->clusterSize = 1;

	this->currentAckId = 0;

	this->noNodesFoundCounter = 0;
	this->passsedTimeSinceLastTimerHandler = 0;

	this->outputRawData = false;

	this->lastRadioActiveCountResetTimerMs = 0;
	this->radioActiveCount = 0;

	globalTimeSetAt = 0;
	globalTime = 0;

	//Set the current state and its timeout
	currentStateTimeoutMs = 0;
	currentDiscoveryState = discoveryState::BOOTUP;
	nextDiscoveryState = discoveryState::INVALID_STATE;
	this->appTimerMs = 0;
	this->lastDecisionTimeMs = 0;

	initializedByGateway = false;

	LedRed->Off();
	LedGreen->Off();
	LedBlue->Off();

	ledBlinkPosition = 0;



	//Register terminal listener
	Terminal::AddTerminalCommandListener(this);

	currentLedMode = Config->defaultLedMode;


	//Receive ConnectionManager events
	cm = ConnectionManager::getInstance();
	cm->setConnectionManagerCallback(this);


	//Initialize all Modules
	//Module ids start with 1, this id is also used for saving persistent
	//module configurations with the Storage class
	//Module ids must persist when nodes are updated to guearantee that the
	//same module receives the same storage slot
#ifdef ACTIVATE_DEBUG_MODULE
	activeModules[0] = new DebugModule(moduleID::DEBUG_MODULE_ID, this, cm, "debug", 1);
#endif
#ifdef ACTIVATE_DFU_MODULE
	activeModules[1] = new DFUModule(moduleID::DFU_MODULE_ID, this, cm, "dfu", 2);
#endif
#ifdef ACTIVATE_STATUS_REPORTER_MODULE
	activeModules[2] = new StatusReporterModule(moduleID::STATUS_REPORTER_MODULE_ID, this, cm, "status", 3);
#endif
#ifdef ACTIVATE_ADVERTISING_MODULE
	activeModules[3] = new AdvertisingModule(moduleID::ADVERTISING_MODULE_ID, this, cm, "adv", 4);
#endif
#ifdef ACTIVATE_SCANNING_MODULE
	activeModules[4] = new ScanningModule(moduleID::SCANNING_MODULE_ID, this, cm, "scan", 5);
#endif
#ifdef ACTIVATE_ENROLLMENT_MODULE
	activeModules[5] = new EnrollmentModule(moduleID::ENROLLMENT_MODULE_ID, this, cm, "enroll", 6);
#endif
#ifdef ACTIVATE_IO_MODULE
	activeModules[6] = new IoModule(moduleID::IO_MODULE_ID, this, cm, "io", 7);
#endif


	//Register a pre/post transmit hook for radio events
	if(Config->enableRadioNotificationHandler){
		ble_radio_notification_init(3, NRF_RADIO_NOTIFICATION_DISTANCE_800US, RadioEventHandler);
	}
	joinMePacketBuffer = new SimpleBuffer((u8*) raw_joinMePacketBuffer, sizeof(joinMeBufferPacket) * JOIN_ME_PACKET_BUFFER_MAX_ELEMENTS, sizeof(joinMeBufferPacket));

	//Fills buffer with empty packets
	for(int i=0; i<JOIN_ME_PACKET_BUFFER_MAX_ELEMENTS; i++){
		joinMePacketBuffer->Reserve();
	}


	//Load Node configuration from slot 0
	if(Config->ignorePersistentNodeConfigurationOnBoot){
		logt("NODE", "ignoring persistent config!");
		persistentConfig.version = 0xFFFFFFFF;
		ConfigurationLoadedHandler();
	} else {
		Storage::getInstance().QueuedRead((u8*) &persistentConfig, sizeof(NodeConfiguration), 0, this);
	}

}

void Node::ConfigurationLoadedHandler()
{
	u32 err;


	//If config is unset, set to default
	if (persistentConfig.version == 0xFFFFFFFF)
	{
		logt("NODE", "Config was empty, default config set");
		persistentConfig.version = 0;
		persistentConfig.connectionLossCounter = 0;
		persistentConfig.networkId = Config->meshNetworkIdentifier;
		memcpy(&persistentConfig.networkKey, &Config->meshNetworkKey, 16);
		persistentConfig.dBmRX = 10;
		persistentConfig.dBmTX = 10;

		//Get an id for our testdevices when not working with persistent storage
		InitWithTestDeviceSettings();
	}

	//Change window title of the Terminal
	SetTerminalTitle();
	logt("NODE", "====> Node %u (%s) <====", persistentConfig.nodeId, Config->serialNumber);

	//Get a random number for the connection loss counter (hard on system start,...stat)
	persistentConfig.connectionLossCounter = Utility::GetRandomInteger();

	clusterId = this->GenerateClusterID();

	//Set the BLE address so that we have the same on every startup, mostly for debugging
	err = sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &persistentConfig.nodeAddress);
	if(err != NRF_SUCCESS){
		//Can be ignored and will not happen
	}

	//Init softdevice and c libraries
	ScanController::Initialize();
	AdvertisingController::Initialize(persistentConfig.networkId);

	//Fill JOIN_ME packet with data
	this->UpdateJoinMePacket();

	//Print configuration and start node
	logt("NODE", "Config loaded nodeId:%d, connLossCount:%u, networkId:%d", persistentConfig.nodeId, persistentConfig.connectionLossCounter, persistentConfig.networkId);


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

	//FIXME: The handshake needs some kind of timeout
}

//Is called after a connection has ended its handshake
void Node::HandshakeDoneHandler(Connection* connection, bool completedAsWinner)
{

	logt("HANDSHAKE", "############ Handshake done (asWinner:%u) ###############", completedAsWinner);

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


	connection->connectionState = Connection::ConnectionState::HANDSHAKE_DONE;
	connection->connectionHandshakedTimestamp = appTimerMs;

	//Call our lovely modules
	for(int i=0; i<MAX_MODULE_COUNT; i++){
		if(activeModules[i] != NULL){
			activeModules[i]->MeshConnectionChangedHandler(connection);
		}
	}

	//Update our advertisement packet
	UpdateJoinMePacket();

	//Go back to Discovery
	ChangeState(discoveryState::DISCOVERY);
}

//TODO: part of the connection manager
void Node::HandshakeTimeoutHandler()
{
	logt("HANDSHAKE", "############ Handshake TIMEOUT ###############");

	//Disconnect the hanging connection
	for(int i=0; i<Config->meshMaxConnections; i++){
		if(cm->connections[i]->isConnected() && !cm->connections[i]->handshakeDone()){
			cm->connections[i]->Disconnect();
		}
	}

	//Go back to discovery
	ChangeState(discoveryState::DISCOVERY);
}

//TODO: part of the connection manager
//If we wanted to connect but our connection timed out (only outgoing connections)
void Node::ConnectingTimeoutHandler(ble_evt_t* bleEvent)
{
	logt("NODE", "Connecting Timeout");

	//We are leaving the discoveryState::CONNECTING state
	ChangeState(discoveryState::DISCOVERY);
}

void Node::DisconnectionHandler(ble_evt_t* bleEvent)
{
	this->persistentConfig.connectionLossCounter++;

	Connection* connection = cm->GetConnectionFromHandle(bleEvent->evt.gap_evt.conn_handle);

	//If the handshake was already done, this node was part of our cluster
	//If the local host terminated the connection, we do not count it as a cluster Size change
	if (
		connection->connectionStateBeforeDisconnection >= Connection::ConnectionState::HANDSHAKE_DONE
	){
		//CASE 1: if our partner has the connection master bit, we must dissolve
		//It may happen that the connection master bit was just passed over and that neither node has it
		//This will result in two clusters dissolving
		if (!connection->connectionMasterBit)
		{
			this->clusterId = GenerateClusterID();

			logt("HANDSHAKE", "ClusterSize Change from %d to %d", this->clusterSize, this->clusterSize - connection->connectedClusterSize);

			this->clusterSize -= connection->connectedClusterSize;

			//Inform the rest of the cluster of our new ID and size
			connPacketClusterInfoUpdate packet;
			memset((u8*)&packet, 0x00, sizeof(connPacketClusterInfoUpdate));

			packet.header.messageType = MESSAGE_TYPE_CLUSTER_INFO_UPDATE;
			packet.header.sender = this->persistentConfig.nodeId;
			packet.header.receiver = NODE_ID_BROADCAST;

			packet.payload.newClusterId = this->clusterId;
			packet.payload.clusterSizeChange = -connection->connectedClusterSize;

			SendClusterInfoUpdate(connection, &packet);


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

	//In either case, we must update our advertising packet
	UpdateJoinMePacket();

	//Go to discovery mode, and force high mode
	noNodesFoundCounter = 0;

	//At this point we can start discovery again if we are in a stable mesh
	//That has discovery disabled. IMPORTANT: Do not change states if we are in Handshake
	if(currentDiscoveryState == discoveryState::DISCOVERY_OFF){
		ChangeState(discoveryState::DISCOVERY);
	}
}

//Handles incoming cluster info update
void Node::ReceiveClusterInfoUpdate(Connection* connection, connPacketClusterInfoUpdate* packet)
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

	this->clusterId = packet->payload.newClusterId;
	outPacket.payload.newClusterId = packet->payload.newClusterId;

	//Now look if our partner has passed over the connection master bit
	if(packet->payload.connectionMasterBitHandover){
		connection->connectionMasterBit = 1;
	}


	//hops to sink are updated in the send method
	//current cluster id is updated in the send method

	SendClusterInfoUpdate(connection, &outPacket);

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

//Saves a cluster update or all connections (except the one that caused it)
//This update will then be sent by a connection as soon as the connection is ready (handshakeDone)
void Node::SendClusterInfoUpdate(Connection* ignoreConnection, connPacketClusterInfoUpdate* packet)
{
	for(int i=0; i<Config->meshMaxConnections; i++){
		if(!cm->connections[i]->isConnected() || cm->connections[i] == ignoreConnection) continue;

		packet->payload.hopsToSink = cm->GetHopsToShortestSink(cm->connections[i]);

		//Get the current packet
		connPacketClusterInfoUpdate* currentPacket = &(cm->connections[i]->currentClusterInfoUpdatePacket);

		//If another clusterUpdate message is about to be sent
		if(currentPacket->header.messageType == MESSAGE_TYPE_CLUSTER_INFO_UPDATE){
			logt("HANDSHAKE", "TO NODE %u Adding to clusterSize change:%d, id:%x, hops:%d", cm->connections[i]->partnerId, packet->payload.clusterSizeChange, packet->payload.newClusterId, packet->payload.hopsToSink);

			currentPacket->payload.clusterSizeChange += packet->payload.clusterSizeChange;
			currentPacket->payload.newClusterId = packet->payload.newClusterId; //TODO: we could intelligently choose our clusterID
			currentPacket->payload.hopsToSink = cm->GetHopsToShortestSink(cm->connections[i]);

			//Check if our connection partner has a bigger cluster on his side, if yes, hand over the connection master bit if we have it
			if(
					cm->connections[i]->connectionMasterBit
					&& cm->connections[i]->connectedClusterSize > (clusterSize - cm->connections[i]->connectedClusterSize)
			){
				currentPacket->payload.connectionMasterBitHandover = 1;
			}

		//If no other clusterUpdate message is waiting to be sent
		} else {
			logt("HANDSHAKE", "TO NODE %u clusterSize change:%d, id:%x, hops:%d", cm->connections[i]->partnerId,  packet->payload.clusterSizeChange, packet->payload.newClusterId, packet->payload.hopsToSink);
			memcpy((u8*)currentPacket, (u8*)packet, sizeof(connPacketClusterInfoUpdate));
		}
	}
	//TODO: If we call fillTransmitBuffers after a timeout, they would accumulate more,...
	cm->fillTransmitBuffers();
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
				logt("HANDSHAKE", "IN <= %d CLUSTER_INFO_UPDATE newClstId:%x, sizeChange:%d, hop:%d", connection->partnerId, packet->payload.newClusterId, packet->payload.clusterSizeChange, packet->payload.hopsToSink);
				ReceiveClusterInfoUpdate(connection, packet);

			}
			break;

		case MESSAGE_TYPE_DATA_1:
			if (dataLength >= SIZEOF_CONN_PACKET_DATA_1)
			{
				connPacketData1* packet = (connPacketData1*) data;

				logt("DATA", "IN <= %d ################## Got Data packet %d:%d:%d (len:%d) ##################", connection->partnerId, packet->payload.data[0], packet->payload.data[1], packet->payload.data[2], inPacket->dataLength);

				//tracef("data is %u/%u/%u" EOL, packet->payload.data[0], packet->payload.data[1], packet->payload.data[2]);
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

				uart("ADVINFO", "{\"sender\":\"%d\",\"addr\":\"%x:%x:%x:%x:%x:%x\",\"count\":%d,\"rssiSum\":%d}" SEP, packet->header.sender, packet->payload.peerAddress[0], packet->payload.peerAddress[1], packet->payload.peerAddress[2], packet->payload.peerAddress[3], packet->payload.peerAddress[4], packet->payload.peerAddress[5], packet->payload.packetCount, packet->payload.inverseRssiSum);

			}
			break;

		case MESSAGE_TYPE_UPDATE_CONNECTION_INTERVAL:
			if(dataLength == SIZEOF_CONN_PACKET_UPDATE_CONNECTION_INTERVAL)
			{
				connPacketUpdateConnectionInterval* packet = (connPacketUpdateConnectionInterval*) data;

				cm->SetConnectionInterval(packet->newInterval);
			}
			break;

	}

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_CONFIG)
	{
		connPacketModule* packet = (connPacketModule*) packetHeader;


		if(packet->actionType == Module::ModuleConfigMessages::GET_MODULE_LIST)
		{
			SendModuleList(packet->header.sender, 7);

		}
		else if(packet->actionType == Module::ModuleConfigMessages::MODULE_LIST)
		{

			uart("MODULE", "{\"nodeId\":%u,\"type\":\"module_list\",\"modules\":[", packet->header.sender);

			u16 moduleCount = (dataLength - SIZEOF_CONN_PACKET_MODULE) / 4;
			bool first = true;
			for(int i=0; i<moduleCount; i++){
				u8 moduleId = 0, version = 0, active = 0;
				memcpy(&moduleId, packet->data + i*4+0, 2);
				memcpy(&version, packet->data + i*4+2, 1);
				memcpy(&active, packet->data + i*4+3, 1);

				if(moduleId)
				{
					//comma seperator issue,....
					if(!first){
						uart("MODULE", ",");
					}
					uart("MODULE", "{\"id\":%u,\"version\":%u,\"active\":%u}", moduleId, version, active);

					first = false;
				}
			}
			uart("MODULE", "]}" SEP);
		}
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
void Node::UpdateJoinMePacket()
{
	SetTerminalTitle();

	//Build a JOIN_ME packet and set it in the advertisement data
	advPacketPayloadJoinMeV0 packet;

	packet.sender = this->persistentConfig.nodeId;
	packet.clusterId = this->clusterId;
	packet.clusterSize = this->clusterSize;
	packet.freeInConnections = cm->freeInConnections;
	packet.freeOutConnections = cm->freeOutConnections;

	packet.batteryRuntime = GetBatteryRuntime();
	packet.txPower = Config->radioTransmitPower;
	packet.deviceType = persistentConfig.deviceType;
	packet.hopsToSink = cm->GetHopsToShortestSink(NULL);
	packet.meshWriteHandle = GATTController::getMeshWriteHandle();

	if (currentAckId != 0)
	{
		packet.ackField = currentAckId;

	} else {
		packet.ackField = 0;
	}

	sizedData data;
	data.data = (u8*) &packet;
	data.length = SIZEOF_ADV_PACKET_PAYLOAD_JOIN_ME_V0;

	logt("JOIN", "JOIN_ME updated clusterId:%x, clusterSize:%d, freeIn:%u, freeOut:%u, handle:%u, ack:%u", packet.clusterId, packet.clusterSize, packet.freeInConnections, packet.freeOutConnections, packet.meshWriteHandle, packet.ackField);

	//Broadcast connectable advertisement if we have a free inConnection, otherwise, we can only act as master
	if (cm->inConnection->isDisconnected()) AdvertisingController::UpdateAdvertisingData(MESSAGE_TYPE_JOIN_ME_V0, &data, true);
	else AdvertisingController::UpdateAdvertisingData(MESSAGE_TYPE_JOIN_ME_V0, &data, false);
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

	u32 bestScore = 0;
	joinMeBufferPacket* bestCluster = NULL;
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
		}

		//Now, if we want to be a master in the connection, we simply answer the ad packet that
		//informs us about that cluster
		if (bestCluster != NULL)
		{
			currentAckId = 0;

			ble_gap_addr_t address;
			address.addr_type = bestCluster->bleAddressType;
			memcpy(address.addr, bestCluster->bleAddress, BLE_GAP_ADDR_LEN);

			cm->ConnectAsMaster(bestCluster->payload.sender, &address, bestCluster->payload.meshWriteHandle);

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
		logt("DISCOVERY", "Other clusters are bigger, we are going to be a slave");

		currentAckId = bestCluster->payload.clusterId;

		//CASE 1: The ack field is already set to our cluster id, we can reach each other
		//Kill connections and broadcast our preferred partner with the ack field
		//so that he connects to us
		if (bestCluster->payload.ackField == clusterId)
		{
			cm->ForceDisconnectOtherConnections(NULL);

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

	//Connection should have a minimum of stability
	if(packet->rssi < -88) return 0;

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
	u32 rssiScore = 0;
	if(packet->payload.freeOutConnections > 2){
		rssiScore = (100+packet->rssi)*(1000);
	} else {
		rssiScore = (100+packet->rssi)*(1);
	}

	//Free in connections are best, free out connections are good as well
	//TODO: RSSI should be factored into the score as well, maybe battery runtime, device type, etc...
	return packet->payload.freeInConnections * 1000 + packet->payload.freeOutConnections * 100 + rssiScore;
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

	//Connection should have a minimum of stability
	if(packet->rssi < -88) return 0;

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
		case MESSAGE_TYPE_JOIN_ME_V0:
			if (dataLength == SIZEOF_ADV_PACKET_JOIN_ME)
			{
				advPacketJoinMeV0* packet = (advPacketJoinMeV0*) data;

				logt("DISCOVERY", "JOIN_ME: sender:%u, clusterId:%x, clusterSize:%d, freeIn:%u, freeOut:%u, ack:%u", packet->payload.sender, packet->payload.clusterId, packet->payload.clusterSize, packet->payload.freeInConnections, packet->payload.freeOutConnections, packet->payload.ackField);

				//Look through the buffer and determine a space where we can put the packet in
				joinMeBufferPacket* targetBuffer = findTargetBuffer(packet);

				//Now, we have the space for our packet and we fill it with the latest information
				if (targetBuffer != NULL)
				{
					memcpy(targetBuffer->bleAddress, bleEvent->evt.gap_evt.params.connected.peer_addr.addr, BLE_GAP_ADDR_LEN);
					targetBuffer->bleAddressType = bleEvent->evt.gap_evt.params.connected.peer_addr.addr_type;

					targetBuffer->payload.clusterId = packet->payload.clusterId;
					targetBuffer->payload.clusterSize = packet->payload.clusterSize;
					targetBuffer->payload.freeInConnections = packet->payload.freeInConnections;
					targetBuffer->payload.freeOutConnections = packet->payload.freeOutConnections;
					targetBuffer->payload.sender = packet->payload.sender;
					targetBuffer->payload.meshWriteHandle = packet->payload.meshWriteHandle;
					targetBuffer->payload.ackField = packet->payload.ackField;
					targetBuffer->connectable = bleEvent->evt.gap_evt.params.adv_report.type;
					targetBuffer->rssi = bleEvent->evt.gap_evt.params.adv_report.rssi;
					targetBuffer->receivedTime = appTimerMs;
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
		targetBuffer = (joinMeBufferPacket*) joinMePacketBuffer->PeekItemAt(i);

		if (packet->payload.sender == targetBuffer->payload.sender)
		{
			logt("DISCOVERY", "Updated old buffer packet");
			return targetBuffer;
		}
	}

	//Next, we look if there's an empty space
	for (int i = 0; i < joinMePacketBuffer->_numElements; i++)
	{
		if(((joinMeBufferPacket*) joinMePacketBuffer->PeekItemAt(i))->payload.sender == 0){
			targetBuffer = (joinMeBufferPacket*) joinMePacketBuffer->PeekItemAt(i);
		}
	}

	if(targetBuffer != NULL){
		logt("DISCOVERY", "Used empty space");
		return targetBuffer;
	}

	//Next, we can overwrite the oldest packet that we saved from our own cluster
	u32 oldestTimestamp = UINT32_MAX;
	for (int i = 0; i < joinMePacketBuffer->_numElements; i++)
	{
		joinMeBufferPacket* tmpPacket = (joinMeBufferPacket*) joinMePacketBuffer->PeekItemAt(i);

		if(tmpPacket->payload.clusterId == clusterId && tmpPacket->receivedTime < oldestTimestamp){
			oldestTimestamp = tmpPacket->receivedTime;
			targetBuffer = tmpPacket;
		}
	}

	if(targetBuffer != NULL){
		logt("DISCOVERY", "Overwrote one from our own cluster");
		return targetBuffer;
	}

	//If there's still no space, we overwrite the oldest packet that we received, this will not fail
	//TODO: maybe do not use oldest one but worst candidate?? Use clusterScore on all packets to find the least interesting
	oldestTimestamp = UINT32_MAX;
	for (int i = 0; i < joinMePacketBuffer->_numElements; i++)
	{
		joinMeBufferPacket* tmpPacket = (joinMeBufferPacket*) joinMePacketBuffer->PeekItemAt(i);

		if(tmpPacket->receivedTime < oldestTimestamp){
			oldestTimestamp = tmpPacket->receivedTime;
			targetBuffer = tmpPacket;
		}
	}

	logt("DISCOVERY", "Overwrote oldest packet from different cluster");
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
	Storage::getInstance().QueuedWrite((u8*) &persistentConfig, sizeof(NodeConfiguration), 0, this);
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
		currentStateTimeoutMs = Config->meshStateTimeoutHigh;
		nextDiscoveryState = discoveryState::DECIDING;

		logt("STATES", "-- DISCOVERY HIGH --");
		AdvertisingController::SetAdvertisingState(ADV_STATE_HIGH);
		ScanController::SetScanState(SCAN_STATE_HIGH);

	}
	else if (newState == discoveryState::DISCOVERY_LOW)
	{
		currentStateTimeoutMs = Config->meshStateTimeoutLow;
		nextDiscoveryState = discoveryState::DECIDING;

		logt("STATES", "-- DISCOVERY LOW --");
		AdvertisingController::SetAdvertisingState(ADV_STATE_LOW);
		ScanController::SetScanState(SCAN_STATE_LOW);

	}
	else if (newState == discoveryState::DECIDING)
	{
		nextDiscoveryState = discoveryState::INVALID_STATE;

		logt("STATES", "-- DECIDING --");

		//Disable scanning and advertising first
		AdvertisingController::SetAdvertisingState(ADV_STATE_OFF);
		ScanController::SetScanState(SCAN_STATE_OFF);

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
	}
	else if (newState == discoveryState::BACK_OFF)
	{
		nextDiscoveryState = discoveryState::DISCOVERY;
		if(Config->meshStateTimeoutBackOff == 0) currentStateTimeoutMs = Config->meshStateTimeoutBackOff;
		else currentStateTimeoutMs = (Config->meshStateTimeoutBackOff + (Utility::GetRandomInteger() % Config->meshStateTimeoutBackOffVariance)); // 5 - 8 sec

		logt("STATES", "-- BACK OFF --");
		AdvertisingController::SetAdvertisingState(ADV_STATE_OFF);
		ScanController::SetScanState(SCAN_STATE_OFF);
	}
	else if (newState == discoveryState::CONNECTING)
	{
		//Connection will be terminated by connection procedure itself
		//This might be a timeout, or a success
		//Which will call the Handshake state
		//But we will set a high timeout in case anything fails
		currentStateTimeoutMs = 30 * 1000;
		nextDiscoveryState = discoveryState::DECIDING;


		logt("STATES", "-- CONNECT_AS_MASTER --");
		AdvertisingController::SetAdvertisingState(ADV_STATE_OFF);
		ScanController::SetScanState(SCAN_STATE_OFF);

	}
	else if (newState == discoveryState::REESTABLISHING_CONNECTION)
	{
		//Connection Manager handles reconnecting
		currentStateTimeoutMs = Config->meshExtendedConnectionTimeout;
		nextDiscoveryState = discoveryState::DECIDING;


		logt("STATES", "-- REESTABLISHING_CONNECTION --");
		AdvertisingController::SetAdvertisingState(ADV_STATE_OFF);
		ScanController::SetScanState(SCAN_STATE_OFF);

	}
	else if (newState == discoveryState::HANDSHAKE)
	{
		//Use a timeout that is high enough for the handshake to finish
		currentStateTimeoutMs = 2 * 1000;
		nextDiscoveryState = discoveryState::HANDSHAKE_TIMEOUT;


		logt("STATES", "-- HANDSHAKE --");
		AdvertisingController::SetAdvertisingState(ADV_STATE_OFF);
		ScanController::SetScanState(SCAN_STATE_OFF);


	}
	else if (newState == discoveryState::HANDSHAKE_TIMEOUT)
	{
		nextDiscoveryState = discoveryState::INVALID_STATE;

		logt("STATES", "-- HANDSHAKE TIMEOUT --");
		HandshakeTimeoutHandler();


	}
	else if (newState == discoveryState::DISCOVERY_OFF)
	{
		nextDiscoveryState = discoveryState::INVALID_STATE;


		logt("STATES", "-- DISCOVERY OFF --");
		AdvertisingController::SetAdvertisingState(ADV_STATE_OFF);
		ScanController::SetScanState(SCAN_STATE_OFF);

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

void Node::TimerTickHandler(u16 timerMs)
{
	passsedTimeSinceLastTimerHandler = timerMs;

	appTimerMs += timerMs;
	currentStateTimeoutMs -= timerMs;

	//Update our global time
	//TODO: should take care of PRESCALER register value as well
	//FIXME: This timer will wrap after around 30 days, this should be changed
	u32 rtc1, passedTime;
	app_timer_cnt_get(&rtc1);
	app_timer_cnt_diff_compute(rtc1, globalTimeSetAt, &passedTime);

	globalTime += passedTime;
	app_timer_cnt_get(&globalTimeSetAt); //Update the time that the timestamp was last updated

	//logt("TIMER", "Tick, appTimer %d, stateTimeout:%d, lastDecisionTime:%d, state:%d=>%d", appTimerMs, currentStateTimeoutMs, lastDecisionTimeMs, currentDiscoveryState, nextDiscoveryState);

	//Check if we should switch states because of timeouts
	if (nextDiscoveryState != INVALID_STATE && currentStateTimeoutMs <= 0)
	{

		//Go to the next state
		ChangeState(nextDiscoveryState);
	}


	if (currentLedMode == ledMode::LED_MODE_CONNECTIONS)
	{
		//Now we test for blinking lights
		u8 countHandshake = (cm->inConnection->handshakeDone() ? 1 : 0) + (cm->outConnections[0]->handshakeDone() ? 1 : 0) + (cm->outConnections[1]->handshakeDone() ? 1 : 0) + (cm->outConnections[2]->handshakeDone() ? 1 : 0);
		u8 countConnected = (cm->inConnection->isConnected() ? 1 : 0) + (cm->outConnections[0]->isConnected() ? 1 : 0) + (cm->outConnections[1]->isConnected() ? 1 : 0) + (cm->outConnections[2]->isConnected() ? 1 : 0);

		u8 i = ledBlinkPosition / 2;

		if(i < Config->meshMaxConnections){
			if(ledBlinkPosition % 2 == 0){
				//Connected and handshake done
				if(cm->connections[i]->handshakeDone()) { LedBlue->On(); }
				//Connected and handshake not done
				if(!cm->connections[i]->handshakeDone() && cm->connections[i]->isConnected()) { LedGreen->On(); }
				//A free connection
				if(!cm->connections[i]->isConnected()) {  }
				//No connections
				if(countHandshake == 0 && countConnected == 0) { LedRed->On(); }
			} else {
				LedRed->Off();
				LedGreen->Off();
				LedBlue->Off();
			}
		}

		ledBlinkPosition = (ledBlinkPosition + 1) % ((Config->meshMaxConnections + 2) * 2);
	}
	else if(currentLedMode == ledMode::LED_MODE_CLUSTERING)
	{
		ledBlinkPosition++;

		int c = 0;
		for(int i=0; i<NUM_TEST_COLOUR_IDS; i++){
			nodeID nodeIdFromClusterId = clusterId & 0xffff;

			if(testColourIDs[i] == nodeIdFromClusterId){
				c = (i+1) % 8;

				if(c & (1 << 0)) LedRed->On();
				else LedRed->Off();

				if(c & (1 << 1)) LedGreen->On();
				else LedGreen->Off();

				if(c & (1 << 2)) LedBlue->On();
				else LedBlue->Off();

				if(i >= 8 && ledBlinkPosition %2 == 0){
					LedRed->Off();
					LedGreen->Off();
					LedBlue->Off();
				}
			}
		}
	}
	else if(currentLedMode == ledMode::LED_MODE_ON)
	{
		LedRed->On();
		LedGreen->On();
		LedBlue->On();
	}
	else if(currentLedMode == ledMode::LED_MODE_OFF)
	{
		LedRed->Off();
		LedGreen->Off();
		LedBlue->Off();
	}
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
	clusterID newId = this->persistentConfig.nodeId + (this->persistentConfig.connectionLossCounter << 16);

	logt("NODE", "New cluster id generated %x", newId);
	return newId;
}

void Node::PrintStatus(void)
{
	u32 err;

	trace("**************" EOL);
	SetTerminalTitle();
	trace("This is Node %u in clusterId:%x with clusterSize:%d, networkId:%u" EOL, this->persistentConfig.nodeId, this->clusterId, this->clusterSize, persistentConfig.networkId);
	trace("Ack Field:%d, ChipIdA:%u, ChipIdB:%u, ConnectionLossCounter:%u, nodeType:%d" EOL, currentAckId, NRF_FICR->DEVICEID[0], NRF_FICR->DEVICEID[1], persistentConfig.connectionLossCounter, this->persistentConfig.deviceType);

	ble_gap_addr_t p_addr;
	err = sd_ble_gap_address_get(&p_addr);
	APP_ERROR_CHECK(err); //OK
	trace("GAP Addr is %02X:%02X:%02X:%02X:%02X:%02X, serial:%s" EOL EOL, p_addr.addr[5], p_addr.addr[4], p_addr.addr[3], p_addr.addr[2], p_addr.addr[1], p_addr.addr[0], Config->serialNumber);

	//Print connection info
	trace("CONNECTIONS (freeIn:%u, freeOut:%u, pendingPackets:%u" EOL, cm->freeInConnections, cm->freeOutConnections, cm->GetPendingPackets());
	cm->inConnection->PrintStatus();
	for (int i = 0; i < Config->meshMaxOutConnections; i++)
	{
		cm->outConnections[i]->PrintStatus();
	}
}

void Node::SetTerminalTitle()
{
	//Change putty terminal title
	trace("\033]0;Node %u (%s) ClusterSize:%d (%x), [%u, %u, %u, %u]\007", persistentConfig.nodeId, Config->serialNumber, clusterSize, clusterId, cm->connections[0]->partnerId, cm->connections[1]->partnerId, cm->connections[2]->partnerId, cm->connections[3]->partnerId);
}

void Node::PrintBufferStatus(void)
{
	//Print JOIN_ME buffer
	trace("JOIN_ME Buffer:" EOL);
	joinMeBufferPacket* packet;
	for (int i = 0; i < joinMePacketBuffer->_numElements; i++)
	{
		packet = (joinMeBufferPacket*) joinMePacketBuffer->PeekItemAt(i);
		trace("=> %d, clusterId:%x, clusterSize:%d, freeIn:%u, freeOut:%u, writeHandle:%u, ack:%u, rssi:%d", packet->payload.sender, packet->payload.clusterId, packet->payload.clusterSize, packet->payload.freeInConnections, packet->payload.freeOutConnections, packet->payload.meshWriteHandle, packet->payload.ackField, packet->rssi);
		if (packet->connectable == BLE_GAP_ADV_TYPE_ADV_IND)
		trace(" ADV_IND" EOL);
		else if (packet->connectable == BLE_GAP_ADV_TYPE_ADV_NONCONN_IND)
		trace(" NON_CONN" EOL);
		else
		trace(" OTHER" EOL);
	}

	trace("**************" EOL);
}

void Node::PrintSingleLineStatus(void)
{
	trace("NodeId: %u, clusterId:%x, clusterSize:%d (%d:%d, %d:%d, %d:%d, %d:%d)" EOL, persistentConfig.nodeId, clusterId, clusterSize, cm->inConnection->partnerId, cm->inConnection->connectedClusterSize, cm->outConnections[0]->partnerId, cm->outConnections[0]->connectedClusterSize, cm->outConnections[1]->partnerId, cm->outConnections[1]->connectedClusterSize, cm->outConnections[2]->partnerId,
			cm->outConnections[2]->connectedClusterSize);
}


/*
 #########################################################################################################
 ### Terminal Methods
 #########################################################################################################
 */

bool Node::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	/************* SYSTEM ***************/
	if (commandName == "reset")
	{
		sd_nvic_SystemReset(); //OK
	}
	else if (commandName == "startterm")
	{
		Terminal::promptAndEchoMode = true;
	}
	else if (commandName == "stopterm")
	{
		Terminal::promptAndEchoMode = false;
		Logger::getInstance().disableAll();
	}
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
	//Get a one-lined stat for the node
	else if (commandName == "stat")
	{
		PrintSingleLineStatus();
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
	//Send some large data that is split over a few messages
	else if(commandName == "datal")
	{
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
	//Simulate connection loss which generates a new cluster id
	else if (commandName == "loss")
	{
		this->persistentConfig.connectionLossCounter++;
		clusterId = this->GenerateClusterID();
		this->UpdateJoinMePacket();
	}
	//Set a timestamp for this node
	else if (commandName == "settime")
	{
		u64 timeStamp = (atoi(commandArgs[0].c_str()) * (u64)APP_TIMER_CLOCK_FREQ);

		//Set the time for our node
		globalTime = timeStamp;
		app_timer_cnt_get(&globalTimeSetAt);
	}
	//Display the time of this node
	else if(commandName == "gettime")
	{
		u32 rtc1;
		app_timer_cnt_get(&rtc1);

		char timestring[50];
		Logger::getInstance().convertTimestampToString(globalTime, timestring);

		trace("Time is currently %s, setAt:%d, rtc1:%u" EOL, timestring, globalTimeSetAt, rtc1);
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

		cm->SendMessageToReceiver(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_UPDATE_TIMESTAMP, true);

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
	//Save the current node configuration
	else if (commandName == "savenode")
	{
		Storage::getInstance().QueuedWrite((u8*) &persistentConfig, sizeof(NodeConfiguration), 0, this);
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
	//Clear the persistant storage of the node configuration
	else if (commandName == "clearstorage")
	{
		persistentConfig.version = 0xFFFFFFFF;
		Storage::getInstance().QueuedWrite((u8*) &persistentConfig, sizeof(NodeConfiguration), 0, this);
	}
	//This variable can be used to toggle conditional breakpoints
	else if (commandName == "break")
	{
		Config->breakpointToggleActive = !Config->breakpointToggleActive;
	}
	//Try to connect to one of the nodes in the test devices array
	else if (commandName == "connect")
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
	//Disconnect a connection by id (0-4)
	else if (commandName == "disconnect")
	{
		if (commandArgs.size() > 0)
		{
			u8 connectionNumber = atoi(commandArgs[0].c_str());

			cm->connections[connectionNumber]->Disconnect();
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
		cm->SendMessageToReceiver(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_UPDATE_CONNECTION_INTERVAL, false);
	}
	//Display the free heap
	else if (commandName == "heap")
	{
		Utility::CheckFreeHeap();
	}
	//Encrypt a connection by id
	else if (commandName == "security")
	{
		u16 connectionId = strtol(commandArgs[0].c_str(), NULL, 10);

		//Enable connection security
		GAPController::startEncryptingConnection(cm->connections[connectionId]->connectionHandle);
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
	//
	else if (commandName == "uart_scan_response")
	{
		if (commandArgs.size() > 0){
			AdvertisingController::SetScanResponseData(this, commandArgs[0]);
		} else {
			uart_error(Logger::ARGUMENTS_WRONG);
		}
	}
	//Get the status information of this node
	else if(commandName == "get_plugged_in")
	{
		uart("NODE", "{\"type\":\"plugged_in\",\"nodeId\":%u,\"serialNumber\":\"%s\"}" SEP, persistentConfig.nodeId, Config->serialNumber);
	}
	//Query all modules from any node
	else if((commandName == "get_modules") && commandArgs.size() == 1)
	{
		nodeID receiver = commandArgs[0] == "this" ? persistentConfig.nodeId : atoi(commandArgs[0].c_str());

		connPacketModule packet;
		packet.header.messageType = MESSAGE_TYPE_MODULE_CONFIG;
		packet.header.sender = persistentConfig.nodeId;
		packet.header.receiver = receiver;

		packet.moduleId = moduleID::NODE;
		packet.actionType = Module::ModuleConfigMessages::GET_MODULE_LIST;

		cm->SendMessageToReceiver(NULL, (u8*) &packet, SIZEOF_CONN_PACKET_MODULE, true);
	}
	else
	{
		return false;
	}
	return true;
}

inline void Node::SendModuleList(nodeID toNode, u8 requestHandle)
{
u8 buffer[SIZEOF_CONN_PACKET_MODULE + MAX_MODULE_COUNT*4];
		memset(buffer, 0, sizeof(buffer));

		connPacketModule* outPacket = (connPacketModule*)buffer;
		outPacket->header.messageType = MESSAGE_TYPE_MODULE_CONFIG;
		outPacket->header.sender = persistentConfig.nodeId;
		outPacket->header.receiver = toNode;

		outPacket->moduleId = moduleID::NODE;
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
		Logger::getInstance().convertBufferToHexString(buffer, SIZEOF_CONN_PACKET_MODULE + MAX_MODULE_COUNT*4, (char*)strbuffer);
		logt("MODULE", "Sending: %s", strbuffer);
*/

		cm->SendMessageToReceiver(NULL, (u8*)outPacket, SIZEOF_CONN_PACKET_MODULE + MAX_MODULE_COUNT*4, true);
}


/*
IDs for development devices:
	Use this section to map the nRF chip id to some of your desired values
	This makes it easy to deploy the same firmware to a number of nodes and have them use Fixed settings

Parameters:
	- chipID: Boot the device with this firmware, enter "status" in the terminal and copy the chipID that is read from the NRF_FICR->DEVICEID[1] register
	- nodeID: Enter the desired nodeID here (the last 3 digits of the segger id for example)
	- deviceType: whether the node is a data endpoint, moving around or static
	- string representation of the node id for the terminal
	- desired BLE access address: Must comply to the spec (only modify the first byte for starters)
*/
Node::testDevice Node::testDevices[NUM_TEST_DEVICES] = {

		{ 1650159794, 45, DEVICE_TYPE_SINK, "045", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x45, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 2267790660, 72, DEVICE_TYPE_SINK, "072", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x72, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 931144702, 458, DEVICE_TYPE_STATIC, "458", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x58, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 1952379473, 635, DEVICE_TYPE_STATIC, "635", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x35, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 3505517882, 847, DEVICE_TYPE_STATIC, "847", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x47, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 0xFFFF, 667, DEVICE_TYPE_STATIC, "667", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x67, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 1812994605, 304, DEVICE_TYPE_STATIC, "304", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x04, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 449693942, 493, DEVICE_TYPE_STATIC, "493", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x93, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 3062062265, 309, DEVICE_TYPE_STATIC, "309", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x09, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } },

		{ 1040859205, 880, DEVICE_TYPE_STATIC, "880", {BLE_GAP_ADDR_TYPE_RANDOM_STATIC, { 0x80, 0x16, 0xE8, 0x52, 0x4E, 0xc0 } } }

	};

nodeID Node::testColourIDs[NUM_TEST_COLOUR_IDS] = {
		45,
		880,
		304,
		4290,
		9115,
		309,

		14980,
		2807,
		583,
		6574,
		12583,
		6388

};

//Uses the testDevice array and copies the configured values to the node settings
void Node::InitWithTestDeviceSettings()
{
	u32 err;
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
		if(Config->defaultNodeId == 0) persistentConfig.nodeId = (nodeID)NRF_FICR->DEVICEID[1] % 15000 + 1;
		else persistentConfig.nodeId = Config->defaultNodeId;

		persistentConfig.deviceType = deviceTypes::DEVICE_TYPE_STATIC;
		err = sd_ble_gap_address_get(&persistentConfig.nodeAddress);
		APP_ERROR_CHECK(err); //OK
	}


	persistentConfig.manufacturerId = 0xFFFF;

}

u8 Node::GetBatteryRuntime()
{
	//TODO: implement, measurement can be done in here or sampled periodically
	//If measurement is done in here, we should save the last measurement and only update it after
	//some time has passed
	return 7;
}

/* EOF */
