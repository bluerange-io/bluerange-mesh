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
#include<GlobalState.h>
#include <Boardconfig.h>
void SetCustomPins_12(CustomPins* pinConfig);
//Ruuvi Tag B5
void SetBoard_12(BoardConfiguration* c)
{
    if(c->boardType == 12)
    {
        c->boardName = "RuuviTag";
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
        c->uartBaudRate = (u32)FruityHal::UartBaudRate::BAUDRATE_1M;
        c->dBmRX = -96;
        c->calibratedTX =  -60;
        //According to https://github.com/ruuvi/ruuvitag_fw/blob/master/bsp/ruuvitag_b.h
        c->lfClockSource = (u8)FruityHal::ClockSource::CLOCK_SOURCE_XTAL;
        c->lfClockAccuracy = (u8)FruityHal::ClockAccuracy::CLOCK_ACCURACY_30_PPM;

        // batteryAdcInput -2 is used if we want to measure battery on MCU and that is only possible if Vbatt_max < 3.6V
        c->batteryAdcInputPin = -2;
        c->dcDcEnabled = true;
        c->powerOptimizationEnabled = true;
        c->powerButtonActiveHigh = false;
        c->powerButton =  13;
        c->getCustomPinset = &SetCustomPins_12;
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
