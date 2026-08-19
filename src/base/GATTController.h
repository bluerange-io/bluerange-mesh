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

#pragma once

#include <FmTypes.h>
#include <FruityHal.h>

/*
 * The GATTController wraps SoftDevice calls that are needed to send messages
 * between devices. Data is transmitted through a single characteristic.
 * The handle of this characteristic is broadcasted with the discovery (JOIN_ME)
 * packets of the mesh. If a write to the mesh characteristic occurs, a handler is called.
 */
class GATTController
{
public:
    GATTController();

    void Init();

    static GATTController& GetInstance();

    //FUNCTIONS

    ErrorType BleWriteCharacteristic(u16 connectionHandle, u16 characteristicHandle, u8* data, MessageLength dataLength, bool reliable) const;
    ErrorType BleSendNotification(u16 connectionHandle, u16 characteristicHandle, u8* data, MessageLength dataLength) const;

    ErrorType DiscoverService(u16 connHandle, const FruityHal::BleGattUuid &p_uuid);

private:

    static void ServiceDiscoveryDoneDispatcher(FruityHal::BleGattDBDiscoveryEvent *p_evt);

};
