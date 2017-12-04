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

#include <ResolverConnection.h>
#include <Logger.h>
#include <ConnectionManager.h>

/**
 * The ResolverConnection must first determine the correct connection type from a small handshake
 * started by the master.
 *
 * @param id
 * @param direction
 */

ResolverConnection::ResolverConnection(u8 id, ConnectionDirection direction, fh_ble_gap_addr_t* partnerAddress)
	: BaseConnection(id, direction, partnerAddress)
{
	logt("ERROR", "New Resolver Connection");

	connectionType = ConnectionTypes::CONNECTION_TYPE_RESOLVER;
}

void ResolverConnection::ConnectionSuccessfulHandler(u16 connectionHandle, u16 connInterval)
{
	BaseConnection::ConnectionSuccessfulHandler(connectionHandle, connInterval);

	connectionState = ConnectionState::CONNECTION_STATE_HANDSHAKING;
}

void ResolverConnection::ReceiveDataHandler(BaseConnectionSendData* sendData, u8* data)
{
	logt("ERROR", "rr");

	//If we receive any data, we use it to resolve the connection type
	GS->cm->ResolveConnection(this, sendData, data);
}

bool ResolverConnection::SendData(u8* data, u8 dataLength, DeliveryPriority priority, bool reliable)
{
	return false;
};

void ResolverConnection::PrintStatus()
{
	const char* directionString = (direction == CONNECTION_DIRECTION_IN) ? "IN " : "OUT";

	trace("%s RSV state:%u, Queue:%u-%u(%u), Buf%u/%u, hnd:%u" EOL, directionString, this->connectionState, (packetSendQueue->readPointer - packetSendQueue->bufferStart), (packetSendQueue->writePointer - packetSendQueue->bufferStart), packetSendQueue->_numElements, reliableBuffersFree, unreliableBuffersFree, connectionHandle);
}
