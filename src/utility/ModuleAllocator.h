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

/* 
*  A stack allocator that gives memory to module allocations.
*/
class ModuleAllocator 
{
private:
    u8 *currentDataPtr = nullptr;
    u32 sizeLeft = 0;
    u32 startSize = 0;

public:
    ModuleAllocator();

    ModuleAllocator           (const ModuleAllocator&)  = delete;
    ModuleAllocator           (      ModuleAllocator&&) = delete;
    ModuleAllocator& operator=(const ModuleAllocator&)  = delete;
    ModuleAllocator& operator=(      ModuleAllocator&&) = delete;

    void SetMemory(u8 *block, u32 size);

    u32 GetMemorySize();

    void* AllocateMemory(u32 size);
};
