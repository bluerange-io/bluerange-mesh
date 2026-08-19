////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// ** BlueRange Mesh – Community Edition (CE)
// ** Copyright (c) 2015-2021 MWAY DIGITAL GmbH, Germany
// ** Copyright (c) 2021-2026 BlueRange GmbH, Germany
// **
// ** This file is part of BlueRange Mesh Community Edition (formerly known as
// ** FruityMesh).
// **
// ** BlueRange Mesh Community Edition is free software: you can redistribute it
// ** and/or modify it under the terms of the GNU General Public License as
// ** published by the Free Software Foundation, either version 3 of the
// ** License, or (at your option) any later version.
// **
// ** BlueRange Mesh Community Edition is distributed in the hope that it will
// ** be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// ** See the GNU General Public License for more details.
// **
// ** You should have received a copy of the GNU General Public License along
// ** with this program. If not, see https://www.gnu.org/licenses/.
// **
// ** IMPORTANT:
// ** Any modification, extension, or derivative work of this file MUST also be
// ** licensed under the GNU General Public License v3 or later and the complete
// ** corresponding source code MUST be made available.
// **
// ** Commercial Use:
// ** If you wish to use this software without the obligations of the GPLv3
// ** (including source code disclosure), a commercial license for
// ** BlueRange Mesh OEM Edition is required.
// **
// ** License violations automatically terminate your rights under this license
// ** and may result in legal action under applicable law.
// ** For further information please use the contact form at:
// ** https://bluerange.io/en/contact
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
 * Connection Handles are used to protect the implementation against null pointer access as a connection might call
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
