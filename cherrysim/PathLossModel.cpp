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
