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
    :vendorModuleId(_vendorModuleId), moduleName(name)
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
                    this,
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
                        this,
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
}

PreEnrollmentReturnCode Module::PreEnrollmentHandler(ConnPacketModule* enrollmentPacket, MessageLength packetLength)
{
    return PreEnrollmentReturnCode::DONE;
}


void Module::RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength)
{
    if(userDataLength > 0){
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
    
    if(userType == (u8)ModuleSaveAction::PRE_ENROLLMENT_RECORD_DELETE)
    {
        logt("MODULE", "Remove config during preEnrollment status %u", (u32)resultCode);

        EnrollmentModule* enrollMod = (EnrollmentModule*)GS->node.GetModuleById(ModuleId::ENROLLMENT_MODULE);
        if(enrollMod != nullptr){
            if(resultCode == RecordStorageResultCode::SUCCESS){
                enrollMod->DispatchPreEnrollment(this, PreEnrollmentReturnCode::DONE);
            } else {
                enrollMod->DispatchPreEnrollment(this, PreEnrollmentReturnCode::FAILED);
            }
        }
    }
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