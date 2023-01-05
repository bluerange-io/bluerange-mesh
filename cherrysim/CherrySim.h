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

/*
This is the main class of CherrySim, which gives you a runtime environment to run multiple
nodes simultaniously and simulate their interaction. The SystemTest.h file must be force included
into all files in order to rewrite macros and other calls.
 */

#pragma once

#ifdef SIM_ENABLED
#include <vector>
#include <FmTypes.h>
#include <GlobalState.h>
#include <FruitySimServer.h>
#include <Terminal.h>
#include <LedWrapper.h>
#include <CherrySimTypes.h>
#include <map>
#include <chrono>
#include <string>

struct ReplayRecordEntry
{
    u32 index = 0;
    u32 time = 0;
    std::string command = "";

    bool operator<(const ReplayRecordEntry &other) const
    {
        return time < other.time;
    }
};

struct FeaturesetPointers
{
    FeatureSetGroup(*getFeaturesetGroupPtr)(void) = nullptr;
    void(*setBoardConfigurationPtr)(BoardConfiguration* config) = nullptr;
    void(*setFeaturesetConfigurationPtr)(ModuleConfiguration* config, void* module) = nullptr;
    void(*setFeaturesetConfigurationVendorPtr)(VendorModuleConfiguration* config, void* module) = nullptr;
    u32(*initializeModulesPtr)(bool createModule) = nullptr;
    DeviceType(*getDeviceTypePtr)(void) = nullptr;
    Chipset(*getChipsetPtr)(void) = nullptr;
    u32(*getWatchdogTimeout)(void) = nullptr;
    u32(*getWatchdogTimeoutSafeBoot)(void) = nullptr;
    LicenseState(*getLicenseState)(void) = nullptr;
    u32 featuresetOrder = 0;
    const char* featuresetName = nullptr;
};

class SocketTerm;

class CherrySim
{
    friend class NodeIndexSetter;

    std::vector<char> nodeEntryBuffer; // As std::vector calls the copy constructor of it's type and NodeEntry has no copy constructor we have to provide the memory like this.

public:
    int globalBreakCounter = 0; //Can be used to increment globally everywhere in sim and break on a specific count
    bool shouldRestartSim = false;
    bool blockConnections = false; //Can be set to true to stop packets from being sent
    volatile bool receivedDataFromMeshGw = false;
    SimConfiguration simConfig; //The current configuration for the simulator
    SimulatorState simState; //The current state of the simulator
    NodeEntry* currentNode = nullptr; //A pointer to the current node under simulation
    NodeEntry* nodes = nullptr; //A pointer that points to the memory that holds the complete state of all nodes
    std::string logAccumulator;

    /// The propagation constant or path loss exponent is a dimensionless quantity
    float propagationConstant = 2.0;
    /// Mean of the RSSI noise
    float rssiNoiseMean = 0.0f;
    /// Standard deviation of the RSSI noise, over 99% of generated values lie within 3-times this value.
    float rssiNoiseStddev = 5.0f;

    bool renderLess = false;

    CherrySimEventListener* simEventListener = nullptr;

    int flashToFileWriteCycle = 0;
    static constexpr int flashToFileWriteInterval = 128; // Will write flash to file every flashToFileWriteInterval's simulation step.

    void ErasePage(u32 pageAddress);

    //Can be used to inject a single record configuration into the flash of the current node before booting it
    void WriteRecordToFlash(u16 recordId, u8* data, u16 dataLength);

    std::map<std::string, FeaturesetPointers> featuresetPointers;

    std::queue<ReplayRecordEntry> replayRecordEntries;

    NodeEntry* GetNodeEntryBySerialNumber(u32 serialNumber);

    static std::string LoadFileContents(const char* path);
    static std::string ExtractReplayToken(const std::string &fileContents, const std::string &startToken, const std::string &endToken);
    static std::queue<ReplayRecordEntry> ExtractReplayRecord(const std::string &fileContents);
    static std::string ExtractAndCleanReplayToken(const std::string& fileContents, const std::string& startToken, const std::string& endToken);
    static SimConfiguration ExtractSimConfigurationFromReplayRecord(const std::string &fileContents);
    static void CheckVersionFromReplayRecord(const std::string &fileContents);
    // Exception can be checked for occurrence. Keep in mind that exception are stored:
    // * only for one simulation step and cleared at the start of the next simulation step
    // * only stored when the exception type is disabled
    bool CheckExceptionWasThrown(std::type_index index);
    //Method called to store exceptions when exception type
    //is disabled
    void LogThrownException(std::type_index index);

private:
    //set to store exception when exception type is disabled
    std::set<std::type_index>loggedExceptions;
TESTER_PUBLIC:
    void SetNode(u32 i); //Should not be called publicly. Use the NodeIndexSetter instead.
    u32 totalNodes = 0;
    u32 assetNodes = 0;
    TerminalPrintListener* terminalPrintListener = nullptr;
    FruitySimServer* webserver = nullptr;
    SocketTerm* socketTerm = nullptr;

    std::chrono::time_point<std::chrono::steady_clock> lastTick;

    //This keeps a list of functions that will be called on every simulation step
    //It is useful for mocks that do not have a sense of time themselves but need one
    struct LambdaWithHandle {
        NodeEntry* owningNode;
        std::function<void(void)> lambda;

        void operator()() const { lambda(); }
    };
    std::vector<LambdaWithHandle> simStepCallbacks;

    std::map<std::string, MoveAnimation> loadedMoveAnimations;
    bool IsValidMoveAnimationJson(const nlohmann::json &json) const;
    MoveAnimation& AnimationGet(const std::string &name);
    void AnimationCreate(const std::string &name);
    bool AnimationExists(const std::string &name) const;
    void AnimationRemove(const std::string &name);
    void AnimationSetDefaultType(const std::string&name, MoveAnimationType type);
    void AnimationAddKeypoint(const std::string &name, float x, float y, float z, float duration);
    void AnimationAddKeypoint(const std::string &name, float x, float y, float z, float duration, MoveAnimationType type);
    void AnimationSetLooped(const std::string& name, bool looped);
    bool AnimationIsRunning(u32 serialNumber);
    std::string AnimationGetName(u32 serialNumber);
    void AnimationStart(u32 serialNumber, const std::string& name);
    void AnimationStop(u32 serialNumber);
    void AnimationShake(u32 serialNumber);
    bool AnimationLoadJsonFromPath(const char* path);

    void StoreFlashToFile();
    void LoadFlashFromFile();
    void PrepareSimulatedFeatureSets();
    void QueueInterrupts();

    i8 FindFloorNumber(float zNorm);

#ifdef GITHUB_RELEASE
    //Used to redirect featuresets on github releases
    bool IsRedirectedFeatureset(const std::string& featureset);
    std::string RedirectFeatureset(const std::string& oldFeatureset);
#endif

public:
    //#### Simulation Control
    explicit CherrySim(const SimConfiguration &simConfig);
    ~CherrySim();

    void SetCherrySimEventListener(CherrySimEventListener* listener); // Register Listener for Simulator events

    void SetFeaturesets();

    void Init(); //Creates and flashes all nodes
    void SimulateStepForAllNodes(); //Simulates on timestep for all nodes
    void QuitSimulation();

    //#### Terminal
    #ifdef TERMINAL_ENABLED
    TerminalCommandHandlerReturnType TerminalCommandHandler(const std::vector<std::string>& commandArgs);
    #endif // Inherited via TerminalCommandListener
    void RegisterTerminalPrintListener(TerminalPrintListener* callback); // Register a class that will be notified when sth. is printed to the Terminal
    void TerminalPrintHandler(const char* message); //Called for all simulator output

    //#### Node Lifecycle
    u32 GetTotalNodes(bool countAgain = false) const; // returns number of all nodes i.e our nodes, vendor nodes and asset nodes
    u32 GetAssetNodes(bool countAgain = false) const; //iterates over all the nodes and calculate the node with device type Asset
    void InitNode(u32 i); // Creates a node with default settings (like manufacturing the hardware)
    void WriteSerialNumberToUicr(const char * serialNumber, u32 i); // Updates serial number and serial index in flash
    void WriteSerialNumberToUicr(u32 serialNumberIndex, u32 i); // Updates serial number and serial index in flash
    void EraseLicense(u32 i); // Removes license from flash
    void GenerateLicense(u32 i); // Generates license using Serial stored in UICR
    void FlashNode(u32 i); // Flashes a node with uicr and settings
    void BootCurrentNode(); // Starts the node. ShutdownCurrentNode() must be called to clean up
    void ResetCurrentNode(RebootReason rebootReason, bool throwException = true, bool powerLoss = false); //Resets a node and boots it again (Only call this after node was booted already)
    void ShutdownCurrentNode(); //Deletes the memory allocated by the node during runtime
    static void SendUartCommand(NodeId nodeId, const u8* message, u32 messageLength);

    static int ChipsetToPageSize(Chipset chipset);
    static int ChipsetToCodeSize(Chipset chipset);
    static int ChipsetToApplicationSize(Chipset chipset);
    static int ChipsetToBootloaderAddr(Chipset chipset);

    void CheckForMultiTensorflowUsage();

    void QueueInterruptCurrentNode(u32 pin);
    void QueueAccelerationInterruptCurrentNode();

    //Adds a handler that will be executed each simulation step
    void AddSimulationStepHandler(std::function<void(void)> lambda);
    //Removes all handlers for a node that is being destroyed
    void CleanSimulationStepHandlers(NodeEntry* nodeEntry);

public:
    //This section is public so that softdevice calls from c code can access the functions

    //Import devices from json or generate a random scenario
    bool IsUsableDeviceEntry(nlohmann::json deviceJson);
    nlohmann::json& NormalizeDeviceEntry(nlohmann::json& device);
    void ImportDataFromJson();
    void ImportPositionsAndDataFromJson();
    void PositionNodesRandomly();
    void LoadPresetNodePositions();

    //Bootloader Simulation
    void SimulateFruityLoader();

    //Flash simulation
    void SimulateFlashCommit();
    void SimCommitFlashOperations();
    void SimCommitSomeFlashOperations(const uint8_t* failData, uint16_t numMaxEvents);

    //GAP Simulation
    void SimulateAdvertising();
    static ble_gap_addr_t Convert(const FruityHal::BleGapAddr* address);
    static FruityHal::BleGapAddr Convert(const ble_gap_addr_t* p_addr);
    void ConnectMasterToSlave(NodeEntry * master, NodeEntry* slave);
    u32 DisconnectSimulatorConnection(SoftdeviceConnection * connection, u32 hciReason, u32 hciReasonPartner);
    void SimulateTimeouts();

    //UART Simulation
    void SimulateUartInterrupts();

    //GATT Simulation
    void SimulateConnections();
    void SendUnreliableTxCompleteEvent(NodeEntry* node, int connHandle, u8 packetCount);
    void GenerateWrite(SoftDeviceBufferedPacket* bufferedPacket);
    void GenerateNotification(SoftDeviceBufferedPacket* bufferedPacket);

    //GPIO Simulation
    void SetSimLed(bool state);

    //Movement Simulation
    void SimulateMovement();

    //Battery usage simulation
    void SimulateBatteryUsage();

    //Service Discovery Simulation
    void StartServiceDiscovery(u16 connHandle, const ble_uuid_t &p_uuid, int discoveryTimeMs);
    void SimulateServiceDiscovery();

    // Timeslot API Simulation
    void SimulateTimeslot();

    // Connection Parameter Update Request Simulation
    void SimulateConnectionParameterUpdateRequestTimeout();

    //Other Simulation
    void SimulateTimer();
    void SimulateWatchDog();

    void SimulateInterrupts();

    //Validity Checking
    void CheckMeshingConsistency();
    ClusterSize DetermineClusterSizeAndPropagateClusterUpdates(NodeEntry* node, NodeEntry* startNode);

    //Configuration
    void SetBleStack(NodeEntry* node);

    //Statistics
    void AddPacketToStats(PacketStat* statArray, PacketStat* packet);
    void AddMessageToStats(PacketStat* statArray, u8* message, u16 messageLength);
    void PrintPacketStats(NodeId nodeId, const char* statId);

    //#### Helpers
    bool IsClusteringDone();
    bool IsClusteringDoneWithDifferentNetworkIds();    //Checks if each network Id for itself is completly clustered.
    bool IsClusteringDoneWithExpectedNumberOfClusters(u32 clusters);

    void ChooseSimulatorTerminal();

    float GetDistanceBetween(const NodeEntry * nodeA, const NodeEntry * nodeB);
    float GetReceptionRssi(const NodeEntry* sender, const NodeEntry* receiver);
    float GetReceptionRssiNoNoise(const NodeEntry* sender, const NodeEntry* receiver);

private:
    uint32_t CalculateReceptionProbabilityFromRssi(float rssi);

public:
    uint32_t CalculateReceptionProbabilityForConnection(const NodeEntry* sendingNode, const NodeEntry* receivingNode);
    uint32_t CalculateReceptionProbabilityForAdvertisement(const NodeEntry* sendingNode, const NodeEntry* receivingNode);

    bool ShouldSimIvTrigger(u32 ivMs);
    bool ShouldSimConnectionIvTrigger(u32 ivMs, SoftdeviceConnection* connection);

    SoftdeviceConnection* FindConnectionByHandle(NodeEntry* node, int connectionHandle);
    u8 GetNumSimConnections(const NodeEntry* node);

    /// Returns true if the terminal of the current node is allowed by the simulator configuration.
    bool IsSimTermOfCurrentNodeActive() const;

    /// Returns the pointer to the first node entry with the given node id. Returns nullptr if there is no node with
    /// the specified id.
    NodeEntry* FindNodeById(int id);

    /// Returns the pointer to the node entry with the given node id. Throws if multiple nodes have the same node id.
    /// Returns nullptr if there is no node with the specified id.
    NodeEntry* FindUniqueNodeById(NodeId nodeId);

    /// Returns the pointer to the node entry with the given node id and network id. Throws if multiple nodes have the
    /// same node id and network id. Returns nullptr if there is no node with the specified ids.
    NodeEntry* FindUniqueNodeById(NodeId nodeId, NetworkId networkId);

    /// Returns the pointer to the node entry with the given terminal id. Throws if multiple nodes have the same
    /// terminal id. Returns nullptr if no node with the specified terminal id was found.
    NodeEntry* FindUniqueNodeByTerminalId(unsigned terminalId);

    void FakeVersionOfCurrentNode(u32 version);

    void EnableTagForAll(const char* tag);
    void DisableTagForAll(const char* tag);

    void SetPosition(u32 nodeIndex, float x, float y, float z);
    void AddPosition(u32 nodeIndex, float x, float y, float z);
};

//Throw this in the simulator in order to quit from the simulation
struct CherrySimQuitException : public std::exception { char const * what() const noexcept override { return "CherrySimQuitException"; } };
//Thrown by a node if the node should be reset
struct NodeSystemResetException : public std::exception { char const * what() const noexcept override { return "NodeSystemResetException"; }
};

//RAII implementation for setNode function
class NodeIndexSetter
{
private:
    u32 originalIndex = 0xFFFFFFFF;

public:
    explicit NodeIndexSetter(u32 indexToSet)
    {
        if (cherrySimInstance->currentNode == nullptr)
        {
            originalIndex = 0xFFFFFFFF;
        }
        else
        {
            originalIndex = cherrySimInstance->currentNode->index;
        }

        cherrySimInstance->SetNode(indexToSet);
    }

    ~NodeIndexSetter()
    {
        cherrySimInstance->SetNode(originalIndex);
    }
};


#endif
/** @} */
