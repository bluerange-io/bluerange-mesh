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
#include "FruityMesh.h"
#include "GlobalState.h"
#include "FruityHal.h"

int main(void)
{
    DYNAMIC_ARRAY(halMemory, FruityHal::GetHalMemorySize());
    CheckedMemset(halMemory, 0, FruityHal::GetHalMemorySize());
    GS->halMemory = halMemory;
    
    BootFruityMesh();
    GS->fruityMeshBooted = true;
    
    const u32 moduleMemoryBlockSize = INITIALIZE_MODULES(false);
    //We must make sure that the memory block for allocating modules is aligned on an 8 byte boundary
    //This allows us to support 4 and 8 byte aligned modules
    alignas(8) u8 moduleMemoryBlock[moduleMemoryBlockSize];
    GS->moduleAllocator.SetMemory(moduleMemoryBlock, moduleMemoryBlockSize);
    BootModules();
    GS->modulesBooted = true;
    
    StartFruityMesh();
}
