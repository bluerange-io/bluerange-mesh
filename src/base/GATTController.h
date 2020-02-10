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

/*
 * The GATTController wraps SoftDevice calls that are needed to send messages
 * between devices. Data is transmitted through a single characteristic.
 * The handle of this characteristic is broadcasted with the discovery (JOIN_ME)
 * packets of the mesh. If a write to the mesh characteristic occurs, a handler is called.
 */


#pragma once

#include <types.h>
#include <FruityHal.h>

class GATTController
{
public:
	GATTController();

	void Init();

	static GATTController& getInstance();

	//FUNCTIONS

	void bleDiscoverHandlesOld(u16 connectionHandle, ble_uuid_t* startUuid);

	u32 bleWriteCharacteristic(u16 connectionHandle, u16 characteristicHandle, u8* data, u16 dataLength, bool reliable) const;
	u32 bleSendNotification(u16 connectionHandle, u16 characteristicHandle, u8* data, u16 dataLength) const;

	u32 DiscoverService(u16 connHandle, const FruityHal::BleGattUuid &p_uuid);

private:

	static void ServiceDiscoveryDoneDispatcher(FruityHal::BleGattDBDiscoveryEvent *p_evt);

};
