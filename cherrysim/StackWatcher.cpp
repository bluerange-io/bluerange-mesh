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

#include "FmTypes.h"
#include "StackWatcher.h"
#include "Exceptions.h"
#include <cstdio> //for std::size_t

std::vector<const void*> StackWatcher::stackBase;
u32 StackWatcher::disableValue = 0;

void StackWatcher::Check()
{
    if (stackBase.size() == 0)
    {
        //Test is disabled if no stack base is set.
        return;
    }
    if (StackWatcher::disableValue != 0)
    {
        return;
    }

    int someDummyStackVariable = 0;

    const u32 uncleanedStackSize = (const char*)StackWatcher::stackBase.back() - (const char*)&someDummyStackVariable;
    const u32 cleanedStackSize = uncleanedStackSize - sizeof(StackBaseSetter);

    if (cleanedStackSize > 12000)
    {
#if !defined(GITHUB_RELEASE) && !defined(__clang__) && !defined(SANITIZERS_ENABLED) && !defined(__SANITIZE_ADDRESS__)
        SIMEXCEPTION(StackOverflowException);
#else
        //The "GITHUB_RELEASE" configuration executes only github featuresets which, by definition, consume much more RAM.
        //__clang__ has vastly different stack frames and is thus not supported as well. As this is just a sanity check,
        //supporting one compiler for the pipeline and one for local runs is sufficient.
        //If sanitizers are enabled, the RAM stack usage is a lot higher than without so we cannot do any useful testing here
#endif //GITHUB_RELEASE
    }
}

StackBaseSetter::StackBaseSetter()
{
    const int someDummyStackVariable = 0;
    // Suppressing the following check is okay because we don't dereference the pointer
    // given to the container anywhere. We just care about value of the pointer itself.
    // cppcheck-suppress danglingLifetime
    StackWatcher::stackBase.push_back(&someDummyStackVariable);
}

StackBaseSetter::~StackBaseSetter()
{
    StackWatcher::stackBase.pop_back();
}

StackWatcherDisabler::StackWatcherDisabler()
{
    StackWatcher::disableValue++;
}

StackWatcherDisabler::~StackWatcherDisabler()
{
    StackWatcher::disableValue--;
}
