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

#include <Module.h>
#include <RecordStorage.h>
#include <Node.h>
#include <Utility.h>
#include <BaseConnection.h>
#include <GlobalState.h>

#include <EnrollmentModule.h>
#include <cstdlib>


Module::Module(ModuleId moduleId, const char* name)
	:moduleId(moduleId), moduleName(name)
{
	//Overwritten by Modules
	this->configurationPointer = nullptr;
	this->configurationLength = 0;
}

Module::~Module()
{
}

void Module::LoadModuleConfigurationAndStart()
{
	//Load the configuration and replace the default configuration if it exists

	GS->config.LoadSettingsFromFlash(this, this->moduleId, this->configurationPointer, this->configurationLength);
}

ErrorTypeUnchecked Module::SendModuleActionMessage(MessageType messageType, NodeId toNode, u8 actionType, u8 requestHandle, const u8* additionalData, u16 additionalDataSize, bool reliable) const
{
	return SendModuleActionMessage(messageType, toNode, actionType, requestHandle, additionalData, additionalDataSize, reliable, true);
}

//Constructs a simple trigger action message and can take aditional payload data
ErrorTypeUnchecked Module::SendModuleActionMessage(MessageType messageType, NodeId toNode, u8 actionType, u8 requestHandle, const u8* additionalData, u16 additionalDataSize, bool reliable, bool loopback) const
{
	return GS->cm.SendModuleActionMessage(messageType, moduleId, toNode, actionType, requestHandle, additionalData, additionalDataSize, reliable, loopback);
}

#ifdef TERMINAL_ENABLED
TerminalCommandHandlerReturnType Module::TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize)
{
	//If somebody wants to set the module config over uart, he's welcome
	//First, check if our module is meant
	if(commandArgsSize >= 3 && TERMARGS(2, moduleName))
	{
		//E.g. UART_MODULE_SET_CONFIG 0 STATUS 00:FF:A0 => command, nodeId (this for current node), moduleId, hex-string
		if(TERMARGS(0, "set_config"))
		{
			NodeId receiver = (TERMARGS(1,"this")) ? GS->node.configuration.nodeId : Utility::StringToU16(commandArgs[1]);
			if (commandArgsSize < 4) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
			//calculate configuration size
			const char* configString = commandArgs[3];
			u16 configLength = (u16) (strlen(commandArgs[3])+1)/3;

			u8 requestHandle = commandArgsSize >= 5 ? Utility::StringToU8(commandArgs[4]) : 0;

			//Send the configuration to the destination node
			DYNAMIC_ARRAY(packetBuffer, configLength + SIZEOF_CONN_PACKET_MODULE);
			connPacketModule* packet = (connPacketModule*)packetBuffer;
			packet->header.messageType = MessageType::MODULE_CONFIG;
			packet->header.sender = GS->node.configuration.nodeId;
			packet->header.receiver = receiver;

			packet->actionType = (u8)ModuleConfigMessages::SET_CONFIG;
			packet->moduleId = moduleId;
			packet->requestHandle = requestHandle;
			//Fill data region with module config
			Logger::parseEncodedStringToBuffer(configString, packet->data, configLength);

			GS->cm.SendMeshMessage(packetBuffer, configLength + SIZEOF_CONN_PACKET_MODULE, DeliveryPriority::LOW);

			return TerminalCommandHandlerReturnType::WARN_DEPRECATED; //Deprecated as of 17.01.2020
		}
		else if(TERMARGS(0,"get_config"))
		{
			NodeId receiver = (TERMARGS(1, "this")) ? GS->node.configuration.nodeId : Utility::StringToU16(commandArgs[1]);
			connPacketModule packet;
			packet.header.messageType = MessageType::MODULE_CONFIG;
			packet.header.sender = GS->node.configuration.nodeId;
			packet.header.receiver = receiver;

			packet.moduleId = moduleId;
			packet.actionType = (u8)ModuleConfigMessages::GET_CONFIG;

			GS->cm.SendMeshMessage((u8*)&packet, SIZEOF_CONN_PACKET_MODULE, DeliveryPriority::LOW);

			return TerminalCommandHandlerReturnType::WARN_DEPRECATED; //Deprecated as of 17.01.2020
		}
		else if(TERMARGS(0,"set_active"))
		{
			NodeId receiver = (TERMARGS(1, "this")) ? GS->node.configuration.nodeId : Utility::StringToU16(commandArgs[1]);
			if(commandArgsSize <= 3) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;

			u8 moduleState = TERMARGS(3,"on") ? 1: 0;
			u8 requestHandle = commandArgsSize >= 5 ? Utility::StringToU8(commandArgs[4]) : 0;

			connPacketModule packet;
			packet.header.messageType = MessageType::MODULE_CONFIG;
			packet.header.sender = GS->node.configuration.nodeId;
			packet.header.receiver = receiver;

			packet.moduleId = moduleId;
			packet.actionType = (u8)ModuleConfigMessages::SET_ACTIVE;
			packet.requestHandle = requestHandle;
			packet.data[0] = moduleState;

			GS->cm.SendMeshMessage((u8*) &packet, SIZEOF_CONN_PACKET_MODULE + 1, DeliveryPriority::LOW);

			return TerminalCommandHandlerReturnType::SUCCESS;
		}
	}

	return TerminalCommandHandlerReturnType::UNKNOWN;
}
#endif

void Module::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader const * packetHeader)
{
	//We want to handle incoming packets that change the module configuration
	if(
			packetHeader->messageType == MessageType::MODULE_CONFIG
	){
		connPacketModule const * packet = (connPacketModule const *) packetHeader;
		if(packet->moduleId == moduleId)
		{

			u16 dataFieldLength = sendData->dataLength - SIZEOF_CONN_PACKET_MODULE;
			ModuleConfigMessages actionType = (ModuleConfigMessages)packet->actionType;
			if(actionType == ModuleConfigMessages::SET_CONFIG)
			{
				//Check if this config seems right
				ModuleConfiguration const * newConfig = (ModuleConfiguration const *)packet->data;
				if(
						newConfig->moduleVersion == configurationPointer->moduleVersion
						&& dataFieldLength == configurationLength
				){
					//Backup the module id because the provided moduleId in the payload might not be set.
					ModuleId moduleId = configurationPointer->moduleId;
					CheckedMemset(configurationPointer, 0x00, configurationLength);
					CheckedMemcpy(configurationPointer, packet->data, dataFieldLength);
					configurationPointer->moduleId = moduleId;

					//Call the configuration loaded handler to reinitialize stuff if necessary (RAM config is already set)
					ConfigurationLoadedHandler(nullptr, 0);

					//Save the module config to flash
					SaveModuleConfigAction userData;
					userData.moduleId = moduleId;
					userData.sender = packetHeader->sender;
					userData.requestHandle = packet->requestHandle;

					Utility::SaveModuleSettingsToFlash(
						this,
						this->configurationPointer,
						this->configurationLength,
						this,
						(u8)ModuleSaveAction::SAVE_MODULE_CONFIG_ACTION,
						(u8*)&userData,
						sizeof(SaveModuleConfigAction));
				}
				else
				{
					if(newConfig->moduleVersion != configurationPointer->moduleVersion) logjson("ERROR", "{\"type\":\"error\",\"module\":%u,\"code\":1,\"text\":\"wrong config version.\"}" SEP, (u32)moduleId);
					else logjson("ERROR", "{\"type\":\"error\",\"module\":%u,\"code\":2,\"text\":\"wrong config length %u instead of %u \"}" SEP, (u32)moduleId, dataFieldLength, configurationLength);
				}
			}
			else if(actionType == ModuleConfigMessages::GET_CONFIG)
			{
				DYNAMIC_ARRAY(buffer, SIZEOF_CONN_PACKET_MODULE + configurationLength);

				connPacketModule* outPacket = (connPacketModule*)buffer;
				outPacket->header.messageType = MessageType::MODULE_CONFIG;
				outPacket->header.sender = GS->node.configuration.nodeId;
				outPacket->header.receiver = packet->header.sender;

				outPacket->moduleId = moduleId;
				outPacket->requestHandle = packet->requestHandle;
				outPacket->actionType = (u8)ModuleConfigMessages::CONFIG;

				CheckedMemcpy(outPacket->data, (u8*)configurationPointer, configurationLength);

				GS->cm.SendMeshMessage(buffer, SIZEOF_CONN_PACKET_MODULE + configurationLength, DeliveryPriority::LOW);
			}
			else if(actionType == ModuleConfigMessages::SET_ACTIVE)
			{
				SetActiveReturnValues retVal = SetActiveReturnValues::NO_SUCH_MODULE;

				//Look for the module and set it active or inactive
				for(u32 i=0; i< GS->amountOfModules; i++){
					if(GS->activeModules[i]->moduleId == packet->moduleId)
					{
						GS->activeModules[i]->configurationPointer->moduleActive = packet->data[0];
						//Reinitialize the module
						GS->activeModules[i]->ConfigurationLoadedHandler(nullptr, 0);

						if (configurationPointer == nullptr)
						{
							retVal = SetActiveReturnValues::NO_CONFIGURATION;
						}
						else
						{
							RecordStorageResultCode err = Utility::SaveModuleSettingsToFlashWithId(moduleId, configurationPointer, configurationLength, nullptr, 0, nullptr, 0);
							if (err != RecordStorageResultCode::SUCCESS)
							{
								retVal = SetActiveReturnValues::RECORD_STORAGE_ERROR;
							}
							else
							{
								retVal = SetActiveReturnValues::SUCCESS;
							}
						}

						break;
					}
				}

				//Send confirmation that the modules activity state changed
				connPacketModule outPacket;
				CheckedMemset(&outPacket, 0, sizeof(outPacket));
				outPacket.header.messageType = MessageType::MODULE_CONFIG;
				outPacket.header.sender = GS->node.configuration.nodeId;
				outPacket.header.receiver = packet->header.sender;

				outPacket.moduleId = moduleId;
				outPacket.requestHandle = packet->requestHandle;
				outPacket.actionType = (u8)ModuleConfigMessages::SET_ACTIVE_RESULT;
				outPacket.data[0] = (u8)retVal;

				GS->cm.SendMeshMessage((u8*)&outPacket, SIZEOF_CONN_PACKET_MODULE + 1, DeliveryPriority::LOW);
			}


			/*
			 * ######################### RESPONSES
			 * */
			if(actionType == ModuleConfigMessages::SET_CONFIG_RESULT)
			{
				logjson_partial("MODULE", "{\"nodeId\":%u,\"type\":\"set_config_result\",\"module\":%u,", packet->header.sender, (u32)packet->moduleId);
				logjson("MODULE",  "\"requestHandle\":%u,\"code\":%u}" SEP, packet->requestHandle, packet->data[0]);
			}
			else if(actionType == ModuleConfigMessages::SET_ACTIVE_RESULT)
			{
				logjson_partial("MODULE", "{\"nodeId\":%u,\"type\":\"set_active_result\",\"module\":%u,", packet->header.sender, (u32)packet->moduleId);
				logjson("MODULE",  "\"requestHandle\":%u,\"code\":%u}" SEP, packet->requestHandle, packet->data[0]);
			}
			else if(actionType == ModuleConfigMessages::CONFIG)
			{
				char buffer[200];
				Logger::convertBufferToHexString(packet->data, dataFieldLength, buffer, sizeof(buffer));

				logjson("MODULE", "{\"nodeId\":%u,\"type\":\"config\",\"module\":%u,\"config\":\"%s\"}" SEP, packet->header.sender, (u32)moduleId, buffer);


			}
		}
	}
}

PreEnrollmentReturnCode Module::PreEnrollmentHandler(connPacketModule* packet, u16 packetLength)
{
#if IS_ACTIVE(ACTIVATE_MODULE_CONFIG_REMOVAL_DURING_ENROLLMENT)
	//The default PreEnrollmentHandler will remove the module configuration if available
	logt("MODULE", "Removing config for module %u", (u32)moduleId);

	SizedData configRecord = GS->recordStorage.GetRecordData((u16)moduleId);
	if(configRecord.data == nullptr)
	{
		//Config not present, nothing to do, pre enrollment done
		return PreEnrollmentReturnCode::DONE;
	}
	else
	{
		//Delete our configuration record and
		RecordStorageResultCode err = GS->recordStorage.DeactivateRecord((u16)moduleId, this, (u32)ModuleSaveAction::PRE_ENROLLMENT_RECORD_DELETE);

		if(err == RecordStorageResultCode::SUCCESS) {
			return PreEnrollmentReturnCode::WAITING;

			// => Now we wait for the flash operation to succeed or fail
		} else {
			return PreEnrollmentReturnCode::FAILED;
		}
	}
#else
	return PreEnrollmentReturnCode::DONE;
#endif
}


void Module::RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength)
{
	if (userType == (u8)ModuleSaveAction::SAVE_MODULE_CONFIG_ACTION) {

		SaveModuleConfigAction* data = (SaveModuleConfigAction*)userData;

		//Send set_config_ok message
		connPacketModule outPacket;
		outPacket.header.messageType = MessageType::MODULE_CONFIG;
		outPacket.header.sender = GS->node.configuration.nodeId;
		outPacket.header.receiver = data->sender;

		outPacket.moduleId = data->moduleId;
		outPacket.requestHandle = data->requestHandle;
		outPacket.actionType = (u8)Module::ModuleConfigMessages::SET_CONFIG_RESULT;
		outPacket.data[0] = (u8)resultCode;

		GS->cm.SendMeshMessage((u8*)&outPacket, SIZEOF_CONN_PACKET_MODULE + 1, DeliveryPriority::LOW);
	}
	else if(userType == (u8)ModuleSaveAction::PRE_ENROLLMENT_RECORD_DELETE)
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
