////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2021 M-Way Solutions GmbH
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

    bool Matches(const std::string &message);
    void MakeFound(const std::string &messageComplete);
    bool MatchesRegex(const std::string &message);

public:
    SimulationMessage(NodeId nodeId, const std::string& messagePart);
    bool CheckAndSet(const std::string &message, bool useRegex);
    bool IsFound() const;
    const std::string& GetCompleteMessage() const;
    NodeId GetNodeId() const;
};

class CherrySimTester : public TerminalPrintListener, public CherrySimEventListener
{
public:
    CherrySim* sim = nullptr;

    //Used for awaiting specific terminal messages
    std::vector<SimulationMessage>* awaitedTerminalOutputs = nullptr;
    bool useRegex = false;
    u16 awaitedMessagePointer = 0;
    bool awaitedMessagesFound = false;

    //Used for awaiting specific ble events
    NodeId awaitedBleEventNodeId = 0;
    u16 awaitedBleEventEventId = 0;
    std::array<u8, 1000> awaitedBleEventDataPart = {};
    u16 awaitedBleEventDataPartLength = 0;
    bool awaitedBleEventFound = false;

    bool appendCrcToMessages = true;

private:
    std::array<char, MAX_TERMINAL_OUTPUT> awaitedMessageResult = { '\0' };
    CherrySimTesterConfig config = {};
    SimConfiguration simConfig = {};
    void _SimulateUntilMessageReceived(int timeoutMs, std::function<void()> executePerStep = std::function<void()>());
    bool started = false;

public:
    CherrySimTester(CherrySimTesterConfig testerConfig, SimConfiguration simConfig);
    
    //Deleting these as they are currently unused and implmenting
    //them correctly would create unnecessary overhead for now.
    CherrySimTester(const CherrySimTester& other) = delete;
    CherrySimTester(CherrySimTester&& other);
    CherrySimTester& operator=(const CherrySimTester& other) = delete;
    CherrySimTester& operator=(CherrySimTester&& other) = delete;

    ~CherrySimTester();

    static CherrySimTesterConfig CreateDefaultTesterConfiguration();
    static SimConfiguration CreateDefaultSimConfiguration();

    //### This boots up all nodes after they were initialized and flashed
    void Start();

    //### Simulation methods
    void SimulateUntilClusteringDone(int timeoutMs, std::function<void()> executePerStep = std::function<void()>());
    void SimulateUntilClusteringDoneWithDifferentNetworkIds(int timeoutMs);

    void SimulateUntilClusteringDoneWithExpectedNumberOfClusters(int timeoutMs, u32 clusters);
    void SimulateGivenNumberOfSteps(int steps);
    void SimulateForGivenTime(int numMilliseconds);
    void SimulateUntilMessageReceived(int timeoutMs, NodeId nodeId, const char* messagePart, ...);
    void SimulateUntilMessageReceivedWithCallback(int timeoutMs, NodeId nodeId, std::function<void()> executePerStep, const char* messagePart, ...);
    //Simulates until all of the messages in the messages vector are received. N copies of the same message must be received N times.
    void SimulateUntilMessagesReceived(int timeoutMs, std::vector<SimulationMessage>& messages, std::function<void()> executePerStep = std::function<void()>());
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
    void TerminalPrintHandler(NodeEntry* currentNode, const char* message) override;

    //Inherited via CherrySimEventListener
    void CherrySimEventHandler(const char* eventType) override;
    void CherrySimBleEventHandler(NodeEntry* currentNode, simBleEvent* simBleEvent, u16 eventSize) override;

};
