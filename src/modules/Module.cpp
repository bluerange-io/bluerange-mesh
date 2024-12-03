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

#include <Module.h>
#include <RecordStorage.h>
#include <Node.h>
#include <Utility.h>
#include <BaseConnection.h>
#include <GlobalState.h>

#include <EnrollmentModule.h>
#include <cstdlib>


Module::Module(ModuleId moduleId, const char* name)
    //We determine the longer wrapped module id from our small ModuleId
    :Module(Utility::GetWrappedModuleId(moduleId), name)
{
}

Module::Module(VendorModuleId _vendorModuleId, const char* name)
    :vendorModuleId(_vendorModuleId), moduleName(name), proxy(*this)
{
    //Overwritten by Modules
    this->configurationPointer = nullptr;
    this->configurationLength = 0;

    // FruityMesh core modules use their moduleId as a record storage id, vendor modules must specify the id themselves
    if (!Utility::IsVendorModuleId(moduleId)) this->recordStorageId = (u16)moduleId;
}

Module::~Module()
{
}

void Module::LoadModuleConfigurationAndStart()
{
    //Load the configuration and replace the default configuration if it exists
    GS->config.LoadSettingsFromFlash(this, this->recordStorageId, (u8*)this->configurationPointer, this->configurationLength);
}

ErrorTypeUnchecked Module::SendModuleActionMessage(MessageType messageType, NodeId toNode, u8 actionType, u8 requestHandle, const u8* additionalData, u16 additionalDataSize, bool reliable) const
{
    return SendModuleActionMessage(messageType, toNode, actionType, requestHandle, additionalData, additionalDataSize, reliable, true);
}

//Constructs a simple trigger action message and can take aditional payload data
ErrorTypeUnchecked Module::SendModuleActionMessage(MessageType messageType, NodeId toNode, u8 actionType, u8 requestHandle, const u8* additionalData, u16 additionalDataSize, bool reliable, bool loopback) const
{
    if (moduleId == ModuleId::VENDOR_MODULE_ID_PREFIX) {
        return GS->cm.SendModuleActionMessage(messageType, vendorModuleId, toNode, actionType, requestHandle, additionalData, additionalDataSize, reliable, loopback);
    }
    else {
        return GS->cm.SendModuleActionMessage(messageType, moduleId, toNode, actionType, requestHandle, additionalData, additionalDataSize, reliable, loopback);
    }
    
}

#ifdef TERMINAL_ENABLED
TerminalCommandHandlerReturnType Module::TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize)
{
    //If somebody wants to set the module config over uart, he's welcome
    //First, check if our module is meant
    if(commandArgsSize >= 3)
    {
        //Read the moduleId from the current module
        ModuleIdWrapper wrappedModuleId = (ModuleIdWrapper)vendorModuleId;

        //Now, we check if the module name or module id given in the command matches our current module
        ModuleIdWrapper requestedModuleId = Utility::GetWrappedModuleIdFromTerminal(commandArgs[2]);

        if (requestedModuleId != wrappedModuleId) return TerminalCommandHandlerReturnType::UNKNOWN;

        //E.g. UART_MODULE_SET_CONFIG 0 STATUS 00:FF:A0 => command, nodeId (this for current node), moduleId, hex-string
        if(TERMARGS(0, "set_config"))
        {
            if (commandArgsSize < 4) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;

            const NodeId receiver = Utility::TerminalArgumentToNodeId(commandArgs[1]);

            //Parse the hex string configuration into a buffer
            const char* configStringPtr = commandArgs[3];
            u8 configBuffer[MAX_MESH_PACKET_SIZE - SIZEOF_CONN_PACKET_MODULE];
            u16 configLength = Logger::ParseEncodedStringToBuffer(configStringPtr, configBuffer, sizeof(configBuffer));

            u8 requestHandle = commandArgsSize >= 5 ? Utility::StringToU8(commandArgs[4]) : 0;

            //We can simply use the wrappedModuleId as the following method will pick the correct packet type to send
            GS->cm.SendModuleActionMessage(
                    MessageType::MODULE_CONFIG,
                    wrappedModuleId,
                    receiver,
                    (u8)ModuleConfigMessages::SET_CONFIG,
                    requestHandle,
                    configBuffer,
                    configLength,
                    false,
                    true);

            return TerminalCommandHandlerReturnType::WARN_DEPRECATED; //Deprecated as of 17.01.2020
        }
        else if(TERMARGS(0, "get_config"))
        {
            const NodeId receiver = Utility::TerminalArgumentToNodeId(commandArgs[1]);
            
            u8 requestHandle = commandArgsSize >= 4 ? Utility::StringToU8(commandArgs[3]) : 0;

            //We can simply use the wrappedModuleId as the following method will pick the correct packet type to send
            GS->cm.SendModuleActionMessage(
                    MessageType::MODULE_CONFIG,
                    wrappedModuleId,
                    receiver,
                    (u8)ModuleConfigMessages::GET_CONFIG,
                    requestHandle,
                    nullptr,
                    0,
                    false,
                    true);

            return TerminalCommandHandlerReturnType::WARN_DEPRECATED; //Deprecated as of 17.01.2020
        }
        else if(TERMARGS(0,"set_active"))
        {
            if(commandArgsSize <= 3) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;

            const NodeId receiver = Utility::TerminalArgumentToNodeId(commandArgs[1]);
            
            u8 buffer[1];
            buffer[0] = TERMARGS(3, "on") ? 1: 0;
            u8 requestHandle = commandArgsSize >= 5 ? Utility::StringToU8(commandArgs[4]) : 0;

            //We can simply use the wrappedModuleId as the following method will pick the correct packet type to send
            GS->cm.SendModuleActionMessage(
                    MessageType::MODULE_CONFIG,
                    wrappedModuleId,
                    receiver,
                    (u8)ModuleConfigMessages::SET_ACTIVE,
                    requestHandle,
                    buffer,
                    sizeof(buffer),
                    false,
                    true);

            return TerminalCommandHandlerReturnType::SUCCESS;
        }
    }

    return TerminalCommandHandlerReturnType::UNKNOWN;
}
#endif

// Can be used to either send a Vendor component message (4 byte vendor module id) or a message
// using only a ModuleId (1 byte) based on the given moduleId
static void HelperSendComponentMessage(ConnPacketComponentMessageVendor* message, u16 size)
{
    if (!Utility::IsVendorModuleId(message->componentHeader.moduleId))
    {
        ConnPacketComponentMessage* actualStructure = (ConnPacketComponentMessage*)message;
        CheckedMemmove(&(actualStructure->componentHeader.requestHandle), &(message->componentHeader.requestHandle), size - 3 - offsetof(ComponentMessageHeader, requestHandle));
        size -= (sizeof(VendorModuleId) - sizeof(ModuleId));
    }
    GS->cm.SendMeshMessage(
        (u8*)message,
        size);
}

void Module::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader)
{
    //We want to handle incoming packets that change the module configuration
    if(
            packetHeader->messageType == MessageType::MODULE_CONFIG
    ){
        //#### PART 1: We extract all necessary information from either ConnPacketModule or ConnPacketModuleVendor
        //By using this data, we can do most logic without accessing either the packet or the vendorPacket
        ConnPacketModule const * packet = (ConnPacketModule const *) packetHeader;
        ConnPacketModuleVendor const * packetVendor = (ConnPacketModuleVendor const *) packetHeader;

        NodeId senderId = packetHeader->sender;
        ModuleIdWrapper wrappedModuleId;
        MessageLength dataLength;
        const u8* dataPtr = nullptr;
        u8 requestHandle = 0;
        ModuleConfigMessages actionType;

        //Extract info from ConnPacketHeader
        if(!Utility::IsVendorModuleId(packet->moduleId) && packet->moduleId == moduleId)
        {
            wrappedModuleId = Utility::GetWrappedModuleId(packet->moduleId);
            requestHandle = packet->requestHandle;
            actionType = (ModuleConfigMessages)packet->actionType;
            dataLength = sendData->dataLength - SIZEOF_CONN_PACKET_MODULE;
            dataPtr = packet->data;
        }
        //Extract info from ConnPacketHeaderVendor
        else if(sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE_VENDOR && packetVendor->moduleId == vendorModuleId)
        {
            wrappedModuleId = packetVendor->moduleId;
            requestHandle = packetVendor->requestHandle;
            actionType = (ModuleConfigMessages)packetVendor->actionType;
            dataLength = sendData->dataLength - SIZEOF_CONN_PACKET_MODULE_VENDOR;
            dataPtr = packetVendor->data;
        }
        else {
            //We return in case the packet is invalid or does not belong to our module
            return;
        }
        
        //#### PART 2: Request Handling ############################
        
        if(actionType == ModuleConfigMessages::SET_CONFIG)
        {
            SetConfigResultCodes result = SetConfigResultCodes::SUCCESS;
            
            //We do not allow setting the configuration for core modules except the StatusReporterModule (IOT-4327)
            if (!Utility::IsVendorModuleId(moduleId) && moduleId != ModuleId::STATUS_REPORTER_MODULE) {
                result = SetConfigResultCodes::NO_CONFIGURATION;
            }
            else if(configurationPointer == nullptr)
            {
                result = SetConfigResultCodes::NO_CONFIGURATION;
            }
            else if(!Utility::IsVendorModuleId(wrappedModuleId))
            {
                //Check the ModuleConfiguration against our local configuration
                ModuleConfiguration* oldConfig = configurationPointer;
                const ModuleConfiguration* newConfig = (const ModuleConfiguration*) packet->data;
                if(
                    moduleId == packet->moduleId
                    && newConfig->moduleId == oldConfig->moduleId
                    && newConfig->moduleVersion == oldConfig->moduleVersion
                    && dataLength.GetRaw() == configurationLength
                ){
                    //Overwrite the old configuration
                    CheckedMemcpy(oldConfig, newConfig, dataLength.GetRaw());
                } else {
                    result = SetConfigResultCodes::WRONG_CONFIGURATION;
                }
            }
            else
            {
                //Check the VendorModuleConfiguration against our local configuration
                VendorModuleConfiguration* oldConfig = vendorConfigurationPointer;
                const VendorModuleConfiguration* newConfig = (const VendorModuleConfiguration*) packetVendor->data;
                if(
                    vendorModuleId == packetVendor->moduleId
                    && newConfig->moduleId == oldConfig->moduleId
                    && newConfig->moduleVersion == oldConfig->moduleVersion
                    && dataLength.GetRaw() == configurationLength
                ){
                    //Overwrite the old configuration
                    CheckedMemcpy(oldConfig, newConfig, dataLength.GetRaw());
                } else {
                    result = SetConfigResultCodes::WRONG_CONFIGURATION;
                }
            }

            if(result == SetConfigResultCodes::SUCCESS){
                //Call the configuration loaded handler to reinitialize stuff if necessary (RAM config is already set)
                ConfigurationLoadedHandler(nullptr, 0);

                //Save the module config to flash
                SaveModuleConfigAction userData;
                CheckedMemset(&userData, 0x00, sizeof(userData));

                userData.moduleId = wrappedModuleId;
                userData.sender = senderId;
                userData.requestHandle = requestHandle;

                result = (SetConfigResultCodes)Utility::SaveModuleSettingsToFlash(
                    this,
                    this->configurationPointer,
                    this->configurationLength,
                    &proxy,
                    (u8)ModuleSaveAction::SAVE_MODULE_CONFIG_ACTION,
                    (u8*)&userData,
                    sizeof(SaveModuleConfigAction));
            }

            //If there was an error before storing the config in the flash, we must send a response immediately
            if(result != SetConfigResultCodes::SUCCESS){
                SendModuleConfigResult(
                    senderId,
                    wrappedModuleId,
                    ModuleConfigMessages::SET_CONFIG_RESULT,
                    result,
                    requestHandle);
            }
        }
        else if(actionType == ModuleConfigMessages::GET_CONFIG)
        {
            //We do not allow reading the configuration for core modules except the StatusReporterModule (IOT-4327)
            if (!Utility::IsVendorModuleId(moduleId) && moduleId != ModuleId::STATUS_REPORTER_MODULE){
                ModuleConfigResultCodeMessage data;
                CheckedMemset(&data, 0x00, sizeof(data));
                data.result = SetConfigResultCodes::NO_CONFIGURATION;

                GS->cm.SendModuleActionMessage(
                    MessageType::MODULE_CONFIG,
                    wrappedModuleId,
                    senderId,
                    (u8)ModuleConfigMessages::GET_CONFIG_ERROR,
                    requestHandle,
                    (u8*)&data,
                    sizeof(data),
                    false,
                    true);
            } else {
                //We can use SendModuleActionMessage with the configurationPointer and configurationLength
                //as the datastruct is a union and can be used to generate both normal and vendor messages
                GS->cm.SendModuleActionMessage(
                    MessageType::MODULE_CONFIG,
                    wrappedModuleId,
                    senderId,
                    (u8)ModuleConfigMessages::CONFIG,
                    requestHandle,
                    (u8*)configurationPointer,
                    configurationLength,
                    false,
                    true);
            }
        }
        else if(actionType == ModuleConfigMessages::SET_ACTIVE)
        {
            SetConfigResultCodes result = SetConfigResultCodes::SUCCESS;

            //We do not allow to modify the state of the node module
            if (moduleId == ModuleId::NODE)
            {
                result = SetConfigResultCodes::NO_CONFIGURATION;
            }
            //Check if the message has the required minimum length
            else if(dataLength < 1)
            {
                result = SetConfigResultCodes::WRONG_CONFIGURATION;
            }
            else if(configurationPointer == nullptr)
            {
                result = SetConfigResultCodes::NO_CONFIGURATION;
            }
            else if(!Utility::IsVendorModuleId(moduleId))
            {
                bool active = packet->data[0];
                configurationPointer->moduleActive = active ? 1 : 0;
            }
            else
            {
                bool active = packetVendor->data[0];
                vendorConfigurationPointer->moduleActive = active ? 1 : 0;
            }
            
            if(result == SetConfigResultCodes::SUCCESS){
                ConfigurationLoadedHandler(nullptr, 0);

                //Save the module config to flash
                SaveModuleConfigAction userData;
                CheckedMemset(&userData, 0x00, sizeof(userData));
                
                userData.moduleId = wrappedModuleId;
                userData.sender = senderId;
                userData.requestHandle = requestHandle;

                result = (SetConfigResultCodes)Utility::SaveModuleSettingsToFlash(
                        this,
                        this->configurationPointer,
                        this->configurationLength,
                        &proxy,
                        (u8)ModuleSaveAction::SET_ACTIVE_CONFIG_ACTION,
                        (u8*)&userData,
                        sizeof(SaveModuleConfigAction));
            }

            //If there was an error before storing the config in the flash, we must send a response immediately
            if(result != SetConfigResultCodes::SUCCESS){
                SendModuleConfigResult(
                    senderId,
                    wrappedModuleId,
                    ModuleConfigMessages::SET_ACTIVE_RESULT,
                    result,
                    requestHandle);
            }
        }


        //#### PART 2: Response Handling ############################

        if(actionType == ModuleConfigMessages::SET_CONFIG_RESULT)
        {
            logjson_partial("MODULE", "{\"nodeId\":%u,\"type\":\"set_config_result\",\"module\":%s,", packetHeader->sender, Utility::GetModuleIdString(wrappedModuleId).data());
            logjson("MODULE",  "\"requestHandle\":%u,\"code\":%u}" SEP, requestHandle, dataPtr[0]);
        }
        else if(actionType == ModuleConfigMessages::SET_ACTIVE_RESULT)
        {
            logjson_partial("MODULE", "{\"nodeId\":%u,\"type\":\"set_active_result\",\"module\":%s,", packetHeader->sender, Utility::GetModuleIdString(wrappedModuleId).data());
            logjson("MODULE",  "\"requestHandle\":%u,\"code\":%u}" SEP, requestHandle, dataPtr[0]);
        }
        else if(actionType == ModuleConfigMessages::CONFIG)
        {
            char buffer[200];
            Logger::ConvertBufferToHexString(dataPtr, dataLength, buffer, sizeof(buffer));

            logjson("MODULE", "{\"nodeId\":%u,\"type\":\"config\",\"module\":%s,\"requestHandle\":%u,\"config\":\"%s\"}" SEP, packetHeader->sender, Utility::GetModuleIdString(wrappedModuleId).data(), requestHandle, buffer);
        }
        else if(actionType == ModuleConfigMessages::GET_CONFIG_ERROR)
        {
            const ModuleConfigResultCodeMessage* data = (const ModuleConfigResultCodeMessage*)dataPtr;

            logjson("MODULE", "{\"nodeId\":%u,\"type\":\"get_config_error\",\"module\":%s,\"requestHandle\":%u,\"code\":%u}" SEP,
            packetHeader->sender, Utility::GetModuleIdString(wrappedModuleId).data(), requestHandle, (u32)data->result);
        }
    }

#if IS_ACTIVE(REGISTER_HANDLER)
    //Handles writing to our Generic Register Handlers
    if (packetHeader->messageType == MessageType::COMPONENT_ACT)
    {
        ConnPacketModuleContents cpmc;
        if (!Utility::ToConnPacketModuleContents(&cpmc, sendData, packetHeader)) return;
        if (!Utility::IsSameModuleId(cpmc.moduleId, vendorModuleId)) return;
        if (cpmc.dataSize < sizeof(ConnPacketComponentMessageContents) - 1) return;

        u16 payloadLen = sendData->dataLength.GetRaw() - SIZEOF_CONN_PACKET_COMPONENT_MESSAGE;
        if (Utility::IsVendorModuleId(vendorModuleId)) payloadLen = sendData->dataLength.GetRaw() - SIZEOF_CONN_PACKET_COMPONENT_MESSAGE_VENDOR;

        const ConnPacketComponentMessageContents* cpcmc = (const ConnPacketComponentMessageContents*)cpmc.data;

        if (cpmc.actionType == (u8)ActorMessageActionType::WRITE
            || cpmc.actionType == (u8)ActorMessageActionType::WRITE_ACK)
        {
            RecordStorageUserData userData;
            userData.receiver = cpmc.sender;
            userData.moduleId = cpmc.moduleId;
            userData.component = cpcmc->component;
            userData.registerAddress = cpcmc->registerAddress;
            userData.requestHandle = cpmc.requestHandle;

            RegisterHandlerCodeStage code = SetRegisterValues(
                cpcmc->component,
                cpcmc->registerAddress,
                cpcmc->payload,
                payloadLen,
                cpmc.actionType == (u8)ActorMessageActionType::WRITE_ACK ? this : nullptr,
                USER_TYPE_COMPONENT_ACT_WRITE,
                (u8*)&userData,
                sizeof(userData),
                RegisterHandlerSetSource::MESH
            );
            //If the register handler is disabled, we do not perform any further functionality and do not send errors
            if (code.code == RegisterHandlerCode::LOCATION_DISABLED) return;

            if (code.code != RegisterHandlerCode::SUCCESS
                && cpmc.actionType == (u8)ActorMessageActionType::WRITE_ACK)
            {
                u8 buffer[SIZEOF_CONN_PACKET_COMPONENT_MESSAGE_VENDOR + 2] = {};
                ConnPacketComponentMessageVendor* reply = (ConnPacketComponentMessageVendor*)buffer;
                reply->componentHeader.header.messageType = MessageType::COMPONENT_SENSE;
                reply->componentHeader.header.sender = GS->node.configuration.nodeId;
                reply->componentHeader.header.receiver = cpmc.sender;
                reply->componentHeader.moduleId = cpmc.moduleId;
                reply->componentHeader.component = cpcmc->component;
                reply->componentHeader.registerAddress = cpcmc->registerAddress;
                reply->componentHeader.requestHandle = cpmc.requestHandle;

                reply->componentHeader.actionType = (u8)SensorMessageActionType::ERROR_RSP;
                reply->payload[0] = (u8)code.code;
                reply->payload[1] = (u8)code.stage;

                HelperSendComponentMessage(reply, SIZEOF_CONN_PACKET_COMPONENT_MESSAGE_VENDOR + 2);
            }
        }
        //Handles Reading from the Generic Register Handlers
        else if (cpmc.actionType == (u8)ActorMessageActionType::READ && sendData->dataLength >= SIZEOF_CONN_PACKET_COMPONENT_MESSAGE + 1)
        {
            DYNAMIC_ARRAY(buffer, SIZEOF_CONN_PACKET_COMPONENT_MESSAGE_VENDOR + cpcmc->payload[0]);
            CheckedMemset(buffer, 0, SIZEOF_CONN_PACKET_COMPONENT_MESSAGE_VENDOR + cpcmc->payload[0]);

            ConnPacketComponentMessageVendor* reply = (ConnPacketComponentMessageVendor*)buffer;
            reply->componentHeader.header.messageType = MessageType::COMPONENT_SENSE;
            reply->componentHeader.header.sender = GS->node.configuration.nodeId;
            reply->componentHeader.header.receiver = cpmc.sender;
            reply->componentHeader.moduleId = cpmc.moduleId;
            reply->componentHeader.component = cpcmc->component;
            reply->componentHeader.registerAddress = cpcmc->registerAddress;
            reply->componentHeader.requestHandle = cpmc.requestHandle;

            RegisterHandlerCode code = GetRegisterValues(cpcmc->component, cpcmc->registerAddress, reply->payload, cpcmc->payload[0]);
            //Do not perform any functionality if the RegisterHandler is disabled for the given location
            if (code == RegisterHandlerCode::LOCATION_DISABLED) return;


            u16 totalLength = SIZEOF_COMPONENT_MESSAGE_HEADER_VENDOR;
            if (code == RegisterHandlerCode::SUCCESS)
            {
                reply->componentHeader.actionType = (u8)SensorMessageActionType::READ_RSP;
                totalLength += cpcmc->payload[0];
            }
            else
            {
                reply->componentHeader.actionType = (u8)SensorMessageActionType::ERROR_RSP;
                reply->payload[0] = (u8)code;
                totalLength += 1;
            }
            HelperSendComponentMessage(reply, totalLength);
        }
    }
#endif //IS_ACTIVE(REGISTER_HANDLER)
}

PreEnrollmentReturnCode Module::PreEnrollmentHandler(ConnPacketModule* enrollmentPacket, MessageLength packetLength)
{
    return PreEnrollmentReturnCode::DONE;
}


Module::RecordStorageEventListenerProxy::RecordStorageEventListenerProxy(Module& mod)
    : mod(mod)
{
}

void Module::ProxyRecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength)
{
    if (userDataLength > 0) {
        DYNAMIC_ARRAY(buffer, userDataLength);
        CheckedMemcpy(buffer, userData, userDataLength);

        SaveModuleConfigAction* requestData = (SaveModuleConfigAction*)buffer;

        if (userType == (u8)ModuleSaveAction::SAVE_MODULE_CONFIG_ACTION)
        {
            //We are allowed to cast the RecordStorageResultCode to a SetConfigResultCodes as they share the same space
            SendModuleConfigResult(
                requestData->sender,
                requestData->moduleId,
                ModuleConfigMessages::SET_CONFIG_RESULT,
                (SetConfigResultCodes)resultCode,
                requestData->requestHandle);
        }
        else if (userType == (u8)ModuleSaveAction::SET_ACTIVE_CONFIG_ACTION)
        {
            //We are allowed to cast the RecordStorageResultCode to a SetConfigResultCodes as they share the same space
            SendModuleConfigResult(
                requestData->sender,
                requestData->moduleId,
                ModuleConfigMessages::SET_ACTIVE_RESULT,
                (SetConfigResultCodes)resultCode,
                requestData->requestHandle);
        }
    }
}

void Module::RecordStorageEventListenerProxy::RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength)
{
    mod.ProxyRecordStorageEventHandler(recordId, resultCode, userType, userData, userDataLength);
}

void Module::SendModuleConfigResult(NodeId senderId, ModuleIdWrapper moduleId, ModuleConfigMessages actionType, SetConfigResultCodes result, u8 requestHandle)
{
    ModuleConfigResultCodeMessage data;
    CheckedMemset(&data, 0x00, sizeof(data));

    //We can cast the record storage result code into our result code as they use the same space
    data.result = result;

    GS->cm.SendModuleActionMessage(
        MessageType::MODULE_CONFIG,
        moduleId,
        senderId,
        (u8)actionType,
        requestHandle,
        (u8*)&data,
        sizeof(data),
        false,
        true);
}


#if IS_ACTIVE(REGISTER_HANDLER)

#ifdef JSTODO_PERSISTENCE
void Module::RecordStorageEventHandlerRegisterProxy(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength)
{
    RecordStorageUserData* data = (RecordStorageUserData*)userData;
    if (resultCode == RecordStorageResultCode::SUCCESS)
    {
        for (u32 i = 0; i < data->length; i++)
        {
            CommitRegisterChange(data->component, data->reg + i, data->source);
        }
    }
    else
    {
        // Nothing. We have no idea how to handle errors in such a case. It's the duty of the passed callback to deal with it.
    }

    if (data->callback)
    {
        u8* userUserData = nullptr;
        u16 userUserDataLength = userDataLength - offsetof(RecordStorageUserData, userData);
        if (userUserDataLength > 0)
        {
            userUserData = data->userData;
        }
        data->callback->RecordStorageEventHandler(recordId, resultCode, userType, userUserData, userUserDataLength);
    }
}
#endif

u16 Module::GetRecordBaseId() const
{
    if (Utility::IsVendorModuleId(vendorModuleId))
    {
        return ((recordStorageId - RECORD_STORAGE_RECORD_ID_VENDOR_MODULE_CONFIG_BASE) * REGISTER_RECORDS_PER_MODULE) + RECORD_STORAGE_RECORD_ID_VENDOR_MODULES_REGISTER_ENTRIES_BASE;
    }
    else
    {
        return (((u32)moduleId) * REGISTER_RECORDS_PER_MODULE) + RECORD_STORAGE_RECORD_ID_INTERNAL_MODULES_REGISTER_ENTRIES_BASE;
    }
}

RegisterHandlerCode Module::GetRegisterValues(u16 component, u16 reg, u8* values, u16 length)
{
    RegisterGeneralChecks generalChecks = GetGeneralChecks(component, reg, length);
    if (generalChecks & RGC_LOCATION_DISABLED) return RegisterHandlerCode::LOCATION_DISABLED;

    //TODO: Implement the other error codes

    for (u32 i = 0; i < length; i++)
    {
        SupervisedValue val;
        u32 dummy = 0;
        MapRegister(component, reg + i, val, dummy);
        if (val.GetError() != RegisterHandlerCode::SUCCESS)
        {
            return val.GetError();
        }
        if (!val.IsSet())
        {
            return RegisterHandlerCode::LOCATION_UNSUPPORTED;
        }
        const u32 bytesLeft = length - i;
        if (bytesLeft < val.GetSize()
            && val.GetType() != SupervisedValue::Type::DYNAMIC_RANGE_WRITABLE
            && val.GetType() != SupervisedValue::Type::DYNAMIC_RANGE_READABLE) // It's totally fine to read a dynamic range only partially.
        {
            return RegisterHandlerCode::ILLEGAL_LENGTH;
        }
        val.ToBuffer(values + i, bytesLeft);
        OnRegisterRead(component, reg + i);
        i += val.GetSize() - 1; //-1 cause loop increment
    }
    return RegisterHandlerCode::SUCCESS;
}

RegisterHandlerCodeStage Module::SetRegisterValues(u16 component, u16 reg, const u8* values, u16 length, RegisterHandlerEventListener* callback, u32 userType, u8* userData, u16 userDataLength, RegisterHandlerSetSource source)
{
    if (source != RegisterHandlerSetSource::FLASH)
    {
        RegisterGeneralChecks generalChecks = GetGeneralChecks(component, reg, length);
        if (generalChecks & RGC_LOCATION_DISABLED)
        {
            return { RegisterHandlerCode::LOCATION_DISABLED, RegisterHandlerStage::GENERAL_CHECK };
        }
        if (generalChecks & RGC_LOCATION_UNSUPPORTED)
        {
            return { RegisterHandlerCode::LOCATION_UNSUPPORTED, RegisterHandlerStage::GENERAL_CHECK };
        }
        if (generalChecks & RGC_NULL_TERMINATED)
        {
            u8 end = values[length - 1];
            if (end != 0)
            {
                return { RegisterHandlerCode::ILLEGAL_VALUE, RegisterHandlerStage::GENERAL_CHECK };
            }
        }
        if (generalChecks & RGC_NO_MIDDLE_NULL)
        {
            for (u32 i = 0; i < length - 1u; i++)
            {
                if (values[i] == 0)
                {
                    return { RegisterHandlerCode::ILLEGAL_VALUE, RegisterHandlerStage::GENERAL_CHECK };
                }
            }
        }

        RegisterHandlerCode checked = CheckValues(component, reg, values, length);
        if (checked != RegisterHandlerCode::SUCCESS)
        {
            return { checked, RegisterHandlerStage::CHECK_VALUES };
        }
    }

    if (reg < REGISTER_WRITABLE_RANGE_BASE || reg >= REGISTER_WRITABLE_RANGE_BASE + REGISTER_WRITABLE_RANGE_SIZE
        || (reg + length - 1u) < REGISTER_WRITABLE_RANGE_BASE || (reg + length - 1u) >= REGISTER_WRITABLE_RANGE_BASE + REGISTER_WRITABLE_RANGE_SIZE)
    {
        return { RegisterHandlerCode::ILLEGAL_WRITE_LOCATION, RegisterHandlerStage::EARLY_CHECK };
    }
    if (length == 0)
    {
        return { RegisterHandlerCode::ILLEGAL_LENGTH, RegisterHandlerStage::EARLY_CHECK };
    }

    u32 persistedId = 0;
    for (u32 i = 0; i < length; i++)
    {
        // First pass to check for errors.
        SupervisedValue val;
        u32 pId = 0; // TODO: Implement persistence stuff
       MapRegister(component, reg + i, val, pId);

        if (val.GetError() != RegisterHandlerCode::SUCCESS)
        {
            return { val.GetError(), RegisterHandlerStage::ADDR_FAIL };
        }
        if (!val.IsSet())
        {
            return { RegisterHandlerCode::LOCATION_UNSUPPORTED, RegisterHandlerStage::ADDR_FAIL };
        }
        if (!val.IsWritable())
        {
            return { RegisterHandlerCode::NOT_WRITABLE, RegisterHandlerStage::ADDR_FAIL };
        }
        if (i == 0) persistedId = pId;
        else if (persistedId != pId)
        {
            // This is a limitation that could be removed in the future. Currently all registers in this
            // range have to report the same persistedId. This simplifies the code in at least two ways:
            //   1. We don't have to write multiple recordIds and wait for all their callbacks, somehow
            //      keeping track of them all and making sure that they are written in an atomic manner.
            //   2. It's clear which callback to call.
            return { RegisterHandlerCode::PERSISTED_ID_MISMATCH, RegisterHandlerStage::ADDR_FAIL };
        }
        if (pId >= REGISTER_RECORDS_PER_MODULE)
        {
            return { RegisterHandlerCode::PERSISTED_ID_OUT_OF_RANGE, RegisterHandlerStage::ADDR_FAIL };
        }
        if (pId > 1)
        {
            // A temporary limitation. The actual limit should be REGISTER_RECORDS_PER_MODULE. This is in
            // place because if we support multiple pIds, then we have to take possible migrations between
            // recordIds into account. This is the case when coming from the flash and from the mesh.
            // Deleting ranges from the flash is currently giving me a headache thinking about it. So
            // implementing this is delayed until it's actually required.
            return { RegisterHandlerCode::PERSISTED_ID_OUT_OF_RANGE, RegisterHandlerStage::ADDR_FAIL };
        }
        const u32 bytesLeft = length - i;
        if (bytesLeft < val.GetSize()
            && val.GetType() != SupervisedValue::Type::DYNAMIC_RANGE_WRITABLE) // It's totally fine to write a dynamic range only partially.
        {
            return { RegisterHandlerCode::ILLEGAL_LENGTH, RegisterHandlerStage::ADDR_FAIL };
        }
        i += val.GetSize() - 1;
    }

    bool valuesChanged = false;
    for (u32 i = 0; i < length; i++)
    {
        // Second pass to actually write the values.
        SupervisedValue val;
        u32 dummy = 0;
        MapRegister(component, reg + i, val, dummy);

        DYNAMIC_ARRAY(changeBuffer, val.GetSize());
        CheckedMemcpy(changeBuffer, values + i, val.GetSize());
        ChangeValue(component, reg + i, changeBuffer, val.GetSize());
        valuesChanged |= memcmp(values + i, changeBuffer, val.GetSize()) != 0;
        val.FromBuffer(changeBuffer, length - i);
        i += val.GetSize() - 1;
    }

    if (persistedId == 0 || source == RegisterHandlerSetSource::FLASH /*If we are already coming from the flash then there is no need to go back to the flash.*/)
    {
        for (u32 i = 0; i < length; i++)
        {
            CommitRegisterChange(component, reg + i, source);
        }
        // Calling the callback so that the caller doesn't have to care if this value is persisted or not.
        if (callback)
        {
            callback->RegisterHandlerEventHandler(0, RecordStorageResultCode::SUCCESS, userType, userData, userDataLength, valuesChanged);
        }

        return { RegisterHandlerCode::SUCCESS, RegisterHandlerStage::SUCCESS };
    }
    else
    {
#ifndef JSTODO_PERSISTENCE
        return { RegisterHandlerCode::NOT_IMPLEMENTED, RegisterHandlerStage::EARLY_RECORD_STORAGE };
#else
        const u16 recordId = GetRecordBaseId() + persistedId;
        RecordStorageRecord* record = GS->recordStorage.GetRecord(recordId);
        u32 maxSize = length * sizeof(u16);
        const u16* oldRecordStorage = nullptr;
        u16 oldRecordStorageLength = 0;
        if (record)
        {
            maxSize += record->recordLength;
            oldRecordStorage = (u16*)record->data;
            oldRecordStorageLength = record->recordLength / sizeof(u16);
        }
        if (maxSize % sizeof(u16) == 1)
        {
            maxSize++;
            // This should never happen - right?
            SIMEXCEPTION(IllegalStateException);
        }
        DYNAMIC_ARRAY(buffer, maxSize);
        const u16 actualLength = sizeof(u16) * InsertRegisterRange(oldRecordStorage, 0, component, reg, length, values, (u16*)buffer);

        // TODO Missing a commit call here! Probably we have to put a record storage callback in between.

        DYNAMIC_ARRAY(surroundingUserDataBuffer, sizeof(RecordStorageUserData) + userDataLength);
        CheckedMemset(surroundingUserDataBuffer, 0, sizeof(RecordStorageUserData) + userDataLength);
        RecordStorageUserData* surroundingUserData = (RecordStorageUserData*)buffer;
        surroundingUserData->component = component;
        surroundingUserData->reg = register_;
        surroundingUserData->length = length;
        surroundingUserData->source = source;
        surroundingUserData->callback = callback;
        if (userData)
        {
            CheckedMemcpy(surroundingUserData->userData, userData, userDataLength);
        }

        RecordStorageResultCode rsCode = GS->recordStorage.SaveRecord(
            recordId,
            buffer,
            actualLength,
            &proxy,
            userType,
            surroundingUserDataBuffer,
            sizeof(RecordStorageUserData) + userDataLength
        );
        if (rsCode == RecordStorageResultCode::SUCCESS) return { RegisterHandlerCode::SUCCESS, RegisterHandlerStage::EARLY_RECORD_STORAGE };
        else
        {
            static_assert((int)RecordStorageResultCode::LAST_ENTRY < (int)RegisterHandlerCode::RECORD_STORAGE_CODES_END - (int)RegisterHandlerCode::RECORD_STORAGE_CODES_START, "Not enough room to embed error code.");
            return { (RegisterHandlerCode)((int)rsCode + (int)RegisterHandlerCode::RECORD_STORAGE_CODES_START), RegisterHandlerStage::EARLY_RECORD_STORAGE };
        }
#endif
    }
}

#ifdef JSTODO_PERSISTENCE
Module::RecordStorageEventListenerRegisterProxy::RecordStorageEventListenerRegisterProxy(Module& mod) :
    mod(mod)
{
}

void Module::RecordStorageEventListenerRegisterProxy::RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength)
{
    mod.RecordStorageEventHandlerRegisterProxy(recordId, resultCode, userType, userData, userDataLength);
}
#endif

void Module::RegisterHandlerEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength, bool dataChanged)
{
    if (userType == USER_TYPE_COMPONENT_ACT_WRITE)
    {
        RecordStorageUserData* rs = (RecordStorageUserData*)userData;

        u8 buffer[SIZEOF_CONN_PACKET_COMPONENT_MESSAGE_VENDOR + 2] = {};
        ConnPacketComponentMessageVendor* reply = (ConnPacketComponentMessageVendor*)buffer;
        reply->componentHeader.header.messageType = MessageType::COMPONENT_SENSE;
        reply->componentHeader.header.sender = GS->node.configuration.nodeId;
        reply->componentHeader.header.receiver = rs->receiver;
        reply->componentHeader.moduleId = rs->moduleId;
        reply->componentHeader.component = rs->component;
        reply->componentHeader.registerAddress = rs->registerAddress;
        reply->componentHeader.requestHandle = rs->requestHandle;

        if (resultCode == RecordStorageResultCode::SUCCESS)
        {
            reply->componentHeader.actionType = (u8)SensorMessageActionType::RESULT_RSP;
            reply->payload[0] = !dataChanged ? (u8)RegisterHandlerCode::SUCCESS : (u8)RegisterHandlerCode::SUCCESS_CHANGE;
            reply->payload[1] = (u8)RegisterHandlerStage::SUCCESS;
        }
        else
        {
            reply->componentHeader.actionType = (u8)SensorMessageActionType::ERROR_RSP;
            reply->payload[0] = (u8)RegisterHandlerCode::RECORD_STORAGE_CODES_START + (u8)resultCode;
            reply->payload[1] = (u8)RegisterHandlerStage::LATE_RECORD_STORAGE;
        }

        HelperSendComponentMessage(reply, SIZEOF_CONN_PACKET_COMPONENT_MESSAGE_VENDOR + 2);
    }
}

#endif //IS_ACTIVE(REGISTER_HANDLER)