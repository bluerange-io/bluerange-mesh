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
#include "ComponentSenseTimer.h"
#include "Utility.h"

bool ComponentSenseTimer::ShouldTrigger(u32 passedTimeDs)
{
    timeSinceLastComponentSenseSentDs += passedTimeDs;
    u32 additionalWaitTime = amountOfRepeatedMessages * amountOfRepeatedMessages;
    if (additionalWaitTime > TIME_BETWEEN_REPEATS_MAX_DS)
    {
        additionalWaitTime = TIME_BETWEEN_REPEATS_MAX_DS;
    }

    if (timeSinceLastComponentSenseSentDs > TIME_BETWEEN_REPEATS_BASE_DS + RANDOM_EVENT_OFFSET_MAX_DS + additionalWaitTime)
    {
        timeSinceLastComponentSenseSentDs = Utility::GetRandomInteger() % RANDOM_EVENT_OFFSET_MAX_DS; //Desync nodes to put less load spikes on mesh.
        amountOfRepeatedMessages++;
        return true;
    }
    return false;
}

void ComponentSenseTimer::Reset()
{
    timeSinceLastComponentSenseSentDs = 0;
    amountOfRepeatedMessages = 0;
}

