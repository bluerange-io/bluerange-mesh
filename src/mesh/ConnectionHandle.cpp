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

#include "ConnectionHandle.h"
#include "GlobalState.h"
#include "ConnectionManager.h"
#include "Logger.h"

//This is a macro rather than a function so that SIMEXCEPTION still has access to the LINE.
#define DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING() Logger::GetInstance().LogCustomCount(CustomErrorTypes::COUNT_ACCESS_TO_REMOVED_CONNECTION); SIMEXCEPTION(AccessToRemovedConnectionException);

template<typename T>
 T* BaseConnectionHandle::GetMutableConnection() const
{
    if (uniqueConnectionId == 0) return nullptr;

    if ((!cacheConnection && cacheAmountOfRemovedConnections == 0) || cacheAmountOfRemovedConnections != BaseConnection::GetAmountOfRemovedConnections())
    {
        cacheConnection = GS->cm.GetRawConnectionByUniqueId(uniqueConnectionId);
        cacheAmountOfRemovedConnections = BaseConnection::GetAmountOfRemovedConnections();
    }
    return (T*)cacheConnection;
}

BaseConnection * BaseConnectionHandle::GetConnection()
{
    return GetMutableConnection<BaseConnection>();
}

const BaseConnection * BaseConnectionHandle::GetConnection() const
{
    return GetMutableConnection<BaseConnection>();
}

BaseConnectionHandle::BaseConnectionHandle()
    : uniqueConnectionId(0)
{
    // Do nothing
}

BaseConnectionHandle::BaseConnectionHandle(u32 uniqueConnectionId)
    : uniqueConnectionId(uniqueConnectionId)
{
    // Do nothing
}

BaseConnectionHandle::BaseConnectionHandle(const BaseConnection & con)
    : uniqueConnectionId(con.uniqueConnectionId)
{
    // Do nothing
}

BaseConnectionHandle::operator bool() const
{
    return Exists();
}

bool BaseConnectionHandle::IsValid() const
{
    return uniqueConnectionId != 0;
}

bool BaseConnectionHandle::Exists() const
{
    return GetConnection() != nullptr;
}

bool BaseConnectionHandle::DisconnectAndRemove(AppDisconnectReason reason)
{
    BaseConnection* con = GetConnection();
    if (con)
    {
        con->DisconnectAndRemove(reason);
        return true;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return false;
    }
}

bool BaseConnectionHandle::IsHandshakeDone()
{
    BaseConnection* con = GetConnection();
    if (con)
    {
        return con->HandshakeDone();
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return false;
    }
}

u16 BaseConnectionHandle::GetConnectionHandle()
{
    BaseConnection* con = GetConnection();
    if (con)
    {
        return con->connectionHandle;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return FruityHal::FH_BLE_INVALID_HANDLE;
    }
}

NodeId BaseConnectionHandle::GetPartnerId()
{
    BaseConnection* con = GetConnection();
    if (con)
    {
        return con->partnerId;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return NODE_ID_INVALID;
    }
}

ConnectionState BaseConnectionHandle::GetConnectionState()
{
    BaseConnection* con = GetConnection();
    if (con)
    {
        return con->connectionState;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return ConnectionState::DISCONNECTED;
    }
}

bool BaseConnectionHandle::SendData(u8 const * data, MessageLength dataLength, bool reliable, u32 * messageHandle)
{
    BaseConnection* con = GetConnection();
    if (con)
    {
        return con->SendData(data, dataLength, reliable, messageHandle);
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return false;
    }
}

bool BaseConnectionHandle::FillTransmitBuffers()
{
    BaseConnection* con = GetConnection();
    if (con)
    {
        con->FillTransmitBuffers();
        return true;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return false;
    }
}

FruityHal::BleGapAddr BaseConnectionHandle::GetPartnerAddress()
{
    const BaseConnection* con = GetConnection();
    if (con)
    {
        return con->partnerAddress;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        FruityHal::BleGapAddr retVal;
        CheckedMemset(&retVal, 0, sizeof(retVal));
        return retVal;
    }
}

u32 BaseConnectionHandle::GetCreationTimeDs()
{
    const BaseConnection* con = GetConnection();
    if (con)
    {
        return con->creationTimeDs;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return 0;
    }
}

i8 BaseConnectionHandle::GetAverageRSSI()
{
    const BaseConnection* con = GetConnection();
    if (con)
    {
        return con->GetAverageRSSI();
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return 0;
    }
}

u16 BaseConnectionHandle::GetSentUnreliable()
{
    const BaseConnection* con = GetConnection();
    if (con)
    {
        return con->sentUnreliable;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return 0;
    }
}

u32 BaseConnectionHandle::GetUniqueConnectionId()
{
    return uniqueConnectionId;
}

ChunkedPacketQueue* BaseConnectionHandle::GetQueueByPriority(DeliveryPriority prio)
{
    BaseConnection* con = GetConnection();
    if (con)
    {
        return con->queue.GetQueueByPriority(prio);
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return nullptr;
    }
    return nullptr;
}

MeshConnectionHandle::MeshConnectionHandle()
    : BaseConnectionHandle()
{
}

MeshConnectionHandle::MeshConnectionHandle(u32 uniqueConnectionHandle)
    : BaseConnectionHandle(uniqueConnectionHandle)
{
}

MeshConnectionHandle::MeshConnectionHandle(const MeshConnection & con)
    : BaseConnectionHandle(con.uniqueConnectionId)
{
}

MeshConnection * MeshConnectionHandle::GetConnection()
{
    return GetMutableConnection<MeshConnection>();
}

const MeshConnection * MeshConnectionHandle::GetConnection() const
{
    return GetMutableConnection<MeshConnection>();
}

bool MeshConnectionHandle::TryReestablishing()
{
    MeshConnection* con = GetConnection();
    if (con)
    {
        con->TryReestablishing();
        return true;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return false;
    }
}

ClusterSize MeshConnectionHandle::GetHopsToSink()
{
    MeshConnection* con = GetConnection();
    if (con)
    {
        return con->GetHopsToSink();
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return -1;
    }
}

bool MeshConnectionHandle::SetHopsToSink(ClusterSize hops)
{
    MeshConnection* con = GetConnection();
    if (con)
    {
        con->SetHopsToSink(hops);
        return true;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return false;
    }
}

bool MeshConnectionHandle::SendData(BaseConnectionSendData * sendData, u8 const * data, u32 * messageHandle)
{
    MeshConnection* con = GetConnection();
    if (con)
    {
        return con->SendData(sendData, data, messageHandle);
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return false;
    }
}

ClusterSize MeshConnectionHandle::GetConnectedClusterSize()
{
    MeshConnection* con = GetConnection();
    if (con)
    {
        return con->connectedClusterSize;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return 0;
    }
}

bool MeshConnectionHandle::HandoverMasterBit()
{
    MeshConnection* con = GetConnection();
    if (con)
    {
        con->HandoverMasterBit();
        return true;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return false;
    }
}

bool MeshConnectionHandle::HasConnectionMasterBit()
{
    MeshConnection* con = GetConnection();
    if (con)
    {
        return con->HasConnectionMasterBit();
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return false;
    }
}


bool MeshConnectionHandle::GetEnrolledNodesSync()
{
    MeshConnection* con = GetConnection();
    if (con)
    {
        return con->GetEnrolledNodesSync();
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return false;
    }
}

bool MeshConnectionHandle::SetEnrolledNodesSync(bool sync)
{
    MeshConnection* con = GetConnection();
    if (con)
    {
        con->SetEnrolledNodesSync(sync);
        return true;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return false;
    }
}

MeshAccessConnectionHandle::MeshAccessConnectionHandle()
    : BaseConnectionHandle()
{
}

MeshAccessConnectionHandle::MeshAccessConnectionHandle(u32 uniqueConnectionHandle)
    : BaseConnectionHandle(uniqueConnectionHandle)
{
}

MeshAccessConnectionHandle::MeshAccessConnectionHandle(const MeshAccessConnection & con)
    : BaseConnectionHandle(con.uniqueConnectionId)
{
}

MeshAccessConnection * MeshAccessConnectionHandle::GetConnection()
{
    return GetMutableConnection<MeshAccessConnection>();
}

const MeshAccessConnection * MeshAccessConnectionHandle::GetConnection() const
{
    return GetMutableConnection<MeshAccessConnection>();
}

bool MeshAccessConnectionHandle::ShouldSendDataToNodeId(NodeId nodeId) const
{
    const MeshAccessConnection* con = GetConnection();
    if (con)
    {
        return con->ShouldSendDataToNodeId(nodeId);
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return false;
    }
}

bool MeshAccessConnectionHandle::SendClusterState()
{
    MeshAccessConnection* con = GetConnection();
    if (con)
    {
        con->SendClusterState();
        return true;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return false;
    }
}

NodeId MeshAccessConnectionHandle::GetVirtualPartnerId()
{
    MeshAccessConnection* con = GetConnection();
    if (con)
    {
        return con->virtualPartnerId;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return NODE_ID_INVALID;
    }
}

bool MeshAccessConnectionHandle::KeepAliveFor(u32 timeDs)
{
    MeshAccessConnection* con = GetConnection();
    if (con)
    {
        con->KeepAliveFor(timeDs);
        return true;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return false;
    }
}

bool MeshAccessConnectionHandle::KeepAliveForIfSet(u32 timeDs)
{
    MeshAccessConnection* con = GetConnection();
    if (con)
    {
        con->KeepAliveForIfSet(timeDs);
        return true;
    }
    else
    {
        DEFAULT_CONNECTION_HANDLE_ERROR_HANDLING();
        return false;
    }
}
