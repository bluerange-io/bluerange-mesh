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
//Accono ACN52832
void SetBoard_32(BoardConfiguration* c)
{
    if(c->boardType == 32)
    {
        c->boardName = "ACN52832 Beacon";
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
    }
}
