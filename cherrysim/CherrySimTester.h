////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2022 M-Way Solutions GmbH
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

#include <functional>
#include <type_traits>

constexpr int MAX_TERMINAL_OUTPUT = 1024;

class NodeEntryPredicate
{
public:
    /// Construct a NodeEntryPredicate which always returns true.
    NodeEntryPredicate() = default;

    /// Construct a NodeEntryPredicate from a given predicate.
    explicit NodeEntryPredicate(std::function<bool(const NodeEntry *)> predicate_) : predicate{std::move(predicate_)}
    {
    }

    /// DEPRECATED: Allow legacy code to function.
    NodeEntryPredicate &operator=(int magicTerminalId);

public:
    static NodeEntryPredicate AllowAll();

    static NodeEntryPredicate AllowNone();

    static NodeEntryPredicate AllowNodeIndex(u32 nodeIndex);

    static NodeEntryPredicate AllowTerminalId(TerminalId terminalId);

    static NodeEntryPredicate AllowNodeId(NodeId nodeId);

    static NodeEntryPredicate AllowNodeId(NodeId nodeId, NetworkId networkId);

public:
    bool operator()(const NodeEntry *nodeEntry) const
    {
        return predicate(nodeEntry);
    }

private:
    static bool TheAllowAllPredicate(const NodeEntry *) noexcept
    {
        return true;
    }

    static bool TheAllowNonePredicate(const NodeEntry *) noexcept
    {
        return false;
    }

private:
    std::function<bool(const NodeEntry *)> predicate = &TheAllowAllPredicate;
};

struct CherrySimTesterConfig
{
    bool verbose;
    NodeEntryPredicate terminalFilter; //Set to -1 for no output, 0 for all nodes or any other number for the terminal of that node
};

class SimulationMessage
{
private:
    NodeEntryPredicate predicate;
    std::string        messagePart;
    std::string        messageComplete = "";
    bool               found           = false;
    // if false e.g. SimulateUntilMessagesReceived will throw an Exception should the message be received
    bool               shouldOccur = true;

    bool Matches(const std::string &message);
    void MakeFound(const std::string &messageComplete);
    bool MatchesRegex(const std::string &message);

public:
    SimulationMessage(TerminalId, const std::string& messagePart, bool shouldOccur=true);
    SimulationMessage(NodeEntryPredicate predicate, const std::string& messagePart, bool shouldOccur=true);
    bool CheckAndSet(const std::string &message, bool useRegex);
    bool IsFound() const;
    bool ShouldOccur() const { return shouldOccur; }
    const std::string& GetCompleteMessage() const;
    void PrintState() const;

    bool AppliesToNodeEntry(const NodeEntry * nodeEntry) const
    {
        return predicate(nodeEntry);
    }
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
    bool unwantedMessageOccured = false;

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
    void SimulateUntilMessageReceived(int timeoutMs, TerminalId, const char* messagePart, ...);
    void SimulateUntilMessageReceivedWithCallback(int timeoutMs, TerminalId terminalId, std::function<void()> executePerStep, const char* messagePart, ...);
    //Simulates until all of the messages in the messages vector are received. N copies of the same message must be received N times.
    void SimulateUntilMessagesReceived(int timeoutMs, std::vector<SimulationMessage>& messages, std::function<void()> executePerStep = std::function<void()>());
    void SimulateUntilRegexMessageReceived(int timeoutMs, TerminalId terminalId, const char* messagePart, ...);
    void SimulateUntilRegexMessagesReceived(int timeoutMs, std::vector<SimulationMessage>& messages);
    void SimulateUntilBleEventReceived(int timeoutMs, NodeId nodeId, u16 eventId, const u8* eventDataPart, u16 eventDataPartLength);
#ifndef CI_PIPELINE
    void SimulateForever();
#endif //!CI_PIPELINE
    void SimulateBroadcastMessage(double x, double y, ble_gap_evt_adv_report_t& advReport, bool ignoreDropProb);

public:
    /// Sends a terminal command to the node with the specified terminal id. Throws if no node is found or the terminal
    /// id is used by multiple nodes.
    ///
    /// This keeps the legacy behaviour of SendTerminalCommand, as the `terminalId` is the node id which was
    /// assigned to a node during initialization (`terminalId == index + 1`).
    void SendTerminalCommand(TerminalId terminalId, const char *message, ...);

    /// Sends a terminal command to the node with the specified node id. Throws if the node is not found or the node
    /// id is used by multiple nodes.
    ///
    /// The `nodeId` is checked against the _actual_ id used by the node at the time this method is called, i.e. it
    /// _respects_ changes in the node id by e.g. enrollment.
    void SendTerminalCommandToNodeId(NodeId nodeId, const char *message, ...);

    /// Sends a terminal command to all nodes.
    void SendTerminalCommandToAllNodes(const char *message, ...);

private:
    void DoSendTerminalCommand(const NodeEntry &nodeEntry, const std::string &originalCommand) const;

public:
    void SendButtonPress(TerminalId terminalId, u8 buttonId, u32 holdTimeDs);

    //### Callbacks
    //Inherited via TerminalPrintListener
    void TerminalPrintHandler(NodeEntry* currentNode, const char* message) override;

    //Inherited via CherrySimEventListener
    void CherrySimEventHandler(const char* eventType) override;
    void CherrySimBleEventHandler(NodeEntry* currentNode, simBleEvent* simBleEvent, u16 eventSize) override;

private:
    static bool verboseTestsByDefault;

public:
    /// Sets the default value of the verbose flag in the `CherrySimTesterConfig` created by
    /// `CherrySimTester::CreateDefaultTesterConfiguration` to true, which causes all tests
    /// that are executed and don't explicitly override the flag to be verbose (i.e. have
    /// the terminal output from the simulated nodes printed).
    static void EnableVerboseTestsByDefault();

    //Returns true if the tester has terminal output enabled.
    bool IsVerbose() const;

    //Use this to disable/ enable terminal output during a test
    void SetVerbose(bool verbose);

    //### Helpers for Simulating updates
    static void DfuStartFromTerminalCommandFile(CherrySimTester& tester, std::string file, TerminalId targetTerminalId);
    static void DfuDataFromTerminalCommandFile(CherrySimTester& tester, std::string file, TerminalId targetTerminalId);
    static void DfuAllTransmittedFromTerminalCommandFile(CherrySimTester& tester, std::string file, TerminalId targetTerminalId);
    static void DfuApplyFromTerminalCommandFile(CherrySimTester& tester, std::string file, TerminalId targetTerminalId);

};
