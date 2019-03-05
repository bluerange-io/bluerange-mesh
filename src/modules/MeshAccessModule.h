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

#ifdef ACTIVATE_MA_MODULE
#include <MeshAccessConnection.h>
#include <AdvertisingController.h>

typedef struct MeshAccessServiceStruct
{
	ble_uuid_t						serviceUuid;
	u16                     		serviceHandle;
	ble_gatts_char_handles_t		txCharacteristicHandle;
	ble_gatts_char_handles_t		rxCharacteristicHandle;
} MeshAccessServiceStruct;


#pragma pack(push)
#pragma pack(1)
//Service Data (max. 24 byte)
#define SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA 16
typedef struct
{
	u8 len;
	u8 type;
	u16 uuid;

	u16 messageType; //0x03 for MeshAccess Service
	NetworkId networkId; //Mesh network id
	u8 isEnrolled : 1; // Flag if this beacon is enrolled
	u8 reserved : 7;
	u32 serialIndex; //SerialNumber index of the beacon
	SimpleArray<u8, 3> moduleIds; //Additional subServices offered with their data

}advStructureMeshAccessServiceData;
STATIC_ASSERT_SIZE(advStructureMeshAccessServiceData, 16);

typedef struct
{
	u8 len;
	u8 moduleId;
	//Some more data

}advStructureMeshAccessServiceSubServiceData;


#define SIZEOF_MESH_ACCESS_SERVICE_DATA_ADV_MESSAGE (SIZEOF_ADV_STRUCTURE_FLAGS + SIZEOF_ADV_STRUCTURE_UUID16 + SIZEOF_ADV_STRUCTURE_MESH_ACCESS_SERVICE_DATA)
typedef struct
{
	advStructureFlags flags;
	advStructureUUID16 serviceUuids;
	advStructureMeshAccessServiceData serviceData;

}meshAccessServiceAdvMessage;
STATIC_ASSERT_SIZE(meshAccessServiceAdvMessage, 23);

#pragma pack(pop)


#define MA_SERVICE_UUID_TYPE BLE_UUID_TYPE_VENDOR_BEGIN
#define MA_SERVICE_BASE_UUID 0x58, 0x18, 0x05, 0xA0, 0x07, 0x0C, 0xFD, 0x93, 0x3C, 0x42, 0xCE, 0xAC, 0x00, 0x00, 0x00, 0x00

#define MA_SERVICE_SERVICE_CHARACTERISTIC_UUID 0x0001
#define MA_SERVICE_RX_CHARACTERISTIC_UUID 0x0002
#define MA_SERVICE_TX_CHARACTERISTIC_UUID 0x0003
#define MA_CHARACTERISTIC_MAX_LENGTH 20

#define SERVICE_DATA_MESSAGE_TYPE_MESH_ACCESS 0x03


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

	#define SIZEOF_MA_MODULE_CONNECT_MESSAGE 28
	typedef struct
	{
		fh_ble_gap_addr_t targetAddress;
		u32 fmKeyId;
		SimpleArray<u8, 16> key;
		u8 tunnelType : 2;
		u8 reserved;

	}MeshAccessModuleConnectMessage;
	STATIC_ASSERT_SIZE(MeshAccessModuleConnectMessage, 29);

	#define SIZEOF_MA_MODULE_DISCONNECT_MESSAGE 7
	typedef struct
	{
		fh_ble_gap_addr_t targetAddress;

	}MeshAccessModuleDisconnectMessage;
	STATIC_ASSERT_SIZE(MeshAccessModuleDisconnectMessage, 7);

	#define SIZEOF_MA_MODULE_CONNECTION_STATE_MESSAGE 3
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
		u8 allowInboundConnections; //Whether incoming connections are allowed at all (No gatt service, no advertising)
		u8 enableAdvertising; //Advertise the meshaccessPacket connectable
		u8 disableIfInMesh; //Once a mesh connection is active, disable advertising
		//Insert more persistent config values here
	};
#pragma pack(pop)

class MeshAccessModule: public Module
{
	public:
		MeshAccessServiceStruct meshAccessService;

	private:

		SimpleArray<u8, 3> moduleIdsToAdvertise;

		void RegisterGattService();
		bool gattRegistered;

		AdvJob* discoveryJobHandle;

		bool logNearby;


		void BroadcastMeshAccessPacket();


		void ReceivedMeshAccessConnectMessage(connPacketModule* packet, u16 packetLength) const;
		void ReceivedMeshAccessDisconnectMessage(connPacketModule* packet, u16 packetLength) const;
		void ReceivedMeshAccessConnectionStateMessage(connPacketModule* packet, u16 packetLength) const;


	public:
		DECLARE_CONFIG_AND_PACKED_STRUCT(MeshAccessModuleConfiguration);

		MeshAccessModule();

		void ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength) override;

		void ResetToDefaultConfiguration() override;

		void TimerEventHandler(u16 passedTimeDs) override;

		void MeshConnectionChangedHandler(MeshConnection& connection) override;

		//Boradcast messages
		void AddModuleIdToAdvertise(moduleID moduleId);
		void DisableBroadcast();

		//Authorization
		MeshAccessAuthorization CheckMeshAccessPacketAuthorization(BaseConnectionSendData* sendData, u8* data, u32 fmKeyId) override;
		MeshAccessAuthorization CheckAuthorizationForAll(BaseConnectionSendData* sendData, u8* data, u32 fmKeyId) const;

		//Messages
		void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader) override;
		void MeshAccessMessageReceivedHandler(MeshAccessConnection* connection, BaseConnectionSendData* sendData, u8* data) const;

		#ifdef TERMINAL_ENABLED
		bool TerminalCommandHandler(char* commandArgs[], u8 commandArgsSize) override;
		#endif
		void BleEventHandler(const ble_evt_t& bleEvent) override;

};

#endif
