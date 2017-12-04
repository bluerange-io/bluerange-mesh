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
 * The Connection manager manages all connections and schedules their packet
 * transmissions. It is the interface that must be used to send data.
 */

#pragma once

#include <GAPController.h>
#include <GATTController.h>
#include <BaseConnection.h>
#include <AppConnection.h>
#ifdef ACTIVATE_CLC_MODULE
#include <ClcAppConnection.h>
#endif
#include <MeshConnection.h>

extern "C"{
#include <ble.h>
}

typedef struct BaseConnections{
	u8 count;
	BaseConnection* connections[MAX_NUM_CONNECTIONS];
} BaseConnections;
typedef struct MeshConnections{
	u8 count;
	MeshConnection* connections[MESH_IN_CONNECTIONS + MESH_OUT_CONNECTIONS];
} MeshConnections;
#ifdef ACTIVATE_CLC_MODULE
typedef struct ClcAppConnections{
	u8 count;
	ClcAppConnection* connections[APP_IN_CONNECTIONS + APP_OUT_CONNECTIONS];
} ClcAppConnections;
#endif

typedef BaseConnection* (*ConnTypeResolver)(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8* data);

class ConnectionManager: public GAPControllerHandler, public GATTControllerHandler
{
	private:
		ConnectionManager();
		
		//Used within the send methods to put data
		void QueuePacket(BaseConnection* connection, u8* data, u8 dataLength, bool reliable);

		//Checks wether a successful connection is from a reestablishment
		BaseConnection* IsConnectionReestablishment(ble_evt_t* bleEvent);

	public:
		static ConnectionManager* getInstance(){
			if(!GS->cm){
				GS->cm = new ConnectionManager();
			}
			return GS->cm;
		}

		//This method is called when empty buffers are available and there is data to send
		void fillTransmitBuffers();

		u8 freeMeshInConnections;
		u8 freeMeshOutConnections;

		BaseConnection* pendingConnection;

		//ConnectionType Resolving
		void ResolveConnection(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8* data);


		BaseConnections GetBaseConnections(ConnectionDirection direction);
		MeshConnections GetMeshConnections(ConnectionDirection direction);
		BaseConnections GetConnectionsOfType(ConnectionTypes connectionType, ConnectionDirection direction);

		BaseConnection* allConnections[MESH_IN_CONNECTIONS + MESH_OUT_CONNECTIONS + APP_IN_CONNECTIONS + APP_OUT_CONNECTIONS];

		BaseConnection** getFreeConnectionSpot();

		void ConnectAsMaster(nodeID partnerId, fh_ble_gap_addr_t* address, u16 writeCharacteristicHandle);

		void Disconnect(u16 connectionHandle);
		void ForceDisconnectOtherMeshConnections(MeshConnection* connection);

		int ReestablishConnections();

		//Functions used for sending messages
		void SendMeshMessage(u8* data, u8 dataLength, DeliveryPriority priority, bool reliable);

		void BroadcastMeshPacket(u8* data, u8 dataLength, DeliveryPriority priority, bool reliable);

		void RouteMeshData(MeshConnection* connection, BaseConnectionSendData* sendData, u8* data);
		void BroadcastMeshData(MeshConnection* ignoreConnection, BaseConnectionSendData* sendData, u8* data);

		BaseConnection* GetConnectionFromHandle(u16 connectionHandle);

		MeshConnection* GetMeshConnectionToShortestSink(MeshConnection* excludeConnection);
		clusterSIZE GetMeshHopsToShortestSink(MeshConnection* excludeConnection);

		u16 GetPendingPackets();

		void SetMeshConnectionInterval(u16 connectionInterval);

		void DeleteConnection(BaseConnection* connection);

		//Connection callbacks
		void ConnectingTimeoutHandler(ble_evt_t* bleEvent);
		void MessageReceivedCallback(BaseConnectionSendData* sendData, u8* data);

		//These methods can be accessed by the Connection classes

		//GAPController Handlers
		void GapConnectingTimeoutHandler(ble_evt_t* bleEvent);
		void GapConnectionConnectedHandler(ble_evt_t* bleEvent);
		void GapConnectionEncryptedHandler(ble_evt_t* bleEvent);
		void GapConnectionDisconnectedHandler(ble_evt_t* bleEvent);

		//Other handler
		void FinalDisconnectionHandler(BaseConnection* connection);

		//GATTController Handlers
		void GattDataReceivedHandler(ble_evt_t* bleEvent);
		void GATTHandleDiscoveredHandler(u16 connectionHandle, u16 characteristicHandle);
		void GATTDataTransmittedHandler(ble_evt_t* bleEvent);

		void PacketSuccessfullyQueuedCallback(MeshConnection* connection, sizedData packetData);

		//Callbacks are kinda complicated, so we handle BLE events directly in this class
		void BleEventHandler(ble_evt_t* bleEvent);
		void TimerEventHandler(u16 passedTimeDs, u32 appTimerDs);

};

