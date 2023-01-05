////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2022 M-Way Solutions GmbH
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

#include "PathLossModel.h"

#include <algorithm>

#include <cfloat>
#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>

float ComputeRssiFromDistance(const float distance, const PathLossModelParameters &parameters)
{
    return parameters.receivedPowerAtReferenceDistanceDbm - 10.f * parameters.propagationConstant * std::log10(std::clamp(distance, 0.0001f, FLT_MAX));
}

float GenerateRssiNoise(MersenneTwister &rng, const float stddev, const float mean)
{
    static_assert(FLT_RADIX == 2);

    // Generate two uniformly distributed floats between 0 and 1
    const float uniform_a = std::ldexp(static_cast<float>(rng.NextU32()), -32);
    const float uniform_b = std::ldexp(static_cast<float>(rng.NextU32()), -32);

    const auto two_pi = static_cast<float>(2 * M_PI);

    // Generate one standard-normal distributed float using the Box-Muller transform (we could get a second one by replacing cos with sin)
    const float normal_a = std::sqrt(-2.f * std::log(uniform_a)) * std::cos(two_pi * uniform_b);

    // Scale to the requested standard deviation
    return mean + stddev * normal_a;
}
