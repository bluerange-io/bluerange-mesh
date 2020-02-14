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

#pragma once


//TODO: types.h should not include references to nrf sdk
#include "types.h"

#define _________________GAP_DEFINITIONS______________________

#define FH_BLE_SIZEOF_GAP_ADDR (7)
#define FH_BLE_GAP_ADDR_LEN (6)

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

struct BleGapAddr
{
public:
	BleGapAddrType addr_type;
	u8 addr[FH_BLE_GAP_ADDR_LEN];
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
#if SDK == 15
	ADV_IND         = 0x01,
	ADV_DIRECT_IND  = 0x03,
	ADV_SCAN_IND    = 0x04,
	ADV_NONCONN_IND = 0x05,
#else
	ADV_IND         = 0x00,
	ADV_DIRECT_IND  = 0x01,
	ADV_SCAN_IND    = 0x02,
	ADV_NONCONN_IND = 0x03,
#endif
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
	u8   longTermKey[BLE_GAP_SEC_KEY_LEN];
	u8   isGeneratedUsingLeSecureConnections  : 1;
	u8   isAuthenticatedKey : 1;
	u8   longTermKeyLength : 6; // in bytes
};

struct BleGapMasterId
{
	u16  encryptionDiversifier;
	u8   rand[BLE_GAP_SEC_RAND_LEN];
};
}
