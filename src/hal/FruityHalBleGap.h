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
 * This file includes platform independent definitions for the BLE GAP layer.
 */

#include <array>

#include "FmTypes.h"

#define _________________GAP_DEFINITIONS______________________

// Check https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/
enum class BleGapAdType
{
    TYPE_FLAGS                               = 0x01,
    TYPE_16BIT_SERVICE_UUID_MORE_AVAILABLE   = 0x02,
    TYPE_16BIT_SERVICE_UUID_COMPLETE         = 0x03,
    TYPE_32BIT_SERVICE_UUID_MORE_AVAILABLE   = 0x04,
    TYPE_32BIT_SERVICE_UUID_COMPLETE         = 0x05,
    TYPE_128BIT_SERVICE_UUID_MORE_AVAILABLE  = 0x06,
    TYPE_128BIT_SERVICE_UUID_COMPLETE        = 0x07,
    TYPE_SHORT_LOCAL_NAME                    = 0x08,
    TYPE_COMPLETE_LOCAL_NAME                 = 0x09,
    TYPE_TX_POWER_LEVEL                      = 0x0A,
    TYPE_CLASS_OF_DEVICE                     = 0x0D,
    TYPE_SIMPLE_PAIRING_HASH_C               = 0x0E,
    TYPE_SIMPLE_PAIRING_RANDOMIZER_R         = 0x0F,
    TYPE_SECURITY_MANAGER_TK_VALUE           = 0x10,
    TYPE_SECURITY_MANAGER_OOB_FLAGS          = 0x11,
    TYPE_SLAVE_CONNECTION_INTERVAL_RANGE     = 0x12,
    TYPE_SOLICITED_SERVICE_UUIDS_16BIT       = 0x14,
    TYPE_SOLICITED_SERVICE_UUIDS_128BIT      = 0x15,
    TYPE_SERVICE_DATA                        = 0x16,
    TYPE_PUBLIC_TARGET_ADDRESS               = 0x17,
    TYPE_RANDOM_TARGET_ADDRESS               = 0x18,
    TYPE_APPEARANCE                          = 0x19,
    TYPE_ADVERTISING_INTERVAL                = 0x1A,
    TYPE_LE_BLUETOOTH_DEVICE_ADDRESS         = 0x1B,
    TYPE_LE_ROLE                             = 0x1C,
    TYPE_SIMPLE_PAIRING_HASH_C256            = 0x1D,
    TYPE_SIMPLE_PAIRING_RANDOMIZER_R256      = 0x1E,
    TYPE_SERVICE_DATA_32BIT_UUID             = 0x20,
    TYPE_SERVICE_DATA_128BIT_UUID            = 0x21,
    TYPE_LESC_CONFIRMATION_VALUE             = 0x22,
    TYPE_LESC_RANDOM_VALUE                   = 0x23,
    TYPE_URI                                 = 0x24,
    TYPE_3D_INFORMATION_DATA                 = 0x3D,
    TYPE_MANUFACTURER_SPECIFIC_DATA          = 0xFF,
};

constexpr u8 FH_BLE_GAP_ADV_FLAG_LE_LIMITED_DISC_MODE = 0x01;
constexpr u8 FH_BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE = 0x02;
constexpr u8 FH_BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED = 0x04;
constexpr u8 FH_BLE_GAP_ADV_FLAG_LE_BR_EDR_CONTROLLER = 0x08;
constexpr u8 FH_BLE_GAP_ADV_FLAG_LE_BR_EDR_HOST = 0x10;

constexpr u8 FH_BLE_GAP_SECURITY_RANDOM_LEN = 8;
constexpr u8 FH_BLE_GAP_SECURITY_KEY_LEN = 16;

constexpr u8 FH_BLE_SIZEOF_GAP_ADDR = 7;
constexpr u8 FH_BLE_GAP_ADDR_LEN = 6;

#define FH_CONNECTION_SECURITY_MODE_SET_NO_ACCESS(ptr)          (ptr)->securityMode = 0; \
                                                                                                                            (ptr)->level = 0;
#define FH_CONNECTION_SECURITY_MODE_SET_OPEN(ptr)               (ptr)->securityMode = 1; \
                                                                                                                            (ptr)->level = 1;
#define FH_CONNECTION_SECURITY_MODE_SET_ENC_NO_MITM(ptr)        (ptr)->securityMode = 1; \
                                                                                                                            (ptr)->level = 2;
#define FH_CONNECTION_SECURITY_MODE_SET_ENC_WITH_MITM(ptr)      (ptr)->securityMode = 1; \
                                                                                                                            (ptr)->level = 3;
#define FH_CONNECTION_SECURITY_MODE_SET_LESC_ENC_WITH_MITM(ptr) (ptr)->securityMode = 1; \
                                                                                                                            (ptr)->level = 4;
#define FH_CONNECTION_SECURITY_MODE_SET_SIGNED_NO_MITM(ptr)     (ptr)->securityMode = 2; \
                                                                                                                            (ptr)->level = 1;
#define FH_CONNECTION_SECURITY_MODE_SET_SIGNED_WITH_MITM(ptr)   (ptr)->securityMode = 2; \
                                                                                                                            (ptr)->level = 2;

namespace FruityHal
{
enum class BleGapAddrType : u8
{
    PUBLIC                        = 0x00,
    RANDOM_STATIC                 = 0x01,
    RANDOM_PRIVATE_RESOLVABLE     = 0x02,
    RANDOM_PRIVATE_NON_RESOLVABLE = 0x03,
    INVALID                       = 0xFF
};

typedef std::array<u8, FH_BLE_GAP_ADDR_LEN> BleGapAddrBytes;

struct BleGapAddr
{
public:
    BleGapAddrType addr_type;
    BleGapAddrBytes addr;
};

struct BleGapScanParams
{
    u16 interval;
    u16 window;
    u16 timeout;
};


// Mask for RF advertising channels. Setting bit to 1 will turn off advertising on related channel
struct BleGapAdvChMask
{
    u8 ch37Off : 1;
    u8 ch38Off : 1;
    u8 ch39Off : 1;
};

enum class BleGapAdvType : u8
{
    ADV_IND         = 0x00,
    ADV_DIRECT_IND  = 0x01,
    ADV_SCAN_IND    = 0x02,
    ADV_NONCONN_IND = 0x03,
};

struct BleGapAdvParams
{
    BleGapAdvType    type;
    u16              interval;
    u16              timeout;
    BleGapAdvChMask  channelMask;
};


// securityMode 0 level 0: No access permissions at all.
// securityMode 1 level 1: No Security (No authentication and no encryption)
// securityMode 1 level 2: Unauthenticated pairing with encryption
// securityMode 1 level 3: Authenticated pairing with AES-CCM encryption
// securityMode 1 level 4: Authenticated LE Secure Connections pairing with encryption.
// securityMode 2 level 1: Unauthenticated pairing with data signing
// securityMode 2 level 2: Authenticated pairing with data signing
struct BleGapConnSecMode
{
    u8 securityMode : 4;
    u8 level : 4;
};

struct BleGapConnParams
{
    u16 minConnInterval;
    u16 maxConnInterval;
    u16 slaveLatency;
    u16 connSupTimeout;
};


struct BleGapEncInfo
{
    u8   longTermKey[FH_BLE_GAP_SECURITY_KEY_LEN];
    u8   isGeneratedUsingLeSecureConnections  : 1;
    u8   isAuthenticatedKey : 1;
    u8   longTermKeyLength : 6; // in bytes
};

struct BleGapMasterId
{
    u16  encryptionDiversifier;
    u8   rand[FH_BLE_GAP_SECURITY_RANDOM_LEN];
};
}
