////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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

/*
 * The Connection manager manages all connections and schedules their packet
 * transmissions. It is the interface that must be used to send data.
 */

#pragma once

#include <GAPController.h>
#include <GATTController.h>
#include <BaseConnection.h>
#include <MeshConnection.h>

typedef struct BaseConnections {
	u8 count;
	u32 connectionIndizes[TOTAL_NUM_CONNECTIONS];
} BaseConnections;
typedef struct MeshConnections {
	u8 count;
	MeshConnection* connections[TOTAL_NUM_CONNECTIONS];
} MeshConnections;

typedef BaseConnection* (*ConnTypeResolver)(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8 const * data);

class ConnectionManager
{
private:
	//Used within the send methods to put data
	void QueuePacket(BaseConnection* connection, u8* data, u16 dataLength, bool reliable) const;

	//Checks wether a successful connection is from a reestablishment
	BaseConnection* IsConnectionReestablishment(const FruityHal::GapConnectedEvent& connectedEvent) const;

	static constexpr u16 TIME_BETWEEN_TIME_SYNC_INTERVALS_DS = SEC_TO_DS(5);
	u16 timeSinceLastTimeSyncIntervalDs = 0;	//Let's not spam the connections with time syncs.

	u32 uniqueConnectionIdCounter = 0; //Counts all created connections to assign "unique" ids

public:
	ConnectionManager();
	void Init();
	static ConnectionManager& getInstance();

	//This method is called when empty buffers are available and there is data to send
	void fillTransmitBuffers() const;

	u8 freeMeshInConnections = 0;
	u8 freeMeshOutConnections = 0;

	BaseConnection* pendingConnection = nullptr;

	u16 droppedMeshPackets = 0;
	u16 sentMeshPacketsUnreliable = 0;
	u16 sentMeshPacketsReliable = 0;

	//ConnectionType Resolving
	void ResolveConnection(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8 const * data);

	void NotifyNewConnection();
	void NotifyDeleteConnection();

	BaseConnections GetBaseConnections(ConnectionDirection direction) const;
	MeshConnections GetMeshConnections(ConnectionDirection direction) const;
	BaseConnections GetConnectionsOfType(ConnectionType connectionType, ConnectionDirection direction) const;

	BaseConnection* allConnections[TOTAL_NUM_CONNECTIONS];

	i8 getFreeConnectionSpot() const;

	bool HasFreeConnection(ConnectionDirection direction) const;

	//Returns the connection that is currently doing a handshake or nullptr
	MeshConnection* GetConnectionInHandshakeState() const;

	ErrorType ConnectAsMaster(NodeId partnerId, FruityHal::BleGapAddr* address, u16 writeCharacteristicHandle, u16 connectionIv);

	void ForceDisconnectOtherMeshConnections(const MeshConnection* ignoreConnection, AppDisconnectReason appDisconnectReason) const;
	void ForceDisconnectOtherHandshakedMeshConnections(const MeshConnection* ignoreConnection, AppDisconnectReason appDisconnectReason) const;
	void ForceDisconnectAllConnections(AppDisconnectReason appDisconnectReason) const;

	int ReestablishConnections() const;

	//Functions used for sending messages
	void SendMeshMessage(u8* data, u16 dataLength, DeliveryPriority priority) const;

	void SendModuleActionMessage(MessageType messageType, ModuleId moduleId, NodeId toNode, u8 actionType, u8 requestHandle, const u8* additionalData, u16 additionalDataSize, bool reliable, bool lookback) const;

	void BroadcastMeshPacket(u8* data, u16 dataLength, DeliveryPriority priority, bool reliable) const;

	void RouteMeshData(BaseConnection* connection, BaseConnectionSendData* sendData, u8 const * data) const;
	void BroadcastMeshData(const BaseConnection* ignoreConnection, BaseConnectionSendData* sendData, u8 const * data, RoutingDecision routingDecision) const;

	//Whether or not the node should receive and dispatch messages that are sent to the given nodeId
	bool IsReceiverOfNodeId(NodeId nodeId) const;

	//Call this to dispatch a message to the node and all modules, this method will perform some basic
	//checks first, e.g. if the receiver matches
	void DispatchMeshMessage(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader const * packet, bool checkReceiver) const;

	//Internal use only, do not use
	//Can send packets as WRITE_REQ (required for some internal functionality) but can lead to problems with the SoftDevice
	void SendMeshMessageInternal(u8* data, u16 dataLength, DeliveryPriority priority, bool reliable, bool loopback, bool toMeshAccess) const;


	BaseConnection* GetConnectionFromHandle(u16 connectionHandle) const;
	BaseConnection* GetConnectionByUniqueId(u32 uniqueConnectionId) const;
	MeshConnection* GetMeshConnectionToPartner(NodeId partnerId) const;
	// By definition, this method will only return a maximum of one connection. Sometimes the Connection Index is interesting however.
	// In such cases this function can be used instead of GetConnectionByUniqueId
	BaseConnections GetConnectionsByUniqueId(u32 uniqueConnectionId) const; 

	MeshConnection* GetMeshConnectionToShortestSink(const BaseConnection* excludeConnection) const;
	ClusterSize GetMeshHopsToShortestSink(const BaseConnection* excludeConnection) const;

	u16 GetPendingPackets() const;

	void SetMeshConnectionInterval(u16 connectionInterval) const;

	void DeleteConnection(BaseConnection* connection, AppDisconnectReason reason);

	//Connection callbacks
	void MessageReceivedCallback(BaseConnectionSendData* sendData, u8* data) const;


	u32 RequestDataLengthExtensionAndMtuExchange(BaseConnection* c);
	void MtuUpdatedHandler(u16 connHandle, u16 mtu);

	void GapConnectionReadyForHandshakeHandler(BaseConnection* c);

	//These methods can be accessed by the Connection classes

	//GAPController Handlers
	void GapConnectingTimeoutHandler(const FruityHal::GapTimeoutEvent & gapTimeoutEvent);
	void GapConnectionConnectedHandler(const FruityHal::GapConnectedEvent & connectedEvent);
	void GapConnectionEncryptedHandler(const FruityHal::GapConnectionSecurityUpdateEvent &connectionSecurityUpdateEvent);
	void GapConnectionDisconnectedHandler(const FruityHal::GapDisconnectedEvent& disconnectedEvent);

	//GATTController Handlers
	void ForwardReceivedDataToConnection(u16 connectionHandle, BaseConnectionSendData &sendData, u8 const * data);
	void GattsWriteEventHandler(const FruityHal::GattsWriteEvent& gattsWriteEvent);
	void GattcHandleValueEventHandler(const FruityHal::GattcHandleValueEvent& handleValueEvent);
	void GattDataTransmittedEventHandler(const FruityHal::GattDataTransmittedEvent& gattDataTransmitted);
	void GattcWriteResponseEventHandler(const FruityHal::GattcWriteResponseEvent& writeResponseEvent);
	void GATTServiceDiscoveredHandler(u16 connHandle, FruityHal::BleGattDBDiscoveryEvent &evt);
	void GattcTimeoutEventHandler(const FruityHal::GattcTimeoutEvent& gattcTimeoutEvent);

	void PacketSuccessfullyQueuedCallback(MeshConnection* connection, SizedData packetData) const;

	//Callbacks are kinda complicated, so we handle BLE events directly in this class
	void GapRssiChangedEventHandler(const FruityHal::GapRssiChangedEvent& rssiChangedEvent) const;
	void TimerEventHandler(u16 passedTimeDs);

	void ResetTimeSync();
	bool IsAnyConnectionCurrentlySyncing();
	void TimeSyncInitialReplyReceivedHandler(const TimeSyncInitialReply& reply);
	void TimeSyncCorrectionReplyReceivedHandler(const TimeSyncCorrectionReply& reply);


	u32 GenerateUniqueConnectionId();
};

