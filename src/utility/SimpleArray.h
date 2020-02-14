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
#pragma once

#ifdef SIM_ENABLED
#include <type_traits>
#endif

#include <cstring>
#include "types.h"

template<typename T, int N>
class SimpleArray
{
private:
	T data[N];

public:
	static constexpr int length = N;

	//SimpleArray should be a pod type and thus should not have ctor and dtor!
	//SimpleArray() {};
	//~SimpleArray() {};

	T* getRaw()
	{
		return data;
	}

	const T* getRaw() const
	{
		return data;
	}

	T& operator[](int index) {
#ifdef SIM_ENABLED
		if (index >= length || index < 0)
		{
			SIMEXCEPTION(IndexOutOfBoundsException); //LCOV_EXCL_LINE assertion
		}
#endif
		return data[index];
	}

	const T& operator[](int index) const {
#ifdef SIM_ENABLED
		if (index >= length || index < 0)
		{
			SIMEXCEPTION(IndexOutOfBoundsException); //LCOV_EXCL_LINE assertion
		}
#endif
		return data[index];
	}

	inline void zeroData() {
		setAllBytesTo(0);
	}

	inline void setAllBytesTo(u8 value) {
#ifdef SIM_ENABLED
		if (!std::is_pod<T>::value)
		{
			SIMEXCEPTION(ZeroOnNonPodTypeException);
		}
#endif
		memset(data, value, sizeof(T) * length);	//CODE_ANALYZER_IGNORE Must be possible to compile for non pod types (cause it's a template), but wont be executed.
	}
};
