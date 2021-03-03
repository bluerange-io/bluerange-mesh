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
#include <CherrySimTester.h>
#include <CherrySimUtils.h>

//Helper function that checks a given message type for a maximum count and clears it if it was ok
void CheckAndClearStat(PacketStat* stat, MessageType mt, ModuleId moduleId, u32 minCount = 0, u32 maxCount = UINT32_MAX, u8 actionType = 0, u8 requestHandle = 0);
void CheckAndClearStat(PacketStat* stat, MessageType mt, ModuleIdWrapper moduleId, u32 minCount = 0, u32 maxCount = UINT32_MAX, u8 actionType = 0, u8 requestHandle = 0);

//After checking and clearing all stat entries we can check if it is empty with this function
void checkStatEmpty(PacketStat* stat);

//Useful for clearing a statistic e.g. after clustering to only check newly sent packets after some action
void clearStat(PacketStat* stat);