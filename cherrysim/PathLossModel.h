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

#pragma once

#include "MersenneTwister.h"

//
// The Path-Loss-Model
//
//     P_T - P_R = PL_0 + 10 N ⋅ log10(d / d_0) + X_g
//
// Nomenclature and units:
//
//      P_T     dBm     Transmission power
//      P_R     dBm     RSSI
//      PL_0    dB      Path loss over one reference distance
//      N       -       Path loss exponent or propagation constant
//      d       m       Distance between sender and receiver
//      d_0     m       Reference distance, fixed at 1m
//      X_g     dB      Gaussian noise (models radio noise)
//
// The parameters of the distance estimator in the gateway can be found in:
//
//      fruity-indoor/src/main/java/com/mwaysolutions/iot/fruityindoor/src/core/positioning/aps/core/signal/ApsRssiDistanceEstimator.java
//
// Parameters of X_g:
//
//      Mean        0
//      Std.dev.    9.6
//

/// Parameters of the Path-Loss-Model
struct PathLossModelParameters
{
    /// Received power at the reference distance in dBm. Combines the transmission power (P_T)
    /// and the path loss over one reference distance (PL_0) into one value.
    float receivedPowerAtReferenceDistanceDbm;

    /// Measure of how well radio waves propagate through space (γ or N).
    float propagationConstant;
};

/// Computes the RSSI from the distance using the Path-Loss-Model with the specified parameters.
float ComputeRssiFromDistance(float distance, const PathLossModelParameters &parameters);

/// Generates a suitable RSSI noise sample.
float GenerateRssiNoise(MersenneTwister &rng, float stddev, float mean);
