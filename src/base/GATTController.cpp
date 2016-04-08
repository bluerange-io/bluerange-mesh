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

#include <GATTController.h>
#include <GAPController.h>
#include <Logger.h>
#include <Node.h>
#include <cstring>

extern "C"
{
#include <app_error.h>
#include <ble_gatt.h>
#include <ble_gattc.h>
#include <ble_gatts.h>
#include <ble_types.h>
#include <nrf_error.h>
}

meshServiceStruct GATTController::meshService;

void (*GATTController::messageReceivedCallback)(ble_evt_t* bleEvent);
void (*GATTController::handleDiscoveredCallback)(u16 connectionHandle, u16 characteristicHandle);
void (*GATTController::dataTransmittedCallback)(ble_evt_t* bleEvent);

void GATTController::bleMeshServiceInit()
{
	u32 err = 0;

	meshService.connectionHandle = BLE_CONN_HANDLE_INVALID;

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

	//Client Characteristic Configuration Descriptor, whatever....
	ble_gatts_attr_md_t clientCharacteristicConfigurationDescriptor;
	memset(&clientCharacteristicConfigurationDescriptor, 0, sizeof(ble_gatts_attr_md_t));
	clientCharacteristicConfigurationDescriptor.vloc = BLE_GATTS_VLOC_STACK;
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&clientCharacteristicConfigurationDescriptor.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&clientCharacteristicConfigurationDescriptor.write_perm);

	//Characteristic metadata, whatever....
	ble_gatts_char_md_t characteristicMetadata;
	memset(&characteristicMetadata, 0, sizeof(ble_gatts_char_md_t));
	characteristicMetadata.char_props.read = 1; /*Reading value permitted*/
	characteristicMetadata.char_props.write = 1; /*Writing value with Write Request permitted*/
	characteristicMetadata.char_props.write_wo_resp = 1; /*Writing value with Write Command permitted*/
	characteristicMetadata.char_props.auth_signed_wr = 0; /*Writing value with Signed Write Command not permitted*/
	characteristicMetadata.char_props.notify = 1; /*Notications of value permitted*/
	characteristicMetadata.char_props.indicate = 0; /*Indications of value not permitted*/
	characteristicMetadata.p_cccd_md = &clientCharacteristicConfigurationDescriptor;

	//Set human readable name
	u8 humanReadableCharacteristicDescription[] = "meshWrite";
	characteristicMetadata.p_char_user_desc = humanReadableCharacteristicDescription;
	characteristicMetadata.char_user_desc_max_size = strlen((const char*) humanReadableCharacteristicDescription);
	characteristicMetadata.char_user_desc_size = strlen((const char*) humanReadableCharacteristicDescription);


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

/**@brief Connect event handler.
 *
 * @param[in]   p_lbs       LEDButton Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
void GATTController::meshServiceConnectHandler(ble_evt_t* bleEvent)
{
	meshService.connectionHandle = bleEvent->evt.gap_evt.conn_handle;
}

/**@brief Disconnect event handler.
 *
 * @param[in]   p_lbs       LEDButton Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
void GATTController::meshServiceDisconnectHandler(ble_evt_t* bleEvent)
{
	meshService.connectionHandle = BLE_CONN_HANDLE_INVALID;
}

/**@brief Write event handler.
 *
 * @param[in]   p_lbs       LEDButton Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
void GATTController::meshServiceWriteHandler(ble_evt_t* bleEvent)
{
	//Send packet to Node which is responsible for sanity checking the packet
	messageReceivedCallback(bleEvent);

}

void GATTController::setMessageReceivedCallback(void (*callback)(ble_evt_t* bleEvent))
{
	messageReceivedCallback = callback;
}

void GATTController::setHandleDiscoveredCallback(void (*callback)(u16 connectionHandle, u16 characteristicHandle))
{
	handleDiscoveredCallback = callback;
}

void GATTController::setDataTransmittedCallback(void (*callback)(ble_evt_t* bleEvent))
{
	dataTransmittedCallback = callback;
}

void GATTController::attributeMissingHandler(ble_evt_t* bleEvent)
{
	u32 err = 0;
	//Handles missing Attributes, don't know why it is needed
	err = sd_ble_gatts_sys_attr_set(bleEvent->evt.gatts_evt.conn_handle, NULL, 0, 0);
	APP_ERROR_CHECK(err); //Unhandeled
}

//Handle discovery is not currently used and should not be used during the discovery
//phase because it adds additional packets and time to the handshake. It is better
//To use the mesh write handle that is broadcasted in the join_me packet and
//is sent with the initial cluster_welcome packet
void GATTController::bleDiscoverHandles(u16 connectionHandle)
{
	u32 err = 0;

	//At first we need to enumerate the services
	err = sd_ble_gattc_primary_services_discover(connectionHandle, 0x0001, &meshService.serviceUuid);
	APP_ERROR_CHECK(err); //Currently Unhandeled
}

//Called after bleDiscoverHandles from the stack as a response
void GATTController::_bleServiceDiscoveryFinishedHandler(ble_evt_t* bleEvent)
{
	u32 err = 0;

	//Service has been found
	if (bleEvent->evt.gattc_evt.params.prim_srvc_disc_rsp.count > 0)
	{
		logt("C", "Found service");

		//Service handle range has been found, now start discovery on characteristics
		ble_gattc_handle_range_t handleRange;
		handleRange.start_handle = bleEvent->evt.gattc_evt.params.prim_srvc_disc_rsp.services[0].handle_range.start_handle;
		handleRange.end_handle = bleEvent->evt.gattc_evt.params.prim_srvc_disc_rsp.services[0].handle_range.end_handle;

		err = sd_ble_gattc_characteristics_discover(bleEvent->evt.gattc_evt.conn_handle, &handleRange);
		APP_ERROR_CHECK(err); //Currently Unhandeled
	}
}
void GATTController::_bleCharacteristicDiscoveryFinishedHandler(ble_evt_t* bleEvent)
{
	u32 err = 0;

	//Service has been found
	if (bleEvent->evt.gattc_evt.params.char_disc_rsp.count > 0)
	{
		//Characteristics have been found (FIXME: more could be requested by calling discovery again see:https://devzone.nordicsemi.com/documentation/nrf51/4.3.0/html/group___b_l_e___g_a_t_t_c___c_h_a_r___d_i_s_c___m_s_c.html)
		if (bleEvent->evt.gattc_evt.params.char_disc_rsp.chars[0].uuid.uuid == MESH_SERVICE_CHARACTERISTIC_UUID && bleEvent->evt.gattc_evt.params.char_disc_rsp.chars[0].uuid.type == meshService.serviceUuid.type)
		{
			u16 characteristicHandle = bleEvent->evt.gattc_evt.params.char_disc_rsp.chars[0].handle_value;
			logt("C", "Found characteristic");
			handleDiscoveredCallback(bleEvent->evt.gattc_evt.conn_handle, characteristicHandle);
		}
	}
}

u16 GATTController::getMeshWriteHandle()
{
	return meshService.sendMessageCharacteristicHandle.value_handle;
}

//Throws different errors that must be handeled
u32 GATTController::bleWriteCharacteristic(u16 connectionHandle, u16 characteristicHandle, u8* data, u16 dataLength, bool reliable)
{
	u32 err = 0;

	u8 t = ((connPacketHeader*)data)->messageType;

	if( t != 20 && t != 21 && t != 22 && t != 23 && t != 30 && t != 31 && t != 50 && t != 51 && t != 52 && t != 53 && t != 56 && t != 57 && t != 60 && t != 61 && t != 62 && t != 80 && t != 81){
		logt("ERROR", "BOOOOOOH, WRONG DATAAAAAAAAAAAAAAAAA!!!!!!!!!");
	}

	logt("CONN_DATA", "Data size is: %d, handles(%d, %d), reliable %d", dataLength, connectionHandle, characteristicHandle, reliable);

	char stringBuffer[100];
	Logger::getInstance().convertBufferToHexString(data, dataLength, stringBuffer, 100);
	logt("CONN_DATA", "%s", stringBuffer);


	//Configure the write parameters with reliable/unreliable, writehandle, etc...
	ble_gattc_write_params_t writeParameters;
	memset(&writeParameters, 0, sizeof(ble_gattc_write_params_t));
	writeParameters.handle = characteristicHandle;
	writeParameters.offset = 0;
	writeParameters.len = dataLength;
	writeParameters.p_value = data;

	if (reliable)
	{
		writeParameters.write_op = BLE_GATT_OP_WRITE_REQ;

		err = sd_ble_gattc_write(connectionHandle, &writeParameters);

		return err;

	}
	else
	{
		writeParameters.write_op = BLE_GATT_OP_WRITE_CMD;

		err = sd_ble_gattc_write(connectionHandle, &writeParameters);

		return err;
	}
}

bool GATTController::bleMeshServiceEventHandler(ble_evt_t* bleEvent)
{
	u32 err = 0;

	switch (bleEvent->header.evt_id)
	{
		case BLE_GAP_EVT_CONNECTED:
			GATTController::meshServiceConnectHandler(bleEvent);
			break;

		case BLE_GAP_EVT_DISCONNECTED:
			GATTController::meshServiceDisconnectHandler(bleEvent);
			break;

			//Is called when a client has written to our characteristic
		case BLE_GATTS_EVT_WRITE:
			GATTController::meshServiceWriteHandler(bleEvent);
			return true;

		case BLE_GATTS_EVT_SYS_ATTR_MISSING:
			GATTController::attributeMissingHandler(bleEvent);
			break;

			//Fired as a result of an unreliable write command
		case BLE_EVT_TX_COMPLETE:
			dataTransmittedCallback(bleEvent);
			return true;
			break;
		case BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP:
		{
			_bleServiceDiscoveryFinishedHandler(bleEvent);

			return true;
		}
		case BLE_GATTC_EVT_CHAR_DISC_RSP:
		{
			GATTController::_bleCharacteristicDiscoveryFinishedHandler(bleEvent);

			return true;
		}

			//Is called when a write has completed
		case BLE_GATTC_EVT_WRITE_RSP:
			//Send next packet after first has been received
			dataTransmittedCallback(bleEvent);

			return true;

		case BLE_GATTC_EVT_TIMEOUT:
			//A GATTC Timeout occurs if a WRITE_RSP is not received within 30s
			//This essentially marks the end of a connection, we'll have to disconnect
			logt("ERROR", "BLE_GATTC_EVT_TIMEOUT");
			Logger::getInstance().logError(Logger::errorTypes::CUSTOM, 1, 0);

			GAPController::disconnectFromPartner(bleEvent->evt.gattc_evt.conn_handle);


			return true;

		default:
			break;
	}

	return false;
}
