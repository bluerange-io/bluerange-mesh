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
