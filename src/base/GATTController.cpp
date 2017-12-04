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

GATTController::GATTController()
{

}
void GATTController::setGATTControllerHandler(GATTControllerHandler* handler)
{
	gattControllerHandler = handler;
}

void GATTController::attributeMissingHandler(ble_evt_t* bleEvent)
{
	u32 err = 0;
	//Handles missing Attributes, don't know why it is needed
	err = sd_ble_gatts_sys_attr_set(bleEvent->evt.gatts_evt.conn_handle, NULL, 0, 0);
}

//Handle discovery is not currently used and should not be used during the discovery
//phase because it adds additional packets and time to the handshake. It is better
//To use the mesh write handle that is broadcasted in the join_me packet and
//is sent with the initial cluster_welcome packet
void GATTController::bleDiscoverHandles(u16 connectionHandle, ble_uuid_t* startUuid)
{
	u32 err = 0;

	//At first we need to enumerate the services
	err = sd_ble_gattc_primary_services_discover(connectionHandle, 0x0001, startUuid);
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

//Throws different errors that must be handeled
u32 GATTController::bleWriteCharacteristic(u16 connectionHandle, u16 characteristicHandle, u8* data, u16 dataLength, bool reliable)
{
	u32 err = 0;

	u8 t = ((connPacketHeader*)data)->messageType;

	if( t != 16 && t != 17 && t != 20 && t != 21 && t != 22 && t != 23 && t != 30 && t != 31 && t != 50 && t != 51 && t != 52 && t != 53 && t != 56 && t != 57 && t != 60 && t != 61 && t != 62 && t != 80 && t != 81 && t != 82 && t != 83){
		logt("ERROR", "BOOOOOOH, WRONG DATAAAAAAAAAAAAAAAAA!!!!!!!!!");
	}

	logt("CONN_DATA", "Data size is: %d, handles(%d, %d), reliable %d", dataLength, connectionHandle, characteristicHandle, reliable);

	char stringBuffer[100];
	Logger::getInstance()->convertBufferToHexString(data, dataLength, stringBuffer, 100);
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

//TODO: Rewrite properly
u32 GATTController::bleSendNotification(u16 connectionHandle, u16 characteristicHandle, u8* data, u16 dataLength)
{
	u32 err = 0;

	logt("CONN_DATA", "hvx Data size is: %d, handles(%d, %d)", dataLength, connectionHandle, characteristicHandle);

	char stringBuffer[100];
	Logger::getInstance()->convertBufferToHexString(data, dataLength, stringBuffer, 100);
	logt("CONN_DATA", "%s", stringBuffer);


	ble_gatts_hvx_params_t notificationParams;
	notificationParams.handle = characteristicHandle;
	notificationParams.offset = 0;
	notificationParams.p_data = data;
	notificationParams.p_len = &dataLength;
	notificationParams.type = BLE_GATT_HVX_NOTIFICATION;

//	NRF_SUCCESS
//	BLE_ERROR_INVALID_CONN_HANDLE
//	NRF_ERROR_INVALID_STATE
//	NRF_ERROR_INVALID_ADDR
//	NRF_ERROR_INVALID_PARAM
//	BLE_ERROR_INVALID_ATTR_HANDLE
//	BLE_ERROR_GATTS_INVALID_ATTR_TYPE
//	NRF_ERROR_NOT_FOUND
//	NRF_ERROR_DATA_SIZE
//	NRF_ERROR_BUSY
//	BLE_ERROR_GATTS_SYS_ATTR_MISSING
//	BLE_ERROR_NO_TX_PACKETS

	err = sd_ble_gatts_hvx(connectionHandle, &notificationParams);

	return err;
}

bool GATTController::bleMeshServiceEventHandler(ble_evt_t* bleEvent)
{
	u32 err = 0;

	switch (bleEvent->header.evt_id)
	{

			//Is called when a client has written to our characteristic
		case BLE_GATTS_EVT_WRITE:
		{
			if(gattControllerHandler != NULL){
				gattControllerHandler->GattDataReceivedHandler(bleEvent);
			}
			break;
		}

		case BLE_GATTS_EVT_SYS_ATTR_MISSING:
			GATTController::attributeMissingHandler(bleEvent);
			break;

			//Fired as a result of an unreliable write command
#if defined(NRF51) || defined(SIM_ENABLED)
		case BLE_EVT_TX_COMPLETE:
#elif defined(NRF52)
		case BLE_GATTS_EVT_HVN_TX_COMPLETE:
		case BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE:
#endif
		{
			if(gattControllerHandler != NULL){
				gattControllerHandler->GATTDataTransmittedHandler(bleEvent);
			}
			return true;
			break;
		}
#ifdef NRF52
		case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
		{
			//Use default MTU if requested
			err = sd_ble_gatts_exchange_mtu_reply(bleEvent->evt.gatts_evt.conn_handle, BLE_GATT_ATT_MTU_DEFAULT);

			break;
		}
#endif

		case BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP:
		{
			_bleServiceDiscoveryFinishedHandler(bleEvent);

			return true;
		}
		case BLE_GATTC_EVT_CHAR_DISC_RSP:
		{
			GS->node->CharacteristicsDiscoveredHandler(bleEvent);

			return true;
		}

			//Is called when a write has completed
		case BLE_GATTC_EVT_WRITE_RSP:
			//Send next packet after first has been received
			if(gattControllerHandler != NULL){
				gattControllerHandler->GATTDataTransmittedHandler(bleEvent);
			}

			return true;

		case BLE_GATTC_EVT_TIMEOUT:
			//A GATTC Timeout occurs if a WRITE_RSP is not received within 30s
			//This essentially marks the end of a connection, we'll have to disconnect
			logt("ERROR", "BLE_GATTC_EVT_TIMEOUT");
			Logger::getInstance()->logError(Logger::errorTypes::CUSTOM, Logger::customErrorTypes::BLE_GATTC_EVT_TIMEOUT_FORCED_US, 0);

			GAPController::getInstance()->disconnectFromPartner(bleEvent->evt.gattc_evt.conn_handle);


			return true;

		default:
			break;
	}

	return false;
}
