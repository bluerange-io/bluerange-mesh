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
#include <Config.h>
#include <Node.h>

void setFeaturesetConfiguration_github(ModuleConfiguration* config)
{
	if(config->moduleId == moduleID::BOARD_CONFIG_ID)
	{
		BoardConfiguration* c = (BoardConfiguration*) config;

		//Additional boards can be put in here to be selected at runtime
		//e.g. setBoard_123(c);
	}
	else if (config->moduleId == moduleID::CONFIG_ID)
	{
		Conf::ConfigConfiguration* c = (Conf::ConfigConfiguration*) config;
		c->defaultLedMode = LED_MODE_CONNECTIONS;
		c->terminalMode = TERMINAL_PROMPT_MODE;
	}
	else if (config->moduleId == moduleID::NODE_ID)
	{
		//Specifies a default enrollment for the github configuration
		//This enrollment will be overwritten as soon as the node is either enrolled or the enrollment removed
		NodeConfiguration* c = (NodeConfiguration*) config;
		c->enrollmentState = EnrollmentState::ENROLLED;
		c->networkId = 11;
		memset(c->networkKey, 0x00, 16);
	}
}
