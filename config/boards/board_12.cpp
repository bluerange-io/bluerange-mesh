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
#include<GlobalState.h>
#include <Boardconfig.h>
void SetCustomPins_12(CustomPins* pinConfig);
//Ruuvi Tag B5
void SetBoard_12(BoardConfiguration* c)
{
    if(c->boardType == 12)
    {
        c->led1Pin =  17;
        c->led2Pin =  19;
        c->led3Pin =  -1;
        c->ledActiveHigh =  false;
        c->button1Pin =  13;
        c->buttonsActiveHigh =  false;
        c->uartRXPin =  -1;
        c->uartTXPin =  -1;
        c->uartCTSPin =  -1;
        c->uartRTSPin =  -1;
        c->uartBaudRate = (u32)FruityHal::UartBaudrate::BAUDRATE_1M;
        c->dBmRX = -96;
        c->calibratedTX =  -60;
        //According to https://github.com/ruuvi/ruuvitag_fw/blob/master/bsp/ruuvitag_b.h
        c->lfClockSource = (u8)FruityHal::ClockSource::CLOCK_SOURCE_XTAL;
        c->lfClockAccuracy = (u8)FruityHal::ClockAccuracy::CLOCK_ACCURACY_30_PPM;

        // batteryAdcInput -2 is used if we want to measure battery on MCU and that is only possible if Vbatt_max < 3.6V
        c->batteryAdcInputPin = -2;
        c->dcDcEnabled = true;
        GS->boardconf.getCustomPinset = &SetCustomPins_12;
        c->powerOptimizationEnabled = false;
        c->powerButton =  -1;
    }
}

// Specific to board_12.cpp
void SetCustomPins_12(CustomPins* pinConfig){
    if(pinConfig->pinsetIdentifier == PinsetIdentifier::LIS2DH12){
        Lis2dh12Pins* pins = (Lis2dh12Pins*)pinConfig;
        pins->misoPin = 28;
        pins->mosiPin = 25;
        pins->sckPin = 29;
        pins->ssPin = 8;
        pins->sensorEnablePinActiveHigh = true;
        pins->sensorEnablePin = -1;
        pins->sdaPin = -1;
        pins->interrupt1Pin = 2;
        pins->interrupt2Pin = 6;
    }

    else if (pinConfig->pinsetIdentifier == PinsetIdentifier::BMG250){
        Bmg250Pins* pins = (Bmg250Pins*)pinConfig;
        pins->sckPin = -1;
        pins->sdaPin = -1;
        pins->interrupt1Pin = 1;
        pins->sensorEnablePin = -1;
        pins->twiEnablePin = -1;
        pins->twiEnablePinActiveHigh = true;
        pins->sensorEnablePinActiveHigh = true;
    }

    else if (pinConfig->pinsetIdentifier == PinsetIdentifier::TLV493D){
        Tlv493dPins* pins = (Tlv493dPins*)pinConfig;
        pins->sckPin = -1;
        pins->sdaPin = -1;
        pins->sensorEnablePin = -1;
        pins->twiEnablePin = -1;
        pins->twiEnablePinActiveHigh = true;
        pins->sensorEnablePinActiveHigh = true;
    }

    else if (pinConfig->pinsetIdentifier == PinsetIdentifier::BME280){
        Bme280Pins* pins = (Bme280Pins*)pinConfig;
        pins->ssPin = 3;
        pins->misoPin = 28;
        pins->mosiPin = 25;
        pins->sckPin = 29;
        pins->sensorEnablePin = -1;
    }

}
