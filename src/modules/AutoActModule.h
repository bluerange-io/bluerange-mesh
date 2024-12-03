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

constexpr u8 AUTO_ACT_MODULE_CONFIG_VERSION = 0;
#pragma pack(push)
#pragma pack(1)
//Module configuration that is saved persistently (size must be multiple of 4)
struct AutoActModuleConfiguration : ModuleConfiguration {
    //Insert more persistent config values here
};
#pragma pack(pop)

enum class AutoActFunction : u8
{
    INVALID = 0,

    // Preamble, only allowed before any of the actual transformations:
    DATA_OFFSET = 1,
    DATA_LENGTH = 2,

    // Actual transformations:
    MIN           = 3,
    MAX           = 4,
    VALUE_OFFSET  = 5,
    REVERSE_BYTES = 6,
    INT_MULT      = 7,
    FLOAT_MULT    = 8,

    LAST_VALID_VALUE = FLOAT_MULT,
    
    UNUSED_BUT_RESERVED_FLOOR = 9,
    UNUSED_BUT_RESERVED_CEIL = 10,
    UNUSED_BUT_RESERVED_ROUND = 11,
    UNUSED_BUT_RESERVED_TO_U16_LE = 12,
    UNUSED_BUT_RESERVED_TO_U32 = 13,

    NO_OP = 0xFF,
};

enum class AutoActModuleResponseCode : u8
{
    SUCCESS = 0,
    ENTRY_INDEX_OUT_OF_RANGE = 1,
    // NO_SUCH_ENTRY = 2, // Currently unused, but reserved to be aligned with PollReportModule error codes
    UNSUPPORTED_ENTRY_SIZE = 3,
    UNSUPPORTED_ENTRY_CONTENTS = 4,
    NO_SUCH_TARGET_MODULE = 5,
    INVALID_FUNCTION = 5,
    INVALID_ARGUMENTS = 6,
    MODULE_VERSION_NOT_SUPPORTED = 7,
    DATA_OFFSET_NOT_IN_PREAMBLE = 8,
    DATA_LENGTH_NOT_IN_PREAMBLE = 9,
    DATA_OFFSET_MULTIPLE = 10,
    DATA_LENGTH_MULTIPLE = 11,
    INPUT_DATA_LEGNTH_TOO_SMALL = 12,
    INPUT_TOO_SMALL = 13,
    FAILED_TO_LOAD_DATATYPE = 14,
    FAILED_TO_APPLY_TRANSFORMATION = 15,
    ILLEGAL_OUTPUT_TYPE = 16,
    FUNCTION_LIST_LENGTH_WRONG = 17,
    DATA_TYPE_MISMATCH = 18,


    RECORD_STORAGE_CODES_START = 100,
    RECORD_STORAGE_CODES_END = 150,
};

struct AutoActModuleResponse
{
    AutoActModuleResponseCode code;
    u32 outputSize;
};

#pragma pack(push)
#pragma pack(1)
struct AutoActTableEntryV0
{
    NodeId receiverNodeIdFilter;
    ModuleIdWrapper moduleIdFilter;
    u16 componentFilter;
    u16 registerFilter;
    ModuleIdWrapper targetModuleId;
    u16 targetComponent;
    u16 targetRegister;
    DataTypeDescriptor orgDataType;
    DataTypeDescriptor targetDataType;
    u8 toSense : 1;
    u8 flags : 7;
    u8 functionListLength;
    u8 functionList[1]; // Variable Length
};
constexpr u32 SIZEOF_AUTO_ACT_TABLE_ENTRY_V0 = 22;
static_assert(sizeof(AutoActTableEntryV0) == SIZEOF_AUTO_ACT_TABLE_ENTRY_V0 + 1, "Wrong sizeof constant!");

STATIC_ASSERT_SIZE(AutoActTableEntryV0, 23);
// This might be overly paranoid, but as these are persisted in
// the flash I want to avoid errors as much as possible.
static_assert(offsetof(AutoActTableEntryV0, receiverNodeIdFilter      ) ==  0, "Member at wrong location!");
static_assert(offsetof(AutoActTableEntryV0, moduleIdFilter    ) ==  2, "Member at wrong location!");
static_assert(offsetof(AutoActTableEntryV0, componentFilter   ) ==  6, "Member at wrong location!");
static_assert(offsetof(AutoActTableEntryV0, registerFilter    ) ==  8, "Member at wrong location!");
static_assert(offsetof(AutoActTableEntryV0, targetModuleId    ) == 10, "Member at wrong location!");
static_assert(offsetof(AutoActTableEntryV0, targetComponent   ) == 14, "Member at wrong location!");
static_assert(offsetof(AutoActTableEntryV0, targetRegister    ) == 16, "Member at wrong location!");
static_assert(offsetof(AutoActTableEntryV0, orgDataType       ) == 18, "Member at wrong location!");
static_assert(offsetof(AutoActTableEntryV0, targetDataType    ) == 19, "Member at wrong location!");
// Bitfields can't be used in offsetof
//static_assert(offsetof(AutoActTableEntryV0, flags             ) == 20, "Member at wrong location!");
static_assert(offsetof(AutoActTableEntryV0, functionListLength) == 21, "Member at wrong location!");
static_assert(offsetof(AutoActTableEntryV0, functionList      ) == 22, "Member at wrong location!");

typedef struct
{
    u8 moduleVersion;
    u8 entryIndex;
    u8 data[1]; // Followed by any Version of TableEntry (including future ones, currently not understood by this version of the firmware).
} AutoActModuleSetEntryMessage;
STATIC_ASSERT_SIZE(AutoActModuleSetEntryMessage, 3);
constexpr u32 SIZEOF_AUTO_ACT_MODULE_SET_ENTRY_MESSAGE_HEADER = SIZEOF_CONN_PACKET_MODULE + sizeof(AutoActModuleSetEntryMessage) - sizeof(AutoActModuleSetEntryMessage::data);


typedef struct
{
    AutoActModuleResponseCode code;
    u8 entryIndex;
} AutoActModuleSetEntryResponse;
STATIC_ASSERT_SIZE(AutoActModuleSetEntryResponse, 2);

typedef struct
{
    u8 entryIndex;
} AutoActModuleClearEntryMessage;
STATIC_ASSERT_SIZE(AutoActModuleClearEntryMessage, 1);

typedef struct
{
    AutoActModuleResponseCode code;
    u8 entryIndex;
} AutoActModuleClearEntryResponse;
STATIC_ASSERT_SIZE(AutoActModuleClearEntryResponse, 2);
#pragma pack(pop)

class AutoActModule : 
    public Module,
    public RecordStorageEventListener
{
TESTER_PUBLIC:
    static constexpr u8 ALL_ENTRIES = 0xFF;
    static constexpr u32 MAX_AMOUNT_OF_ENTRIES = 20;
    static constexpr u32 MAX_IO_SIZE = 256; //Maximum size of any transformation input or output
    
    // For the AutoActModule, both the trigger and response messages have the same value.
    enum class AutoActModuleTriggerAndResponseMessages : u8
    {
        SET_ENTRY         = 0,
        CLEAR_ENTRY       = 1,
        CLEAR_ALL_ENTRIES = 2,
    };

    struct RecordStorageUserData
    {
        NodeId sender;
        u8 entryIndex;
        u8 requestHandle;
    };

    void RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength) override final;

    void SendResponse(const AutoActModuleSetEntryResponse&   response, NodeId id, u8 requestHandle) const;
    void SendResponse(const AutoActModuleClearEntryResponse& response, NodeId id, u8 requestHandle) const;

    static AutoActModuleResponseCode TranslateRecordStorageCode(const RecordStorageResultCode& code);
public:
    //Declare the configuration used for this module
    DECLARE_CONFIG_AND_PACKED_STRUCT(AutoActModuleConfiguration);

    AutoActModule();

    void ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength) override;

    void ResetToDefaultConfiguration() override;

    void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader) override;

    const AutoActTableEntryV0* getTableEntryV0(u8 entryIndex);

    void SetEntry(u8 entryIndex, const AutoActTableEntryV0* tableEntry, MessageLength tableEntryBufferSize, u8 moduleVersion, NodeId sender, u8 requestHandle);
    void ClearEntry(u8 entryIndex, NodeId sender, u8 requestHandle);
    void ClearAllEntries(NodeId sender, u8 requestHandle);

    #ifdef TERMINAL_ENABLED
    TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override;
    #endif
};
