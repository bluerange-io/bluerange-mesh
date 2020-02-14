////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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
#include "SimpleArray.h"
#include "types.h"

TEST(TestSimpleArray, TestLength) {
	{
		SimpleArray<u8, 42> arr;
		ASSERT_EQ(arr.length, 42);
		ASSERT_EQ(sizeof(arr), 42 * sizeof(u8));
	}

	{
		SimpleArray<u16, 12> arr;
		ASSERT_EQ(arr.length, 12);
		ASSERT_EQ(sizeof(arr), 12 * sizeof(u16));
	}

	{
		SimpleArray<u32, 1337> arr;
		ASSERT_EQ(arr.length, 1337);
		ASSERT_EQ(sizeof(arr), 1337 * sizeof(u32));
	}

	{
		SimpleArray<BootloaderSettings, 19> arr;
		ASSERT_EQ(arr.length, 19);
		ASSERT_EQ(sizeof(arr), 19 * sizeof(BootloaderSettings));
	}

	{
		SimpleArray<SimpleArray<int, 12>, 100> arr;
		ASSERT_EQ(arr.length, 100);
		ASSERT_EQ(arr[0].length, 12);
		ASSERT_EQ(sizeof(arr), 12 * 100 * sizeof(int));
	}
}

TEST(TestSimpleArray, TestAccess) {
	SimpleArray<int, 128> arr;
	for (int i = 0; i < arr.length; i++)
	{
		arr[i] = i;
	}
	for (int i = 0; i < arr.length; i++)
	{
		ASSERT_EQ(arr[i], i);
	}

	const auto &arr2 = arr;
	for (int i = 0; i < arr2.length; i++)
	{
		ASSERT_EQ(arr2[i], i);
	}
}

TEST(TestSimpleArray, TestRawAccess) {
	SimpleArray<int, 128> arr;
	int* ptr = arr.getRaw();
	for (int i = 0; i < arr.length; i++)
	{
		ptr[i] = i;
	}
	for (int i = 0; i < arr.length; i++)
	{
		ASSERT_EQ(arr[i], i);
	}

	for (int i = 0; i < arr.length; i++)
	{
		arr[i] = i + 1000;
	}
	for (int i = 0; i < arr.length; i++)
	{
		ASSERT_EQ(ptr[i], i + 1000);
	}
}

TEST(TestSimpleArray, TestZero) {
	SimpleArray<int, 128> arr;
	for (int i = 0; i < arr.length; i++)
	{
		arr[i] = i + 1337;
	}
	arr.zeroData();
	for (int i = 0; i < arr.length; i++)
	{
		ASSERT_EQ(arr[i], 0);
	}
}

TEST(TestSimpleArray, TestNonPodZero)
{
	Exceptions::DisableDebugBreakOnException disable;
	struct MyNonPod {
		~MyNonPod() {}
	};
	SimpleArray<MyNonPod, 16> arr;

	bool exceptionCaught = false;
	try {
		arr.zeroData();
	}
	catch (...) {
		exceptionCaught = true;
	}

	ASSERT_TRUE(exceptionCaught);
}

