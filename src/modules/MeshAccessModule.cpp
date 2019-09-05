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

		if (allowInboundConnections) {
			BroadcastMeshAccessPacket();
		} else {
			DisableBroadcast();
		}
	} else {
		DisableBroadcast();
	}
}


void MeshAccessModule::TimerEventHandler(u16 passedTimeDs)
{
	if(!configuration.moduleActive) return;

}

void MeshAccessModule::RegisterGattService()
{
	if(gattRegistered) return;

	gattRegistered = true;

	logt("MAMOD", "Registering gatt service");

	u32 err = 0;

	//########################## At first, we register our custom service ########################
	//Add our Service UUID to the BLE stack for management
	ble_uuid128_t baseUUID128 = { MA_SERVICE_BASE_UUID };
	err = sd_ble_uuid_vs_add(&baseUUID128, &meshAccessService.serviceUuid.type);
	APP_ERROR_CHECK(err); //OK

	//Add the service
	meshAccessService.serviceUuid.uuid = MA_SERVICE_SERVICE_CHARACTERISTIC_UUID;

	err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &meshAccessService.serviceUuid, &meshAccessService.serviceHandle);
	APP_ERROR_CHECK(err); //OK

	//########################## Now we need to add a characteristic to that service ########################
	//Read and write permissions, variable length, etc...
	ble_gatts_attr_md_t rxAttributeMetadata;
	CheckedMemset(&rxAttributeMetadata, 0, sizeof(ble_gatts_attr_md_t));
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&rxAttributeMetadata.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&rxAttributeMetadata.write_perm);
	rxAttributeMetadata.vloc = BLE_GATTS_VLOC_STACK; //We currently have the value on the SoftDevice stack, we might port that to the application space
	rxAttributeMetadata.rd_auth = 0;
	rxAttributeMetadata.wr_auth = 0;
	rxAttributeMetadata.vlen = 1; //Make it a variable length attribute

	//Characteristic metadata
	ble_gatts_char_md_t rxCharacteristicMetadata;
	CheckedMemset(&rxCharacteristicMetadata, 0, sizeof(ble_gatts_char_md_t));
	rxCharacteristicMetadata.char_props.read = 0; /*Reading value permitted*/
	rxCharacteristicMetadata.char_props.write = 1; /*Writing value with Write Request permitted*/
	rxCharacteristicMetadata.char_props.write_wo_resp = 1; /*Writing value with Write Command permitted*/
	rxCharacteristicMetadata.char_props.auth_signed_wr = 0; /*Writing value with Signed Write Command not permitted*/
	rxCharacteristicMetadata.char_props.notify = 0; /*Notications of value permitted*/
	rxCharacteristicMetadata.char_props.indicate = 0; /*Indications of value not permitted*/
	rxCharacteristicMetadata.p_cccd_md = nullptr;//&clientCharacteristicConfigurationDescriptor;

	//The rxAttribute
	ble_uuid_t rxAttributeUUID;
	rxAttributeUUID.type = meshAccessService.serviceUuid.type;
	rxAttributeUUID.uuid = MA_SERVICE_RX_CHARACTERISTIC_UUID;

	ble_gatts_attr_t rxAttribute;
	CheckedMemset(&rxAttribute, 0, sizeof(ble_gatts_attr_t));
	rxAttribute.p_uuid = &rxAttributeUUID; /* The UUID of the Attribute*/
	rxAttribute.p_attr_md = &rxAttributeMetadata; /* The previously defined attribute Metadata */
	rxAttribute.max_len = MA_CHARACTERISTIC_MAX_LENGTH;
	rxAttribute.init_len = 0;
	rxAttribute.init_offs = 0;

	//Finally, add the characteristic
	err = sd_ble_gatts_characteristic_add(meshAccessService.serviceHandle, &rxCharacteristicMetadata, &rxAttribute, &meshAccessService.rxCharacteristicHandle);
	if(err != 0) logt("ERROR", "maRxHandle %u, err:%u", meshAccessService.rxCharacteristicHandle.value_handle, err);


	//##################### Add another characteristic for Sending Data via notifications ####################
	//Characteristic metadata
	ble_gatts_char_md_t txCharacteristicMetadata;
	CheckedMemset(&txCharacteristicMetadata, 0, sizeof(ble_gatts_char_md_t));
	txCharacteristicMetadata.char_props.read = 1; /*Reading value permitted*/
	txCharacteristicMetadata.char_props.write = 0; /*Writing value with Write Request permitted*/
	txCharacteristicMetadata.char_props.write_wo_resp = 0; /*Writing value with Write Command permitted*/
	txCharacteristicMetadata.char_props.auth_signed_wr = 0; /*Writing value with Signed Write Command not permitted*/
	txCharacteristicMetadata.char_props.notify = 1; /*Notications of value permitted*/
	txCharacteristicMetadata.char_props.indicate = 0; /*Indications of value not permitted*/
	txCharacteristicMetadata.p_cccd_md = nullptr; /*Default values*/

	//Attribute Metadata
	ble_gatts_attr_md_t txAttributeMetadata;
	CheckedMemset(&txAttributeMetadata, 0, sizeof(ble_gatts_attr_md_t));
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&txAttributeMetadata.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&txAttributeMetadata.write_perm);
	txAttributeMetadata.vloc = BLE_GATTS_VLOC_STACK; //We currently have the value on the SoftDevice stack, we might port that to the application space
	txAttributeMetadata.rd_auth = 0;
	txAttributeMetadata.wr_auth = 0;
	txAttributeMetadata.vlen = 1; //Make it a variable length attribute

	//Attribute UUID
	ble_uuid_t txAttributeUUID;
	txAttributeUUID.type = meshAccessService.serviceUuid.type;
	txAttributeUUID.uuid = MA_SERVICE_TX_CHARACTERISTIC_UUID;

	//TxAttribute for sending Data via Notifications
	ble_gatts_attr_t txAttribute;
	CheckedMemset(&txAttribute, 0, sizeof(ble_gatts_attr_t));
	txAttribute.p_uuid = &txAttributeUUID; /* The UUID of the Attribute*/
	txAttribute.p_attr_md = &txAttributeMetadata; /* The previously defined attribute Metadata */
	txAttribute.max_len = MA_CHARACTERISTIC_MAX_LENGTH;
	txAttribute.init_len = 0;
	txAttribute.init_offs = 0;

	//Add the attribute

	err = sd_ble_gatts_characteristic_add(meshAccessService.serviceHandle, &txCharacteristicMetadata, &txAttribute, &meshAccessService.txCharacteristicHandle);

	if(err != 0) logt("ERROR", "maTxHandle %u, err:%u", meshAccessService.txCharacteristicHandle.value_handle, err);
}

void MeshAccessModule::BroadcastMeshAccessPacket()
{
	if(!enableAdvertising){
		if(discoveryJobHandle != nullptr){
			GS->advertisingController.RemoveJob(discoveryJobHandle);
		}
		return;
	}
	
	//build advertising packet
	AdvJob job = {
		AdvJobTypes::SCHEDULED, //JobType
		5, //Slots
		0, //Delay
		MSEC_TO_UNITS((GS->node.isInBulkMode ? 1000 : 100), UNIT_0_625_MS), //AdvInterval
		0, //AdvChannel
		0, //CurrentSlots
		0, //CurrentDelay
		GapAdvType::ADV_IND, //Advertising Mode
		{0}, //AdvData
		0, //AdvDataLength
		{0}, //ScanData
		0 //ScanDataLength
	};

	//Select either the new advertising job or the already existing
	u8* buffer;
	if(discoveryJobHandle == nullptr){
		buffer = job.advData;
	} else {
		buffer = discoveryJobHandle->advData;
	}
	u8* bufferPointer = buffer;


	advStructureFlags* flags = (advStructureFlags*)bufferPointer;
	flags->len = SIZEOF_ADV_STRUCTURE_FLAGS-1; //minus length field itself
	flags->type = BLE_GAP_AD_TYPE_FLAGS;
	flags->flags = BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;

	advStructureUUID16* serviceUuidList = (advStructureUUID16*)(bufferPointer+SIZEOF_ADV_STRUCTURE_FLAGS);
	serviceUuidList->len = SIZEOF_ADV_STRUCTURE_UUID16 - 1;
	serviceUuidList->type = BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE;
	serviceUuidList->uuid = SERVICE_DATA_SERVICE_UUID16;

	advStructureMeshAccessServiceData* serviceData = (advStructureMeshAccessServiceData*)(bufferPointer+SIZEOF_ADV_STRUCTURE_FLAGS+SIZEOF_ADV_STRUCTURE_UUID16);
	serviceData->len = SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA - 1;
	serviceData->type = BLE_GAP_AD_TYPE_SERVICE_DATA;
	serviceData->uuid = SERVICE_DATA_SERVICE_UUID16;
	serviceData->messageType = SERVICE_DATA_MESSAGE_TYPE_MESH_ACCESS;
	serviceData->networkId = GS->node.configuration.networkId;
	serviceData->isEnrolled = GS->node.configuration.enrollmentState == EnrollmentState::ENROLLED;
	serviceData->isSink = GET_DEVICE_TYPE() == DeviceType::SINK ? 1 : 0;
	serviceData->isZeroKeyConnectable = IsZeroKeyConnectable(ConnectionDirection::DIRECTION_IN) ? 1 : 0;
	serviceData->serialIndex = RamConfig->GetSerialNumberIndex();
	//Insert the moduleIds that should be advertised
	memcpy(serviceData->moduleIds.getRaw(), moduleIdsToAdvertise.getRaw(), 3);

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
			BroadcastMeshAccessPacket();
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

	u16 uniqueConnId = MeshAccessConnection::ConnectAsMaster(&message->targetAddress, 10, 4, message->fmKeyId, message->key.getRaw(), (MeshAccessTunnelType)message->tunnelType);

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

MeshAccessAuthorization MeshAccessModule::CheckMeshAccessPacketAuthorization(BaseConnectionSendData* sendData, u8* data, u32 fmKeyId, DataDirection direction)
{
	connPacketHeader* packet = (connPacketHeader*)data;

	//We must always whitelist handshake packets for the MeshAccess Connection
	if(packet->messageType >= MessageType::ENCRYPT_CUSTOM_START
			&& packet->messageType <= MessageType::ENCRYPT_CUSTOM_DONE)
	{
		return MeshAccessAuthorization::WHITELIST;
	}

	//Default authorization for different keys
	//This can be further restricted by other modules, but allowing more rights is not currently possible
	if(fmKeyId == FM_KEY_ID_ZERO){
		return MeshAccessAuthorization::UNDETERMINED;
	} else if(fmKeyId == FM_KEY_ID_NODE){
		return MeshAccessAuthorization::LOCAL_ONLY;
	} else if(fmKeyId == FM_KEY_ID_NETWORK){
		return MeshAccessAuthorization::WHITELIST;
	} else if(fmKeyId >= FM_KEY_ID_USER_DERIVED_START && fmKeyId <= FM_KEY_ID_USER_DERIVED_END){
		return MeshAccessAuthorization::UNDETERMINED;
	} else if(fmKeyId == FM_KEY_ID_ORGANIZATION){
		return MeshAccessAuthorization::UNDETERMINED;
	} else if (fmKeyId == FM_KEY_ID_RESTRAINED) {
		return MeshAccessAuthorization::UNDETERMINED;
	}
	else {
		return MeshAccessAuthorization::BLACKLIST;
	}
}

MeshAccessAuthorization MeshAccessModule::CheckAuthorizationForAll(BaseConnectionSendData* sendData, u8* data, u32 fmKeyId, DataDirection direction) const
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
	// Find out if we have an active mesh connection
	// If we have an active connection, we disable broadcasting MeshAccessPackets
	// The user will still be able to connect through the mesh afterwards
	// (only if the other nodes do not do the same, which is a problem)
	if(disableIfInMesh)
	{
		MeshConnections conns = GS->cm.GetMeshConnections(ConnectionDirection::INVALID);
		for(u32 i=0; i<conns.count; i++){
			if(conns.connections[i]->handshakeDone()){
				logt("MAMOD", "In Mesh, disabling MA broadcast");
				DisableBroadcast();
				return;
			}
		}
		logt("MAMOD", "Single node, enabling MA Broadcast");
		BroadcastMeshAccessPacket();
	}
};

#ifdef TERMINAL_ENABLED
bool MeshAccessModule::TerminalCommandHandler(char* commandArgs[], u8 commandArgsSize)
{
#if IS_INACTIVE(SAVE_SPACE)
	if(TERMARGS(0, "maconn"))
	{
		if(commandArgsSize <= 1) return false;

		//Allows us to connect to any node when giving the GAP Address
		fh_ble_gap_addr_t addr;
		Logger::parseEncodedStringToBuffer(commandArgs[1], addr.addr, 6);
		addr.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
		Utility::swapBytes(addr.addr, 6);

		u32 fmKeyId = FM_KEY_ID_NETWORK;
		if(commandArgsSize > 2){
			fmKeyId = atoi(commandArgs[2]);
		}

		MeshAccessConnection::ConnectAsMaster(&addr, 10, 4, fmKeyId, nullptr, MeshAccessTunnelType::REMOTE_MESH);

		return true;
	}
	else if(TERMARGS(0, "malog"))
	{
		if (commandArgsSize >= 2)
		{
			if (strlen(commandArgs[1]) + 1 > sizeof(logWildcard))
			{
				return false;
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
		

		return true;
	}
#endif

	//React on commands, return true if handled, false otherwise
	if(commandArgsSize >= 4 && TERMARGS(0, "action") && TERMARGS(2, moduleName))
	{
		NodeId destinationNode = (TERMARGS(1,"this")) ? GS->node.configuration.nodeId : atoi(commandArgs[1]);

		//action this ma connect AA:BB:CC:DD:EE:FF fmKeyId nodeKey
		if(TERMARGS(3,"connect") && commandArgsSize >= 5)
		{
			MeshAccessModuleConnectMessage data;
			CheckedMemset(&data, 0x00, sizeof(data));
			data.fmKeyId = FM_KEY_ID_NODE;

			//Allows us to connect to any node when giving the GAP Address
			data.targetAddress.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
			Logger::parseEncodedStringToBuffer(commandArgs[4], data.targetAddress.addr, 6);
			Utility::swapBytes(data.targetAddress.addr, 6);

			if(commandArgsSize > 5) data.fmKeyId = atoi(commandArgs[5]);
			if(commandArgsSize > 6){
				Logger::parseEncodedStringToBuffer(commandArgs[6], data.key.getRaw(), 16);
			} else {
				//Set to invalid key so that the receiving node knows it should select from its own keys
				data.key.setAllBytesTo(0xFF);
			}
			data.tunnelType = (commandArgsSize > 7) ? atoi(commandArgs[7]) : 0;
			u8 requestHandle = (commandArgsSize > 8) ? atoi(commandArgs[8]) : 0;

			SendModuleActionMessage(
				MessageType::MODULE_TRIGGER_ACTION,
				destinationNode,
				(u8)MeshAccessModuleTriggerActionMessages::MA_CONNECT,
				requestHandle,
				(u8*)&data,
				SIZEOF_MA_MODULE_CONNECT_MESSAGE,
				false
			);

			return true;
		}
		else if(TERMARGS(3,"disconnect") && commandArgsSize >= 5)
		{
			MeshAccessModuleDisconnectMessage data;
			CheckedMemset(&data, 0x00, sizeof(data));

			//Allows us to connect to any node when giving the GAP Address
			data.targetAddress.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
			Logger::parseEncodedStringToBuffer(commandArgs[4], data.targetAddress.addr, 6);
			Utility::swapBytes(data.targetAddress.addr, 6);

			u8 requestHandle = (commandArgsSize > 5) ? atoi(commandArgs[5]) : 0;

			SendModuleActionMessage(
				MessageType::MODULE_TRIGGER_ACTION,
				destinationNode,
				(u8)MeshAccessModuleTriggerActionMessages::MA_DISCONNECT,
				requestHandle,
				(u8*)&data,
				SIZEOF_MA_MODULE_DISCONNECT_MESSAGE,
				false
			);

			return true;
		}
	}

	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif

void MeshAccessModule::GapAdvertisementReportEventHandler(const GapAdvertisementReportEvent& advertisementReportEvent)
{
#if IS_INACTIVE(GW_SAVE_SPACE)
	if(logNearby){
		const advPacketServiceAndDataHeader* packet = (const advPacketServiceAndDataHeader*)advertisementReportEvent.getData();
		const advStructureMeshAccessServiceData* maPacket = (const advStructureMeshAccessServiceData*)&packet->data;

		//Check if the advertising packet is an asset packet
		if (
				advertisementReportEvent.getDataLength() >= SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA
				&& packet->flags.len == SIZEOF_ADV_STRUCTURE_FLAGS-1
				&& packet->uuid.len == SIZEOF_ADV_STRUCTURE_UUID16-1
				&& packet->data.type == BLE_GAP_AD_TYPE_SERVICE_DATA
				&& packet->data.uuid == SERVICE_DATA_SERVICE_UUID16
				&& packet->data.messageType == SERVICE_DATA_MESSAGE_TYPE_MESH_ACCESS
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
}

bool MeshAccessModule::IsZeroKeyConnectable(const ConnectionDirection direction)
{
	return (GS->node.configuration.enrollmentState == EnrollmentState::NOT_ENROLLED
		|| direction == ConnectionDirection::DIRECTION_OUT)
		&& allowUnenrolledUnsecureConnections;
}
