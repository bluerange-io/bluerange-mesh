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

/*
 * The GATTController wraps SoftDevice calls that are needed to send messages
 * between devices. Data is transmitted through a single characteristic.
 * The handle of this characteristic is broadcasted with the discovery (JOIN_ME)
 * packets of the mesh. If a write to the mesh characteristic occurs, a handler is called.
 */

#pragma once

#include <types.h>

extern "C"{
#include <ble.h>
#include <ble_gap.h>
}

//meshServiceStruct that contains all information about the service
typedef struct meshServiceStruct_temporary
{
	u16                     		serviceHandle;
	ble_gatts_char_handles_t		testValCharacteristicHandle;
	ble_gatts_char_handles_t		sendMessageCharacteristicHandle;
	u16								sendMessageCharacteristicDescriptorHandle;
	ble_uuid_t						serviceUuid;
	u16								connectionHandle;  // Holds the current connection handle (can only be one at a time)
	bool							isNotifying;
} meshServiceStruct;


class GATTController
{
public:
	//FUNCTIONS
	static void bleMeshServiceInit(void);


	//Configure the callbacks
	static void setMessageReceivedCallback(void (*callback)(ble_evt_t* bleEvent));
	static void setHandleDiscoveredCallback(void (*callback)(u16 connectionHandle, u16 characteristicHandle));
	static void setDataTransmittedCallback(void (*callback)(ble_evt_t* bleEvent));

	static bool bleMeshServiceEventHandler(ble_evt_t * p_ble_evt);
	static void bleDiscoverHandles(u16 connectionHandle);

	static u32 bleWriteCharacteristic(u16 connectionHandle, u16 characteristicHandle, u8* data, u16 dataLength, bool reliable);


	//Returns the handle that is used to write to the mesh characteristic
	static u16 getMeshWriteHandle();

private:
	static meshServiceStruct meshService;

	static void (*messageReceivedCallback)(ble_evt_t* bleEvent);
	static void (*handleDiscoveredCallback)(u16 connectionHandle, u16 characteristicHandle);
	static void (*dataTransmittedCallback)(ble_evt_t* bleEvent);

	//Private stuff only meant as forward declaration
	static void _bleDiscoverCharacteristics(u16 startHandle, u16 endHandle);
	static void _bleServiceDiscoveryFinishedHandler(ble_evt_t* bleEvent);

	static void _bleCharacteristicDiscoveryFinishedHandler(ble_evt_t* bleEvent);


	static void meshServiceConnectHandler(ble_evt_t* bleEvent);
	static void meshServiceDisconnectHandler(ble_evt_t* bleEvent);
	static void meshServiceWriteHandler(ble_evt_t* bleEvent);
	static void attributeMissingHandler(ble_evt_t* bleEvent);

};
