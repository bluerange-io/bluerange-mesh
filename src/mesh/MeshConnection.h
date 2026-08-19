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

#include <BaseConnection.h>
#include <TimeManager.h>

/*
 * The MeshConnection class is used to represent FruityMesh connections between
 * nodes. It holds all the necessary properties for clustering.
 * Also, it is responsible for doing the clustering handshake together with
 * the ConnectionManager and the Node.
 */
class MeshConnection
    : public BaseConnection
{
#ifdef SIM_ENABLED
    friend class CherrySim;
    friend class FruitySimServer;
    friend class MultiStackFixture_TestSinkDetectionWithSingleSink_Test;
#endif
    friend class ConnectionManager;
    friend class Node;
    friend class MeshConnectionHandle;

    private:

        enum class TimeSyncState : u8 {
            UNSYNCED             = 0,
            INITIAL_SENT         = 1,
            CORRECTION_SENT      = 2,
        };

        u16 partnerWriteCharacteristicHandle;

        //Mesh variables
        u8 connectionMasterBit;
        ClusterId connectedClusterId;
        ClusterSize connectedClusterSize;

        ClusterId clusterIDBackup;
        ClusterSize clusterSizeBackup;
        ClusterSize hopsToSink;

        //Timestamp and Clustering messages must be sent immediately and are not queued
        //Multiple updates can accumulate in this variable
        //This packet must not be sent during handshakes
        ConnPacketClusterInfoUpdate currentClusterInfoUpdatePacket;

        //Handshake
        ConnPacketClusterAck1 clusterAck1Packet;
        ConnPacketClusterAck2 clusterAck2Packet;

        //Timing
        TimeSyncState timeSyncState = TimeSyncState::UNSYNCED;
#ifdef SIM_ENABLED
        bool correctionTicksSuccessfullyWritten = false;
#endif
        u32 correctionTicks = 0;
        TimePoint syncSendingOrdered;

        //Enrolled nodes synchronization
        bool enrolledNodesSynced = false;

        //Reestablishing
        bool mustRetryReestablishing = false;
        u32 reestablishmentStartedDs = 0;

#if IS_ACTIVE(CONN_PARAM_UPDATE)
        /// If the long term connection interval was already requested, so that
        /// the request is not repeated.
        bool longTermConnectionIntervalRequested = false;
#endif

#ifdef SIM_ENABLED
        //Cluster validity checking in the Simulator
        i16 validityClusterUpdatesToSend;
        i16 validityClusterUpdatesReceived;
#endif

    public:
        //Init + Destroy
        MeshConnection(u8 id, ConnectionDirection direction, FruityHal::BleGapAddr const * partnerAddress, u16 partnerWriteCharacteristicHandle);

        virtual ~MeshConnection();
        static BaseConnection* ConnTypeResolver(BaseConnection* oldConnection, BaseConnectionSendData* sendData, u8 const * data);

        void SaveClusteringSnapshot();
        void DisconnectAndRemove(AppDisconnectReason reason) override final;

        //Mesh Handshake
        void StartHandshake();
        void StartHandshakeAfterMtuExchange();
        void ConnectionMtuUpgradedHandler(u16 gattPayloadSize) override final;
        void ReceiveHandshakePacketHandler(BaseConnectionSendData* sendData, u8 const * data);
        void SendReconnectionHandshakePacket();
        ErrorType SendReconnectionHandshakePacketAfterMtuExchange(); //Pay attention as this might disconnect the connection
        void ReceiveReconnectionHandshakePacket(ConnPacketReconnect const * packet);

        bool SendHandshakeMessage(u8* data, u16 dataLength, bool reliable);

        void TryReestablishing();

        void HandoverMasterBit();
        bool HasConnectionMasterBit();

        //Sending Data
        bool QueueVitalPrioData() override final;
        void ClearCurrentClusterInfoUpdatePacket();
        void PacketSuccessfullyQueuedWithSoftdevice(SizedData* sentData) override final;
        void DataSentHandler(const u8* data, MessageLength length, u32 messageHandle) override final;

        bool SendData(BaseConnectionSendData* sendData, u8 const * data, u32 * messageHandle=nullptr);
        bool SendData(u8 const * data, MessageLength dataLength, bool reliable, u32 * messageHandle=nullptr) override final;

        //Receiving Data
        void ReceiveDataHandler(BaseConnectionSendData* sendData, u8 const * data) override final;
        //Called for received mesh messages after data has been processed
        void ReceiveMeshMessageHandler(BaseConnectionSendData* sendData, u8 const * data);

        //Handler
        bool GapDisconnectionHandler(FruityHal::BleHciError hciDisconnectReason) override final;
        void GapReconnectionSuccessfulHandler(const FruityHal::GapConnectedEvent& connectedEvent) override final;
#if IS_ACTIVE(CONN_PARAM_UPDATE)
        void GapConnParamUpdateHandler(const FruityHal::BleGapConnParams & params) override final;
        void GapConnParamUpdateRequestHandler(const FruityHal::BleGapConnParams & params) override final;
#endif

        //Helpers
        void PrintStatus() override final;
        u32 GetPendingPackets() const override final;
        bool IsValidMessageType(MessageType type);
        bool ClusterInfoUpdateHasData() const;

        //Setter
        void SetHopsToSink(ClusterSize hops);
        ClusterSize GetHopsToSink();
        void SetEnrolledNodesSync(bool sync);
        bool GetEnrolledNodesSync();
};
