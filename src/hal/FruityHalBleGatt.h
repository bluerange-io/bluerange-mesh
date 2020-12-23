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

#pragma once

/*
 * This file includes platform independent definitions for the BLE GATT layer.
 */

#include <FmTypes.h>
#include <FruityHalBleGap.h>

// Invalid Attribute Handle
#define FH_BLE_GATT_HANDLE_INVALID            0x0000

// define
#define FH_BLE_GATTS_VALUE_LOCATION_INVALID       0x00
#define FH_BLE_GATTS_VALUE_LOCATION_STACK         0x01
#define FH_BLE_GATTS_VALUE_LOCATION_USER          0x02

namespace FruityHal
{

//The ATT protocol header overhead (MTU - ATT header = packet payload)
constexpr u16 ATT_HEADER_SIZE = 3;

enum class BleGattSrvcType : u8
{
    INVALID          = 0x00,
    PRIMARY          = 0x01,
    SECONDARY        = 0x02
};

enum class BleGattWriteType : u8
{
    NOTIFICATION = 0x00,
    INDICATION   = 0x01,
    WRITE_REQ    = 0x02,
    WRITE_CMD    = 0x03
};

struct BleGattWriteParams
{
    BleGattWriteType type; 
    u16              offset;
    u16              handle;
    MessageLength    len;
    u8              *p_data;
};

struct BleGattUuid
{
    u16    uuid;
    u8     type;
};

struct BleGattCharProperties
{
    u8 broadcast            :1;
    u8 read                 :1;
    u8 writeWithoutResponse :1;
    u8 write                :1;
    u8 notify               :1;
    u8 indicate             :1;
    u8 authSignedWrite      :1;
};

struct BleGattCharExtendedProperties
{
    u8 reliableWrite        :1;
    u8 writeableAuxiliaries :1;
};

struct BleGattCharPf
{
    u8          format;
    int8_t      exponent;
    u16         unit;
    u8          nameSpace;
    u16         desc;
};

struct BleGattAttributeMetadata
{
    BleGapConnSecMode    readPerm;
    BleGapConnSecMode    writePerm;
    u8                   variableLength       :1;
    u8                   valueLocation       :2;
    u8                   readAuthorization    :1;
    u8                   writeAuthorization    :1;
};

struct BleGattCharMd
{
    BleGattCharProperties            charProperties;
    BleGattCharExtendedProperties    charExtendedProperties;
    u8 const                        *p_charUserDescriptor;
    u16                              charUserDescriptorMaxSize;
    u16                              charUserDescriptorSize;
    BleGattCharPf const             *p_charPf;
    BleGattAttributeMetadata const  *p_userDescriptorMd;
    BleGattAttributeMetadata const  *p_cccdMd;
    BleGattAttributeMetadata const  *p_sccdMd;
};

struct BleGattAttribute
{
    BleGattUuid const               *p_uuid;
    BleGattAttributeMetadata const  *p_attributeMetadata;
    u16                              initLen;
    u16                              initOffset;
    u16                              maxLen;
    u8                              *p_value;
};

struct BleGattCharHandles
{
    u16    valueHandle;
    u16    userDescriptorHandle;
    u16    cccdHandle;
    u16    sccdHandle;
};

enum class BleGattDBDiscoveryEventType
{
    COMPLETE,
    SERVICE_NOT_FOUND,
};

struct BleGattDBDiscoveryCharacteristic
{
    BleGattUuid charUUID;
    u16         handleValue;
    u16         cccdHandle;
};

struct BleGattDBDiscoveryEvent
{
    u16                              connHandle;
    BleGattDBDiscoveryEventType      type;
    BleGattUuid                      serviceUUID;
    u8                               charateristicsCount;
    BleGattDBDiscoveryCharacteristic dbChar[6];
};

}
