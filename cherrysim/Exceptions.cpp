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
#include "Exceptions.h"
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
