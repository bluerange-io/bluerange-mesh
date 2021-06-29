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
    //Default, can be overridden by boards
    uint32_t uartBaudRate : 32;

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

    // If set to value different than 0, turns on some battery opimizations
    uint8_t powerOptimizationEnabled;

    int8_t powerButton;
    uint8_t powerButtonActiveHigh : 8;
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
            void (*getCustomPinset)(CustomPins*) = nullptr;

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
