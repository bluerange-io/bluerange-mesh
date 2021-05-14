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
 * This is the main class that initializes the SoftDevice and starts the code.
 * It contains error handlers for all unfetched errors.
 */

#pragma once

#include <FmTypes.h>
#include <Config.h>
#include <FruityHal.h>

//FruityMesh
void BootFruityMesh();
void BootModules();
void StartFruityMesh();

//Event dispatchers
void DispatchSystemEvents(FruityHal::SystemEvents sys_evt);
void DispatchButtonEvents(u8 buttonId, u32 buttonHoldTime);
void DispatchTimerEvents(u16 passedTimeDs);

void DispatchEvent(const FruityHal::GapRssiChangedEvent& e);
void DispatchEvent(const FruityHal::GapAdvertisementReportEvent& e);
void DispatchEvent(const FruityHal::GapConnectedEvent& e);
void DispatchEvent(const FruityHal::GapDisconnectedEvent& e);
void DispatchEvent(const FruityHal::GapTimeoutEvent& e);
void DispatchEvent(const FruityHal::GapSecurityInfoRequestEvent& e);
void DispatchEvent(const FruityHal::GapConnectionSecurityUpdateEvent& e);
#if IS_ACTIVE(CONN_PARAM_UPDATE)
void DispatchEvent(const FruityHal::GapConnParamUpdateEvent & e);
void DispatchEvent(const FruityHal::GapConnParamUpdateRequestEvent & e);
#endif
void DispatchEvent(const FruityHal::GattcWriteResponseEvent& e);
void DispatchEvent(const FruityHal::GattcTimeoutEvent& e);
void DispatchEvent(const FruityHal::GattsWriteEvent& e);
void DispatchEvent(const FruityHal::GattcHandleValueEvent& e);
void DispatchEvent(const FruityHal::GattDataTransmittedEvent& e);

//Error handlers
void FruityMeshErrorHandler(u32 err);
void BleStackErrorHandler(u32 id, u32 pc, u32 info);
void HardFaultErrorHandler(stacked_regs_t* stack);

//Other
void CheckRamRetainStruct();

