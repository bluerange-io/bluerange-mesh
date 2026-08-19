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

#ifdef SIM_ENABLED

#include <SimulatedTimers.h>
#include <CherrySim.h>
#include <app_timer.h>
#include <map>
#include <vector>

/**
 * @brief Represents a simulated timer in the CherrySim environment
 */
struct SimTimer {
    /** @brief Whether the timer is currently running */
    bool active;

    /** @brief The timer mode (repeated or single shot) */
    app_timer_mode_t mode;

    /** @brief The timeout interval in milliseconds */
    u32 timeoutMs;

    /** @brief The absolute simulation time (ms) when the timer will next fire */
    u32 nextFireTime;

    /** @brief The callback function to execute when the timer fires */
    FruityHal::TimerHandler handler;

    /** @brief Pointer to the owning node context */
    void* owner;
};

/**
 * @brief Map of simulated timers
 */
static std::map<FruityHal::swTimer, SimTimer> simTimers;

ErrorType CherrySimTimers_CreateTimer(FruityHal::swTimer& timer, bool repeated, FruityHal::TimerHandler handler)
{
    app_timer_mode_t mode = repeated ? APP_TIMER_MODE_REPEATED : APP_TIMER_MODE_SINGLE_SHOT;

    static u32 timerIdCounter = 0;
    timer = new u32(++timerIdCounter);

    SimTimer t;
    t.active = false;
    t.mode = mode;
    t.handler = handler;
    t.owner = cherrySimInstance ? (void*)cherrySimInstance->currentNode : nullptr;
    simTimers[timer] = t;

    return ErrorType::SUCCESS;
}

ErrorType CherrySimTimers_StartTimer(FruityHal::swTimer timer, u32 timeoutMs)
{
    if (simTimers.find(timer) == simTimers.end()) {
        return ErrorType::INVALID_PARAM;
    }

    if (timeoutMs == 0) {
        return ErrorType::INVALID_PARAM;
    }

    simTimers[timer].timeoutMs = timeoutMs;
    simTimers[timer].nextFireTime = FruityHal::GetRtcMs() + timeoutMs;
    simTimers[timer].active = true;

    return ErrorType::SUCCESS;
}

ErrorType CherrySimTimers_StopTimer(FruityHal::swTimer timer)
{
    if (simTimers.find(timer) == simTimers.end()) {
        return ErrorType::INVALID_PARAM;
    }

    simTimers[timer].active = false;

    return ErrorType::SUCCESS;
}

void CherrySimTimers_SimulateTimers(void* nodePtr)
{
    u32 now = FruityHal::GetRtcMs();

    // In case timers are modified during processing, we first collect all
    // relevant timer IDs for this node

    std::vector<FruityHal::swTimer> timersToProcess;
    for (auto& pair : simTimers) {
        if (pair.second.owner == nodePtr && pair.second.active) {
            timersToProcess.push_back(pair.first);
        }
    }

    for (auto handle : timersToProcess) {
        // Prevent infinite loops in case of misbehaving timers
        int loopLimit = 100;

        // Loop to handle repeated firings if the timer is overdue
        while (loopLimit-- > 0) {
            auto it = simTimers.find(handle);
            if (it == simTimers.end()) break; // Timer was removed in the meantime

            SimTimer timer = it->second;
            if (!timer.active || (i32)(now - timer.nextFireTime) < 0) break;

            if (timer.mode == APP_TIMER_MODE_REPEATED && timer.timeoutMs > 0) {
                timer.nextFireTime += timer.timeoutMs;
            } else {
                timer.active = false;
            }

            it->second = timer;

            if (timer.handler) timer.handler(nullptr);
        }
    }
}

void CherrySimTimers_FreeTimers(void* nodePtr)
{
    for (auto it = simTimers.begin(); it != simTimers.end();) {
        if (it->second.owner == nodePtr) {
            delete it->first;
            it = simTimers.erase(it);
            continue;
        }

        it++;
    }
}

#endif // SIM_ENABLED
