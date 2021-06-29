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

/*
 * This is a test Module for testing stuff
 */

#pragma once

#include <Module.h>

enum class FloodMode : u8{
    OFF = 0,
    RELIABLE = 1,
    UNRELIABLE = 2,
    LISTEN = 3,
    UNRELIABLE_SPLIT = 4
};

/**
 * The DebugModule contains functionality for debugging a mesh at runtime, e.g.
 * sending of flood messages, testing for throughput, ....
 * The functionality might change at any time so it is not a specified api.
 */
class DebugModule: public Module
{
    private:

        static constexpr u8 debugButtonEnableUartDs = 0;

        static constexpr u16 readMemMaxLength = 32;

        static constexpr u32 surplusAccuracy = 10000;

        #pragma pack(push, 1)
        //Module configuration that is saved persistently
        struct DebugModuleConfiguration : ModuleConfiguration{
            //Insert more persistent config values here
        };
        #pragma pack(pop)

        //Counters for flood messages
        FloodMode floodMode;
        bool floodFrameSkip = false; //When entering the flood command in the local terminal, the passed timeDs can be large. This skips these large values
        NodeId floodDestinationId;
        u32 floodMessagesPer10Sec = 0; // the number of ticks per 100 sec should ideally be dividable by this or should be a multiple
        u32 floodMessagesSurplus = 0;
        u32 floodTimeoutSec = 0;
        u32 floodEndTimeDs = 0;
        //Counters for flood messages
        u32 packetsOut;
        u32 packetsIn;

        //Flood counter + time calc
        u32 firstFloodPacketMs = 0;
        u32 autoFloodSum = 0;
        u32 lastFloodPacketMs = 0;
        u32 throughputInBytesPerSecond = 0;

        //Variables for counter mode
        NodeId counterDestinationId = 0;
        u16 counterMessagesPer10Sec = 0;
        u32 counterMessagesSurplus = 0;
        u32 counterMaxCount = 0;
        u32 currentCounter = 0;
        u32 counterCheck = 0;

        //Counters for ping
        u32 pingSentTimeMs;
        u8 pingHandle;
        u16 pingCount;
        u16 pingCountResponses;
        bool syncTest;

#ifdef SIM_ENABLED
        u32 queueFloodCounterLow    = 0;
        u32 queueFloodCounterMedium = 0;
        u32 queueFloodCounterHigh   = 0;
#endif

        #pragma pack(push)
        #pragma pack(1)

        static constexpr int SIZEOF_DEBUG_MODULE_INFO_MESSAGE = 8;
        typedef struct
        {
            u16 connectionLossCounter;
            u16 droppedPackets;
            u16 sentPacketsReliable;
            u16 sentPacketsUnreliable;
        } DebugModuleInfoMessage;
        STATIC_ASSERT_SIZE(DebugModuleInfoMessage, 8);

        static constexpr int SIZEOF_DEBUG_MODULE_PINGPONG_MESSAGE = 1;
        typedef struct
        {
            u8 ttl;

        } DebugModulePingpongMessage;
        STATIC_ASSERT_SIZE(DebugModulePingpongMessage, 1);

        static constexpr int SIZEOF_DEBUG_MODULE_LPING_MESSAGE = 4;
        typedef struct
        {
            NodeId leafNodeId;
            u16 hops;

        } DebugModuleLpingMessage;
        STATIC_ASSERT_SIZE(DebugModuleLpingMessage, 4);


        static constexpr int SIZEOF_DEBUG_MODULE_SET_DISCOVERY_MESSAGE = 1;
        typedef struct
        {
            u8 discoveryMode;

        } DebugModuleSetDiscoveryMessage;
        STATIC_ASSERT_SIZE(DebugModuleSetDiscoveryMessage, 1);


        static constexpr int SIZEOF_DEBUG_MODULE_RESET_MESSAGE = 1;
        typedef struct
        {
            u8 resetSeconds;

        } DebugModuleResetMessage;
        STATIC_ASSERT_SIZE(DebugModuleResetMessage, 1);


        static constexpr int SIZEOF_DEBUG_MODULE_FLOOD_MESSAGE = 4;
        typedef struct
        {
            u16 packetsIn;
            u16 packetsOut;

            u8 chunkData[66]; // This chunk is only there to bloat the message to such a size, that it has to be split.
        } DebugModuleFloodMessage;
        STATIC_ASSERT_SIZE(DebugModuleFloodMessage, 70);


        static constexpr int SIZEOF_DEBUG_MODULE_SET_FLOOD_MODE_MESSAGE = 7;
        typedef struct
        {
            NodeId floodDestinationId;
            u16 packetsPer10Sec;
            u8 floodMode;
            u16 timeoutSec;

        } DebugModuleSetFloodModeMessage;
        STATIC_ASSERT_SIZE(DebugModuleSetFloodModeMessage, 7);

        static constexpr int SIZEOF_DEBUG_MODULE_START_COUNTER_MESSAGE = 8;
        typedef struct
        {
            NodeId counterDestinationId;
            u16 packetsPer10Sec;
            u32 maxCount;

        } DebugModuleStartCounterMessage;
        STATIC_ASSERT_SIZE(DebugModuleStartCounterMessage, 8);

        static constexpr int SIZEOF_DEBUG_MODULE_COUNTER_MESSAGE = 4;
        typedef struct
        {
            u32 counter;
        } DebugModuleCounterMessage;
        STATIC_ASSERT_SIZE(DebugModuleCounterMessage, 4);

        typedef struct
        {
            u8 data[MAX_MESH_PACKET_SIZE - SIZEOF_CONN_PACKET_MODULE];
        } DebugModuleSendMaxMessageResponse;

        //Read Memory
        static constexpr int SIZEOF_DEBUG_MODULE_READ_MEMORY_MESSAGE = 6;
        typedef struct
        {
            u32 address;
            u16 length;
        } DebugModuleReadMemoryMessage;
        STATIC_ASSERT_SIZE(DebugModuleReadMemoryMessage, SIZEOF_DEBUG_MODULE_READ_MEMORY_MESSAGE);

        static constexpr int SIZEOF_DEBUG_MODULE_MEMORY_MESSAGE_HEADER = 4;
        typedef struct
        {
            u32 address;
            u8 data[readMemMaxLength];
        } DebugModuleMemoryMessage;
        STATIC_ASSERT_SIZE(DebugModuleMemoryMessage, readMemMaxLength + SIZEOF_DEBUG_MODULE_MEMORY_MESSAGE_HEADER);

        #pragma pack(pop)

        void CauseHardfault() const;

        void CauseStackOverflow() const;

    public:
        DECLARE_CONFIG_AND_PACKED_STRUCT(DebugModuleConfiguration);

        enum class DebugModuleTriggerActionMessages : u8{
            //RESET_NODE = 0, Removed as of 21.05.2019
            RESET_CONNECTION_LOSS_COUNTER = 1,
            FLOOD_MESSAGE = 2,
            GET_STATS_MESSAGE = 3,
            CAUSE_HARDFAULT_MESSAGE = 4,
            //REQUEST_FORCE_REESTABLISH = 5, deprecated
            PING = 6,
            PINGPONG = 7,
            //SET_DISCOVERY = 8, deprecated
            LPING = 9,
            SET_FLOOD_MODE = 10,
            //FORCE_REESTABLISH = 11, deprecated //Must be sent to a node that is connected to us to force a reestablishment
            //SET_LIVEREPORTING_DEPRECATED = 12, Removed as of 21.05.2019
            EINK_SETANDDRAW_DEPRECATED = 13,
            GET_JOIN_ME_BUFFER = 14,
            RESET_FLOOD_COUNTER = 15,
            SEND_MAX_MESSAGE = 16,
            START_COUNTER = 17,
            COUNTER = 18,
            READ_MEMORY = 19,

        };

        enum class DebugModuleActionResponseMessages : u8{
            STATS_MESSAGE = 3,
            PING_RESPONSE = 6,
            //SET_DISCOVERY_RESPONSE = 8, deprecated
            LPING_RESPONSE = 9,
            JOIN_ME_BUFFER_ITEM = 10,
            EINK_SETANDDRAW_RESPONSE_DEPRECATED = 11,
            SEND_MAX_MESSAGE_RESPONSE = 16,
            MEMORY = 19,
            QUEUE_FLOOD_RESPONSE_LOW = 20,
            QUEUE_FLOOD_RESPONSE_MEDIUM = 21,
            QUEUE_FLOOD_RESPONSE_HIGH = 22,
            QUEUE_FLOOD_RESPONSE_VITAL = 23,
        };

        DebugModule();

        void ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength) override final;

        void ResetToDefaultConfiguration() override final;

        void TimerEventHandler(u16 passedTimeDs) override final;

        void SendStatistics(NodeId receiver) const;

#if IS_ACTIVE(BUTTONS)
        void ButtonHandler(u8 buttonId, u32 holdTimeDs) override final;
#endif

        #ifdef TERMINAL_ENABLED
        TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override final;
        #endif

        void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader) override final;

        u32 GetPacketsIn();
        u32 GetPacketsOut();

#ifdef SIM_ENABLED
        void SendQueueFloodMessage(DeliveryPriority prio);
        void SendQueueFloodMessages();
        u32 GetQueueFloodCounterLow();
        u32 GetQueueFloodCounterMedium();
        u32 GetQueueFloodCounterHigh();
        //Priority
        virtual DeliveryPriority GetPriorityOfMessage(const u8* data, MessageLength size) override;
#endif
        
        u32 GetThroughputTestResult();
        
};
