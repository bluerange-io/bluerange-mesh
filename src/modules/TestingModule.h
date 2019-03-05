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

#pragma once

#include <Module.h>
#include <AdvertisingController.h>

#pragma pack(push)
#pragma pack(1)

//Service Data (max. 24 byte)
#define SIZEOF_AUTOMATES_TEST_PAYLOAD 6
#define SERVICE_DATA_TEST_MESSAGE_TYPE 0x04
typedef struct {
	// 2 byte data
	u16 len;
	u16 messageType; //0x04 for Test Service
	u16 data;
} advPacketPayload;

typedef struct {
	advStructureFlags flags;
	advStructureUUID16 serviceuuid;
	advPacketPayload payload;
} testAdvertisementPacket;

#pragma pack(pop)


class TestingModule: public Module {
private:

	//Module configuration that is saved persistently (size must be multiple of 4)
	struct TestingModuleConfiguration: ModuleConfiguration {
		//Insert more persistent config values here
	};

	TestingModuleConfiguration configuration;

#pragma pack(push)
#pragma pack(1)

#define SIZEOF_LATENCY_TEST_MESSAGE 1
	typedef struct {
		u8 ttl;
	} TestingModuleLatencyMessage;

#define SIZEOF_FLOOD_TEST_MESSAGE 4
	typedef struct {
		//u8 packetIn;
		u32 packetOut;
	} TestingModuleFloodMessage;

#pragma pack(pop)

	enum class TestingModuleTriggerActionMessages {
		STARTAUTOMATEDTEST = 0,
		AUTOMATEDTEST_LATENCY,
		AUTOMATEDTEST_FLOODING
	};

	enum class TestingModuleActionResponseMessages {
		STARTAUTOMATEDTEST_RESPONSE = 0,
		AUTOMATEDTEST_LATENCY_RESPONSE
	};

	enum TestApplication {
		ALL = 0,
		LED_TEST,
		ADVERTISEMENT_TEST,
		SCANNING_TEST,
		CONNECTION_TEST,
		LATENCY_TEST,
		NO_TEST
	};

	enum TestRole {
		Master = 0,
		Slave
	};

	public:

	fh_ble_gap_addr_t address;
	u16 uniqueConnId;bool testIsActive;bool startTest;
	u8 queuedPackets;
	u16 connectionHandle;
	u32 pingSentTicks;
	u32 packetCounter;
	u32 startTimeMs;
	u8 counter;
	TestingModule();
	TestRole role;
	TestApplication applicationToTest;

	void ResetToDefaultConfiguration() override;
	void TimerEventHandler(u16 passedTimeDs) override;
	void BleEventHandler(const ble_evt_t& bleEvent) override;
	void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader) override;


	#ifdef TERMINAL_ENABLED
	bool TerminalCommandHandler(char* commandArgs[], u8 commandArgsSize);
	#endif

	void StartAutomatedTestAsMaster(TestApplication application, NodeId destinationNode);
	void StartAutomatedTestAsSlave(TestApplication application,	NodeId destionationNode);
	void AdvertisementsTest();
	void ScanningTest();
	void HandleScannedPackets(ble_evt_t* bleEvent);
	void ConnectionTest(NodeId destinationNode, u8 packetsToBeSent);
	void LatencyTest(NodeId destinationNode, u8 pingCount);
	void ConnectToSlave(u16 connIntervalMs);
	void StartAdvertising(i8 txPower, u16 advIntervalMs, u8 packetSize, u8 advType);
	void StartScanning(u16 interval, u16 window, u16 timeout);
	void TestInit(TestApplication application, fh_ble_gap_addr_t* address);
	void LEDsTest();
	void FloodMessage(NodeId destinationNode, u8 packetsToBeSent);
};
