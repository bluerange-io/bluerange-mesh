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


#include <AdvertisingController.h>
#include <AdvertisingModule.h>

#include <Node.h>
#include <IoModule.h>
#include <Logger.h>
#include <Utility.h>
#include <GlobalState.h>
constexpr u8 ADVERTISING_MODULE_CONFIG_VERSION = 1;

//This module allows a number of advertising messages to be configured.
//These will be broadcasted periodically

AdvertisingModule::AdvertisingModule()
	: Module(ModuleId::ADVERTISING_MODULE, "adv")
{
	//Register callbacks n' stuff

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(AdvertisingModuleConfiguration);

	advJobHandles.zeroData();

	//Set defaults
	ResetToDefaultConfiguration();
}

void AdvertisingModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = true;
	configuration.moduleVersion = ADVERTISING_MODULE_CONFIG_VERSION;

	configuration.advertisingIntervalMs = 100;
	configuration.messageCount = 0;
	configuration.txPower = (i8)0xFF; //Set to invalid value

	SET_FEATURESET_CONFIGURATION(&configuration, this);
}

void AdvertisingModule::ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength)
{
#if IS_INACTIVE(GW_SAVE_SPACE)
	u32 err = 0;

	//Start the Module...
	//Delete previous jobs if they exist
	for(u32 i=0; i<advJobHandles.length; i++){
		if(advJobHandles[i] != nullptr) GS->advertisingController.RemoveJob(advJobHandles[i]);
	}

	//Configure Advertising Jobs for all advertising messages
	for(u32 i=0; i < configuration.messageCount; i++){
		AdvJob job = {
			AdvJobTypes::SCHEDULED,
			3, //Slots
			0, //Delay
			MSEC_TO_UNITS(100, UNIT_0_625_MS), //AdvInterval
			0, //AdvChannel
			0, //CurrentSlots
			0, //CurrentDelay
			FruityHal::BleGapAdvType::ADV_IND, //Advertising Mode
			{0}, //AdvData
			0, //AdvDataLength
			{0}, //ScanData
			0 //ScanDataLength
		};

		CheckedMemcpy(&job.advData, configuration.messageData[i].messageData.getRaw(), configuration.messageData[i].messageLength);
		job.advDataLength = configuration.messageData[i].messageLength;

		advJobHandles[i] = GS->advertisingController.AddJob(job);
	}
#endif
}


#ifdef TERMINAL_ENABLED
TerminalCommandHandlerReturnType AdvertisingModule::TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize)
{
	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif

