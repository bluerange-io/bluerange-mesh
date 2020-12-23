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
#pragma once
#include "FmTypes.h"

class ComponentSenseTimer
{
private:
    constexpr static u32 TIME_BETWEEN_REPEATS_BASE_DS = SEC_TO_DS(10); //The minimum time between repeated messages.
    constexpr static u32 TIME_BETWEEN_REPEATS_MAX_DS = SEC_TO_DS(/*one day*/ 60 * 60 * 24); //The maximum time between repeated messages
    constexpr static u32 RANDOM_EVENT_OFFSET_MAX_DS = SEC_TO_DS(1); //Random offset range to desync nodes and put less load spikes on the mesh.
    u32 timeSinceLastComponentSenseSentDs = 0;
    u32 amountOfRepeatedMessages = 0;
public:

    bool ShouldTrigger(u32 passedTimeDs);
    void Reset();

};
