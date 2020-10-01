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
#include <ConnectionHandle.h>

struct BaseConnections
{
    u8 count = 0;
    BaseConnectionHandle handles[TOTAL_NUM_CONNECTIONS];
};
struct MeshConnections
{
    u8 count = 0;
    MeshConnectionHandle handles[TOTAL_NUM_CONNECTIONS];
};
struct MeshAccessConnections
{
    u8 count = 0;
    MeshAccessConnectionHandle handles[TOTAL_NUM_CONNECTIONS];
};


typedef BaseConnection* (*ConnTypeResolver)(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8 const * data);

class MeshAccessConnection;
class BaseConnectionHandle;
class CherrySim;

/*
 * The ConnectionManager is the central place that manages the creation and deletion of all connections and also
 * provides management functionality for tasks that cannot be done without having access to multiple connections.
 */
class ConnectionManager
{
    friend class MeshAccessConnection;
    friend class BaseConnectionHandle;
    friend class CherrySim;
private:
    //Used within the send methods to put data
    void QueuePacket(BaseConnection* connection, u8* data, u16 dataLength, bool reliable) const;

    //Checks wether a successful connection is from a reestablishment
    BaseConnection* IsConnectionReestablishment(const FruityHal::GapConnectedEvent& connectedEvent) const;

    static constexpr u16 TIME_BETWEEN_TIME_SYNC_INTERVALS_DS = SEC_TO_DS(5);
    u16 timeSinceLastTimeSyncIntervalDs = 0;    //Let's not spam the connections with time syncs.

    u32 uniqueConnectionIdCounter = 0; //Counts all created connections to assign "unique" ids

    BaseConnection* GetRawConnectionByUniqueId(u32 uniqueConnectionId) const;
    BaseConnection* GetRawConnectionFromHandle(u16 connectionHandle) const;

TESTER_PUBLIC:
    BaseConnection* allConnections[TOTAL_NUM_CONNECTIONS];



public:
    ConnectionManager();
    void Init();
    static ConnectionManager& GetInstance();

    //This method is called when empty buffers are available and there is data to send
    void FillTransmitBuffers() const;

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

    BaseConnections       GetBaseConnections(ConnectionDirection direction) const;
    MeshConnections       GetMeshConnections(ConnectionDirection direction) const;
    MeshAccessConnections GetMeshAccessConnections(ConnectionDirection direction) const;
    BaseConnections GetConnectionsOfType(ConnectionType connectionType, ConnectionDirection direction) const;

    i8 GetFreeConnectionSpot() const;

    bool HasFreeConnection(ConnectionDirection direction) const;

    //Returns the connection that is currently doing a handshake or nullptr
    MeshConnectionHandle GetConnectionInHandshakeState() const;

    ErrorType ConnectAsMaster(NodeId partnerId, FruityHal::BleGapAddr* address, u16 writeCharacteristicHandle, u16 connectionIv);

    void ForceDisconnectOtherMeshConnections(const MeshConnection* ignoreConnection, AppDisconnectReason appDisconnectReason) const;
    void ForceDisconnectOtherHandshakedMeshConnections(const MeshConnection* ignoreConnection, AppDisconnectReason appDisconnectReason) const;
    void ForceDisconnectAllConnections(AppDisconnectReason appDisconnectReason) const;

    int ReestablishConnections() const;

    //Functions used for sending messages
    void SendMeshMessage(u8* data, u16 dataLength, DeliveryPriority priority) const;

    //Send a message with a ConnPacketModule header by using a ModuleId
    ErrorTypeUnchecked SendModuleActionMessage(MessageType messageType, ModuleId moduleId, NodeId toNode, u8 actionType, u8 requestHandle, const u8* additionalData, u16 additionalDataSize, bool reliable, bool lookback) const;
    
    //Send a message with a ConnPacketModuleVendor header by using a VendorModuleId
    //This method will check and moduleId parameter and will send a ConnPacketModule instead if the given id is not a VendorModuleId
    ErrorTypeUnchecked SendModuleActionMessage(MessageType messageType, VendorModuleId moduleId, NodeId toNode, u8 actionType, u8 requestHandle, const u8* additionalData, u16 additionalDataSize, bool reliable, bool lookback) const;

    void BroadcastMeshPacket(u8* data, u16 dataLength, DeliveryPriority priority, bool reliable) const;

    void RouteMeshData(BaseConnection* connection, BaseConnectionSendData* sendData, u8 const * data) const;
    void BroadcastMeshData(const BaseConnection* ignoreConnection, BaseConnectionSendData* sendData, u8 const * data, RoutingDecision routingDecision) const;

    //Whether or not the node should receive and dispatch messages that are sent to the given nodeId
    bool IsReceiverOfNodeId(NodeId nodeId) const;

    //Can be used to do basic checks on packet to see if it is a valid FruityMesh packet
    bool IsValidFruityMeshPacket(const u8* data, u16 dataLength) const;

    static u32 MessageTypeToMinimumPacketSize(MessageType messageType);

    //Call this to dispatch a message to the node and all modules, this method will perform some basic
    //checks first, e.g. if the receiver matches
    void DispatchMeshMessage(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packet, bool checkReceiver) const;

    //Internal use only, do not use
    //Can send packets as WRITE_REQ (required for some internal functionality) but can lead to problems with the SoftDevice
    ErrorType SendMeshMessageInternal(u8* data, u16 dataLength, DeliveryPriority priority, bool reliable, bool loopback, bool toMeshAccess) const;


    BaseConnectionHandle GetConnectionFromHandle(u16 connectionHandle) const;
    BaseConnectionHandle GetConnectionByUniqueId(u32 uniqueConnectionId) const;
    MeshAccessConnectionHandle GetMeshAccessConnectionByUniqueId(u32 uniqueConnectionId) const;
    MeshConnectionHandle GetMeshConnectionToPartner(NodeId partnerId) const;

    MeshConnectionHandle GetMeshConnectionToShortestSink(const BaseConnection* excludeConnection) const;
    ClusterSize GetMeshHopsToShortestSink(const BaseConnection* excludeConnection) const;

    u16 GetPendingPackets() const;

    void SetMeshConnectionInterval(u16 connectionInterval) const;

    void DeleteConnection(BaseConnection* connection, AppDisconnectReason reason);

    //Connection callbacks
    void MessageReceivedCallback(BaseConnectionSendData* sendData, u8* data) const;


    ErrorType RequestDataLengthExtensionAndMtuExchange(BaseConnection* c);
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

