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

#include <Terminal.h>
#include <AdvertisingController.h>
#include <ScanController.h>
#include <Node.h>
#include <Testing.h>
#include <Logger.h>
#include <Storage.h>
#include <NewStorage.h>
#include <Utility.h>

extern "C"
{
#include <ble.h>
#include <ble_gap.h>
#include <stdlib.h>
#include <inttypes.h>
#include <nrf_nvic.h>
}



u32 Testing::nodeId;
Testing* Testing::instance;

Testing::Testing()
{

	instance = this;



	connectedDevices = 0;

	nodeId = Node::getInstance()->persistentConfig.nodeId;

	//Used to test stuff

	Terminal::AddTerminalCommandListener(this);

	//Storage::getInstance();

//	Logger::getInstance().enableTag("STORAGE");
//	Logger::getInstance().enableTag("STATES");
//	Logger::getInstance().enableTag("CONN_QUEUE");
	Logger::getInstance().enableTag("HANDSHAKE");
//	Logger::getInstance().enableTag("TESTING");
//	Logger::getInstance().enableTag("CONN");
//	Logger::getInstance().enableTag("DATA");
//	Logger::getInstance().enableTag("ADV");
//	Logger::getInstance().enableTag("C");
	Logger::getInstance().enableTag("DISCONNECT");
//	Logger::getInstance().enableTag("JOIN");
//	Logger::getInstance().enableTag("CONN");
	Logger::getInstance().enableTag("MODULE");




	//Logger::getInstance().enableTag("SCAN");

	//Logger::getInstance().logEverything = true;

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

	//ScanController::setScanFilter(&filter);



	//Run some tests
	if(nodeId == 45)
	{
		//cm->ConnectAsMaster(458, &testAddr[2], 11);

	}

	if(nodeId == 880 || nodeId == 458 || nodeId == 847)
	{
		//AdvertisingController::bleSetAdvertisingState(advState::ADV_STATE_HIGH);
	}


}



void Testing::testPacketQueue(){
	u8 buffer[800];

	memset(buffer, 0, 800);

	u8* innerBuffer = buffer + 100;

	PacketQueue* queue = new PacketQueue(innerBuffer, 600);

	u8 testData[200];
	for(int i=0; i<200; i+=1){
		testData[i] = i+1;
	}

	int i=0;
	while(true){
		u32 rand = (234876 % i) % 100;
		rand += 3;
		if(i% 100 == 0) trace("%u,",rand);

		memset(testData, rand, rand);

		u32 j=0;
		for(j=0; j<rand; j++){
			if(!queue->Put(testData, rand, true)) break;
		}

		for(; j>0; j--){
			sizedData readData = queue->PeekNext();
			queue->DiscardNext();

			if(readData.length == 0){
				logt("ERROR", "size is 0");
			}

			if(((u32)readData.length - 1) != rand){
				logt("ERROR", "Wrong size read (%u instead of %u)", readData.length, rand);
			}

			//Check data
			if(memcmp(testData, readData.data+1, rand) != 0){
				logt("ERROR", "wrong data");
			}
		}

		//Check bounds
		if(buffer[99] != 0 || buffer[700] != 0){
			logt("ERROR", "overflow");
		}

		i++;
	}

}



void Testing::Step2()
{

}

void Testing::Step3()
{


}

int discoveredHandles = 0;

void Testing::DisconnectionHandler(ble_evt_t* bleEvent)
{
	log("DISCONNECT");
}
void Testing::ConnectionSuccessfulHandler(ble_evt_t* bleEvent)
{
	log("CONNECTED");


}

void Testing::ConnectingTimeoutHandler(ble_evt_t* bleEvent)
{
	log("TIMEOUT");
}

void Testing::messageReceivedCallback(connectionPacket* inPacket)
{
	/*log("message incoming, reliable:%d", bleEvent->evt.gatts_evt.params.write.op);

	u8* data = bleEvent->evt.gatts_evt.params.write.data;
	u16 len = bleEvent->evt.gatts_evt.params.write.len;

	log("Message IN: %d %d %d", data[0], data[1], data[2]);*/

}

u32 testData;
u32 testData2;

bool Testing::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	if (commandName == "nswrite")
	{

		testData = 0x12345678;
		testData2 = 0x55667744;

		u32* dest = (u32*)(210 * NRF_FICR->CODEPAGESIZE);
		u32* dest2 = (u32*)(211 * NRF_FICR->CODEPAGESIZE);

		NewStorage::WriteData(&testData, dest, 4, NULL, 0);
		NewStorage::WriteData(&testData2, dest2, 4, NULL, 0);

		return true;
	}
	if (commandName == "nserase")
	{

		NewStorage::ErasePage(210, NULL, 0);

		return true;
	}
	if (commandName == "nserasepages")
	{

		NewStorage::ErasePages(210, 2, NULL, 0);

		return true;
	}




	if (commandName == "send")
	{
		//parameter 1: R=reliable, U=unreliable, B=both
		//parameter 2: count

		connPacketData1 data;
		data.header.messageType = MESSAGE_TYPE_DATA_1;
		data.header.sender = nodeId;
		data.header.receiver = 0;

		data.payload.length = 7;
		data.payload.data[2] = 7;


		u8 reliable = (commandArgs.size() < 1 || commandArgs[0] == "b") ? 2 : (commandArgs[0] == "u" ? 0 : 1);

		//Second parameter is number of messages
		u8 count = commandArgs.size() > 1 ? atoi(commandArgs[1].c_str()) : 5;

		for (int i = 0; i < count; i++)
		{
			if(reliable == 0 || reliable == 2){
				data.payload.data[0] = i*2;
				data.payload.data[1] = 0;
				cm->SendMessage(cm->inConnection, (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, false);
				cm->SendMessage(cm->outConnections[0], (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, false);
				cm->SendMessage(cm->outConnections[1], (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, false);
				cm->SendMessage(cm->outConnections[2], (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, false);
			}

			if(reliable == 1 || reliable == 2){
				data.payload.data[0] = i*2+1;
				data.payload.data[1] = 1;
				cm->SendMessage(cm->inConnection, (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, true);
				cm->SendMessage(cm->outConnections[0], (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, true);
				cm->SendMessage(cm->outConnections[1], (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, true);
				cm->SendMessage(cm->outConnections[2], (u8*)&data, SIZEOF_CONN_PACKET_DATA_1, true);
			}
		}

	}
	else if (commandName == "fill")
	{
		cm->fillTransmitBuffers();
	}
	else if (commandName == "forcefill")
	{
		cm->connections[0]->reliableBuffersFree = 1;
		cm->connections[1]->reliableBuffersFree = 1;
		cm->connections[2]->reliableBuffersFree = 1;
		cm->connections[3]->reliableBuffersFree = 1;
		cm->fillTransmitBuffers();
	}
	else if (commandName == "advertise")
	{

		AdvertisingController::SetAdvertisingState(advState::ADV_STATE_HIGH);

	}
	else if (commandName == "scan")
	{

		ScanController::SetScanState(scanState::SCAN_STATE_HIGH);

	}
	else if (commandName == "reset")
	{
		sd_nvic_SystemReset();

	}
	else
	{
		return false;
	}
	return true;
}

