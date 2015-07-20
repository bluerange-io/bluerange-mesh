/**

Copyright (c) 2014-2015 "M-Way Solutions GmbH"
FruityMesh - Bluetooth Low Energy mesh protocol [http://mwaysolutions.com/]

This file is part of FruityMesh

FruityMesh is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

/*
 * The status reporter module is responsible for measuring battery, connections,
 * etc... and report them back to a sink
 */

#pragma once

#include <Module.h>
#include <Terminal.h>

class StatusReporterModule: public Module
{
	private:

		//Module configuration that is saved persistently (size must be multiple of 4)
		struct StatusReporterModuleConfiguration : ModuleConfiguration{
			//Insert more persistent config values here
			u16 samplingIntervalMs;
			u16 reportingIntervalMs;
		};

		StatusReporterModuleConfiguration configuration;

		enum StatusModuleMessages{LED_MESSAGE=0};

		u32 lastReportingTimer;

		void SendConnectionInformation(Connection* toConnection);

	public:
		StatusReporterModule(u16 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot);

		void ConfigurationLoadedHandler();

		void ResetToDefaultConfiguration();

		void TimerEventHandler(u16 passedTime, u32 appTimer);

		bool TerminalCommandHandler(string commandName, vector<string> commandArgs);

		void ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength);
};
