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
 * The GAP Controller wraps SoftDevice calls for initiating and accepting connections
 * It should also provide encryption in the future.
 */

#pragma once

#include "types.h"
#include "GlobalState.h"

extern "C"{
	#include <ble.h>
}

class GAPControllerHandler
{
public:
		GAPControllerHandler(){};
	virtual ~GAPControllerHandler(){};

	virtual void  GapConnectingTimeoutHandler(ble_evt_t* bleEvent) = 0;
	virtual void  GapConnectionConnectedHandler(ble_evt_t* bleEvent) = 0;
	virtual void  GapConnectionEncryptedHandler(ble_evt_t* bleEvent) = 0;
	virtual void  GapConnectionDisconnectedHandler(ble_evt_t* bleEvent) = 0;

};

class GAPController
{

private:
	GAPController();

	//Set to true if a connection procedure is ongoing
	bool currentlyConnecting;
	GAPControllerHandler* gapControllerHandler;


public:
	static GAPController* getInstance(){
		if(!GS->gapController){
			GS->gapController = new GAPController();
		}
		return GS->gapController;
	}
	//Initialize the GAP module
	void bleConfigureGAP();

	//Configure the callbacks
	void setGAPControllerHandler(GAPControllerHandler* handler);

	//Connects to a peripheral with the specified address and calls the corresponding callbacks
	bool connectToPeripheral(fh_ble_gap_addr_t* address, u16 connectionInterval, u16 timeout);
	//Disconnects from a peripheral when given a connection handle
	void disconnectFromPartner(u16 connectionHandle);

	//Encryption
	void startEncryptingConnection(u16 connectionHandle);

	//Update the connection interval
	void RequestConnectionParameterUpdate(u16 connectionHandle, u16 minConnectionInterval, u16 maxConnectionInterval, u16 slaveLatency, u16 supervisionTimeout);



	//This handler is called with bleEvents from the softdevice
	bool bleConnectionEventHandler(ble_evt_t* bleEvent);
};

