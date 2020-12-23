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
#include <string>

TEST(TestLogger, TestTags) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert( { "prod_mesh_nrf52", 2 } );
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    NodeIndexSetter setter(0);
    auto tag = "TEST123";
    ASSERT_FALSE(Logger::GetInstance().IsTagEnabled(tag));
    Logger::GetInstance().EnableTag(tag);
    ASSERT_TRUE (Logger::GetInstance().IsTagEnabled(tag));
    Logger::GetInstance().DisableTag(tag);
    ASSERT_FALSE(Logger::GetInstance().IsTagEnabled(tag));
    Logger::GetInstance().ToggleTag(tag);
    ASSERT_TRUE (Logger::GetInstance().IsTagEnabled(tag));
    Logger::GetInstance().ToggleTag(tag);
    ASSERT_FALSE(Logger::GetInstance().IsTagEnabled(tag));
}

TEST(TestLogger, TestParseHexStringToBuffer) 
{
    {
        auto string = "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF";
        u8 buffer[1024];
        u8 prediction[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

        ASSERT_EQ(Logger::ParseEncodedStringToBuffer(string, buffer, sizeof(buffer)), 16);
        for (size_t i = 0; i < sizeof(prediction); i++)
        {
            ASSERT_EQ(buffer[i], prediction[i]);
        }
    }

    {
        auto string = "AA";
        u8 buffer[1024];
        u8 prediction[] = { 0xAA };

        ASSERT_EQ(Logger::ParseEncodedStringToBuffer(string, buffer, sizeof(buffer)), 1);
        for (size_t i = 0; i < sizeof(prediction); i++)
        {
            ASSERT_EQ(buffer[i], prediction[i]);
        }
    }

    {
        auto string = "";
        u8 buffer[1024] = {};

        ASSERT_EQ(Logger::ParseEncodedStringToBuffer(string, buffer, sizeof(buffer)), 0);
        for (size_t i = 0; i < sizeof(buffer); i++)
        {
            ASSERT_EQ(buffer[i], 0);
        }
    }
}

void fillMemoryGuard(u8* data, u32 length) {
    for (u32 i = 0; i < length; i++) {
        data[i] = (u8)((i + 100) % 255);
    }
}

void checkMemoryGuard(const u8* data, u32 length) {
    for (u32 i = 0; i < length; i++) {
        u8 expectedData = (u8)((i + 100) % 255);
        if (data[i] != expectedData) {
            FAIL() << "Memoryguard got corrupted!"; //LCOV_EXCL_LINE assertion
        }
    }
}

TEST(TestLogger, TestBase64StringToBuffer)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();


    std::string base64 = "";
    u8 buffer[1024];
    CheckedMemset(buffer, 0, sizeof(buffer));

    base64 = "Qg==";
    ASSERT_EQ(Logger::ParseEncodedStringToBuffer(base64.c_str(), buffer, sizeof(buffer)), 1);
    ASSERT_STREQ((char*)buffer, "B");
    CheckedMemset(buffer, 0, sizeof(buffer));

    base64 = "QlI=";
    ASSERT_EQ(Logger::ParseEncodedStringToBuffer(base64.c_str(), buffer, sizeof(buffer)), 2);
    ASSERT_STREQ((char*)buffer, "BR");
    CheckedMemset(buffer, 0, sizeof(buffer));

    base64 = "QlJU";
    ASSERT_EQ(Logger::ParseEncodedStringToBuffer(base64.c_str(), buffer, sizeof(buffer)), 3);
    ASSERT_STREQ((char*)buffer, "BRT");
    CheckedMemset(buffer, 0, sizeof(buffer));

    base64 = "QlJUQw==";
    ASSERT_EQ(Logger::ParseEncodedStringToBuffer(base64.c_str(), buffer, sizeof(buffer)), 4);
    ASSERT_STREQ((char*)buffer, "BRTC");
    CheckedMemset(buffer, 0, sizeof(buffer));

    base64 = "QlJUQ1I=";
    ASSERT_EQ(Logger::ParseEncodedStringToBuffer(base64.c_str(), buffer, sizeof(buffer)), 5);
    ASSERT_STREQ((char*)buffer, "BRTCR");
    CheckedMemset(buffer, 0, sizeof(buffer));

    base64 = "QlJUQ1I=";
    ASSERT_EQ(Logger::ParseEncodedStringToBuffer(base64.c_str(), buffer, 5), 5);
    ASSERT_STREQ((char*)buffer, "BRTCR");
    CheckedMemset(buffer, 0, sizeof(buffer));

    {
        Exceptions::DisableDebugBreakOnException disable;
        base64 = "Malformed";
        ASSERT_THROW(Logger::ParseEncodedStringToBuffer(base64.c_str(), buffer, sizeof(buffer)), IllegalArgumentException);
        CheckedMemset(buffer, 0, sizeof(buffer));

        base64 = "TooBig==";
        ASSERT_THROW(Logger::ParseEncodedStringToBuffer(base64.c_str(), buffer, 0), BufferTooSmallException);
        ASSERT_STREQ((char*)buffer, "");
        CheckedMemset(buffer, 0, sizeof(buffer));

        base64 = "VGhpcyBpcyB0b28gbG9uZyBidXQgcGFydHMgd2lsbCByZW1haW4gLSBvaCBoaSB0aGVyZSB5b3UgY2xldmVyIGd1eSEgIDotKQ==";
        ASSERT_THROW(Logger::ParseEncodedStringToBuffer(base64.c_str(), buffer, 34), BufferTooSmallException);
        ASSERT_STREQ((char*)buffer, "This is too long but parts will re");
    }

    std::string data = "";
    char encodeBuffer[1024];

    data = "B";
    Logger::ConvertBufferToBase64String((const u8*)data.c_str(), data.size(), encodeBuffer, sizeof(encodeBuffer));
    ASSERT_STREQ(encodeBuffer, "Qg==");

    data = "BR";
    Logger::ConvertBufferToBase64String((const u8*)data.c_str(), data.size(), encodeBuffer, sizeof(encodeBuffer));
    ASSERT_STREQ(encodeBuffer, "QlI=");

    data = "BRT";
    Logger::ConvertBufferToBase64String((const u8*)data.c_str(), data.size(), encodeBuffer, sizeof(encodeBuffer));
    ASSERT_STREQ(encodeBuffer, "QlJU");

    data = "BRTC";
    Logger::ConvertBufferToBase64String((const u8*)data.c_str(), data.size(), encodeBuffer, sizeof(encodeBuffer));
    ASSERT_STREQ(encodeBuffer, "QlJUQw==");

    data = "BRTCR";
    Logger::ConvertBufferToBase64String((const u8*)data.c_str(), data.size(), encodeBuffer, sizeof(encodeBuffer));
    ASSERT_STREQ(encodeBuffer, "QlJUQ1I=");

    {
        Exceptions::DisableDebugBreakOnException disable;
        data = "BRTCR";
        ASSERT_THROW(Logger::ConvertBufferToBase64String((const u8*)data.c_str(), data.size(), encodeBuffer, 5), BufferTooSmallException);

        Exceptions::ExceptionDisabler<BufferTooSmallException> btsDisabler;
        data = "BRTCR";
        fillMemoryGuard((u8*)(encodeBuffer + 5), sizeof(encodeBuffer) - 5);
        Logger::ConvertBufferToBase64String((const u8*)data.c_str(), data.size(), encodeBuffer, 5);
        checkMemoryGuard((const u8*)(encodeBuffer + 5), sizeof(encodeBuffer) - 5);
        ASSERT_STREQ(encodeBuffer, "QlJU");
    }



    //Generate random data, encode it as base64 and decode it again.
    //The result must be the original random data. Additionally, 
    //memory guards are place inbetween each buffer to make sure, that
    //the en-/decoding does not corrupt any data.
    struct TestData {
        //Packed inside a struct because the compiler is allowed
        //to rearrange stack variables, but not struct attributes.
        u8 memoryGuard_1[1024];
        u8 dataBuffer[1024];
        u8 memoryGuard_2[1024];
        char base64Buffer[1024 * 2];
        u8 memoryGuard_3[1024];
        u8 endBuffer[sizeof(dataBuffer)];
        u8 memoryGuard_4[1024];
    };
    TestData td;

    fillMemoryGuard(td.memoryGuard_1, sizeof(td.memoryGuard_1));
    fillMemoryGuard(td.memoryGuard_2, sizeof(td.memoryGuard_2));
    fillMemoryGuard(td.memoryGuard_3, sizeof(td.memoryGuard_3));
    fillMemoryGuard(td.memoryGuard_4, sizeof(td.memoryGuard_4));

    MersenneTwister mt(1337);
    for (u32 i = 0; i < 1024 * 2; i++) {
        u32 dataLength = (i + 1) % 1024;
        for (u32 k = 0; k < dataLength; k++) {
            td.dataBuffer[k] = (u8)mt.NextU32(0, 255);
        }
        Logger::ConvertBufferToBase64String(td.dataBuffer, dataLength, td.base64Buffer, sizeof(td.base64Buffer));
        Logger::ParseEncodedStringToBuffer(td.base64Buffer, td.endBuffer, sizeof(td.endBuffer));

        for (u32 k = 0; k < dataLength; k++) {
            if (td.dataBuffer[k] != td.endBuffer[k]) {
                FAIL() << "endBuffer and dataBuffer are not equal at index " << k; //LCOV_EXCL_LINE assertion
            }
        }

        checkMemoryGuard(td.memoryGuard_1, sizeof(td.memoryGuard_1));
        checkMemoryGuard(td.memoryGuard_2, sizeof(td.memoryGuard_2));
        checkMemoryGuard(td.memoryGuard_3, sizeof(td.memoryGuard_3));
        checkMemoryGuard(td.memoryGuard_4, sizeof(td.memoryGuard_4));
    }
}
