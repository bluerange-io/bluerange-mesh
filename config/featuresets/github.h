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

//The following can be undefined to drastically change the size of the firmware
#define ENABLE_LOGGING //Undefine to remove most human readable logs
#define ENABLE_JSON_LOGGING //Undefine to remove json communication over uart
#define USE_UART //Undefine to remove the UART terminal
#define USE_SEGGER_RTT //Undefine to disable debugging over Segger Rtt

//The following shouldn't be modified
#define ACTIVATE_MA_MODULE

#ifdef NRF51
#define SET_FW_GROUPID_CHIPSET GROUP_ID_NRF51
#define SET_FW_GROUPID_FEATURESET GROUP_ID_NRF51_MESH
#else
#define SET_FW_GROUPID_CHIPSET GROUP_ID_NRF52
#define SET_FW_GROUPID_FEATURESET GROUP_ID_NRF52_MESH
#endif
