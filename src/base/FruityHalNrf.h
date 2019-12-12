////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2019 M-Way Solutions GmbH
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

#include <FruityHal.h>

extern "C"{
#include <ble.h>
#include <ble_gap.h>
#include <ble_gatt.h>
#include <ble_gatts.h>
#include <nrf_soc.h>
#include "app_timer.h"

#ifndef SIM_ENABLED
#include <nrf_sdm.h>
#include <nrf_delay.h>
#include <nrf_drv_gpiote.h>
#include <nrf_nvic.h>
#include <nrf_wdt.h>
#include <ble_radio_notification.h>
#include <ble_db_discovery.h>
#else
#include <nrf51_bitfields.h>
#endif


#if defined(NRF51) || defined(SIM_ENABLED)
#include <softdevice_handler.h>
#endif

}
