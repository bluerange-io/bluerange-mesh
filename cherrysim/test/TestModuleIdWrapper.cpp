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