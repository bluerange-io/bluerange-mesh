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

#pragma once

#include <types.h>
#include <BaseConnection.h>



class ResolverConnection
		: public BaseConnection
{
private:
public:
	ResolverConnection(u8 id, ConnectionDirection direction, fh_ble_gap_addr_t* partnerAddress);

	void ConnectionSuccessfulHandler(u16 connectionHandle, u16 connInterval);

	void ReceiveDataHandler(BaseConnectionSendData* sendData, u8* data);

	void PrintStatus();

	bool SendData(u8* data, u8 dataLength, DeliveryPriority priority, bool reliable);

};

