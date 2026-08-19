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
#include "Exceptions.h"
#include "CherrySim.h"
#include <map>

static std::map<std::type_index, int> ignoredExceptions;
static int disableDebugBreakOnExceptionCounter = 0;

bool Exceptions::GetDebugBreakOnException()
{
    return disableDebugBreakOnExceptionCounter <= 0;
}

void Exceptions::DisableExceptionByIndex(std::type_index index)
{
    if (ignoredExceptions.find(index) == ignoredExceptions.end()) {
        ignoredExceptions.insert({ index, 1 });
    }
    else {
        ignoredExceptions[index]++;
    }
}

void Exceptions::EnableExceptionByIndex(std::type_index index)
{
    if (ignoredExceptions.find(index) == ignoredExceptions.end()) {
        SIMEXCEPTION(MemoryCorruptionException);
    }
    else {
        ignoredExceptions[index]--;
        if (ignoredExceptions[index] <= 0)
        {
            ignoredExceptions.erase(index);
        }
    }
}

bool Exceptions::IsExceptionEnabledByIndex(std::type_index index)
{
    //Check if all non-critical exceptions should be ignored
    if (cherrySimInstance != nullptr && cherrySimInstance->simConfig.disableNonCriticalExceptions) return false;

    if (ignoredExceptions.find(index) == ignoredExceptions.end()) {
        return true;
    }
    else {
        return ignoredExceptions[index] <= 0;
    }
}

Exceptions::DisableDebugBreakOnException::DisableDebugBreakOnException()
{
    disableDebugBreakOnExceptionCounter++;
}

Exceptions::DisableDebugBreakOnException::~DisableDebugBreakOnException() noexcept(false)
{
    disableDebugBreakOnExceptionCounter--;
    if (disableDebugBreakOnExceptionCounter < 0)
    {
        SIMEXCEPTION(MemoryCorruptionException);
    }
}
