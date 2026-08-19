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
void DispatchButtonEvents(u8 buttonId, u32 buttonHoldTimeDs);
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

