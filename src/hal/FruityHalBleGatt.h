////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// ** BlueRange Mesh – Community Edition (CE)
// ** Copyright (c) 2015-2021 MWAY DIGITAL GmbH, Germany
// ** Copyright (c) 2021-2026 BlueRange GmbH, Germany
// **
// ** This file is part of BlueRange Mesh Community Edition (formerly known as
// ** FruityMesh).
// **
// ** BlueRange Mesh Community Edition is free software: you can redistribute it
// ** and/or modify it under the terms of the GNU General Public License as
// ** published by the Free Software Foundation, either version 3 of the
// ** License, or (at your option) any later version.
// **
// ** BlueRange Mesh Community Edition is distributed in the hope that it will
// ** be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// ** See the GNU General Public License for more details.
// **
// ** You should have received a copy of the GNU General Public License along
// ** with this program. If not, see https://www.gnu.org/licenses/.
// **
// ** IMPORTANT:
// ** Any modification, extension, or derivative work of this file MUST also be
// ** licensed under the GNU General Public License v3 or later and the complete
// ** corresponding source code MUST be made available.
// **
// ** Commercial Use:
// ** If you wish to use this software without the obligations of the GPLv3
// ** (including source code disclosure), a commercial license for
// ** BlueRange Mesh OEM Edition is required.
// **
// ** License violations automatically terminate your rights under this license
// ** and may result in legal action under applicable law.
// ** For further information please use the contact form at:
// ** https://bluerange.io/en/contact
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
    u8                               characteristicsCount;
    BleGattDBDiscoveryCharacteristic dbChar[6];
};

}
