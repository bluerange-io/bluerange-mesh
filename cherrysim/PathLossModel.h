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
