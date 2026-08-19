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
#include <FruityHal.h>
#include <Boardconfig.h>
#include <GlobalState.h>

void SetCustomPins_4(CustomPins* pinConfig);
//PCA10040 - nRF52 DK
void SetBoard_4(BoardConfiguration* c)
{
    if(c->boardType == 4)
    {
        c->boardName = "nRF52-DK";
        c->led1Pin =  17;
        c->led2Pin =  18;
        c->led3Pin =  19;
        c->ledActiveHigh =  false;
        c->button1Pin =  13;
        c->buttonsActiveHigh =  false;
        c->uartRXPin =  8;
        c->uartTXPin =  6;
        c->uartCTSPin =  7;
        c->uartRTSPin =  5;
        c->uartBaudRate = (u32)FruityHal::UartBaudRate::BAUDRATE_1M;
        c->dBmRX = -96;
        c->calibratedTX =  -60;
        c->lfClockSource = (u8)FruityHal::ClockSource::CLOCK_SOURCE_XTAL;
        c->lfClockAccuracy = (u8)FruityHal::ClockAccuracy::CLOCK_ACCURACY_20_PPM;
        c->dcDcEnabled = true;
        c->powerOptimizationEnabled = false;
        c->powerButton =  16;
        c->powerButtonActiveHigh = false;
        c->getCustomPinset = &SetCustomPins_4;
    }
}

//This is an example implementation of how custom pins
//can be configured. This function is then passed to the function pointer
//void (*getCustomPinset)(CustomPinset*), defined in BoardConfig.h

void SetCustomPins_4(CustomPins* pinConfig){
    if(pinConfig->pinsetIdentifier == PinsetIdentifier::LIS2DH12){
        Lis2dh12Pins* pins = (Lis2dh12Pins*)pinConfig;
        pins->misoPin = -1;
        pins->mosiPin = -1;
        pins->sckPin = -1;
        pins->ssPin = -1;
        pins->sensorEnablePinActiveHigh = true;
        pins->sensorEnablePin = -1;
        pins->sdaPin = -1;
        pins->interrupt1Pin = -1;
        pins->interrupt2Pin = -1;
    }
}