////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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
//PCA10031 - nRF51 Dongle
void setBoard_0(BoardConfiguration* c)
{
	//Use this boardType for all nRF51 configurations with either boardId 0 or if no DeviceConfiguration data is set
	if(c->boardType == 0)
	{
		c->led1Pin =  21;
		c->led2Pin =  22;
		c->led3Pin =  23;
		c->ledActiveHigh =  false;
		c->button1Pin =  17;
		c->buttonsActiveHigh =  false;
		c->uartRXPin =  11;
		c->uartTXPin =  9;
		c->uartCTSPin =  10;
		c->uartRTSPin =  8;
		c->uartBaudRate = (u32)FruityHal::UartBaudrate::BAUDRATE_1M;
		c->dBmRX = -90;
		c->calibratedTX =  -63;
		c->lfClockSource = (u8)FruityHal::ClockSource::CLOCK_SOURCE_XTAL;
		c->lfClockAccuracy = (u8)FruityHal::ClockAccuracy::CLOCK_ACCURACY_20_PPM;
		c->dcDcEnabled = true;
	}
}
