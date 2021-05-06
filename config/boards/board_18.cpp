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
//PCA10056 - nRF82840 Devkit
void SetBoard_18(BoardConfiguration* c)
{
    if(c->boardType == 18)
    {
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
        c->uartBaudRate = (u32)FruityHal::UartBaudrate::BAUDRATE_1M;
        c->dBmRX = -90;
        c->calibratedTX =  -63;
        c->lfClockSource = (u8)FruityHal::ClockSource::CLOCK_SOURCE_XTAL;
        c->lfClockAccuracy = (u8)FruityHal::ClockAccuracy::CLOCK_ACCURACY_20_PPM;
        c->dcDcEnabled = true;
        c->powerOptimizationEnabled = false;
    }
}
