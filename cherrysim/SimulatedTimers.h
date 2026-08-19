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

#pragma once

#ifdef SIM_ENABLED

#include <FruityHal.h>

/**
 * @brief Creates a simulated timer.
 *
 * @param[out] timer    Handle to the created timer
 * @param[in]  repeated True for a repeating timer, false for a one-shot timer
 * @param[in]  handler  Callback invoked when the timer fires
 *
 * @return ErrorType::SUCCESS on success
 */
ErrorType CherrySimTimers_CreateTimer(FruityHal::swTimer &timer, bool repeated, FruityHal::TimerHandler handler);

/**
 * @brief Starts a simulated timer.
 *
 * @param[in] timer     Handle of the timer to start
 * @param[in] timeoutMs Timeout in milliseconds
 *
 * @return ErrorType::SUCCESS on success
 */
ErrorType CherrySimTimers_StartTimer(FruityHal::swTimer timer, u32 timeoutMs);

/**
 * @brief Stops a simulated timer.
 *
 * @param[in] timer Handle of the timer to stop
 *
 * @return ErrorType::SUCCESS on success
 */
ErrorType CherrySimTimers_StopTimer(FruityHal::swTimer timer);

/**
 * @brief Simulates the progression of timers for a specific node.
 *        Should be called once per simulation tick for each node.
 *
 * @param[in] nodePtr Pointer to the node whose timers should be simulated
 */
void CherrySimTimers_SimulateTimers(void *nodePtr);

/**
 * @brief Frees all timers associated with a specific node.
 *        Should be called when a node is shut down.
 *
 * @param[in] nodePtr Pointer to the node whose timers should be freed
 */
void CherrySimTimers_FreeTimers(void *nodePtr);

#endif // SIM_ENABLED
