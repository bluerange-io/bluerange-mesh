////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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
#ifdef CHERRYSIM_TESTER_ENABLED
#include "gtest/gtest.h"
#endif
#include "CherrySimTester.h"
#include "CherrySim.h"
#include "Node.h"
#include <regex>
#include <string>
#include <chrono>
#include <iostream>
#include "FruityHal.h"
#include "Utility.h"
 
/***
This class is a wrapper around the simulator and provides methods for injecting data into the simulator
and several simulation function to simulate until some event happens.
This allows tests to be implemented in a synchronous way.
*/


//This leak detection does not work reliably because, for example, GTEST has some open allocations that it doesn't clean up until
//program termination. However it was useful in the past to manually find a leak, so it should stay and can be manually turned on
//in the future.
#define MANUAL_LEAK_SEARCH 0

#if MANUAL_LEAK_SEARCH == 1
static int openAllocs = 0;

void * operator new(std::size_t n)
{
	void* retVal = malloc(n);
	if (retVal == nullptr) {
		throw std::bad_alloc();
	}
	openAllocs++;
	return retVal;
}
void operator delete(void * p) throw()
{
	if (p != nullptr) {
		openAllocs--;
		free(p);
	}
}
#endif

#ifdef CHERRYSIM_TESTER_ENABLED
int main(int argc, char **argv) {

	//A workaround to find out if the Visual Studio Test Explorer is executing us (either on first run through list_tests or the second real run for testing)
	bool runByVisualStudioTestExplorer = argc >= 2 && (std::string(argv[1]).find("gtest_output=xml:") != std::string::npos || (std::string(argv[1]).find("gtest_list_tests") != std::string::npos));

	//Initialize google tests
	//WARNING: Will modify the arc and argv and will remove all the GTEST command line parameters
	::testing::InitGoogleTest(&argc, argv);

	// ###########################
	// Manual testing
	// ###########################
	if (!runByVisualStudioTestExplorer)
	{
		::testing::GTEST_FLAG(break_on_failure) = true;

		//If we only want to execute specific tests, we can specify them here
		::testing::GTEST_FLAG(filter) = "*TestRebootReason*";

		//Do not catch exceptions is useful for debugging (Automatically set to 1 if running on Gitlab)
		::testing::GTEST_FLAG(catch_exceptions) = 0;
	}
	// ###########################
	// Automated testing
	// ###########################
	bool GitLab = false;
	bool Scheduled = false;

	for (int i = 0; i < argc; i++)
	{
		std::string s = argv[i];
		if (s == "GitLab")
		{
			GitLab = true;
			::testing::GTEST_FLAG(catch_exceptions) = 1;
			::testing::GTEST_FLAG(filter) = "*";
			::testing::GTEST_FLAG(break_on_failure) = false;
		}
		else if (s == "Scheduled")
		{
			Scheduled = true;
		}
	}

	if (GitLab) {
		if (Scheduled){
			printf("I am scheduled!" SEP);
			::testing::GTEST_FLAG(filter) += "*_scheduled*:-*_long";
		} else {
			printf("I am NOT scheduled!" SEP);
			::testing::GTEST_FLAG(filter) += ":-*_scheduled*:*_long";
		}
	}

	auto startTime = std::chrono::high_resolution_clock::now();
#if MANUAL_LEAK_SEARCH == 1
	openAllocs = 0; //Some allocations happen from global objects or GTEST Code. We don't care about those, so we set the openAllocs to zero.
#endif
	int exitCode = 0;
	if (GitLab) {
		Exceptions::DisableDebugBreakOnException disabler;
		exitCode = RUN_ALL_TESTS();
	}
	else {
		exitCode = RUN_ALL_TESTS();
	}
	auto endTime = std::chrono::high_resolution_clock::now();
	auto diff = endTime - startTime;
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();

	std::cout << "\n\nTime for all tests: " << (ms / 1000.0) << " seconds." << std::endl;

#if MANUAL_LEAK_SEARCH == 1
	if (openAllocs)
	{
		std::cerr << std::endl;
		std::cerr << "#### ERROR ####" << std::endl;
		std::cerr << "Something was allocated but not freed!" << std::endl;
		std::cerr << "Still open allocations is: " << openAllocs << std::endl;
		return 1;
	}
#endif

	return exitCode;
}
#endif


CherrySimTesterConfig CherrySimTester::CreateDefaultTesterConfiguration()
{
	CherrySimTesterConfig config;
	config.verbose = false;
	config.terminalFilter = 0;

	return config;
}

SimConfiguration CherrySimTester::CreateDefaultSimConfiguration()
{
	SimConfiguration simConfig;

	simConfig.numNodes = 10; //asset node ids are at the end e.g if we have numNode 2 and numAssetNode 1, the node id of asset node will be 3.
	simConfig.numAssetNodes = 0;
	simConfig.seed = 1;
	simConfig.mapWidthInMeters = 60;
	simConfig.mapHeightInMeters = 40;
	simConfig.simTickDurationMs = 50;
	simConfig.terminalId = 0; //Terminal must be active in order for a test to trigger on the terminal output

	simConfig.simOtherDelay = 1; // Enter 1 - 100000 to send sim_other message only each ... simulation steps, this increases the speed significantly
	simConfig.playDelay = 0; //Allows us to view the simulation slower than simulated, is added after each step
	
	simConfig.connectionTimeoutProbabilityPerSec = 0; //Every minute or so: 0.00001;
	simConfig.sdBleGapAdvDataSetFailProbability = 0;// 0.0001; //Simulate fails on setting adv Data
	simConfig.sdBusyProbability = 0.01;// 0.0001; //Simulates getting back busy errors from softdevice
	simConfig.simulateAsyncFlash = true;
	simConfig.asyncFlashCommitTimeProbability = 0.9;

	simConfig.importFromJson = false;
	strcpy(simConfig.siteJsonPath, "C:\\Users\\MariusHeil\\Desktop\\testsite.json");
	strcpy(simConfig.devicesJsonPath, "C:\\Users\\MariusHeil\\Desktop\\testdevices.json");

	strcpy(simConfig.defaultNodeConfigName, "prod_mesh_nrf52");
	strcpy(simConfig.defaultSinkConfigName, "prod_sink_nrf52");

	simConfig.defaultBleStackType = BleStackType::NRF_SD_132_ANY;

	simConfig.defaultNetworkId = 10;

	simConfig.rssiNoise = false;

	simConfig.verboseCommands = true;
	simConfig.enableSimStatistics = false;


	return simConfig;
}

CherrySimTester::CherrySimTester(CherrySimTesterConfig testerConfig, SimConfiguration simConfig)
	: config(testerConfig), 
	  simConfig(simConfig), 
	  awaitedMessageResult("")
{
	awaitedMessagePointer = 0;
	awaitedMessagesFound = false;
	
	awaitedBleEventNodeId = 0;
	awaitedBleEventEventId = 0;
	awaitedBleEventFound = false;
	CheckedMemset(awaitedBleEventDataPart, 0x00, sizeof(awaitedBleEventDataPart));
	
	sim = new CherrySim(simConfig);
	sim->SetCherrySimEventListener(this);
	sim->Init();
	sim->RegisterTerminalPrintListener(this);
	
	//Load the number of nodes from the sim in case a json or sth. was loaded with a different amount
	CherrySimTester::simConfig.numNodes = sim->getNumNodes();
}

CherrySimTester::~CherrySimTester()
{
	if(sim != nullptr) delete sim;
	sim = nullptr;
}

void CherrySimTester::Start()
{
	if (started)
	{
		//Probably you called Start twice!
		SIMEXCEPTION(IllegalStateException);
	}

	//Boot up all nodes
	for (u32 i = 0; i < simConfig.numNodes; i++) {
#ifdef GITHUB_RELEASE
		strcpy(sim->nodes[i].nodeConfiguration, "github_nrf52");
#endif //GITHUB_RELEASE
		sim->setNode(i);
		sim->bootCurrentNode();
	}

	started = true;
}

void CherrySimTester::SimulateUntilClusteringDone(int timeoutMs)
{
	int startTimeMs = sim->simState.simTimeMs;

	while (!sim->IsClusteringDone()) {
		sim->SimulateStepForAllNodes();

		//Watch if a timeout occurs
		if (timeoutMs != 0 && startTimeMs + timeoutMs < (i32)sim->simState.simTimeMs) {
			SIMEXCEPTION(TimeoutException); //Timeout waiting for clustering
		}
	}
}

void CherrySimTester::SimulateUntilClusteringDoneWithDifferentNetworkIds(int timeoutMs)
{
	int startTimeMs = sim->simState.simTimeMs;

	while (!sim->IsClusteringDoneWithDifferentNetworkIds()) {
		sim->SimulateStepForAllNodes();

		//Watch if a timeout occurs
		if (timeoutMs != 0 && startTimeMs + timeoutMs < (i32)sim->simState.simTimeMs) {
			SIMEXCEPTION(TimeoutException); //Timeout waiting for clustering
		}
	}
}

void CherrySimTester::SimulateBroadcastMessage(double x, double y, ble_gap_evt_adv_report_t& advReport, bool ignoreDropProb)
{
	printf("Simulating broadcast message" EOL);

	sim->currentNode->x = (float)x;
	sim->currentNode->y = (float)y;

	for (u32 i = 0; i < simConfig.numNodes; i++) {
		//If the other node is scanning
		if (sim->nodes[i].state.scanningActive) {
			//If the random value hits the probability, the event is sent
			double probability = sim->calculateReceptionProbability(sim->currentNode, &(sim->nodes[i]));
			if (PSRNG() < probability || ignoreDropProb) {
				simBleEvent s;
				s.globalId = sim->simState.globalEventIdCounter++;
				s.bleEvent.header.evt_id = BLE_GAP_EVT_ADV_REPORT;
				s.bleEvent.header.evt_len = s.globalId;
				s.bleEvent.evt.gap_evt.conn_handle = BLE_CONN_HANDLE_INVALID;

				CheckedMemcpy(&s.bleEvent.evt.gap_evt.params.adv_report.data, advReport.data, sizeof(advReport.data));
				s.bleEvent.evt.gap_evt.params.adv_report.dlen = sizeof(advReport.data);
				CheckedMemcpy(&s.bleEvent.evt.gap_evt.params.adv_report.peer_addr, &advReport.peer_addr, sizeof(ble_gap_addr_t));

				s.bleEvent.evt.gap_evt.params.adv_report.rssi = (i8) sim->GetReceptionRssi(sim->currentNode, &(sim->nodes[i]));
				s.bleEvent.evt.gap_evt.params.adv_report.scan_rsp = 0;
				s.bleEvent.evt.gap_evt.params.adv_report.type = (u8)sim->currentNode->state.advertisingType;
				sim->nodes[i].eventQueue.push_back(s);
			}
		}
	}
	
}
void CherrySimTester::SimulateUntilClusteringDoneWithExpectedNumberOfClusters(int timeoutMs, int clusters)
{
	int startTimeMs = sim->simState.simTimeMs;

	while (!sim->IsClusteringDoneWithExpectedNumberOfClusters(clusters)) {
		sim->SimulateStepForAllNodes();

		//Watch if a timeout occurs
		if (timeoutMs != 0 && startTimeMs + timeoutMs < (i32)sim->simState.simTimeMs) {
			SIMEXCEPTION(TimeoutException); //Timeout waiting for clustering
		}
	}
}

void CherrySimTester::SimulateGivenNumberOfSteps(int steps)
{
	for(int i=0; i<steps; i++){
		sim->SimulateStepForAllNodes();
	}
}

void CherrySimTester::SimulateForGivenTime(int numMilliseconds)
{
	int startTimeMs = sim->simState.simTimeMs;

	while (startTimeMs + numMilliseconds > (i32)sim->simState.simTimeMs) {
		sim->SimulateStepForAllNodes();
	}
}

void CherrySimTester::SimulateUntilMessageReceived(int timeoutMs, NodeId nodeId, const char* messagePart, ...)
{
	char buffer[2048];
	va_list aptr;
	va_start(aptr, messagePart);
	vsnprintf(buffer, 2048, messagePart, aptr);
	va_end(aptr);
	std::vector<SimulationMessage> messages;
	messages.push_back(SimulationMessage(nodeId, buffer));

	SimulateUntilMessagesReceived(timeoutMs, messages);
}

void CherrySimTester::SimulateUntilMessagesReceived(int timeoutMs, std::vector<SimulationMessage>& messages)
{
	useRegex = false;
	awaitedTerminalOutputs = &messages;

	_SimulateUntilMessageReceived(timeoutMs);
}

void CherrySimTester::SimulateUntilRegexMessageReceived(int timeoutMs, NodeId nodeId, const char * messagePart, ...)
{
	char buffer[2048];
	va_list aptr;
	va_start(aptr, messagePart);
	vsnprintf(buffer, 2048, messagePart, aptr);
	va_end(aptr);
	std::vector<SimulationMessage> messages;
	messages.push_back(SimulationMessage(nodeId, buffer));
	SimulateUntilRegexMessagesReceived(timeoutMs, messages);
}

void CherrySimTester::SimulateUntilRegexMessagesReceived(int timeoutMs, std::vector<SimulationMessage>& messages)
{
	useRegex = true;
	awaitedTerminalOutputs = &messages;

	_SimulateUntilMessageReceived(timeoutMs);
}

void CherrySimTester::_SimulateUntilMessageReceived(int timeoutMs)
{
	int startTimeMs = sim->simState.simTimeMs;
	awaitedMessagesFound = false;

	while (!awaitedMessagesFound) {
		sim->SimulateStepForAllNodes();

		//Watch if a timeout occurs
		if (timeoutMs != 0 && startTimeMs + timeoutMs < (i32)sim->simState.simTimeMs) {
			SIMEXCEPTION(TimeoutException); //Timeout waiting for message
		}
	}
	awaitedTerminalOutputs = nullptr;
}

//Simulates until an event with a specific eventId is received that contains the binary data if given (binary data can be a part of the event)
void CherrySimTester::SimulateUntilBleEventReceived(int timeoutMs, NodeId nodeId, u16 eventId, const u8* eventDataPart, u16 eventDataPartLength)
{
	int startTimeMs = sim->simState.simTimeMs;

	awaitedBleEventNodeId = nodeId;
	awaitedBleEventEventId = eventId;
	awaitedBleEventDataPartLength = eventDataPartLength;
	CheckedMemcpy(awaitedBleEventDataPart, eventDataPart, eventDataPartLength);
	awaitedBleEventFound = false;

	while (!awaitedBleEventFound) {
		sim->SimulateStepForAllNodes();

		//Watch if a timeout occurs
		if (timeoutMs != 0 && startTimeMs + timeoutMs < (i32)sim->simState.simTimeMs) {
			SIMEXCEPTION(TimeoutException); //Timeout waiting for clustering
		}
	}
	awaitedBleEventNodeId = 0;
	awaitedBleEventEventId = 0;
}

//Just simulates, probably never used in tests, but only while developing
void CherrySimTester::SimulateForever()
{
	while (true) {
		sim->SimulateStepForAllNodes();
	}
}

//Sends a TerminalCommand to the currentNode as part of the current time step, use nodeId = 0 to send to all nodes
void CherrySimTester::SendTerminalCommand(NodeId nodeId, const char* message, ...)
{
	//TODO: Check what happens if we execute multiple terminal commands without simulating that node
	//TODO: Should put the terminal command in a buffer and the node should then fetch it

	char buffer[2048];
	va_list aptr;
	va_start(aptr, message);
	vsnprintf(buffer, 2048, message, aptr);
	va_end(aptr);

	if (nodeId == 0) {
		for (u32 i = 0; i < simConfig.numNodes; i++) {
			sim->setNode(i);
			if (!GS->terminal.terminalIsInitialized) {
				//you have not activated the terminal of that node either through the config or through the sim config
				SIMEXCEPTION(IllegalStateException); //Terminal of node is not active, cannot send message
			}
			GS->terminal.PutIntoReadBuffer(buffer);
			if (config.verbose) {
				printf("NODE %d TERM_IN: %s" EOL, sim->currentNode->id, buffer);
			}
		}
	} else if (nodeId > 0 && nodeId < simConfig.numNodes + 1) {
		sim->setNode(nodeId - 1);
		if (!GS->terminal.terminalIsInitialized) {
			//you have not activated the terminal of that node either through the config or through the sim config
			SIMEXCEPTION(IllegalStateException); //Terminal of node is not active, cannot send message
		}
		GS->terminal.PutIntoReadBuffer(buffer);
		if (config.verbose) {
			printf("NODE %d TERM_IN: %s" EOL, sim->currentNode->id, buffer);
		}
	} else {
		SIMEXCEPTION(IllegalStateException); //Wrong nodeId given for SendTerminalCommand
	}
}

void CherrySimTester::SendButtonPress(NodeId nodeId, u8 buttonId, u32 holdTimeDs)
{
	sim->setNode(nodeId);
	if (buttonId == 1) {
		GS->button1HoldTimeDs = holdTimeDs;
	}
	else {
		SIMEXCEPTION(IllegalStateException); //Not implemented
	}
}


//########################### Callbacks ###############################

void CherrySimTester::TerminalPrintHandler(nodeEntry* currentNode, const char* message)
{
	//Send to console
	if (config.verbose && (config.terminalFilter == 0 || config.terminalFilter == currentNode->id)) {
		printf("%s", message);
	}

	//If we are not waiting for some specific terminal output, return
	if (awaitedTerminalOutputs == nullptr || awaitedMessagesFound) return;

	//Concatenate all output into one message until an end of line is received
	u16 messageLength = (u16)strlen(message);
	CheckedMemcpy(awaitedMessageResult + awaitedMessagePointer, message, messageLength);
	awaitedMessagePointer += messageLength;

	if (awaitedMessageResult[awaitedMessagePointer - 1] == '\n') {
		awaitedMessageResult[awaitedMessagePointer - 1] = '\0';
		std::vector<SimulationMessage>& awaited = *this->awaitedTerminalOutputs;
		for (unsigned int i = 0; i < awaited.size(); i++) {
			if (!awaited[i].isFound()) {
				if (sim->currentNode->id == awaited[i].getNodeId())
				{
					if (awaited[i].checkAndSet(awaitedMessageResult, useRegex))
					{
						break; //A received message should validate only one awaited message.
					}
				}
			}
		}
		
		awaitedMessagesFound = std::all_of(awaited.begin(), awaited.end(), [](SimulationMessage& sm) {return sm.isFound(); });
		
		awaitedMessagePointer = 0;
	}
}

void CherrySimTester::CherrySimBleEventHandler(nodeEntry* currentNode, simBleEvent* simBleEvent, u16 eventSize)
{
	if (
		(awaitedBleEventNodeId == 0 || currentNode->gs.node.configuration.nodeId == awaitedBleEventNodeId)
		&& awaitedBleEventEventId != 0
		&& simBleEvent->bleEvent.header.evt_id == simBleEvent->bleEvent.header.evt_id
	) {
		if (awaitedBleEventDataPartLength > 0)
		{
			if (awaitedBleEventDataPartLength <= eventSize) {
				u16 offset = 0;
				for (int i = 0; i < eventSize; i++) {
					if (((u8*)&simBleEvent->bleEvent)[i] == awaitedBleEventDataPart[offset]) {
						offset++;
						if (offset == awaitedBleEventDataPartLength) {
							awaitedBleEventFound = true;
							break;
						}
					}
					else {
						offset = 0;
					}
				}
			}
		}
		else {
			awaitedBleEventFound = true;
		}
	}
}

void CherrySimTester::CherrySimEventHandler(const char* eventType) {

};

SimulationMessage::SimulationMessage(NodeId nodeId, const std::string& messagePart)
	:nodeId(nodeId), messagePart(messagePart)
{
}

bool SimulationMessage::checkAndSet(const std::string & message, bool useRegex)
{
	if (found) {
		SIMEXCEPTION(IllegalStateException); //The message was already found!
	}

	if (
		(useRegex && matchesRegex(message)) || 
		(!useRegex && matches(message))) {
		makeFound(message);
		return true;
	}
	else {
		return false;
	}
}

bool SimulationMessage::isFound() const
{
	return found;
}

const std::string& SimulationMessage::getCompleteMessage() const
{
	if (!found) {
		SIMEXCEPTION(IllegalStateException); //Message was not found yet!
	}
	return messageComplete;
}

NodeId SimulationMessage::getNodeId() const
{
	return nodeId;
}

bool SimulationMessage::matches(const std::string & message)
{
	return message.find(messagePart) != std::string::npos;
}

void SimulationMessage::makeFound(const std::string & messageComplete)
{
	this->messageComplete = messageComplete;
	found = true;
}

bool SimulationMessage::matchesRegex(const std::string & message)
{
	std::regex reg(messagePart);
	return std::regex_search(message, reg);
}
