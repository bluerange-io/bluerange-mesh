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
#ifdef CHERRYSIM_TESTER_ENABLED
#include "gtest/gtest.h"
#endif
#include "CherrySimTester.h"
#include "CherrySim.h"
#include "Node.h"
#include <regex>
#include <string>
#include <cstdarg>
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

        //If we only want to execute specific tests, we can specify them here, default: *:-*_scheduled*:*_long*
        ::testing::GTEST_FLAG(filter) = "*:-*_scheduled*:*_long*";

        //Do not catch exceptions is useful for debugging (Automatically set to 1 if running on Gitlab)
        ::testing::GTEST_FLAG(catch_exceptions) = 0;

        bool quiet = false;
        if (quiet)
        {
            testing::TestEventListeners& listeners = testing::UnitTest::GetInstance()->listeners();
            listeners.Release(listeners.default_result_printer());
        }
    }
    // ###########################
    // Automated testing
    // ###########################
    bool GitLab = false;
    bool Scheduled = false;

    std::regex seedStartRegex("SeedStart=(\\w+)");
    std::regex seedIncrementRegex("SeedIncrement=(\\w+)");
    std::regex numRunsRegex("numRuns=(\\w+)");
    std::smatch matches;

    uint32_t seedOffset = 0;
    uint32_t seedIncrement = 0;
    uint32_t numRuns = 1;
    bool didError = false;
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
        else if (std::regex_search(s, matches, seedStartRegex))
        {
            seedOffset = Utility::StringToU32(matches[1].str().c_str(), &didError);
        }
        else if (std::regex_search(s, matches, seedIncrementRegex))
        {
            seedIncrement = Utility::StringToU32(matches[1].str().c_str(), &didError);
        }
        else if (std::regex_search(s, matches, numRunsRegex))
        {
            numRuns = Utility::StringToU32(matches[1].str().c_str(), &didError);
        }
    }

    if (didError)
    {
        SIMEXCEPTION(IllegalParameterException);
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
    for(uint32_t i = 0; i<numRuns && !exitCode; i++, seedOffset += seedIncrement)
    {
        std::cout << "Seed offset is now: " << seedOffset << std::endl;
        MersenneTwister::seedOffset = seedOffset;

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
    }

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

    simConfig.seed = 1;
    simConfig.mapWidthInMeters = 60;
    simConfig.mapHeightInMeters = 40;
    simConfig.mapElevationInMeters = 1;
    simConfig.simTickDurationMs = 50;
    simConfig.terminalId = 0; //Terminal must be active in order for a test to trigger on the terminal output

    simConfig.simOtherDelay = 1; // Enter 1 - 100000 to send sim_other message only each ... simulation steps, this increases the speed significantly
    simConfig.playDelay = 0; //Allows us to view the simulation slower than simulated, is added after each step
    
    simConfig.interruptProbability = UINT32_MAX / 10;

    simConfig.connectionTimeoutProbabilityPerSec = 0; //Every minute or so: UINT32_MAX * 0.00001;
    simConfig.sdBleGapAdvDataSetFailProbability = 0;  // UINT32_MAX * 0.0001; //Simulate fails on setting adv Data
    simConfig.sdBusyProbability = UINT32_MAX / 100;   // UINT32_MAX * 0.0001; //Simulates getting back busy errors from softdevice
    simConfig.simulateAsyncFlash = true;
    simConfig.asyncFlashCommitTimeProbability = UINT32_MAX / 10 * 9;

    simConfig.importFromJson = false;
    simConfig.siteJsonPath = "C:\\Users\\MariusHeil\\Desktop\\testsite.json";
    simConfig.devicesJsonPath = "C:\\Users\\MariusHeil\\Desktop\\testdevices.json";


    simConfig.defaultBleStackType = BleStackType::NRF_SD_132_ANY;

    simConfig.defaultNetworkId = 10;

    simConfig.rssiNoise = false;

    simConfig.fastLaneToSimTimeMs = 0;

    simConfig.verboseCommands = true;
    simConfig.enableSimStatistics = false;


    return simConfig;
}

CherrySimTester::CherrySimTester(CherrySimTesterConfig testerConfig, SimConfiguration simConfig)
      : config(testerConfig),
      simConfig(simConfig) 
{
    sim = new CherrySim(simConfig);
    sim->SetCherrySimEventListener(this);
    sim->Init();
    sim->RegisterTerminalPrintListener(this);
    
}

CherrySimTester::CherrySimTester(CherrySimTester && other)
    : sim                        (std::move(other.sim)),
    awaitedTerminalOutputs       (std::move(other.awaitedTerminalOutputs)),
    useRegex                     (std::move(other.useRegex)),
    awaitedMessagePointer        (std::move(other.awaitedMessagePointer)),
    awaitedMessagesFound         (std::move(other.awaitedMessagesFound)),
    awaitedBleEventNodeId        (std::move(other.awaitedBleEventNodeId)),
    awaitedBleEventEventId       (std::move(other.awaitedBleEventEventId)),
    awaitedBleEventDataPart      (std::move(other.awaitedBleEventDataPart)),
    awaitedBleEventDataPartLength(std::move(other.awaitedBleEventDataPartLength)),
    awaitedBleEventFound         (std::move(other.awaitedBleEventFound)),
    appendCrcToMessages          (std::move(other.appendCrcToMessages)),
    awaitedMessageResult         (std::move(other.awaitedMessageResult)),
    config                       (std::move(other.config)),
    simConfig                    (std::move(other.simConfig)),
    started                      (std::move(other.started))
{
    other.sim = nullptr;
}

CherrySimTester::~CherrySimTester()
{
    delete sim;
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
    for (u32 i = 0; i < sim->GetTotalNodes(); i++) {
#ifdef GITHUB_RELEASE
        sim->nodes[i].nodeConfiguration = sim->RedirectFeatureset(sim->nodes[i].nodeConfiguration);
#endif
        NodeIndexSetter setter(i);
        sim->BootCurrentNode();
    }

    started = true;
}

void CherrySimTester::SimulateUntilClusteringDone(int timeoutMs, std::function<void()> executePerStep)
{
    if (timeoutMs == 0) SIMEXCEPTION(ZeroTimeoutNotSupportedException);
    int startTimeMs = sim->simState.simTimeMs;

    while (!sim->IsClusteringDone()) {
        if (executePerStep)
        {
            executePerStep();
        }

        sim->SimulateStepForAllNodes();

        //Watch if a timeout occurs
        if (startTimeMs + timeoutMs < (i32)sim->simState.simTimeMs) {
            SIMEXCEPTION(TimeoutException); //Timeout waiting for clustering
        }
    }
}

void CherrySimTester::SimulateUntilClusteringDoneWithDifferentNetworkIds(int timeoutMs)
{
    if (timeoutMs == 0) SIMEXCEPTION(ZeroTimeoutNotSupportedException);
    int startTimeMs = sim->simState.simTimeMs;

    while (!sim->IsClusteringDoneWithDifferentNetworkIds()) {
        sim->SimulateStepForAllNodes();

        //Watch if a timeout occurs
        if (startTimeMs + timeoutMs < (i32)sim->simState.simTimeMs) {
            SIMEXCEPTION(TimeoutException); //Timeout waiting for clustering
        }
    }
}

void CherrySimTester::SimulateBroadcastMessage(double x, double y, ble_gap_evt_adv_report_t& advReport, bool ignoreDropProb)
{
    printf("Simulating broadcast message" EOL);

    sim->currentNode->x = (float)x;
    sim->currentNode->y = (float)y;
    u32 numNoneAssetNodes = sim->GetTotalNodes() - sim->GetAssetNodes();
    for (u32 i = 0; i < numNoneAssetNodes; i++) {
        //If the other node is scanning
        if (sim->nodes[i].state.scanningActive) {
            //If the random value hits the probability, the event is sent
            uint32_t probability = sim->CalculateReceptionProbability(sim->currentNode, &(sim->nodes[i]));
            if (PSRNG(probability) || ignoreDropProb) {
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
void CherrySimTester::SimulateUntilClusteringDoneWithExpectedNumberOfClusters(int timeoutMs, u32 clusters)
{
    if (timeoutMs == 0) SIMEXCEPTION(ZeroTimeoutNotSupportedException);
    int startTimeMs = sim->simState.simTimeMs;

    while (!sim->IsClusteringDoneWithExpectedNumberOfClusters(clusters)) {
        sim->SimulateStepForAllNodes();

        //Watch if a timeout occurs
        if (startTimeMs + timeoutMs < (i32)sim->simState.simTimeMs) {
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
    if (timeoutMs == 0) SIMEXCEPTION(ZeroTimeoutNotSupportedException);
    char buffer[2048];
    va_list aptr;
    va_start(aptr, messagePart);
    vsnprintf(buffer, 2048, messagePart, aptr);
    va_end(aptr);
    std::vector<SimulationMessage> messages;
    messages.push_back(SimulationMessage(nodeId, buffer));

    SimulateUntilMessagesReceived(timeoutMs, messages);
}

void CherrySimTester::SimulateUntilMessageReceivedWithCallback(int timeoutMs, NodeId nodeId, std::function<void()> executePerStep, const char* messagePart, ...)
{
    if (timeoutMs == 0) SIMEXCEPTION(ZeroTimeoutNotSupportedException);
    char buffer[2048];
    va_list aptr;
    va_start(aptr, messagePart);
    vsnprintf(buffer, 2048, messagePart, aptr);
    va_end(aptr);
    std::vector<SimulationMessage> messages;
    messages.push_back(SimulationMessage(nodeId, buffer));

    SimulateUntilMessagesReceived(timeoutMs, messages, executePerStep);
}

void CherrySimTester::SimulateUntilMessagesReceived(int timeoutMs, std::vector<SimulationMessage>& messages, std::function<void()> executePerStep)
{
    if (timeoutMs == 0) SIMEXCEPTION(ZeroTimeoutNotSupportedException);
    useRegex = false;
    awaitedTerminalOutputs = &messages;

    _SimulateUntilMessageReceived(timeoutMs, executePerStep);
}

void CherrySimTester::SimulateUntilRegexMessageReceived(int timeoutMs, NodeId nodeId, const char * messagePart, ...)
{
    if (timeoutMs == 0) SIMEXCEPTION(ZeroTimeoutNotSupportedException);
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
    if (timeoutMs == 0) SIMEXCEPTION(ZeroTimeoutNotSupportedException);
    useRegex = true;
    awaitedTerminalOutputs = &messages;

    _SimulateUntilMessageReceived(timeoutMs);
}

void CherrySimTester::_SimulateUntilMessageReceived(int timeoutMs, std::function<void()> executePerStep)
{
    if (timeoutMs == 0) SIMEXCEPTION(ZeroTimeoutNotSupportedException);
    int startTimeMs = sim->simState.simTimeMs;
    awaitedMessagesFound = false;

    while (!awaitedMessagesFound) {
        if (executePerStep)
        {
            executePerStep();
        }

        sim->SimulateStepForAllNodes();

        //Watch if a timeout occurs
        if (startTimeMs + timeoutMs < (i32)sim->simState.simTimeMs) {
            awaitedTerminalOutputs = nullptr;
            SIMEXCEPTION(TimeoutException); //Timeout waiting for message
        }
    }
    awaitedTerminalOutputs = nullptr;
}

//Simulates until an event with a specific eventId is received that contains the binary data if given (binary data can be a part of the event)
void CherrySimTester::SimulateUntilBleEventReceived(int timeoutMs, NodeId nodeId, u16 eventId, const u8* eventDataPart, u16 eventDataPartLength)
{
    if (timeoutMs == 0) SIMEXCEPTION(ZeroTimeoutNotSupportedException);
    int startTimeMs = sim->simState.simTimeMs;

    awaitedBleEventNodeId = nodeId;
    awaitedBleEventEventId = eventId;
    awaitedBleEventDataPartLength = eventDataPartLength;
    CheckedMemcpy(awaitedBleEventDataPart.data(), eventDataPart, eventDataPartLength);
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
#ifndef CI_PIPELINE
void CherrySimTester::SimulateForever()
{
    while (true) {
        sim->SimulateStepForAllNodes();
    }
}
#endif //!CI_PIPELINE

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

    const std::string originalCommand = buffer;
    const u32 crc = Utility::CalculateCrc32String(originalCommand.c_str());
    const std::string crcCommand = originalCommand + std::string(" CRC: ") + std::to_string(crc);
    if (nodeId == 0) {
        for (u32 i = 0; i < sim->GetTotalNodes(); i++) {
            NodeIndexSetter setter(i);
            if (!GS->terminal.terminalIsInitialized) {
                //you have not activated the terminal of that node either through the config or through the sim config
                SIMEXCEPTION(IllegalStateException); //Terminal of node is not active, cannot send message
            }
            std::string commandToSend = originalCommand;
            if (GS->terminal.IsCrcChecksEnabled() && originalCommand.find(" CRC: ") == std::string::npos && appendCrcToMessages)
            {
                commandToSend = crcCommand;
            }
            if (config.verbose) {
                printf("NODE %d TERM_IN: %s" EOL, sim->currentNode->id, commandToSend.c_str());
            }
            GS->terminal.PutIntoTerminalCommandQueue(commandToSend, false);
        }
    } else if (nodeId > 0 && nodeId < sim->GetTotalNodes() + 1) {
        NodeIndexSetter setter(nodeId - 1);
        if (!GS->terminal.terminalIsInitialized) {
            //you have not activated the terminal of that node either through the config or through the sim config
            SIMEXCEPTION(IllegalStateException); //Terminal of node is not active, cannot send message
        }
        std::string commandToSend = originalCommand;
        if (GS->terminal.IsCrcChecksEnabled() && originalCommand.find(" CRC: ") == std::string::npos && appendCrcToMessages)
        {
            commandToSend = crcCommand;
        }
        if (config.verbose) {
            printf("NODE %d TERM_IN: %s" EOL, sim->currentNode->id, commandToSend.c_str());
        }
        GS->terminal.PutIntoTerminalCommandQueue(commandToSend, false);
    } else {
        SIMEXCEPTION(IllegalStateException); //Wrong nodeId given for SendTerminalCommand
    }
}

void CherrySimTester::SendButtonPress(NodeId nodeId, u8 buttonId, u32 holdTimeDs)
{
    NodeIndexSetter setter(nodeId);
    if (buttonId == 1) {
        GS->button1HoldTimeDs = holdTimeDs;
    }
    else {
        SIMEXCEPTION(IllegalStateException); //Not implemented
    }
}


//########################### Callbacks ###############################

void CherrySimTester::TerminalPrintHandler(NodeEntry* currentNode, const char* message)
{
    //Send to console
    if (config.verbose && (config.terminalFilter == 0 || config.terminalFilter == currentNode->id)) {
        // Important: The check _must_ succeed if both are 0, otherwise the
        // configuration will not be printed in (e.g.) the System Test pipeline.
        if (sim->simConfig.fastLaneToSimTimeMs <= sim->simState.simTimeMs) {
            printf("%s", message);
        }
    }

    //If we are not waiting for some specific terminal output, return
    if (awaitedTerminalOutputs == nullptr || awaitedMessagesFound) return;

    //Concatenate all output into one message until an end of line is received
    u16 messageLength = (u16)strlen(message);
    CheckedMemcpy(awaitedMessageResult.data() + awaitedMessagePointer, message, messageLength);
    awaitedMessagePointer += messageLength;

    if (awaitedMessageResult[awaitedMessagePointer - 1] == '\n') {
        awaitedMessageResult[awaitedMessagePointer - 1] = '\0';
        std::vector<SimulationMessage>& awaited = *this->awaitedTerminalOutputs;
        for (unsigned int i = 0; i < awaited.size(); i++) {
            if (!awaited[i].IsFound()) {
                if (sim->currentNode->id == awaited[i].GetNodeId())
                {
                    if (awaited[i].CheckAndSet(awaitedMessageResult.data(), useRegex))
                    {
                        break; //A received message should validate only one awaited message.
                    }
                }
            }
        }
        
        awaitedMessagesFound = std::all_of(awaited.begin(), awaited.end(), [](const SimulationMessage& sm) {return sm.IsFound(); });
        
        awaitedMessagePointer = 0;
    }
}

void CherrySimTester::CherrySimBleEventHandler(NodeEntry* currentNode, simBleEvent* simBleEvent, u16 eventSize)
{
    if (
        (awaitedBleEventNodeId == 0 || currentNode->gs.node.configuration.nodeId == awaitedBleEventNodeId)
        && awaitedBleEventEventId != 0
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

bool SimulationMessage::CheckAndSet(const std::string & message, bool useRegex)
{
    if (found) {
        SIMEXCEPTION(IllegalStateException); //The message was already found!
    }

    if (
        (useRegex && MatchesRegex(message)) || 
        (!useRegex && Matches(message))) {
        MakeFound(message);
        return true;
    }
    else {
        return false;
    }
}

bool SimulationMessage::IsFound() const
{
    return found;
}

const std::string& SimulationMessage::GetCompleteMessage() const
{
    if (!found) {
        SIMEXCEPTION(IllegalStateException); //Message was not found yet!
    }
    return messageComplete;
}

NodeId SimulationMessage::GetNodeId() const
{
    return nodeId;
}

bool SimulationMessage::Matches(const std::string & message)
{
    return message.find(messagePart) != std::string::npos;
}

void SimulationMessage::MakeFound(const std::string & messageComplete)
{
    this->messageComplete = messageComplete;
    size_t crcLoc = this->messageComplete.find(" CRC: ");
    if (crcLoc != std::string::npos)
    {
        this->messageComplete = this->messageComplete.substr(0, crcLoc);
    }
    found = true;
}

bool SimulationMessage::MatchesRegex(const std::string & message)
{
    std::regex reg(messagePart);
    return std::regex_search(message, reg);
}
