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
#pragma once

#include <Module.h>
#include <ScanController.h>

#include <MeshAccessModule.h>

enum class EnrollmentModuleSaveActions : u8{ 
	SAVE_ENROLLMENT_ACTION, 
	SAVE_REMOVE_ENROLLMENT_ACTION,
	ERASE_RECORD_STORAGE,
};

enum class enrollmentMethods : u8{ 
	SERIAL_2 = 0, 
	CHIP_ID = 1, 
	SERIAL = 2 
};

enum class EnrollmentResponseCode : u8 {
	OK                                   = 0x00,
	// There are more enroll response codes that are taken from the Flash Storage response codes
	ALREADY_ENROLLED_WITH_DIFFERENT_DATA = 0x10,
	PREENROLLMENT_FAILED                 = 0x11,
	HIGHEST_POSSIBLE_VALUE               = 0xFF,
};

#pragma pack(push, 1)
//Module configuration that is saved persistently
struct EnrollmentModuleConfiguration : ModuleConfiguration {
	u8 buttonRemoveEnrollmentDs;
};
#pragma pack(pop)

//####### Module specific message structs (these need to be packed)
#pragma pack(push)
#pragma pack(1)

	constexpr int  SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_SERIAL_MESSAGE_MIN = (8);
	constexpr int  SIZEOF_ENROLLMENT_MODULE_SET_ENROLLMENT_BY_SERIAL_MESSAGE     = (73);
	typedef struct
	{
		u32 serialNumberIndex;
		NodeId newNodeId;
		NetworkId newNetworkId;
		SimpleArray<u8, 16> newNetworkKey;
		SimpleArray<u8, 16> newUserBaseKey;
		SimpleArray<u8, 16> newOrganizationKey;
		SimpleArray<u8, 16> nodeKey; // Key used to connect to the unenrolled node
		u8 timeoutSec : 7; //how long to try to connect to the unenrolled node, 0 means default time
		u8 enrollOnlyIfUnenrolled : 1; //Set to 1 in order to return an error if already enrolled

	}EnrollmentModuleSetEnrollmentBySerialMessage;
	STATIC_ASSERT_SIZE(EnrollmentModuleSetEnrollmentBySerialMessage, 73);

	struct EnrollmentModuleSetNetworkMessage
	{
		NetworkId newNetworkId;
	};
	STATIC_ASSERT_SIZE(EnrollmentModuleSetNetworkMessage, 2);

	enum class EnrollmentModuleSetNetworkResponse : u8
	{
		SUCCESS      = 0,
		NOT_AN_ASSET = 1,
		INVALID      = 255,
	};

	struct EnrollmentModuleSetNetworkResponseMessage
	{
		EnrollmentModuleSetNetworkResponse response;
	};

	constexpr int SIZEOF_ENROLLMENT_MODULE_REMOVE_ENROLLMENT = (4);
	typedef struct
	{
		u32 serialNumberIndex;

	}EnrollmentModuleRemoveEnrollmentMessage;
	STATIC_ASSERT_SIZE(EnrollmentModuleRemoveEnrollmentMessage, SIZEOF_ENROLLMENT_MODULE_REMOVE_ENROLLMENT);

	//Answers
	typedef struct
	{
		u32 serialNumberIndex;
		EnrollmentResponseCode result;

	}EnrollmentModuleEnrollmentResponse;
	STATIC_ASSERT_SIZE(EnrollmentModuleEnrollmentResponse, 5);

	typedef struct
	{
		u32 serialNumberIndex[3];

	}EnrollmentModuleEnrollmentProposalMessage;
	STATIC_ASSERT_SIZE(EnrollmentModuleEnrollmentProposalMessage, 12);

#pragma pack(pop)
//####### Module messages end


class EnrollmentModule: public Module
{
	public:
		enum class EnrollmentModuleTriggerActionMessages : u8{
			SET_ENROLLMENT_BY_SERIAL   = 0,
			REMOVE_ENROLLMENT          = 1,
			//SET_ENROLLMENT_BY_SERIAL = 2, //Deprecated since version 0.7.22
			SET_NETWORK                = 3,
		};

		enum class EnrollmentModuleActionResponseMessages : u8 {
			ENROLLMENT_RESPONSE        = 0,
			REMOVE_ENROLLMENT_RESPONSE = 1,
			ENROLLMENT_PROPOSAL        = 2,
			SET_NETWORK_RESPONSE       = 3,
		};

		void DispatchPreEnrollment(Module* lastModuleCalled, PreEnrollmentReturnCode lastStatus);

	private:
		#pragma pack(push, 1)
		struct SaveEnrollmentAction {
			NodeId sender;
			u8 requestHandle;
		};

		#pragma pack(pop)

		enum class EnrollmentStates : u8 {
			NOT_ENROLLING,
			PREENROLLMENT_RUNNING,
			SCANNING,
			CONNECTING,
			CONNECTED,
			MESSAGE_SENT
		};

		//Data for enrolling over mesh access connections
#pragma pack(push)
#pragma pack(1)
		struct TemporaryEnrollmentData{
			EnrollmentStates state;
			u16 packetLength;
			connPacketModule requestHeader;
			union {
				EnrollmentModuleSetEnrollmentBySerialMessage requestData;
				EnrollmentModuleRemoveEnrollmentMessage unenrollData;
			};
			u32 endTimeDs;

			u32 uniqueConnId;
		};
#pragma pack(pop)

		//While an enrollment request is active, we temporarily save the data here
		//This can be used for an enrollment request
		TemporaryEnrollmentData ted;



		//Save a few nearby node serials in this proposal message
		static constexpr int ENROLLMENT_PROPOSAL_MESSAGE_NUM_ENTRIES = 3;
		u8 proposalIndexCounter;
		EnrollmentModuleEnrollmentProposalMessage proposal;

		ScanJob * p_scanJob;



		void Enroll(connPacketModule* packet, u16 packetLength);

		void EnrollOverMesh(connPacketModule* packet, u16 packetLength, BaseConnection* connection);

		void SaveEnrollment(connPacketModule* packet, u16 packetLength);

		void SaveUnenrollment(connPacketModule* packet, u16 packetLength);

		void EnrollmentConnectionConnectedHandler();

		void EnrollNodeViaMeshAccessConnection(FruityHal::BleGapAddr& addr, const meshAccessServiceAdvMessage* advMessage);

		void SendEnrollmentResponse(EnrollmentModuleActionResponseMessages responseType, EnrollmentResponseCode result, u8 requestHandle) const;

		void Unenroll(connPacketModule* packet, u16 packetLength);

	public:
		DECLARE_CONFIG_AND_PACKED_STRUCT(EnrollmentModuleConfiguration);

		EnrollmentModule();

		void ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength) override;

		void ResetToDefaultConfiguration() override;

		void TimerEventHandler(u16 passedTimeDs) override;

		void GapAdvertisementReportEventHandler(const FruityHal::GapAdvertisementReportEvent& advertisementReportEvent) override;

		void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader) override;

		//PreEnrollment

		void StoreTemporaryEnrollmentDataAndDispatch(connPacketModule* packet, u16 packetLength);

		void PreEnrollmentFailed();

		//Handlers
#if IS_ACTIVE(BUTTONS)
		void ButtonHandler(u8 buttonId, u32 holdTimeDs) override;
#endif

		#ifdef TERMINAL_ENABLED
		TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override;
		#endif

		void RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength) override;

		MeshAccessAuthorization CheckMeshAccessPacketAuthorization(BaseConnectionSendData* sendData, u8* data, u32 fmKeyId, DataDirection direction) override;
};
