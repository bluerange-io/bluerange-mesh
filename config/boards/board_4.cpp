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
#include <FruityHal.h>
#include <Boardconfig.h>
#include <GlobalState.h>

void SetCustomPins_4(CustomPins* pinConfig);
//PCA10040 - nRF52 DK
void SetBoard_4(BoardConfiguration* c)
{
    if(c->boardType == 4)
    {
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
        c->uartBaudRate = (u32)FruityHal::UartBaudrate::BAUDRATE_1M;
        c->dBmRX = -96;
        c->calibratedTX =  -60;
        c->lfClockSource = (u8)FruityHal::ClockSource::CLOCK_SOURCE_XTAL;
        c->lfClockAccuracy = (u8)FruityHal::ClockAccuracy::CLOCK_ACCURACY_20_PPM;
        c->dcDcEnabled = true;
        c->powerOptimizationEnabled = false;
        c->powerButton =  -1;
        GS->boardconf.getCustomPinset = &SetCustomPins_4;
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