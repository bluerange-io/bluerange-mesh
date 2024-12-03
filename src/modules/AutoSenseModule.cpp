////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2022 M-Way Solutions GmbH
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

#include <AutoSenseModule.h>

#include <GlobalState.h>
#include <Logger.h>
#include <Utility.h>
#include <Node.h>
#include <IoModule.h>

AutoSenseModule::AutoSenseModule()
    : Module(ModuleId::AUTO_SENSE_MODULE, "autosense")
{
    GS->logger.EnableTag("ASMOD");

    //Save configuration to base class variables
    //sizeof configuration must be a multiple of 4 bytes
    configurationPointer = &configuration;
    configurationLength = sizeof(AutoSenseModuleConfiguration);

    GS->timeManager.AddTimeSyncedListener(this);

    //Set defaults
    ResetToDefaultConfiguration();
}

void AutoSenseModule::ResetToDefaultConfiguration()
{
    //Set default configuration values
    configuration.moduleId = ModuleId::AUTO_SENSE_MODULE;
    configuration.moduleActive = true;
    configuration.moduleVersion = AUTO_SENSE_MODULE_CONFIG_VERSION;

    //Set additional config values...

    //This line allows us to have different configurations of this module depending on the featureset
    SET_FEATURESET_CONFIGURATION(&configuration, this);
}

void AutoSenseModule::ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength)
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    AutoSenseModuleConfiguration* newConfig = (AutoSenseModuleConfiguration*)migratableConfig;

    //Version migration can be added here, e.g. if module has version 2 and config is version 1
    if(newConfig != nullptr && newConfig->moduleVersion == 1){/* ... */};

    //Do additional initialization upon loading the config

    for (u32 entryIndex = 0; entryIndex < MAX_AMOUNT_OF_ENTRIES; entryIndex++)
    {
        const AutoSenseTableEntryV0* tableEntry = getTableEntryV0(entryIndex);
        if (tableEntry)
        {
            AddScheduleEvents(entryIndex, tableEntry);
            valueCache.registerSlot(entryIndex, tableEntry->length);
        }
    }
    //Start the Module...

#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
}

void AutoSenseModule::TimerEventHandler(u16 passedTimeDs)
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    pollSchedule.advanceTime(passedTimeDs);
    for (u32 breakout = 0; breakout < 100 && (pollSchedule.isEventReady()); breakout++) // Breakout to avoid endless loops on misconfig
    {
        const u8 entryIndex = pollSchedule.getAndReenter();
        const AutoSenseTableEntryV0* tableEntry = getTableEntryV0(entryIndex);
        bool foundMatchingDataProvider = false;
        for (u32 i = 0; i < MAX_AMOUNT_DATA_PROVIDERS; i++)
        {
            if (dataProviders[i].dataProvider && Utility::IsSameModuleId(dataProviders[i].moduleId, tableEntry->moduleId))
            {
                foundMatchingDataProvider = true;
                dataProviders[i].dataProvider->RequestData(tableEntry->component, tableEntry->register_, tableEntry->length, this);
            }
        }
        if (!foundMatchingDataProvider)
        {
            logt("ERROR", "Got no DataProvider for moduleId %u", tableEntry->moduleId);  //LCOV_EXCL_LINE Unclear how to get in this state.
            SIMEXCEPTION(IllegalStateException);                                         //LCOV_EXCL_LINE Unclear how to get in this state.
        }
    }

    reportSchedule.advanceTime(passedTimeDs);
    for (u32 breakout = 0; breakout < 100 && (reportSchedule.isEventReady()); breakout++) // Breakout to avoid endless loops on misconfig
    {
        const u8 entryIndex = reportSchedule.getAndReenter();
        if (!anyValueRecorded.get(entryIndex))
        {
            GS->logger.LogCustomCount(CustomErrorTypes::WARN_AUTO_SENSE_REPORT_WITHOUT_DATA);
            continue; // Got no data to report yet.
        }
        const AutoSenseTableEntryV0* tableEntry = getTableEntryV0(entryIndex);

        if (tableEntry->reportFunction == AutoSenseFunction::LAST)
        {
            // No special handling required.
        }
        else if (tableEntry->reportFunction == AutoSenseFunction::ON_CHANGE_RATE_LIMITED)
        {
            if (!readyForSending.get(entryIndex))
            {
                continue;
            }
            readyForSending.set(entryIndex, false);
        }
        else if (tableEntry->reportFunction == AutoSenseFunction::ON_CHANGE_WITH_PERIODIC_REPORT)
        {
            // No special handling required.
        }
        else
        {
            SIMEXCEPTION(IllegalStateException); // LCOV_EXCL_LINE Unclear how to get in this state.
        }

        SendEntry(entryIndex, tableEntry);
    }
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
}

#ifdef TERMINAL_ENABLED
TerminalCommandHandlerReturnType AutoSenseModule::TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize)
{
    //React on commands, return true if handled, false otherwise
    if(commandArgsSize >= 3 && TERMARGS(2, moduleName))
    {
        if(TERMARGS(0, "action"))
        {
            if(!TERMARGS(2, moduleName)) return TerminalCommandHandlerReturnType::UNKNOWN;

            bool didError = false;
            const NodeId destinationNode = Utility::TerminalArgumentToNodeId(commandArgs[1], &didError);
            if (didError) return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;

            if(TERMARGS(3, "set_autosense_entry"))
            {
                if (commandArgsSize < 7) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
                const u8 moduleVersion = Utility::StringToU8(commandArgs[4], &didError);
                const u8 entryIndex    = Utility::StringToU8(commandArgs[5], &didError);
                // Example payload hex string: 02:4D:00:9D:01:03:23:27:04:00:00:0A:0A:00
                // has length of 41 + \0
                u8 entryBuffer[250];
                const u32 entrySize = Logger::ParseEncodedStringToBuffer(commandArgs[6], entryBuffer, sizeof(entryBuffer), &didError);
                const u8 requestHandle = commandArgsSize >= 8 ? Utility::StringToU8(commandArgs[7], &didError) : 0;
                if (didError || entrySize == 0) return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                
                u32 sendBufferSize = sizeof(AutoSenseModuleSetEntryMessage) - sizeof(AutoSenseModuleSetEntryMessage::data) + entrySize;
                DYNAMIC_ARRAY(sendBuffer, sendBufferSize);
                CheckedMemset(sendBuffer, 0, sendBufferSize);
                AutoSenseModuleSetEntryMessage* msg = (AutoSenseModuleSetEntryMessage*)(sendBuffer);
                msg->moduleVersion = moduleVersion;
                msg->entryIndex = entryIndex;
                CheckedMemcpy(msg->data, entryBuffer, entrySize);
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)AutoSenseModuleTriggerAndResponseMessages::SET_ENTRY,
                    requestHandle,
                    sendBuffer,
                    sendBufferSize,
                    false
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            else if (TERMARGS(3, "get_autosense_entry"))
            {
                if (commandArgsSize < 5) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
                AutoSenseModuleGetEntryMessage msg = {};
                msg.entryIndex = Utility::StringToU8(commandArgs[4], &didError);
                const u8 requestHandle = commandArgsSize >= 6 ? Utility::StringToU8(commandArgs[5], &didError) : 0;
                if (didError) return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)AutoSenseModuleTriggerAndResponseMessages::GET_ENTRY,
                    requestHandle,
                    (u8*)&msg,
                    sizeof(msg),
                    false
                );

                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            else if (TERMARGS(3, "clear_autosense_entry"))
            {
                if (commandArgsSize < 5) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
                AutoSenseModuleClearEntryMessage msg = {};
                msg.entryIndex = Utility::StringToU8(commandArgs[4], &didError);
                const u8 requestHandle = commandArgsSize >= 6 ? Utility::StringToU8(commandArgs[5], &didError) : 0;
                if (didError) return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;

                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)AutoSenseModuleTriggerAndResponseMessages::CLEAR_ENTRY,
                    requestHandle,
                    (u8*)&msg,
                    sizeof(msg),
                    false
                );
                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            else if (TERMARGS(3, "get_autosense_table"))
            {
                const u8 requestHandle = commandArgsSize >= 5 ? Utility::StringToU8(commandArgs[4], &didError) : 0;
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)AutoSenseModuleTriggerAndResponseMessages::GET_TABLE,
                    requestHandle,
                    nullptr,
                    0,
                    false
                );
                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            else if (TERMARGS(3, "set_example"))
            {
                const u8 requestHandle = commandArgsSize >= 5 ? Utility::StringToU8(commandArgs[4], &didError) : 0;
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)AutoSenseModuleTriggerAndResponseMessages::SET_EXAMPLE,
                    requestHandle,
                    nullptr,
                    0,
                    false
                );
                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            else if (TERMARGS(3, "clear_example"))
            {
                const u8 requestHandle = commandArgsSize >= 5 ? Utility::StringToU8(commandArgs[4], &didError) : 0;
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)AutoSenseModuleTriggerAndResponseMessages::CLEAR_EXAMPLE,
                    requestHandle,
                    nullptr,
                    0,
                    false
                );
                return TerminalCommandHandlerReturnType::SUCCESS;
            }

            return TerminalCommandHandlerReturnType::UNKNOWN;

        }
    }

    //Must be called to allow the module to get and set the config
    return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif

bool AutoSenseModule::ConsumeData(ModuleId moduleId, u16 component, u16 register_, u8 length, const u8* data)
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    return ConsumeData(Utility::GetWrappedModuleId(moduleId), component, register_, length, data);
#else
    return true;
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
}

bool AutoSenseModule::ConsumeData(ModuleIdWrapper moduleId, u16 component, u16 register_, u8 length, const u8* data)
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    bool anyEntryTookValue = false;
    for (u32 entryIndex = 0; entryIndex < MAX_AMOUNT_OF_ENTRIES; entryIndex++)
    {
        const AutoSenseTableEntryV0* tableEntry = getTableEntryV0(entryIndex);
        if (!tableEntry
            || !Utility::IsSameModuleId(tableEntry->moduleId, moduleId)
            || tableEntry->component != component
            || tableEntry->register_ != register_
            || tableEntry->length != length)
        {
            continue;
        }
        u16 l = valueCache.getSizeOfSlot(entryIndex);
        u8* writePointer = valueCache.get(entryIndex);
        if (!writePointer || l != length)
        {
            SIMEXCEPTION(IllegalStateException);                                                                              //LCOV_EXCL_LINE Unclear how to get in this state.
            logt("ERROR", "Value Cache had other size (%u) than tableEntry (%u). Most likely a firmware bug. Value dropped.", //LCOV_EXCL_LINE Unclear how to get in this state.
                (u32)l,                                                                                                       //LCOV_EXCL_LINE Unclear how to get in this state.
                (u32)length);                                                                                                 //LCOV_EXCL_LINE Unclear how to get in this state.
            continue;                                                                                                         //LCOV_EXCL_LINE Unclear how to get in this state.
        }

        if (tableEntry->reportFunction == AutoSenseFunction::LAST)
        {
            CheckedMemcpy(writePointer, data, length);
        }
        else if (tableEntry->reportFunction == AutoSenseFunction::ON_CHANGE_RATE_LIMITED)
        {
            if (!anyValueRecorded.get(entryIndex)
                || 0 != memcmp(writePointer, data, length))
            {
                readyForSending.set(entryIndex, true);
                CheckedMemcpy(writePointer, data, length);
                // Careful: We must NOT set "readyForSending" to false here, or else
                //          multiple reported values might wrongly never send out.
            }
        }
        else if (tableEntry->reportFunction == AutoSenseFunction::ON_CHANGE_WITH_PERIODIC_REPORT)
        {
            bool changed = false;
            if (!anyValueRecorded.get(entryIndex)
                || 0 != memcmp(writePointer, data, length))
            {
                changed = true;
            }
            CheckedMemcpy(writePointer, data, length);
            if (changed)
            {
                SendEntry(entryIndex, tableEntry);
            }
        }
        else
        {
            SIMEXCEPTION(IllegalStateException); //LCOV_EXCL_LINE Unclear how to get in this state.
        }

        anyEntryTookValue = true;
        anyValueRecorded.set(entryIndex, true);
    }
    return anyEntryTookValue;
#else
    return true;
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
}

void AutoSenseModule::TimeSyncedHandler()
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    const u32 ds = SEC_TO_DS(GS->timeManager.GetLocalTime());
    pollSchedule.setAbsoluteTime(ds);
    reportSchedule.setAbsoluteTime(ds);
#endif
}

const AutoSenseTableEntryV0* AutoSenseModule::getTableEntryV0(u8 entryIndex)
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    SizedData data = GS->recordStorage.GetRecordData(RECORD_STORAGE_RECORD_ID_AUTO_SENSE_ENTRIES_BASE + entryIndex);
    if (data.data && data.length >= sizeof(AutoSenseTableEntryV0))
    {
        return (const AutoSenseTableEntryV0*)data.data;
    }
    return nullptr;
#else
    return nullptr;
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
}

void AutoSenseModule::RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength)
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    if (userDataLength != sizeof(RecordStorageUserData))
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE Unclear how to get in this state.
    }
    RecordStorageUserData* ud = (RecordStorageUserData*)userData;
    if (userType == (u32)AutoSenseModuleTriggerAndResponseMessages::SET_ENTRY)
    {
        if (resultCode != RecordStorageResultCode::SUCCESS)
        {
            valueCache.unregisterSlot(ud->entryIndex);                                                                                             //LCOV_EXCL_LINE Unclear how to get in this state.
            SendResponse(AutoSenseModuleSetEntryResponse{ TranslateRecordStorageCode(resultCode), ud->entryIndex }, ud->sender, ud->requestHandle); //LCOV_EXCL_LINE Unclear how to get in this state.
        }
        else
        {
            const AutoSenseTableEntryV0* table = getTableEntryV0(ud->entryIndex);
            if (table == nullptr)
            {
                // The record storage just called us that we were successfully writing the record, yet it's not available? Bug?
                SIMEXCEPTION(IllegalStateException); //LCOV_EXCL_LINE Unclear how to get in this state.
            }
            else
            {
                pollSchedule.removeEvent(ud->entryIndex);
                reportSchedule.removeEvent(ud->entryIndex);

                AddScheduleEvents(ud->entryIndex, table);
            }

            SendResponse(AutoSenseModuleSetEntryResponse{ AutoSenseModuleResponseCode::SUCCESS, ud->entryIndex }, ud->sender, ud->requestHandle);
        }
    }
    else if (userType == (u32)AutoSenseModuleTriggerAndResponseMessages::CLEAR_ENTRY
          || userType == (u32)AutoSenseModuleTriggerAndResponseMessages::CLEAR_ALL_ENTRIES)
    {
        if (resultCode != RecordStorageResultCode::SUCCESS)
        {
            u8 reportedEntry = ud->entryIndex;
            if (userType == (u32)AutoSenseModuleTriggerAndResponseMessages::CLEAR_ALL_ENTRIES) reportedEntry = ALL_ENTRIES;
            SendResponse(AutoSenseModuleClearEntryResponse{ TranslateRecordStorageCode(resultCode), reportedEntry }, ud->sender, ud->requestHandle); //LCOV_EXCL_LINE Unclear how to get in this state.
        }
        else
        {
            pollSchedule.removeEvent(ud->entryIndex);
            reportSchedule.removeEvent(ud->entryIndex);
            valueCache.unregisterSlot(ud->entryIndex);
            anyValueRecorded.set(ud->entryIndex, false);
            readyForSending.set(ud->entryIndex, false);

            if (userType == (u32)AutoSenseModuleTriggerAndResponseMessages::CLEAR_ENTRY)
            {
                SendResponse(AutoSenseModuleClearEntryResponse{ AutoSenseModuleResponseCode::SUCCESS, ud->entryIndex }, ud->sender, ud->requestHandle);
            }
            else if (userType == (u32)AutoSenseModuleTriggerAndResponseMessages::CLEAR_ALL_ENTRIES)
            {
                ClearAllEntries(ud->sender, ud->requestHandle);
            }
            else
            {
                SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE Unclear how to get in this state.
            }
        }
    }
    else
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE Unclear how to get in this state.
    }
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
}

AutoSenseModuleResponseCode AutoSenseModule::TranslateRecordStorageCode(const RecordStorageResultCode& code)
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    if (code == RecordStorageResultCode::SUCCESS) return AutoSenseModuleResponseCode::SUCCESS;                                                      //LCOV_EXCL_LINE Unclear how to get in this state.
                                                                                                                                                     //LCOV_EXCL_LINE Unclear how to get in this state.
    constexpr u8 range = (u8)AutoSenseModuleResponseCode::RECORD_STORAGE_CODES_END - (u8)AutoSenseModuleResponseCode::RECORD_STORAGE_CODES_START;  //LCOV_EXCL_LINE Unclear how to get in this state.
                                                                                                                                                     //LCOV_EXCL_LINE Unclear how to get in this state.
    u8 u8Code = (u8)code;                                                                                                                            //LCOV_EXCL_LINE Unclear how to get in this state.
    if (u8Code > range)                                                                                                                              //LCOV_EXCL_LINE Unclear how to get in this state.
    {                                                                                                                                                //LCOV_EXCL_LINE Unclear how to get in this state.
        logt("WARNING", "Got Record Storage Result Code outside the range of translatable codes!");                                                  //LCOV_EXCL_LINE Unclear how to get in this state.
        u8Code = range;                                                                                                                              //LCOV_EXCL_LINE Unclear how to get in this state.
    }                                                                                                                                                //LCOV_EXCL_LINE Unclear how to get in this state.
                                                                                                                                                     //LCOV_EXCL_LINE Unclear how to get in this state.
    return AutoSenseModuleResponseCode(u8Code + (u8)AutoSenseModuleResponseCode::RECORD_STORAGE_CODES_START);                                      //LCOV_EXCL_LINE Unclear how to get in this state.
#else
    return AutoSenseModuleResponseCode::NO_SUCH_DATA_PROVIDER;
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
}

void AutoSenseModule::SendEntry(u8 entryIndex, const AutoSenseTableEntryV0* tableEntry)
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    //JSTODO basically a copy past from the node. Shouldn't we put this somewhere central?
    ModuleIdWrapper moduleId = tableEntry->moduleId;

    u8 buffer[200];
    const u16 payloadLength = valueCache.getSizeOfSlot(entryIndex);
    u16 totalLength = 0;

    //We use a different packet format with an additional 3 bytes in case of a vendor module id
    if (Utility::IsVendorModuleId(moduleId))
    {
        ConnPacketComponentMessageVendor* message = (ConnPacketComponentMessageVendor*)buffer;
        message->componentHeader.header.messageType = MessageType::COMPONENT_SENSE;
        message->componentHeader.header.sender = GS->node.configuration.nodeId;
        message->componentHeader.header.receiver = tableEntry->destNodeId;
        message->componentHeader.moduleId = (VendorModuleId)moduleId;
        message->componentHeader.actionType = (u8)SensorMessageActionType::UNSPECIFIED;
        message->componentHeader.component = tableEntry->component;
        message->componentHeader.registerAddress = tableEntry->register_;
        message->componentHeader.requestHandle = tableEntry->requestHandle;
        CheckedMemcpy(buffer + SIZEOF_COMPONENT_MESSAGE_HEADER_VENDOR, valueCache.get(entryIndex), payloadLength);
        totalLength = SIZEOF_CONN_PACKET_COMPONENT_MESSAGE_VENDOR + payloadLength;
    }
    else
    {
        ConnPacketComponentMessage* message = (ConnPacketComponentMessage*)buffer;
        message->componentHeader.header.messageType = MessageType::COMPONENT_SENSE;
        message->componentHeader.header.sender = GS->node.configuration.nodeId;
        message->componentHeader.header.receiver = tableEntry->destNodeId;
        message->componentHeader.moduleId = Utility::GetModuleId(moduleId);
        message->componentHeader.actionType = (u8)SensorMessageActionType::UNSPECIFIED;
        message->componentHeader.component = tableEntry->component;
        message->componentHeader.registerAddress = tableEntry->register_;
        message->componentHeader.requestHandle = tableEntry->requestHandle;
        CheckedMemcpy(buffer + SIZEOF_COMPONENT_MESSAGE_HEADER, valueCache.get(entryIndex), payloadLength);
        totalLength = SIZEOF_CONN_PACKET_COMPONENT_MESSAGE + payloadLength;
    }

    GS->cm.SendMeshMessage(
        buffer,
        totalLength);
#endif
}

void AutoSenseModule::AddScheduleEvents(u32 entryIndex, const AutoSenseTableEntryV0* table)
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    if (table->periodicReportInterval == (u8)SyncedReportInterval::NONE)
    {
        pollSchedule  .addEvent(entryIndex, table->pollingIvDs,   0, EventTimeType::RELATIVE);
        reportSchedule.addEvent(entryIndex, table->reportingIvDs, 0, EventTimeType::RELATIVE);
    }
    else
    {
        u32 ds = 10;
             if (table->periodicReportInterval == (u8)SyncedReportInterval::SECOND)      ds = SEC_TO_DS(1);
        else if (table->periodicReportInterval == (u8)SyncedReportInterval::TEN_SECONDS) ds = SEC_TO_DS(10);
        else if (table->periodicReportInterval == (u8)SyncedReportInterval::MINUTE)      ds = SEC_TO_DS(60);
        else if (table->periodicReportInterval == (u8)SyncedReportInterval::TEN_MINUTES) ds = SEC_TO_DS(60 * 10);
        else if (table->periodicReportInterval == (u8)SyncedReportInterval::HALF_HOUR)   ds = SEC_TO_DS(60 * 30);
        else if (table->periodicReportInterval == (u8)SyncedReportInterval::HOUR)        ds = SEC_TO_DS(60 * 60);
        else if (table->periodicReportInterval == (u8)SyncedReportInterval::DAILY)       ds = SEC_TO_DS(60 * 60 * 24);
        else
        {
            SIMEXCEPTION(IllegalStateException);
        }
        pollSchedule  .addEvent(entryIndex, ds, table->pollingIvDs,   EventTimeType::SYNCED);
        reportSchedule.addEvent(entryIndex, ds, table->reportingIvDs, EventTimeType::SYNCED);
    }
#endif
}

void AutoSenseModule::SendResponse(const AutoSenseModuleSetEntryResponse& response, NodeId id, u8 requestHandle) const
{
    SendModuleActionMessage(
        MessageType::MODULE_ACTION_RESPONSE,
        id,
        (u8)AutoSenseModuleTriggerAndResponseMessages::SET_ENTRY,
        requestHandle,
        (const u8*)&response,
        sizeof(response),
        false
    );
}

void AutoSenseModule::SendResponse(const AutoSenseModuleGetEntryResponse& response, NodeId id, u8 requestHandle) const
{
    SendModuleActionMessage(
        MessageType::MODULE_ACTION_RESPONSE,
        id,
        (u8)AutoSenseModuleTriggerAndResponseMessages::GET_ENTRY,
        requestHandle,
        (const u8*)&response,
        sizeof(response),
        false
    );
}

void AutoSenseModule::SendResponse(const AutoSenseModuleClearEntryResponse& response, NodeId id, u8 requestHandle) const
{
    SendModuleActionMessage(
        MessageType::MODULE_ACTION_RESPONSE,
        id,
        (u8)AutoSenseModuleTriggerAndResponseMessages::CLEAR_ENTRY,
        requestHandle,
        (const u8*)&response,
        sizeof(response),
        false
    );
}

void AutoSenseModule::RegisterDataProvider(ModuleIdWrapper moduleId, AutoSenseModuleDataProvider* dataProvider)
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    bool found = false;
    for (size_t i = 0; i < MAX_AMOUNT_DATA_PROVIDERS; i++)
    {
        if (dataProviders[i].dataProvider == dataProvider)
        {
            // Did you accidentally register the same provider twice?
            SIMEXCEPTION(IllegalArgumentException);
            found = true;
            break;
        }
        if (dataProviders[i].dataProvider == nullptr)
        {
            dataProviders[i] = ModuleIdDataProviderPair(moduleId, dataProvider);
            found = true;
            break;
        }
    }
    if (!found)
    {
        logt("ERROR", "Failed to register Data Provider for AutoSenseModule!");
        SIMEXCEPTION(BufferTooSmallException);
    }
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
}

void AutoSenseModule::RegisterDataProvider(ModuleId moduleId, AutoSenseModuleDataProvider* dataProvider)
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    RegisterDataProvider(Utility::GetWrappedModuleId(moduleId), dataProvider);
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
}

void AutoSenseModule::ClearAllEntries(NodeId sender, u8 requestHandle)
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    for (u8 entryIndex = 0; entryIndex < MAX_AMOUNT_OF_ENTRIES; entryIndex++)
    {
        if (getTableEntryV0(entryIndex))
        {
            RecordStorageUserData userData = {};
            userData.sender = sender;
            userData.entryIndex = entryIndex;
            userData.requestHandle = requestHandle;
            RecordStorageResultCode code = GS->recordStorage.DeactivateRecord(RECORD_STORAGE_RECORD_ID_AUTO_SENSE_ENTRIES_BASE + entryIndex, this, (u8)AutoSenseModuleTriggerAndResponseMessages::CLEAR_ALL_ENTRIES, (u8*)&userData, sizeof(userData));
            if (code != RecordStorageResultCode::SUCCESS)
            {
                SendResponse(AutoSenseModuleClearEntryResponse{ TranslateRecordStorageCode(code), ALL_ENTRIES }, sender, requestHandle);
            }
            return;
        }
    }

    // If we have no entries, then deleting all entries was "successful".
    SendResponse(AutoSenseModuleClearEntryResponse{ AutoSenseModuleResponseCode::SUCCESS, ALL_ENTRIES }, sender, requestHandle);
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
}

void AutoSenseModule::ClearEntry(u8 entryIndex, NodeId sender, u8 requestHandle)
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    RecordStorageUserData userData = {};
    userData.sender = sender;
    userData.entryIndex = entryIndex;
    userData.requestHandle = requestHandle;
    RecordStorageResultCode code = GS->recordStorage.DeactivateRecord(RECORD_STORAGE_RECORD_ID_AUTO_SENSE_ENTRIES_BASE + entryIndex, this, (u8)AutoSenseModuleTriggerAndResponseMessages::CLEAR_ENTRY, (u8*)&userData, sizeof(userData));
    if (code != RecordStorageResultCode::SUCCESS)
    {
        SendResponse(AutoSenseModuleClearEntryResponse{ TranslateRecordStorageCode(code), entryIndex }, sender, requestHandle);     //LCOV_EXCL_LINE Unclear how to get in this state.
        return;                                                                                                                      //LCOV_EXCL_LINE Unclear how to get in this state.
    }
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
}

void AutoSenseModule::SetEntry(u8 entryIndex, const AutoSenseTableEntryV0* tableEntry, u8 moduleVersion, NodeId sender, u8 requestHandle)
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    if (entryIndex >= MAX_AMOUNT_OF_ENTRIES)
    {
        SendResponse(AutoSenseModuleSetEntryResponse{ AutoSenseModuleResponseCode::ENTRY_INDEX_OUT_OF_RANGE, entryIndex }, sender, requestHandle);
        return;
    }
    if (moduleVersion != AUTO_SENSE_MODULE_CONFIG_VERSION)
    {
        SendResponse(AutoSenseModuleSetEntryResponse{ AutoSenseModuleResponseCode::MODULE_VERSION_NOT_SUPPORTED, entryIndex }, sender, requestHandle);
        return;
    }
    if (tableEntry->reservedFlags != 0
        || tableEntry->pollingIvDs == 0
        || tableEntry->pollingIvDs > 36000 // From the Ticket: Reserve everything above 36000 (1 hour) as future use for now.
        || tableEntry->reportingIvDs == 0
        || tableEntry->reportingIvDs > 36000 // From the Ticket: Reserve everything above 36000 (1 hour) as future use for now.
        || (
            tableEntry->reportFunction != AutoSenseFunction::LAST
            && tableEntry->reportFunction != AutoSenseFunction::ON_CHANGE_RATE_LIMITED
            && tableEntry->reportFunction != AutoSenseFunction::ON_CHANGE_WITH_PERIODIC_REPORT
            )
        || !Utility::IsValidModuleIdFormat(tableEntry->moduleId)
        )
    {
        SendResponse(AutoSenseModuleSetEntryResponse{ AutoSenseModuleResponseCode::UNSUPPORTED_ENTRY_CONTENTS, entryIndex }, sender, requestHandle);
        return;
    }
    bool foundMatchingDataProvider = false;
    for (u32 i = 0; i < MAX_AMOUNT_DATA_PROVIDERS; i++)
    {
        if (dataProviders[i].dataProvider 
            && Utility::IsSameModuleId(dataProviders[i].moduleId, tableEntry->moduleId)
            && dataProviders[i].dataProvider->AcceptsRegister(tableEntry->component, tableEntry->register_, tableEntry->length))
        {
            foundMatchingDataProvider = true;
        }
    }
    if (!foundMatchingDataProvider)
    {
        SendResponse(AutoSenseModuleSetEntryResponse{ AutoSenseModuleResponseCode::NO_SUCH_DATA_PROVIDER, entryIndex }, sender, requestHandle);
        return;
    }
    u8* valueCacheEntry = valueCache.registerSlot(entryIndex, tableEntry->length);
    if (!valueCacheEntry)
    {
        SendResponse(AutoSenseModuleSetEntryResponse{ AutoSenseModuleResponseCode::FAILED_TO_CREATE_VALUE_CACHE_ENTRY, entryIndex }, sender, requestHandle);
        return;
    }
    RecordStorageUserData userData = {};
    userData.sender = sender;
    userData.entryIndex = entryIndex;
    userData.requestHandle = requestHandle;
    RecordStorageResultCode code = GS->recordStorage.SaveRecord(RECORD_STORAGE_RECORD_ID_AUTO_SENSE_ENTRIES_BASE + entryIndex, (const u8*)tableEntry, sizeof(*tableEntry), this, (u32)AutoSenseModuleTriggerAndResponseMessages::SET_ENTRY, (u8*)&userData, sizeof(userData));
    if (code != RecordStorageResultCode::SUCCESS)
    {
        valueCache.unregisterSlot(entryIndex);
        SendResponse(AutoSenseModuleSetEntryResponse{ TranslateRecordStorageCode(code), entryIndex }, sender, requestHandle);
        return;
    }
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
}


void AutoSenseModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader)
{
    //Must call superclass for handling
    Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);


#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    //Parse trigger actions
    if (packetHeader->messageType == MessageType::MODULE_TRIGGER_ACTION && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE)
    {
        ConnPacketModule const* packet = (ConnPacketModule const*)packetHeader;

        //Check if our module is meant and we should trigger an action
        if (packet->moduleId == ModuleId::AUTO_SENSE_MODULE)
        {
            if (packet->actionType == (u8)AutoSenseModuleTriggerAndResponseMessages::SET_ENTRY && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE + sizeof(AutoSenseModuleSetEntryMessage))
            {
                const AutoSenseModuleSetEntryMessage* msg = (const AutoSenseModuleSetEntryMessage*)packet->data;
                u32 entrySize = sendData->dataLength.GetRaw() - SIZEOF_CONN_PACKET_MODULE - sizeof(AutoSenseModuleSetEntryMessage) + sizeof(AutoSenseModuleSetEntryMessage::data);
                if (entrySize != sizeof(AutoSenseTableEntryV0))
                {
                    SendResponse(AutoSenseModuleSetEntryResponse{ AutoSenseModuleResponseCode::UNSUPPORTED_ENTRY_SIZE, msg->entryIndex }, packet->header.sender, packet->requestHandle);
                    return;
                }
                const AutoSenseTableEntryV0* tableEntry = (const AutoSenseTableEntryV0*)msg->data;
                SetEntry(msg->entryIndex, tableEntry, msg->moduleVersion, packet->header.sender, packet->requestHandle);
            }
            else if (packet->actionType == (u8)AutoSenseModuleTriggerAndResponseMessages::GET_ENTRY && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE + sizeof(AutoSenseModuleGetEntryMessage))
            {
                const AutoSenseModuleGetEntryMessage* msg = (const AutoSenseModuleGetEntryMessage*)packet->data;
                if (msg->entryIndex >= MAX_AMOUNT_OF_ENTRIES)
                {
                    SendResponse(AutoSenseModuleGetEntryResponse{ AutoSenseModuleResponseCode::ENTRY_INDEX_OUT_OF_RANGE, msg->entryIndex, {0} }, packet->header.sender, packet->requestHandle);
                    return;
                }
                SizedData record = GS->recordStorage.GetRecordData(RECORD_STORAGE_RECORD_ID_AUTO_SENSE_ENTRIES_BASE + msg->entryIndex);
                if (!record.data)
                {
                    SendResponse(AutoSenseModuleGetEntryResponse{ AutoSenseModuleResponseCode::NO_SUCH_ENTRY, msg->entryIndex, {0} }, packet->header.sender, packet->requestHandle);
                    return;
                }
                const u32 bufferSize = sizeof(AutoSenseModuleGetEntryResponse) - sizeof(AutoSenseModuleGetEntryResponse::data) + record.length.GetRaw();
                DYNAMIC_ARRAY(sendBuffer, bufferSize);
                CheckedMemset(sendBuffer, 0, bufferSize);
                AutoSenseModuleGetEntryResponse* response = (AutoSenseModuleGetEntryResponse*)sendBuffer;
                response->code = AutoSenseModuleResponseCode::SUCCESS;
                response->entryIndex = msg->entryIndex;
                CheckedMemcpy(response->data, record.data, record.length.GetRaw());
                SendModuleActionMessage(
                    MessageType::MODULE_ACTION_RESPONSE,
                    packet->header.sender,
                    (u8)AutoSenseModuleTriggerAndResponseMessages::GET_ENTRY,
                    packet->requestHandle,
                    sendBuffer,
                    bufferSize,
                    false
                );
            }
            else if (packet->actionType == (u8)AutoSenseModuleTriggerAndResponseMessages::CLEAR_ENTRY && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE + sizeof(AutoSenseModuleClearEntryMessage))
            {
                const AutoSenseModuleClearEntryMessage* msg = (const AutoSenseModuleClearEntryMessage*)packet->data;
                if (msg->entryIndex >= MAX_AMOUNT_OF_ENTRIES && msg->entryIndex != ALL_ENTRIES)
                {
                    SendResponse(AutoSenseModuleClearEntryResponse{ AutoSenseModuleResponseCode::ENTRY_INDEX_OUT_OF_RANGE, msg->entryIndex }, packet->header.sender, packet->requestHandle);
                    return;
                }
                if (msg->entryIndex == ALL_ENTRIES)
                {
                    ClearAllEntries(packet->header.sender, packet->requestHandle);
                }
                else
                {
                    ClearEntry(msg->entryIndex, packet->header.sender, packet->requestHandle);
                }
            }
            else if (packet->actionType == (u8)AutoSenseModuleTriggerAndResponseMessages::GET_TABLE)
            {
                const u32 amountOfBitmaskBytes = Utility::NextMultipleOf(MAX_AMOUNT_OF_ENTRIES, 8) / 8;
                const u32 sendBufferSize = sizeof(AutoSenseModuleGetTableResponse) - sizeof(AutoSenseModuleGetTableResponse::data) + amountOfBitmaskBytes;
                DYNAMIC_ARRAY(sendBuffer, sendBufferSize);
                CheckedMemset(sendBuffer, 0, sendBufferSize);
                AutoSenseModuleGetTableResponse* response = (AutoSenseModuleGetTableResponse*)sendBuffer;
                response->supportedAmount = MAX_AMOUNT_OF_ENTRIES;
                response->entryMaxSize = sizeof(AutoSenseTableEntryV0);
                for (u32 i = 0; i < MAX_AMOUNT_OF_ENTRIES; i++)
                {
                    const u32 byte = i / 8;
                    const u32 bit = i % 8;
                    if (getTableEntryV0(i) != nullptr)
                    {
                        response->data[byte] |= 1UL << bit;
                    }
                }
                SendModuleActionMessage(
                    MessageType::MODULE_ACTION_RESPONSE,
                    packet->header.sender,
                    (u8)AutoSenseModuleTriggerAndResponseMessages::GET_TABLE,
                    packet->requestHandle,
                    sendBuffer,
                    sendBufferSize,
                    false
                );
            }
            else if (packet->actionType == (u8)AutoSenseModuleTriggerAndResponseMessages::SET_EXAMPLE)
            {
                AutoSenseTableEntryV0 tableEntry = {};
                tableEntry.destNodeId = 0;
                tableEntry.moduleId = Utility::GetWrappedModuleId(ModuleId::MODBUS_MODULE);
                tableEntry.component = 0x0103; // Read Holding Register from DeviceId 1
                tableEntry.register_ = 1;
                tableEntry.length = 2;
                tableEntry.requestHandle = 0;
                tableEntry.dataType = DataTypeDescriptor::RAW;
                tableEntry.periodicReportInterval = (u8)SyncedReportInterval::NONE;
                tableEntry.reservedFlags = 0;
                tableEntry.pollingIvDs = SEC_TO_DS(10);
                tableEntry.reportingIvDs = SEC_TO_DS(10);
                tableEntry.reportFunction = AutoSenseFunction::ON_CHANGE_RATE_LIMITED;
                SetEntry(0, &tableEntry, 0, packet->header.sender, packet->requestHandle);
            }
            else if (packet->actionType == (u8)AutoSenseModuleTriggerAndResponseMessages::CLEAR_EXAMPLE)
            {
                ClearEntry(0, packet->header.sender, packet->requestHandle);
            }
            else
            {
                SIMEXCEPTION(IllegalArgumentException);
            }

        }
    }
    
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)

    //Parse Module responses
    if (packetHeader->messageType == MessageType::MODULE_ACTION_RESPONSE && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE)
    {
        ConnPacketModule const* packet = (ConnPacketModule const*)packetHeader;

        //Check if our module is meant and we should trigger an action
        if (packet->moduleId == ModuleId::AUTO_SENSE_MODULE)
        {
            if (packet->actionType == (u8)AutoSenseModuleTriggerAndResponseMessages::SET_ENTRY && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE + sizeof(AutoSenseModuleSetEntryResponse))
            {
                const AutoSenseModuleSetEntryResponse* msg = (const AutoSenseModuleSetEntryResponse*)packet->data;
                logjson("ASMOD", 
                    "{"
                        "\"type\":\"set_autosense_entry_result\","
                        "\"nodeId\":%u,"
                        "\"requestHandle\":%u,"
                        "\"module\":15,"
                        "\"code\":%u,"
                        "\"index\":%u"
                    "}" SEP,
                    packet->header.sender,
                    packet->requestHandle,
                    (u32)msg->code,
                    msg->entryIndex);
            }
            else if (packet->actionType == (u8)AutoSenseModuleTriggerAndResponseMessages::GET_ENTRY && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE + sizeof(AutoSenseModuleGetEntryResponse))
            {
                const AutoSenseModuleGetEntryResponse* msg = (const AutoSenseModuleGetEntryResponse*)packet->data;
                char base64Buffer[256];
                Logger::ConvertBufferToBase64String(msg->data, sendData->dataLength - SIZEOF_CONN_PACKET_MODULE - sizeof(AutoSenseModuleGetEntryResponse) + sizeof(AutoSenseModuleGetEntryResponse::data), base64Buffer, sizeof(base64Buffer));
                logjson("ASMOD",
                    "{"
                        "\"type\":\"autosense_entry\","
                        "\"nodeId\":%u,"
                        "\"requestHandle\":%u,"
                        "\"module\":15,"
                        "\"code\":%u,"
                        "\"index\":%u,"
                        "\"data\":\"%s\""
                    "}" SEP,
                    packet->header.sender,
                    packet->requestHandle,
                    (u32)msg->code,
                    msg->entryIndex,
                    base64Buffer);
            }
            else if (packet->actionType == (u8)AutoSenseModuleTriggerAndResponseMessages::CLEAR_ENTRY && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE + sizeof(AutoSenseModuleClearEntryResponse))
            {
                const AutoSenseModuleClearEntryResponse* msg = (const AutoSenseModuleClearEntryResponse*)packet->data;
                logjson("ASMOD", 
                    "{"
                        "\"type\":\"clear_autosense_entry_result\","
                        "\"nodeId\":%u,"
                        "\"requestHandle\":%u,"
                        "\"module\":15,"
                        "\"code\":%u,"
                        "\"index\":%u"
                    "}" SEP,
                    packet->header.sender,
                    packet->requestHandle,
                    (u32)msg->code,
                    msg->entryIndex);
            }
            else if (packet->actionType == (u8)AutoSenseModuleTriggerAndResponseMessages::GET_TABLE && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE + sizeof(AutoSenseModuleGetTableResponse))
            {
                const AutoSenseModuleGetTableResponse* msg = (const AutoSenseModuleGetTableResponse*)packet->data;
                char hexBuffer[256];
                Logger::ConvertBufferToHexString(msg->data, sendData->dataLength - SIZEOF_CONN_PACKET_MODULE - sizeof(AutoSenseModuleGetEntryResponse) + sizeof(AutoSenseModuleGetEntryResponse::data), hexBuffer, sizeof(hexBuffer));
                logjson("ASMOD",
                    "{"
                        "\"type\":\"autosense_table\","
                        "\"nodeId\":%u,"
                        "\"requestHandle\":%u,"
                        "\"module\":15,"
                        "\"supportedAmount\":%u,"
                        "\"entryMaxSize\":%u,"
                        "\"bitmask\":\"%s\""
                    "}" SEP,
                    packet->header.sender,
                    packet->requestHandle,
                    msg->supportedAmount,
                    msg->entryMaxSize,
                    hexBuffer);
            }
            else
            {
                SIMEXCEPTION(IllegalArgumentException);
            }
        }
    }
}

ModuleIdDataProviderPair::ModuleIdDataProviderPair() :
    dataProvider(nullptr)
{
    moduleId = INVALID_WRAPPED_MODULE_ID;
}

ModuleIdDataProviderPair::ModuleIdDataProviderPair(ModuleIdWrapper moduleId, AutoSenseModuleDataProvider* dataProvider) :
    moduleId(moduleId),
    dataProvider(dataProvider)
{
}
