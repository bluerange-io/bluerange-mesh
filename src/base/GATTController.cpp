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
#include <ble_db_discovery.h>
}

GATTController::GATTController()
{
	memset(&discoveredServices, 0x00, sizeof(ble_db_discovery_t));
	//Initialize the nordic service discovery module (we could write that ourselves,...)
	FruityHal::DiscovereServiceInit(GATTController::ServiceDiscoveryDoneDispatcher);
}
void GATTController::setGATTControllerHandler(GATTControllerHandler* handler)
{
	gattControllerHandler = handler;
}

u32 GATTController::DiscoverService(u16 connHandle, const ble_uuid_t &p_uuid)
{
	logt("GATTCTRL", "Starting Service discovery %04x type %u, connHnd %u", p_uuid.uuid, p_uuid.type, connHandle);

	u32 err;
	//Discovery only works for one connection at a time
	if(discoveredServices.discovery_in_progress) return NRF_ERROR_BUSY;

	err = FruityHal::DiscoverService(connHandle, p_uuid, &discoveredServices);

	return err;
}



void GATTController::ServiceDiscoveryDoneDispatcher(ble_db_discovery_evt_t *p_evt)
{
	logt("GATTCTRL", "DB Discovery Event");

	if(p_evt->evt_type == BLE_DB_DISCOVERY_COMPLETE){
		if(GS->gattController->gattControllerHandler != nullptr){
			GS->gattController->gattControllerHandler->GATTServiceDiscoveredHandler(p_evt->conn_handle, *p_evt);
		}
	}
}



void GATTController::attributeMissingHandler(const ble_evt_t &bleEvent) const
{
	u32 err = 0;
	//Handles missing Attributes, don't know why it is needed
	err = sd_ble_gatts_sys_attr_set(bleEvent.evt.gatts_evt.conn_handle, nullptr, 0, 0);
	logt("ERROR", "SysAttr %u", err);
}

//Throws different errors that must be handeled
u32 GATTController::bleWriteCharacteristic(u16 connectionHandle, u16 characteristicHandle, u8* data, u16 dataLength, bool reliable) const
{
	u32 err = 0;

	logt("CONN_DATA", "Data size is: %d, handles(%d, %d), reliable %d", dataLength, connectionHandle, characteristicHandle, reliable);

	char stringBuffer[100];
	GS->logger->convertBufferToHexString(data, dataLength, stringBuffer, sizeof(stringBuffer));
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
u32 GATTController::bleSendNotification(u16 connectionHandle, u16 characteristicHandle, u8* data, u16 dataLength) const
{
	u32 err = 0;

	logt("CONN_DATA", "hvx Data size is: %d, handles(%d, %d)", dataLength, connectionHandle, characteristicHandle);

	char stringBuffer[100];
	GS->logger->convertBufferToHexString(data, dataLength, stringBuffer, sizeof(stringBuffer));
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

bool GATTController::bleMeshServiceEventHandler(ble_evt_t &bleEvent)
{
	//Calls the Db Discovery modules event handler
#ifdef NRF51
	ble_db_discovery_on_ble_evt(&discoveredServices, &bleEvent);
#elif defined(NRF52)
	ble_db_discovery_on_ble_evt(&bleEvent, &discoveredServices);
#endif


	u32 err = 0;

	switch (bleEvent.header.evt_id)
	{

			//Is called when a client has written to our characteristic
		case BLE_GATTS_EVT_WRITE:
		case BLE_GATTC_EVT_HVX:
		{
			if(gattControllerHandler != nullptr){
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
			if(gattControllerHandler != nullptr){
				gattControllerHandler->GATTDataTransmittedHandler(bleEvent);
			}
			return true;
		}
#ifdef NRF52
		case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
		{
			//Use default MTU if requested
			err = sd_ble_gatts_exchange_mtu_reply(bleEvent.evt.gatts_evt.conn_handle, BLE_GATT_ATT_MTU_DEFAULT);

			break;
		}
#endif

			//Is called when a write has completed
		case BLE_GATTC_EVT_WRITE_RSP:
			//Send next packet after first has been received
			if(gattControllerHandler != nullptr){
				gattControllerHandler->GATTDataTransmittedHandler(bleEvent);
			}

			return true;

		case BLE_GATTC_EVT_TIMEOUT:
			//A GATTC Timeout occurs if a WRITE_RSP is not received within 30s
			//This essentially marks the end of a connection, we'll have to disconnect
			logt("ERROR", "BLE_GATTC_EVT_TIMEOUT");
			GS->logger->logError(ErrorTypes::CUSTOM, (u32)CustomErrorTypes::BLE_GATTC_EVT_TIMEOUT_FORCED_US, 0);

			GS->gapController->disconnectFromPartner(bleEvent.evt.gattc_evt.conn_handle);


			return true;

		default:
			break;
	}

	return false;
}
