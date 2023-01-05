////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2022 M-Way Solutions GmbH
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
        c->uartBaudRate = (u32)FruityHal::UartBaudrate::BAUDRATE_1M;
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
        GS->boardconf.getCustomPinset = &SetCustomPins_35;
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