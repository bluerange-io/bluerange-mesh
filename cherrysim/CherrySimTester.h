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
#pragma once

#include <CherrySim.h>

constexpr int MAX_TERMINAL_OUTPUT = 1024;

struct CherrySimTesterConfig
{
	bool verbose;
	int terminalFilter; //Set to -1 for no output, 0 for all nodes or any other number for the terminal of that node
};

class SimulationMessage
{
private:
	NodeId      nodeId;
	std::string messagePart;
	std::string messageComplete = "";
	bool        found = false;

	bool matches(const std::string &message);
	void makeFound(const std::string &messageComplete);
	bool matchesRegex(const std::string &message);

public:
	SimulationMessage(NodeId nodeId, const std::string& messagePart);
	bool checkAndSet(const std::string &message, bool useRegex);
	bool isFound() const;
	const std::string& getCompleteMessage() const;
	NodeId getNodeId() const;
};

class CherrySimTester : public TerminalPrintListener, public CherrySimEventListener
{
public:
	CherrySim* sim;

	//Used for awaiting specific terminal messages
	std::vector<SimulationMessage>* awaitedTerminalOutputs = nullptr;
	bool useRegex;
	char awaitedMessageResult[MAX_TERMINAL_OUTPUT];
	u16 awaitedMessagePointer;
	bool awaitedMessagesFound;

	//Used for awaiting specific ble events
	NodeId awaitedBleEventNodeId;
	u16 awaitedBleEventEventId;
	u8 awaitedBleEventDataPart[1000];
	u16 awaitedBleEventDataPartLength;
	bool awaitedBleEventFound;
private:
	CherrySimTesterConfig config;
	SimConfiguration simConfig;
	void _SimulateUntilMessageReceived(int timeoutMs);
	bool started = false;

public:
	CherrySimTester(CherrySimTesterConfig testerConfig, SimConfiguration simConfig);
	~CherrySimTester();

	static CherrySimTesterConfig CreateDefaultTesterConfiguration();
	static SimConfiguration CreateDefaultSimConfiguration();
	//### Starting
	void Start();

	//### Simulation methods
	void SimulateUntilClusteringDone(int timeoutMs);
	void SimulateUntilClusteringDoneWithDifferentNetworkIds(int timeoutMs);

	void SimulateUntilClusteringDoneWithExpectedNumberOfClusters(int timeoutMs, int clusters);
	void SimulateGivenNumberOfSteps(int steps);
	void SimulateForGivenTime(int numMilliseconds);
	void SimulateUntilMessageReceived(int timeoutMs, NodeId nodeId, const char* messagePart, ...);
	//Simulates until all of the messages in the messages vector are received. N copies of the same message must be received N times.
	void SimulateUntilMessagesReceived(int timeoutMs, std::vector<SimulationMessage>& messages);
	void SimulateUntilRegexMessageReceived(int timeoutMs, NodeId nodeId, const char* messagePart, ...);
	void SimulateUntilRegexMessagesReceived(int timeoutMs, std::vector<SimulationMessage>& messages);
	void SimulateUntilBleEventReceived(int timeoutMs, NodeId nodeId, u16 eventId, const u8* eventDataPart, u16 eventDataPartLength);
#ifndef CI_PIPELINE
	void SimulateForever();
#endif //!CI_PIPELINE
	void SimulateBroadcastMessage(double x, double y, ble_gap_evt_adv_report_t& advReport, bool ignoreDropProb);
	void SendTerminalCommand(NodeId nodeId, const char* message, ...);
	void SendButtonPress(NodeId nodeId, u8 buttonId, u32 holdTimeDs);
	
	//### Callbacks
	//Inherited via TerminalPrintListener
	void TerminalPrintHandler(nodeEntry* currentNode, const char* message) override;

	//Inherited via CherrySimEventListener
	void CherrySimEventHandler(const char* eventType) override;
	void CherrySimBleEventHandler(nodeEntry* currentNode, simBleEvent* simBleEvent, u16 eventSize) override;

};
