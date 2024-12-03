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

#include <AutoActModule.h>

#include <GlobalState.h>
#include <Logger.h>
#include <Utility.h>
#include <Node.h>
#include <IoModule.h>

// If the function is valid for numeric calculations.
static bool IsValidNumericFunc(AutoActFunction func)
{
    if (func == AutoActFunction::INVALID) return false;
    if (func == AutoActFunction::DATA_LENGTH) return false;
    if (func == AutoActFunction::REVERSE_BYTES) return false;
    if (func <= AutoActFunction::LAST_VALID_VALUE) return true;
    if (func == AutoActFunction::NO_OP) return true;

    return false;
}

// If the function is valid for string calculations.
static bool IsValidStringFunc(AutoActFunction func)
{
    if (func == AutoActFunction::DATA_OFFSET) return true;
    if (func == AutoActFunction::DATA_LENGTH) return true;
    if (func == AutoActFunction::REVERSE_BYTES) return true;
    if (func == AutoActFunction::NO_OP) return true;
    return false;
}

// Returns the size in bytes that are followed after the Function token within the transformation list.
static u8 GetFunctionArgumentSize(AutoActFunction func)
{
    switch (func)
    {
    case AutoActFunction::DATA_OFFSET: return 1;
    case AutoActFunction::DATA_LENGTH: return 1;
    case AutoActFunction::MIN: return 4;
    case AutoActFunction::MAX: return 4;
    case AutoActFunction::VALUE_OFFSET: return 4;
    case AutoActFunction::INT_MULT: return 4;
    case AutoActFunction::FLOAT_MULT: return 4;
    default: return 0;
    }
}

// Applies a single numeric transformation from a transformation pipeline.
static bool ApplyNumericTransformation(float& value, AutoActFunction func, const u8* arguments)
{
    if (func == AutoActFunction::MIN)
    {
        i32 min = Utility::ToAlignedI32(arguments);
        if (value < min) value = min;
    }
    else if (func == AutoActFunction::MAX)
    {
        i32 max = Utility::ToAlignedI32(arguments);
        if (value > max) value = max;
    }
    else if (func == AutoActFunction::VALUE_OFFSET)
    {
        i32 offset = Utility::ToAlignedI32(arguments);
        value += offset;
    }
    else if (func == AutoActFunction::REVERSE_BYTES)
    {
        SIMEXCEPTION(NotImplementedException);
        return false;
    }
    else if (func == AutoActFunction::INT_MULT)
    {
        i32 mult = Utility::ToAlignedI32(arguments);
        value *= mult;
    }
    else if (func == AutoActFunction::FLOAT_MULT)
    {
        float mult = Utility::ToAlignedFloat(arguments);
        value *= mult;
    }
    else if (func == AutoActFunction::NO_OP)
    {
        // Do nothing.
    }
    else
    {
        return false;
    }

    return true;
}

// Returns the size of the type described by the type descriptor.
static u32 GetSize(DataTypeDescriptor dataType)
{
    dataType = Utility::ToLittleEndianDescriptor(dataType);
    switch (dataType)
    {
    case DataTypeDescriptor::RAW: SIMEXCEPTION(IllegalArgumentException); return 1;
    case DataTypeDescriptor::U8_LE: return 1;
    case DataTypeDescriptor::U16_LE: return 2;
    case DataTypeDescriptor::U32_LE: return 4;
    case DataTypeDescriptor::FLOAT32_LE: return 4;
    default: SIMEXCEPTION(IllegalArgumentException); return 0;
    }
}

// Loads the initial value from "any type" into a float for further calculations.
static bool LoadInitialValue(float& value, const u8* input, DataTypeDescriptor dataType)
{
    u8 inputCopy[4] = {};
    CheckedMemcpy(inputCopy, input, GetSize(dataType));
    if (Utility::GetEndianness(dataType) == Endianness::BIG)
    {
        Utility::SwapBytes(inputCopy, GetSize(dataType));
        dataType = Utility::ToLittleEndianDescriptor(dataType);
    }

    switch (dataType)
    {
    case DataTypeDescriptor::RAW:        SIMEXCEPTION(IllegalArgumentException); return false;
    case DataTypeDescriptor::U8_LE:      value = *(inputCopy); return true;
    case DataTypeDescriptor::U16_LE:     value = Utility::ToAlignedU16  (inputCopy); return true;
    case DataTypeDescriptor::U32_LE:     value = Utility::ToAlignedU32  (inputCopy); return true;
    case DataTypeDescriptor::FLOAT32_LE: value = Utility::ToAlignedFloat(inputCopy); return true;
    default: SIMEXCEPTION(IllegalArgumentException); return false;
    }
}

// Performs all transformations from a transformation pipeline, one after the other. Only for numeric pipelines.
static AutoActModuleResponse TransformNumeric(const u8* input, u32 inputSize, DataTypeDescriptor inputDataType, u8* output, DataTypeDescriptor outputDataType, const u8* transformations, u32 transformationsLength)
{
    const u8* readPtr = input;
    const u8* endTransformations = transformations + transformationsLength;

    bool transformationStarted = false;
    float interimResult = 0;

    bool dataOffsetFound = false;

    while (transformations < endTransformations)
    {
        AutoActFunction func = (AutoActFunction)*transformations;
        if (!IsValidNumericFunc(func))
        {
            SIMEXCEPTION(IllegalStateException);
            return { AutoActModuleResponseCode::INVALID_FUNCTION, 0 };
        }
        transformations++;
        const u8* arguments = transformations;
        transformations += GetFunctionArgumentSize(func);
        if (transformations > endTransformations)
        {
            // There wasn't enough data in the transformations list to fill out all arguments.
            SIMEXCEPTION(IllegalStateException);
            return { AutoActModuleResponseCode::INVALID_ARGUMENTS, 0 };
        }
        // Handle Preamble
        if (func == AutoActFunction::DATA_OFFSET)
        {
            if (transformationStarted)
            {
                SIMEXCEPTION(PreambleNotAtStartException);
                return { AutoActModuleResponseCode::DATA_OFFSET_NOT_IN_PREAMBLE, 0 };
            }
            if (dataOffsetFound)
            {
                SIMEXCEPTION(DoubleDataOffsetException);
                return { AutoActModuleResponseCode::DATA_OFFSET_MULTIPLE, 0 };
            }
            dataOffsetFound = true;
            const u8 offset = *arguments;
            readPtr += offset;
            if (inputSize > offset)
            {
                inputSize -= offset;
            }
            else
            {
                SIMEXCEPTION(IllegalStateException);
                return { AutoActModuleResponseCode::INPUT_TOO_SMALL, 0 };
            }
        }
        else
        {
            if (!transformationStarted)
            {
                transformationStarted = true;
                if (GetSize(inputDataType) > inputSize)
                {
                    SIMEXCEPTION(IllegalStateException);
                    return { AutoActModuleResponseCode::INPUT_TOO_SMALL, 0 };
                }

                if (!LoadInitialValue(interimResult, readPtr, inputDataType))
                {
                    SIMEXCEPTION(IllegalStateException);
                    return { AutoActModuleResponseCode::FAILED_TO_LOAD_DATATYPE, 0 };
                }
            }
            if (!ApplyNumericTransformation(interimResult, func, arguments))
            {
                // A transformation failed.
                SIMEXCEPTION(TransformationFailedException);
                return { AutoActModuleResponseCode::FAILED_TO_APPLY_TRANSFORMATION, 0 };
            }
        }
    }

    if (!transformationStarted)
    {
        if (GetSize(inputDataType) > inputSize)
        {
            SIMEXCEPTION(IllegalStateException);
            return { AutoActModuleResponseCode::INPUT_TOO_SMALL, 0 };
        }

        if (!LoadInitialValue(interimResult, readPtr, inputDataType))
        {
            SIMEXCEPTION(IllegalStateException);
            return { AutoActModuleResponseCode::FAILED_TO_LOAD_DATATYPE, 0 };
        }
    }

    switch (Utility::ToLittleEndianDescriptor(outputDataType))
    {
    case DataTypeDescriptor::U8_LE:      { u8    value = interimResult; CheckedMemcpy(output, &value, sizeof(value)); break; }
    case DataTypeDescriptor::U16_LE:     { u16   value = interimResult; CheckedMemcpy(output, &value, sizeof(value)); break; }
    case DataTypeDescriptor::U32_LE:     { u32   value = interimResult; CheckedMemcpy(output, &value, sizeof(value)); break; }
    case DataTypeDescriptor::FLOAT32_LE: { float value = interimResult; CheckedMemcpy(output, &value, sizeof(value)); break; }
    default: SIMEXCEPTION(IllegalArgumentException); return { AutoActModuleResponseCode::ILLEGAL_OUTPUT_TYPE, 0 };
    }

    if (Utility::GetEndianness(outputDataType) == Endianness::BIG)
    {
        Utility::SwapBytes(output, GetSize(outputDataType));
    }

    return { AutoActModuleResponseCode::SUCCESS, GetSize(outputDataType) };
}

// Performs all transformations from a transformation pipeline, one after the other. Only for string pipelines.
static AutoActModuleResponse TransformString(const u8* input, u32 inputSize, u8* output, const u8* transformations, u32 transformationsLength)
{
    const u8* readPtr = input;
    const u8* endTransformations = transformations + transformationsLength;

    bool transformationStarted = false;

    bool dataOffsetFound = false;
    bool dataLengthFound = false;
    u8 dataLength = 0;
    bool reverseBytes = false;

    while (transformations < endTransformations)
    {
        AutoActFunction func = (AutoActFunction)*transformations;
        if (!IsValidStringFunc(func))
        {
            SIMEXCEPTION(IllegalStateException);
            return { AutoActModuleResponseCode::INVALID_FUNCTION, 0 };
        }
        transformations++;
        const u8* arguments = transformations;
        transformations += GetFunctionArgumentSize(func);
        if (transformations > endTransformations)
        {
            // There wasn't enough data in the transformations list to fill out all arguments.
            SIMEXCEPTION(IllegalStateException);
            return { AutoActModuleResponseCode::INVALID_ARGUMENTS, 0 };
        }
        // Handle Preamble
        if (func == AutoActFunction::DATA_OFFSET)
        {
            if (transformationStarted)
            {
                SIMEXCEPTION(PreambleNotAtStartException);
                return { AutoActModuleResponseCode::DATA_OFFSET_NOT_IN_PREAMBLE, 0 };
            }
            if (dataOffsetFound)
            {
                SIMEXCEPTION(DoubleDataOffsetException);
                return { AutoActModuleResponseCode::DATA_OFFSET_MULTIPLE, 0 };
            }
            dataOffsetFound = true;
            const u8 offset = *arguments;
            readPtr += offset;
            if (inputSize > offset)
            {
                inputSize -= offset;
            }
            else
            {
                SIMEXCEPTION(IllegalStateException);
                return { AutoActModuleResponseCode::INPUT_TOO_SMALL, 0 };
            }
        }
        else if (func == AutoActFunction::DATA_LENGTH)
        {
            if (transformationStarted)
            {
                SIMEXCEPTION(PreambleNotAtStartException);
                return { AutoActModuleResponseCode::DATA_LENGTH_NOT_IN_PREAMBLE, 0 };
            }
            if (dataLengthFound)
            {
                SIMEXCEPTION(IllegalStateException);
                return { AutoActModuleResponseCode::DATA_LENGTH_MULTIPLE, 0 };
            }
            dataLengthFound = true;
            dataLength = *arguments;
            if (dataLength > inputSize)
            {
                SIMEXCEPTION(IllegalStateException);
                return { AutoActModuleResponseCode::INPUT_TOO_SMALL, 0 };
            }
        }
        else
        {
            if (!transformationStarted)
            {
                transformationStarted = true;
                if (dataLength == 0)
                {
                    dataLength = inputSize;
                }
            }
            if (func == AutoActFunction::REVERSE_BYTES)
            {
                reverseBytes = !reverseBytes;
            }
            else
            {
                // A transformation failed.
                SIMEXCEPTION(TransformationFailedException);
                return { AutoActModuleResponseCode::FAILED_TO_APPLY_TRANSFORMATION, 0 };
            }
        }
    }

    if (!transformationStarted)
    {
        if (dataLength == 0)
        {
            dataLength = inputSize;
        }
    }
    CheckedMemcpy(output, readPtr, dataLength);
    if (reverseBytes)
    {
        Utility::SwapBytes(output, dataLength);
    }

    return { AutoActModuleResponseCode::SUCCESS, dataLength };
}

// Performs all transformations from a transformation pipeline, one after the other.
static AutoActModuleResponse Transform(const u8* input, u32 inputSize, DataTypeDescriptor inputDataType, u8* output, DataTypeDescriptor outputDataType, const u8* transformations, u32 transformationsLength)
{
    if ((inputDataType == DataTypeDescriptor::RAW && outputDataType != DataTypeDescriptor::RAW)
        || (inputDataType != DataTypeDescriptor::RAW && outputDataType == DataTypeDescriptor::RAW))
    {
        // Either both types are numeric, or both are strings. We currently do not support conversion between strings and numerics.
        SIMEXCEPTION(IllegalStateException);
        return { AutoActModuleResponseCode::DATA_TYPE_MISMATCH, 0 };
    }

    if (inputDataType == DataTypeDescriptor::RAW)
    {
        return TransformString(input, inputSize, output, transformations, transformationsLength);
    }
    else
    {
        return TransformNumeric(input, inputSize, inputDataType, output, outputDataType, transformations, transformationsLength);
    }
}

void AutoActModule::RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength)
{
    if (userDataLength != sizeof(RecordStorageUserData))
    {
        SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE Unclear how to get in this state.
    }
    RecordStorageUserData* ud = (RecordStorageUserData*)userData;
    if (userType == (u32)AutoActModuleTriggerAndResponseMessages::SET_ENTRY)
    {
        if (resultCode != RecordStorageResultCode::SUCCESS)
        {
            SendResponse(AutoActModuleSetEntryResponse{ TranslateRecordStorageCode(resultCode), ud->entryIndex }, ud->sender, ud->requestHandle); //LCOV_EXCL_LINE Unclear how to get in this state.
        }
        else
        {
            SendResponse(AutoActModuleSetEntryResponse{ AutoActModuleResponseCode::SUCCESS, ud->entryIndex }, ud->sender, ud->requestHandle);
        }
    }
    else if (userType == (u32)AutoActModuleTriggerAndResponseMessages::CLEAR_ENTRY
          || userType == (u32)AutoActModuleTriggerAndResponseMessages::CLEAR_ALL_ENTRIES)
    {
        if (resultCode != RecordStorageResultCode::SUCCESS)
        {
            u8 reportedEntry = ud->entryIndex;
            if (userType == (u32)AutoActModuleTriggerAndResponseMessages::CLEAR_ALL_ENTRIES) reportedEntry = ALL_ENTRIES;
            SendResponse(AutoActModuleClearEntryResponse{ TranslateRecordStorageCode(resultCode), reportedEntry }, ud->sender, ud->requestHandle); //LCOV_EXCL_LINE Unclear how to get in this state.
        }
        else
        {
            if (userType == (u32)AutoActModuleTriggerAndResponseMessages::CLEAR_ENTRY)
            {
                SendResponse(AutoActModuleClearEntryResponse{ AutoActModuleResponseCode::SUCCESS, ud->entryIndex }, ud->sender, ud->requestHandle);
            }
            else if (userType == (u32)AutoActModuleTriggerAndResponseMessages::CLEAR_ALL_ENTRIES)
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
}

// Sends a AutoActModuleSetEntryResponse to the nodeId
void AutoActModule::SendResponse(const AutoActModuleSetEntryResponse& response, NodeId id, u8 requestHandle) const
{
    SendModuleActionMessage(
        MessageType::MODULE_ACTION_RESPONSE,
        id,
        (u8)AutoActModuleTriggerAndResponseMessages::SET_ENTRY,
        requestHandle,
        (const u8*)&response,
        sizeof(response),
        false
    );
}

// Sends a AutoActModuleClearEntryResponse to the nodeId
void AutoActModule::SendResponse(const AutoActModuleClearEntryResponse& response, NodeId id, u8 requestHandle) const
{
    SendModuleActionMessage(
        MessageType::MODULE_ACTION_RESPONSE,
        id,
        (u8)AutoActModuleTriggerAndResponseMessages::CLEAR_ENTRY,
        requestHandle,
        (const u8*)&response,
        sizeof(response),
        false
    );
}

// Translates a RecordStorageResultCode to a AutoActModuleResponseCode. All RecordStorageResultCodes are embedded inside AutoActModuleResponseCodes.
AutoActModuleResponseCode AutoActModule::TranslateRecordStorageCode(const RecordStorageResultCode& code)                                       //LCOV_EXCL_LINE Unclear how to get in this state.
{                                                                                                                                                                    //LCOV_EXCL_LINE Unclear how to get in this state.
    if (code == RecordStorageResultCode::SUCCESS) return AutoActModuleResponseCode::SUCCESS;                                                              //LCOV_EXCL_LINE Unclear how to get in this state.
                                                                                                                                                                     //LCOV_EXCL_LINE Unclear how to get in this state.
    constexpr u8 range = (u8)AutoActModuleResponseCode::RECORD_STORAGE_CODES_END - (u8)AutoActModuleResponseCode::RECORD_STORAGE_CODES_START;  //LCOV_EXCL_LINE Unclear how to get in this state.
                                                                                                                                                                     //LCOV_EXCL_LINE Unclear how to get in this state.
    u8 u8Code = (u8)code;                                                                                                                                            //LCOV_EXCL_LINE Unclear how to get in this state.
    if (u8Code > range)                                                                                                                                              //LCOV_EXCL_LINE Unclear how to get in this state.
    {                                                                                                                                                                //LCOV_EXCL_LINE Unclear how to get in this state.
        logt("WARNING", "Got Record Storage Result Code outside the range of translatable codes!");                                                                  //LCOV_EXCL_LINE Unclear how to get in this state.
        u8Code = range;                                                                                                                                              //LCOV_EXCL_LINE Unclear how to get in this state.
    }                                                                                                                                                                //LCOV_EXCL_LINE Unclear how to get in this state.
                                                                                                                                                                     //LCOV_EXCL_LINE Unclear how to get in this state.
    return AutoActModuleResponseCode(u8Code + (u8)AutoActModuleResponseCode::RECORD_STORAGE_CODES_START);                                      //LCOV_EXCL_LINE Unclear how to get in this state.
}

AutoActModule::AutoActModule()
    : Module(ModuleId::AUTO_ACT_MODULE, "autoact")
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    GS->logger.EnableTag("AAMOD");

    //Save configuration to base class variables
    //sizeof configuration must be a multiple of 4 bytes
    configurationPointer = &configuration;
    configurationLength = sizeof(AutoActModuleConfiguration);

    //Set defaults
    ResetToDefaultConfiguration();
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
}

void AutoActModule::ResetToDefaultConfiguration()
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    //Set default configuration values
    configuration.moduleId = ModuleId::AUTO_ACT_MODULE;
    configuration.moduleActive = true;
    configuration.moduleVersion = AUTO_ACT_MODULE_CONFIG_VERSION;

    //Set additional config values...

    //This line allows us to have different configurations of this module depending on the featureset
    SET_FEATURESET_CONFIGURATION(&configuration, this);
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
}

void AutoActModule::ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength)
{
#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    AutoActModuleConfiguration* newConfig = (AutoActModuleConfiguration*)migratableConfig;

    //Version migration can be added here, e.g. if module has version 2 and config is version 1
    if(newConfig != nullptr && newConfig->moduleVersion == 1){/* ... */};

    //Do additional initialization upon loading the config
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)

}

#ifdef TERMINAL_ENABLED
TerminalCommandHandlerReturnType AutoActModule::TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize)
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

            if (TERMARGS(3, "set_autoact_entry"))
            {
                if (commandArgsSize < 7) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
                const u8 moduleVersion = Utility::StringToU8(commandArgs[4], &didError);
                const u8 entryIndex = Utility::StringToU8(commandArgs[5], &didError);
                u8 entryBuffer[250];
                const u32 entrySize = Logger::ParseEncodedStringToBuffer(commandArgs[6], entryBuffer, sizeof(entryBuffer), &didError);
                const u8 requestHandle = commandArgsSize >= 8 ? Utility::StringToU8(commandArgs[7], &didError) : 0;
                if (didError || entrySize == 0) return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                
                u32 sendBufferSize = sizeof(AutoActModuleSetEntryMessage) - sizeof(AutoActModuleSetEntryMessage::data) + entrySize;
                DYNAMIC_ARRAY(sendBuffer, sendBufferSize);
                CheckedMemset(sendBuffer, 0, sendBufferSize);
                AutoActModuleSetEntryMessage* msg = (AutoActModuleSetEntryMessage*)(sendBuffer);
                msg->moduleVersion = moduleVersion;
                msg->entryIndex = entryIndex;
                CheckedMemcpy(msg->data, entryBuffer, entrySize);
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)AutoActModuleTriggerAndResponseMessages::SET_ENTRY,
                    requestHandle,
                    sendBuffer,
                    sendBufferSize,
                    false
                );
                
                return TerminalCommandHandlerReturnType::SUCCESS;
            }
            else if (TERMARGS(3, "clear_autoact_entry"))
            {
                if (commandArgsSize < 5) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
                AutoActModuleClearEntryMessage msg = {};
                msg.entryIndex = Utility::StringToU8(commandArgs[4], &didError);
                const u8 requestHandle = commandArgsSize >= 6 ? Utility::StringToU8(commandArgs[5], &didError) : 0;
                if (didError) return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                
                SendModuleActionMessage(
                    MessageType::MODULE_TRIGGER_ACTION,
                    destinationNode,
                    (u8)AutoActModuleTriggerAndResponseMessages::CLEAR_ENTRY,
                    requestHandle,
                    (u8*)&msg,
                    sizeof(msg),
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

void AutoActModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader)
{
    //Must call superclass for handling
    Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    //Parse trigger actions
    if (packetHeader->messageType == MessageType::MODULE_TRIGGER_ACTION && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE)
    {
        ConnPacketModule const* packet = (ConnPacketModule const*)packetHeader;

        //Check if our module is meant and we should trigger an action
        if (packet->moduleId == ModuleId::AUTO_ACT_MODULE)
        {
            if (packet->actionType == (u8)AutoActModuleTriggerAndResponseMessages::SET_ENTRY && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE + sizeof(AutoActModuleSetEntryMessage))
            {
                const AutoActModuleSetEntryMessage* msg = (const AutoActModuleSetEntryMessage*)packet->data;
                u32 entrySize = sendData->dataLength.GetRaw() - SIZEOF_AUTO_ACT_MODULE_SET_ENTRY_MESSAGE_HEADER;
                if (entrySize < sizeof(AutoActTableEntryV0))
                {
                    SendResponse(AutoActModuleSetEntryResponse{ AutoActModuleResponseCode::UNSUPPORTED_ENTRY_SIZE, msg->entryIndex }, packet->header.sender, packet->requestHandle);
                    return;
                }
                const AutoActTableEntryV0* tableEntry = (const AutoActTableEntryV0*)msg->data;
                SetEntry(msg->entryIndex, tableEntry, entrySize, msg->moduleVersion, packet->header.sender, packet->requestHandle);
            }
            else if (packet->actionType == (u8)AutoActModuleTriggerAndResponseMessages::CLEAR_ENTRY && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE + sizeof(AutoActModuleClearEntryMessage))
            {
                const AutoActModuleClearEntryMessage* msg = (const AutoActModuleClearEntryMessage*)packet->data;
                if (msg->entryIndex == ALL_ENTRIES)
                {
                    ClearAllEntries(packet->header.sender, packet->requestHandle);
                }
                else if (msg->entryIndex >= MAX_AMOUNT_OF_ENTRIES)
                {
                    SendResponse(AutoActModuleClearEntryResponse{ AutoActModuleResponseCode::ENTRY_INDEX_OUT_OF_RANGE, msg->entryIndex }, packet->header.sender, packet->requestHandle);
                }
                else
                {
                    ClearEntry(msg->entryIndex, packet->header.sender, packet->requestHandle);
                }
            }
            else
            {
                SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE Unclear how to get in this state.
            }
        }
    }
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)

    //Parse Module responses
    if (packetHeader->messageType == MessageType::MODULE_ACTION_RESPONSE && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE)
    {
        ConnPacketModule const* packet = (ConnPacketModule const*)packetHeader;

        //Check if our module is meant and we should trigger an action
        if (packet->moduleId == ModuleId::AUTO_ACT_MODULE)
        {
            if (packet->actionType == (u8)AutoActModuleTriggerAndResponseMessages::SET_ENTRY && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE + sizeof(AutoActModuleSetEntryResponse))
            {
                const AutoActModuleSetEntryResponse* msg = (const AutoActModuleSetEntryResponse*)packet->data;
                logjson("AAMOD",
                    "{"
                    "\"type\":\"set_autoact_entry_result\","
                    "\"nodeId\":%u,"
                    "\"requestHandle\":%u,"
                    "\"module\":%u,"
                    "\"code\":%u,"
                    "\"index\":%u"
                    "}" SEP,
                    packet->header.sender,
                    packet->requestHandle,
                    (u32)ModuleId::AUTO_ACT_MODULE,
                    (u32)msg->code,
                    msg->entryIndex);
            }
            else if (packet->actionType == (u8)AutoActModuleTriggerAndResponseMessages::CLEAR_ENTRY && sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE + sizeof(AutoActModuleClearEntryResponse))
            {
                const AutoActModuleClearEntryResponse* msg = (const AutoActModuleClearEntryResponse*)packet->data;
                logjson("AAMOD",
                    "{"
                    "\"type\":\"clear_autoact_entry_result\","
                    "\"nodeId\":%u,"
                    "\"requestHandle\":%u,"
                    "\"module\":%u,"
                    "\"code\":%u,"
                    "\"index\":%u"
                    "}" SEP,
                    packet->header.sender,
                    packet->requestHandle,
                    (u32)ModuleId::AUTO_ACT_MODULE,
                    (u32)msg->code,
                    msg->entryIndex);
            }
            else
            {
                SIMEXCEPTION(IllegalArgumentException); //LCOV_EXCL_LINE Unclear how to get in this state.
            }
        }
    }

#if IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
    //We parse all incoming component_sense message regardless of moduleId
    //and check if we have subscribed to them
    if (packetHeader->messageType == MessageType::COMPONENT_SENSE && sendData->dataLength >= SIZEOF_COMPONENT_MESSAGE_HEADER)
    {
        ModuleIdWrapper moduleId = INVALID_WRAPPED_MODULE_ID;
        NodeId receiverId = 0;
        u8 actionType = 0;
        u16 component = 0;
        u16 registerAddress = 0;
        const u8* inPayload = nullptr;
        MessageLength inPayloadLength = 0;
        bool isVendor = false;
        bool headerRead = false;

        const ComponentMessageHeader* componentHeader = (const ComponentMessageHeader*)packetHeader;

        //We must first check if the first byte indicates a ModuleId or a VendorModuleId
        if (!Utility::IsVendorModuleId(componentHeader->moduleId))
        {
            const ConnPacketComponentMessage* data = (const ConnPacketComponentMessage*)packetHeader;

            moduleId = Utility::GetWrappedModuleId(componentHeader->moduleId);

            receiverId = data->componentHeader.header.receiver;
            actionType = data->componentHeader.actionType;
            component = data->componentHeader.component;
            registerAddress = data->componentHeader.registerAddress;
            inPayload = data->payload;
            inPayloadLength = sendData->dataLength - sizeof(data->componentHeader);
            isVendor = false;
            headerRead = true;

        }
        else if (Utility::IsVendorModuleId(componentHeader->moduleId) && sendData->dataLength >= SIZEOF_COMPONENT_MESSAGE_HEADER_VENDOR)
        {
            const ConnPacketComponentMessageVendor* data = (const ConnPacketComponentMessageVendor*)packetHeader;

            moduleId = (ModuleIdWrapper)data->componentHeader.moduleId;

            receiverId = data->componentHeader.header.receiver;
            actionType = data->componentHeader.actionType;
            component = data->componentHeader.component;
            registerAddress = data->componentHeader.registerAddress;
            inPayload = data->payload;
            inPayloadLength = sendData->dataLength - sizeof(data->componentHeader);
            isVendor = true;
            headerRead = true;
        }

        if(headerRead)
        {
            for (u32 i = 0; i < MAX_AMOUNT_OF_ENTRIES; i++)
            {
                // FIXME: Quite a heavy performance hit currently. 
                //        See https://repo.mwaysolutions.com/relution/fruitymesh/-/merge_requests/1409#note_336338
                const AutoActTableEntryV0* entry = getTableEntryV0(i);
                if (entry) // Entry exists
                {
                    if (   entry->receiverNodeIdFilter    == receiverId
                        && entry->moduleIdFilter  == moduleId
                        && entry->componentFilter == component
                        && entry->registerFilter  == registerAddress) // Entry filters match
                    {
                        u8 buffer[sizeof(ConnPacketComponentMessageVendor) + MAX_IO_SIZE];
                        CheckedMemset(buffer, 0, sizeof(buffer));
                        ConnPacketHeader* header = (ConnPacketHeader*)buffer;
                        header->messageType = entry->toSense == 1 ? MessageType::COMPONENT_SENSE : MessageType::COMPONENT_ACT;
                        header->sender = GS->node.configuration.nodeId;
                        header->receiver = GS->node.configuration.nodeId;
                        u8* outPayload;
                        u32 headerSize;
                        if (!Utility::IsVendorModuleId(entry->targetModuleId))
                        {
                            ConnPacketComponentMessage* transformedMessage = (ConnPacketComponentMessage*)buffer;

                            transformedMessage->componentHeader.moduleId        = Utility::GetModuleId(entry->targetModuleId);
                            transformedMessage->componentHeader.requestHandle   = 0;
                            transformedMessage->componentHeader.actionType      = entry->toSense ? (u8)SensorMessageActionType::UNSPECIFIED : (u8)ActorMessageActionType::WRITE;
                            transformedMessage->componentHeader.component       = entry->targetComponent;
                            transformedMessage->componentHeader.registerAddress = entry->targetRegister;

                            outPayload = transformedMessage->payload;
                            headerSize = SIZEOF_CONN_PACKET_COMPONENT_MESSAGE;
                        }
                        else
                        {
                            ConnPacketComponentMessageVendor* transformedMessage = (ConnPacketComponentMessageVendor*)buffer;

                            transformedMessage->componentHeader.moduleId        = entry->targetModuleId;
                            transformedMessage->componentHeader.requestHandle   = 0;
                            transformedMessage->componentHeader.actionType      = entry->toSense ? (u8)SensorMessageActionType::UNSPECIFIED : (u8)ActorMessageActionType::WRITE;
                            transformedMessage->componentHeader.component       = entry->targetComponent;
                            transformedMessage->componentHeader.registerAddress = entry->targetRegister;

                            outPayload = transformedMessage->payload;
                            headerSize = SIZEOF_CONN_PACKET_COMPONENT_MESSAGE_VENDOR;
                        }

                        AutoActModuleResponse transformResponse = Transform(inPayload, inPayloadLength.GetRaw(), entry->orgDataType, outPayload, entry->targetDataType, entry->functionList, entry->functionListLength);
                        if (AutoActModuleResponseCode::SUCCESS == transformResponse.code)
                        {
                            BaseConnectionSendData sendData;
                            sendData.characteristicHandle = FruityHal::FH_BLE_INVALID_HANDLE;
                            sendData.dataLength = headerSize + transformResponse.outputSize;
                            sendData.deliveryOption = DeliveryOption::WRITE_CMD;
                            // NOTE: GS->cm.DispatchMeshMessage does similar stuff, however it also counts things like received messages which we don't want to increase here.
                            for (u32 i = 0; i < GS->amountOfModules; i++) {
                                if (GS->activeModules[i]->configurationPointer->moduleActive) {
                                    GS->activeModules[i]->MeshMessageReceivedHandler(nullptr, &sendData, header);
                                }
                            }
                        }
                        else
                        {
                            SIMEXCEPTION(IllegalStateException);
                        }
                    }
                }
            }
        }
    }
#endif //IS_INACTIVE(ONLY_SINK_FUNCTIONALITY)
}

const AutoActTableEntryV0* AutoActModule::getTableEntryV0(u8 entryIndex)
{
    SizedData data = GS->recordStorage.GetRecordData(RECORD_STORAGE_RECORD_ID_AUTO_ACT_ENTRIES_BASE + entryIndex);
    if (data.data && data.length >= sizeof(AutoActTableEntryV0))
    {
        return (const AutoActTableEntryV0*)data.data;
    }
    return nullptr;
}

void AutoActModule::SetEntry(u8 entryIndex, const AutoActTableEntryV0* tableEntry, MessageLength tableEntryBufferSize, u8 moduleVersion, NodeId sender, u8 requestHandle)
{
    if (entryIndex >= MAX_AMOUNT_OF_ENTRIES)
    {
        SendResponse(AutoActModuleSetEntryResponse{ AutoActModuleResponseCode::ENTRY_INDEX_OUT_OF_RANGE, entryIndex }, sender, requestHandle);
        return;
    }
    if (moduleVersion != AUTO_ACT_MODULE_CONFIG_VERSION)
    {
        SendResponse(AutoActModuleSetEntryResponse{ AutoActModuleResponseCode::MODULE_VERSION_NOT_SUPPORTED, entryIndex }, sender, requestHandle);
        return;
    }
    if (GS->node.GetModuleById(tableEntry->targetModuleId) == nullptr)
    {
        SendResponse(AutoActModuleSetEntryResponse{ AutoActModuleResponseCode::NO_SUCH_TARGET_MODULE, entryIndex }, sender, requestHandle);
        return;
    }
    if (SIZEOF_AUTO_ACT_TABLE_ENTRY_V0 + tableEntry->functionListLength != tableEntryBufferSize)
    {
        SendResponse(AutoActModuleSetEntryResponse{ AutoActModuleResponseCode::FUNCTION_LIST_LENGTH_WRONG, entryIndex }, sender, requestHandle);
        return;
    }

    {
        // Perform a dry run with dummy data and see if all transformations were successful.
        u8 input[MAX_IO_SIZE];
        CheckedMemset(input, 0, sizeof(input));
        u8 output[MAX_IO_SIZE];
        CheckedMemset(output, 0, sizeof(output));
        AutoActModuleResponseCode dryRunResult = Transform(input, sizeof(input), tableEntry->orgDataType, output, tableEntry->targetDataType, tableEntry->functionList, tableEntry->functionListLength).code;
        if (dryRunResult != AutoActModuleResponseCode::SUCCESS)
        {
            SendResponse(AutoActModuleSetEntryResponse{ dryRunResult, entryIndex }, sender, requestHandle);
            return;
        }
    }
    RecordStorageUserData userData = {};
    userData.sender = sender;
    userData.entryIndex = entryIndex;
    userData.requestHandle = requestHandle;
    RecordStorageResultCode code = GS->recordStorage.SaveRecord(
        RECORD_STORAGE_RECORD_ID_AUTO_ACT_ENTRIES_BASE + entryIndex, 
        (const u8*)tableEntry, tableEntryBufferSize.GetRaw(), 
        this, 
        (u32)AutoActModuleTriggerAndResponseMessages::SET_ENTRY, 
        (u8*)&userData, sizeof(userData));
    if (code != RecordStorageResultCode::SUCCESS)
    {
        SendResponse(AutoActModuleSetEntryResponse{ TranslateRecordStorageCode(code), entryIndex }, sender, requestHandle);
        return;
    }
    // => Continued asynchronosly in RecordStorageEventHandler
}

void AutoActModule::ClearEntry(u8 entryIndex, NodeId sender, u8 requestHandle)
{
    RecordStorageUserData userData = {};
    userData.sender = sender;
    userData.entryIndex = entryIndex;
    userData.requestHandle = requestHandle;
    RecordStorageResultCode code = GS->recordStorage.DeactivateRecord(RECORD_STORAGE_RECORD_ID_AUTO_ACT_ENTRIES_BASE + entryIndex, this, (u8)AutoActModuleTriggerAndResponseMessages::CLEAR_ENTRY, (u8*)&userData, sizeof(userData));
    if (code != RecordStorageResultCode::SUCCESS)
    {
        SendResponse(AutoActModuleClearEntryResponse{ TranslateRecordStorageCode(code), entryIndex }, sender, requestHandle);     //LCOV_EXCL_LINE Unclear how to get in this state.
        return;                                                                                                                      //LCOV_EXCL_LINE Unclear how to get in this state.
    }
    // => Continued asynchronosly in RecordStorageEventHandler
}

void AutoActModule::ClearAllEntries(NodeId sender, u8 requestHandle)
{
    for (u8 entryIndex = 0; entryIndex < MAX_AMOUNT_OF_ENTRIES; entryIndex++)
    {
        if (getTableEntryV0(entryIndex))
        {
            RecordStorageUserData userData = {};
            userData.sender = sender;
            userData.entryIndex = entryIndex;
            userData.requestHandle = requestHandle;
            RecordStorageResultCode code = GS->recordStorage.DeactivateRecord(RECORD_STORAGE_RECORD_ID_AUTO_ACT_ENTRIES_BASE + entryIndex, this, (u8)AutoActModuleTriggerAndResponseMessages::CLEAR_ALL_ENTRIES, (u8*)&userData, sizeof(userData));
            if (code != RecordStorageResultCode::SUCCESS)
            {
                SendResponse(AutoActModuleClearEntryResponse{ TranslateRecordStorageCode(code), ALL_ENTRIES }, sender, requestHandle);
            }
            return;
        }
    }

    // If we have no entries, then deleting all entries was "successful".
    SendResponse(AutoActModuleClearEntryResponse{ AutoActModuleResponseCode::SUCCESS, ALL_ENTRIES }, sender, requestHandle);
}
