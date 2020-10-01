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


#include <DebugModule.h>


#include <Utility.h>
#include <Node.h>
#include <IoModule.h>
#include <StatusReporterModule.h>
#include <AdvertisingController.h>
#include <ScanController.h>
#include <FlashStorage.h>
#include "GlobalState.h"
constexpr u8 DEBUG_MODULE_CONFIG_VERSION = 2;

#if IS_ACTIVE(EINK_MODULE)
#ifndef GITHUB_RELEASE
#include "EinkModule.h"
#endif //GITHUB_RELEASE
#endif

#if IS_ACTIVE(ASSET_MODULE)
#ifndef GITHUB_RELEASE
#include <AssetModule.h>
#endif //GITHUB_RELEASE
#endif

#include <climits>
#include <cstdlib>


DebugModule::DebugModule()
    : Module(ModuleId::DEBUG_MODULE, "debug")
{
    //Save configuration to base class variables
    //sizeof configuration must be a multiple of 4 bytes
    configurationPointer = &configuration;
    configurationLength = sizeof(DebugModuleConfiguration);

    floodMode = FloodMode::OFF;
    packetsOut = 0;
    packetsIn = 0;

    pingSentTimeMs = 0;
    pingHandle = 0;
    pingCount = 0;
    pingCountResponses = 0;
    syncTest = false;

    //Set defaults
    ResetToDefaultConfiguration();
}

void DebugModule::ResetToDefaultConfiguration()
{
    //Set default configuration values
    configuration.moduleId = moduleId;
    configuration.moduleActive = true;
    configuration.moduleVersion = DEBUG_MODULE_CONFIG_VERSION;

    SET_FEATURESET_CONFIGURATION(&configuration, this);
}

void DebugModule::ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength)
{
    //Do additional initialization upon loading the config

}

void DebugModule::SendStatistics(NodeId receiver) const
{
    DebugModuleInfoMessage infoMessage;
    CheckedMemset(&infoMessage, 0x00, sizeof(infoMessage));

    infoMessage.sentPacketsUnreliable = GS->cm.sentMeshPacketsUnreliable;
    infoMessage.sentPacketsReliable = GS->cm.sentMeshPacketsReliable;
    infoMessage.droppedPackets = GS->cm.droppedMeshPackets;
    infoMessage.connectionLossCounter = GS->node.connectionLossCounter;

    SendModuleActionMessage(
        MessageType::MODULE_ACTION_RESPONSE,
        receiver,
        (u8)DebugModuleActionResponseMessages::STATS_MESSAGE,
        0,
        (u8*)&infoMessage,
        SIZEOF_DEBUG_MODULE_INFO_MESSAGE,
        false
    );
}

void DebugModule::TimerEventHandler(u16 passedTimeDs){
#if IS_ACTIVE(TIME_SYNC_TEST_CODE) && !defined(SIM_ENABLED)

    /*When time is synced, this will switch on green led after every 10 sec for 2 sec from the start of the minute*/
    u32 seconds = GS->timeManager.GetTime();
    char timestring[80];
    if (seconds % 60 == 0 && !syncTest && GS->timeManager.IsTimeSynced()) {
        //Enable LED
        IoModule* ioMod = (IoModule*)GS->node.GetModuleById(ModuleId::IO_MODULE);
        if(ioMod != nullptr){
            ioMod->currentLedMode = LedMode::OFF;
        }

        syncTest = true;
    }

    if (syncTest)
    {
        if (seconds % 10 == 0) {
            GS->timeManager.convertTimestampToString(timestring);

            trace("Time is currently %s" EOL, timestring);

            GS->ledGreen.On();
            FruityHal::DelayMs(2000);
        }
    }
#endif


#if IS_INACTIVE(GW_SAVE_SPACE)
    //Counter message generation
    if(currentCounter <= counterMaxCount){
        if (counterMessagesPer10Sec != 0)
        {
            //Distribute the packets evenly over 10 seconds
            counterMessagesSurplus += passedTimeDs * surplusAccuracy;
            u32 numPacketsToSend    = (counterMessagesSurplus * counterMessagesPer10Sec) / SEC_TO_DS(10) / surplusAccuracy;
            counterMessagesSurplus -= numPacketsToSend * SEC_TO_DS(10) * surplusAccuracy / counterMessagesPer10Sec;

            if (numPacketsToSend > 0) {
                logt("DEBUGMOD", "Queuing %u counter packets at time %u", numPacketsToSend, GS->appTimerDs);
            }

            for (u32 i = 0; i < numPacketsToSend; i++)
            {
                DebugModuleCounterMessage data;
                data.counter = currentCounter;

                ErrorTypeUnchecked err = SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    counterDestinationId,
                    (u8)DebugModuleTriggerActionMessages::COUNTER,
                    0,
                    (u8*)&data,
                    SIZEOF_DEBUG_MODULE_COUNTER_MESSAGE,
                    false);
                if (err == ErrorTypeUnchecked::SUCCESS) currentCounter++;
            }
        }
    }

    if(floodMode != FloodMode::OFF){
        if (floodFrameSkip)
        {
            floodFrameSkip = false;
            //Calculating the timeout here instead of the MeshMessageReceived handler
            //to avoid lost packages when manually entering the command into the terminal
            floodEndTimeDs = GS->appTimerDs + SEC_TO_DS(floodTimeoutSec);
        }
        else
        {
            if (GS->appTimerDs % 10 == 0) {
                MeshConnections conns = GS->cm.GetMeshConnections(ConnectionDirection::INVALID);

                logt("DEBUGMOD", "Sent %u: %u, %u, %u, %u",
                    GS->appTimerDs,
                    conns.count >= 1 ? conns.handles[0].GetSentUnreliable() : 0,
                    conns.count >= 2 ? conns.handles[1].GetSentUnreliable() : 0,
                    conns.count >= 3 ? conns.handles[2].GetSentUnreliable() : 0,
                    conns.count >= 4 ? conns.handles[3].GetSentUnreliable() : 0
                );
            }

            if (floodMessagesPer10Sec != 0)
            {
                //Distribute the packets evenly over 10 seconds
                floodMessagesSurplus +=  passedTimeDs * surplusAccuracy;
                u32 numPacketsToSend  = (floodMessagesSurplus * floodMessagesPer10Sec) / SEC_TO_DS(10) / surplusAccuracy;
                floodMessagesSurplus -=  numPacketsToSend * SEC_TO_DS(10) * surplusAccuracy / floodMessagesPer10Sec;

                if (numPacketsToSend > 0) {
                    logt("DEBUGMOD", "Queuing %u flood packets at time %u", numPacketsToSend, GS->appTimerDs);
                }

                for (u32 i = 0; i < numPacketsToSend; i++)
                {
                    packetsOut++;

                    DebugModuleFloodMessage data;
                    data.packetsIn = packetsIn;
                    data.packetsOut = packetsOut;
                    CheckedMemset(data.chunkData, 0, 21);

                    SendModuleActionMessage(
                        MessageType::MODULE_TRIGGER_ACTION,
                        floodDestinationId,
                        (u8)DebugModuleTriggerActionMessages::FLOOD_MESSAGE,
                        0,
                        (u8*)&data,
                        floodMode == FloodMode::UNRELIABLE_SPLIT ? 25 : 12, //Send a big message that must be split
                        floodMode == FloodMode::RELIABLE ? true : false);

                }
            }
        }
    }

    if (GS->appTimerDs >= floodEndTimeDs && floodMode != FloodMode::OFF) {
        logt("DEBUGMOD", "flood mode timeout");
        floodMode = FloodMode::OFF;
    }
#endif
}

u8 modeCounter = 0;

#if IS_ACTIVE(BUTTONS)
void DebugModule::ButtonHandler(u8 buttonId, u32 holdTimeDs)
{
//    //Advertise each 100ms
//    if(modeCounter == 0){
//        GS->terminal->UartDisable();
//        IoModule* iomod = (IoModule*)GS->node.GetModuleById(moduleID::IO_MODULE_ID);
//        iomod->currentLedMode = LedMode::OFF;
//
//        //FruityHal::BleGapAdvStop();
//        FruityHal::BleGapScanStop();
//
////        BleGapAdvParams advparams;
////        CheckedMemset(&advparams, 0x00, sizeof(BleGapAdvParams));
////        advparams.interval = MSEC_TO_UNITS(100, CONFIG_UNIT_0_625_MS);
////        advparams.type = BleGapAdvType::ADV_IND;
////        FruityHal::BleGapAdvStart(&advparams);
//
//
//        GS->ledGreen->On();
//
//        FruityHal::DelayMs(1000);
//
//        GS->ledRed.Off();
//        GS->ledGreen.Off();
//        GS->ledBlue.Off();
//
//        modeCounter++;
//    }
//
//    else if(modeCounter == 1){
//
//            GS->ledRed->On();
//
//            FruityHal::DelayMs(1000);
//
//            GS->ledRed->Off();
//            GS->ledGreen->Off();
//            GS->ledBlue->Off();
//
//#if IS_ACTIVE(ASSET_MODULE)
//            AssetModule* asMod = (AssetModule*)GS->node.GetModuleById(moduleID::ASSET_MODULE_ID);
//            asMod->configuration.enableBarometer = false;
//#endif
//
//            modeCounter++;
//        }


    if(SHOULD_BUTTON_EVT_EXEC(debugButtonEnableUartDs)){
        //Enable UART
#if IS_ACTIVE(UART)
        GS->terminal.UartEnable(false);
#endif

        //Enable LED
        IoModule* ioMod = (IoModule*)GS->node.GetModuleById(ModuleId::IO_MODULE);
        if(ioMod != nullptr){
            ioMod->currentLedMode = LedMode::CONNECTIONS;
        }
    }
}
#endif

#ifdef TERMINAL_ENABLED
TerminalCommandHandlerReturnType DebugModule::TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize)
{
    //React on commands, return true if handled, false otherwise
    if(commandArgsSize >= 3 && (TERMARGS(2 ,moduleName) || TERMARGS(2 ,"eink")))
    {
        NodeId destinationNode = Utility::TerminalArgumentToNodeId(commandArgs[1]);


        if(commandArgsSize >= 4 && TERMARGS(0 ,"action"))
        {
#if IS_INACTIVE(CLC_GW_SAVE_SPACE)
            if(commandArgsSize >= 4 && TERMARGS(3 ,"get_buffer")){
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)DebugModuleTriggerActionMessages::GET_JOIN_ME_BUFFER,
                    0,
                    nullptr,
                    0,
                    false
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }
#endif
#if IS_INACTIVE(SAVE_SPACE)
            //Reset the connection loss counter of any node
            else if(TERMARGS(3, "reset_connection_loss_counter"))
            {
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)DebugModuleTriggerActionMessages::RESET_CONNECTION_LOSS_COUNTER,
                    0,
                    nullptr,
                    0,
                    false
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            else if (TERMARGS(3, "send_max_message"))
            {
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)DebugModuleTriggerActionMessages::SEND_MAX_MESSAGE,
                    0,
                    nullptr,
                    0,
                    false
                );
                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            //Query for statistics
            else if(TERMARGS(3, "get_stats"))
            {
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)DebugModuleTriggerActionMessages::GET_STATS_MESSAGE,
                    0,
                    nullptr,
                    0,
                    false
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            //Tell any node to generate a hardfault
            else if(TERMARGS(3, "hardfault"))
            {
                logt("DEBUGMOD", "send hardfault");
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)DebugModuleTriggerActionMessages::CAUSE_HARDFAULT_MESSAGE,
                    0,
                    nullptr,
                    0,
                    false
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            //Queries a nodes to read back parts of its memory and send it back over the mesh
            //This is helpful if a remote node has some issues and cannot be accessed
            else if (TERMARGS(3, "readmem"))
            {
                DebugModuleReadMemoryMessage data;
                CheckedMemset(&data, 0x00, sizeof(data));

                data.address = Utility::StringToU32(commandArgs[4]);
                data.length = Utility::StringToU16(commandArgs[5]);
                
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)DebugModuleTriggerActionMessages::READ_MEMORY,
                    0,
                    (u8*)&data,
                    SIZEOF_DEBUG_MODULE_READ_MEMORY_MESSAGE,
                    false
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            //Flood the network with messages and count them
            else if (TERMARGS(3, "flood") && commandArgsSize > 6)
            {
                DebugModuleSetFloodModeMessage data;

                data.floodDestinationId                 = Utility::TerminalArgumentToNodeId(commandArgs[4]);
                data.floodMode                          = Utility::StringToU8 (commandArgs[5]);
                data.packetsPer10Sec                    = Utility::StringToU16(commandArgs[6]);
                if(commandArgsSize > 7) data.timeoutSec = Utility::StringToU16(commandArgs[7]);
                else data.timeoutSec = 10;

                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)DebugModuleTriggerActionMessages::SET_FLOOD_MODE,
                    0,
                    (u8*)&data,
                    SIZEOF_DEBUG_MODULE_SET_FLOOD_MODE_MESSAGE,
                    false
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            else if (TERMARGS(3, "ping") && commandArgsSize >= 6)
            {
                //action 45 debug ping 10 u 7
                //Send 10 pings to node 45, unreliable with handle 7

                //Save Ping sent time
                pingSentTimeMs = FruityHal::GetRtcMs();
                pingCount = Utility::StringToU16(commandArgs[4]);
                pingCountResponses = 0;
                u8 pingModeReliable = TERMARGS(5, "r");

                for(int i=0; i<pingCount; i++){
                    SendModuleActionMessage(
                        MessageType::MODULE_TRIGGER_ACTION,
                        destinationNode,
                        (u8)DebugModuleTriggerActionMessages::PING,
                        0,
                        nullptr,
                        0,
                        pingModeReliable
                    );
                }
                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            else if (TERMARGS(3, "counter") && commandArgsSize >= 6)
            {
                DebugModuleStartCounterMessage data;

                data.counterDestinationId = Utility::TerminalArgumentToNodeId(commandArgs[4]);
                data.packetsPer10Sec = Utility::StringToU16(commandArgs[5]);
                data.maxCount = Utility::StringToU32(commandArgs[6]);

                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)DebugModuleTriggerActionMessages::START_COUNTER,
                    0,
                    (u8*)&data,
                    SIZEOF_DEBUG_MODULE_START_COUNTER_MESSAGE,
                    false
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            else if (TERMARGS(3, "pingpong") && commandArgsSize >= 6)
            {
                //action 45 debug pingpong 10 u
                //Send 10 pings to node 45, which will pong it back, then it pings again

                //Save Ping sent time
                pingSentTimeMs = FruityHal::GetRtcMs();
                pingCount = Utility::StringToU16(commandArgs[4]);
                u8 pingModeReliable = TERMARGS(5 , "r");

                DebugModulePingpongMessage data;
                data.ttl = pingCount * 2 - 1;

                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)DebugModuleTriggerActionMessages::PINGPONG,
                    0,
                    (u8*)&data,
                    SIZEOF_DEBUG_MODULE_PINGPONG_MESSAGE,
                    pingModeReliable,
                    false    //A loopback would have the potential to cause a stack overflow (even for small ping counts of 12), so we don't loopback here.
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            else if (TERMARGS(3, "getRtcTime") && commandArgsSize >= 4)
            {
                u32 time = FruityHal::GetRtcMs();
                logt("DEBUGMOD", "Time is %d", time);

                return TerminalCommandHandlerReturnType::SUCCESS;
            }
#endif
        }

    }

#if IS_INACTIVE(SAVE_SPACE)
    else if (TERMARGS(0, "data"))
    {
        NodeId receiverId = 0;
        if (commandArgsSize > 1) {
            if (TERMARGS(1, "sink")) receiverId = NODE_ID_SHORTEST_SINK;
            else if (TERMARGS(1, "hop")) receiverId = NODE_ID_HOPS_BASE + 1;
            else receiverId = NODE_ID_BROADCAST;
        }

        ConnPacketData1 data;
        CheckedMemset(&data, 0x00, sizeof(ConnPacketData1));

        data.header.messageType = MessageType::DATA_1;
        data.header.sender = GS->node.configuration.nodeId;
        data.header.receiver = receiverId;

        data.payload.length = 7;
        data.payload.data[0] = 1;
        data.payload.data[1] = 3;
        data.payload.data[2] = 3;

        GS->cm.SendMeshMessage((u8*)&data, SIZEOF_CONN_PACKET_DATA_1, DeliveryPriority::LOW);

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    //Flood the network with messages and count them
    else if (TERMARGS(0, "floodstat"))
    {
        logt("DEBUGMOD", "Flooding has %u packetsIn and %u packetsOut", packetsIn, packetsOut);

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    //Display the free heap
    else if (TERMARGS(0, "heap"))
    {
        u8 checkvar = 1;
        logjson("NODE", "{\"stack\":%u}" SEP, (u32)(&checkvar - 0x20000000));
        logt("NODE", "Module usage: %u" SEP, GS->moduleAllocator.GetMemorySize());

        return TerminalCommandHandlerReturnType::SUCCESS;

    }
    //Reads a page of the memory (0-256) and prints it
    if(TERMARGS(0, "readblock"))
    {
        if(commandArgsSize <= 1) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;

        u16 blockSize = 1024;

        u32 offset = FLASH_REGION_START_ADDRESS;
        if(TERMARGS(1, "uicr")) offset = (u32)FruityHal::GetUserMemoryAddress();
        if(TERMARGS(1, "ficr")) offset = (u32)FruityHal::GetDeviceMemoryAddress();
        if(TERMARGS(1, "ram")) offset = (u32)0x20000000;
        bool didError = false;

        u16 numBlocks = 1;
        if(commandArgsSize > 2){
            numBlocks = Utility::StringToU16(commandArgs[2], &didError);
            if (didError)
            {
                return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
            }
        }

        u32 bufferSize = 32;
        DYNAMIC_ARRAY(buffer, bufferSize);
        DYNAMIC_ARRAY(charBuffer, bufferSize * 3 + 1);
        for(int j=0; j<numBlocks; j++){
            u32 block = Utility::StringToU32(commandArgs[1], &didError) + j;
            if (didError)
            {
                return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
            }

            for(u32 i=0; i<blockSize/bufferSize; i++)
            {
                CheckedMemcpy(buffer, (u8*)(block*blockSize+i*bufferSize + offset), bufferSize);
                Logger::ConvertBufferToHexString(buffer, bufferSize, (char*)charBuffer, bufferSize*3+1);
                trace("0x%08X: %s" EOL,(block*blockSize)+i*bufferSize + offset, charBuffer);
            }
        }

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    //Prints a map of empty (0) and used (1) memory pages
    if(TERMARGS(0 ,"memorymap"))
    {
        u32 offset = FLASH_REGION_START_ADDRESS;
        u16 blockSize = 1024; //Size of a memory block to check
        u16 numBlocks = FruityHal::GetCodeSize() * FruityHal::GetCodePageSize() / blockSize;

        for(u32 j=0; j<numBlocks; j++){
            u32 buffer = 0xFFFFFFFF;
            for(u32 i=0; i<blockSize; i+=4){
                buffer = buffer & *(u32*)(j*blockSize+i+offset);
            }
            if(buffer == 0xFFFFFFFF) trace("0");
            else trace("1");
        }

        trace(EOL);

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    if (TERMARGS(0,"log_error"))
    {
        if(commandArgsSize <= 2) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;

        u32 errorCode = Utility::StringToU32(commandArgs[1]);
        u16 extra = Utility::StringToU16(commandArgs[2]);

        GS->logger.LogError(LoggingError::CUSTOM, errorCode, extra);

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    else if (TERMARGS(0,"saverec"))
    {
        if(commandArgsSize <= 2) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;

        u32 recordId = Utility::StringToU32(commandArgs[1]);

        u8 buffer[50];
        u16 len = Logger::ParseEncodedStringToBuffer(commandArgs[2], buffer, 50);

        GS->recordStorage.SaveRecord(recordId, buffer, len, nullptr, 0);

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    else if (TERMARGS(0,"delrec"))
    {
        if(commandArgsSize <= 1) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;

        u32 recordId = Utility::StringToU32(commandArgs[1]);

        GS->recordStorage.DeactivateRecord(recordId, nullptr, 0);

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    else if (TERMARGS(0, "getrec"))
    {
        if(commandArgsSize <= 1) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;

        u32 recordId = Utility::StringToU32(commandArgs[1]);

        SizedData data = GS->recordStorage.GetRecordData(recordId);

        if(data.length > 0){
            for(int i=0; i<data.length; i++){
                trace("%02X:", data.data[i]);
            }

            trace(" (%u)" EOL, data.length);
        } else {
            trace("Record not found" EOL);
        }

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    else if (TERMARGS(0, "send"))
    {
        if(commandArgsSize <= 1) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;

        //parameter 1: r=reliable, u=unreliable, b=both
        //parameter 2: count

        ConnPacketData1 data;
        data.header.messageType = MessageType::DATA_1;
        data.header.sender = GS->node.configuration.nodeId;
        data.header.receiver = 0;

        data.payload.length = 7;
        data.payload.data[2] = 7;


        u8 reliable = (commandArgsSize < 2 || TERMARGS(1, "b")) ? 2 : (TERMARGS(1,"u") ? 0 : 1);

        //Second parameter is number of messages
        u8 count = commandArgsSize > 2 ? Utility::StringToU8(commandArgs[2]) : 5;

        ErrorType err = ErrorType::SUCCESS;
        for (int i = 0; i < count; i++)
        {
            if(reliable == 0 || reliable == 2){
                data.payload.data[0] = i*2;
                data.payload.data[1] = 0;
                err = GS->cm.SendMeshMessageInternal((u8*)&data, SIZEOF_CONN_PACKET_DATA_1, DeliveryPriority::LOW, false, true, true);
            }

            if(reliable == 1 || reliable == 2){
                data.payload.data[0] = i*2+1;
                data.payload.data[1] = 1;
                err = GS->cm.SendMeshMessageInternal((u8*)&data, SIZEOF_CONN_PACKET_DATA_1, DeliveryPriority::LOW, true, true, true);
            }
        }

        if (err == ErrorType::SUCCESS) return TerminalCommandHandlerReturnType::SUCCESS;
        else return TerminalCommandHandlerReturnType::INTERNAL_ERROR;
    }
    //Add an advertising job
    else if (TERMARGS(0, "advadd") && commandArgsSize >= 4)
    {
        u8 slots       = Utility::StringToU8(commandArgs[1]);
        u8 delay       = Utility::StringToU8(commandArgs[2]);
        u8 advDataByte = Utility::StringToU8(commandArgs[3]);

        AdvJob job = {
            AdvJobTypes::SCHEDULED,
            slots,
            delay,
            MSEC_TO_UNITS(100, CONFIG_UNIT_0_625_MS),
            0, //AdvChannel
            0,
            0,
            FruityHal::BleGapAdvType::ADV_IND,
            {0x02, 0x01, 0x06, 0x05, 0xFF, 0x4D, 0x02, 0xAA, advDataByte},
            9,
            {0},
            0 //ScanDataLength
        };

        GS->advertisingController.AddJob(job);

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    else if (TERMARGS(0, "advrem"))
    {
        if(commandArgsSize <= 1) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;

        i8 jobNum = Utility::StringToI8(commandArgs[1]);

        if (jobNum >= ADVERTISING_CONTROLLER_MAX_NUM_JOBS) return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;

        GS->advertisingController.RemoveJob(&(GS->advertisingController.jobs[jobNum]));

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    else if (TERMARGS(0, "advjobs"))
    {
        AdvertisingController* advCtrl = &(GS->advertisingController);
        char buffer[150];

        for (u32 i = 0; i < advCtrl->currentNumJobs; i++) {
            Logger::ConvertBufferToHexString(advCtrl->jobs[i].advData, advCtrl->jobs[i].advDataLength, buffer, sizeof(buffer));
            trace("Job type:%u, slots:%u, iv:%u, advData:%s" EOL, (u32)advCtrl->jobs[i].type, advCtrl->jobs[i].slots, advCtrl->jobs[i].advertisingInterval, buffer);
        }

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    else if (TERMARGS(0, "scanjobs"))
    {
        ScanController* scanCtrl = &(GS->scanController);

        for (u32 i = 0; i < scanCtrl->jobs.size(); i++) {
            trace("Job type %u, state %u, window %u, iv %u" EOL, (u32)scanCtrl->jobs[i].type, (u32)scanCtrl->jobs[i].state, scanCtrl->jobs[i].window, scanCtrl->jobs[i].interval);
        }

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    else if (TERMARGS(0, "feed"))
    {
        FruityHal::FeedWatchdog();
        logt("WATCHDOG", "Watchdogs fed.");

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    else if (TERMARGS(0, "lping") && commandArgsSize >= 3)
    {
        //A leaf ping will receive a response from all leaf nodes in the mesh
        //and reports the leafs nodeIds together with the number of hops

        //Save Ping sent time
        pingSentTimeMs = FruityHal::GetRtcMs();
        pingCount = Utility::StringToU16(commandArgs[1]);
        pingCountResponses = 0;
        u8 pingModeReliable = TERMARGS(2, "r");

        DebugModuleLpingMessage lpingData = {0, 0};

        for(int i=0; i<pingCount; i++){
            SendModuleActionMessage(
                MessageType::MODULE_TRIGGER_ACTION,
                NODE_ID_HOPS_BASE + 500,
                (u8)DebugModuleTriggerActionMessages::LPING,
                0,
                (u8*)&lpingData,
                SIZEOF_DEBUG_MODULE_LPING_MESSAGE,
                pingModeReliable
            );
        }
        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    if (TERMARGS(0, "nswrite")  && commandArgsSize >= 3)    //jstodo rename nswrite to flashwrite? Might also be unused because we already have saverec
    {
        u32 addr = strtoul(commandArgs[1], nullptr, 10) + FLASH_REGION_START_ADDRESS;
        u8 buffer[200];
        u16 dataLength = Logger::ParseEncodedStringToBuffer(commandArgs[2], buffer, 200);


        GS->flashStorage.CacheAndWriteData((u32*)buffer, (u32*)addr, dataLength, nullptr, 0);

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    if (TERMARGS(0, "erasepage"))
    {
        if(commandArgsSize <= 1) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;

        u16 pageNum = Utility::StringToU16(commandArgs[1]);

        GS->flashStorage.ErasePage(pageNum, nullptr, 0);

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    if (TERMARGS(0, "erasepages") && commandArgsSize >= 3)
    {

        u16 page = Utility::StringToU16(commandArgs[1]);
        u16 numPages = Utility::StringToU16(commandArgs[2]);

        GS->flashStorage.ErasePages(page, numPages, nullptr, 0);

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    if (TERMARGS(0, "filltx"))
    {
        GS->cm.FillTransmitBuffers();

        return TerminalCommandHandlerReturnType::SUCCESS;
    }
    if (TERMARGS(0, "getpending"))
    {
        logt("DEBUGMOD", "cm pending %u", GS->cm.GetPendingPackets());

        BaseConnections conns = GS->cm.GetBaseConnections(ConnectionDirection::INVALID);
        for (u32 i = 0; i < conns.count; i++) {
            BaseConnection* conn = conns.handles[i].GetConnection();
            logt("DEBUGMOD", "conn %u pend %u", conn->connectionId, conn->GetPendingPackets());
        }

        return TerminalCommandHandlerReturnType::SUCCESS;
    }

    if (TERMARGS(0, "writedata"))
    {
        if(commandArgsSize < 3) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;

        u32 destAddr = Utility::StringToU32(commandArgs[1]) + FLASH_REGION_START_ADDRESS;

        u32 buffer[16];
        u16 len = Logger::ParseEncodedStringToBuffer(commandArgs[2], (u8*)buffer, 64);

        GS->flashStorage.CacheAndWriteData(buffer, (u32*)destAddr, len, nullptr, 0);


        return TerminalCommandHandlerReturnType::SUCCESS;
    }

    if(TERMARGS(0, "printqueue") )
    {
        if(commandArgsSize <= 1) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;

        u16 hnd = Utility::StringToU16(commandArgs[1]);
        BaseConnectionHandle conn = GS->cm.GetConnectionFromHandle(hnd);

        if (conn) {
            conn.GetPacketSendQueue()->Print();
        }

        return TerminalCommandHandlerReturnType::SUCCESS;
    }

    if (TERMARGS(0, "stack_overflow"))
    {
        CauseStackOverflow();
    }
#endif

    //Must be called to allow the module to get and set the config
    return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif

void DebugModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader)
{
    //Must call superclass for handling
    Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

    //Check if this request is meant for modules in general
    if (packetHeader->messageType == MessageType::MODULE_TRIGGER_ACTION) {
        ConnPacketModule const * packet = (ConnPacketModule const *)packetHeader;
        DebugModuleTriggerActionMessages actionType = (DebugModuleTriggerActionMessages)packet->actionType;

        //Check if our module is meant and we should trigger an action
        if (packet->moduleId == moduleId) {

            if (actionType == DebugModuleTriggerActionMessages::SET_FLOOD_MODE)
            {
#if IS_INACTIVE(SAVE_SPACE)
                DebugModuleSetFloodModeMessage const * data = (DebugModuleSetFloodModeMessage const *)packet->data;
                floodMode = (FloodMode)data->floodMode;
                floodFrameSkip = true;
                floodDestinationId = data->floodDestinationId;
                floodMessagesPer10Sec = data->packetsPer10Sec;
                floodTimeoutSec = data->timeoutSec;

                //If flood mode is disabled, clear count of incoming packets, if it is enabled clear packetsOut first
                if (!(floodMode == FloodMode::OFF || floodMode == FloodMode::LISTEN)) {
                    packetsOut = 0;
                }
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    packet->header.sender,
                    (u8)DebugModuleTriggerActionMessages::RESET_FLOOD_COUNTER,
                    packet->requestHandle,
                    nullptr,
                    sizeof(joinMeBufferPacket),
                    false
                );

            }
            if (actionType == DebugModuleTriggerActionMessages::START_COUNTER)
            {
                DebugModuleStartCounterMessage const * data = (DebugModuleStartCounterMessage const *)packet->data;
                counterDestinationId = data->counterDestinationId;
                counterMessagesPer10Sec = data->packetsPer10Sec;
                counterMaxCount = data->maxCount;
                currentCounter = 0;
            }
            if (actionType == DebugModuleTriggerActionMessages::COUNTER)
            {
                DebugModuleCounterMessage const * data = (DebugModuleCounterMessage const *)packet->data;

                logjson("DEBUGMOD", "{\"type\":\"counter\",\"nodeId\":%u,\"value\":%u}" SEP, packet->header.sender, data->counter);

                if (data->counter  == 0) {
                    counterCheck = 0;
                    logt("DEBUGMOD", "Resetting counter");
                }
                else if (data->counter == counterCheck) {
                    logt("DEBUGMOD", "Counter correct at %u", counterCheck);
                }
                else {
                    logt("DEBUGMOD", "Got wrong counter value %u instead of %u", data->counter, counterCheck);
                }

                counterCheck++;
            }
            else if (actionType == DebugModuleTriggerActionMessages::FLOOD_MESSAGE)
            {
                DebugModuleFloodMessage const * data = (DebugModuleFloodMessage const *)packet->data;

                if (floodMode == FloodMode::LISTEN) {
                    if (packet->header.sender == floodDestinationId) {
                        packetsIn++;
                        if (packetsIn != data->packetsOut) {
                            logt("DEBUGMOD", "Lost messages, got %u should have %u", packetsIn, data->packetsOut);
                        }
                    }
                }
                //Listens to all nodes
                else {
                    //Keep track of all flood messages received
                    packetsIn++;
                }
#endif
            }
#if IS_ACTIVE(UNSECURE_MEMORY_READBACK)
            //This part should not be used in release builds, otherwise, the whole memory
            //contents can be read back. This is only for analyzing bugs
            else if (actionType == DebugModuleTriggerActionMessages::READ_MEMORY)
            {
                DebugModuleReadMemoryMessage const * data = (DebugModuleReadMemoryMessage const *)packet->data;

                if (data->length > readMemMaxLength) return;

                DebugModuleMemoryMessage response;
                CheckedMemset(&response, 0x00, sizeof(response));

                response.address = data->address;
                u8* memoryAddress = (u8*)(FLASH_REGION_START_ADDRESS + data->address);
                CheckedMemcpy(response.data, memoryAddress, data->length);

                SendModuleActionMessage(
                    MessageType::MODULE_ACTION_RESPONSE,
                    packet->header.sender,
                    (u8)DebugModuleActionResponseMessages::MEMORY,
                    0,
                    (u8*)&response,
                    SIZEOF_DEBUG_MODULE_MEMORY_MESSAGE_HEADER + data->length,
                    false);
            }
#endif
#if IS_INACTIVE(CLC_GW_SAVE_SPACE)
            else if (packet->actionType == (u8)DebugModuleTriggerActionMessages::RESET_FLOOD_COUNTER)
            {
                logt("DEBUGMOD", "Resetting flood counter.");
                packetsOut = 0;
                packetsIn = 0;
            }
#endif
#if IS_INACTIVE(CLC_GW_SAVE_SPACE)
            else if (actionType == DebugModuleTriggerActionMessages::GET_JOIN_ME_BUFFER)
            {
                //Send a special packet that contains my own information
                joinMeBufferPacket p;
                CheckedMemset(&p, 0x00, sizeof(joinMeBufferPacket));
                p.receivedTimeDs = GS->appTimerDs;
                p.advType = GS->advertisingController.currentAdvertisingParams.type;
                p.payload.ackField = GS->node.currentAckId;
                p.payload.clusterId = GS->node.clusterId;
                p.payload.clusterSize = GS->node.clusterSize;
                p.payload.deviceType = GET_DEVICE_TYPE();
                p.payload.freeMeshInConnections = GS->cm.freeMeshInConnections;
                p.payload.freeMeshOutConnections = GS->cm.freeMeshOutConnections;
                p.payload.sender = GS->node.configuration.nodeId;

                SendModuleActionMessage(
                    MessageType::MODULE_ACTION_RESPONSE,
                    packet->header.sender,
                    (u8)DebugModuleActionResponseMessages::JOIN_ME_BUFFER_ITEM,
                    packet->requestHandle,
                    (u8*)&p,
                    sizeof(joinMeBufferPacket),
                    false
                );

                //Send the join_me buffer items
                for (u32 i = 0; i < GS->node.joinMePackets.size(); i++)
                {
                    SendModuleActionMessage(
                        MessageType::MODULE_ACTION_RESPONSE,
                        packet->header.sender,
                        (u8)DebugModuleActionResponseMessages::JOIN_ME_BUFFER_ITEM,
                        packet->requestHandle,
                        (u8*)&(GS->node.joinMePackets[i]),
                        sizeof(joinMeBufferPacket),
                        false
                    );
                }

            }
#endif
#if IS_INACTIVE(SAVE_SPACE)
            else if (actionType == DebugModuleTriggerActionMessages::RESET_CONNECTION_LOSS_COUNTER) {

                logt("DEBUGMOD", "Resetting connection loss counter");

                GS->node.connectionLossCounter = 0;
                GS->logger.errorLogPosition = 0;

            }
            else if (actionType == DebugModuleTriggerActionMessages::SEND_MAX_MESSAGE) {
                DebugModuleSendMaxMessageResponse message;
                for (u32 i = 0; i < sizeof(message.data); i++) {
                    message.data[i] = (i % 50) + 100;
                }
                SendModuleActionMessage(
                    MessageType::MODULE_ACTION_RESPONSE,
                    packet->header.sender,
                    (u8)DebugModuleActionResponseMessages::SEND_MAX_MESSAGE_RESPONSE,
                    packet->requestHandle,
                    (u8*)&message,
                    sizeof(message),
                    false
                );
            }
            else if (actionType == DebugModuleTriggerActionMessages::GET_STATS_MESSAGE) {

                SendStatistics(packet->header.sender);

            }
            else if (actionType == DebugModuleTriggerActionMessages::CAUSE_HARDFAULT_MESSAGE) {
                logt("DEBUGMOD", "receive hardfault");
                CauseHardfault();
            }
            else if (actionType == DebugModuleTriggerActionMessages::PING) {
                //We respond to the ping
                SendModuleActionMessage(
                    MessageType::MODULE_ACTION_RESPONSE,
                    packet->header.sender,
                    (u8)DebugModuleActionResponseMessages::PING_RESPONSE,
                    packet->requestHandle,
                    nullptr,
                    0,
                    sendData->deliveryOption == DeliveryOption::WRITE_REQ
                );

            }
            else if (actionType == DebugModuleTriggerActionMessages::LPING) {
                //Only respond to the leaf ping if we are a leaf
                if (GS->cm.GetMeshConnections(ConnectionDirection::INVALID).count != 1) {
                    return;
                }

                //Insert our nodeId into the packet
                DebugModuleLpingMessage reply = *(DebugModuleLpingMessage const *)packet->data;
                reply.hops = 500 - (packetHeader->receiver - NODE_ID_HOPS_BASE);
                reply.leafNodeId = GS->node.configuration.nodeId;

                //We respond to the ping
                SendModuleActionMessage(
                    MessageType::MODULE_ACTION_RESPONSE,
                    packet->header.sender,
                    (u8)DebugModuleActionResponseMessages::LPING_RESPONSE,
                    packet->requestHandle,
                    (u8*)&reply,
                    SIZEOF_DEBUG_MODULE_LPING_MESSAGE,
                    sendData->deliveryOption == DeliveryOption::WRITE_REQ
                );

            }
            else if (actionType == DebugModuleTriggerActionMessages::PINGPONG) {

                DebugModulePingpongMessage const * data = (DebugModulePingpongMessage const *)packet->data;

                //Ping should still pong, return it
                if (data->ttl > 0) {
                    DebugModulePingpongMessage reply = *data;
                    reply.ttl = data->ttl - 1;

                    SendModuleActionMessage(
                        MessageType::MODULE_TRIGGER_ACTION,
                        packet->header.sender,
                        (u8)DebugModuleTriggerActionMessages::PINGPONG,
                        packet->requestHandle,
                        (u8*)&reply,
                        SIZEOF_DEBUG_MODULE_PINGPONG_MESSAGE,
                        sendData->deliveryOption == DeliveryOption::WRITE_REQ
                    );
                    //Arrived at destination, print it
                }
                else {
                    u32 nowTimeMs;
                    u32 timePassedMs;
                    nowTimeMs = FruityHal::GetRtcMs();
                    timePassedMs = FruityHal::GetRtcDifferenceMs(nowTimeMs, pingSentTimeMs);

                    logjson("DEBUGMOD", "{\"type\":\"pingpong_response\",\"passedTime\":%u}" SEP, timePassedMs);

                }
            }
#endif
        }
    }
    else if (packetHeader->messageType == MessageType::DATA_1) {
        if (sendData->dataLength >= SIZEOF_CONN_PACKET_HEADER + 3) //We do not need the full data packet, just the bytes that we read
        {
            ConnPacketData1 const * packet = (ConnPacketData1 const *)packetHeader;
            NodeId partnerId = connection == nullptr ? 0 : connection->partnerId;

            logt("DATA", "IN <= %d ################## Got Data packet %d:%d:%d (len:%d,%s) ##################", partnerId, packet->payload.data[0], packet->payload.data[1], packet->payload.data[2], sendData->dataLength, sendData->deliveryOption == DeliveryOption::WRITE_REQ ? "r" : "u");
        }
    }
    else if (packetHeader->messageType == MessageType::MODULE_ACTION_RESPONSE) {
        ConnPacketModule const * packet = (ConnPacketModule const *)packetHeader;
        DebugModuleActionResponseMessages actionType = (DebugModuleActionResponseMessages)packet->actionType;

        //Check if our module is meant
        if(packet->moduleId == moduleId){
#if IS_INACTIVE(SAVE_SPACE)
            if (actionType == DebugModuleActionResponseMessages::STATS_MESSAGE){
                DebugModuleInfoMessage const * infoMessage = (DebugModuleInfoMessage const *) packet->data;

                logjson_partial("DEBUGMOD", "{\"nodeId\":%u,\"type\":\"debug_stats\", \"conLoss\":%u,", packet->header.sender, infoMessage->connectionLossCounter);
                logjson_partial("DEBUGMOD", "\"dropped\":%u,", infoMessage->droppedPackets);
                logjson("DEBUGMOD", "\"sentRel\":%u,\"sentUnr\":%u}" SEP, infoMessage->sentPacketsReliable, infoMessage->sentPacketsUnreliable);
            }
            else if(actionType == DebugModuleActionResponseMessages::PING_RESPONSE){
                //Calculate the time it took to ping the other node

                u32 nowTimeMs;
                u32 timePassedMs;
                nowTimeMs = FruityHal::GetRtcMs();
                timePassedMs = FruityHal::GetRtcDifferenceMs(nowTimeMs, pingSentTimeMs);

                trace("p %u ms" EOL, timePassedMs);
                //logjson("DEBUGMOD", "{\"type\":\"ping_response\",\"passedTime\":%u}" SEP, timePassedMs);
            }
            else if (actionType == DebugModuleActionResponseMessages::SEND_MAX_MESSAGE_RESPONSE) {
                if (sendData->dataLength != MAX_MESH_PACKET_SIZE) {
                    logt("ERROR", "Packet has an invalid size");
                }

                DebugModuleSendMaxMessageResponse const * message = (DebugModuleSendMaxMessageResponse const *)packet->data;
                u32 i;
                for (i = 0; i < sizeof(message->data); i++){
                    u8 expectedValue = (i % 50) + 100;
                    if (message->data[i] != expectedValue)
                    {
                        break;
                    }
                }
                logjson("DEBUGMOD", "{\"nodeId\":%u,\"type\":\"send_max_message_response\", \"correctValues\":%u, \"expectedCorrectValues\":%u}" SEP, packet->header.sender, i, sizeof(message->data));
            }
            else if (actionType == DebugModuleActionResponseMessages::MEMORY) {
                if (sendData->dataLength < SIZEOF_CONN_PACKET_MODULE + SIZEOF_DEBUG_MODULE_MEMORY_MESSAGE_HEADER) return;

                DebugModuleMemoryMessage const * message = (DebugModuleMemoryMessage const *)packet->data;
                
                u16 memoryLength = sendData->dataLength - SIZEOF_CONN_PACKET_MODULE - SIZEOF_DEBUG_MODULE_MEMORY_MESSAGE_HEADER;

                u16 bufferLength = memoryLength * 3 + 1;
                DYNAMIC_ARRAY(buffer, bufferLength);
                CheckedMemset(buffer, 0x00, bufferLength);

                Logger::ConvertBufferToHexString(message->data, memoryLength, (char*)buffer, bufferLength);

                logjson("DEBUGMOD", "{\"nodeId\":%u,\"type\":\"memory\",\"address\":%u,\"data\":\"%s\"}" SEP,
                    packet->header.sender,
                    message->address,
                    buffer
                    );
            }
            else if(actionType == DebugModuleActionResponseMessages::LPING_RESPONSE){
                //Calculate the time it took to ping the other node

                DebugModuleLpingMessage const * lpingData = (DebugModuleLpingMessage const *)packet->data;

                u32 nowTimeMs;
                u32 timePassedMs;
                nowTimeMs = FruityHal::GetRtcMs();
                timePassedMs = FruityHal::GetRtcDifferenceMs(nowTimeMs, pingSentTimeMs);

                trace("lp %u(%u): %u ms" EOL, lpingData->leafNodeId, lpingData->hops, timePassedMs);
            }

#endif
#if IS_INACTIVE(CLC_GW_SAVE_SPACE)
            if(actionType == DebugModuleActionResponseMessages::JOIN_ME_BUFFER_ITEM){
                //Must copy the data to not produce a hardfault because of unaligned access
                joinMeBufferPacket data;
                CheckedMemcpy(&data, packet->data, sizeof(joinMeBufferPacket));

                logjson("DEBUG", "{\"buf\":\"advT %u,rssi %d,time %u,last %u,node %u,cid %u,csiz %d, in %u, out %u, devT %u, ack %x\"}" SEP,
                        (u32)data.advType,                  data.rssi,                          data.receivedTimeDs, data.lastConnectAttemptDs,
                        data.payload.sender,                data.payload.clusterId,             data.payload.clusterSize,
                        data.payload.freeMeshInConnections, data.payload.freeMeshOutConnections,
                        (u32)data.payload.deviceType, data.payload.ackField
                );
            }
#endif
        }
    }
}

u32 DebugModule::GetPacketsIn()
{
    return packetsIn;
}

u32 DebugModule::GetPacketsOut()
{
    return packetsOut;
}

void DebugModule::CauseHardfault() const
{
#ifdef SIM_ENABLED
    SIMEXCEPTION(HardfaultException);
#else
    //Attempts to write to write to address 0, which is in flash
    *((int*)0x0) = 10;
#endif
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winfinite-recursion"
#endif
void DebugModule::CauseStackOverflow() const
{
    volatile char someDummyData[128];
    for (size_t i = 0; i < sizeof(someDummyData); i++)
    {
        someDummyData[i] = 0x12;
    }
    logt("MAIN", "Dummy data addr: %u", (u32)&someDummyData);
    CauseStackOverflow();
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
