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

#include <Terminal.h>
#include <AdvertisingController.h>
#include <ScanController.h>
#include <Node.h>
#include <Testing.h>
#include <Logger.h>
#include <NewStorage.h>
#include <Utility.h>

extern "C"
{
#include <ble.h>
#include <ble_gap.h>
#include <stdlib.h>
#include <inttypes.h>
#ifndef SIM_ENABLED
#include <nrf_nvic.h>
#endif
}



u32 Testing::nodeId;
Testing* Testing::instance;

Testing::Testing()
{

	instance = this;



	connectedDevices = 0;

	//nodeId = Node::getInstance()->persistentConfig.nodeId;

	//Used to test stuff

	Terminal::getInstance()->AddTerminalCommandListener(this);


//	Logger::getInstance()->enableTag("STORAGE");
//	Logger::getInstance()->enableTag("STATES");
//	Logger::getInstance()->enableTag("CONN_QUEUE");
	Logger::getInstance()->enableTag("HANDSHAKE");
//	Logger::getInstance()->enableTag("TESTING");
//	Logger::getInstance()->enableTag("CONN");
//	Logger::getInstance()->enableTag("DATA");
//	Logger::getInstance()->enableTag("ADV");
//	Logger::getInstance()->enableTag("C");
	Logger::getInstance()->enableTag("DISCONNECT");
//	Logger::getInstance()->enableTag("JOIN");
//	Logger::getInstance()->enableTag("CONN");
	Logger::getInstance()->enableTag("MODULE");




	//Logger::getInstance()->enableTag("SCAN");

	//Logger::getInstance()->logEverything = true;

	cm = ConnectionManager::getInstance();
	//cm->setConnectionManagerCallback(this);


	//Node* node = Node::getInstance();
	//node->ChangeState(discoveryState::DISCOVERY_OFF);
	//node->DisableStateMachine();

	/*scanFilterEntry filter;

	filter.grouping = groupingType::NO_GROUPING;
	filter.address.addr_type = 0xFF;
	filter.advertisingType = 0xFF;
	filter.minRSSI = -100;
	filter.maxRSSI = 100;*/

	//ScanController::getInstance()->setScanFilter(&filter);



	//Run some tests
	if(nodeId == 45)
	{
		//cm->ConnectAsMaster(458, &testAddr[2], 11);

	}

	if(nodeId == 880 || nodeId == 458 || nodeId == 847)
	{
		//AdvertisingController::getInstance()->bleSetAdvertisingState(advState::ADV_STATE_HIGH);
	}


}




void Testing::Step2()
{

}

void Testing::Step3()
{


}

int discoveredHandles = 0;


//void Testing::messageReceivedCallback(connectionPacket* inPacket)
//{
//	/*logs("message incoming, reliable:%d", bleEvent->evt.gatts_evt.params.write.op);
//
//	u8* data = bleEvent->evt.gatts_evt.params.write.data;
//	u16 len = bleEvent->evt.gatts_evt.params.write.len;
//
//	logs("Message IN: %d %d %d", data[0], data[1], data[2]);*/
//
//}

u32 testData;
u32 testData2;

bool Testing::TerminalCommandHandler(std::string commandName, std::vector<std::string> commandArgs)
{





	if (commandName == "fill")
	{
		cm->fillTransmitBuffers();
	}
	else if (commandName == "advertise")
	{

		//TODO: ADVREF AdvertisingController::getInstance()->SetAdvertisingState(advState::ADV_STATE_HIGH);

	}
	else if (commandName == "scan")
	{

		ScanController::getInstance()->SetScanState(scanState::SCAN_STATE_HIGH);

	}
	else if (commandName == "reset")
	{
		FruityHal::SystemReset();

	}
	else
	{
		return false;
	}
	return true;
}

