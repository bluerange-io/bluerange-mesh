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
#include<FruityHal.h>
#include <Boardconfig.h>
#include <GlobalState.h>
//Accono acnFIND with ACN52832 and accelerometer (no LED, no buzzer)
//Datasheet can be found https://aconno.de/products/acnfind-beacon/ WITHOUT LED and Buzzer
extern void SetCustomPins_35(CustomPins* pinConfig);
void SetBoard_35(BoardConfiguration* c)
{
    if(c->boardType == 35)
    {
        c->boardName = "acnFIND V1.0 with accelerometer";
        c->led1Pin = -1;
        c->led2Pin = -1;
        c->led3Pin = -1;
        c->ledActiveHigh = true;
        c->button1Pin = -1;
        c->buttonsActiveHigh = false;
        c->uartRXPin = -1;
        c->uartTXPin = -1;
        c->uartCTSPin = -1;
        c->uartRTSPin = -1;
        c->uartBaudRate = (u32)FruityHal::UartBaudRate::BAUDRATE_1M;
        c->dBmRX = -96;
        c->calibratedTX = -55;
        //Accuracy from Datasheet: https://aconno.de/download/acn52832-data-sheet-v1-2/
        c->lfClockSource = (u8)FruityHal::ClockSource::CLOCK_SOURCE_XTAL;
        c->lfClockAccuracy = (u8)FruityHal::ClockAccuracy::CLOCK_ACCURACY_100_PPM;
        c->dcDcEnabled = true;
        // Use chip input voltage measurement
        c->batteryAdcInputPin = -2;
        c->powerOptimizationEnabled = false;
        c->powerButton = -1;
        c->powerButtonActiveHigh = false;
        c->getCustomPinset = &SetCustomPins_35;
    }
}

void SetCustomPins_35(CustomPins* pinConfig){
    if(pinConfig->pinsetIdentifier == PinsetIdentifier::LIS2DH12){
        Lis2dh12Pins* pins = (Lis2dh12Pins*)pinConfig;
        pins->misoPin = -1;
        pins->mosiPin = -1;
        pins->sckPin = 17;
        pins->ssPin = -1;
        pins->sensorEnablePinActiveHigh = true;
        pins->sensorEnablePin = 11;
        pins->sdaPin = 20;
        pins->interrupt1Pin = 16;
        pins->interrupt2Pin = 15;
    }

    else if (pinConfig->pinsetIdentifier == PinsetIdentifier::BMG250){
        Bmg250Pins* pins = (Bmg250Pins*)pinConfig;
        pins->sckPin = -1;
        pins->sdaPin = -1;
        pins->interrupt1Pin = -1;
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
        pins->ssPin = -1;
        pins->misoPin = -1;
        pins->mosiPin = -1;
        pins->sckPin = -1;
        pins->sensorEnablePin = -1;
    }

}