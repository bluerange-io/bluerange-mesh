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
 * This file includes BLE types not directly associated with a protocol layer.
 */

#pragma once

#include "FmTypes.h"

namespace FruityHal
{

constexpr u16 FH_BLE_INVALID_HANDLE = 0xFFFF;

enum class BleAppearance
{
    UNKNOWN                             = 0,
    GENERIC_PHONE                       = 64,
    GENERIC_COMPUTER                    = 128,
    GENERIC_WATCH                       = 192,
    WATCH_SPORTS_WATCH                  = 193,
    GENERIC_CLOCK                       = 256,
    GENERIC_DISPLAY                     = 320,
    GENERIC_REMOTE_CONTROL              = 384,
    GENERIC_EYE_GLASSES                 = 448,
    GENERIC_TAG                         = 512,
    GENERIC_KEYRING                     = 576,
    GENERIC_MEDIA_PLAYER                = 640,
    GENERIC_BARCODE_SCANNER             = 704,
    GENERIC_THERMOMETER                 = 768,
    THERMOMETER_EAR                     = 769,
    GENERIC_HEART_RATE_SENSOR           = 832,
    HEART_RATE_SENSOR_HEART_RATE_BELT   = 833,
    GENERIC_BLOOD_PRESSURE              = 896,
    BLOOD_PRESSURE_ARM                  = 897,
    BLOOD_PRESSURE_WRIST                = 898,
    GENERIC_HID                         = 960,
    HID_KEYBOARD                        = 961,
    HID_MOUSE                           = 962,
    HID_JOYSTICK                        = 963,
    HID_GAMEPAD                         = 964,
    HID_DIGITIZERSUBTYPE                = 965,
    HID_CARD_READER                     = 966,
    HID_DIGITAL_PEN                     = 967,
    HID_BARCODE                         = 968,
    GENERIC_GLUCOSE_METER               = 1024,
    GENERIC_RUNNING_WALKING_SENSOR      = 1088,
    RUNNING_WALKING_SENSOR_IN_SHOE      = 1089,
    RUNNING_WALKING_SENSOR_ON_SHOE      = 1090,
    RUNNING_WALKING_SENSOR_ON_HIP       = 1091,
    GENERIC_CYCLING                     = 1152,
    CYCLING_CYCLING_COMPUTER            = 1153,
    CYCLING_SPEED_SENSOR                = 1154,
    CYCLING_CADENCE_SENSOR              = 1155,
    CYCLING_POWER_SENSOR                = 1156,
    CYCLING_SPEED_CADENCE_SENSOR        = 1157,
    GENERIC_PULSE_OXIMETER              = 3136,
    PULSE_OXIMETER_FINGERTIP            = 3137,
    PULSE_OXIMETER_WRIST_WORN           = 3138,
    GENERIC_WEIGHT_SCALE                = 3200,
    GENERIC_OUTDOOR_SPORTS_ACT          = 5184,
    OUTDOOR_SPORTS_ACT_LOC_DISP         = 5185,
    OUTDOOR_SPORTS_ACT_LOC_AND_NAV_DISP = 5186,
    OUTDOOR_SPORTS_ACT_LOC_POD          = 5187,
    OUTDOOR_SPORTS_ACT_LOC_AND_NAV_POD  = 5188,
};

}
