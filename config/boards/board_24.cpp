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
//Laird BLE654 USB Dongle R1.0 451-00004
void SetBoard_24(BoardConfiguration* c)
{
    if(c->boardType == 24)
    {
        c->led1Pin =  13;
        c->led2Pin =  -1;
        c->led3Pin =  -1;
        c->ledActiveHigh =  true;
        c->button1Pin =  -1;
        c->buttonsActiveHigh =  false;
        c->uartRXPin =  -1;
        c->uartTXPin =  -1;
        c->uartCTSPin =  -1;
        c->uartRTSPin =  -1;
        c->uartBaudRate = (u32)FruityHal::UartBaudrate::BAUDRATE_1M;
        c->dBmRX = -90;
        c->calibratedTX =  -63;
        c->lfClockSource = (u8)FruityHal::ClockSource::CLOCK_SOURCE_SYNTH;
        c->lfClockAccuracy = (u8)FruityHal::ClockAccuracy::CLOCK_ACCURACY_50_PPM;
        c->dcDcEnabled = false;
        c->powerOptimizationEnabled = false;
    }
}
