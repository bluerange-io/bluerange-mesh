////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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

#include "types.h"
#include "StackWatcher.h"
#include "Exceptions.h"
#include <cstdio> //for std::size_t

std::vector<void*> StackWatcher::stackBase;
u32 StackWatcher::disableValue = 0;

StackWatcher::StackWatcher()
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

	const u32 uncleanedStackSize = (char*)StackWatcher::stackBase.back() - (char*)&someDummyStackVariable;
	const u32 cleanedStackSize = uncleanedStackSize - StackWatcher::stackBase.size() * sizeof(StackBaseSetter);

	//static on purpose. This variable should track the biggest Stack usage accross all nodes and all simulations.
	static u32 biggest = 0;
	if (cleanedStackSize > biggest)
	{
		biggest = cleanedStackSize;
		printf("BIGGEST: %u\n", biggest);
	}
	if (cleanedStackSize > 10000)
	{
#ifndef GITHUB_RELEASE
		SIMEXCEPTION(StackOverflowException);
#else
		//The "GITHUB_RELEASE" configuration executes only github featuresets which, by definition, consume much more RAM.
#endif //GITHUB_RELEASE
	}
}

StackBaseSetter::StackBaseSetter()
{
	int someDummyStackVariable = 0;
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
