////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// ** BlueRange Mesh – Community Edition (CE)
// ** Copyright (c) 2015-2021 MWAY DIGITAL GmbH, Germany
// ** Copyright (c) 2021-2026 BlueRange GmbH, Germany
// **
// ** This file is part of BlueRange Mesh Community Edition (formerly known as
// ** FruityMesh).
// **
// ** BlueRange Mesh Community Edition is free software: you can redistribute it
// ** and/or modify it under the terms of the GNU General Public License as
// ** published by the Free Software Foundation, either version 3 of the
// ** License, or (at your option) any later version.
// **
// ** BlueRange Mesh Community Edition is distributed in the hope that it will
// ** be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// ** See the GNU General Public License for more details.
// **
// ** You should have received a copy of the GNU General Public License along
// ** with this program. If not, see https://www.gnu.org/licenses/.
// **
// ** IMPORTANT:
// ** Any modification, extension, or derivative work of this file MUST also be
// ** licensed under the GNU General Public License v3 or later and the complete
// ** corresponding source code MUST be made available.
// **
// ** Commercial Use:
// ** If you wish to use this software without the obligations of the GPLv3
// ** (including source code disclosure), a commercial license for
// ** BlueRange Mesh OEM Edition is required.
// **
// ** License violations automatically terminate your rights under this license
// ** and may result in legal action under applicable law.
// ** For further information please use the contact form at:
// ** https://bluerange.io/en/contact
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
}

void GATTController::Init()
{
    //Initialize the nordic service discovery module (we could write that ourselves,...)
    const ErrorType err = FruityHal::DiscoveryServiceInit(GATTController::ServiceDiscoveryDoneDispatcher);
    if (err != ErrorType::SUCCESS)
    {
        logt("ERROR", "Failed to init discovery service, %u", (u32)err);
    }
}

ErrorType GATTController::DiscoverService(u16 connHandle, const FruityHal::BleGattUuid &p_uuid)
{
    logt("GATTCTRL", "Starting Service discovery %04x type %u, connHnd %u", p_uuid.uuid, p_uuid.type, connHandle);

    //Discovery only works for one connection at a time
    if (FruityHal::DiscoveryIsInProgress()) return ErrorType::BUSY;

    return FruityHal::DiscoverService(connHandle, p_uuid);
}



void GATTController::ServiceDiscoveryDoneDispatcher(FruityHal::BleGattDBDiscoveryEvent *p_evt)
{
    logt("GATTCTRL", "DB Discovery Event");

    if(p_evt->type == FruityHal::BleGattDBDiscoveryEventType::COMPLETE){
        ConnectionManager::GetInstance().GATTServiceDiscoveredHandler(p_evt->connHandle, *p_evt);
    }
}

//Throws different errors that must be handled
ErrorType GATTController::BleWriteCharacteristic(u16 connectionHandle, u16 characteristicHandle, u8* data, MessageLength dataLength, bool reliable) const
{
    logt("CONN_DATA", "TX Data size is: %d, handles(%d, %d), reliable %d", dataLength.GetRaw(), connectionHandle, characteristicHandle, reliable);

    char stringBuffer[100];
    Logger::ConvertBufferToHexString(data, dataLength.GetRaw(), stringBuffer, sizeof(stringBuffer));
    logt("CONN_DATA", "%s", stringBuffer);


    //Configure the write parameters with reliable/unreliable, writehandle, etc...
    FruityHal::BleGattWriteParams writeParameters = {};
    writeParameters.handle = characteristicHandle;
    writeParameters.offset = 0;
    writeParameters.len = dataLength;
    writeParameters.p_data = data;

    if (reliable)
    {
        writeParameters.type = FruityHal::BleGattWriteType::WRITE_REQ;

        return FruityHal::BleGattWrite(connectionHandle, writeParameters);
    }
    else
    {
        writeParameters.type = FruityHal::BleGattWriteType::WRITE_CMD;

        return FruityHal::BleGattWrite(connectionHandle, writeParameters);
    }
}

//TODO: Rewrite properly
ErrorType GATTController::BleSendNotification(u16 connectionHandle, u16 characteristicHandle, u8* data, MessageLength dataLength) const
{
    logt("CONN_DATA", "hvx Data size is: %d, handles(%d, %d)", dataLength.GetRaw(), connectionHandle, characteristicHandle);

    char stringBuffer[100];
    Logger::ConvertBufferToHexString(data, dataLength.GetRaw(), stringBuffer, sizeof(stringBuffer));
    logt("CONN_DATA", "%s", stringBuffer);


    FruityHal::BleGattWriteParams notificationParams = {};
    notificationParams.handle = characteristicHandle;
    notificationParams.offset = 0;
    notificationParams.p_data = data;
    notificationParams.len = dataLength;
    notificationParams.type = FruityHal::BleGattWriteType::NOTIFICATION;

    return FruityHal::BleGattSendNotification(connectionHandle, notificationParams);
}

GATTController & GATTController::GetInstance()
{
    return GS->gattController;
}
