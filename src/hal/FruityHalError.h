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
 * This is a collection of BLE error types.
 */

#pragma once
#include "FmTypes.h"

namespace FruityHal
{

// Standard Bluetooth HCI errors
enum class BleHciError : u8
{
    SUCCESS                                     = 0x00,
    UNKNOWN_BLE_COMMAND                         = 0x01,
    UNKNOWN_CONNECTION_IDENTIFIER               = 0x02,
    AUTHENTICATION_FAILURE                      = 0x05,
    PIN_OR_KEY_MISSING                          = 0x06,
    MEMORY_CAPACITY_EXCEEDED                    = 0x07,
    CONNECTION_TIMEOUT                          = 0x08,
    COMMAND_DISALLOWED                          = 0x0C,
    INVALID_BLE_COMMAND_PARAMETERS              = 0x12,
    REMOTE_USER_TERMINATED_CONNECTION           = 0x13,
    REMOTE_DEV_TERMINATION_DUE_TO_LOW_RESOURCES = 0x14,
    REMOTE_DEV_TERMINATION_DUE_TO_POWER_OFF     = 0x15,
    LOCAL_HOST_TERMINATED_CONNECTION            = 0x16,
    UNSUPPORTED_REMOTE_FEATURE                  = 0x1A,
    INVALID_LMP_PARAMETERS                      = 0x1E,
    UNSPECIFIED_ERROR                           = 0x1F,
    LMP_RESPONSE_TIMEOUT                        = 0x22,
    LMP_ERROR_TRANSACTION_COLLISION             = 0x23,
    LMP_PDU_NOT_ALLOWED                         = 0x24,
    INSTANT_PASSED                              = 0x28,
    PAIRING_WITH_UNIT_KEY_UNSUPPORTED           = 0x29,
    DIFFERENT_TRANSACTION_COLLISION             = 0x2A,
    PARAMETER_OUT_OF_MANDATORY_RANGE            = 0x30,
    CONTROLLER_BUSY                             = 0x3A,
    CONN_INTERVAL_UNACCEPTABLE                  = 0x3B,
    DIRECTED_ADVERTISER_TIMEOUT                 = 0x3C,
    CONN_TERMINATED_DUE_TO_MIC_FAILURE          = 0x3D,
    CONN_FAILED_TO_BE_ESTABLISHED               = 0x3E,
};

// Standard Bluetooth GATT Errors 
enum class BleGattEror : u8
{  
    SUCCESS                    = 0x00,
    UNKNOWN                    = 0x01,
    READ_NOT_PERMITTED         = 0x02,
    WRITE_NOT_PERMITTED        = 0x03,
    INVALID_PDU                = 0x04,
    INSUF_AUTHENTICATION       = 0x05,
    REQUEST_NOT_SUPPORTED      = 0x06,
    INVALID_OFFSET             = 0x07,
    INSUF_AUTHORIZATION        = 0x08,
    PREPARE_QUEUE_FULL         = 0x09,
    ATTRIBUTE_NOT_FOUND        = 0x0A,
    ATTRIBUTE_NOT_LONG         = 0x0B,
    INSUF_ENC_KEY_SIZE         = 0x0C,
    INVALID_ATT_VAL_LENGTH     = 0x0D,
    UNLIKELY_ERROR             = 0x0E,
    INSUF_ENCRYPTION           = 0x0F,
    UNSUPPORTED_GROUP_TYPE     = 0x10,
    INSUF_RESOURCES            = 0x11,
    RFU_RANGE1_BEGIN           = 0x12,
    RFU_RANGE1_END             = 0x7F,
    APP_BEGIN                  = 0x80,
    APP_END                    = 0x9F,
    RFU_RANGE2_BEGIN           = 0xA0,
    RFU_RANGE2_END             = 0xDF,
    RFU_RANGE3_BEGIN           = 0xE0,
    RFU_RANGE3_END             = 0xFC,
    CPS_CCCD_CONFIG_ERROR      = 0xFD,
    CPS_PROC_ALR_IN_PROG       = 0xFE,
    CPS_OUT_OF_RANGE           = 0xFF 
};

}
