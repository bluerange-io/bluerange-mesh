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
#include <GlobalState.h>
#include <Logger.h>
#include <Node.h>
#include <cstring>
#include "Utility.h"

GATTController::GATTController()
{
	CheckedMemset(&discoveredServices, 0x00, sizeof(ble_db_discovery_t));
}

void GATTController::Init()
{
	//Initialize the nordic service discovery module (we could write that ourselves,...)
	FruityHal::DiscovereServiceInit(GATTController::ServiceDiscoveryDoneDispatcher);
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
		ConnectionManager::getInstance().GATTServiceDiscoveredHandler(p_evt->conn_handle, *p_evt);
	}
}

//Throws different errors that must be handeled
u32 GATTController::bleWriteCharacteristic(u16 connectionHandle, u16 characteristicHandle, u8* data, u16 dataLength, bool reliable) const
{
	u32 err = 0;

	logt("CONN_DATA", "Data size is: %d, handles(%d, %d), reliable %d", dataLength, connectionHandle, characteristicHandle, reliable);

	char stringBuffer[100];
	Logger::convertBufferToHexString(data, dataLength, stringBuffer, sizeof(stringBuffer));
	logt("CONN_DATA", "%s", stringBuffer);


	//Configure the write parameters with reliable/unreliable, writehandle, etc...
	ble_gattc_write_params_t writeParameters;
	CheckedMemset(&writeParameters, 0, sizeof(ble_gattc_write_params_t));
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
	Logger::convertBufferToHexString(data, dataLength, stringBuffer, sizeof(stringBuffer));
	logt("CONN_DATA", "%s", stringBuffer);


	ble_gatts_hvx_params_t notificationParams;
	notificationParams.handle = characteristicHandle;
	notificationParams.offset = 0;
	notificationParams.p_data = data;
	notificationParams.p_len = &dataLength;
	notificationParams.type = BLE_GATT_HVX_NOTIFICATION;

	err = sd_ble_gatts_hvx(connectionHandle, &notificationParams);

	return err;
}

GATTController & GATTController::getInstance()
{
	return GS->gattController;
}

