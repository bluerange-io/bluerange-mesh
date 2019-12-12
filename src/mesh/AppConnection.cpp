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

AppConnection::AppConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr* partnerAddress)
	: BaseConnection(id, direction, partnerAddress)
{

}

//This is the generic method for sending data
bool AppConnection::SendData(const BaseConnectionSendData &sendData, u8* data)
{
	//Print packet as hex
	char stringBuffer[100];
	Logger::convertBufferToHexString(data, sendData.dataLength, stringBuffer, sizeof(stringBuffer));

	logt("APP_CONN", "PUT_PACKET:%s", stringBuffer);

	//Put packet in the queue for sending
	return QueueData(sendData, data);
}

void AppConnection::PrintStatus()
{
	const char* directionString = (direction == ConnectionDirection::DIRECTION_IN) ? "IN " : "OUT";

	trace("%s APP state:%u, Queue:%u-%u(%u), hnd:%u" EOL, directionString, (u32)this->connectionState, (packetSendQueue.readPointer - packetSendQueue.bufferStart), (packetSendQueue.writePointer - packetSendQueue.bufferStart), packetSendQueue._numElements, connectionHandle);
}

