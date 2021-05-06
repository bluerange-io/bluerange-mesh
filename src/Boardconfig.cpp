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

#include <FmTypes.h>
#include <Boardconfig.h>
#include <Config.h>
#include <GlobalState.h>
#include <RecordStorage.h>

extern void SetBoard_4(BoardConfiguration* c);
extern void SetBoard_18(BoardConfiguration* c);
extern void SetBoard_19(BoardConfiguration* c);

void* fmBoardConfigPtr;

Boardconf::Boardconf()
{
    CheckedMemset(&configuration, 0x00, sizeof(configuration));
}

void Boardconf::ResetToDefaultConfiguration()
{
    //Set a default boardType for all different platforms in case we do not have the boardType in UICR
    configuration.boardType = BOARD_TYPE;

    DeviceConfiguration config;
    //If there is data in the DeviceConfiguration, we use the boardType from there
    ErrorType err = FruityHal::GetDeviceConfiguration(config);
    if (err == ErrorType::SUCCESS) {
        if (config.boardType != EMPTY_WORD) configuration.boardType = config.boardType;
    }

    //Set everything else to safe defaults
    configuration.led1Pin = -1;
    configuration.led2Pin = -1;
    configuration.led3Pin = -1;
    configuration.ledActiveHigh = false;
    configuration.button1Pin = -1;
    configuration.buttonsActiveHigh = false;
    configuration.uartRXPin = -1;
    configuration.uartTXPin = -1;
    configuration.uartCTSPin = -1;
    configuration.uartRTSPin = -1;
    configuration.uartBaudRate = (u32)FruityHal::UartBaudrate::BAUDRATE_1M;
    configuration.dBmRX = -90;
    configuration.calibratedTX = -60;
    configuration.lfClockSource = (u8)FruityHal::ClockSource::CLOCK_SOURCE_RC;
    configuration.lfClockAccuracy = (u8)FruityHal::ClockAccuracy::CLOCK_ACCURACY_500_PPM; //Use a safe default if this is not given
    configuration.displayWidth = 400;
    configuration.displayHeight = 300;
    configuration.batteryAdcInputPin = -1;
    configuration.batteryMeasurementEnablePin = -1;
    configuration.voltageDividerR1 = 0;
    configuration.voltageDividerR2 = 0;
    configuration.dcDcEnabled = false;
    configuration.powerOptimizationEnabled = false;

    //Now, we load all Default boards (nRf Development kits)
    SetBoard_4(&configuration);
    SetBoard_18(&configuration);

#ifdef SIM_ENABLED
#ifndef GITHUB_RELEASE
    SetBoard_19(&configuration);
#endif //GITHUB_RELEASE
#endif

    //We call our featureset to check if additional boards are available and if they should be set
    //Each featureset can include a number of boards that it can run on
    SET_BOARD_CONFIGURATION(&configuration);
}

Boardconf & Boardconf::GetInstance()
{
    return GS->boardconf;
}

void Boardconf::Initialize()
{
    ResetToDefaultConfiguration();

    //Can be used from C code to access the config
    fmBoardConfigPtr = (void*)&(configuration.boardType);
}
