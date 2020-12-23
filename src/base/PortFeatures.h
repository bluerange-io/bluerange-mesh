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

/*
 * This file is a defintion of hardware/software capabilities of supported platforms.
 * It does not define whether or not a feature is active, but if it could be activated
 * on the given platform. As such, it is not part of configuration and should not
 * vary among featuresets.
 */

#pragma once

#define FEATURE_AVAILABLE(FEATURE) (FEATURE ## _AVAILABLE)

// Chipset string name
#if defined(NRF52)
    #define CHIPSET_NAME "NRF52"
#elif defined(SIM_ENABLED)
    #define CHIPSET_NAME "SIMULATOR"
#elif defined(ARM_TEMPLATE)
    #define CHIPSET_NAME "ARM"
#else
    #error "No defined chipset"
#endif

// Chipset board ID
#if defined(NRF52832)
    #define BOARD_TYPE 4
#elif defined(NRF52840)
    #define BOARD_TYPE 18
#elif defined(SIM_ENABLED)
    #define BOARD_TYPE 19
#elif defined(ARM_TEMPLATE)
    #define BOARD_TYPE 1 // just for now
#else
    #error "No defined chipset"
#endif

// INS
#if defined(NRF52840)
    #define INS_AVAILABLE 1
#elif defined(NRF52)
    #define INS_AVAILABLE 0
#elif defined(SIM_ENABLED)
    #define INS_AVAILABLE 1
#elif defined(ARM_TEMPLATE)
    #define INS_AVAILABLE 0
#else
    #error "No defined chipset"
#endif

// adc internal measurement
#if defined(NRF52)
    #define ADC_INTERNAL_MEASUREMENT_AVAILABLE 1
#elif defined(SIM_ENABLED)
    #define ADC_INTERNAL_MEASUREMENT_AVAILABLE 0
#elif defined(ARM_TEMPLATE)
    #define ADC_INTERNAL_MEASUREMENT_AVAILABLE 1
#else
    #error "No defined chipset"
#endif

