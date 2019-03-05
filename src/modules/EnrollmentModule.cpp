////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2019 M-Way Solutions GmbH
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

#include <EnrollmentModule.h>

#ifdef ACTIVATE_ENROLLMENT_MODULE

#include <Logger.h>
#include <Utility.h>
#include <Node.h>
#include <IoModule.h>
#include <ScanController.h>
#ifdef ACTIVATE_MA_MODULE
#include <MeshAccessModule.h>
#endif

extern "C"{
#include <stdlib.h>
#ifndef SIM_ENABLED
#include <nrf_nvic.h>
#endif
}

/*
Module purpose:
After a node has been flashed, it is in an unconfigured state
This module should allow configuration of network id, network key, NodeId and other necessary parameters
 */

#define ENROLLMENT_MODULE_CONFIG_VERSION 1

#define ENROLLMENT_MODULE_PRE_ENROLLMENT_TIMEOUT_DS 60


EnrollmentModule::EnrollmentModule()
	: Module(moduleID::ENROLLMENT_MODULE_ID, "enroll")
{
	moduleVersion = ENROLLMENT_MODULE_CONFIG_VERSION;
	proposalIndexCounter = 0;

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(EnrollmentModuleConfiguration);

	//Set defaults
	ResetToDefaultConfiguration();
}

void EnrollmentModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = true;
	configuration.moduleVersion = ENROLLMENT_MODULE_CONFIG_VERSION;

	//Set additional config values...
	configuration.buttonRemoveEnrollmentDs = 0;

	SET_FEATURESET_CONFIGURATION(&configuration);
}

void EnrollmentModule::ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength)
{
	//Do additional initialization upon loading the config
	memset(&ted, 0x00, sizeof(TemporaryEnrollmentData));
	memset(&proposal, 0x00, sizeof(EnrollmentModuleEnrollmentProposalMessage));
	proposalIndexCounter = 0;

	//Start the Module...

}

void EnrollmentModule::TimerEventHandler(u16 passedTimeDs)
{
	//Check if a PreEnrollment should time out
	if(ted.state == EnrollmentStates::PREENROLLMENT_RUNNING && GS->appTimerDs > ted.endTimeDs){
		logt("ENROLLMOD", "PreEnrollment timed out");

		PreEnrollmentFailed();
	}

#ifdef ACTIVATE_MA_MODULE

	MeshAccessConnection* conn = nullptr;
	if(ted.state > EnrollmentStates::PREENROLLMENT_RUNNING) conn = (MeshAccessConnection*)GS->cm->GetConnectionByUniqueId(ted.uniqueConnId);

	//Check if the current enrollment over the mesh should time out
	if (ted.state > EnrollmentStates::PREENROLLMENT_RUNNING && GS->appTimerDs > ted.endTimeDs) {
		logt("ENROLLMOD", "Enrollment over mesh timed out");

		//Check if the current enrollment over a mesh access connection should time out
		if (ted.state == EnrollmentStates::SCANNING) {
			//We do not stop the scanner as the node also needs it //TODO: scanning should be stopped once we have a scanController
		} else if (ted.state == EnrollmentStates::CONNECTING) {
			//Stop connecting and ensure that if the connection was already made by the softdevice, we do not accept it
			FruityHal::ConnectCancel();
			if (GS->cm->pendingConnection != nullptr) {
				GS->cm->pendingConnection->appDisconnectionReason = AppDisconnectReason::ENROLLMENT_TIMEOUT;
				GS->cm->DeleteConnection(GS->cm->pendingConnection);
			}
		}
		else if(ted.state >= EnrollmentStates::CONNECTED)
		{
			if(conn != nullptr){
				conn->appDisconnectionReason = AppDisconnectReason::ENROLLMENT_TIMEOUT2;
				conn->DisconnectAndRemove();
			}
		}

		ted.state = EnrollmentStates::NOT_ENROLLING;
	}

	//Check if the enrollment connection was handshaked as we have no handler for that
	if(ted.state == EnrollmentStates::CONNECTING && conn != nullptr) {
		if(conn->connectionState == ConnectionState::HANDSHAKE_DONE) {
			EnrollmentConnectionConnectedHandler();
		}
	}
#endif
}

#ifdef TERMINAL_ENABLED
bool EnrollmentModule::TerminalCommandHandler(char* commandArgs[], u8 commandArgsSize)
{
	//React on commands, return true if handled, false otherwise
	if(commandArgsSize >= 3 && TERMARGS(2 ,moduleName))
	{
		NodeId receiver = (TERMARGS(1,"this")) ? GS->node->configuration.nodeId : atoi(commandArgs[1]);


		if(TERMARGS(0 ,"action"))
		{
			//Old enrollment method for compatibility reasons
			if( commandArgsSize >= 7 && TERMARGS(3,"serial"))
			{
				EnrollmentModuleSetEnrollmentBySerialMessage enrollmentMessage;
				memset(&enrollmentMessage, 0x00, SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_SERIAL_MESSAGE);

				enrollmentMessage.serialNumberIndex = Utility::GetIndexForSerial(commandArgs[4]);
				enrollmentMessage.newNodeId = atoi(commandArgs[5]);
				enrollmentMessage.newNetworkId = atoi(commandArgs[6]);
				if(commandArgsSize > 7){
					GS->logger->parseHexStringToBuffer(commandArgs[7], enrollmentMessage.newNetworkKey.getRaw(), 16);
				}
				u8 requestHandle = commandArgsSize > 8 ? atoi(commandArgs[8]) : 0;

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					receiver,
					(u8)EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT_BY_SERIAL,
					requestHandle,
					(u8*)&enrollmentMessage,
					SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_SERIAL_MESSAGE,
					false
				);

				return true;
			}
			//Enroll by serial number
			else if(commandArgsSize >= 7 && TERMARGS(3,"basic"))
			{
				EnrollmentModuleSetEnrollmentBySerialMessage enrollmentMessage;
				memset(&enrollmentMessage, 0x00, SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_SERIAL_MESSAGE);

				enrollmentMessage.serialNumberIndex = Utility::GetIndexForSerial(commandArgs[4]);
				enrollmentMessage.newNodeId = atoi(commandArgs[5]);
				enrollmentMessage.newNetworkId = atoi(commandArgs[6]);
				if(commandArgsSize > 7){
					GS->logger->parseHexStringToBuffer(commandArgs[7], enrollmentMessage.newNetworkKey.getRaw(), 16);
				}
				if(commandArgsSize > 8){
					GS->logger->parseHexStringToBuffer(commandArgs[8], enrollmentMessage.newUserBaseKey.getRaw(), 16);
				}
				if(commandArgsSize > 9){
					GS->logger->parseHexStringToBuffer(commandArgs[9], enrollmentMessage.newOrganizationKey.getRaw(), 16);
				}
				if(commandArgsSize > 10){
					GS->logger->parseHexStringToBuffer(commandArgs[10], enrollmentMessage.nodeKey.getRaw(), 16);
				}
				enrollmentMessage.timeoutSec = commandArgsSize > 11? atoi(commandArgs[11]) : 10;
				enrollmentMessage.enrollOnlyIfUnenrolled = commandArgsSize > 12 ? atoi(commandArgs[12]) : 0;
				u8 requestHandle = commandArgsSize > 13 ? atoi(commandArgs[13]) : 0;

				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					receiver,
					(u8)EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT_BY_SERIAL,
					requestHandle,
					(u8*)&enrollmentMessage,
					SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_SERIAL_MESSAGE,
					false
				);

				return true;
			} else if(commandArgsSize > 4 && TERMARGS(3, "remove"))
			{
				EnrollmentModuleRemoveEnrollmentMessage message;
				memset(&message, 0x00, SIZEOF_ENROLLMENT_MODULE_REMOVE_ENROLLMENT);

				message.serialNumberIndex = Utility::GetIndexForSerial(commandArgs[4]);
				u8 requestHandle = commandArgsSize > 5 ? atoi(commandArgs[5]) : 0;


				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
					receiver,
					(u8)EnrollmentModuleTriggerActionMessages::REMOVE_ENROLLMENT,
					requestHandle,
					(u8*)&message,
					SIZEOF_ENROLLMENT_MODULE_REMOVE_ENROLLMENT,
					false
				);

				return true;
			}
		}
	}

	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif

void EnrollmentModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader)
{
	//Must call superclass for handling
	Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId)
		{
			EnrollmentModuleTriggerActionMessages actionType = (EnrollmentModuleTriggerActionMessages)packet->actionType;
			//Enrollment request
			if(actionType == EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT_BY_SERIAL
				&& sendData->dataLength >= SIZEOF_CONN_PACKET_MODULE + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_SERIAL_MESSAGE_MIN)
			{
				EnrollmentModuleSetEnrollmentBySerialMessage* data = (EnrollmentModuleSetEnrollmentBySerialMessage*)packet->data;

				//Local enrollment
				if(data->serialNumberIndex == RamConfig->serialNumberIndex)
				{
					Enroll(packet, sendData->dataLength);
				}
				//Enrollment overmesh
				else {
					EnrollOverMesh(packet, sendData->dataLength);
				}
			//Unenrollment request
			} else if(actionType == EnrollmentModuleTriggerActionMessages::REMOVE_ENROLLMENT)
			{
				EnrollmentModuleRemoveEnrollmentMessage* data = (EnrollmentModuleRemoveEnrollmentMessage*)packet->data;

				if(data->serialNumberIndex == RamConfig->serialNumberIndex)
				{

					Unenroll(packet, sendData->dataLength);
				}
			}
		}
	}

	//Parse Module responses
	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_ACTION_RESPONSE){
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId)
		{
			EnrollmentModuleActionResponseMessages actionType = (EnrollmentModuleActionResponseMessages)packet->actionType;
			if(
					actionType == EnrollmentModuleActionResponseMessages::ENROLLMENT_RESPONSE
					|| actionType == EnrollmentModuleActionResponseMessages::REMOVE_ENROLLMENT_RESPONSE
			){
				EnrollmentModuleEnrollmentResponse* data = (EnrollmentModuleEnrollmentResponse*)packet->data;

				//Add null terminator to string
				char serialNumber[NODE_SERIAL_NUMBER_LENGTH+1];
				Utility::GenerateBeaconSerialForIndex(data->serialNumberIndex, serialNumber);

				//Check if this came over our EnrollmentOverMesh Connection and terminate that connection
				if(ted.state >= EnrollmentStates::CONNECTED && connection != nullptr && ted.uniqueConnId == connection->uniqueConnectionId){
					ted.state = EnrollmentStates::NOT_ENROLLING;
					connection->DisconnectAndRemove();
				}

				const char* cmdName = packet->actionType == (u8)EnrollmentModuleActionResponseMessages::ENROLLMENT_RESPONSE ? "enroll_response_serial" : "remove_enroll_response_serial";

				logjson("ENROLLMOD", "{\"nodeId\":%u,\"type\":\"%s\",\"module\":%d,", packet->header.sender, cmdName, moduleId);
				logjson("ENROLLMOD", "\"requestId\":%u,\"serialNumber\":\"%s\",\"code\":%u}" SEP,  packet->requestHandle, serialNumber, data->result);
			}
			else if(actionType == EnrollmentModuleActionResponseMessages::ENROLLMENT_PROPOSAL)
			{
				EnrollmentModuleEnrollmentProposalMessage* data = (EnrollmentModuleEnrollmentProposalMessage*)packet->data;

				logjson("ENROLLMOD", "{\"nodeId\":%u,\"type\":\"enroll_proposal\",\"module\":%d,", packet->header.sender, moduleId);
				logjson("ENROLLMOD", "\"proposals\":[%u,%u,%u]}" SEP, data->serialNumberIndex[0], data->serialNumberIndex[1], data->serialNumberIndex[2]);
			}
		}
	}
}


#define _____________LOCAL_ENROLLMENT_____________

void EnrollmentModule::Enroll(connPacketModule* packet, u16 packetLength)
{
	EnrollmentModuleSetEnrollmentBySerialMessage* data = (EnrollmentModuleSetEnrollmentBySerialMessage*)packet->data;


	logt("WARNING", "Enrollment (by serial) received nodeId:%u, networkid:%u, key[0]=%u, key[1]=%u, key[14]=%u, key[15]=%u", data->newNodeId, data->newNetworkId, data->newNetworkKey[0], data->newNetworkKey[1], data->newNetworkKey[14], data->newNetworkKey[15]);

	//If enrollment is the same, we respond with OK, no reboot necessary
	if(
		GS->node->configuration.enrollmentState == EnrollmentState::ENROLLED
		&& GS->node->configuration.nodeId == data->newNodeId
		&& GS->node->configuration.networkId == data->newNetworkId
		&& (CHECK_MSG_SIZE(packet, data->newNetworkKey.getRaw(), 16, packetLength) && memcmp(data->newNetworkKey.getRaw(), GS->node->configuration.networkKey, 16) == 0)
		&& (CHECK_MSG_SIZE(packet, data->newUserBaseKey.getRaw(), 16, packetLength) && memcmp(data->newUserBaseKey.getRaw(), GS->node->configuration.userBaseKey, 16) == 0)
		&& (CHECK_MSG_SIZE(packet, data->newOrganizationKey.getRaw(), 16, packetLength) && memcmp(data->newOrganizationKey.getRaw(), GS->node->configuration.organizationKey, 16) == 0)
	){
		// Already enrolled with same data, send ok, do not reboot
		SendEnrollmentResponse(
				EnrollmentModuleActionResponseMessages::ENROLLMENT_RESPONSE,
				NODE_ID_BROADCAST, //To be sure because own node id changes
				RamConfig->serialNumberIndex,
				ENROLL_RESPONSE_OK,
				packet->requestHandle);

		return;
	}

	//In case we should only enroll if not already enrolled
	if(data->enrollOnlyIfUnenrolled
		&& GS->node->configuration.enrollmentState == EnrollmentState::ENROLLED
	){
		// Node is enrolled with different data, send an error
		SendEnrollmentResponse(
				EnrollmentModuleActionResponseMessages::ENROLLMENT_RESPONSE,
				NODE_ID_BROADCAST, //To be sure because own node id changes
				RamConfig->serialNumberIndex,
				ENROLL_RESPONSE_ALREADY_ENROLLED_WITH_DIFFERENT_DATA,
				packet->requestHandle);

		return;
	}

	StoreTemporaryEnrollmentDataAndDispatch(packet, packetLength);

}

void EnrollmentModule::SaveEnrollment(connPacketModule* packet, u16 packetLength)
{
	EnrollmentModuleSetEnrollmentBySerialMessage* data = (EnrollmentModuleSetEnrollmentBySerialMessage*)packet->data;

	//First, clear all settings that are stored on the chip
	//GS->recordStorage->ClearAllSettings();

	//Save values to persistent config of the node
	GS->node->configuration.enrollmentState = EnrollmentState::ENROLLED;
	if(data->newNodeId != 0) GS->node->configuration.nodeId = data->newNodeId;
	GS->node->configuration.networkId = data->newNetworkId;

	if(
		CHECK_MSG_SIZE(packet, data->newNetworkKey.getRaw(), 16, packetLength)
		&& !Utility::CompareMem(0x00, data->newNetworkKey.getRaw(), 16)
	){
		memcpy(GS->node->configuration.networkKey, data->newNetworkKey.getRaw(), 16);
	}
	if(
		CHECK_MSG_SIZE(packet, data->newUserBaseKey.getRaw(), 16, packetLength)
		&& !Utility::CompareMem(0x00, data->newUserBaseKey.getRaw(), 16)
	){
		memcpy(GS->node->configuration.userBaseKey, data->newUserBaseKey.getRaw(), 16);
	}
	if(
		CHECK_MSG_SIZE(packet, data->newOrganizationKey.getRaw(), 16, packetLength)
		&& !Utility::CompareMem(0x00, data->newOrganizationKey.getRaw(), 16)
	){
		memcpy(GS->node->configuration.organizationKey, data->newOrganizationKey.getRaw(), 16);
	}

	//Stop advertising until reboot
	#ifdef ACTIVATE_MA_MODULE

	MeshAccessModule* maModule = (MeshAccessModule*)GS->node->GetModuleById(moduleID::MESH_ACCESS_MODULE_ID);
	if (maModule != nullptr) {
		maModule->DisableBroadcast();
	}

	//Stop advertising immediately
	GS->advertisingController->DeleteAllJobs();

	#endif

	//Cache some of the data in a struct to have it available when it is saved
	SaveEnrollmentAction userData;
	userData.sender = packet->header.sender;
	userData.requestHandle = packet->requestHandle;

	//Save the new node config to flash
	GS->recordStorage->SaveRecord(
		moduleID::NODE_ID,
		(u8*)&(GS->node->configuration),
		sizeof(NodeConfiguration),
		this,
		(u32)EnrollmentModuleSaveActions::SAVE_ENROLLMENT_ACTION,
		(u8*)&userData,
		sizeof(SaveEnrollmentAction));
}

void EnrollmentModule::Unenroll(connPacketModule* packet, u16 packetLength)
{
	logt("ENROLLMOD", "Unenrolling");

	//If we are already unenrolled, we respond with OK, no reboot necessary
	if(
		GS->node->configuration.enrollmentState == EnrollmentState::NOT_ENROLLED
	){
		// Already enrolled with same data, send ok, do not reboot
		SendEnrollmentResponse(
				EnrollmentModuleActionResponseMessages::REMOVE_ENROLLMENT_RESPONSE,
				NODE_ID_BROADCAST, //To be sure because own node id changes
				RamConfig->serialNumberIndex,
				ENROLL_RESPONSE_OK,
				packet->requestHandle);

		return;
	}

	StoreTemporaryEnrollmentDataAndDispatch(packet, packetLength);
}

void EnrollmentModule::SaveUnenrollment(connPacketModule* packet, u16 packetLength)
{

	GS->node->configuration.enrollmentState = EnrollmentState::NOT_ENROLLED;
	GS->node->configuration.deviceType = RamConfig->deviceType;
	GS->node->configuration.networkId = RamConfig->defaultNetworkId;
	memcpy(GS->node->configuration.networkKey, RamConfig->defaultNetworkKey, 16);
	GS->node->configuration.nodeId = RamConfig->defaultNodeId;
	//Reset userBaseKey to 00, we could also save a default userBaseKey in the UICR,....
	memset(GS->node->configuration.userBaseKey, 0x00, 16);
	memset(GS->node->configuration.organizationKey, 0x00, 16);

		//Stop advertising until reboot
	#ifdef ACTIVATE_MA_MODULE
		MeshAccessModule* maModule = (MeshAccessModule*)GS->node->GetModuleById(moduleID::MESH_ACCESS_MODULE_ID);
		if (maModule != nullptr) {
			maModule->DisableBroadcast();
		}
	#endif

		//Cache some of the data in a struct to have it available when it is saved
		SaveEnrollmentAction userData;
		userData.sender = packet->header.sender;
		userData.requestHandle = packet->requestHandle;

		//Save the new node config to flash
		GS->recordStorage->SaveRecord(
			moduleID::NODE_ID,
			(u8*)&(GS->node->configuration),
			sizeof(NodeConfiguration),
			this,
			(u32)EnrollmentModuleSaveActions::SAVE_REMOVE_ENROLLMENT_ACTION,
			(u8*)&userData,
			sizeof(SaveEnrollmentAction));
}

#define _____________PRE_ENROLLMENT_____________

void EnrollmentModule::StoreTemporaryEnrollmentDataAndDispatch(connPacketModule* packet, u16 packetLength)
{
	//Before we can call other modules to tell them that we want to enroll the node,
	//we have to temporarily save the enrollment data until the other module has answered
	ted.state = EnrollmentStates::PREENROLLMENT_RUNNING;
	ted.endTimeDs = GS->appTimerDs + ENROLLMENT_MODULE_PRE_ENROLLMENT_TIMEOUT_DS;
	ted.packetLength = packetLength;
	memcpy(&ted.requestHeader, packet, packetLength);

	DispatchPreEnrollment(nullptr, PreEnrollmentReturnCode::PRE_ENROLLMENT_DONE);
}

void EnrollmentModule::DispatchPreEnrollment(Module* lastModuleCalled, PreEnrollmentReturnCode lastStatus)
{
	u8 moduleIndex = 0;

	//If the PreEnrollment was already done for some modules, we continue with the next module in line
	if(lastModuleCalled != nullptr)
	{
		//Check the last status that was given, if it was not ok, the PreEnrollmentFailed
		if(lastStatus != PreEnrollmentReturnCode::PRE_ENROLLMENT_DONE){
			PreEnrollmentFailed();
			return;
		}

		for(int i=0; i<MAX_MODULE_COUNT; i++){
			if(GS->activeModules[i] == lastModuleCalled){
				moduleIndex = i + 1;
			}
		}
	}

	//Go through all remaining modules and call the PreEnrollmentHandler and evaluate its response
	for (u32 i = moduleIndex; i < MAX_MODULE_COUNT; i++) {
		if (GS->activeModules[i] == nullptr || !GS->activeModules[i]->configurationPointer->moduleActive) continue;

		u32 err = GS->activeModules[i]->PreEnrollmentHandler(&ted.requestHeader, ted.packetLength);

		if (err == PRE_ENROLLMENT_WAITING) {
			// => The module must call the DispatchPreEnrollment function after it has received the result
			return;
		}
		else if (err == PRE_ENROLLMENT_FAILED) {
			PreEnrollmentFailed();
			return;
		}
	}

	//The PreEnrollment might already have timed out
	if(ted.state != EnrollmentStates::PREENROLLMENT_RUNNING) return;

	logt("ENROLLMOD", "PreEnrollment succeeded");


	// Set to not enrolling to ensure that the PreEnrollmentTimeout doesn't trigger in the meantime
	ted.state = EnrollmentStates::NOT_ENROLLING;

	if(ted.requestHeader.actionType == (u8)EnrollmentModuleTriggerActionMessages::SET_ENROLLMENT_BY_SERIAL){
		SaveEnrollment(&ted.requestHeader, ted.packetLength);
	}
	else if(ted.requestHeader.actionType == (u8)EnrollmentModuleTriggerActionMessages::REMOVE_ENROLLMENT)
	{
		SaveUnenrollment(&ted.requestHeader, ted.packetLength);
	}
}

void EnrollmentModule::PreEnrollmentFailed()
{
	logt("ENROLLMOD", "PreEnrollment failed");

	//Clear ted data
	memset(&ted, 0x00, sizeof(TemporaryEnrollmentData));
	ted.state = EnrollmentStates::NOT_ENROLLING;

	//Send a response to the enroller that the preenrollment failed
	SendEnrollmentResponse(
			EnrollmentModuleActionResponseMessages::ENROLLMENT_RESPONSE,
			NODE_ID_BROADCAST, //To be sure because own node id changes
			RamConfig->serialNumberIndex,
			ENROLL_RESPONSE_PREENROLLMENT_FAILED,
			ted.requestHeader.requestHandle);
}


#define _____________ENROLLMENT_OVER_MESH_____________

void EnrollmentModule::EnrollOverMesh(connPacketModule* packet, u16 packetLength)
{
	logt("ENROLLMOD", "Received Enrollment over the mesh request");

	EnrollmentModuleSetEnrollmentBySerialMessage* data = (EnrollmentModuleSetEnrollmentBySerialMessage*)packet->data;


	u32 rand = Utility::GetRandomInteger();

	//If no node key was given, don't attempt to enroll over a meshaccess connection
	if(
		!CHECK_MSG_SIZE(packet, data->nodeKey.getRaw(), 16, packetLength)
		|| Utility::CompareMem(0x00, data->nodeKey.getRaw(), 16)
	){
		logt("ENROLLMOD", "No node key given");
		return;
	}

	//At a 10% chance we send an enrollment proposal
	if(rand < UINT32_MAX/10){
		SendModuleActionMessage(
			MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
			packet->header.sender,
			(u8)EnrollmentModuleActionResponseMessages::ENROLLMENT_PROPOSAL,
			0,
			(u8*)&proposal,
			sizeof(EnrollmentModuleEnrollmentProposalMessage),
			true
		);
	}


	//Check if another enrollment is in progress, if yes, return
	if(ted.state != EnrollmentStates::NOT_ENROLLING){
		logt("ENROLLMOD", "Still busy");
		return;
	}

	//If the enrollment was directed to broadcast, only perform the request at a
	//chance of 50% => Helps us to not block all nodes in a mesh at once
	if(packet->header.receiver == NODE_ID_BROADCAST && GS->node->clusterSize > 1 && rand < UINT32_MAX/2){
		logt("ENROLLMOD", "Random skip");
		return;
	}

	//If there are any open MeshAccessConnections, we disconnect these so we can use them after
	//our scan returned a positive result. This will throw out users that try to connect during an
	//enrollment, but we cannot easily distinguish between used and unused meshAccessConnections
	BaseConnections conns = GS->cm->GetConnectionsOfType(ConnectionTypes::CONNECTION_TYPE_MESH_ACCESS, ConnectionDirection::INVALID);
	for(u32 i=0; i<conns.count; i++){
		BaseConnection *conn = GS->cm->allConnections[conns.connectionIndizes[i]];
		if (conn != nullptr) {
			conn->DisconnectAndRemove();
		}
	}

	//Save the enrollment data
	//If this data is saved, we will check incoming advertisements if they match
	//If yes, we will connect to the other node and try to enroll it
	memset(&ted, 0x00, sizeof(TemporaryEnrollmentData));
	ted.requestHeader = *packet;
	ted.requestData = *data;

	//Set timeout time for enrollment
	ted.endTimeDs = GS->appTimerDs + SEC_TO_DS(data->timeoutSec);

	//Start scanning for mesh access packets
	//TODO: Should use a scancontroller that allows job handling
	ted.state = EnrollmentStates::SCANNING;
	GS->scanController->SetScanState(scanState::SCAN_STATE_HIGH);

	//=> Next, we simple wait for the timeout or if a handler is called with a matching advertisement

	//TODO: Pay attention that scanning and the meshaccess connection are killed even if we could not enroll the node
}

#ifdef ACTIVATE_MA_MODULE
//This is triggered once we receive an advertising of a node that should be enrolled over the mesh
void EnrollmentModule::EnrollNodeViaMeshAccessConnection(fh_ble_gap_addr_t& addr, const meshAccessServiceAdvMessage* advMessage)
{
	if(ted.state != EnrollmentStates::SCANNING) return;

	logt("ENROLLMOD", "Received message from beacon to be enrolled");

	logt("ENROLLMOD", "Connecting to %02X:%02X:%02X:%02X:%02X:%02X",
			addr.addr[5],addr.addr[4],addr.addr[3],addr.addr[2],addr.addr[1],addr.addr[0]);

	//Check if we still have enough time for connecting
	if(GS->appTimerDs + SEC_TO_DS(2) > ted.endTimeDs) return;

	//TODO: Build Mesh access connection, remove hardcoded values
	u16 timeLeftSec = ted.endTimeDs - GS->appTimerDs;

	//Try to connect to the device using a MeshAccess Connection
	//TODO: replace hardcoded value
	ted.state = EnrollmentStates::CONNECTING;
	ted.uniqueConnId = MeshAccessConnection::ConnectAsMaster(&addr, 10, timeLeftSec, FM_KEY_ID_NODE, ted.requestData.nodeKey.getRaw(), MeshAccessTunnelType::PEER_TO_PEER);

	logt("ENROLLMOD", "uiniqueId: %u", ted.uniqueConnId);

	//Now, we use our Timer handler to check if the Connection reaches the handshake state
}
#endif

void EnrollmentModule::EnrollmentConnectionConnectedHandler()
{
#ifdef ACTIVATE_MA_MODULE
	logt("ENROLLMOD", "Enrollment Connection handshaked");

	ted.state = EnrollmentStates::CONNECTED;
	//Increase timeout if we do not have enough time to send the enrollment
	if(GS->appTimerDs + SEC_TO_DS(4) > ted.endTimeDs){
		ted.endTimeDs = GS->appTimerDs + SEC_TO_DS(4);
	}


	MeshAccessConnection* conn = (MeshAccessConnection*)GS->cm->GetConnectionByUniqueId(ted.uniqueConnId);
	
	//We need to overwrite the receiver as our node might have been instructed to enroll the remote node
	//If we send the message unmodified, our partner would not accept the packet as it was not adressed to him
	ted.requestHeader.header.receiver = conn->virtualPartnerId;

	//Send the enrollment to our partner after we are connected
	u8 len = SIZEOF_CONN_PACKET_MODULE + SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_SERIAL_MESSAGE;
	DYNAMIC_ARRAY(buffer, len);
	memcpy(buffer, &ted.requestHeader, SIZEOF_CONN_PACKET_MODULE);
	memcpy(buffer + SIZEOF_CONN_PACKET_MODULE, &ted.requestData, SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_SERIAL_MESSAGE);

	logt("ERROR", "Sender was %u", ted.requestHeader.header.sender);

	if(conn != nullptr){
		conn->SendData(buffer, len, DeliveryPriority::LOW, false);
	}

	//Final state reached, will be cleared after timeout is reached
	ted.state = EnrollmentStates::MESSAGE_SENT;
#endif
}

void EnrollmentModule::SendEnrollmentResponse(EnrollmentModuleActionResponseMessages responseType, NodeId receiver, u32 serialNumberIndex, u8 result, u8 requestHandle) const
{
	//Pay attention when testing: This command is sent to the node that requested the enrollment
	//If only testing on a single node, its nodeId will have changed after it was enrolled, so it will not
	//receive the response itself

	logt("WARNING", "Sending enrollment response %u", result);

	//Inform the sender, that the enrollment was successful
	EnrollmentModuleEnrollmentResponse data;
	data.result = result;
	data.serialNumberIndex = serialNumberIndex;

	SendModuleActionMessage(
		MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
		receiver,
		(u8)responseType,
		requestHandle,
		(u8*)&data,
		sizeof(EnrollmentModuleEnrollmentResponse),
		true
	);
}

#define _____________HANDLERS_____________

void EnrollmentModule::ButtonHandler(u8 buttonId, u32 holdTimeDs)
{
	//Remove beacon enrollment and set to default
	if(SHOULD_BUTTON_EVT_EXEC(configuration.buttonRemoveEnrollmentDs)){
		logt("WARNING", "Resetting to unenrolled mode");

		//Prepare a packet with the unenrollment
		const u8 len = SIZEOF_CONN_PACKET_MODULE + SIZEOF_ENROLLMENT_MODULE_REMOVE_ENROLLMENT;
		u8 buffer[len];
		connPacketModule* packet = (connPacketModule*)buffer;
		EnrollmentModuleRemoveEnrollmentMessage* data = (EnrollmentModuleRemoveEnrollmentMessage*)packet->data;

		packet->header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
		packet->header.sender = GS->node->configuration.nodeId;
		packet->header.receiver = GS->node->configuration.nodeId;

		packet->moduleId = moduleID::ENROLLMENT_MODULE_ID;
		packet->actionType = (u8)EnrollmentModuleTriggerActionMessages::REMOVE_ENROLLMENT;
		packet->requestHandle = 0;

		data->serialNumberIndex = RamConfig->serialNumberIndex;

		Unenroll(packet, len);

	}
}


void EnrollmentModule::BleEventHandler(const ble_evt_t& bleEvent)
{
	if(!configuration.moduleActive) return;


#ifdef ACTIVATE_MA_MODULE
	if(bleEvent.header.evt_id == BLE_GAP_EVT_ADV_REPORT){
		fh_ble_gap_addr_t addr = FruityHal::Convert(&bleEvent.evt.gap_evt.params.adv_report.peer_addr);
		const u8* data = bleEvent.evt.gap_evt.params.adv_report.data;
		u16 dataLength = bleEvent.evt.gap_evt.params.adv_report.dlen;

		const meshAccessServiceAdvMessage* message = (const meshAccessServiceAdvMessage*) data;

		//Check if this is a connectable mesh access packet
		if (
			bleEvent.evt.gap_evt.params.adv_report.type == BLE_GAP_ADV_TYPE_ADV_IND
			&& bleEvent.evt.gap_evt.params.adv_report.rssi > STABLE_CONNECTION_RSSI_THRESHOLD
			&& dataLength >= SIZEOF_MESH_ACCESS_SERVICE_DATA_ADV_MESSAGE
			&& message->flags.type == BLE_GAP_AD_TYPE_FLAGS
			&& message->serviceUuids.uuid == SERVICE_DATA_SERVICE_UUID16
			&& message->serviceData.messageType == SERVICE_DATA_MESSAGE_TYPE_MESH_ACCESS
		){
			//Check if the nearby serial is in our proposal list and save it if it is not
			//This will ensure that the list of proposals is always changing "randomly"
			if(
				proposal.serialNumberIndex[0] != message->serviceData.serialIndex
				&& proposal.serialNumberIndex[1] != message->serviceData.serialIndex
				&& proposal.serialNumberIndex[2] != message->serviceData.serialIndex
			){
				proposal.serialNumberIndex[proposalIndexCounter] = message->serviceData.serialIndex;
				proposalIndexCounter = (proposalIndexCounter + 1) % ENROLLMENT_PROPOSAL_MESSAGE_NUM_ENTRIES;
			}

			// Check if we received a message from a beacon that must be enrolled
			if(ted.state == EnrollmentStates::SCANNING && ted.requestData.serialNumberIndex == message->serviceData.serialIndex)
			{
				EnrollNodeViaMeshAccessConnection(addr, message);
			}
		}
	}
#endif
}

void EnrollmentModule::RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength)
{
	logt("WARNING", "StorageEvent %u, %u, %u", recordId, resultCode, userType);

	//After an enrollment has been saved, send back the ack
	if (userType == (u32)EnrollmentModuleSaveActions::SAVE_ENROLLMENT_ACTION)
	{
		SaveEnrollmentAction* data = (SaveEnrollmentAction*)userData;

		//Set ted state to not enrolling so that a timeout won't trigger
		ted.state = EnrollmentStates::NOT_ENROLLING;

		SendEnrollmentResponse(
				EnrollmentModuleActionResponseMessages::ENROLLMENT_RESPONSE,
				NODE_ID_BROADCAST, //To be sure because own node id changes
				RamConfig->serialNumberIndex,
				(u8)resultCode,
				data->requestHandle);

		if(resultCode == RecordStorageResultCode::SUCCESS){
			//Enable green light, first switch io module led control off
			IoModule* ioModule = (IoModule*)GS->node->GetModuleById(moduleID::IO_MODULE_ID);
			if (ioModule != nullptr) {
				ioModule->currentLedMode = ledMode::LED_MODE_CUSTOM;
			}
			GS->ledRed->Off();
			GS->ledGreen->On();
			GS->ledBlue->Off();

			// Reboot after successful enroll
			//TODO: Should ideally listen for an event that the message has been sent
			GS->node->Reboot(SEC_TO_DS(4));
		} else {
			logt("ERROR", "Could not save because %u", resultCode);
		}
	}
	else if (userType == (u32)EnrollmentModuleSaveActions::SAVE_REMOVE_ENROLLMENT_ACTION)
	{
		SaveEnrollmentAction* saveData = (SaveEnrollmentAction*)userData;

		//Set ted state to not enrolling so that a timeout won't trigger
		ted.state = EnrollmentStates::NOT_ENROLLING;

		//Inform the sender, that the enrollment was removed successfully
		if(saveData->sender != 0){
			SendEnrollmentResponse(
					EnrollmentModuleActionResponseMessages::REMOVE_ENROLLMENT_RESPONSE,
					NODE_ID_BROADCAST, //To be sure because own node id changes
					RamConfig->serialNumberIndex,
					(u8)resultCode,
					saveData->requestHandle);
		}

		if(resultCode == RecordStorageResultCode::SUCCESS){
			GS->node->Reboot(SEC_TO_DS(4));
		} else {
			logt("ERROR", "Could not save because %u", resultCode);
		}
	}
}

#endif
