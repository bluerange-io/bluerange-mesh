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

/*
 * The GATTController wraps SoftDevice calls that are needed to send messages
 * between devices. Data is transmitted through a single characteristic.
 * The handle of this characteristic is broadcasted with the discovery (JOIN_ME)
 * packets of the mesh. If a write to the mesh characteristic occurs, a handler is called.
 */

#pragma once

#include <types.h>
#include <GlobalState.h>

extern "C"{
#include <ble.h>
#include <ble_gap.h>
}

class GATTControllerHandler
{
public:
		GATTControllerHandler(){};
	virtual ~GATTControllerHandler(){};

	virtual void  GattDataReceivedHandler(ble_evt_t* bleEvent) = 0;
	virtual void  GATTDataTransmittedHandler(ble_evt_t* bleEvent) = 0;

};

class GATTController
{
public:
	static GATTController* getInstance(){
		if(!GS->gattController){
			GS->gattController = new GATTController();
		}
		return GS->gattController;
	}

	//FUNCTIONS

	bool bleMeshServiceEventHandler(ble_evt_t * p_ble_evt);
	void bleDiscoverHandles(u16 connectionHandle, ble_uuid_t* startUuid);

	u32 bleWriteCharacteristic(u16 connectionHandle, u16 characteristicHandle, u8* data, u16 dataLength, bool reliable);
	u32 bleSendNotification(u16 connectionHandle, u16 characteristicHandle, u8* data, u16 dataLength);

	void setGATTControllerHandler(GATTControllerHandler* handler);

private:
	GATTController();

	GATTControllerHandler* gattControllerHandler;

	//Private stuff only meant as forward declaration
	void _bleDiscoverCharacteristics(u16 startHandle, u16 endHandle);
	void _bleServiceDiscoveryFinishedHandler(ble_evt_t* bleEvent);

	void _bleCharacteristicDiscoveryFinishedHandler(ble_evt_t* bleEvent);


	void meshServiceConnectHandler(ble_evt_t* bleEvent);
	void meshServiceDisconnectHandler(ble_evt_t* bleEvent);
	void attributeMissingHandler(ble_evt_t* bleEvent);

};
