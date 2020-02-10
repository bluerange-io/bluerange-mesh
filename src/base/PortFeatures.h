////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2019 M-Way Solutions GmbH
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

#define FEATURE_AVAILABLE(FEATURE) (FEATURE ## _AVAILABLE)

// Chipset string name
#if defined(NRF52)
	#define CHIPSET_NAME "NRF52"
#elif defined(NRF51)
	#define CHIPSET_NAME "NRF51"
#elif defined(SIM_ENABLED)
	#define CHIPSET_NAME "SIMULATOR"
#else
	#error "No defined chipset"
#endif

// Chipset board ID
#if defined(NRF52832)
	#define BOARD_TYPE 4
#elif defined(NRF52840)
	#define BOARD_TYPE 18
#elif defined(NRF51)
	#define BOARD_TYPE 1
#elif defined(SIM_ENABLED)
	#define BOARD_TYPE 19
#else
	#error "No defined chipset"
#endif

// Preferred connections
#if defined(NRF52)
	#define PREFERRED_CONNECTIONS_AVAILABLE 1
#elif defined(NRF51)
	#define PREFERRED_CONNECTIONS_AVAILABLE 0
#elif defined(SIM_ENABLED)
	#define PREFERRED_CONNECTIONS_AVAILABLE 1
#else
	#error "No defined chipset"
#endif

// Device capabilites
#if defined(NRF52)
	#define DEVICE_CAPABILITIES_AVAILABLE 1
#elif defined(NRF51)
	#define DEVICE_CAPABILITIES_AVAILABLE 0
#elif defined(SIM_ENABLED)
	#define DEVICE_CAPABILITIES_AVAILABLE 1
#else
	#error "No defined chipset"
#endif

// Accelerometer
#if defined(NRF52)
	#define ACCELEROMETER_AVAILABLE 1
#elif defined(NRF51)
	#define ACCELEROMETER_AVAILABLE 0
#elif defined(SIM_ENABLED)
	#define ACCELEROMETER_AVAILABLE 1
#else
	#error "No defined chipset"
#endif

// Gyroscope
#if defined(NRF52)
	#define GYROSCOPE_AVAILABLE 1
#elif defined(NRF51)
	#define GYROSCOPE_AVAILABLE 0
#elif defined(SIM_ENABLED)
	#define GYROSCOPE_AVAILABLE 1
#else
	#error "No defined chipset"
#endif

// Barometer
#if defined(NRF52)
	#define BAROMETER_AVAILABLE 1
#elif defined(NRF51)
	#define BAROMETER_AVAILABLE 0
#elif defined(SIM_ENABLED)
	#define BAROMETER_AVAILABLE 1
#else
	#error "No defined chipset"
#endif

// Magneto
#if defined(NRF52)
	#define MAGNETOMETER_AVAILABLE 1
#elif defined(NRF51)
	#define MAGNETOMETER_AVAILABLE 0
#elif defined(SIM_ENABLED)
	#define MAGNETOMETER_AVAILABLE 1
#else
	#error "No defined chipset"
#endif

// INS
#if defined(NRF52840)
	#if IS_ACTIVE(INS)
		#define INS_AVAILABLE 1
	#else
		#define INS_AVAILABLE 0
	#endif
#elif defined(NRF52)
	#define INS_AVAILABLE 0
#elif defined(NRF51)
	#define INS_AVAILABLE 0
#elif defined(SIM_ENABLED)
	#define INS_AVAILABLE 1
#else
	#error "No defined chipset"
#endif

// adc internal measurement
#if defined(NRF52)
	#define ADC_INTERNAL_MEASUREMENT_AVAILABLE 1
#elif defined(NRF51)
	#define ADC_INTERNAL_MEASUREMENT_AVAILABLE 0
#elif defined(SIM_ENABLED)
	#define ADC_INTERNAL_MEASUREMENT_AVAILABLE 0
#else
	#error "No defined chipset"
#endif

static_assert((FEATURE_AVAILABLE(INS)) ? (FEATURE_AVAILABLE(ACCELEROMETER) && FEATURE_AVAILABLE(GYROSCOPE)) : true, "INS feature requires Accelerometer and Gyroscope to be available.");
