/**

Copyright (c) 2014-2017 "M-Way Solutions GmbH"
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
 * All temporary Testing code is put here.
 */

#pragma once

#include <Terminal.h>
#include <ConnectionManager.h>

extern "C"{
#include <ble_gap.h>
}



class Testing : public TerminalCommandListener
{
public:
	Testing();

	static Testing* instance;

	static void Step2();
	static void Step3();
	

	//Methods of TerminalCommandListener
	bool TerminalCommandHandler(std::string commandName, std::vector<std::string> commandArgs);


private:
	static u32 nodeId;

	ConnectionManager* cm;



	u32 connectedDevices;
};
