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


#include "MeshAccessModule.h"

#include <Logger.h>
#include <Utility.h>
#include <Node.h>
#include <ConnectionManager.h>
#include <GlobalState.h>
constexpr u8 MESH_ACCESS_MODULE_CONFIG_VERSION = 2;


MeshAccessModule::MeshAccessModule()
	: Module(ModuleId::MESH_ACCESS_MODULE, "ma")
{
	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(MeshAccessModuleConfiguration);

	discoveryJobHandle = nullptr;
	logNearby = false;
	gattRegistered = false;

	//Set defaults
	ResetToDefaultConfiguration();
}

void MeshAccessModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = true;
	configuration.moduleVersion = MESH_ACCESS_MODULE_CONFIG_VERSION;

	//Set additional config values...

	moduleIdsToAdvertise.zeroData();

	SET_FEATURESET_CONFIGURATION(&configuration, this);
}

void MeshAccessModule::ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength)
{
	//Do additional initialization upon loading the config
	if(configuration.moduleActive)
	{
		RegisterGattService();
	}
	UpdateMeshAccessBroadcastPacket();
}


void MeshAccessModule::TimerEventHandler(u16 passedTimeDs)
{
}

void MeshAccessModule::RegisterGattService()
{
	if(gattRegistered) return;

	gattRegistered = true;

	logt("MAMOD", "Registering gatt service");

	u32 err = 0;

	//########################## At first, we register our custom service ########################
	//Add our Service UUID to the BLE stack for management
	err = FruityHal::BleUuidVsAdd(MA_SERVICE_BASE_UUID, &meshAccessService.serviceUuid.type);
	APP_ERROR_CHECK(err); //OK

	//Add the service
	meshAccessService.serviceUuid.uuid = MA_SERVICE_SERVICE_CHARACTERISTIC_UUID;

	err = FruityHal::BleGattServiceAdd(FruityHal::BleGattSrvcType::PRIMARY, meshAccessService.serviceUuid, &meshAccessService.serviceHandle);
	APP_ERROR_CHECK(err); //OK

	//########################## Now we need to add a characteristic to that service ########################
	//Read and write permissions, variable length, etc...
	FruityHal::BleGattAttributeMetadata rxAttributeMetadata;
	CheckedMemset(&rxAttributeMetadata, 0, sizeof(rxAttributeMetadata));
	FH_CONNECTION_SECURITY_MODE_SET_OPEN(&rxAttributeMetadata.readPerm);
	FH_CONNECTION_SECURITY_MODE_SET_OPEN(&rxAttributeMetadata.writePerm);
	rxAttributeMetadata.valueLocation = BLE_GATTS_VLOC_STACK; //We currently have the value on the SoftDevice stack, we might port that to the application space
	rxAttributeMetadata.readAuthorization = 0;
	rxAttributeMetadata.writeAuthorization = 0;
	rxAttributeMetadata.variableLength = 1; //Make it a variable length attribute

	//Characteristic metadata
	FruityHal::BleGattCharMd rxCharacteristicMetadata;
	CheckedMemset(&rxCharacteristicMetadata, 0, sizeof(rxCharacteristicMetadata));
	rxCharacteristicMetadata.charProperties.read = 0; /*Reading value permitted*/
	rxCharacteristicMetadata.charProperties.write = 1; /*Writing value with Write Request permitted*/
	rxCharacteristicMetadata.charProperties.writeWithoutResponse = 1; /*Writing value with Write Command permitted*/
	rxCharacteristicMetadata.charProperties.authSignedWrite = 0; /*Writing value with Signed Write Command not permitted*/
	rxCharacteristicMetadata.charProperties.notify = 0; /*Notications of value permitted*/
	rxCharacteristicMetadata.charProperties.indicate = 0; /*Indications of value not permitted*/
	rxCharacteristicMetadata.p_cccdMd = nullptr;//&clientCharacteristicConfigurationDescriptor;

	//The rxAttribute
	FruityHal::BleGattUuid rxAttributeUUID;
	CheckedMemset(&rxAttributeUUID, 0, sizeof(rxAttributeUUID));
	rxAttributeUUID.type = meshAccessService.serviceUuid.type;
	rxAttributeUUID.uuid = MA_SERVICE_RX_CHARACTERISTIC_UUID;

	FruityHal::BleGattAttribute rxAttribute;
	CheckedMemset(&rxAttribute, 0, sizeof(rxAttribute));
	rxAttribute.p_uuid = &rxAttributeUUID; /* The UUID of the Attribute*/
	rxAttribute.p_attributeMetadata = &rxAttributeMetadata; /* The previously defined attribute Metadata */
	rxAttribute.maxLen = MA_CHARACTERISTIC_MAX_LENGTH;
	rxAttribute.initLen = 0;
	rxAttribute.initOffset = 0;

	//Finally, add the characteristic
	err = FruityHal::BleGattCharAdd(meshAccessService.serviceHandle, rxCharacteristicMetadata, rxAttribute, meshAccessService.rxCharacteristicHandle);
	if(err != 0) logt("ERROR", "maRxHandle %u, err:%u", meshAccessService.rxCharacteristicHandle.valueHandle, err);


	//##################### Add another characteristic for Sending Data via notifications ####################
	//Characteristic metadata
	FruityHal::BleGattCharMd txCharacteristicMetadata;
	CheckedMemset(&txCharacteristicMetadata, 0, sizeof(txCharacteristicMetadata));
	txCharacteristicMetadata.charProperties.read = 1; /*Reading value permitted*/
	txCharacteristicMetadata.charProperties.write = 0; /*Writing value with Write Request permitted*/
	txCharacteristicMetadata.charProperties.writeWithoutResponse = 0; /*Writing value with Write Command permitted*/
	txCharacteristicMetadata.charProperties.authSignedWrite = 0; /*Writing value with Signed Write Command not permitted*/
	txCharacteristicMetadata.charProperties.notify = 1; /*Notications of value permitted*/
	txCharacteristicMetadata.charProperties.indicate = 0; /*Indications of value not permitted*/
	txCharacteristicMetadata.p_cccdMd = nullptr; /*Default values*/

	//Attribute Metadata
	FruityHal::BleGattAttributeMetadata txAttributeMetadata;
	CheckedMemset(&txAttributeMetadata, 0, sizeof(txAttributeMetadata));
	FH_CONNECTION_SECURITY_MODE_SET_OPEN(&txAttributeMetadata.readPerm);
	FH_CONNECTION_SECURITY_MODE_SET_OPEN(&txAttributeMetadata.writePerm);
	txAttributeMetadata.valueLocation = BLE_GATTS_VLOC_STACK; //We currently have the value on the SoftDevice stack, we might port that to the application space
	txAttributeMetadata.readAuthorization = 0;
	txAttributeMetadata.writeAuthorization = 0;
	txAttributeMetadata.variableLength = 1; //Make it a variable length attribute

	//Attribute UUID
	FruityHal::BleGattUuid txAttributeUUID;
	CheckedMemset(&txAttributeUUID, 0, sizeof(txAttributeUUID));
	txAttributeUUID.type = meshAccessService.serviceUuid.type;
	txAttributeUUID.uuid = MA_SERVICE_TX_CHARACTERISTIC_UUID;

	//TxAttribute for sending Data via Notifications
	FruityHal::BleGattAttribute txAttribute;
	CheckedMemset(&txAttribute, 0, sizeof(txAttribute));
	txAttribute.p_uuid = &txAttributeUUID; /* The UUID of the Attribute*/
	txAttribute.p_attributeMetadata = &txAttributeMetadata; /* The previously defined attribute Metadata */
	txAttribute.maxLen = MA_CHARACTERISTIC_MAX_LENGTH;
	txAttribute.initLen = 0;
	txAttribute.initOffset = 0;

	//Add the attribute

	err = FruityHal::BleGattCharAdd(meshAccessService.serviceHandle, txCharacteristicMetadata, txAttribute, meshAccessService.txCharacteristicHandle);

	if(err != 0) logt("ERROR", "maTxHandle %u, err:%u", meshAccessService.txCharacteristicHandle.valueHandle, err);
}

void MeshAccessModule::UpdateMeshAccessBroadcastPacket(u16 advIntervalMs,bool interestedInConnection)
{
	if(    !enableAdvertising
		|| !allowInboundConnections
		|| !configuration.moduleActive)
	{
		DisableBroadcast();
		return;
	}

	if (disableIfInMesh)
	{
		// Find out if we have an active mesh connection
		// If we have an active connection, we disable broadcasting MeshAccessPackets
		// The user will still be able to connect through the mesh afterwards
		// (only if the other nodes do not do the same, which is a problem)

		MeshConnections conns = GS->cm.GetMeshConnections(ConnectionDirection::INVALID);
		for (u32 i = 0; i < conns.count; i++) {
			if (conns.connections[i]->handshakeDone()) {
				logt("MAMOD", "In Mesh, disabling MA broadcast");
				DisableBroadcast();
				return;
			}
		}
	}
	
	//build advertising packet
	AdvJob job = {
		AdvJobTypes::SCHEDULED, //JobType
		5, //Slots
		0, //Delay
		0, //AdvInterval
		0, //AdvChannel
		0, //CurrentSlots
		0, //CurrentDelay
		FruityHal::BleGapAdvType::ADV_IND, //Advertising Mode
		{0}, //AdvData
		0, //AdvDataLength
		{0}, //ScanData
		0 //ScanDataLength
	};

	//Select either the new advertising job or the already existing
	AdvJob* currentJob;
	if(discoveryJobHandle == nullptr){
		currentJob = &job;
		//buffer = job.advData;
	} else {
		currentJob = discoveryJobHandle;
	}

	u8* buffer = currentJob->advData;

	currentJob->advertisingInterval = MSEC_TO_UNITS((GS->node.isInBulkMode ? 1000 : advIntervalMs), UNIT_0_625_MS);

	advStructureFlags* flags = (advStructureFlags*)buffer;
	flags->len = SIZEOF_ADV_STRUCTURE_FLAGS-1; //minus length field itself
	flags->type = BLE_GAP_AD_TYPE_FLAGS;
	flags->flags = BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;

	advStructureUUID16* serviceUuidList = (advStructureUUID16*)(buffer +SIZEOF_ADV_STRUCTURE_FLAGS);
	serviceUuidList->len = SIZEOF_ADV_STRUCTURE_UUID16 - 1;
	serviceUuidList->type = BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE;
	serviceUuidList->uuid = SERVICE_DATA_SERVICE_UUID16;

	advStructureMeshAccessServiceData* serviceData = (advStructureMeshAccessServiceData*)(buffer +SIZEOF_ADV_STRUCTURE_FLAGS+SIZEOF_ADV_STRUCTURE_UUID16);
	serviceData->data.uuid.len = SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA - 1;
	serviceData->data.uuid.type = BLE_GAP_AD_TYPE_SERVICE_DATA;
	serviceData->data.uuid.uuid = SERVICE_DATA_SERVICE_UUID16;
	serviceData->data.messageType = ServiceDataMessageType::MESH_ACCESS;
	serviceData->networkId = GS->node.configuration.networkId;
	serviceData->isEnrolled = GS->node.configuration.enrollmentState == EnrollmentState::ENROLLED;
	serviceData->isSink = GET_DEVICE_TYPE() == DeviceType::SINK ? 1 : 0;
	serviceData->isZeroKeyConnectable = IsZeroKeyConnectable(ConnectionDirection::DIRECTION_IN) ? 1 : 0;
	const u32 totalInConnections = GS->cm.GetBaseConnections(ConnectionDirection::DIRECTION_IN).count;
	serviceData->isConnectable = totalInConnections < Conf::getInstance().totalInConnections ? 1 : 0;
	serviceData->interestedInConnetion = (interestedInConnection) ? 1 : 0;
	serviceData->serialIndex = RamConfig->GetSerialNumberIndex();
	//Insert the moduleIds that should be advertised
	CheckedMemcpy(serviceData->moduleIds.getRaw(), moduleIdsToAdvertise.getRaw(), 3);

	u32 length = SIZEOF_ADV_STRUCTURE_FLAGS + SIZEOF_ADV_STRUCTURE_UUID16 + SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA;
	job.advDataLength = length;

	//Either update the job or create it if not done
	if(discoveryJobHandle == nullptr){
		discoveryJobHandle = GS->advertisingController.AddJob(job);
	} else {
		GS->advertisingController.RefreshJob(discoveryJobHandle);
	}

	//FIXME: Adv data must be worng, not advertising

	char cbuffer[100];
	Logger::convertBufferToHexString(buffer, length, cbuffer, sizeof(cbuffer));
	logt("MAMOD", "Broadcasting mesh access %s, len %u", cbuffer, length);

}

void MeshAccessModule::DisableBroadcast()
{
	GS->advertisingController.RemoveJob(discoveryJobHandle);
	discoveryJobHandle = nullptr;
}

// The other module must be loaded after this module was initialized!
void MeshAccessModule::AddModuleIdToAdvertise(ModuleId moduleId)
{
	//Check if already present
	for(u32 i=0; i<3; i++){
		if(moduleIdsToAdvertise[i] == moduleId) return;
	}
	//Insert into empty slot
	for(u32 i=0; i<3; i++){
		if(moduleIdsToAdvertise[i] == ModuleId::NODE){
			moduleIdsToAdvertise[i] = moduleId;
			UpdateMeshAccessBroadcastPacket();
			return;
		}
	}
}


#define ________________________MESSAGES_________________________


void MeshAccessModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader)
{
	//Must call superclass for handling
	Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

	if(packetHeader->messageType == MessageType::MODULE_TRIGGER_ACTION){
		connPacketModule* packet = (connPacketModule*)packetHeader;
		u16 dataFieldLength = sendData->dataLength - SIZEOF_CONN_PACKET_MODULE;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){
			MeshAccessModuleTriggerActionMessages actionType = (MeshAccessModuleTriggerActionMessages)packet->actionType;
			if(actionType == MeshAccessModuleTriggerActionMessages::MA_CONNECT){
				ReceivedMeshAccessConnectMessage(packet, sendData->dataLength);
			} else if(actionType == MeshAccessModuleTriggerActionMessages::MA_DISCONNECT){
				ReceivedMeshAccessDisconnectMessage(packet, sendData->dataLength);
			}
		}
	}
	else if(packetHeader->messageType == MessageType::MODULE_GENERAL){
		connPacketModule* packet = (connPacketModule*)packetHeader;
		u16 dataFieldLength = sendData->dataLength - SIZEOF_CONN_PACKET_MODULE;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){
			MeshAccessModuleGeneralMessages actionType = (MeshAccessModuleGeneralMessages)packet->actionType;
			if(actionType == MeshAccessModuleGeneralMessages::MA_CONNECTION_STATE){
				ReceivedMeshAccessConnectionStateMessage(packet, sendData->dataLength);
			}
		}
	}
	//If we are done with a handshake or if a cluster update is received, we inform all our meshAccessConnections
	//this way, it will know if the cluster size changed or if a gateway is now available or not
	else if (
		packetHeader->messageType == MessageType::CLUSTER_INFO_UPDATE
		&& sendData->dataLength >= sizeof(connPacketClusterInfoUpdate)
	) {
		connPacketClusterInfoUpdate* data = (connPacketClusterInfoUpdate*)packetHeader;

		logt("MAMOD", "ClusterInfo for mamod size:%u, sink:%d", data->payload.clusterSizeChange, data->payload.hopsToSink);
	}
}

void MeshAccessModule::MeshAccessMessageReceivedHandler(MeshAccessConnection* connection, BaseConnectionSendData* sendData, u8* data) const
{

}

void MeshAccessModule::ReceivedMeshAccessConnectMessage(connPacketModule* packet, u16 packetLength) const
{
	logt("MAMOD", "Received connect task");
	MeshAccessModuleConnectMessage* message = (MeshAccessModuleConnectMessage*) packet->data;

	u32 uniqueConnId = MeshAccessConnection::ConnectAsMaster(&message->targetAddress, 10, 4, message->fmKeyId, message->key.getRaw(), (MeshAccessTunnelType)message->tunnelType);

	MeshAccessConnection* conn = (MeshAccessConnection*)GS->cm.GetConnectionByUniqueId(uniqueConnId);
	if(conn != nullptr){
		//Register for changes in the connection state
		conn->connectionStateSubscriberId = packet->header.sender;
	}
}

void MeshAccessModule::ReceivedMeshAccessDisconnectMessage(connPacketModule* packet, u16 packetLength) const
{
	logt("MAMOD", "Received disconnect task");
	MeshAccessModuleDisconnectMessage* message = (MeshAccessModuleDisconnectMessage*) packet->data;

	BaseConnections conns = GS->cm.GetBaseConnections(ConnectionDirection::INVALID);

	//Look for a connection with a matchin mac address
	for(u32 i=0; i<conns.count; i++){
		BaseConnection *conn = GS->cm.allConnections[conns.connectionIndizes[i]];
		if(conn != nullptr && memcmp(&(conn->partnerAddress), &message->targetAddress, FH_BLE_SIZEOF_GAP_ADDR) == 0)
		{
			conn->DisconnectAndRemove(AppDisconnectReason::USER_REQUEST);
			break;
		}
	}
}

void MeshAccessModule::ReceivedMeshAccessConnectionStateMessage(connPacketModule* packet, u16 packetLength) const
{
	MeshAccessModuleConnectionStateMessage* message = (MeshAccessModuleConnectionStateMessage*) packet->data;

	logjson("MAMOD", "{\"nodeId\":%u,\"type\":\"ma_conn_state\",\"module\":%u,", packet->header.sender, (u32)packet->moduleId);
	logjson("MAMOD",  "\"requestHandle\":%u,\"vPartnerId\":%u,\"state\":%u}" SEP, packet->requestHandle, message->vPartnerId, message->state);
}

MeshAccessAuthorization MeshAccessModule::CheckMeshAccessPacketAuthorization(BaseConnectionSendData* sendData, u8* data, FmKeyId fmKeyId, DataDirection direction)
{
	connPacketHeader* packet = (connPacketHeader*)data;

	//We must always whitelist handshake packets for the MeshAccess Connection
	if(packet->messageType >= MessageType::ENCRYPT_CUSTOM_START
			&& packet->messageType <= MessageType::ENCRYPT_CUSTOM_DONE)
	{
		return MeshAccessAuthorization::WHITELIST;
	}

	//The dead data message type must also always be allowed so that we can inform
	//the connection partner about a malformed packet.
	if (packet->messageType == MessageType::DEAD_DATA)
	{
		return MeshAccessAuthorization::LOCAL_ONLY;
	}

	//Default authorization for different keys
	//This can be further restricted by other modules, but allowing more rights is not currently possible
	if(fmKeyId == FmKeyId::ZERO){
		return MeshAccessAuthorization::UNDETERMINED;
	} else if(fmKeyId == FmKeyId::NODE){
		return MeshAccessAuthorization::LOCAL_ONLY;
	} else if(fmKeyId == FmKeyId::NETWORK){
		return MeshAccessAuthorization::WHITELIST;
	} else if(fmKeyId >= FmKeyId::USER_DERIVED_START && fmKeyId <= FmKeyId::USER_DERIVED_END){
		return MeshAccessAuthorization::UNDETERMINED;
	} else if(fmKeyId == FmKeyId::ORGANIZATION){
		return MeshAccessAuthorization::UNDETERMINED;
	} else if (fmKeyId == FmKeyId::RESTRAINED) {
		return MeshAccessAuthorization::UNDETERMINED;
	}
	else {
		return MeshAccessAuthorization::BLACKLIST;
	}
}

MeshAccessAuthorization MeshAccessModule::CheckAuthorizationForAll(BaseConnectionSendData* sendData, u8* data, FmKeyId fmKeyId, DataDirection direction) const
{
	//Check if our partner is authorized to send this packet, so we ask
	//all of our modules if this message is whitelisted or not
	MeshAccessAuthorization auth = MeshAccessAuthorization::UNDETERMINED;
	for (u32 i = 0; i < GS->amountOfModules; i++){
		if (GS->activeModules[i]->configurationPointer->moduleActive){
			MeshAccessAuthorization newAuth = GS->activeModules[i]->CheckMeshAccessPacketAuthorization(sendData, data, fmKeyId, direction);
			if(newAuth > auth){
				auth = newAuth;
			}
		}
	}
	logt("MACONN", "Message auth is %u", (u32)auth);

	return auth;
}


#define ________________________OTHER_________________________


void MeshAccessModule::MeshConnectionChangedHandler(MeshConnection& connection)
{
	UpdateMeshAccessBroadcastPacket();
};

#ifdef TERMINAL_ENABLED
TerminalCommandHandlerReturnType MeshAccessModule::TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize)
{
#if IS_INACTIVE(SAVE_SPACE)
	if(TERMARGS(0, "maconn"))
	{
		if(commandArgsSize <= 1) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;

		//Allows us to connect to any node when giving the GAP Address
		FruityHal::BleGapAddr addr;
		CheckedMemset(&addr, 0, sizeof(addr));
		Logger::parseEncodedStringToBuffer(commandArgs[1], addr.addr, 6);
		addr.addr_type = FruityHal::BleGapAddrType::RANDOM_STATIC;
		Utility::swapBytes(addr.addr, 6);

		FmKeyId fmKeyId = FmKeyId::NETWORK;
		if(commandArgsSize > 2){
			fmKeyId = (FmKeyId)Utility::StringToU32(commandArgs[2]);
		}

		MeshAccessConnection::ConnectAsMaster(&addr, 10, 4, fmKeyId, nullptr, MeshAccessTunnelType::REMOTE_MESH);

		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if(TERMARGS(0, "malog"))
	{
		if (commandArgsSize >= 2)
		{
			if (strlen(commandArgs[1]) + 1 > sizeof(logWildcard))
			{
				return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
			}
		}
		CheckedMemset(logWildcard, 0, sizeof(logWildcard));

		if (commandArgsSize >= 2)
		{
			strcpy(logWildcard, commandArgs[1]);
			logNearby = true;
		}
		else
		{
			logNearby = !logNearby;
		}
		

		return TerminalCommandHandlerReturnType::SUCCESS;
	}
#endif

	//React on commands, return true if handled, false otherwise
	if(commandArgsSize >= 4 && TERMARGS(0, "action") && TERMARGS(2, moduleName))
	{
		NodeId destinationNode = Utility::TerminalArgumentToNodeId(commandArgs[1]);

		//action this ma connect AA:BB:CC:DD:EE:FF fmKeyId nodeKey
		if(TERMARGS(3,"connect"))
		{
			if (commandArgsSize < 5) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
			MeshAccessModuleConnectMessage data;
			CheckedMemset(&data, 0x00, sizeof(data));
			data.fmKeyId = FmKeyId::NODE;

			//Allows us to connect to any node when giving the GAP Address
			data.targetAddress.addr_type = FruityHal::BleGapAddrType::RANDOM_STATIC;
			Logger::parseEncodedStringToBuffer(commandArgs[4], data.targetAddress.addr, 6);
			Utility::swapBytes(data.targetAddress.addr, 6);

			if(commandArgsSize > 5) data.fmKeyId = (FmKeyId)Utility::StringToU32(commandArgs[5]);
			if(commandArgsSize > 6){
				Logger::parseEncodedStringToBuffer(commandArgs[6], data.key.getRaw(), 16);
			} else {
				//Set to invalid key so that the receiving node knows it should select from its own keys
				data.key.setAllBytesTo(0xFF);
			}
			data.tunnelType = (commandArgsSize > 7) ? Utility::StringToU8(commandArgs[7]) : 0;
			u8 requestHandle = (commandArgsSize > 8) ? Utility::StringToU8(commandArgs[8]) : 0;

			SendModuleActionMessage(
				MessageType::MODULE_TRIGGER_ACTION,
				destinationNode,
				(u8)MeshAccessModuleTriggerActionMessages::MA_CONNECT,
				requestHandle,
				(u8*)&data,
				SIZEOF_MA_MODULE_CONNECT_MESSAGE,
				false
			);

			return TerminalCommandHandlerReturnType::SUCCESS;
		}
		else if(TERMARGS(3,"disconnect"))
		{
			if (commandArgsSize < 5) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
			MeshAccessModuleDisconnectMessage data;
			CheckedMemset(&data, 0x00, sizeof(data));

			//Allows us to connect to any node when giving the GAP Address
			data.targetAddress.addr_type = FruityHal::BleGapAddrType::RANDOM_STATIC;
			Logger::parseEncodedStringToBuffer(commandArgs[4], data.targetAddress.addr, 6);
			Utility::swapBytes(data.targetAddress.addr, 6);

			u8 requestHandle = (commandArgsSize > 5) ? Utility::StringToU8(commandArgs[5]) : 0;

			SendModuleActionMessage(
				MessageType::MODULE_TRIGGER_ACTION,
				destinationNode,
				(u8)MeshAccessModuleTriggerActionMessages::MA_DISCONNECT,
				requestHandle,
				(u8*)&data,
				SIZEOF_MA_MODULE_DISCONNECT_MESSAGE,
				false
			);

			return TerminalCommandHandlerReturnType::SUCCESS;
		}
	}

	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif

void MeshAccessModule::GapAdvertisementReportEventHandler(const FruityHal::GapAdvertisementReportEvent& advertisementReportEvent)
{
	const advPacketServiceAndDataHeader* packet = (const advPacketServiceAndDataHeader*)advertisementReportEvent.getData();
	const advStructureMeshAccessServiceData* maPacket = (const advStructureMeshAccessServiceData*)&packet->data;
#if IS_INACTIVE(GW_SAVE_SPACE)
	if(logNearby){
		//Check if the advertising packet is an asset packet
		if (
				advertisementReportEvent.getDataLength() >= SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA
				&& packet->flags.len == SIZEOF_ADV_STRUCTURE_FLAGS-1
				&& packet->uuid.len == SIZEOF_ADV_STRUCTURE_UUID16-1
				&& packet->data.uuid.type == BLE_GAP_AD_TYPE_SERVICE_DATA
				&& packet->data.uuid.uuid == SERVICE_DATA_SERVICE_UUID16
				&& packet->data.messageType == ServiceDataMessageType::MESH_ACCESS
		){
			char serialNumber[6];
			Utility::GenerateBeaconSerialForIndex(maPacket->serialIndex, serialNumber);

			if (strstr(serialNumber, logWildcard) != nullptr) {

				logt("MAMOD", "Serial %s, Addr %02X:%02X:%02X:%02X:%02X:%02X, networkId %u, enrolled %u, sink %u, connectable %u, rssi %d",
					serialNumber,
					advertisementReportEvent.getPeerAddr()[5],
					advertisementReportEvent.getPeerAddr()[4],
					advertisementReportEvent.getPeerAddr()[3],
					advertisementReportEvent.getPeerAddr()[2],
					advertisementReportEvent.getPeerAddr()[1],
					advertisementReportEvent.getPeerAddr()[0],
					maPacket->networkId,
					maPacket->isEnrolled,
					maPacket->isSink,
					advertisementReportEvent.isConnectable(),
					advertisementReportEvent.getRssi()
				);
			}
		}
	}
#endif
	if (advertisementReportEvent.getDataLength() >= SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA
		&& packet->flags.len == SIZEOF_ADV_STRUCTURE_FLAGS - 1
		&& packet->uuid.len == SIZEOF_ADV_STRUCTURE_UUID16 - 1
		&& packet->data.uuid.type == BLE_GAP_AD_TYPE_SERVICE_DATA
		&& packet->data.uuid.uuid == SERVICE_DATA_SERVICE_UUID16
		&& packet->data.messageType == ServiceDataMessageType::MESH_ACCESS
		&& maPacket->interestedInConnetion == 1
		&& (maPacket->networkId == GS->node.configuration.networkId
			|| maPacket->networkId == 0)) {

		FruityHal::BleGapAddr addr;
		addr.addr_type = advertisementReportEvent.getPeerAddrType();
		CheckedMemcpy(addr.addr,advertisementReportEvent.getPeerAddr(),6);
		u16 connectionId = MeshAccessConnection::ConnectAsMaster(&addr, 10, 4, FmKeyId::ORGANIZATION, GS->node.configuration.organizationKey, MeshAccessTunnelType::LOCAL_MESH);
	}
}

bool MeshAccessModule::IsZeroKeyConnectable(const ConnectionDirection direction)
{
	return (GS->node.configuration.enrollmentState == EnrollmentState::NOT_ENROLLED
		|| direction == ConnectionDirection::DIRECTION_OUT)
		&& allowUnenrolledUnsecureConnections;
}
