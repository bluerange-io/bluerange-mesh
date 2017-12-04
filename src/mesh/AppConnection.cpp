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

#include <AppConnection.h>
#include <Logger.h>

/**
 * App Connection is the base class for all Connections with Smartphones, etc,..
 * The BaseConnection should provide some generic serialCommunication methods for communicating with
 * rx/tx Characteristics, either via notifications or via write commands / requests
 *
 * @param id
 * @param direction
 */

AppConnection::AppConnection(u8 id, ConnectionDirection direction, fh_ble_gap_addr_t* partnerAddress)
	: BaseConnection(id, direction, partnerAddress)
{

}

//This is the generic method for sending data
bool AppConnection::SendData(BaseConnectionSendData* sendData, u8* data)
{
	//Print packet as hex
	char stringBuffer[400];
	Logger::getInstance()->convertBufferToHexString(data, sendData->dataLength, stringBuffer, 400);

	logt("APP_CONN", "PUT_PACKET:%s", stringBuffer);

	//Put packet in the queue for sending
	return QueueData(sendData, data);
}

void AppConnection::PrintStatus()
{
	const char* directionString = (direction == CONNECTION_DIRECTION_IN) ? "IN " : "OUT";

	trace("%s APP state:%u, Queue:%u-%u(%u), Buf%u/%u, hnd:%u" EOL, directionString, this->connectionState, (packetSendQueue->readPointer - packetSendQueue->bufferStart), (packetSendQueue->writePointer - packetSendQueue->bufferStart), packetSendQueue->_numElements, reliableBuffersFree, unreliableBuffersFree, connectionHandle);
}

