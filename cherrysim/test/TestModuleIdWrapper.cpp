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
#include "gtest/gtest.h"
#include "Utility.h"
#include "CherrySimTester.h"
#include "CherrySimUtils.h"
#include "Logger.h"
#include "IoModule.h"
#include "VendorTemplateModule.h"

TEST(TestModuleIdWrapper, TestBasic) {
    //Check how the conversion feels
    VendorModuleId myId = Utility::GetWrappedModuleId(0xABCD, 1);

    //Check that our macro (for static initialization of variables) does the same as our helper function
    VendorModuleId myId2 = GET_VENDOR_MODULE_ID(0xABCD, 1);
    ASSERT_EQ(myId, myId2);

    //This is how the id should be printed and written in its entire hex format
    ASSERT_EQ((u32)myId, 0xABCD01F0UL);

    //Test if GetWrappedModuleId works
    ModuleIdWrapper testGetWrapped = Utility::GetWrappedModuleId(ModuleId::BEACONING_MODULE);
    ASSERT_EQ((u32)testGetWrapped, 0xFFFFFF01);

    //Test that getting the moduleId from a wrapped one works
    ModuleId unwrappedModuleId = Utility::GetModuleId(testGetWrapped);
    ASSERT_EQ(ModuleId::BEACONING_MODULE, unwrappedModuleId);

    //This is how we should be able to log and process the individual parts
    ModuleIdWrapperUnion wrapper;
    wrapper.wrappedModuleId = myId;
    ASSERT_EQ((u8)wrapper.prefix, 0xF0);
    ASSERT_EQ((u8)wrapper.subId, 1);
    ASSERT_EQ((u16)wrapper.vendorId, 0xABCD);

    //Test how building and checking a packet feels
    ConnPacketModule p1;
    u8* data1 = (u8*)&p1;
    u16 data1Length = sizeof(ConnPacketModule);
    p1.moduleId = ModuleId::STATUS_REPORTER_MODULE;

    ConnPacketModuleVendor p2;
    u8* data2 = (u8*)&p2;
    u16 data2Length = sizeof(ConnPacketModuleVendor);
    p2.moduleId = myId;

    if (data1Length >= SIZEOF_CONN_PACKET_MODULE) {
        ConnPacketModule* cpm = (ConnPacketModule*)data1;
        if (cpm->moduleId == ModuleId::STATUS_REPORTER_MODULE) {
            //This should happen
        }
        else {
            GTEST_FAIL();
        }
    }

    if (data2Length >= SIZEOF_CONN_PACKET_MODULE_VENDOR) {
        ConnPacketModuleVendor* cpme = (ConnPacketModuleVendor*)data2;
        if (cpme->moduleId == myId) {
            //This should happen
        }
        else {
            GTEST_FAIL();
        }
    }
}