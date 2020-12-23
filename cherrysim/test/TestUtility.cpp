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
#include <set>

TEST(TestUtility, TestGetIndexForSerial) {
    //The original serial number range had 5 characters
    ASSERT_EQ(Utility::GetIndexForSerial("BBBBB"), 0);
    ASSERT_EQ(Utility::GetIndexForSerial("BBBBC"), 1);
    ASSERT_EQ(Utility::GetIndexForSerial("BBBB9"), 29);
    ASSERT_EQ(Utility::GetIndexForSerial("CDWXY"), 879859);
    ASSERT_EQ(Utility::GetIndexForSerial("QRSTV"), 10084066);
    ASSERT_EQ(Utility::GetIndexForSerial("WPMNB"), 14075400);
    ASSERT_EQ(Utility::GetIndexForSerial("12345"), 17625445);
    ASSERT_EQ(Utility::GetIndexForSerial("99999"), 24299999);

    //Old Asset serial numbers (A should be ignored and handled as a B)
    ASSERT_EQ(Utility::GetIndexForSerial("ABBBB"), 0);
    ASSERT_EQ(Utility::GetIndexForSerial("ABBBC"), 1);

    //The open source range for testing
    ASSERT_EQ(Utility::GetIndexForSerial("FMBBB"), 2673000);
    ASSERT_EQ(Utility::GetIndexForSerial("FM999"), 2699999);

    //The extended range has 7 characters, 5 character serial numbers can always be prefixed with BB if desired
    ASSERT_EQ(Utility::GetIndexForSerial("BB99999"), 24299999);
    ASSERT_EQ(Utility::GetIndexForSerial("BCBBBBB"), 24300000);
    ASSERT_EQ(Utility::GetIndexForSerial("BCBBBBC"), 24300001);
    ASSERT_EQ(Utility::GetIndexForSerial("H62Q56T"), UINT32_MAX);
}

TEST(TestUtility, TestGenerateBeaconSerialForIndex) {
    char buffer[128];
    Utility::GenerateBeaconSerialForIndex(0, buffer); ASSERT_STREQ(buffer, "BBBBB");
    Utility::GenerateBeaconSerialForIndex(1, buffer); ASSERT_STREQ(buffer, "BBBBC");
    Utility::GenerateBeaconSerialForIndex(29, buffer); ASSERT_STREQ(buffer, "BBBB9");
    Utility::GenerateBeaconSerialForIndex(879859, buffer); ASSERT_STREQ(buffer, "CDWXY");
    Utility::GenerateBeaconSerialForIndex(10084066, buffer); ASSERT_STREQ(buffer, "QRSTV");
    Utility::GenerateBeaconSerialForIndex(14075400, buffer); ASSERT_STREQ(buffer, "WPMNB");
    Utility::GenerateBeaconSerialForIndex(17625445, buffer); ASSERT_STREQ(buffer, "12345");
    Utility::GenerateBeaconSerialForIndex(24299999, buffer); ASSERT_STREQ(buffer, "99999");

    //The open source range used for testing
    Utility::GenerateBeaconSerialForIndex(2673000, buffer); ASSERT_STREQ(buffer, "FMBBB");
    Utility::GenerateBeaconSerialForIndex(2699999, buffer); ASSERT_STREQ(buffer, "FM999");

    //The serial numbers of the extended range (7 chars instead of 5)
    Utility::GenerateBeaconSerialForIndex(24300000, buffer); ASSERT_STREQ(buffer, "BCBBBBB"); //first
    Utility::GenerateBeaconSerialForIndex(24300001, buffer); ASSERT_STREQ(buffer, "BCBBBBC"); //second
    Utility::GenerateBeaconSerialForIndex(0x7FFFFFFFUL, buffer); ASSERT_STREQ(buffer, "D8PJQ8K"); //Last serial of the Mway range
    Utility::GenerateBeaconSerialForIndex(UINT32_MAX, buffer); ASSERT_STREQ(buffer, "H62Q56T"); //last

    //The following tests the Vendor range
    VendorSerial serial = {};
    serial.parts.vendorFlag = 1;
    ASSERT_EQ(serial.serialIndex, 0x80000000UL); //Make sure that the bitfields are properly ordered

    //Tests the serial numbers for vendor 0x0000
    serial.parts.vendorId = 0x0000;

    serial.parts.vendorSerialId = 0; //First serial number
    Utility::GenerateBeaconSerialForIndex(serial.serialIndex, buffer); ASSERT_STREQ(buffer, "D8PJQ8L");
    serial.parts.vendorSerialId = 1; //Second serial number
    Utility::GenerateBeaconSerialForIndex(serial.serialIndex, buffer); ASSERT_STREQ(buffer, "D8PJQ8M");
    serial.parts.vendorSerialId = 0x7FFF; //last serial number
    Utility::GenerateBeaconSerialForIndex(serial.serialIndex, buffer); ASSERT_STREQ(buffer, "D8PKYNT");

    //Tests the serial numbers for vendor 0x024D (Our M-Way range)
    serial.parts.vendorId = 0x024D;

    serial.parts.vendorSerialId = 0; //First serial number
    Utility::GenerateBeaconSerialForIndex(serial.serialIndex, buffer); ASSERT_STREQ(buffer, "D9HCK3N");
    serial.parts.vendorSerialId = 1; //Second serial number
    Utility::GenerateBeaconSerialForIndex(serial.serialIndex, buffer); ASSERT_STREQ(buffer, "D9HCK3P");
    serial.parts.vendorSerialId = 0x7FFF; //last serial number
    Utility::GenerateBeaconSerialForIndex(serial.serialIndex, buffer); ASSERT_STREQ(buffer, "D9HDSHW");

    //Make sure the bitfield is properly ordered
    serial.parts.vendorId = 0x0002;
    serial.parts.vendorSerialId = 3;
    ASSERT_EQ(serial.serialIndex, (1 << 31) | (0x2 << 15) | (3));
}

TEST(TestUtility, TestIsPowerOfTwo) {
    for (u32 i = 0; i < 32; i++) {
        ASSERT_TRUE(Utility::IsPowerOfTwo(1ul << i));
    }
    ASSERT_FALSE(Utility::IsPowerOfTwo(3));
    ASSERT_FALSE(Utility::IsPowerOfTwo(5));
    ASSERT_FALSE(Utility::IsPowerOfTwo(6));
    ASSERT_FALSE(Utility::IsPowerOfTwo(7));
    ASSERT_FALSE(Utility::IsPowerOfTwo(9));
    ASSERT_FALSE(Utility::IsPowerOfTwo(10));
    ASSERT_FALSE(Utility::IsPowerOfTwo(11));
    for (u32 i = 3; i < 31; i++) {
        ASSERT_FALSE(Utility::IsPowerOfTwo((1ul << i) - 1));
        ASSERT_FALSE(Utility::IsPowerOfTwo((1ul << i) + 1));
    }
}

TEST(TestUtility, TestGetVersionStringFromInt) {
    char buffer[128];
    Utility::GetVersionStringFromInt(0         , buffer); ASSERT_STREQ(buffer, "0.0.0");
    Utility::GetVersionStringFromInt(1         , buffer); ASSERT_STREQ(buffer, "0.0.1");
    Utility::GetVersionStringFromInt(10        , buffer); ASSERT_STREQ(buffer, "0.0.10");
    Utility::GetVersionStringFromInt(283       , buffer); ASSERT_STREQ(buffer, "0.0.283");
    Utility::GetVersionStringFromInt(3029      , buffer); ASSERT_STREQ(buffer, "0.0.3029");
    Utility::GetVersionStringFromInt(12092     , buffer); ASSERT_STREQ(buffer, "0.1.2092");
    Utility::GetVersionStringFromInt(392348    , buffer); ASSERT_STREQ(buffer, "0.39.2348");
    Utility::GetVersionStringFromInt(9938472   , buffer); ASSERT_STREQ(buffer, "0.993.8472");
    Utility::GetVersionStringFromInt(49284902  , buffer); ASSERT_STREQ(buffer, "4.928.4902");
    Utility::GetVersionStringFromInt(389283203 , buffer); ASSERT_STREQ(buffer, "38.928.3203");
    Utility::GetVersionStringFromInt(4294967295, buffer); ASSERT_STREQ(buffer, "429.496.7295"); //Highest possible version
}

TEST(TestUtility, TestGetRandomInteger) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();
    NodeIndexSetter setter(0);

    std::set<u32> randoms;
    int amountOfClashes = 0;
    for (int i = 0; i < 10000; i++)
    {
        u32 rand = Utility::GetRandomInteger();
        if (randoms.count(rand) != 0)
        {
            amountOfClashes++;
        }
        else
        {
            randoms.insert(rand);
        }
    }
    //Yes I know its random, but think about it for a second. We generate 10000 random 32 bit integers (4.294.967.296 different values). 
    //Having more than 10 clashes would be surprising, having more than 100 definetly means there is something wrong with the random function.
    ASSERT_TRUE(amountOfClashes < 100);
}

TEST(TestUtility, TestCRC) {
    char data[] = "This is my data and I shall check if its valid or not. Deep-sea fishes are scary!";
    const u32 len = strlen(data);
    ASSERT_EQ(Utility::CalculateCrc8 ((u8*)data, len), 37);
    ASSERT_EQ(Utility::CalculateCrc16((u8*)data, len, nullptr), 58061);
    constexpr u32 expectedCrc32 = 2744420781;
    ASSERT_EQ(Utility::CalculateCrc32((u8*)data, len), expectedCrc32);

    //Calculate the crc32 of only half the data...
    u32 partialCrc32 = Utility::CalculateCrc32((u8*)data, len / 2);
    ASSERT_EQ(partialCrc32, 3310278043);
    //...and then use the partial Crc32 to calculate the full expectedCrc32 with the second half of the data.
    ASSERT_EQ(Utility::CalculateCrc32((u8*)data + len / 2, len / 2 + 1, partialCrc32), expectedCrc32);

    data[40] = 'A'; //Just change something.
    ASSERT_EQ(Utility::CalculateCrc8 ((u8*)data, len), 70);
    ASSERT_EQ(Utility::CalculateCrc16((u8*)data, len, nullptr), 55639);
    ASSERT_EQ(Utility::CalculateCrc32((u8*)data, len), 1322553117);
}

TEST(TestUtility, TestFindLast) {
    char data[] = "This string has many sheeps! The reason for this is that sheeps are cool. sheeps? sheeps! And apples.";
    ASSERT_STREQ(Utility::FindLast(data, "sheep"), "sheeps! And apples.");
    ASSERT_EQ(Utility::FindLast(data, "wolf"), nullptr);
}

TEST(TestUtility, TestAes128BlockEncrypt) {
    Aes128Block msgBlock = {
        0x00, 0x10, 0x20, 0x30,
        0x0A, 0x1A, 0x2A, 0x3A,
        0x0B, 0x1B, 0x2B, 0x3B,
        0x0C, 0x1C, 0x2C, 0x3C,
    };

    Aes128Block key = {
        0xA0, 0xB0, 0xC0, 0xD0,
        0xAA, 0xBA, 0xCA, 0xDA,
        0xAB, 0xBB, 0xCB, 0xDB,
        0xAC, 0xBC, 0xCC, 0xDC,
    };

    Aes128Block encrypted;

    Utility::Aes128BlockEncrypt(&msgBlock, &key, &encrypted);

    ASSERT_EQ(encrypted.data[0 ], 0x2B);
    ASSERT_EQ(encrypted.data[1 ], 0x62);
    ASSERT_EQ(encrypted.data[2 ], 0x7A);
    ASSERT_EQ(encrypted.data[3 ], 0x78);
    ASSERT_EQ(encrypted.data[4 ], 0x3C);
    ASSERT_EQ(encrypted.data[5 ], 0x0E);
    ASSERT_EQ(encrypted.data[6 ], 0xB9);
    ASSERT_EQ(encrypted.data[7 ], 0xBC);
    ASSERT_EQ(encrypted.data[8 ], 0x57);
    ASSERT_EQ(encrypted.data[9 ], 0x8D);
    ASSERT_EQ(encrypted.data[10], 0x7F);
    ASSERT_EQ(encrypted.data[11], 0x69);
    ASSERT_EQ(encrypted.data[12], 0x36);
    ASSERT_EQ(encrypted.data[13], 0xD4);
    ASSERT_EQ(encrypted.data[14], 0x23);
    ASSERT_EQ(encrypted.data[15], 0xC6);
}

TEST(TestUtility, TestXorWords) {
    u32 src1[]    = {  100,  1000, 100000, 324543, 23491291, 20, 1 };
    u32 src2[]    = { 2919, 13282,     10,  10492,    12245, 20, 2 };
    u32 correct[] = { 2819, 12298, 100010, 318275, 23485710,  0, 3 };
    u32 out[sizeof(src1) / sizeof(*src1)];

    Utility::XorWords(src1, src2, sizeof(src1) / sizeof(*src1), out);

    for (size_t i = 0; i < sizeof(src1) / sizeof(*src1); i++)
    {
        ASSERT_EQ(correct[i], out[i]);
    }
}


TEST(TestUtility, TestXorBytes) {
    u8 src1[]    = { 100, 200, 20,   0,  1,  4, 1, 100 };
    u8 src2[]    = {  50,  27, 10, 100, 25, 13, 2, 100 };
    u8 correct[] = {  86, 211, 30, 100, 24,  9, 3,   0 };
    u8 out[sizeof(src1) / sizeof(*src1)];

    Utility::XorBytes(src1, src2, sizeof(src1) / sizeof(*src1), out);

    for (size_t i = 0; i < sizeof(src1) / sizeof(*src1); i++)
    {
        ASSERT_EQ(correct[i], out[i]);
    }
}

TEST(TestUtility, TestSwapBytes)
{
    {
        //With unswapped middle
        u8 toSwap[] = { 100, 10, 20, 50, 20 };
        u8 swapped[] = { 20, 50, 20, 10, 100 };

        Utility::SwapBytes(toSwap, 5);

        for (size_t i = 0; i < sizeof(toSwap) / sizeof(*toSwap); i++)
        {
            ASSERT_EQ(toSwap[i], swapped[i]);
        }
    }

    {
        //With swapped middle
        u8 toSwap[] = { 100, 10, 20,  50 };
        u8 swapped[] = { 50, 20, 10, 100 };

        Utility::SwapBytes(toSwap, 4);

        for (int i = 0; i < 4; i++)
        {
            ASSERT_EQ(toSwap[i], swapped[i]);
        }
    }
}


TEST(TestUtility, TestCompareMem)
{
    for (int i = 0; i <= 255; i++)
    {
        u8 *data = new u8[i + 1];
        CheckedMemset(data, i, i + 1);
        ASSERT_TRUE (Utility::CompareMem((u8)i,       data, i + 1));
        ASSERT_FALSE(Utility::CompareMem((u8)(i + 1), data, i + 1));
        delete[] data;
    }
}

TEST(TestUtility, TestToUpperCase)
{
    char data [1024];

    strcpy(data, "apple");  Utility::ToUpperCase(data); ASSERT_STREQ(data, "APPLE");
    strcpy(data, "APPLE");  Utility::ToUpperCase(data); ASSERT_STREQ(data, "APPLE");
    strcpy(data, "BaNaNa"); Utility::ToUpperCase(data); ASSERT_STREQ(data, "BANANA");
    strcpy(data, "9 kiwi"); Utility::ToUpperCase(data); ASSERT_STREQ(data, "9 KIWI");
}

TEST(TestUtility, TestConfigurableBackOff)
{
    bool result = false;

    u32 backOffIvsDs[] = { SEC_TO_DS(30), SEC_TO_DS(1 * 60), SEC_TO_DS(2 * 60), SEC_TO_DS(30 * 60) };

    //appTimer changed from 200 to 499, Interval counting was stated at 200
    //Should NOT TRIGGER as we did not approach the first interval yet
    result = Utility::ShouldBackOffIvTrigger(499, 300, 200, backOffIvsDs, sizeof(backOffIvsDs));
    ASSERT_EQ(result, 0);

    //appTimer changed from 200 to 500, Interval counting was stated at 200
    //Should TRIGGER as we exactly hit the first interval of 300
    result = Utility::ShouldBackOffIvTrigger(500, 300, 200, backOffIvsDs, sizeof(backOffIvsDs));
    ASSERT_EQ(result, 1);

    //appTimer changed from 201 to 501, Interval counting was stated at 200
    //Should TRIGGER as we exceeded the first interval of 300
    result = Utility::ShouldBackOffIvTrigger(501, 300, 200, backOffIvsDs, sizeof(backOffIvsDs));
    ASSERT_EQ(result, 1);

    //appTimer changed from 600 to 700, Interval counting was stated at 200
    //Should NOT TRIGGER as the first interval should only trigger once we are at 200 startTime + 300 at which the interval should trigger
    result = Utility::ShouldBackOffIvTrigger(700, 100, 200, backOffIvsDs, sizeof(backOffIvsDs));
    ASSERT_EQ(result, 0);

    //appTimer changed from 600 to 700
    //Should NOT TRIGGER as the second interval at 600 was already hit the last time when the timer was at 600
    result = Utility::ShouldBackOffIvTrigger(700, 100, 0, backOffIvsDs, sizeof(backOffIvsDs));
    ASSERT_EQ(result, 0);

    //appTimer changed from 500 to 700
    //Should TRIGGER as the second interval at 600 is hit inbetween
    result = Utility::ShouldBackOffIvTrigger(700, 200, 0, backOffIvsDs, sizeof(backOffIvsDs));
    ASSERT_EQ(result, 1);

    u32 passedTimeDs = 2;
    u32 startTimeDs = Utility::INVALID_BACKOFF_START_TIME;

    u32 TEST_START_TIME = 500;

    //We simulate an appTimer that always increases by 2 deciseconds
    //It should trigger at some points, but only once at these points
    for (u32 appTimerDs = 0; appTimerDs < SEC_TO_DS(30 * 60) + 1000; appTimerDs += passedTimeDs)
    {

        if (appTimerDs == TEST_START_TIME) startTimeDs = appTimerDs;

        result = Utility::ShouldBackOffIvTrigger(appTimerDs, passedTimeDs, startTimeDs, backOffIvsDs, sizeof(backOffIvsDs));

        if (appTimerDs == TEST_START_TIME + SEC_TO_DS(30)) ASSERT_EQ(result, 1);
        else if (appTimerDs == TEST_START_TIME + SEC_TO_DS(1 * 60)) ASSERT_EQ(result, 1);
        else if (appTimerDs == TEST_START_TIME + SEC_TO_DS(2 * 60)) ASSERT_EQ(result, 1);
        else if (appTimerDs == TEST_START_TIME + SEC_TO_DS(30 * 60)) ASSERT_EQ(result, 1);
        else ASSERT_EQ(result, 0);

        //printf("AppTimerDs %u, PassedTimeDs %u, Trigger %u" EOL, appTimerDs, passedTimeDs, result);
    }
}
