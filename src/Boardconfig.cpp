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

#include <FmTypes.h>
#include <Boardconfig.h>
#include <Config.h>
#include <GlobalState.h>
#include <RecordStorage.h>

//At compile time, we choose a default boardId depending on the chipset
//This is usually the boardId of the development kit for this chipset
//If no boardId is specified in the UICR, this id will be used
#ifdef BOARD_TYPE
// The board type can also be defined through the featureset.h at compile time
#elif defined(NRF52832)
    #define BOARD_TYPE 4
#elif defined(NRF52833)
    #define BOARD_TYPE 39
#elif defined(NRF52840)
    #define BOARD_TYPE 18
#elif defined(SIM_ENABLED)
    #define BOARD_TYPE 19
#elif defined(ARM_TEMPLATE)
    #define BOARD_TYPE 1 // just for now
#else
    #error "No defined BOARD_TYPE"
#endif

extern void SetBoard_4(BoardConfiguration* c);
extern void SetBoard_18(BoardConfiguration* c);
extern void SetBoard_19(BoardConfiguration* c);
extern void SetBoard_39(BoardConfiguration* c);

void* fmBoardConfigPtr;

Boardconf::Boardconf()
{
    configuration = {};
}

//A dummy so that we cannot have nullptr access issues
void SetCustomPins_dummy(CustomPins* pinConfig) {

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
    configuration.boardName = nullptr;
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
    configuration.uartBaudRate = (u32)FruityHal::UartBaudRate::BAUDRATE_1M;
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

    configuration.getCustomPinset = &SetCustomPins_dummy;

    //Now, we load all Default boards (nRf Development kits)
    SetBoard_4(&configuration);
    SetBoard_18(&configuration);
    SetBoard_39(&configuration);

#ifdef SIM_ENABLED
    SetBoard_19(&configuration);
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