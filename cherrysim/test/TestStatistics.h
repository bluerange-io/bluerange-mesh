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
#include <CherrySimTester.h>
#include <CherrySimUtils.h>

//Helper function that checks a given message type for a maximum count and clears it if it was ok
void CheckAndClearStat(PacketStat* stat, MessageType mt, ModuleId moduleId, u32 minCount = 0, u32 maxCount = UINT32_MAX, u8 actionType = 0, u8 requestHandle = 0);
void CheckAndClearStat(PacketStat* stat, MessageType mt, ModuleIdWrapper moduleId, u32 minCount = 0, u32 maxCount = UINT32_MAX, u8 actionType = 0, u8 requestHandle = 0);

//After checking and clearing all stat entries we can check if it is empty with this function
void checkStatEmpty(PacketStat* stat);

//Useful for clearing a statistic e.g. after clustering to only check newly sent packets after some action
void clearStat(PacketStat* stat);