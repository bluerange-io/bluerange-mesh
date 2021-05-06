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
