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
 * The Advertising Controller is responsible for wrapping all advertising
 * functionality and the necessary softdevice calls in one class.
 */

#pragma once

#include <types.h>
#include <adv_packets.h>
#include <Node.h>

extern "C"{
#include <ble.h>
#include <ble_gap.h>
}

class AdvertisingController
{
private:
	AdvertisingController();
	static AdvertisingController* instance;

public:

	static advState advertisingState; //The current state of advertising

	static AdvertisingController* getInstance();
	~AdvertisingController();

	//The currently used parameters for advertising
	static ble_gap_adv_params_t currentAdvertisingParams;

	//The current advertisement packet and its header
	static u8 currentAdvertisementPacket[40];
	static u8 currentScanResponsePacket[40];
	static advPacketHeader* header;
	static scanPacketHeader* scanHeader;
	static u8 currentAdvertisementPacketLength;

	static bool advertisingPacketAwaitingUpdate; //If there was an error updating the packet



	static void Initialize(u16 networkIdentifier);
	static u32 UpdateAdvertisingData(u8 messageType, sizedData* payload, bool connectable);
	static u32 SetScanResponse(sizedData* payload);
	static void SetAdvertisingState(advState newState);
	static void AdvertisingInterruptedBecauseOfIncomingConnectionHandler(void);
	static bool AdvertiseEventHandler(ble_evt_t* bleEvent);



	static bool SetScanResponseData(Node* node, string dataString);

	//FIXME: Only for testing, should be managed in a better way
	static void SetNonConnectable();


};

