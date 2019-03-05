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

#define TESTING_MODULE_CONFIG_VERSION 1

#include <TestingModule.h>
#ifdef ACTIVATE_TESTING_MODULE
#include <types.h>
#include <Logger.h>
#include <Utility.h>
#include <Node.h>
#include <GAPController.h>
#include <FruityHal.h>
#include <MeshAccessConnection.h>
#include <Config.h>
extern "C"{

}

#define ____________Initialization____________________________________
TestingModule::TestingModule()
	: Module(moduleID::TESTING_MODULE_ID, "testing")
{

	moduleVersion = TESTING_MODULE_CONFIG_VERSION;

	//Register callbacks n' stuff

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(TestingModuleConfiguration);

	//Set defaults
	ResetToDefaultConfiguration();

	// variable initializations
	uniqueConnId = 0;
	testIsActive = false;
	connectionHandle = 0;
	applicationToTest = TestApplication::ALL;
	role = TestRole::Master;
	pingSentTicks = 0;
	packetCounter = 0;
	queuedPackets = 0;
	startTimeMs = 0;
	counter = 0;
	startTest = false;
}


void TestingModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleID::TESTING_MODULE_ID;
	configuration.moduleActive = false;
	configuration.moduleVersion = TESTING_MODULE_CONFIG_VERSION;

	//Set additional config values...

	if(configuration.moduleActive){
		//Start advertisement so connection is possible
		StartAdvertising(0, 100, 10, BLE_GAP_ADV_TYPE_ADV_IND);
	}

}

//Test initialization, Entry point to starting the test
void TestingModule::TestInit(TestApplication application, fh_ble_gap_addr_t* address)
{
	applicationToTest = application;
	testIsActive = false;
	FruityHal::BleGapAdvStop();
	ConnectToSlave(10);
}

#define ____________Test_Start_______________________________________

//Start Test as Master, Some tests require master to start test e.g Latency and Connection tests
void TestingModule::StartAutomatedTestAsMaster(TestApplication application, NodeId destinationNode)
{
	logt("TEST", "Start Test as Master");

	if (application == TestApplication::LATENCY_TEST) {
		LatencyTest(destinationNode, 10);
	}

	if (application == TestApplication::CONNECTION_TEST) {
		//TODO: Start connection with different connection parameter:@ the moment mesh access connection can only be connected as master
		queuedPackets = 0;
		ConnectionTest(destinationNode, 40);
		startTimeMs = GS->appTimerDs;
	}

}

//Start Test as Slave, Some tests require slave to start the test
void TestingModule::StartAutomatedTestAsSlave(TestApplication application, NodeId destinationNode)
{

	if (application == TestApplication::LED_TEST) {
		LEDsTest();
	}

	if (application == TestApplication::ADVERTISEMENT_TEST) {
		AdvertisementsTest();
	}

	if (application == TestApplication::SCANNING_TEST) {
		logt("TEST", "Scanning Test Starting");
		FruityHal::BleGapAdvStop();
		ScanningTest();
	}

}

void TestingModule::ConnectToSlave(u16 connIntervalMs)
{
	uniqueConnId = MeshAccessConnection::ConnectAsMaster(&address, connIntervalMs, 6, FM_KEY_ID_NETWORK, nullptr, MeshAccessTunnelType::PEER_TO_PEER);
}

#define ____________Test_Applications_______________________________________
void TestingModule::LEDsTest()
{
	logt("TEST","LED Test Starting");

	//Switch on Red LED
	GS->ledRed->On();
	FruityHal::DelayMs(10000);
	GS->ledRed->Off();

	//Switch on Green LED
	GS->ledGreen->On();
	FruityHal::DelayMs(10000);
	GS->ledGreen->Off();

}

// Start Advertisement test, each advertisement configuration will run for 30 sec
void TestingModule::AdvertisementsTest()
{
	startTimeMs = (counter == 36) ? 0 : GS->appTimerDs;
	switch (counter) {
	//Advertise connectable at 100ms, 200ms, 400ms, 1000ms, 4000ms with Tx Power 0 dbm, adv type connectable and payload 10 byte
	case 0:
		StartAdvertising(0, 100, 10, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	case 1:
		StartAdvertising(0, 200, 10, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	case 2:
		StartAdvertising(0, 400, 10, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	case 3:
		StartAdvertising(0, 1000, 10, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	case 4:
		StartAdvertising(0, 4000, 10, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
		//Advertise connectable at 100ms, 200ms, 400ms, 1000ms, 4000ms with Tx Power 0 dbm, adv type connectable and payload 30 byte
	case 5:
		StartAdvertising(0, 100, 30, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	case 6:
		StartAdvertising(0, 200, 30, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	case 7:
		StartAdvertising(0, 400, 30, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	case 8:
		StartAdvertising(0, 1000, 30, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	case 9:
		StartAdvertising(0, 4000, 30, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	case 10:
		//Advertise connectable at 100ms, 200ms, 400ms, 1000ms, 4000ms with Tx Power 4 dbm, adv type connectable and payload 10 byte
		StartAdvertising(4, 100, 10, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	case 11:
		StartAdvertising(4, 400, 10, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	case 12:
		StartAdvertising(4, 1000, 10, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	case 13:
		StartAdvertising(4, 4000, 10, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
		//Advertise connectable at 100ms, 200ms, 400ms, 1000ms, 4000ms with Tx Power 4 dbm, adv type connectable and payload 30 byte
	case 14:
		StartAdvertising(4, 100, 30, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	case 15:
		StartAdvertising(4, 200, 30, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	case 16:
		StartAdvertising(4, 1000, 30, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	case 17:
		StartAdvertising(4, 4000, 30, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
		//Advertise connectable at 100ms, 200ms, 400ms, 1000ms, 4000ms with Tx Power 0 dbm, adv type non-connectable and payload 10 byte
	case 18:
		StartAdvertising(0, 100, 10, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
	case 19:
		StartAdvertising(0, 200, 10, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
	case 20:
		StartAdvertising(0, 400, 10, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
	case 21:
		StartAdvertising(0, 1000, 10, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
	case 22:
		StartAdvertising(0, 4000, 10, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
		//Advertise connectable at 100ms, 200ms, 400ms, 1000ms, 4000ms with Tx Power 0 dbm, adv type non-connectable and payload 30 byte
	case 23:
		StartAdvertising(0, 100, 30, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
	case 24:
		StartAdvertising(0, 200, 30, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
	case 25:
		StartAdvertising(0, 400, 30, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
	case 26:
		StartAdvertising(0, 1000, 30, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
	case 27:
		StartAdvertising(0, 4000, 30, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
	case 28:
		//Advertise connectable at 100ms, 200ms, 400ms, 1000ms, 4000ms with Tx Power 4 dbm, adv type non-connectable and payload 10 byte
		StartAdvertising(4, 100, 10, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
	case 29:
		StartAdvertising(4, 400, 10, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
	case 30:
		StartAdvertising(4, 1000, 10, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
	case 31:
		StartAdvertising(4, 4000, 10, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
		//Advertise connectable at 100ms, 200ms, 400ms, 1000ms, 4000ms with Tx Power 4 dbm, adv type non-connectable and payload 30 byte
	case 32:
		StartAdvertising(4, 100, 30, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
	case 33:
		StartAdvertising(4, 200, 30, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
	case 34:
		StartAdvertising(4, 1000, 30, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
	case 35:
		StartAdvertising(4, 4000, 30, BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
		break;
		//end test condition
	case 36:
		counter = 0;
		StartAdvertising(0, 100, 10, BLE_GAP_ADV_TYPE_ADV_IND);
		break;

	}

}

//Start Scanning Test, each scanning test will run for 30 sec
void TestingModule::ScanningTest()
{
	startTimeMs = (counter == 2) ? 0 : GS->appTimerDs;
	switch (counter) {
	//scan for 3ms in 20ms time interval
	case 0:
		FruityHal::BleGapScanStop();
		StartScanning(20, 3, 0);
		break;
	//scan for 10ms in 10ms time interval(full duty cycle)
	case 1:
		FruityHal::BleGapScanStop();
		StartScanning(10, 10, 0);
		break;
	case 2:
		FruityHal::BleGapScanStop();
		StartAdvertising(0, 100, 10, BLE_GAP_ADV_TYPE_ADV_IND);
		break;
	}
}

//Start Connection Test,currently only flooding test is performed
//TODO: Different Connection parameters should be test
void TestingModule::ConnectionTest(NodeId destinationNode, u8 packetsToBeSent)
{
	logt("TEST","Start Connection Test");
	TestingModuleFloodMessage data;
	for(u16 i =0; i<packetsToBeSent; i++){
	packetCounter++;
	data.packetOut = packetCounter;
	SendModuleActionMessage(
		MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
		destinationNode,
		(u8)TestingModuleTriggerActionMessages::AUTOMATEDTEST_FLOODING,
		0,
		(u8*)&data,
		sizeof(data),
		false
	);
	}
}

//Latency Test
void TestingModule::LatencyTest(NodeId destinationNode, u8 pingCount)
{
	logt("TEST", "Start Latency Test");
	pingSentTicks = FruityHal::GetRtc();
	TestingModuleLatencyMessage data;
	data.ttl = 2*pingCount-1;
	SendModuleActionMessage(
		MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
		destinationNode,
		(u8)TestingModuleTriggerActionMessages::AUTOMATEDTEST_LATENCY,
		0,
		(u8*)&data,
		sizeof(data),
		false
	);

}

#define ____________Scan_and_Advertisement_Start_______________________________________
void TestingModule::StartScanning(u16 interval, u16 window, u16 timeout)
{
	u32 err;
	logt("TEST","Start Scanning for %ums every %ums", window, interval);
	fh_ble_gap_scan_params_t scanParams = { (u16) MSEC_TO_UNITS(interval, UNIT_0_625_MS), (u16) MSEC_TO_UNITS(window, UNIT_0_625_MS), 0 };
	err = FruityHal::BleGapScanStart(&scanParams);
	if (err != NRF_SUCCESS) {
		logt("ERROR","error code = %u", err);
	}
}

void TestingModule::StartAdvertising(i8 txPower, u16 advIntervalMs, u8 packetSize, u8 advType)
{
	logt("TEST", "Start Advertising with Tx Power = %u, advIntervalMs = %u, packetSize = %u", txPower, advIntervalMs, packetSize);
	u32 err;

	fh_ble_gap_adv_ch_mask_t channelMask = { 0, 0, 0 };
	fh_ble_gap_adv_params_t advertisingParameter = {
													advType,
													(u16) MSEC_TO_UNITS(advIntervalMs, UNIT_0_625_MS),
													0,
													channelMask };

	//Advertising Packet
	DYNAMIC_ARRAY(packet, packetSize);

	//set tx power for advertisement
	err = sd_ble_gap_tx_power_set(txPower);
	if (err != NRF_SUCCESS) {
		logt("ERROR","error code = %u", err);
	}

	//set Advertising Data
	err = FruityHal::BleGapAdvDataSet(packet, packetSize, 0, 0);
	if (err != NRF_SUCCESS) {
		logt("ERROR","error code = %u", err);
	}

	//start Advertisement Power
	err = FruityHal::BleGapAdvStart(&advertisingParameter);
	if (err != NRF_SUCCESS) {
		logt("ERROR","error code = %u", err);
	}

}


#define ____________Event_Handlers_______________________________________
void TestingModule::TimerEventHandler(u16 passedTimeDs)
{

	MeshAccessConnection*conn = (MeshAccessConnection*)GS->cm->GetConnectionByUniqueId(uniqueConnId);
    if (role == TestRole::Master && conn != nullptr)
    {
		if (conn->connectionState == ConnectionState::HANDSHAKE_DONE && testIsActive == false) {
			NodeId destinationNode = conn->virtualPartnerId;
			logt("TEST", "Start with destination node = %u and applicationToTest = %u", destinationNode, applicationToTest);

			//Sends a start test message to slave
			SendModuleActionMessage(
				MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
				destinationNode,
				(u8)TestingModuleTriggerActionMessages::STARTAUTOMATEDTEST,
				0,
				(u8*)&applicationToTest,
				sizeof(applicationToTest),
				false
		);

			testIsActive = true;
		}

		if (conn->connectionState == ConnectionState::HANDSHAKE_DONE && startTest == true && role == TestRole::Master && testIsActive == true) {
			StartAutomatedTestAsMaster(applicationToTest, conn->virtualPartnerId);
			startTest = false;
		}
		//Disconnect connection Test after 10 sec
		if (GS->appTimerDs == startTimeMs + SEC_TO_DS(10) && startTimeMs != 0) {
			applicationToTest = TestApplication::NO_TEST;
			GS->gapController->disconnectFromPartner(connectionHandle);
			startTimeMs = 0;
		}
	}

	if (role == TestRole::Slave && GS->appTimerDs == startTimeMs + SEC_TO_DS(30) && startTimeMs != 0 ){
		counter++;
		StartAutomatedTestAsSlave(applicationToTest, NODE_ID_BROADCAST);
	}

}

#ifdef TERMINAL_ENABLED
bool TestingModule::TerminalCommandHandler(char* commandArgs[], u8 commandArgsSize)
{
	//React on commands, return true if handled, false otherwise
	if(commandArgsSize >= 3 && TERMARGS(2, moduleName))
	{
		NodeId destinationNode = (TERMARGS(1 ,"this")) ? GS->node->configuration.nodeId : atoi(commandArgs[1]);

		if(TERMARGS(0, "action"))
		{
			if (commandArgsSize >= 4 && TERMARGS(3, "starttest"))
			{
				u8 buffer[6];
				GS->logger->parseHexStringToBuffer(commandArgs[4], buffer, 6);
				u8 applicationToTest = atoi(commandArgs[5]);

				address.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
				address.addr[0] = buffer[5];
				address.addr[1] = buffer[4];
				address.addr[2] = buffer[3];
				address.addr[3] = buffer[2];
				address.addr[4] = buffer[1];
				address.addr[5] = buffer[0];

				TestInit((TestApplication) applicationToTest, &address);

				return true;
			}

		}
	}
	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandArgs, commandArgsSize);
}
#endif

void TestingModule::MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader)
{
	//Must call superclass for handling
	Module::MeshMessageReceivedHandler(connection, sendData, packetHeader);

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModule* packet = (connPacketModule*)packetHeader;
		//Check if our module is meant and we should trigger an action
		if(packet->moduleId == moduleId){
			if(packet->actionType == (u8)TestingModuleTriggerActionMessages::STARTAUTOMATEDTEST){
				// Message Received by slave to start test
				applicationToTest = (TestApplication) (*packet->data);
				role = TestRole::Slave;
				//Send Response Message
				SendModuleActionMessage(
					MESSAGE_TYPE_MODULE_ACTION_RESPONSE,
					packet->header.sender,
					(u8)TestingModuleActionResponseMessages::STARTAUTOMATEDTEST_RESPONSE,
					packet->requestHandle,
					nullptr,
					0,
					false
				);

				GS->gapController->disconnectFromPartner(connectionHandle);

				//Switch Led on for 2 sec as an indication of start of test
				GS->ledRed->On();
				GS->ledGreen->On();

				FruityHal::DelayMs(2000);

				GS->ledRed->Off();
				GS->ledGreen->Off();

			}

			if(packet->actionType == (u8)TestingModuleTriggerActionMessages::AUTOMATEDTEST_LATENCY) {
				TestingModuleLatencyMessage* data = (TestingModuleLatencyMessage*)packet->data;
				//Ping should still pong, return it
				if (data->ttl > 0) {
					data->ttl--;
					SendModuleActionMessage(
							MESSAGE_TYPE_MODULE_TRIGGER_ACTION,
							packet->header.sender,
							(u8)TestingModuleTriggerActionMessages::AUTOMATEDTEST_LATENCY,
							0,
							(u8*)data,
							sizeof(data),
							false
						);

				//Arrived at destination, print it
				} else {
					u32 nowTicks;
					u32 timePassed;
					nowTicks = FruityHal::GetRtc();
					timePassed = FruityHal::GetRtcDifference(nowTicks, pingSentTicks);
					u32 timePassedMs = timePassed/(APP_TIMER_CLOCK_FREQ / 1000);
					logt("TEST", "timePassedMs = %u", timePassedMs);
					applicationToTest = TestApplication::NO_TEST;
					GS->gapController->disconnectFromPartner(connectionHandle);
				}
			}

			if (packet->actionType == (u8) TestingModuleTriggerActionMessages::AUTOMATEDTEST_FLOODING) {
				TestingModuleFloodMessage* data = (TestingModuleFloodMessage*) packet->data;
				u32 packetIn = data->packetOut;
			}

		}
	}

	//Parse Module responses
	if (packetHeader->messageType == MESSAGE_TYPE_MODULE_ACTION_RESPONSE) {
		connPacketModule* packet = (connPacketModule*)packetHeader;

		//Check if our module is meant and we should trigger an action
		if (packet->moduleId == moduleId) {
			logt("TEST", "RESPONSE");

			if (packet->actionType == (u8) TestingModuleActionResponseMessages::STARTAUTOMATEDTEST_RESPONSE) {
				GS->ledRed->On();
				GS->ledGreen->On();

				FruityHal::DelayMs(2000);

				GS->ledRed->Off();
				GS->ledGreen->Off();

			}

		}
	}
}

void TestingModule::HandleScannedPackets(ble_evt_t* bleEvent)
{
	u8* buffer = (u8*)bleEvent->evt.gap_evt.params.adv_report.data;
	logt("TEST","buffer = %u  *buffer = %u", buffer[0], buffer);
}

void TestingModule::BleEventHandler(const ble_evt_t& bleEvent) {
	if (bleEvent.header.evt_id == BLE_GAP_EVT_CONNECTED) {
		connectionHandle = bleEvent.evt.gap_evt.conn_handle;
		logt("TEST", "Connected with connection handle = %u", connectionHandle);
	}

	if (bleEvent.header.evt_id == BLE_GAP_EVT_DISCONNECTED) {

		if (role == TestRole::Slave) {
			if (applicationToTest == TestApplication::ADVERTISEMENT_TEST
					|| applicationToTest == TestApplication::SCANNING_TEST
					|| applicationToTest == TestApplication::LED_TEST) {
				MeshAccessConnection*conn = (MeshAccessConnection*) GS->cm;
				logt("TEST", "Disconnect");
				conn->DisconnectAndRemove();
				StartAutomatedTestAsSlave(applicationToTest, NODE_ID_BROADCAST);
			}

			else if (applicationToTest == TestApplication::LATENCY_TEST || applicationToTest == TestApplication::CONNECTION_TEST){
				StartAdvertising(0, 100, 10, BLE_GAP_ADV_TYPE_ADV_IND);
				StartAutomatedTestAsSlave(applicationToTest, NODE_ID_BROADCAST);
			}

			else
			{
				MeshAccessConnection*conn = (MeshAccessConnection*) GS->cm;
				conn->DisconnectAndRemove();
				StartAdvertising(0, 100, 10, BLE_GAP_ADV_TYPE_ADV_IND);
			}
		}

		if (applicationToTest == TestApplication::LATENCY_TEST || applicationToTest == TestApplication::CONNECTION_TEST){
			if (role == TestRole::Master) {
				startTest = true;
				ConnectToSlave(10);
			}
		}

	}




#if defined(NRF51) || defined(SIM_ENABLED)
	if(bleEvent.header.evt_id == BLE_EVT_TX_COMPLETE && role == TestRole::Master)
#elif defined(NRF52)
	if (bleEvent.header.evt_id == BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE && role == TestRole::Master)
#endif
	{

		if (bleEvent.evt.gattc_evt.gatt_status == BLE_GATT_STATUS_SUCCESS) {
			queuedPackets++;
			logt("TEST", "Packet Queued = %u", queuedPackets);

		}

	}
}

#endif
