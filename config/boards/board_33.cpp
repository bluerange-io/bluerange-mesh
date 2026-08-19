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
//Accono acnFIND with ACN52832 and no peripherals (no LED, no buzzer, no accelerometer)
//Same functionality as board id 32, but form factor of the https://aconno.de/products/acnfind-beacon/ WITHOUT periphery
void SetBoard_33(BoardConfiguration* c)
{
    if(c->boardType == 33)
    {
        c->boardName = "acnFIND without periphery";
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
    }
}
