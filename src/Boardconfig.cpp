////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2019 M-Way Solutions GmbH
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

#include <Boardconfig.h>
#include <Config.h>
#include <GlobalState.h>
#include <RecordStorage.h>

extern void setBoard_0(BoardConfiguration* c);
extern void setBoard_1(BoardConfiguration* c);
extern void setBoard_4(BoardConfiguration* c);
extern void setBoard_18(BoardConfiguration* c);
extern void setBoard_19(BoardConfiguration* c);

void* fmBoardConfigPtr;

Boardconf::Boardconf()
{

}

void Boardconf::ResetToDefaultConfiguration()
{
	configuration.moduleId = moduleID::BOARD_CONFIG_ID;
	configuration.moduleVersion = 1;
	configuration.moduleActive = true;

	//Set a default boardType for all different platforms in case we do not have the boardType in UICR
#if defined(NRF51)
	//NRF51-DK is default for platform NRF51
	configuration.boardType = 1;
#elif defined(NRF52832)
	//NRF52-DK is default for platform NRF52
	configuration.boardType = 4;
#elif defined(NRF52840)
	//NRF82840-DK is default for NRF52840
	configuration.boardType = 18;
#elif defined(SIM_ENABLED)
configuration.boardType = 19;
#endif

	//If there is data in the UICR, we use the boardType from there
	u32* uicrData = getUicrDataPtr();
	if (uicrData != nullptr) {
		if (uicrData[1] != EMPTY_WORD) configuration.boardType = uicrData[1];
	}

	//Set everything else to safe defaults
	configuration.led1Pin = -1;
	configuration.led2Pin = -1;
	configuration.led3Pin = -1;
	configuration.ledActiveHigh = false;
	configuration.button1Pin = -1;
	configuration.buttonsActiveHigh = false;
	configuration.uartRXPin = -1;
	configuration.uartTXPin = -1;
	configuration.uartCTSPin = -1;
	configuration.uartRTSPin = -1;
	configuration.uartBaudRate = UART_BAUDRATE_BAUDRATE_Baud1M;
	configuration.dBmRX = -90;
	configuration.calibratedTX = -60;
	configuration.lfClockSource = NRF_CLOCK_LF_SRC_RC;
	configuration.batteryAdcAin = -1;
	configuration.batteryCheckDIO = -1;
	configuration.spiM0SckPin = -1;
	configuration.spiM0MosiPin = -1;
	configuration.spiM0MisoPin = -1;
	configuration.spiM0SSAccPin = -1;
	configuration.spiM0SSBmePin = -1;
	configuration.lis2dh12Interrupt1Pin = -1;
	configuration.dcDcEnabled = false;

	//Now, we load all Default boards (nRf Development kits)
	setBoard_0(&configuration);
	setBoard_1(&configuration);
	setBoard_4(&configuration);
	setBoard_18(&configuration);

#ifdef SIM_ENABLED
	setBoard_19(&configuration);
#endif

	//We call our featureset to check if additional boards are available and if they should be set
	//Each featureset can include a number of boards that it can run on
	SET_FEATURESET_CONFIGURATION(&configuration);
}

void Boardconf::Initialize(bool loadConfigFromFlash)
{
	ResetToDefaultConfiguration();

	//Can be used from C code to access the config
	fmBoardConfigPtr = (void*)&(configuration.boardType);
}
