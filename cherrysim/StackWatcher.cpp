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
#if !defined(GITHUB_RELEASE) && !defined(__clang__) && !defined(SANITIZERS_ENABLED)
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
