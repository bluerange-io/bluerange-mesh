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


/*
 * This file contains the definitions that are used to create and parse advertising (broadcast)
 * messages used by FruityMesh.
 * 
 * *** ATTENTION ***
 * These types should not be modified to stay compatible with other FruityMesh nodes.
 * If a vendor needs to introduce additional advertising messages, they must be sent
 * under a different company identifier or a different service uuid. The M-Way ranges
 * must not be used for this purpose to be compatible with future firmware updates.
 * *** ATTENTION ***
 * */

#pragma once

#include <FmTypes.h>

constexpr u32 ADV_PACKET_MAX_SIZE = 31;

//####### Advertising packets => Message Types #################################################

//Message types: Protocol defined, up to 19 because we want to have a unified
//type across advertising and connection packets if we need to unify these.
enum class ServiceDataMessageType : u16
{
    INVALID        = 0,
    // depricated : JOIN_ME_V0   = 0x01,
    LEGACY_ASSET   = 0x02,
    MESH_ACCESS    = 0x03,
    ASSET          = 0x04,
    SENSOR_MESSAGE = 0x05,
    ASSET_BLE      = 0x06,
    ASSET_INS      = 0x07
};

enum class ManufacturerSpecificMessageType : u8
{
    INVALID      = 0,
    JOIN_ME_V0   = 0x01,
};

//Start packing all these structures
//These are packed so that they can be transmitted savely over the air
//Smaller datatypes could be implemented with bitfields?
//Sizeof operator is not to be trusted because of padding
// Pay attention to http://www.keil.com/support/man/docs/armccref/armccref_Babjddhe.htm

#pragma pack(push)
#pragma pack(1)

//###### AD structures for advertising messages ###############################

//BLE AD Type FLAGS
constexpr size_t SIZEOF_ADV_STRUCTURE_FLAGS = 3;
typedef struct
{
    u8 len;
    u8 type;
    u8 flags;
}AdvStructureFlags;
STATIC_ASSERT_SIZE(AdvStructureFlags, SIZEOF_ADV_STRUCTURE_FLAGS);

//BLE AD Type list of 16-bit service UUIDs
constexpr size_t SIZEOF_ADV_STRUCTURE_UUID16 = 4;
typedef struct
{
    u8 len;
    u8 type;
    u16 uuid;
}AdvStructureUUID16;
STATIC_ASSERT_SIZE(AdvStructureUUID16, SIZEOF_ADV_STRUCTURE_UUID16);

//Header of service data + our custom messageType
constexpr size_t SIZEOF_ADV_STRUCTURE_SERVICE_DATA_AND_TYPE = 6;
typedef struct
{
    AdvStructureUUID16 uuid;
    ServiceDataMessageType messageType; //Message type depending on our custom service
}AdvStructureServiceDataAndType;
STATIC_ASSERT_SIZE(AdvStructureServiceDataAndType, SIZEOF_ADV_STRUCTURE_SERVICE_DATA_AND_TYPE);

//BLE AD Type Manufacturer specific data
constexpr size_t SIZEOF_ADV_STRUCTURE_MANUFACTURER = 4;
typedef struct
{
    u8 len;
    u8 type;
    u16 companyIdentifier;
}AdvStructureManufacturer;
STATIC_ASSERT_SIZE(AdvStructureManufacturer, SIZEOF_ADV_STRUCTURE_MANUFACTURER);


constexpr size_t SIZEOF_ADV_PACKET_SERVICE_AND_DATA_HEADER = (SIZEOF_ADV_STRUCTURE_FLAGS + SIZEOF_ADV_STRUCTURE_UUID16 + SIZEOF_ADV_STRUCTURE_SERVICE_DATA_AND_TYPE);
typedef struct
{
    AdvStructureFlags flags;
    AdvStructureUUID16 uuid;
    AdvStructureServiceDataAndType data;
}AdvPacketServiceAndDataHeader;
STATIC_ASSERT_SIZE(AdvPacketServiceAndDataHeader, SIZEOF_ADV_PACKET_SERVICE_AND_DATA_HEADER);


//####### Advertising packets => Structs #################################################

//Header that is common to all mesh advertising messages
#define SIZEOF_ADV_PACKET_STUFF_AFTER_MANUFACTURER 4 //1byte mesh identifier + 2 byte networkid + 1 byte message type
#define SIZEOF_ADV_PACKET_HEADER (SIZEOF_ADV_STRUCTURE_FLAGS + SIZEOF_ADV_STRUCTURE_MANUFACTURER + SIZEOF_ADV_PACKET_STUFF_AFTER_MANUFACTURER) //11 byte
typedef struct
{
    AdvStructureFlags flags;
    AdvStructureManufacturer manufacturer;
    u8 meshIdentifier;
    NetworkId networkId;
    ManufacturerSpecificMessageType messageType; 
}AdvPacketHeader;
STATIC_ASSERT_SIZE(AdvPacketHeader, 11);

// ==> This leaves us with 20 bytes payload that are saved in the manufacturer specific data field

// ######## JOIN_ME Advertising Packet #########

//JOIN_ME packet that is used for cluster discovery
//TODO: Add  the current discovery mode/length,... which would allow other nodes to determine
//        How long they need to wait until this node scans or advertises again?

//This is v0 of the packet, other versions will have different values in the packet,
//Future research must show which values are the most interesting to determine the
//best connection partner.
#define SIZEOF_ADV_PACKET_PAYLOAD_JOIN_ME_V0 20
typedef struct
{
    NodeId sender;
    ClusterId clusterId; //Consists of the founding node's id and the connection loss / restart counter
    ClusterSize clusterSize;
    u8 freeMeshInConnections : 3; //Up to 8 in-connections
    u8 freeMeshOutConnections : 5; //Up to 32 out-connections

    u8 batteryRuntime; //batteryRuntime. Contains the expected runtime of the device (1-59=minutes, 60-83=1-23hours, 84-113=1-29days, 114-233=1-119months, 234-254=10-29year, 255=infinite)
    i8 txPower; //txPower. Send power in two's complement dbm
    DeviceType deviceType; //Type of device
    u16 hopsToSink; //Number of hops to the shortest sink
    u16 meshWriteHandle; //The GATT handle for the mesh communication characteristic
    ClusterId ackField;//Contains the acknowledgement from another node for the slave connection procedure
}AdvPacketPayloadJoinMeV0;
STATIC_ASSERT_SIZE(AdvPacketPayloadJoinMeV0, 20);

/*
 * Explanation:
The JOIN_ME packet can have a number of different fields that contain different information.
The version number indicates the JOIN_ME packet type
This information can then be used in the clusterScore function to build clusters based
on different criteria
 * */
#define SIZEOF_ADV_PACKET_JOIN_ME (SIZEOF_ADV_PACKET_HEADER + SIZEOF_ADV_PACKET_PAYLOAD_JOIN_ME_V0)
typedef struct
{
    AdvPacketHeader header;
    AdvPacketPayloadJoinMeV0 payload;
}AdvPacketJoinMeV0;
STATIC_ASSERT_SIZE(AdvPacketJoinMeV0, 31);

//####### Asset Tracking #################################################

#pragma pack(push)
#pragma pack(1)

#define SIZEOF_ADV_STRUCTURE_ASSET_SERVICE_DATA_PAYLOAD 7
struct AdvPacketAssetBleServiceDataPayload {

    NetworkId networkId; //Either 0 if roaming in the organization or != 0 if part of a network and statically attached

    // The received signal strength will be measured 
    // on the mesh node receiver side, but is considered as part of the payload,
    // later on, when the tracked asset packets are assembled.
    i8 rssi37;
    i8 rssi38;
    i8 rssi39;

    u8 reserved[2];
};
STATIC_ASSERT_SIZE(AdvPacketAssetBleServiceDataPayload, SIZEOF_ADV_STRUCTURE_ASSET_SERVICE_DATA_PAYLOAD);

struct AdvPacketAssetInsServiceDataPayload {

    u8 insMeta; // Encoded INS meta data
    u16 ins1; // Encoded INS state estimate data
    u16 ins2;
    u16 ins3;
};
STATIC_ASSERT_SIZE(AdvPacketAssetInsServiceDataPayload, SIZEOF_ADV_STRUCTURE_ASSET_SERVICE_DATA_PAYLOAD);

//Service Data (max. 24 byte)
#define SIZEOF_ADV_STRUCTURE_ASSET_SERVICE_DATA 24

struct AdvPacketAssetServiceData
{
    //6 byte header
    AdvStructureServiceDataAndType data;

    //1 byte flags
    u8 moving : 1;
    u8 hasFreeInConnection : 1;
    u8 interestedInConnection : 1;
    u8 channel : 2; // 0 = unknown, 1 = 37, 2 = 38, 3 = 39
    u8 reservedBits : 3;

    u16 nodeId; //Either a nodeId of a network (networkId must be != 0) or a organization wide unique nodeId

    u8 payload[SIZEOF_ADV_STRUCTURE_ASSET_SERVICE_DATA_PAYLOAD];

    u8 reserved[3];
    u8 reservedEncryptionCounter; // Potentially reserved for encryption counter
    u32 mic; //Encryption is probably done using a synchronized time, not yet specified. For now, not encrypted if mic is 0
};
STATIC_ASSERT_SIZE(AdvPacketAssetServiceData, SIZEOF_ADV_STRUCTURE_ASSET_SERVICE_DATA);

//Service Data (max. 24 byte)
#define SIZEOF_ADV_STRUCTURE_LEGACY_V2_ASSET_SERVICE_DATA 24

//Legacy support! This message is no longer sent out by our new assets, however old assets
//must still be supported and thus this message must still be scannable.
struct AdvPacketLegacyV2AssetServiceData
{
    //6 byte header
    AdvStructureServiceDataAndType data;

    //1 byte flags
    u8 gyroscopeAvailable : 1;
    u8 magnetometerAvailable : 1;
    u8 moving : 1;
    u8 hasFreeInConnection : 1;
    u8 interestedInConnection : 1;
    u8 positionValid : 1;
    u8 reservedBits : 2;

    //8 byte assetData
    u16 assetNodeId;
    u8 batteryPower; //0xFF = not available
    u16 absolutePositionX;
    u16 absolutePositionY;
    u8 pressure; //0xFF = not available
    
    NetworkId networkId;
    u32 serialNumberIndex;

    u8 reservedForTimeOrCounter[3]; //This is reserved for some kind of replay protection. DO NOT USE FOR ANYTHING ELSE! We need AT LEAST 3 bytes for this!
};
STATIC_ASSERT_SIZE(AdvPacketAssetServiceData, SIZEOF_ADV_STRUCTURE_LEGACY_V2_ASSET_SERVICE_DATA);

//Service Data (max. 24 byte)
#define SIZEOF_ADV_STRUCTURE_LEGACY_ASSET_SERVICE_DATA 24
//Legacy support! This message is no longer sent out by our new assets, however old assets
//must still be supported and thus this message must still be scannable.
struct AdvPacketLegacyAssetServiceData
{
    AdvStructureServiceDataAndType data;

    //1 byte capabilities
    u8 advertisingChannel : 2; // 0 = not available, 1=37, 2=38, 3=39
    u8 gyroscopeAvailable : 1;
    u8 magnetometerAvailable : 1;
    u8 hasFreeInConnection : 1;
    u8 interestedInConnection : 1;
    u8 reservedBit : 2;

    //11 byte assetData
    u32 serialNumberIndex;
    u8 batteryPower; //0xFF = not available
    u8 speed; //0xFF = not available
    u8 reserved;
    u16 pressure; //0xFFFF = not available
    i8 temperature; //0xFF = not available
    u8 humidity; //0xFF = not available

    NodeId nodeId;
    NetworkId networkId;

    u8 reserved2[2];
};
STATIC_ASSERT_SIZE(AdvPacketLegacyAssetServiceData, SIZEOF_ADV_STRUCTURE_LEGACY_ASSET_SERVICE_DATA);

//Service Data (max. 24 byte)
#define SIZEOF_ADV_STRUCTURE_SENSOR_MESSAGE_SERVICE_DATA 15
struct AdvPacketSensorMessageServiceData
{
    //6 byte header
    AdvStructureServiceDataAndType data;

    //1 byte flags
    u8 isEncrypted : 1;
    u8 isOemProduct : 1;
    u8 reservedBits : 6;

    // If isEncrypted bit is true then first struct is used otherwise second.
    union
    {
        struct
        {
            u32 MIC;
            NodeId nodeId;
        }encrypted;
        struct
        {
            u32 serialNumberIndex;
            NodeId nodedId;
        }unencrypted;
    }encryptedField;

    // The module that generated this value
    ModuleId moduleId;

    // The module that generated this value
    SensorType sensorType;

    // Payload placeholder
    u8 payload[1];
};
STATIC_ASSERT_SIZE(AdvPacketSensorMessageServiceData, SIZEOF_ADV_STRUCTURE_SENSOR_MESSAGE_SERVICE_DATA + 1);

#pragma pack(pop)

//End Packing
#pragma pack(pop)
