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
 * Board configurations are used to have a firmware that can run with the same
 * functionality on multiple boards. The decision which board configuration is
 * used is done at runtime.
 */

#ifndef BOARDCONFIG_H
#define BOARDCONFIG_H

#include <stdint.h>
/*## BoardConfiguration #############################################################*/
// The BoardConfiguration must contain the correct settings for the board that the firmware
// is flashed on. The featureset must contain all board configurations that the featureset
// wants to run on.

#pragma pack(push)
#pragma pack(1)
typedef struct BoardConfiguration
{
    //Board Type (aka. boardId) identifies a PCB with its wiring and configuration
    //Multiple boards can be added and the correct one is chosen at runtime depending on the UICR boardId
    //Custom boardTypes should start from 10000
    uint16_t boardType;

    const char*  boardName;

    //Default board is pca10031, modify SET_BOARD if different board is required
    //Or flash config data to UICR
    int8_t led1Pin;
    int8_t led2Pin;
    int8_t led3Pin;
    //Defines if writing 0 or 1 to an LED turns it on
    uint8_t ledActiveHigh : 8;

    int8_t button1Pin;
    uint8_t buttonsActiveHigh : 8;

    //UART configuration. Set RX-Pin to -1 to disable UART
    int8_t uartRXPin;
    int8_t uartTXPin;
    int8_t uartCTSPin;
    int8_t uartRTSPin;
    uint32_t uartBaudRate;

    //Display Dimensions
    uint16_t displayWidth;
    uint16_t displayHeight;

    //Receiver sensitivity of this device, set from board configs
    int8_t dBmRX;
    // This value should be calibrated at 1m distance, set by board configs
    int8_t calibratedTX;

    uint8_t lfClockSource;
    uint8_t lfClockAccuracy;

    int8_t batteryAdcInputPin;
    int8_t batteryMeasurementEnablePin;

    uint32_t voltageDividerR1;
    uint32_t voltageDividerR2;
    uint8_t dcDcEnabled;

    // If set to value different than 0, turns on some battery optimizations
    uint8_t powerOptimizationEnabled;

    int8_t powerButton;
    uint8_t powerButtonActiveHigh;

    /**
     * @brief Enable UART when device is not enrolled
     *
     * If set to true, the UART will be enabled when the device is not enrolled.
     * This can be used to e.g. allow debugging or other stuff during production
     * of the device without the need for a separate debug firmware.
     *
     * The UART will be disabled as soon as the device is enrolled to prevent
     * unauthorized access to the UART after deployment.
     *
     * @important Needs Debug Module to be active.
     */
    bool enableUartIfNotEnrolled;

//This is also included from C code where we do not have access to some of the types that are accessible from C++
//so we use void pointers instead
#ifdef __cplusplus
    void (*getCustomPinset)(CustomPins*) = nullptr;
    void (*setCustomModuleSettings)(ModuleConfiguration* config, void* module) = nullptr;
#else
    void (*getCustomPinset)(void*);
    void (*setCustomModuleSettings)(void* config, void* module);
#endif
} BoardConfiguration;
#pragma pack(pop)

#ifdef __cplusplus
    #ifndef Boardconfig
    #define Boardconfig (&(Boardconf::GetInstance().configuration))
    #endif
#endif //__cplusplus


#ifdef __cplusplus
    class Boardconf
    {
        public:
            Boardconf();
            static Boardconf& GetInstance();

            void Initialize();
            void ResetToDefaultConfiguration();

            BoardConfiguration configuration;

    };
#endif //__cplusplus

//Can be used to make the boardconfig available to C
#ifdef __cplusplus
extern void* fmBoardConfigPtr;
#else
extern struct BoardConfiguration* fmBoardConfigPtr;
#endif

#endif //BOARDCONFIG_H
