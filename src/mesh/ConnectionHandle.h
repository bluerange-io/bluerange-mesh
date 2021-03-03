////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2021 M-Way Solutions GmbH
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
#pragma once

#include "PrimitiveTypes.h"
#include "BaseConnection.h"
#include "MeshConnection.h"
#include "MeshAccessConnection.h"
#include "PacketQueue.h"

class ConnectionManager;

/*
 * Connection Handles are used to protect the implementation agains null pointer access as a connection might call
 * some handlers that delete the connection and another handler might try to access the connection again.
 */
class BaseConnectionHandle
{
    friend class ConnectionManager;
protected:
    u32 uniqueConnectionId;

    //The both cache variables are used to store a previously retrieved
    //connection for quick access. If the amount of deleted connections
    //since the last retrieval process has not changed, then we know that
    //the cached connection must still be valid and can be returned.
    mutable u32 cacheAmountOfRemovedConnections = 0;
    mutable BaseConnection* cacheConnection = nullptr;
    template<typename T>
    T* GetMutableConnection() const;

public:
    BaseConnectionHandle();
    explicit BaseConnectionHandle(u32 uniqueConnectionId);
    explicit BaseConnectionHandle(const BaseConnection& con);

    explicit operator bool() const;
    
    //These should be used very rarely as they don't provide any form of protection!
    BaseConnection* GetConnection();
    const BaseConnection* GetConnection() const;

    bool IsValid() const;
    bool Exists() const;

    bool DisconnectAndRemove(AppDisconnectReason reason);
    bool IsHandshakeDone();
    u16 GetConnectionHandle();
    NodeId GetPartnerId();
    ConnectionState GetConnectionState();
    bool SendData(u8 const * data, MessageLength dataLength, bool reliable, u32 * messageHandle=nullptr);
    bool FillTransmitBuffers();
    FruityHal::BleGapAddr GetPartnerAddress();
    u32 GetCreationTimeDs();
    i8 GetAverageRSSI();
    u16 GetSentUnreliable();
    u32 GetUniqueConnectionId();
    ChunkedPacketQueue* GetQueueByPriority(DeliveryPriority prio);
};

class MeshConnectionHandle : public BaseConnectionHandle
{
    friend class ConnectionManager;
private:

public:
    MeshConnectionHandle();
    explicit MeshConnectionHandle(u32 uniqueConnectionId);
    explicit MeshConnectionHandle(const MeshConnection& con);

    MeshConnectionHandle& operator=(const MeshConnectionHandle &other) = default;

    //These should be used very rarely as they don't provide any form of protection!
    MeshConnection* GetConnection();
    const MeshConnection* GetConnection() const;

    bool TryReestablishing();
    ClusterSize GetHopsToSink();
    bool SetHopsToSink(ClusterSize hops);
    using BaseConnectionHandle::SendData;
    bool SendData(BaseConnectionSendData* sendData, u8 const * data, u32 * messageHandle=nullptr);
    ClusterSize GetConnectedClusterSize();
    bool HandoverMasterBit();
    bool HasConnectionMasterBit();
    bool GetEnrolledNodesSync();
    bool SetEnrolledNodesSync(bool sync);
};

class MeshAccessConnectionHandle : public BaseConnectionHandle
{
    friend class ConnectionManager;
private:

public:
    MeshAccessConnectionHandle();
    explicit MeshAccessConnectionHandle(u32 uniqueConnectionId);
    explicit MeshAccessConnectionHandle(const MeshAccessConnection& con);

    MeshAccessConnectionHandle& operator=(const MeshAccessConnectionHandle &other) = default;

    //These should be used very rarely as they don't provide any form of protection!
    MeshAccessConnection* GetConnection();
    const MeshAccessConnection* GetConnection() const;

    bool ShouldSendDataToNodeId(NodeId nodeId) const;
    bool SendClusterState();
    NodeId GetVirtualPartnerId();
    bool KeepAliveFor(u32 timeDs);
    bool KeepAliveForIfSet(u32 timeDs);
};
