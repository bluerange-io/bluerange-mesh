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

//For SDK17, we need to configure the translation layer
#define NRFX_USBD_ENABLED 1
#define NRFX_USBD_CONFIG_DMASCHEDULER_ISO_BOOST 1
#define NRFX_USBD_CONFIG_IRQ_PRIORITY 6

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

