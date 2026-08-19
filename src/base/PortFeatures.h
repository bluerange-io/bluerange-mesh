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

/*
 * This file is a definition of hardware/software capabilities of supported platforms.
 * It does not define whether or not a feature is active, but if it could be activated
 * on the given platform. As such, it is not part of configuration and should not
 * vary among featuresets.
 */

#pragma once

#define FEATURE_AVAILABLE(FEATURE) (FEATURE ## _AVAILABLE)

// Chipset string name (Only used for logging)
#if defined(NRF52832) || defined(NRF52840) || defined(NRF52833)
    #define CHIPSET_NAME "NRF52"
#elif defined(SIM_ENABLED)
    #define CHIPSET_NAME "SIMULATOR"
#elif defined(ARM_TEMPLATE)
    #define CHIPSET_NAME "ARM"
#else
    #error "No defined chipset"
#endif

// INS
#if defined(NRF52840)
    #define INS_AVAILABLE 1
#elif defined(NRF52832) || defined(NRF52833)
    #define INS_AVAILABLE 0
#elif defined(SIM_ENABLED)
    #define INS_AVAILABLE 1
#elif defined(ARM_TEMPLATE)
    #define INS_AVAILABLE 0
#else
    #error "No defined chipset"
#endif

// adc internal measurement
#if defined(NRF52832) || defined(NRF52840) || defined(NRF52833)
    #define ADC_INTERNAL_MEASUREMENT_AVAILABLE 1
#elif defined(SIM_ENABLED)
    #define ADC_INTERNAL_MEASUREMENT_AVAILABLE 0
#elif defined(ARM_TEMPLATE)
    #define ADC_INTERNAL_MEASUREMENT_AVAILABLE 1
#else
    #error "No defined chipset"
#endif

