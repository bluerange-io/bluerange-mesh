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

#define TEMPLATE_MODULE_CONFIG_VERSION 1

#include <TemplateModule.h>
#include <Logger.h>
#include <Utility.h>
#include <Node.h>

extern "C"{

}

TemplateModule::TemplateModule()
	: Module(moduleID::TEMPLATE_MODULE_ID, "template")
{
	moduleVersion = TEMPLATE_MODULE_CONFIG_VERSION;

	//Register callbacks n' stuff

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(TemplateModuleConfiguration);

	//Set defaults
	ResetToDefaultConfiguration();
}

void TemplateModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = true;
	configuration.moduleVersion = TEMPLATE_MODULE_CONFIG_VERSION;

	//Set additional config values...

}

void TemplateModule::ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength)
{
	//Version migration can be added here, e.g. if module has version 2 and config is version 1
	if(migratableConfig->moduleVersion == 1){/* ... */};

	//Do additional initialization upon loading the config


	//Start the Module...

}

void TemplateModule::TimerEventHandler(u16 passedTimeDs)
{
	//Do stuff on timer...

}

#ifdef TERMINAL_ENABLED
bool TemplateModule::TerminalCommandHandler(char* commandArgs[], u8 commandArgsSize)
{
	//React on commands, return true if handled, false otherwise
	if(commandArgsSize >= 3 && TERMARGS(2, moduleName))
	{
		if(TERMARGS(0, "action"))
		{
			if(!TERMARGS(2, moduleName)) return false;

			if(commandArgsSize >= 4 && TERMARGS(3, "argument_a"))
			{

			return true;
			}
			else if(commandArgsSize >= 4 && TERMARGS(3, "argument_b"))
			{

			return true;
			}

			return false;

		}
	}

	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif


void TemplateModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader)
{
	//Must call superclass for handling
	Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){
			if(packet->actionType == TemplateModuleTriggerActionMessages::MESSAGE_0){

			}
		}
	}

	//Parse Module responses
	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_ACTION_RESPONSE){
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId)
		{
			if(packet->actionType == TemplateModuleActionResponseMessages::MESSAGE_0_RESPONSE)
			{

			}
		}
	}
}
