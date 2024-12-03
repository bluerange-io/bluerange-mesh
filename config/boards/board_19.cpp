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
#include <FruityHal.h>
#include <Boardconfig.h>
#include <IoModule.h>
//Simulator board
#ifdef SIM_ENABLED
#include<GlobalState.h>

void SetCustomModuleSettings_19(ModuleConfiguration* config, void* module);
void SetCustomPins_19(CustomPins* pinConfig);

void SetBoard_19(BoardConfiguration* c)
{
    if (c->boardType == 19)
    {
        c->boardName = "Simulator Board";
        c->led1Pin = 13;
        c->led2Pin = 14;
        c->led3Pin = 15;
        c->ledActiveHigh = true;
        c->button1Pin = 11;
        c->buttonsActiveHigh = false;
        c->uartRXPin = 8;
        c->uartTXPin = 6;
        c->uartCTSPin = 7;
        c->uartRTSPin = 5;
        c->uartBaudRate = (u32)FruityHal::UartBaudRate::BAUDRATE_1M;
        c->dBmRX = -90;
        c->calibratedTX = SIMULATOR_NODE_DEFAULT_CALIBRATED_TX;
        c->lfClockSource = (u8)FruityHal::ClockSource::CLOCK_SOURCE_RC;
        c->lfClockAccuracy = (u8)FruityHal::ClockAccuracy::CLOCK_ACCURACY_250_PPM;
        c->dcDcEnabled = true;
        c->getCustomPinset = &SetCustomPins_19;
        c->powerOptimizationEnabled = false;
        c->powerButton =  -1;
        c->setCustomModuleSettings = &SetCustomModuleSettings_19;
    }
}

void SetCustomModuleSettings_19(ModuleConfiguration* config, void* module)
{
    if (Boardconfig->boardType != 19) return;

    //We configure a number of virtual pins to test the IoModule in the simulator
    if (config->moduleId == ModuleId::IO_MODULE)
    {
        IoModule* mod = (IoModule*)module;
        mod->currentLedMode = LedMode::CUSTOM;

        //Digital Outputs
        mod->AddDigitalOutForBoard(100, true);
        mod->AddDigitalOutForBoard(101, true);

        //Digital Inputs
        mod->AddDigitalInForBoard(102, true, IoModule::DigitalInReadMode::INTERRUPT);
        mod->AddDigitalInForBoard(103, true, IoModule::DigitalInReadMode::INTERRUPT);
        mod->AddDigitalInForBoard(104, true, IoModule::DigitalInReadMode::ON_DEMAND);

        //Toggle Pairs
        mod->AddTogglePairForBoard(0, 1);
    }
}

// Specific to board_19.cpp
void SetCustomPins_19(CustomPins* pinConfig){
    if(pinConfig->pinsetIdentifier == PinsetIdentifier::LIS2DH12){
        Lis2dh12Pins* pins = (Lis2dh12Pins*)pinConfig;
        pins->misoPin = -1;
        pins->mosiPin = -1;
        pins->sckPin = -1;
        pins->ssPin = 20;
        pins->sensorEnablePinActiveHigh = true;
        pins->sensorEnablePin = -1;
        pins->sdaPin = -1;
        pins->interrupt1Pin = 9;
        pins->interrupt2Pin = 10;
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
        pins->ssPin = 2; //Some dummy value so that the AssetModule thinks the pin is available.
        pins->misoPin = -1;
        pins->mosiPin = -1;
        pins->sckPin = -1;
        pins->sensorEnablePin = -1;
    }

    else if (pinConfig->pinsetIdentifier == PinsetIdentifier::GDEWO27W3) {
        Gdewo27w3Pins* pins = (Gdewo27w3Pins*)pinConfig;
        pins->ssPin = 2; //Some dummy value so that the EinkModule thinks the pin is available.
    }


}


#endif
