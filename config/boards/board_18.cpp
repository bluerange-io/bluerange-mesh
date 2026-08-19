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
#include <IoModule.h>
#include <GlobalState.h>

void SetCustomModuleSettings_18(ModuleConfiguration* config, void* module);

//PCA10056 - nRF82840 Devkit
void SetBoard_18(BoardConfiguration* c)
{
    if(c->boardType == 18)
    {
        c->boardName = "nRF52840-DK";
        c->led1Pin =  13;
        c->led2Pin =  14;
        c->led3Pin =  15;
        c->ledActiveHigh =  false;
        c->button1Pin =  11;
        c->buttonsActiveHigh =  false;
        c->uartRXPin =  8;
        c->uartTXPin =  6;
        c->uartCTSPin =  7;
        c->uartRTSPin =  5;
        c->uartBaudRate = (u32)FruityHal::UartBaudRate::BAUDRATE_1M;
        c->dBmRX = -90;
        c->calibratedTX =  -63;
        c->lfClockSource = (u8)FruityHal::ClockSource::CLOCK_SOURCE_XTAL;
        c->lfClockAccuracy = (u8)FruityHal::ClockAccuracy::CLOCK_ACCURACY_20_PPM;
        c->dcDcEnabled = true;
        c->powerOptimizationEnabled = false;
        c->powerButton =  -1;
        c->setCustomModuleSettings = &SetCustomModuleSettings_18;
    }
}

void SetCustomModuleSettings_18(ModuleConfiguration* config, void* module)
{
    //We configure a number of digital outputs and inputs that were not
    //configured as LEDs or buttons above
    if (config->moduleId == ModuleId::IO_MODULE)
    {
        IoModule* mod = (IoModule*)module;
        mod->currentLedMode = LedMode::CUSTOM;

        //Digital Outputs
        mod->AddDigitalOutForBoard(FruityHal::ConvertPortToGpio(0, 16), false); //LED4

        //Digital Inputs
        mod->AddDigitalInForBoard(FruityHal::ConvertPortToGpio(0, 24), false, IoModule::DigitalInReadMode::INTERRUPT); //Button3
        mod->AddDigitalInForBoard(FruityHal::ConvertPortToGpio(0, 25), false, IoModule::DigitalInReadMode::INTERRUPT); //Button4

        //Toggle Pairs
        mod->AddTogglePairForBoard(0, 1);
    }
}