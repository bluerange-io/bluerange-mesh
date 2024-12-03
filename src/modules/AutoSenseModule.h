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

#pragma once

#include <Module.h>
#include <MultiScheduler.h>
#include <BitMask.h>
#include <SlotStorage.h>

class AutoSenseModuleDataConsumer
{
public:
    virtual bool ConsumeData(ModuleId moduleId, u16 component, u16 register_, u8 length, const u8* data) = 0;
    virtual bool ConsumeData(ModuleIdWrapper moduleId, u16 component, u16 register_, u8 length, const u8* data) = 0;
};

class AutoSenseModuleDataProvider
{
public:
    virtual bool AcceptsRegister(u16 component, u16 register_, u8 length) { return true; };
    virtual void RequestData(u16 component, u16 register_, u8 length, AutoSenseModuleDataConsumer* provideTo) = 0;
};

struct ModuleIdDataProviderPair
{
    ModuleIdWrapper moduleId;
    AutoSenseModuleDataProvider* dataProvider;

    ModuleIdDataProviderPair();
    ModuleIdDataProviderPair(ModuleIdWrapper moduleId, AutoSenseModuleDataProvider* dataProvider);
};

#pragma pack(push)
#pragma pack(1)
enum class AutoSenseFunction : u8
{
    LAST                           = 0,
    ON_CHANGE_RATE_LIMITED         = 1,
    ON_CHANGE_WITH_PERIODIC_REPORT = 2,

    // These are already reserved so that multiple devs can
    // implement them while avoiding some merge conflicts.
    UNUSED_BUT_RESERVED_UNBUFFERED = 3,
    UNUSED_BUT_RESERVED_FIRST = 4,
    UNUSED_BUT_RESERVED_SUM = 5,
    UNUSED_BUT_RESERVED_COUNT_AND_SUM = 6,
    UNUSED_BUT_RESERVED_MEDIAN = 7,
    UNUSED_BUT_RESERVED_AVERAGE = 8,
    UNUSED_BUT_RESERVED_MIN = 9,
    UNUSED_BUT_RESERVED_MAX = 10,
    UNUSED_BUT_RESERVED_THRESHOLD = 11,
    UNUSED_BUT_RESERVED_RATE_LIMIT_THRESHOLD_PER_MIN = 12,
};

// Known Limitation: When an absolut event should trigger at time X, but the time is
//                   resynced at X-1 to X+1, then the event will never trigger.
enum class SyncedReportInterval : u8
{
    // CAREFUL! Must only be 3 bit
    NONE        = 0b000,
    SECOND      = 0b001,
    TEN_SECONDS = 0b010,
    MINUTE      = 0b011,
    TEN_MINUTES = 0b100,
    HALF_HOUR   = 0b101,
    HOUR        = 0b110,
    DAILY       = 0b111,
};

struct AutoSenseTableEntryV0
{
    NodeId destNodeId;
    ModuleIdWrapper moduleId;
    u16 component;
    u16 register_; // _ because register is a keyword
    u8 length;
    u8 requestHandle;
    DataTypeDescriptor dataType; //JSTODO no effect
    u8 periodicReportInterval : 3; // Actual Type: SyncedReportInterval
    u8 reservedFlags : 5;
    u16 pollingIvDs;
    u16 reportingIvDs;
    AutoSenseFunction reportFunction; // CAREFUL! V0 only supports a subset of functions
    //u8 functionParams[]; // Currently unused, thus not implemented.
};
STATIC_ASSERT_SIZE(AutoSenseTableEntryV0, 19);
// This might be overly paranoid, but as these are persisted in
// the flash I want to avoid errors as much as possible.
static_assert(offsetof(AutoSenseTableEntryV0, destNodeId    ) ==  0, "Member at wrong location!");
static_assert(offsetof(AutoSenseTableEntryV0, moduleId      ) ==  2, "Member at wrong location!");
static_assert(offsetof(AutoSenseTableEntryV0, component     ) ==  6, "Member at wrong location!");
static_assert(offsetof(AutoSenseTableEntryV0, register_     ) ==  8, "Member at wrong location!");
static_assert(offsetof(AutoSenseTableEntryV0, length        ) == 10, "Member at wrong location!");
static_assert(offsetof(AutoSenseTableEntryV0, requestHandle ) == 11, "Member at wrong location!");
static_assert(offsetof(AutoSenseTableEntryV0, dataType      ) == 12, "Member at wrong location!");
static_assert(offsetof(AutoSenseTableEntryV0, pollingIvDs   ) == 14, "Member at wrong location!");
static_assert(offsetof(AutoSenseTableEntryV0, reportingIvDs ) == 16, "Member at wrong location!");
static_assert(offsetof(AutoSenseTableEntryV0, reportFunction) == 18, "Member at wrong location!");
#pragma pack(pop)

enum class AutoSenseModuleResponseCode : u8
{
    SUCCESS                            = 0,
    ENTRY_INDEX_OUT_OF_RANGE           = 1,
    NO_SUCH_ENTRY                      = 2,
    UNSUPPORTED_ENTRY_SIZE             = 3,
    UNSUPPORTED_ENTRY_CONTENTS         = 4,
    // This is not called "NO_SUCH_MODULE_ID" on purpose, because
    // such a moduleId might exist, and even a module with this
    // moduleId might be created. However, it may not have registered
    // itself properly as a data provider to the AutoSenseModule.
    NO_SUCH_DATA_PROVIDER              = 5,
    FAILED_TO_CREATE_VALUE_CACHE_ENTRY = 6,
    MODULE_VERSION_NOT_SUPPORTED       = 7,

    RECORD_STORAGE_CODES_START = 100,
    RECORD_STORAGE_CODES_END   = 150,
};

#pragma pack(push)
#pragma pack(1)
typedef struct
{
    u8 moduleVersion;
    u8 entryIndex;
    u8 data[1]; // Followed by any Version of TableEntry (including future ones, currently not understood by this version of the firmware).
} AutoSenseModuleSetEntryMessage;
STATIC_ASSERT_SIZE(AutoSenseModuleSetEntryMessage, 3);
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
typedef struct
{
    AutoSenseModuleResponseCode code;
    u8 entryIndex;
} AutoSenseModuleSetEntryResponse;
STATIC_ASSERT_SIZE(AutoSenseModuleSetEntryResponse, 2);
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
typedef struct
{
    u8 entryIndex;
} AutoSenseModuleGetEntryMessage;
STATIC_ASSERT_SIZE(AutoSenseModuleGetEntryMessage, 1);
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
typedef struct
{
    AutoSenseModuleResponseCode code;
    u8 entryIndex;
    u8 data[1]; // Followed by any Version of TableEntry (including future ones, currently not understood by this version of the firmware).
} AutoSenseModuleGetEntryResponse;
STATIC_ASSERT_SIZE(AutoSenseModuleGetEntryResponse, 3);
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
typedef struct
{
    u8 entryIndex;
} AutoSenseModuleClearEntryMessage;
STATIC_ASSERT_SIZE(AutoSenseModuleClearEntryMessage, 1);
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
typedef struct
{
    AutoSenseModuleResponseCode code;
    u8 entryIndex;
} AutoSenseModuleClearEntryResponse;
STATIC_ASSERT_SIZE(AutoSenseModuleClearEntryResponse, 2);
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
//typedef struct
//{
//    EMPTY MESSAGE
//} AutoSenseModuleGetTableMessage;
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
typedef struct
{
    u8 supportedAmount;
    u8 entryMaxSize;
    u8 data[1]; // Followed by a bitmask, describing which entry is active and which isn't.
} AutoSenseModuleGetTableResponse;
STATIC_ASSERT_SIZE(AutoSenseModuleGetTableResponse, 3);
#pragma pack(pop)


constexpr u8 AUTO_SENSE_MODULE_CONFIG_VERSION = 0;
#pragma pack(push)
#pragma pack(1)
//Module configuration that is saved persistently (size must be multiple of 4)
struct AutoSenseModuleConfiguration : ModuleConfiguration {
    //Insert more persistent config values here
};
#pragma pack(pop)

class AutoSenseModule : 
    public Module,
    public AutoSenseModuleDataConsumer,
    public RecordStorageEventListener,
    public TimeSyncedListener
{
TESTER_PUBLIC:
    static constexpr u8 ALL_ENTRIES = 0xFF;
    static constexpr u32 MAX_AMOUNT_OF_ENTRIES = 20;
    static constexpr size_t MAX_AMOUNT_DATA_PROVIDERS = 4;

#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    std::array<ModuleIdDataProviderPair, MAX_AMOUNT_DATA_PROVIDERS> dataProviders = {};
    MultiScheduler<u8, MAX_AMOUNT_OF_ENTRIES> pollSchedule;
    MultiScheduler<u8, MAX_AMOUNT_OF_ENTRIES> reportSchedule;
    BitMask<MAX_AMOUNT_OF_ENTRIES> anyValueRecorded = {};
    BitMask<MAX_AMOUNT_OF_ENTRIES> readyForSending = {};
    SlotStorage<MAX_AMOUNT_OF_ENTRIES, 512> valueCache;
#endif

    const AutoSenseTableEntryV0* getTableEntryV0(u8 entryIndex);
    static constexpr u8 INVALID_TABLE_INDEX = 0xFF;

    struct RecordStorageUserData
    {
        NodeId sender;
        u8 entryIndex;
        u8 requestHandle;
    };

    void RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength) override final;

    void SendResponse(const AutoSenseModuleSetEntryResponse&   response, NodeId id, u8 requestHandle) const;
    void SendResponse(const AutoSenseModuleGetEntryResponse&   response, NodeId id, u8 requestHandle) const;
    void SendResponse(const AutoSenseModuleClearEntryResponse& response, NodeId id, u8 requestHandle) const;

    static AutoSenseModuleResponseCode TranslateRecordStorageCode(const RecordStorageResultCode& code);

    // Sends the data attached to the given tableEntry
    void SendEntry(u8 entryIndex, const AutoSenseTableEntryV0* tableEntry);

    void AddScheduleEvents(u32 entryIndex, const AutoSenseTableEntryV0* table);

public:

    // For the AutoSenseModule, both the trigger and response messages have the same value.
    enum class AutoSenseModuleTriggerAndResponseMessages : u8
    {
        SET_ENTRY         = 0,
        GET_ENTRY         = 1,
        CLEAR_ENTRY       = 2,
        CLEAR_ALL_ENTRIES = 3,
        GET_TABLE         = 4,
        SET_EXAMPLE       = 5,
        CLEAR_EXAMPLE     = 6,
    };

    //####### Module messages (these need to be packed)
#pragma pack(push)
#pragma pack(1)

    static constexpr int SIZEOF_AUTO_SENSE_MODULE_COMMAND_ONE_MESSAGE = 1;
    typedef struct
    {
        //Insert values here
        u8 exampleValue;

    } AutoSenseModuleCommandOneMessage;
    STATIC_ASSERT_SIZE(AutoSenseModuleCommandOneMessage, SIZEOF_AUTO_SENSE_MODULE_COMMAND_ONE_MESSAGE);
    
#pragma pack(pop)
    //####### Module messages end

    //Declare the configuration used for this module
    DECLARE_CONFIG_AND_PACKED_STRUCT(AutoSenseModuleConfiguration);

    AutoSenseModule();

    void ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength) override;

    void ResetToDefaultConfiguration() override;

    void TimerEventHandler(u16 passedTimeDs) override;

    void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader) override;

    void SetEntry(u8 entryIndex, const AutoSenseTableEntryV0* tableEntry, u8 moduleVersion, NodeId sender, u8 requestHandle);
    void ClearEntry(u8 entryIndex, NodeId sender, u8 requestHandle);
    void ClearAllEntries(NodeId sender, u8 requestHandle);

    #ifdef TERMINAL_ENABLED
    TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override;
    #endif

    void RegisterDataProvider(ModuleId moduleId, AutoSenseModuleDataProvider* dataProvider);
    void RegisterDataProvider(ModuleIdWrapper moduleId, AutoSenseModuleDataProvider* dataProvider);

    bool ConsumeData(ModuleId moduleId, u16 component, u16 register_, u8 length, const u8* data) override;
    bool ConsumeData(ModuleIdWrapper moduleId, u16 component, u16 register_, u8 length, const u8* data) override;

    // TimeSyncedListener
    virtual void TimeSyncedHandler() override;
};
