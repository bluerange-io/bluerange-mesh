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
 * The Connection manager manages all connections and schedules their packet
 * transmissions. It is the interface that must be used to send data.
 */

#pragma once

#include <Connection.h>
#include <types.h>

extern "C"{
#include <ble.h>
}

class ConnectionManagerCallback{
	public:
		ConnectionManagerCallback();
		virtual ~ConnectionManagerCallback();
		virtual void DisconnectionHandler(Connection* connection) = 0;
		virtual void ConnectionSuccessfulHandler(ble_evt_t* bleEvent) = 0;
		virtual void ConnectingTimeoutHandler(ble_evt_t* bleEvent) = 0;
		virtual void messageReceivedCallback(connectionPacket* inPacket) = 0;
};

class ConnectionManager
{
	private:
		ConnectionManager();
		static ConnectionManager* instance;

		//Used within the send methods
		void QueuePacket(Connection* connection, u8* data, u16 dataLength, bool reliable);

		//Checks wether a successful connection is from a reestablishment
		Connection* IsConnectionReestablishment(ble_evt_t* bleEvent);

		//An outConnection is initialized before being connected (saved here during initializing phase)
		Connection* pendingConnection;

	public:
		static ConnectionManager* getInstance(){
			if(!instance){
				instance = new ConnectionManager();
			}
			return instance;
		}

		Node* node;

		ConnectionManagerCallback* connectionManagerCallback;

		//This method is called when empty buffers are available and there is data to send
		void fillTransmitBuffers();
		void fillTransmitBuffersOld();

		void setConnectionManagerCallback(ConnectionManagerCallback* cb);

		bool doHandshake;

		u8 freeInConnections;
		u8 freeOutConnections;

		Connection* inConnection;
		Connection* outConnections[MESH_OUT_CONNECTIONS];

		Connection* connections[MESH_IN_CONNECTIONS + MESH_OUT_CONNECTIONS];

		Connection* getFreeConnection();
		u32 getNumConnections();


		Connection* ConnectAsMaster(nodeID partnerId, ble_gap_addr_t* address, u16 writeCharacteristicHandle);

		void Disconnect(u16 connectionHandle);
		void ForceDisconnectOtherConnections(Connection* connection);


		int ReestablishConnections();

		//Functions used for sending messages
		bool SendMessage(Connection* connection, u8* data, u16 dataLength, bool reliable);
		void SendMessageOverConnections(Connection* ignoreConnection, u8* data, u16 dataLength, bool reliable);
		void SendMessageToReceiver(Connection* originConnection, u8* data, u16 dataLength, bool reliable);

		//Do not use this function because it will send packets to connections whose handshake is not yet finished
		bool SendHandshakeMessage(Connection* connection, u8* data, u16 dataLength, bool reliable);

		Connection* GetConnectionFromHandle(u16 connectionHandle);
		Connection* GetFreeOutConnection();

		Connection* GetConnectionToShortestSink(Connection* excludeConnection);
		clusterSIZE GetHopsToShortestSink(Connection* excludeConnection);

		u16 GetPendingPackets();

		void SetConnectionInterval(u16 connectionInterval);

		//These methods can be accessed by the Connection classes

		//GAPController Handlers
		static void DisconnectionHandler(ble_evt_t* bleEvent);
		static void ConnectionSuccessfulHandler(ble_evt_t* bleEvent);
		static void ConnectionEncryptedHandler(ble_evt_t* bleEvent);
		static void ConnectingTimeoutHandler(ble_evt_t* bleEvent);

		//Other handler
		void FinalDisconnectionHandler(Connection* connection);

		//GATTController Handlers
		static void messageReceivedCallback(ble_evt_t* bleEvent);
		static void handleDiscoveredCallback(u16 connectionHandle, u16 characteristicHandle);
		static void dataTransmittedCallback(ble_evt_t* bleEvent);

		void PacketSuccessfullyQueuedCallback(Connection* connection, sizedData packetData);

		//Callbacks are kinda complicated, so we handle BLE events directly in this class
		void BleEventHandler(ble_evt_t* bleEvent);

};

