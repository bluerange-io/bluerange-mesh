/**

Copyright (c) 2014-2015 "M-Way Solutions GmbH"
FruityMesh - Bluetooth Low Energy mesh protocol [http://mwaysolutions.com/]

This file is part of FruityMesh

FruityMesh is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

/*
 * This file contains the structs that are used for packets that are sent
 * over standard BLE connections, such as the mesh-handshake and other packets
 */

#pragma once

#include <types.h>

//Start packing all these structures
//These are packed so that they can be transmitted savely over the air
//Smaller datatypes could be implemented with bitfields?
//Sizeof operator is not to be trusted because of padding
// Pay attention to http://www.keil.com/support/man/docs/armccref/armccref_Babjddhe.htm

#pragma pack(push)
#pragma pack(1)


//########## Connection packets ###############################################

//Mesh clustering and handshake
#define MESSAGE_TYPE_CLUSTER_WELCOME 50
#define MESSAGE_TYPE_CLUSTER_ACK_1 51
#define MESSAGE_TYPE_CLUSTER_ACK_2 52
#define MESSAGE_TYPE_CLUSTER_INFO_UPDATE 53

//Module messages

//We need two messages to set and get a complete config
//We need a message to query data with commands that are different for each module

#define MESSAGE_TYPE_MODULE_REQUEST 85
#define MESSAGE_TYPE_ADVINFO 84
#define MESSAGE_TYPE_QOS_CONNECTION_DATA 82
#define MESSAGE_TYPE_QOS_REQUEST 81

//Other
#define MESSAGE_TYPE_DATA_1 80
#define MESSAGE_TYPE_DATA_2 83

//TODO: Should support message splitting
#define SIZEOF_CONN_PACKET_HEADER 5
typedef struct
{
	u8 messageType;
	nodeID sender;
	nodeID receiver;
}connPacketHeader;

//CLUSTER_WELCOME
#define SIZEOF_CONN_PACKET_PAYLOAD_CLUSTER_WELCOME 10
typedef struct
{
	clusterID clusterId;
	clusterSIZE clusterSize;
	u16 meshWriteHandle;
	clusterSIZE hopsToSink;
}connPacketPayloadClusterWelcome;

#define SIZEOF_CONN_PACKET_CLUSTER_WELCOME (SIZEOF_CONN_PACKET_HEADER + SIZEOF_CONN_PACKET_PAYLOAD_CLUSTER_WELCOME)
typedef struct
{
	connPacketHeader header;
	connPacketPayloadClusterWelcome payload;
}connPacketClusterWelcome;


//CLUSTER_ACK_1
#define SIZEOF_CONN_PACKET_PAYLOAD_CLUSTER_ACK_1 3
typedef struct
{
	clusterSIZE hopsToSink;
	u8 reserved;
}connPacketPayloadClusterAck1;

#define SIZEOF_CONN_PACKET_CLUSTER_ACK_1 (SIZEOF_CONN_PACKET_HEADER + SIZEOF_CONN_PACKET_PAYLOAD_CLUSTER_ACK_1)
typedef struct
{
	connPacketHeader header;
	connPacketPayloadClusterAck1 payload;
}connPacketClusterAck1;


//CLUSTER_ACK_2
#define SIZEOF_CONN_PACKET_PAYLOAD_CLUSTER_ACK_2 6
typedef struct
{
	clusterID clusterId;
	clusterSIZE clusterSize;
}connPacketPayloadClusterAck2;

#define SIZEOF_CONN_PACKET_CLUSTER_ACK_2 (SIZEOF_CONN_PACKET_HEADER + SIZEOF_CONN_PACKET_PAYLOAD_CLUSTER_ACK_2)
typedef struct
{
	connPacketHeader header;
	connPacketPayloadClusterAck2 payload;
}connPacketClusterAck2;


//CLUSTER_INFO_UPDATE
#define SIZEOF_CONN_PACKET_PAYLOAD_CLUSTER_INFO_UPDATE 12
typedef struct
{
	clusterID currentClusterId;
	clusterID newClusterId;
	clusterSIZE clusterSizeChange;
	clusterSIZE hopsToSink;
	
}connPacketPayloadClusterInfoUpdate;

#define SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE (SIZEOF_CONN_PACKET_HEADER + SIZEOF_CONN_PACKET_PAYLOAD_CLUSTER_INFO_UPDATE)
typedef struct
{
	connPacketHeader header;
	connPacketPayloadClusterInfoUpdate payload;
}connPacketClusterInfoUpdate;


//DATA_PACKET
#define SIZEOF_CONN_PACKET_PAYLOAD_DATA_1 (MESH_SERVICE_CHARACTERISTIC_VALUE_LENGTH_MAX - SIZEOF_CONN_PACKET_HEADER)
typedef struct
{
	u8 length;
	u8 data[SIZEOF_CONN_PACKET_PAYLOAD_DATA_1 - 1];
	
}connPacketPayloadData1;

#define SIZEOF_CONN_PACKET_DATA_1 (SIZEOF_CONN_PACKET_HEADER + SIZEOF_CONN_PACKET_PAYLOAD_DATA_1)
typedef struct
{
	connPacketHeader header;
	connPacketPayloadData1 payload;
}connPacketData1;




//DATA_2_PACKET
#define SIZEOF_CONN_PACKET_PAYLOAD_DATA_2 (MESH_SERVICE_CHARACTERISTIC_VALUE_LENGTH_MAX - SIZEOF_CONN_PACKET_HEADER)
typedef struct
{
	u8 length;
	u8 data[SIZEOF_CONN_PACKET_PAYLOAD_DATA_2 - 1];

}connPacketPayloadData2;

#define SIZEOF_CONN_PACKET_DATA_2 (SIZEOF_CONN_PACKET_HEADER + SIZEOF_CONN_PACKET_PAYLOAD_DATA_2)
typedef struct
{
	connPacketHeader header;
	connPacketPayloadData1 payload;
}connPacketData2;


//MODULE_SET: Used to set/get a configuration for any module and to trigger an action
//TODO: This message needs a variable size and message splitting
enum moduleRequestTypes{
	MODULE_SET_CONFIGURATION=0,
	MODULE_GET_CONFIGURATION=1,
	MODULE_SET_ACTIVE=2,
	MODULE_TRIGGER_ACTION=3
};

#define SIZEOF_CONN_PACKET_MODULE_REQUEST (SIZEOF_CONN_PACKET_HEADER + 15)
typedef struct
{
	connPacketHeader header;
	u16 moduleId;
	u8 moduleRequestType; //typeof moduleRequestTypes
	u8 data[12]; //Data can be larger and will be transmitted in subsequent packets

}connPacketModuleRequest;


//ADVINFO_PACKET
#define SIZEOF_CONN_PACKET_PAYLOAD_ADV_INFO 9
typedef struct
{
	u8 peerAddress[6];
	u16 inverseRssiSum;
	u8 packetCount;

}connPacketPayloadAdvInfo;

//ADV_INFO
//This packet is used to distribute receied advertising messages in the mesh
//if the packet has passed the filterung
#define SIZEOF_CONN_PACKET_ADV_INFO (SIZEOF_CONN_PACKET_HEADER + SIZEOF_CONN_PACKET_PAYLOAD_ADV_INFO)
typedef struct
{
	connPacketHeader header;
	connPacketPayloadAdvInfo payload;
}connPacketAdvInfo;

//QOS_REQUEST
#define SIZEOF_CONN_PACKET_PAYLOAD_QOS_REQUEST 3
typedef struct
{
	nodeID nodeId;
	u8 type;

}connPacketPayloadQosRequest;

#define SIZEOF_CONN_PACKET_QOS_REQUEST (SIZEOF_CONN_PACKET_HEADER + SIZEOF_CONN_PACKET_PAYLOAD_QOS_REQUEST)
typedef struct
{
	connPacketHeader header;
	connPacketPayloadQosRequest payload;
}connPacketQosRequest;

//QOS_CONNECTION_DATA
#define SIZEOF_CONN_PACKET_PAYLOAD_QOS_CONNECTION_DATA 12
typedef struct
{
		nodeID partner1;
		nodeID partner2;
		nodeID partner3;
		nodeID partner4;
		u8 rssi1;
		u8 rssi2;
		u8 rssi3;
		u8 rssi4;

}connPacketPayloadQosConnectionData;

#define SIZEOF_CONN_PACKET_QOS_CONNECTION_DATA (SIZEOF_CONN_PACKET_HEADER + SIZEOF_CONN_PACKET_PAYLOAD_QOS_CONNECTION_DATA)
typedef struct
{
	connPacketHeader header;
	connPacketPayloadQosConnectionData payload;
}connPacketQosConnectionData;


//End Packing
#pragma pack(pop)

