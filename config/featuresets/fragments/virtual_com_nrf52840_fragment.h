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

//This fragment is used for enabling the virtual com port feature

#pragma once


// ########## FruityMesh configuration #########

#define ACTIVATE_VIRTUAL_COM_PORT 1

// ########## NRF specific configuration #########

//Enable necessary dependencies
#define NRF_CLOCK_ENABLED 1
#define CLOCK_ENABLED 1
#define POWER_ENABLED 1

//Enable the USB Device Driver
#define USBD_ENABLED 1
#define USBD_POWER_DETECTION true
#define USBD_CONFIG_DMASCHEDULER_MODE 0
#define USBD_CONFIG_DMASCHEDULER_ISO_BOOST 1

//Enable the App USB Device Library
#define APP_USBD_ENABLED 1
#define APP_USBD_CONFIG_POWER_EVENTS_PROCESS 1
#define APP_USBD_CONFIG_EVENT_QUEUE_ENABLE 1
#define APP_USBD_CONFIG_EVENT_QUEUE_SIZE 128
#define APP_USBD_STRING_ID_MANUFACTURER 1
#define APP_USBD_STRING_ID_PRODUCT 2
#define APP_USBD_STRING_ID_SERIAL 3
#define APP_USBD_STRING_ID_CONFIGURATION 4
#define APP_USBD_STRINGS_USER X(APP_USER_1, , APP_USBD_STRING_DESC("User 1"))
#define APP_USBD_CONFIG_SELF_POWERED 1
#define APP_USBD_CONFIG_MAX_POWER 500
#define APP_USBD_CONFIG_DESC_STRING_UTF_ENABLED 0
#define APP_USBD_STRINGS_LANGIDS APP_USBD_LANG_AND_SUBLANG(APP_USBD_LANG_ENGLISH, APP_USBD_SUBLANG_ENGLISH_US)
#define APP_USBD_STRINGS_MANUFACTURER APP_USBD_STRING_DESC("M-Way Solutions")
#define APP_USBD_STRINGS_PRODUCT APP_USBD_STRING_DESC("BlueRange Mesh Bridge")
#define APP_USBD_STRING_SERIAL_EXTERN 1
#define APP_USBD_STRING_SERIAL g_extern_serial_number
#define APP_USBD_STRINGS_CONFIGURATION APP_USBD_STRING_DESC("Default configuration")
#define APP_USBD_CONFIG_DESC_STRING_SIZE 31

#define APP_USBD_VID (0x1915) //Nordic Vendor Id can be used according to devzone
#define APP_USBD_PID (0x520F) //PID according to usbd_cdc_acm example

#define APP_USBD_DEVICE_VER_MAJOR 1
#define APP_USBD_DEVICE_VER_MINOR 0

//Enable the USBD CDC ACM library (Virtual Com Port)
#define APP_USBD_CDC_ACM_ENABLED 1
#define APP_USBD_CDC_ACM_ZLP_ON_EPSIZE_WRITE 1

