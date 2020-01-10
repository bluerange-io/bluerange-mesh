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

#include <MeshAccessConnection.h>
#include <AdvertisingController.h>

typedef struct MeshAccessServiceStruct
{
	FruityHal::BleGattUuid          serviceUuid;
	u16                                serviceHandle;
	FruityHal::BleGattCharHandles    txCharacteristicHandle;
	FruityHal::BleGattCharHandles    rxCharacteristicHandle;
} MeshAccessServiceStruct;


#pragma pack(push)
#pragma pack(1)
//Service Data (max. 24 byte)
constexpr int SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA = 16;
typedef struct
{
	advStructureServiceDataAndType data;

	NetworkId networkId; //Mesh network id
	u8 isEnrolled : 1; // Flag if this beacon is enrolled
	u8 isSink : 1;
	u8 isZeroKeyConnectable : 1;
	u8 isConnectable : 1;
	u8 interestedInConnetion : 1;
	u8 reserved : 3;
	u32 serialIndex; //SerialNumber index of the beacon
	SimpleArray<ModuleId, 3> moduleIds; //Additional subServices offered with their data

}advStructureMeshAccessServiceData;
STATIC_ASSERT_SIZE(advStructureMeshAccessServiceData, 16);

typedef struct
{
	u8 len;
	ModuleId moduleId;
	//Some more data

}advStructureMeshAccessServiceSubServiceData;
STATIC_ASSERT_SIZE(advStructureMeshAccessServiceSubServiceData, 2);


constexpr int SIZEOF_MESH_ACCESS_SERVICE_DATA_ADV_MESSAGE = (SIZEOF_ADV_STRUCTURE_FLAGS + SIZEOF_ADV_STRUCTURE_UUID16 + SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA);
typedef struct
{
	advStructureFlags flags;
	advStructureUUID16 serviceUuids;
	advStructureMeshAccessServiceData serviceData;

}meshAccessServiceAdvMessage;
STATIC_ASSERT_SIZE(meshAccessServiceAdvMessage, 23);

#pragma pack(pop)


#define MA_SERVICE_UUID_TYPE BLE_UUID_TYPE_VENDOR_BEGIN
constexpr u8 MA_SERVICE_BASE_UUID[] = { 0x58, 0x18, 0x05, 0xA0, 0x07, 0x0C, 0xFD, 0x93, 0x3C, 0x42, 0xCE, 0xAC, 0x00, 0x00, 0x00, 0x00 };

constexpr int MA_SERVICE_SERVICE_CHARACTERISTIC_UUID = 0x0001;
constexpr int MA_SERVICE_RX_CHARACTERISTIC_UUID = 0x0002;
constexpr int MA_SERVICE_TX_CHARACTERISTIC_UUID = 0x0003;
constexpr int MA_CHARACTERISTIC_MAX_LENGTH = 20;


enum class MeshAccessModuleTriggerActionMessages : u8{
	MA_CONNECT = 0,
	MA_DISCONNECT = 1,
};

enum class MeshAccessModuleActionResponseMessages : u8{

};

enum class MeshAccessModuleGeneralMessages : u8{
	MA_CONNECTION_STATE = 0,
};

//####### Module messages (these need to be packed)
#pragma pack(push)
#pragma pack(1)

	constexpr int SIZEOF_MA_MODULE_CONNECT_MESSAGE = 28;
	typedef struct
	{
		FruityHal::BleGapAddr targetAddress;
		FmKeyId fmKeyId;
		SimpleArray<u8, 16> key;
		u8 tunnelType : 2;
		u8 reserved;

	}MeshAccessModuleConnectMessage;
	STATIC_ASSERT_SIZE(MeshAccessModuleConnectMessage, 29);

	constexpr int SIZEOF_MA_MODULE_DISCONNECT_MESSAGE = 7;
	typedef struct
	{
		FruityHal::BleGapAddr targetAddress;

	}MeshAccessModuleDisconnectMessage;
	STATIC_ASSERT_SIZE(MeshAccessModuleDisconnectMessage, 7);

	constexpr int SIZEOF_MA_MODULE_CONNECTION_STATE_MESSAGE = 3;
	typedef struct
	{
		NodeId vPartnerId;
		u8 state;

	}MeshAccessModuleConnectionStateMessage;
	STATIC_ASSERT_SIZE(MeshAccessModuleConnectionStateMessage, 3);

#pragma pack(pop)
//####### Module messages end

#pragma pack(push)
#pragma pack(1)
	//Module configuration that is saved persistently (size must be multiple of 4)
	struct MeshAccessModuleConfiguration : ModuleConfiguration{
		//Insert more persistent config values here
	};
#pragma pack(pop)

class MeshAccessModule: public Module
{
	public:
		MeshAccessServiceStruct meshAccessService;
		static constexpr bool allowInboundConnections = true; //Whether incoming connections are allowed at all (No gatt service, no advertising)
		u8 enableAdvertising = true; //Advertise the meshaccessPacket connectable
		u8 disableIfInMesh = false; //Once a mesh connection is active, disable advertising
		bool allowUnenrolledUnsecureConnections = false; //whether or not unsecure connections should be allowed when unenrolled
	private:

		SimpleArray<ModuleId, 3> moduleIdsToAdvertise;

		void RegisterGattService();
		bool gattRegistered;

		AdvJob* discoveryJobHandle;

		bool logNearby;
		char logWildcard[6];



		void ReceivedMeshAccessConnectMessage(connPacketModule* packet, u16 packetLength) const;
		void ReceivedMeshAccessDisconnectMessage(connPacketModule* packet, u16 packetLength) const;
		void ReceivedMeshAccessConnectionStateMessage(connPacketModule* packet, u16 packetLength) const;


	public:
		DECLARE_CONFIG_AND_PACKED_STRUCT(MeshAccessModuleConfiguration);

		MeshAccessModule();
		void UpdateMeshAccessBroadcastPacket(u16 advIntervalMs = 100, bool interestedInConnection = false);

		void ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength) override;

		void ResetToDefaultConfiguration() override;

		void TimerEventHandler(u16 passedTimeDs) override;

		void MeshConnectionChangedHandler(MeshConnection& connection) override;

		//Boradcast messages
		void AddModuleIdToAdvertise(ModuleId moduleId);
		void DisableBroadcast();

		//Authorization
		MeshAccessAuthorization CheckMeshAccessPacketAuthorization(BaseConnectionSendData* sendData, u8* data, FmKeyId fmKeyId, DataDirection direction) override;
		MeshAccessAuthorization CheckAuthorizationForAll(BaseConnectionSendData* sendData, u8* data, FmKeyId fmKeyId, DataDirection direction) const;

		//Messages
		void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader) override;
		void MeshAccessMessageReceivedHandler(MeshAccessConnection* connection, BaseConnectionSendData* sendData, u8* data) const;

		#ifdef TERMINAL_ENABLED
		TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override;
		#endif
		void GapAdvertisementReportEventHandler(const FruityHal::GapAdvertisementReportEvent& advertisementReportEvent) override;

		bool IsZeroKeyConnectable(const ConnectionDirection direction);

};

