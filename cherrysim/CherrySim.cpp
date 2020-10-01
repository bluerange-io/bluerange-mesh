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
#ifdef SIM_ENABLED
#include "PrimitiveTypes.h"
#include <CherrySim.h>
#include <CherrySimUtils.h>
#include <FruitySimServer.h>
#include <FruityHal.h>
#include <FruityMesh.h>

#include <malloc.h>
#include <algorithm>
#include <thread>
#include <sstream>
#include <fstream>
#include <iostream>
#include <string>
#include <functional>
#include <json.hpp>
#include <fstream>

extern "C"{
#include <dbscan.h>
}

#include <GlobalState.h>
#include <Node.h>
#include <MeshConnection.h>
#include <Terminal.h>
#include <AdvertisingController.h>
#include <ScanController.h>
#include <GAPController.h>
#include <IoModule.h>
#include <GATTController.h>
#include <Logger.h>
#include <LedWrapper.h>
#include <Utility.h>
#include <FmTypes.h>
#include <Config.h>
#include <Boardconfig.h>
#include <FlashStorage.h>
#ifndef GITHUB_RELEASE
#include <ClcComm.h>
#include "VsComm.h"
#endif //GITHUB_RELEASE
#include <ConnectionMessageTypes.h>


#include <Module.h>
#include <BeaconingModule.h>
#include <EnrollmentModule.h>
#include <IoModule.h>
#ifndef GITHUB_RELEASE
#include <ClcModule.h>
#include <ClcComm.h>
#include "ClcMock.h"
#include "AssetModule.h"
#endif //GITHUB_RELEASE
#include "ConnectionAllocator.h"

#include <regex>

#ifdef CI_PIPELINE
//Stacktrace handling on segfault
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#endif

using json = nlohmann::json;



//########################## Simulation-Step-Constants ####################################
// These values may not change while simulating a node.
//#########################################################################################

CherrySim* cherrySimInstance = nullptr; // Use this to access the simulator from C functions
NRF_UART_Type* simUartPtr = nullptr;
bool meshGwCommunication = false;

//This is normally populated by the linker script when compiling FruityMesh,
//Sadly, this must be done manually for the simulator. Include all the connection resolvers used
ConnTypeResolver connTypeResolvers[] = {
    MeshConnection::ConnTypeResolver,
    MeshAccessConnection::ConnTypeResolver
};

//################################## Simulation Control ###################################
// These functions can start / stop / reset the simulator
//#########################################################################################

struct FlashFileHeader
{
    u32 version;
    u32 sizeOfHeader;
    u32 flashSize;
    u32 amountOfNodes;
};

bool CherrySim::ShouldSimIvTrigger(u32 ivMs)
{
    return (currentNode->state.timeMs % ivMs) == 0;
}

void CherrySim::StoreFlashToFile()
{
    if (simConfig.storeFlashToFile == "") return;

    std::ofstream file(simConfig.storeFlashToFile, std::ios::binary);
    
    FlashFileHeader ffh;
    CheckedMemset(&ffh, 0, sizeof(ffh));

    ffh.version = FM_VERSION;
    ffh.sizeOfHeader = sizeof(ffh);
    ffh.flashSize = SIM_MAX_FLASH_SIZE;
    ffh.amountOfNodes = GetTotalNodes();

    file.write((const char*)&ffh, sizeof(ffh));

    for (u32 i = 0; i < GetTotalNodes(); i++)
    {
        file.write((const char*)this->nodes[i].flash, SIM_MAX_FLASH_SIZE);
    }
}

void CherrySim::LoadFlashFromFile()
{
    if (simConfig.storeFlashToFile == "") return;

    std::ifstream infile(simConfig.storeFlashToFile);

    //If file does not exist we just return
    if (!infile.good())
    {
        return;
    }

    infile.seekg(0, std::ios::end);
    size_t length = infile.tellg();
    infile.seekg(0, std::ios::beg);
    char* buffer = new char[length];
    infile.read(buffer, length);

    FlashFileHeader ffh;
    CheckedMemset(&ffh, 0, sizeof(ffh));
    ffh = *(FlashFileHeader*)(buffer);

    if (
           ffh.sizeOfHeader  != sizeof(ffh)
        || ffh.flashSize     != SIM_MAX_FLASH_SIZE
        || ffh.amountOfNodes != GetTotalNodes()
        || length            != sizeof(ffh) + SIM_MAX_FLASH_SIZE * GetTotalNodes()
        )
    {
        //Probably the correct action if this happens is to just remove the flash safe file (see simConfig.storeFlashToFile)
        //This is NOT automatically performed here as it would be rather rude to just remove it in case the user accidentally
        //launched a different version of CherrySim or another config.
        SIMEXCEPTION(CorruptOrOutdatedSavefile);
        return;
    }

    for (u32 i = 0; i < GetTotalNodes(); i++)
    {
        CheckedMemcpy(this->nodes[i].flash, buffer + SIM_MAX_FLASH_SIZE * i + sizeof(ffh), SIM_MAX_FLASH_SIZE);
    }

    delete[] buffer;
}

#define AddSimulatedFeatureSet(featureset) \
{ \
    extern FeatureSetGroup GetFeatureSetGroup_##featureset(); \
    extern void SetBoardConfiguration_##featureset(BoardConfiguration* config); \
    extern void SetFeaturesetConfiguration_##featureset(ModuleConfiguration* config, void* module); \
    extern void SetFeaturesetConfigurationVendor_##featureset(VendorModuleConfiguration* config, void* module); \
    extern u32 InitializeModules_##featureset(bool createModule); \
    extern DeviceType GetDeviceType_##featureset(); \
    extern Chipset GetChipset_##featureset(); \
    extern u32 GetWatchdogTimeout_##featureset(); \
    extern u32 GetWatchdogTimeoutSafeBoot_##featureset(); \
    FeaturesetPointers fp = {}; \
    fp.setBoardConfigurationPtr = SetBoardConfiguration_##featureset; \
    fp.getFeaturesetGroupPtr = GetFeatureSetGroup_##featureset; \
    fp.setFeaturesetConfigurationPtr = SetFeaturesetConfiguration_##featureset; \
    fp.setFeaturesetConfigurationVendorPtr = SetFeaturesetConfigurationVendor_##featureset; \
    fp.initializeModulesPtr = InitializeModules_##featureset; \
    fp.getDeviceTypePtr = GetDeviceType_##featureset; \
    fp.getChipsetPtr = GetChipset_##featureset; \
    fp.featuresetOrder = featuresetOrderCounter; \
    fp.getWatchdogTimeout = GetWatchdogTimeout_##featureset; \
    fp.getWatchdogTimeoutSafeBoot = GetWatchdogTimeoutSafeBoot_##featureset; \
    featuresetPointers.insert(std::pair<std::string, FeaturesetPointers>(std::string(#featureset), fp)); \
    featuresetOrderCounter++; \
}

void CherrySim::PrepareSimulatedFeatureSets()
{
    //NOTE: Add the featureset in order in which NodeIds will be assigned e.g 
    //if we have defined 3 nodes with sink featureset and 2 with mesh featureset then
    //NodeId 1,2,3 will have sink featureset and 4,5 with mesh featureset
    u32 featuresetOrderCounter = 0;
    AddSimulatedFeatureSet(github_nrf52);
#ifndef GITHUB_RELEASE
    AddSimulatedFeatureSet(prod_sink_nrf52);
    AddSimulatedFeatureSet(prod_mesh_nrf52);
    AddSimulatedFeatureSet(prod_mesh_usb_nrf52840);
    AddSimulatedFeatureSet(dev_vslog);
    AddSimulatedFeatureSet(prod_vs_nrf52);
    AddSimulatedFeatureSet(prod_clc_mesh_nrf52);
    AddSimulatedFeatureSet(dev);
    AddSimulatedFeatureSet(dev_sig_mesh);
    AddSimulatedFeatureSet(dev_automated_tests_master_nrf52);
    AddSimulatedFeatureSet(dev_automated_tests_slave_nrf52);
    AddSimulatedFeatureSet(prod_vs_converter_nrf52);
    AddSimulatedFeatureSet(prod_pcbridge_nrf52);
    AddSimulatedFeatureSet(prod_wm_nrf52840);
    AddSimulatedFeatureSet(prod_bp_nrf52840);
    AddSimulatedFeatureSet(prod_eink_nrf52);
    //AssetNodes will be assigned the last nodeIds
    AddSimulatedFeatureSet(prod_asset_ins_nrf52840);
    AddSimulatedFeatureSet(prod_asset_nrf52);

#endif //GITHUB_RELEASE
}
#undef AddSimulatedFeatureSet

void CherrySim::QueueInterrupts()
{
    if (currentNode->lastMovementSimTimeMs != 0 && currentNode->lastMovementSimTimeMs + 2000 > simState.simTimeMs)
    {
        QueueAccelerationInterrutCurrentNode();
    }
}

#ifdef GITHUB_RELEASE
bool CherrySim::IsRedirectedFeatureset(const std::string& featureset)
{
    return featureset == "prod_sink_nrf52" || featureset == "prod_mesh_nrf52";
}
std::string CherrySim::RedirectFeatureset(const std::string & oldFeatureset)
{
    if (IsRedirectedFeatureset(oldFeatureset))
    {
        return "github_nrf52";
    }
    return oldFeatureset;
}
#endif


#ifdef CI_PIPELINE
void SegFaultHandler(int sig)
{
    constexpr size_t stacktraceMaxSize = 128;
    void* stacktrace[stacktraceMaxSize] = {};
    const size_t stacktraceSize = backtrace(stacktrace, stacktraceMaxSize);
    printf(EOL
           "##########################" EOL
           "#                        #" EOL
           "#   SEGMENTATION FAULT   #" EOL
           "#                        #" EOL
           "##########################" EOL
           EOL);
    backtrace_symbols_fd(stacktrace, stacktraceSize, STDERR_FILENO);
    exit(1);
}
#endif

CherrySim::CherrySim(const SimConfiguration &simConfig)
    : simConfig(simConfig)
{
#ifdef CI_PIPELINE
    //Static is okay, as the seg fault handler works accross all simulations.
    static bool segfaultHandlerSet = false;
    if (!segfaultHandlerSet)
    {
        signal(SIGSEGV, SegFaultHandler);
        segfaultHandlerSet = true;
        printf("Segfault handler set!\n");
    }
#endif

    if (simConfig.replayPath != "")
    {
#if defined(CI_PIPELINE) && !defined(CHERRYSIM_TESTER_ENABLED)
        // The replay feature is prohibited on the pipeline to make sure
        // that we don't accidentally run the same stuff all the time.
        SIMEXCEPTION(IllegalStateException);
#endif
        auto replayPath = simConfig.replayPath;
        const std::string replayFileContents = LoadFileContents(simConfig.replayPath.c_str());
        CheckVersionFromReplayRecord(replayFileContents);
        replayRecordEntries = ExtractReplayRecord(replayFileContents);
        this->simConfig = ExtractSimConfigurationFromReplayRecord(replayFileContents);
        this->simConfig.replayPath = replayPath; //Overwrite the replay path so that we know that we are currently in a replay
        if (this->simConfig.storeFlashToFile != "")
        {
            // Replaying a file that required persistent flash storage is currently not supported.
            // This is because the content of the persistent flash storage is not part of the log
            // and thus we can't know with which state the simulator started.
            SIMEXCEPTION(IllegalStateException);
        }
    }
    //Set a reference that can be used from fruitymesh if necessary
    cherrySimInstance = this;
    lastTick = std::chrono::steady_clock::now();

    PrepareSimulatedFeatureSets();

    //Reset variables to default
    simState.~SimulatorState();
    new (&simState) SimulatorState();
    simState.simTimeMs = 0;
    simState.globalConnHandleCounter = 0;
}

CherrySim::~CherrySim()
{
    StoreFlashToFile();

    //Clean up up all nodes
    for (u32 i = 0; i < GetTotalNodes(); i++) {
        NodeIndexSetter setter(i);
        ShutdownCurrentNode();
    }

    for (u32 i = 0; i < GetTotalNodes(); i++)
    {
        nodes[i].~NodeEntry();
    }
    nodes = nullptr;
    currentNode = nullptr;
    nodeEntryBuffer.clear();

    if(server != nullptr) delete server;
    server = nullptr;

    if (cherrySimInstance == this) cherrySimInstance = nullptr;
}

void CherrySim::SetCherrySimEventListener(CherrySimEventListener* listener)
{
    this->simEventListener = listener;
}

//This will create the initial node configuration
void CherrySim::Init()
{
    if (simConfig.logReplayCommands)
    {
        nlohmann::json configJson = simConfig;
        std::string configurationString = "\n\n\n[!]CONFIGURATION START:[!]\n\n\n" + configJson.dump(4) + "\n\n\n[!]CONFIGURATION END[!]\n\n\n";
        TerminalPrintHandler(configurationString.c_str());

        std::string versionString = "[!]VERSION START:[!]" + std::to_string(FM_VERSION) + "[!]VERSION END[!]";
        TerminalPrintHandler(versionString.c_str());
    }

    //Generate a psuedo random number generator with a uniform distribution
    simState.rnd.SetSeed(simConfig.seed);

    //Load site and device data from a json if given
    if (simConfig.importFromJson) {
        ImportDataFromJson();
    }

    nodeEntryBuffer.resize(GetTotalNodes() * (sizeof(NodeEntry) + alignof(NodeEntry)));
    CheckedMemset(nodeEntryBuffer.data(), 0, nodeEntryBuffer.size());
    nodes = (NodeEntry*)nodeEntryBuffer.data();
    for (u32 i = 0; i < GetTotalNodes(); i++)
    {
        new (&nodes[i]) NodeEntry;
    }

    for (u32 i = 0; i < GetTotalNodes(); i++) {
        InitNode(i);
    }

    SetFeaturesets();

    GetAssetNodes();
    for (u32 i = 0; i<GetTotalNodes(); i++) {
        FlashNode(i);
    }

    LoadFlashFromFile();

    //Either use given positions from json or generate them randomly
    if (simConfig.importFromJson) {
        ImportPositionsFromJson();
    } else {
        PositionNodesRandomly();
        LoadPresetNodePositions();
    }

    server = new FruitySimServer();
}

//This will load the site data from a json and will read the device json to import all devices
void CherrySim::ImportDataFromJson()
{
    simConfig.nodeConfigName.clear();
    json siteJson;
    json devicesJson;

    if (simConfig.replayPath != "")
    {
        const std::string replayFileContents = LoadFileContents(simConfig.replayPath.c_str());
        siteJson    = nlohmann::json::parse(ExtractAndCleanReplayToken(replayFileContents, "[!]SITE START:[!]",    "[!]SITE END[!]"));
        devicesJson = nlohmann::json::parse(ExtractAndCleanReplayToken(replayFileContents, "[!]DEVICES START:[!]", "[!]DEVICES END[!]"));
    }
    else
    {
        //Load the site json
        std::ifstream siteJsonStream(simConfig.siteJsonPath);
        siteJsonStream >> siteJson;

        //Load the devices json
        std::ifstream devicesJsonStream(simConfig.devicesJsonPath);
        devicesJsonStream >> devicesJson;
    }

    if (simConfig.logReplayCommands)
    {
        const std::string siteString   = "\n\n\n[!]SITE START:[!]\n\n\n" + siteJson.dump(4) + "\n\n\n[!]SITE END[!]\n\n\n";
        TerminalPrintHandler(siteString.c_str());
        const std::string deviceString = "\n\n\n[!]DEVICES START:[!]\n\n\n" + devicesJson.dump(4) + "\n\n\n[!]DEVICES END[!]\n\n\n";
        TerminalPrintHandler(deviceString.c_str());
    }

    //Get some data from the site
    simConfig.mapWidthInMeters = siteJson["results"][0]["lengthInMeter"];
    simConfig.mapHeightInMeters = siteJson["results"][0]["heightInMeter"];

    //Get number of nodes
    for (size_t i = 0; i < devicesJson["results"].size(); i++) 
    {
        if ((devicesJson["results"][i]["platform"] == "BLENODE" ||
             devicesJson["results"][i]["platform"] == "ASSET" ||
             devicesJson["results"][i]["platform"] == "EDGEROUTER") &&
            (devicesJson["results"][i]["properties"]["onMap"] == true || devicesJson["results"][i]["properties"]["onMap"] == "true"))
        {
            bool available = (devicesJson["results"][i]["properties"].contains("cherrySimFeatureSet"));
            if (available)
            {
                auto featuresetAlreadyInserted = simConfig.nodeConfigName.find(devicesJson["results"][i]["properties"]["cherrySimFeatureSet"]);
                if (featuresetAlreadyInserted != simConfig.nodeConfigName.end())
                {
                    u8 count = featuresetAlreadyInserted->second + 1;
                    simConfig.nodeConfigName.insert_or_assign(devicesJson["results"][i]["properties"]["cherrySimFeatureSet"], count );
                }
                else
                {
                    simConfig.nodeConfigName.insert({ devicesJson["results"][i]["properties"]["cherrySimFeatureSet"], 1 });
                }
            }
            else
            {
                auto featuresetAlreadyInserted = simConfig.nodeConfigName.find((devicesJson["results"][i]["platform"] == "EDGEROUTER") ? "prod_sink_nrf52" : "prod_mesh_nrf52");
                if (featuresetAlreadyInserted != simConfig.nodeConfigName.end())
                {
                    u8 count = featuresetAlreadyInserted->second + 1;
                    simConfig.nodeConfigName.insert_or_assign((devicesJson["results"][i]["platform"] == "EDGEROUTER") ? "prod_sink_nrf52" : "prod_mesh_nrf52", count);
                }
                else
                {
                    simConfig.nodeConfigName.insert({(devicesJson["results"][i]["platform"] == "EDGEROUTER") ? "prod_sink_nrf52" : "prod_mesh_nrf52", 1 });
                }

            }
        }
    }
}

//This will read the device json and will set all the node positions from it
void CherrySim::ImportPositionsFromJson()
{
    json devicesJson;

    if (simConfig.replayPath != "")
    {
        const std::string replayFileContents = LoadFileContents(simConfig.replayPath.c_str());
        devicesJson = nlohmann::json::parse(ExtractAndCleanReplayToken(replayFileContents, "[!]DEVICES START:[!]", "[!]DEVICES END[!]"));
    }
    else
    {
        //Load the devices json
        std::ifstream devicesJsonStream(simConfig.devicesJsonPath);
        devicesJsonStream >> devicesJson;
    }

    //Get other data from our devices
    int j = 0;
    for (u32 i = 0; i < devicesJson["results"].size(); i++) 
    {
        if ((devicesJson["results"][i]["platform"] == "BLENODE" ||
             devicesJson["results"][i]["platform"] == "EDGEROUTER" ||
             devicesJson["results"][i]["platform"] == "ASSET") &&
            (devicesJson["results"][i]["properties"]["onMap"] == true || devicesJson["results"][i]["properties"]["onMap"] == "true"))
        {
            if (std::string("number") == devicesJson["results"][i]["properties"]["x"].type_name())
            {
                nodes[j].x = devicesJson["results"][i]["properties"]["x"];
                nodes[j].y = devicesJson["results"][i]["properties"]["y"];
                //The z coordinate is optional.
                auto jsonEntryZ = devicesJson["results"][i]["properties"]["z"];
                nodes[j].z = jsonEntryZ != nullptr ? (float)jsonEntryZ : 0.0f;
            }
            else if (std::string("string") == devicesJson["results"][i]["properties"]["x"].type_name())
            {
                nodes[j].x = (float)std::stod(devicesJson["results"][i]["properties"]["x"].get<std::string>(), 0);
                nodes[j].y = (float)std::stod(devicesJson["results"][i]["properties"]["y"].get<std::string>(), 0);
                //The z coordinate is optional.
                auto jsonEntryZ = devicesJson["results"][i]["properties"]["z"];
                nodes[j].z = jsonEntryZ != nullptr ? (float)std::stod(jsonEntryZ.get<std::string>(), 0) : 0;

            }
            else
                SIMEXCEPTION(NonCompatibleDataTypeException);

            j++;
        }
    }
}

void HelperPositionNodesRandomly(CherrySim &instance, std::vector<point_t> &points, u32 numberOfNodesToPlace)
{
    //Calculate the epsilon using the rssi threshold and the transmission powers
    double epsilon = pow(10, ((double)-STABLE_CONNECTION_RSSI_THRESHOLD + SIMULATOR_NODE_DEFAULT_CALIBRATED_TX + SIMULATOR_NODE_DEFAULT_DBM_TX) / 10 / instance.N);

    unsigned int minpts = 0; //a point must reach only one of another cluster to become part of its cluster

    bool retry = true;
    while (retry) {
        //printf("try\n");
        retry = false;

        //Create 2D points for all node positions
        for (u32 i = 0; i < instance.GetTotalNodes(); i++) {
            points[i].cluster_id = -1; //unclassified
            points[i].x = (double)instance.nodes[i].x * (double)instance.simConfig.mapWidthInMeters;
            points[i].y = (double)instance.nodes[i].y * (double)instance.simConfig.mapHeightInMeters;
            points[i].z = (double)instance.nodes[i].z * (double)instance.simConfig.mapElevationInMeters;
        }

        //Use dbscan algorithm to check how many clusters these nodes can generate
        dbscan(points.data(), numberOfNodesToPlace, epsilon, minpts, euclidean_dist);

        //printf("Epsilon for dbscan: %lf\n", epsilon);
        //printf("Minimum points: %u\n", minpts);
        //print_points(points, num_points);

        for (u32 i = 0; i < numberOfNodesToPlace; i++) {
            if (points[i].cluster_id != 0) {
                retry = true;
                instance.nodes[i].x = (float)instance.simState.rnd.NextU32() / (float)0xFFFFFFFF;
                instance.nodes[i].y = (float)instance.simState.rnd.NextU32() / (float)0xFFFFFFFF;
            }
        }
    }
}

//This will position all nodes randomly, by using dbscan to generate a configuration that can be clustered
void CherrySim::PositionNodesRandomly()
{
    //Set some random x and y position for all nodes
    for (u32 nodeIndex = 0; nodeIndex < GetTotalNodes(); nodeIndex++) {
        nodes[nodeIndex].x = (float)simState.rnd.NextU32() / (float)0xFFFFFFFF;
        nodes[nodeIndex].y = (float)simState.rnd.NextU32() / (float)0xFFFFFFFF;
        nodes[nodeIndex].z = 0;
    }

    u32 numNoneAssetNodes = GetTotalNodes() - GetAssetNodes();
    //Next, we must check if the configuraton can cluster
    std::vector<point_t> points = {};
    points.resize(GetTotalNodes());

    //Two passes for DBScan are required, once for none assets, once for assets.
    //This is necessary to make sure that assets are not considered as valid mesh
    //nodes to DBScan.
    HelperPositionNodesRandomly(*this, points, numNoneAssetNodes);
    HelperPositionNodesRandomly(*this, points, GetTotalNodes());
    
}


NodeEntry * CherrySim::GetNodeEntryBySerialNumber(u32 serialNumber)
{
    for (u32 i = 0; i < GetTotalNodes(); i++)
    {
        NodeIndexSetter setter(i);
        if (RamConfig->GetSerialNumberIndex() == serialNumber)
        {
            return &nodes[i];
        }
    }
    return nullptr;
}

std::string CherrySim::LoadFileContents(const char * path)
{
    std::ifstream input(path, std::ios::ate);
    if (!input)
    {
        SIMEXCEPTION(FileException);
    }
    std::vector<char> fileContentsVector;
    fileContentsVector.resize(input.tellg());
    input.seekg(0);
    input.read(fileContentsVector.data(), fileContentsVector.size());
    input.close();

    return std::string(fileContentsVector.begin(), fileContentsVector.end());
}

std::string CherrySim::ExtractReplayToken(const std::string & fileContents, const std::string & startToken, const std::string & endToken)
{
    size_t startIndex = fileContents.find(startToken);
    const size_t endIndex = fileContents.find(endToken);

    if (startIndex == std::string::npos || endIndex == std::string::npos)
    {
        //The replay file did not contain the given token
        SIMEXCEPTION(IllegalArgumentException);
    }

    startIndex += startToken.size();

    return fileContents.substr(startIndex, endIndex - startIndex);
}

std::queue<ReplayRecordEntry> CherrySim::ExtractReplayRecord(const std::string &fileContents)
{
    const std::string commandStartPattern = "[!]COMMAND EXECUTION START:[!]";
    const std::string commandEndPattern = "[!]COMMAND EXECUTION END[!]";

    std::vector<ReplayRecordEntry> replayRecordEntries;

    size_t commandStartIndex = 0;
    while ((commandStartIndex = fileContents.find(commandStartPattern, commandStartIndex)) != std::string::npos)
    {
        commandStartIndex += commandStartPattern.size();
        size_t commandEndIndex = fileContents.find(commandEndPattern, commandStartIndex);
        if (commandEndIndex == std::string::npos)
        {
            //A command start did not have a corresponding command
            //end. The file seems to be corrupted.
            SIMEXCEPTION(IllegalArgumentException);
        }

        std::string commandWithMeta = fileContents.substr(commandStartIndex, commandEndIndex - commandStartIndex);
        std::regex reg("index:(\\d+),time:(\\d+),cmd:(.+)$");
        std::smatch matches;
        if (std::regex_search(commandWithMeta, matches, reg) && matches.size() == 4)
        {
            ReplayRecordEntry entry;
            entry.index = Utility::StringToU32(matches[1].str().c_str());
            entry.time = Utility::StringToU32(matches[2].str().c_str());
            entry.command = matches[3].str();
            replayRecordEntries.push_back(entry);
        }
        else
        {
            //The command with meta was malformed.
            SIMEXCEPTION(IllegalArgumentException);
        }

        commandStartIndex = commandEndIndex + commandEndPattern.size();
    }

    //Although the vector is probably already sorted, it is probably better to
    //make sure as features like jittering might disturb the vector a little.
    std::sort(replayRecordEntries.begin(), replayRecordEntries.end());

    std::queue<ReplayRecordEntry> retVal;
    for (size_t i = 0; i < replayRecordEntries.size(); i++)
    {
        retVal.push(replayRecordEntries[i]);
    }

    return retVal;
}

std::string CherrySim::ExtractAndCleanReplayToken(const std::string& fileContents, const std::string& startToken, const std::string& endToken)
{
    // Lines are dirty. Often they don't look like this:
    // [!]CONFIGURATION START:[!]
    // but like this:
    // [36mcherry-sim_1  |[0m [!]CONFIGURATION START:[!]\r
    // This function cleans the lines so that they have the expected form.

    const std::string knownCleanContent = startToken;
    const size_t cleanStart = fileContents.find(knownCleanContent);
    const size_t dirtStart = fileContents.rfind("\n", cleanStart) + 1;
    const std::string dirt = fileContents.substr(dirtStart, cleanStart - dirtStart);

    std::string configurationString = ExtractReplayToken(fileContents, startToken, endToken);

    if (dirt.size() > 0)
    {
        size_t dirtPos = 0;
        while ((dirtPos = configurationString.find(dirt, dirtPos)) != std::string::npos)
        {
            configurationString.erase(dirtPos, dirt.size());
        }

        dirtPos = 0;
        while ((dirtPos = configurationString.find("\\r", dirtPos)) != std::string::npos)
        {
            configurationString.erase(dirtPos, 2);
        }
    }
    return configurationString;
}

SimConfiguration CherrySim::ExtractSimConfigurationFromReplayRecord(const std::string &fileContents)
{
    auto jsonString = ExtractAndCleanReplayToken(fileContents, "[!]CONFIGURATION START:[!]", "[!]CONFIGURATION END[!]");
    return nlohmann::json::parse(jsonString);
}

void CherrySim::CheckVersionFromReplayRecord(const std::string &fileContents)
{
    const std::string versionString = ExtractReplayToken(fileContents, "[!]VERSION START:[!]", "[!]VERSION END[!]");
    const u32 versionFromFile = Utility::StringToU32(versionString.c_str());
    if (versionFromFile != FM_VERSION)
    {
        //The version from the replay file does not match the version from the current repository state.
        SIMEXCEPTION(IllegalArgumentException);
    }
}

void CherrySim::LoadPresetNodePositions()
{
    if (simConfig.preDefinedPositions.size() != 0)
    {
        for (u32 nodeIndex = 0; nodeIndex < std::min(simConfig.preDefinedPositions.size(), (size_t)GetTotalNodes()); nodeIndex++)
        {
            nodes[nodeIndex].x = (float)simConfig.preDefinedPositions[nodeIndex].first;
            nodes[nodeIndex].y = (float)simConfig.preDefinedPositions[nodeIndex].second;
        }
    }
}


//This simulates a time step for all nodes
void CherrySim::SimulateStepForAllNodes()
{
    //If we are in meshGwCommunication mode, the meshGw needs to boot first.
    //This may take some time. There is no need for us to run already so we
    //wait until we received something. This has the advantage that the replay
    //log is not filled with unnecessary wait time at the beginning which
    //makes debugging a little bit more easy.
    while (meshGwCommunication && !receivedDataFromMeshGw)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (simConfig.realTime)
    {
        auto currentTime = std::chrono::steady_clock::now();
        auto diff = currentTime - lastTick;
        auto passedMs = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
        if (passedMs < simConfig.simTickDurationMs)
        {
            return;
        }
        lastTick += std::chrono::milliseconds(simConfig.simTickDurationMs);
    }

    CheckForMultiTensorflowUsage();

    //Check if the webserver has some open requests to process
    server->ProcessServerRequests();

    int64_t sumOfAllSimulatedFrames = 0;
    for (u32 i = 0; i < GetTotalNodes(); i++) {
        NodeIndexSetter setter(i);
        sumOfAllSimulatedFrames += currentNode->simulatedFrames;
    }
    const int64_t avgSimulatedFrames = sumOfAllSimulatedFrames / GetTotalNodes();

    size_t s = replayRecordEntries.size(); //Meant to be used as a break point condition.
    while ((s = replayRecordEntries.size()) > 0 && replayRecordEntries.front().time <= simState.simTimeMs)
    {
        NodeIndexSetter setter(replayRecordEntries.front().index);
        GS->terminal.PutIntoTerminalCommandQueue(replayRecordEntries.front().command, false);
        replayRecordEntries.pop();
    }

    //printf("-- %u --" EOL, simState.simTimeMs);
    for (u32 i = 0; i < GetTotalNodes(); i++) {
        NodeIndexSetter setter(i);
        bool simulateNode = true;
        if (simConfig.simulateJittering)
        {
            const int64_t frameOffset = currentNode->simulatedFrames - avgSimulatedFrames;
            // Sigmoid function, flipped on the Y-Axis.
            const double probability = 1.0 / (1 + std::exp((double)(frameOffset) * 0.1));
            if (PSRNG(probability * UINT32_MAX))
            {
                simulateNode = false;
            }
        }
        if (simulateNode)
        {
            StackBaseSetter sbs;

            currentNode->simulatedFrames++;
            SimulateMovement();
            QueueInterrupts();
            SimulateTimer();
            SimulateTimeouts();
            SimulateBroadcast();
            SimulateConnections();
            SimulateServiceDiscovery();
            SimulateUartInterrupts();
#ifndef GITHUB_RELEASE
            SimulateClcData();
#endif //GITHUB_RELEASE
            try {
                FruityHal::EventLooper();
                SimulateFlashCommit();
                SimulateBatteryUsage();
                SimulateWatchDog();
            }
            catch (const NodeSystemResetException& e) {
                //Node broke out of its current simulation and rebootet
                if (simEventListener) simEventListener->CherrySimEventHandler("NODE_RESET");
            }
        }

        globalBreakCounter++;
    }

    //Run a check on the current clustering state
    if(simConfig.enableClusteringValidityCheck) CheckMeshingConsistency();

    simState.simTimeMs += simConfig.simTickDurationMs;
    
    //Back up the flash every flashToFileWriteInterval's step.
    flashToFileWriteCycle++;
    if (flashToFileWriteCycle % flashToFileWriteInterval == 0) StoreFlashToFile();
}

void CherrySim::QuitSimulation()
{
    throw CherrySimQuitException();
}

u32 CherrySim::GetTotalNodes(bool countAgain) const
{
    u32 counter = 0;
    if (cherrySimInstance->totalNodes == 0 || countAgain) {
        for (auto it = simConfig.nodeConfigName.begin(); it != simConfig.nodeConfigName.end(); it++) {
            counter += it->second;
        }
        cherrySimInstance->totalNodes = counter;
    }
    if (cherrySimInstance->totalNodes == 0)
    {
        // Could not find any node!
        SIMEXCEPTION(IllegalStateException);
    }
    return cherrySimInstance->totalNodes;

}


u32 CherrySim::GetAssetNodes(bool countAgain) const
{
    u32 counter = 0;
    if (cherrySimInstance->assetNodes == 0 || countAgain) {
        for (u32 i = 0; i < GetTotalNodes(); i++)
        {
            NodeIndexSetter setter(i);

            if (GET_DEVICE_TYPE() == DeviceType::ASSET)
            {
                counter++;
            }
        }
        cherrySimInstance->assetNodes = counter;
    }
    return cherrySimInstance->assetNodes;
}

//################################## Terminal #############################################
// Terminal input and output for the nodes and the sim
//#########################################################################################

//Terminal functions to control the simulator (CherrySim registers its TerminalCommandHandler with FruityMesh)
TerminalCommandHandlerReturnType CherrySim::TerminalCommandHandler(const std::vector<std::string>& commandArgs)
{
    if (commandArgs.size() >= 2 && commandArgs[0] == "sim")
    {
        if (commandArgs[1] == "stat") {
            printf("---- Configurable via terminal ----\n");
            printf("Terminal (sim term): %d\n", simConfig.terminalId);
            printf("Number of non-asset Nodes (sim nodes): %u\n", GetTotalNodes() - GetAssetNodes());
            printf("Number of Asset Nodes (sim assetnodes): %u\n", GetAssetNodes());
            printf("Current Seed (sim seed): %u\n", simConfig.seed);
            printf("Map Width (sim width): %u\n", simConfig.mapWidthInMeters);
            printf("Map Height (sim height): %u\n", simConfig.mapHeightInMeters);
            printf("Map Elevation (sim elevation): %u\n", simConfig.mapElevationInMeters);
            printf("ConnectionLossProbability (sim lossprob): %f\n", simConfig.connectionTimeoutProbabilityPerSec);
            printf("Play delay (sim delay): %d\n", simConfig.playDelay);
            printf("Import Json (sim json): %u\n", simConfig.importFromJson);
            printf("Site json (sim site): %s\n", simConfig.siteJsonPath.c_str());
            printf("Devices json (sim devices): %s\n", simConfig.devicesJsonPath.c_str());
            printf("---- Other ----\n");
            printf("Simtime %u\n", simState.simTimeMs);

            sim_print_statistics();

            printf("Enter 'sim sendstat {nodeId=0}' or 'sim routestat {nodeId=0}' for packet statistics" EOL);

            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs.size() >= 3 && commandArgs[1] == "term") {
            if (commandArgs[2] == "all") {
                simConfig.terminalId = 0;
            }
            else
            {
                simConfig.terminalId = Utility::StringToI32(commandArgs[2].c_str());
            }
            printf("Switched Terminal to %d\n", simConfig.terminalId);

            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs[1] == "nodes") {
            if (commandArgs.size() < 4) return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
            
            bool didError = false;
            const u32 amountOfNodes = Utility::StringToU32(commandArgs[2].c_str(), &didError);
            if (didError) return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
            
            if (amountOfNodes > 0)
            {
                simConfig.nodeConfigName.insert_or_assign(commandArgs[3].c_str(), amountOfNodes);
            }
            else
            {
                simConfig.nodeConfigName.erase(commandArgs[3].c_str());
            }

            QuitSimulation();
            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs[1] == "seed" || commandArgs[1] == "seedr") {
            if (commandArgs.size() >= 3) {
                simConfig.seed = Utility::StringToU32(commandArgs[2].c_str());
            }
            else
            {
                simConfig.seed++;
            }

            printf("Seed set to %u\n", simConfig.seed);

            if (commandArgs[1] == "seedr") {
                QuitSimulation();
            }

            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs.size() >= 3 && commandArgs[1] == "width") {
            simConfig.mapWidthInMeters = Utility::StringToU32(commandArgs[2].c_str());
            QuitSimulation();
            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs.size() >= 3 && commandArgs[1] == "height") {
            simConfig.mapHeightInMeters = Utility::StringToU32(commandArgs[2].c_str());
            QuitSimulation();
            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs.size() >= 3 && commandArgs[1] == "elevation") {
            simConfig.mapElevationInMeters = Utility::StringToU32(commandArgs[2].c_str());
            QuitSimulation();
            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs.size() >= 3 && commandArgs[1] == "lossprob") {
            simConfig.connectionTimeoutProbabilityPerSec = atof(commandArgs[2].c_str());
            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs.size() >= 3 && commandArgs[1] == "delay") {
            simConfig.playDelay = Utility::StringToI32(commandArgs[2].c_str());
            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs.size() >= 3 && commandArgs[1] == "json") {
            simConfig.importFromJson = Utility::StringToU8(commandArgs[2].c_str());
            QuitSimulation();
            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs.size() >= 3 && commandArgs[1] == "site") {
            simConfig.siteJsonPath = commandArgs[2];
            QuitSimulation();
            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs.size() >= 3 && commandArgs[1] == "devices") {
            simConfig.devicesJsonPath = commandArgs[2];
            QuitSimulation();
            return TerminalCommandHandlerReturnType::SUCCESS;
        }

        //For testing

        else if (commandArgs[1] == "Reset") {
            QuitSimulation();
            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs[1] == "flush") {
            SimCommitFlashOperations();
            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs[1] == "flushfail") {
            u8 failData[] = { 1,1,1,1,1,1,1,1,1,1 };
            SimCommitSomeFlashOperations(failData, 10);
            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs[1] == "nodestat") {
            printf("Node advertising %d (iv %d)\n", currentNode->state.advertisingActive, currentNode->state.advertisingIntervalMs);
            printf("Node scanning %d (window %d, iv %d)\n", currentNode->state.scanningActive, currentNode->state.scanWindowMs, currentNode->state.scanIntervalMs);

            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs.size() >= 3 && commandArgs[1] == "delay") {
            simConfig.playDelay = Utility::StringToI32(commandArgs[2].c_str());
            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs[1] == "loss") {
            for (int i = 0; i < currentNode->state.configuredTotalConnectionCount; i++) {
                if (currentNode->state.connections[i].connectionActive) {
                    printf("Simulated Connection Loss for node %d to partner %d (handle %d)" EOL, currentNode->id, currentNode->state.connections[i].partner->id, currentNode->state.connections[i].connectionHandle);
                    DisconnectSimulatorConnection(&currentNode->state.connections[i], BLE_HCI_CONNECTION_TIMEOUT, BLE_HCI_CONNECTION_TIMEOUT);
                }
            }

            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs.size() >= 3 && commandArgs[1] == "rees") {
            int handle = Utility::StringToI32(commandArgs[2].c_str());
            SoftdeviceConnection *conn = nullptr;
            for (int i = 0; i < currentNode->state.configuredTotalConnectionCount; i++)
            {
                if (currentNode->state.connections[i].connectionHandle == handle)
                {
                    conn = &currentNode->state.connections[i];
                }
            }
            if (conn != nullptr)
            {
                DisconnectSimulatorConnection(conn, BLE_HCI_MEMORY_CAPACITY_EXCEEDED, BLE_HCI_MEMORY_CAPACITY_EXCEEDED);
                blockConnections = false;
            }
            else
            {
                printf("Connection with that handle not available" EOL);
            }

            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs[1] == "blockconn") {
            blockConnections = !blockConnections;
            printf("Block connections is now %u" EOL, blockConnections);

            return TerminalCommandHandlerReturnType::SUCCESS;
        }

        //For statistics
        else if (commandArgs[1] == "sendstat") {
            //Print statistics about all packets generated by a node
            NodeId nodeId = commandArgs.size() >= 3 ? Utility::StringToU16(commandArgs[2].c_str()) : 0;
            PrintPacketStats(nodeId, "SENT");
            return TerminalCommandHandlerReturnType::SUCCESS;
        }
        else if (commandArgs[1] == "routestat") {
            //Print statistics about all packet routed by a node
            NodeId nodeId = commandArgs.size() >= 3 ? Utility::StringToU16(commandArgs[2].c_str()) : 0;
            PrintPacketStats(nodeId, "ROUTED");
            return TerminalCommandHandlerReturnType::SUCCESS;
        }

        else if (commandArgs[1] == "animation")
        {
            if(commandArgs.size() >= 4)
            {
                const std::string& animationSubCommand = commandArgs[2];
                if (   animationSubCommand == "create"
                    || animationSubCommand == "remove"
                    || animationSubCommand == "exists"
                    || animationSubCommand == "set_default_type"
                    || animationSubCommand == "add_keypoint"
                    || animationSubCommand == "set_looped")
                {
                    //These have in common that commandArgs[3] is the name of the animation
                    const std::string& animationName = commandArgs[3];
                    if (animationName.size() == 0)
                    {
                        return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                    }
                    if (animationSubCommand == "create")
                    {
                        // sim animation create my_anim_name
                        if (AnimationExists(animationName))
                        {
                            return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                        }
                        else
                        {
                            AnimationCreate(animationName);
                            return TerminalCommandHandlerReturnType::SUCCESS;
                        }
                    }
                    else if (animationSubCommand == "remove")
                    {
                        // sim animation remove my_anim_name
                        if (!AnimationExists(animationName))
                        {
                            return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                        }
                        else
                        {
                            AnimationRemove(animationName);
                            return TerminalCommandHandlerReturnType::SUCCESS;
                        }
                    }
                    else if (animationSubCommand == "exists")
                    {
                        //sim animation exists my_anim_name
                        NodeIndexSetter logjsonSetter(0);
                        if (AnimationExists(animationName))
                        {
                            logjson("SIM", "{\"type\":\"sim_animation_exists\",\"name\":\"%s\",\"exists\":true}" SEP, animationName.c_str());
                        }
                        else
                        {
                            logjson("SIM", "{\"type\":\"sim_animation_exists\",\"name\":\"%s\",\"exists\":false}" SEP, animationName.c_str());
                        }
                        return TerminalCommandHandlerReturnType::SUCCESS;
                    }
                    else if (animationSubCommand == "set_default_type")
                    {
                        //sim animation set_default_type my_anim_name 1
                        if (commandArgs.size() >= 5)
                        {
                            bool didError = false;
                            const MoveAnimationType animationType = (MoveAnimationType)Utility::StringToU32(commandArgs[4].c_str(), &didError);
                            if (didError)
                            {
                                return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                            }

                            if (!AnimationExists(animationName))
                            {
                                return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                            }
                            else
                            {
                                AnimationSetDefaultType(animationName, animationType);
                                return TerminalCommandHandlerReturnType::SUCCESS;
                            }
                        }
                        else
                        {
                            return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
                        }
                    }
                    else if (animationSubCommand == "add_keypoint")
                    {
                        //sim animation add_keypoint my_anim_name 1 2 3 10
                        if (commandArgs.size() >= 8)
                        {
                            const float x        = ::atof(commandArgs[4].c_str());
                            const float y        = ::atof(commandArgs[5].c_str());
                            const float z        = ::atof(commandArgs[6].c_str());
                            const float duration = ::atof(commandArgs[7].c_str());

                            if (!AnimationExists(animationName))
                            {
                                return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                            }

                            if (commandArgs.size() >= 9)
                            {
                                bool didError = false;
                                const MoveAnimationType animationType = (MoveAnimationType)Utility::StringToU32(commandArgs[8].c_str(), &didError);
                                if (didError)
                                {
                                    return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                                }
                                AnimationAddKeypoint(animationName, x, y, z, duration, animationType);
                            }
                            else
                            {
                                AnimationAddKeypoint(animationName, x, y, z, duration);
                            }
                            return TerminalCommandHandlerReturnType::SUCCESS;
                        }
                        else
                        {
                            return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
                        }
                    }
                    else if (animationSubCommand == "set_looped")
                    {
                        //sim animation set_looped my_anim_name 1
                        if (commandArgs.size() >= 5)
                        {
                            bool looped = false;
                            if (commandArgs[4] == "1")
                            {
                                looped = true;
                            }
                            else if (commandArgs[4] == "0")
                            {
                                looped = false;
                            }
                            else
                            {
                                return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                            }
                            if (!AnimationExists(animationName))
                            {
                                return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                            }

                            AnimationGet(animationName).SetLooped(looped);
                            return TerminalCommandHandlerReturnType::SUCCESS;
                        }
                        else
                        {
                            return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
                        }
                    }
                }
                else if (animationSubCommand == "is_running"
                      || animationSubCommand == "get_name"
                      || animationSubCommand == "start"
                      || animationSubCommand == "stop")
                {
                    bool didError = false;
                    const u32 serialNumber = Utility::GetIndexForSerial(commandArgs[3].c_str(), &didError);
                    if (didError) return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                    if (!GetNodeEntryBySerialNumber(serialNumber))
                    {
                        return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                    }

                    if (animationSubCommand == "is_running")
                    {
                        //sim animation is_running BBCBC
                        NodeIndexSetter logjsonSetter(0);
                        if (AnimationIsRunning(serialNumber))
                        {
                            logjson("SIM", "{\"type\":\"sim_animation_is_running\",\"serial\":\"%s\",\"code\":1}" SEP, commandArgs[3].c_str());
                        }
                        else
                        {
                            logjson("SIM", "{\"type\":\"sim_animation_is_running\",\"serial\":\"%s\",\"code\":0}" SEP, commandArgs[3].c_str());
                        }
                        return TerminalCommandHandlerReturnType::SUCCESS;
                    }
                    else if (animationSubCommand == "get_name")
                    {
                        //sim animation get_name BBCBC
                        NodeIndexSetter logjsonSetter(0);
                        const std::string animationName = AnimationGetName(serialNumber);
                        logjson("SIM", "{\"type\":\"sim_animation_get_name\",\"serial\":\"%s\",\"name\":\"%s\"}" SEP, commandArgs[3].c_str(), animationName.c_str());
                        return TerminalCommandHandlerReturnType::SUCCESS;
                    }
                    else if (animationSubCommand == "start")
                    {
                        //sim animation start BBCBC my_anim_name
                        if (commandArgs.size() >= 5)
                        {
                            const std::string& animationName = commandArgs[4];
                            if (animationName.size() == 0)
                            {
                                return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                            }
                            if (!AnimationExists(animationName))
                            {
                                return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                            }
                            if (AnimationGet(animationName).GetAmounOfKeyPoints() == 0)
                            {
                                return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
                            }
                            AnimationStart(serialNumber, animationName);
                            return TerminalCommandHandlerReturnType::SUCCESS;
                        }
                        else
                        {
                            return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
                        }
                    }
                    else if (animationSubCommand == "stop")
                    {
                        //sim animation stop BBCBC
                        AnimationStop(serialNumber);
                        return TerminalCommandHandlerReturnType::SUCCESS;
                    }
                }
                else if (animationSubCommand == "load_path")
                {
                    //sim animation load_path /Bla/bli/blub
                    if (commandArgs.size() >= 4)
                    {
                        //This is relative to the normalized path so that it is (more or less) guaranteed that
                        //it is part of the repository. This is necessary so that the replay function still works
                        //properly.
                        if (AnimationLoadJsonFromPath((CherrySimUtils::GetNormalizedPath() + commandArgs[3]).c_str()))
                        {
                            return TerminalCommandHandlerReturnType::SUCCESS;
                        }
                        else
                        {
                            return TerminalCommandHandlerReturnType::INTERNAL_ERROR;
                        }
                    }
                    else
                    {
                        return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
                    }
                }
            }
            else
            {
                return TerminalCommandHandlerReturnType::NOT_ENOUGH_ARGUMENTS;
            }
        }

        //sim set_position BBBBD 0.5 0.21 0.17
        else if (commandArgs.size() >= 5 && (commandArgs[1] == "set_position" || commandArgs[1] == "add_position" || commandArgs[1] == "set_position_norm" || commandArgs[1] == "add_position_norm"))
        {
            size_t index = 0;
            bool nodeFound = false;
            for (u32 i = 0; i < GetTotalNodes(); i++)
            {
                const std::string serialNumberOfNode = nodes[i].gs.config.GetSerialNumber();
                const std::string serialNumberToSearch = commandArgs[2];
                if (serialNumberOfNode == serialNumberToSearch)
                {
                    index = i;
                    nodeFound = true;
                    break;
                }
            }
            if (!nodeFound)
            {
                return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
            }

            float x = 0;
            float y = 0;
            float z = 0;

            try
            {
                x = std::stof(commandArgs[3]);
                y = std::stof(commandArgs[4]);
                z = std::stof(commandArgs[5]);
            }
            catch (std::invalid_argument &e)
            {
                return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
            }

            if (commandArgs[1] == "set_position" || commandArgs[1] == "add_position")
            {
                // We divide by the size of the virtual environment because the user
                // should not have to care about these dimensions. What the user most
                // likely wants to do is to move the node X meter in one direction.
                // For this task the mapWidth/Height are just an annoying distraction.
                // An exception to this is when the user explicitly asked for normalized
                // coordinated via *_norm.
                x /= simConfig.mapWidthInMeters;
                y /= simConfig.mapHeightInMeters;
                z /= simConfig.mapElevationInMeters;
            }
            
            if (commandArgs[1] == "set_position" || commandArgs[1] == "set_position_norm")
            {
                SetPosition(index, x, y, z);
            }
            else if (commandArgs[1] == "add_position" || commandArgs[1] == "add_position_norm")
            {
                AddPosition(index, x, y, z);
            }
            else
            {
                return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
            }

            nodes[index].animation = MoveAnimation(); //Stops the animation

            return TerminalCommandHandlerReturnType::SUCCESS;
        }
    }

    return TerminalCommandHandlerReturnType::UNKNOWN;
}

//Allows another class to register a handler for terminal output
void CherrySim::RegisterTerminalPrintListener(TerminalPrintListener* callback)
{
    terminalPrintListener = callback;
}

//Called for all terminal output from all nodes
void CherrySim::TerminalPrintHandler(const char* message)
{
    if (simConfig.useLogAccumulator)
    {
        logAccumulator += std::string(message);
    }
    if (terminalPrintListener != nullptr) {
        // If currentNode is nullptr, then we printed out something that does not belong to any node
        // This is probably some simulator log e.g. a replay command.
        if (currentNode == nullptr || currentNode->id == simConfig.terminalId || simConfig.terminalId == 0) {
            terminalPrintListener->TerminalPrintHandler(currentNode, message);
        }
    }
}

//################################## Node Lifecycle #######################################
// Create a node, flash a node, boot a node and shut it down
//#########################################################################################

/**
Redirects all pointers used by the FruityMesh implementation to point to the correct data for the node
*/
void CherrySim::SetNode(u32 i)
{
    if (i == 0xFFFFFFFF)
    {
        currentNode       = nullptr;
        simGlobalStatePtr = nullptr;
        simFicrPtr        = nullptr;
        simUicrPtr        = nullptr;
        simGpioPtr        = nullptr;
        simFlashPtr       = nullptr;
        simUartPtr        = nullptr;
        return;
    }
    if (i >= GetTotalNodes())
    {
        std::cerr << "Tried to access node: " << i << std::endl;
        SIMEXCEPTION(IndexOutOfBoundsException);
    }

    //printf("**SIM**: Setting node %u\n", i+1);
    currentNode = &nodes[i];

    simGlobalStatePtr = &(nodes[i].gs);

    simFicrPtr = &(nodes[i].ficr);
    simUicrPtr = &(nodes[i].uicr);
    simGpioPtr = &(nodes[i].gpio);
    simFlashPtr = nodes[i].flash;
    simUartPtr = &(nodes[i].state.uartType);

    __application_start_address = (uint32_t)simFlashPtr + FruityHal::GetSoftDeviceSize();
    __application_end_address = (uint32_t)__application_start_address + ChipsetToApplicationSize(GET_CHIPSET());
    __application_ram_start_address = (uint32_t)currentNode; //FIXME not the correct value, just adummy.

    //Point the linker sections for connectionTypeResolvers to the correct array
    __start_conn_type_resolvers = (u32)connTypeResolvers;
    __stop_conn_type_resolvers = ((u32)connTypeResolvers) + sizeof(connTypeResolvers);

    //TODO: Find a better way to do this
    ChooseSimulatorTerminal();
}

int CherrySim::ChipsetToPageSize(Chipset chipset)
{
    switch (chipset)
    {
    case Chipset::CHIP_NRF52:
    case Chipset::CHIP_NRF52840:
        return 4096;
    default:
        //I don't know this chipset!
        SIMEXCEPTION(IllegalStateException);
        return -1;
    }
}

int CherrySim::ChipsetToCodeSize(Chipset chipset)
{
    switch (chipset)
    {
    case Chipset::CHIP_NRF52:
    case Chipset::CHIP_NRF52840:
        return 128;
    default:
        //I don't know this chipset!
        SIMEXCEPTION(IllegalStateException);
        return -1;
    }
}

int CherrySim::ChipsetToApplicationSize(Chipset chipset)
{
    switch (chipset)
    {
    case Chipset::CHIP_NRF52:
    case Chipset::CHIP_NRF52840:
        return 128 * 1024;
    default:
        //I don't know this chipset!
        SIMEXCEPTION(IllegalStateException);
        return -1;
    }
}

int CherrySim::ChipsetToBootloaderAddr(Chipset chipset)
{
    switch (chipset)
    {
    case Chipset::CHIP_NRF52:
    case Chipset::CHIP_NRF52840:
        return 491520;
    default:
        //I don't know this chipset!
        SIMEXCEPTION(IllegalStateException);
        return -1;
    }
}

void CherrySim::CheckForMultiTensorflowUsage()
{
#ifndef GITHUB_RELEASE
    u32 amountOfTensorflowUsers = 0;
    for (u32 i = 0; i < GetTotalNodes(); i++)
    {
        NodeIndexSetter setter(i);
        AssetModule *assetMod = static_cast<AssetModule*>(GS->node.GetModuleById(ModuleId::ASSET_MODULE));

        if (assetMod != nullptr && assetMod->useIns)
        {
            amountOfTensorflowUsers++;
        }
    }

    if (amountOfTensorflowUsers > 1)
    {
        printf("The Simulator currently only support the use of a single Tensorflow user, but %u users were found!" SEP, amountOfTensorflowUsers);
        SIMEXCEPTION(IllegalStateException);
    }
#endif //GITHUB_RELEASE
}

void CherrySim::QueueInterruptCurrentNode(u32 pin)
{
    if (pin != 0xFFFFFFFF)
    {
        if (currentNode->gpioInitializedPins.find((u32)pin) != currentNode->gpioInitializedPins.end())
        {
            currentNode->interruptQueue.push(pin);
        }
    }
}

void CherrySim::QueueAccelerationInterrutCurrentNode()
{
    if (GS->boardconf.getCustomPinset != nullptr)
    {
        Lis2dh12Pins lis2dh12PinConfig;
        lis2dh12PinConfig.pinsetIdentifier = PinsetIdentifier::LIS2DH12;
        GS->boardconf.getCustomPinset(&lis2dh12PinConfig);
        if (lis2dh12PinConfig.interrupt1Pin != -1) QueueInterruptCurrentNode(lis2dh12PinConfig.interrupt1Pin);
        if (lis2dh12PinConfig.interrupt2Pin != -1) QueueInterruptCurrentNode(lis2dh12PinConfig.interrupt2Pin);
    }
}

/**
Prepares the memory of a node and resets all its data

After calling this function, the node will have its basic data structures prepared, but FlashNode
must be called before booting the node in order to have the flash prepared as well.
*/
void CherrySim::InitNode(u32 i)
{
    //Clean our node state
    nodes[i].~NodeEntry();
    new (&nodes[i]) NodeEntry();

    //Set index and id
    nodes[i].index = i;
    nodes[i].id = i + 1;

    //Initialize flash memory
    CheckedMemset(nodes[i].flash, 0xFF, sizeof(nodes[i].flash));
    //TODO: We could load a softdevice and app image into flash, would that help for something?

    //Generate device address based on the id
    nodes[i].address.addr_type = FruityHal::BleGapAddrType::RANDOM_STATIC;
    CheckedMemset(&nodes[i].address.addr, 0x00, 6);
    CheckedMemcpy(nodes[i].address.addr.data() + 2, &nodes[i].id, 2);
}
void CherrySim::SetFeaturesets()
{
#ifdef GITHUB_RELEASE
    for (u32 i = 0; i < GetTotalNodes(); i++) {
        nodes[i].nodeConfiguration = RedirectFeatureset(nodes[i].nodeConfiguration);
    }
    int amountOfRedirectedFeaturesets = 0;
    std::vector<decltype(simConfig.nodeConfigName.begin())> simConfigsToErase;
    for (auto it = simConfig.nodeConfigName.begin(); it != simConfig.nodeConfigName.end(); it++) {
        if (IsRedirectedFeatureset(it->first))
        {
            amountOfRedirectedFeaturesets += it->second;
            simConfigsToErase.push_back(it);
        }
    }
    for (auto& eraser : simConfigsToErase) {
        simConfig.nodeConfigName.erase(eraser);
    }
    if (simConfig.nodeConfigName.find("github_nrf52") == simConfig.nodeConfigName.end())
    {
        simConfig.nodeConfigName.insert({ "github_nrf52", amountOfRedirectedFeaturesets });
    }
    else
    {
        simConfig.nodeConfigName["github_nrf52"] += amountOfRedirectedFeaturesets;
    }
#endif //GITHUB_RELEASE
    struct FeatureNameOrderPair{
        std::string featuresetName = "";
        u32 noOfNodesWithFeatureset = 0;
        u32 orderNumber = 0;
        bool operator < (const FeatureNameOrderPair &comp) const
        {
            return orderNumber < comp.orderNumber;
        }
    };

    std::vector<FeatureNameOrderPair> definedFeaturesets;
    for (auto it = simConfig.nodeConfigName.begin(); it != simConfig.nodeConfigName.end(); it++)
    {
        auto entry = cherrySimInstance->featuresetPointers.find(it->first);
        if (entry == cherrySimInstance->featuresetPointers.end())
        {
            SIMEXCEPTION(IllegalStateException); //Featureset is not defined yet
        }
        if (it->second <= 0)
        {
            //Found entry for featureset that does not contain any featuresets.
            SIMEXCEPTION(IllegalStateException);
        }
        FeatureNameOrderPair pair;
        pair.featuresetName = it->first;
        pair.noOfNodesWithFeatureset = it->second;
        pair.orderNumber = entry->second.featuresetOrder;
        definedFeaturesets.emplace_back(pair);
    }
    std::sort(definedFeaturesets.begin(), definedFeaturesets.end());

    u32 nodeIndex = 0;
    for (auto it = definedFeaturesets.begin(); it != definedFeaturesets.end();it++)
    {
        for (u32 i = 0; i < it->noOfNodesWithFeatureset; i++) {
            nodes[nodeIndex++].nodeConfiguration = it->featuresetName;
        }
    }
}

//This will configure UICR / FICR and flash (settings,...) of a node
void CherrySim::FlashNode(u32 i) {
    //Configure UICR
    nodes[i].uicr.CUSTOMER[0] = UICR_SETTINGS_MAGIC_WORD; //magicNumber
    nodes[i].uicr.CUSTOMER[1] = 19; //boardType (Simulator board)
    Utility::GenerateBeaconSerialForIndex(i, (char*)(nodes[i].uicr.CUSTOMER + 2)); //serialNumber
    nodes[i].uicr.CUSTOMER[4] = i + 1; //NodeKey
    nodes[i].uicr.CUSTOMER[5] = i + 1;
    nodes[i].uicr.CUSTOMER[6] = i + 1;
    nodes[i].uicr.CUSTOMER[7] = i + 1;
    nodes[i].uicr.CUSTOMER[8] = 0x024D; //manufacturerId
    nodes[i].uicr.CUSTOMER[9] = simConfig.defaultNetworkId; //meshnetworkidentifier (do not put it in 1, as this is the enrollment network
    nodes[i].uicr.CUSTOMER[10] = i + 1; //defaultNodeId
    nodes[i].uicr.CUSTOMER[11] = (u32)DeviceType::STATIC; //deviceType
    nodes[i].uicr.CUSTOMER[12] = i; //serialNumberIndex
    nodes[i].uicr.CUSTOMER[13] = 4; //networkkey
    nodes[i].uicr.CUSTOMER[14] = 0; //networkkey
    nodes[i].uicr.CUSTOMER[15] = 0; //networkkey
    nodes[i].uicr.CUSTOMER[16] = 0; //networkkey

    //TODO: Add app, softdevice, etc,... from .hex files into flash
    //Afterwards, we can use the normal size calculation for addresses without redefining it

    NodeIndexSetter setter(i);
    if (GET_DEVICE_TYPE() == DeviceType::ASSET)
    {
        nodes[i].uicr.CUSTOMER[11] = (u32)DeviceType::ASSET; //deviceType
        //Set the simulated ble stack to default
        nodes[i].bleStackType = BleStackType::NRF_SD_132_ANY;

        // Set a default network id for assets as they cannot be enrolled
        if (simConfig.defaultNetworkId == 0) {
            nodes[i].uicr.CUSTOMER[9] = 123;
        }
    }


    else {
        //Set the simulated ble stack to default
        nodes[i].bleStackType = simConfig.defaultBleStackType;
    }
}

void CherrySim::ErasePage(u32 pageAddress)
{
    u32* p = (u32*)pageAddress;

    for (u32 i = 0; i < FruityHal::GetCodePageSize() / sizeof(u32); i++) {
        p[i] = 0xFFFFFFFF;
    }
}

void CherrySim::BootCurrentNode()
{
    //Configure FICR
    //We can't do this any earlier because test code might want to change the featureset.
    currentNode->ficr.CODESIZE = ChipsetToCodeSize(GetChipset_CherrySim());
    currentNode->ficr.CODEPAGESIZE = ChipsetToPageSize(GetChipset_CherrySim());

    //Initialize UICR
    currentNode->uicr.BOOTLOADERADDR = ChipsetToBootloaderAddr(GetChipset_CherrySim());
    //Put some data where the bootloader is supposed to be (add a version number)
    *((u32*)&currentNode->flash[currentNode->uicr.BOOTLOADERADDR + 1024]) = 123;

    if (currentNode->ficr.CODESIZE * currentNode->ficr.CODEPAGESIZE > SIM_MAX_FLASH_SIZE)
    {
        SIMEXCEPTION(RequiredFlashTooBigException);
    }
    //############## Prepare the node memory

    currentNode->restartCounter++;
    currentNode->bmgWasInit        = false;
    currentNode->twiWasInit        = false;
    currentNode->Tlv49dA1b6WasInit = false;
    currentNode->spiWasInit        = false;
    currentNode->lis2dh12WasInit   = false;
    currentNode->bme280WasInit     = false;
    currentNode->gpioInitializedPins.clear();
    currentNode->interruptQueue = {};
    currentNode->lastMovementSimTimeMs = 0;

    //Place a new GlobalState instance into our NodeEntry
    if (simGlobalStatePtr != nullptr) {
        simGlobalStatePtr->~GlobalState();
    }

    //Erase bootloader settings page
    ErasePage(FruityHal::GetBootloaderSettingsAddress());

    simGlobalStatePtr = new (&currentNode->gs) GlobalState();

    //Reset our GPIO Peripheral
    CheckedMemset(simGpioPtr, 0x00, sizeof(NRF_GPIO_Type));

    //Create a queue for events    if (simGlobalStatePtr != nullptr) {
    currentNode->eventQueue.~deque();
    new (&(currentNode->eventQueue)) std::queue<simBleEvent>();

    //Set the Ble stack parameters in the node so that we can use them later
    SetBleStack(currentNode);

    //Initialize new SoftDevice state
    currentNode->state.~SoftdeviceState();
    new (&currentNode->state) SoftdeviceState();

    //Allocate halMemory
    const u32 halMemorySize = FruityHal::GetHalMemorySize() / sizeof(u32) + 1;
    u32* halMemory = new u32[halMemorySize];
    CheckedMemset(halMemory, 0, halMemorySize * sizeof(u32));
    GS->halMemory = halMemory;

    //############## Boot the node using the FruityMesh boot routine
    BootFruityMesh();

    //Create memory for modules
    const u32 moduleMemoryBlockSize = INITIALIZE_MODULES(false);
    currentNode->moduleMemoryBlock = (u8*)new u32[moduleMemoryBlockSize / sizeof(u32) + 1];
    GS->moduleAllocator.SetMemory(currentNode->moduleMemoryBlock, moduleMemoryBlockSize);
    //Boot the modules
    BootModules();

    //############## Register the simulator terminal and Pre-configure the node

    //FIXME: Move to runner / tester
    //Lets us do some configuration after the boot
    Conf::GetInstance().terminalMode = TerminalMode::PROMPT;
    Conf::GetInstance().defaultLedMode = LedMode::OFF;
}

void CherrySim::ResetCurrentNode(RebootReason rebootReason, bool throwException) {
    if (simConfig.verbose) printf("Node %d resetted\n", currentNode->id);

    //Save the node index because it will be gone after node shutdown
    u32 index = currentNode->index;

    //Clean up node
    ShutdownCurrentNode();

    //Disconnect all simulator connections to this node
    for (int i = 0; i < currentNode->state.configuredTotalConnectionCount; i++) {
        SoftdeviceConnection* connection = &nodes[index].state.connections[i];
        DisconnectSimulatorConnection(connection, BLE_HCI_CONNECTION_TIMEOUT, BLE_HCI_CONNECTION_TIMEOUT);
    }

    //Boot node again
    NodeIndexSetter setter(index);
    if (rebootReason != RebootReason::UNKNOWN)
    {
        currentNode->rebootReason = rebootReason;
    }
    BootCurrentNode();

    //throw an exception so we can step out of the current simulation step for that node
    if (throwException)
    {
        throw NodeSystemResetException();
    }
}

void CherrySim::ShutdownCurrentNode() {
    delete[] currentNode->moduleMemoryBlock;
    //Cast is needed because the following passage from the C++ Standard:
    //"This implies that an object cannot be deleted using a pointer of type void* because there are no objects of type void"
    u32* halMemory = (u32*)GS->halMemory;
    delete[] halMemory;
}

//################################## Flash Simulation #####################################
// Simulation of Flash Access
// TODO: Currently calls the DispatchSystemEvents handler,
// but should instead queue the event in a systemEventQueue and the node should fetch it.
// Also, the flash write should only happen after some time and not instant, should also be able to fail
//#########################################################################################

void CherrySim::SimulateFlashCommit() {
    if (PSRNG(simConfig.asyncFlashCommitTimeProbability)) {
        SimCommitFlashOperations();
    }
}

//Calls the system event dispatcher to mark flash operations complete
//The erases/writes themselves are executed immediately at the moment, though
//This will loop until all flash operations (also those that are queued in response to a successfuly operation) are executed
void CherrySim::SimCommitFlashOperations()
{
    if (cherrySimInstance->simConfig.simulateAsyncFlash) {
        while (cherrySimInstance->currentNode->state.numWaitingFlashOperations > 0) {
            DispatchSystemEvents(FruityHal::SystemEvents::FLASH_OPERATION_SUCCESS);
            cherrySimInstance->currentNode->state.numWaitingFlashOperations--;
        }
    }
}

//Uses a list of fails to simulate some successful and some failed flash operations
void CherrySim::SimCommitSomeFlashOperations(const uint8_t* failData, uint16_t numMaxEvents)
{
    u32 i = 0;
    if (cherrySimInstance->simConfig.simulateAsyncFlash) {
        while (cherrySimInstance->currentNode->state.numWaitingFlashOperations > 0 && i < numMaxEvents) {
            if (failData[i] == 0) DispatchSystemEvents(FruityHal::SystemEvents::FLASH_OPERATION_SUCCESS);
            else DispatchSystemEvents(FruityHal::SystemEvents::FLASH_OPERATION_ERROR);
            cherrySimInstance->currentNode->state.numWaitingFlashOperations--;
            i++;
        }
    }
}

//################################## GAP Simulation ################################
// Simulates advertising, connections and disconnections
//#########################################################################################

//Simuliert das aussenden von Advertising nachrichten. Wenn andere nodes gerade scannen bekommen sie es als advertising event mitgeteilt,
//Wenn eine andere node gerade eine verbindung zu diesem Partner aufbauen will, wird das advertisen der anderen node gestoppt, die verbindung wird
//connected und es wird an beide nodes ein Event geschickt, dass sie nun verbunden sind
void CherrySim::SimulateBroadcast() {
    //Check for other nodes that are scanning and send them the events
    if (currentNode->state.advertisingActive) {
        if (ShouldSimIvTrigger(currentNode->state.advertisingIntervalMs)) {
            //Distribute the event to all nodes in range
            for (u32 i = 0; i < GetTotalNodes(); i++) {
                if (i != currentNode->index) {

                    //If the other node is scanning
                    if (nodes[i].state.scanningActive) {
                        //If the random value hits the probability, the event is sent
                        uint32_t probability = CalculateReceptionProbability(currentNode, &nodes[i]);
                        if (PSRNG(probability)) {
                            simBleEvent s;
                            s.globalId = simState.globalEventIdCounter++;
                            s.bleEvent.header.evt_id = BLE_GAP_EVT_ADV_REPORT;
                            s.bleEvent.header.evt_len = s.globalId;
                            s.bleEvent.evt.gap_evt.conn_handle = BLE_CONN_HANDLE_INVALID;

                            CheckedMemcpy(&s.bleEvent.evt.gap_evt.params.adv_report.data, &currentNode->state.advertisingData, currentNode->state.advertisingDataLength);
                            s.bleEvent.evt.gap_evt.params.adv_report.dlen = currentNode->state.advertisingDataLength;
                            CheckedMemset(&s.bleEvent.evt.gap_evt.params.adv_report.peer_addr, 0, sizeof(s.bleEvent.evt.gap_evt.params.adv_report.peer_addr));
                            s.bleEvent.evt.gap_evt.params.adv_report.peer_addr.addr_type = (u8)currentNode->address.addr_type;
                            static_assert(sizeof(s.bleEvent.evt.gap_evt.params.adv_report.peer_addr.addr) == sizeof(currentNode->address.addr), "See next line.");
                            CheckedMemcpy(&s.bleEvent.evt.gap_evt.params.adv_report.peer_addr.addr, &currentNode->address.addr, sizeof(currentNode->address.addr));
                            //TODO: bleEvent.evt.gap_evt.params.adv_report.peer_addr = ...;
                            s.bleEvent.evt.gap_evt.params.adv_report.rssi = (i8)GetReceptionRssi(currentNode, &nodes[i]);
                            s.bleEvent.evt.gap_evt.params.adv_report.scan_rsp = 0;
                            s.bleEvent.evt.gap_evt.params.adv_report.type = (u8)currentNode->state.advertisingType;

                            nodes[i].eventQueue.push_back(s);
                        }
                    }
                    //If the other node is connecting
                    else if (nodes[i].state.connectingActive && currentNode->state.advertisingType == FruityHal::BleGapAdvType::ADV_IND) {
                        //If the other node matches our partnerId we are connecting to
                        if (memcmp(&nodes[i].state.connectingPartnerAddr, &currentNode->address, sizeof(FruityHal::BleGapAddr)) == 0) {
                            //If the random value hits the probability, the event is sent
                            uint32_t probability = CalculateReceptionProbability(currentNode, &nodes[i]);
                            if (PSRNG(probability)) {

                                ConnectMasterToSlave(&nodes[i], currentNode);

                                //Disable advertising for the own node, because this will be stopped after a connection is made
                                //Then, Immediately return to not broadcast more packets
                                currentNode->state.advertisingActive = false;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
}

ble_gap_addr_t CherrySim::Convert(const FruityHal::BleGapAddr* address)
{
    ble_gap_addr_t addr;
    CheckedMemset(&addr, 0x00, sizeof(addr));
    CheckedMemcpy(addr.addr, address->addr.data(), FH_BLE_GAP_ADDR_LEN);
    addr.addr_type = (u8)address->addr_type;
#ifdef NRF52
    addr.addr_id_peer = 0;
#endif
    return addr;
}

FruityHal::BleGapAddr CherrySim::Convert(const ble_gap_addr_t* p_addr)
{
    FruityHal::BleGapAddr address;
    CheckedMemset(&address, 0x00, sizeof(address));
    CheckedMemcpy(address.addr.data(), p_addr->addr, FH_BLE_GAP_ADDR_LEN);
    address.addr_type = (FruityHal::BleGapAddrType)p_addr->addr_type;

    return address;
}

//Connects two nodes, the currentNode is the slave
void CherrySim::ConnectMasterToSlave(NodeEntry* master, NodeEntry* slave)
{
    //Increments global counter for global unique connection Handle
    simState.globalConnHandleCounter++;
    if (simState.globalConnHandleCounter > 65000) {
        //We can only simulate this far, output a warning that the state is inconsistent after this point in simulation
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! ATTENTION:Wrapping globalConnHandleCounter !!!!!!!!!!!!!!!!!!!!!!!\n");
        simState.globalConnHandleCounter = 1;
    }

    //Print json for sim_connect_central
    if (this->simConfig.verbose) {
        json j1;
        j1["type"] = "sim_connect";
        j1["nodeId"] = master->id;
        j1["partnerId"] = slave->id;
        j1["globalConnectionHandle"] = simState.globalConnHandleCounter;
        j1["rssi"] = (int)GetReceptionRssi(master, slave);
        j1["timeMs"] = simState.simTimeMs;

        printf("%s" EOL, j1.dump().c_str());
    }

    //###### Current node

    //Find out if the device has another free Peripheral connection available
    u8 activePeripheralConnCount = 0;
    SoftdeviceConnection* freeInConnection = nullptr;
    for (int i = 0; i < currentNode->state.configuredTotalConnectionCount; i++) {
        if (slave->state.connections[i].connectionActive && !slave->state.connections[i].isCentral) {
            activePeripheralConnCount++;
        }
        //Save a reference to a free connection spot if we found one
        else if (!slave->state.connections[i].connectionActive) {
            freeInConnection = &slave->state.connections[i];
        }
    }

    //Must not happen, the device was being connected to, but does not have a free inConnection
    //It shouldn't have advertised in the first place
    if (activePeripheralConnCount == slave->state.configuredPeripheralConnectionCount) {
        SIMEXCEPTION(IllegalStateException);
    }
    if (freeInConnection == nullptr) {
        SIMEXCEPTIONFORCE(IllegalStateException);
    }

    freeInConnection->connectionActive = true;
    freeInConnection->rssiMeasurementActive = false;
    freeInConnection->connectionIndex = 0;
    freeInConnection->connectionHandle = simState.globalConnHandleCounter;
    freeInConnection->connectionInterval = master->state.connectionParamIntervalMs;
    freeInConnection->connectionSupervisionTimeoutMs = master->state.connectionTimeoutMs;
    freeInConnection->owningNode = slave;
    freeInConnection->partner = master;
    freeInConnection->connectionMtu = GATT_MTU_SIZE_DEFAULT;
    freeInConnection->isCentral = false;
    freeInConnection->lastReceivedPacketTimestampMs = simState.simTimeMs;

    //Generate an event for the current node
    simBleEvent s2;
    s2.globalId = simState.globalEventIdCounter++;
    s2.bleEvent.header.evt_id = BLE_GAP_EVT_CONNECTED;
    s2.bleEvent.header.evt_len = s2.globalId;
    s2.bleEvent.evt.gap_evt.conn_handle = simState.globalConnHandleCounter;

    s2.bleEvent.evt.gap_evt.params.connected.conn_params.min_conn_interval = slave->state.connectionParamIntervalMs;
    s2.bleEvent.evt.gap_evt.params.connected.conn_params.max_conn_interval = slave->state.connectionParamIntervalMs;
    s2.bleEvent.evt.gap_evt.params.connected.peer_addr = Convert(&master->address);
    s2.bleEvent.evt.gap_evt.params.connected.role = BLE_GAP_ROLE_PERIPH;

    slave->eventQueue.push_back(s2);

    //###### Remote node

    u8 activeCentralConnCount = 0;
    SoftdeviceConnection* freeOutConnection = nullptr;
    u16 connIndex = 0;
    for (int i = 0; i < currentNode->state.configuredTotalConnectionCount; i++) {
        if (master->state.connections[i].connectionActive && master->state.connections[i].isCentral) {
            activeCentralConnCount++;
        }
        //Save a reference to a free connection spot if we found one
        else if (!master->state.connections[i].connectionActive) {
            freeOutConnection = &master->state.connections[i];
            connIndex = i;
        }
    }

    //Check if the maximum number of active central connections was exceeded
    //Must not happen as the node should not have been allowed to call gap_connect in the first place
    if (activeCentralConnCount == master->state.configuredCentralConnectionCount) {
        SIMEXCEPTION(IllegalStateException);
    }

    if (freeOutConnection == nullptr) {
        SIMEXCEPTIONFORCE(IllegalStateException);
    }

    freeOutConnection->connectionIndex = connIndex;
    freeOutConnection->connectionActive = true;
    freeOutConnection->rssiMeasurementActive = false;
    freeOutConnection->connectionHandle = simState.globalConnHandleCounter;
    freeOutConnection->connectionInterval = master->state.connectionParamIntervalMs;
    freeOutConnection->connectionSupervisionTimeoutMs = master->state.connectionTimeoutMs;
    freeOutConnection->owningNode = master;
    freeOutConnection->partner = slave;
    freeOutConnection->connectionMtu = GATT_MTU_SIZE_DEFAULT;
    freeOutConnection->isCentral = true;
    freeOutConnection->lastReceivedPacketTimestampMs = simState.simTimeMs;

    //Save connection references
    freeInConnection->partnerConnection = freeOutConnection;
    freeOutConnection->partnerConnection = freeInConnection;

    //Generate an event for the remote node
    simBleEvent s;
    s.globalId = simState.globalEventIdCounter++;
    s.bleEvent.header.evt_id = BLE_GAP_EVT_CONNECTED;
    s.bleEvent.header.evt_len = s.globalId;
    s.bleEvent.evt.gap_evt.conn_handle = simState.globalConnHandleCounter;

    s.bleEvent.evt.gap_evt.params.connected.conn_params.min_conn_interval = slave->state.connectionParamIntervalMs;
    s.bleEvent.evt.gap_evt.params.connected.conn_params.max_conn_interval = slave->state.connectionParamIntervalMs;
    s.bleEvent.evt.gap_evt.params.connected.peer_addr = Convert(&slave->address);
    s.bleEvent.evt.gap_evt.params.connected.role = BLE_GAP_ROLE_CENTRAL;

    master->eventQueue.push_back(s);

    //Disable connecting for the other node because we just got the remote SoftDevice a connection
    master->state.connectingActive = false;
}

u32 CherrySim::DisconnectSimulatorConnection(SoftdeviceConnection* connection, u32 hciReason, u32 hciReasonPartner) {

    //If it could not be found, the connection might have been terminated by a peer or the sim already or something was wrong
    if (connection == nullptr || !connection->connectionActive) {
        //TODO: maybe log possible SIM error?
        return BLE_ERROR_INVALID_CONN_HANDLE;
    }

    //Find the connection on the partners side
    NodeEntry* partnerNode = connection->partner;
    SoftdeviceConnection* partnerConnection = connection->partnerConnection;

    if (this->simConfig.verbose)
    {
        json j;
        j["type"] = "sim_disconnect";
        j["nodeId"] = connection->owningNode->id;
        j["partnerId"] = partnerNode->id;
        j["globalConnectionHandle"] = connection->connectionHandle;
        j["timeMs"] = simState.simTimeMs;
        j["reason"] = hciReason;

        printf("%s" EOL, j.dump().c_str());
    }

    //TODO: this is wrong, fix when this happens
    if (partnerConnection == nullptr) {
        SIMEXCEPTIONFORCE(IllegalStateException);
    }

    //Clear the transmitbuffers for both nodes
    CheckedMemset(connection->reliableBuffers, 0x00, sizeof(connection->reliableBuffers));
    CheckedMemset(connection->unreliableBuffers, 0x00, sizeof(connection->unreliableBuffers));
    CheckedMemset(partnerConnection->reliableBuffers, 0x00, sizeof(connection->reliableBuffers));
    CheckedMemset(partnerConnection->unreliableBuffers, 0x00, sizeof(connection->unreliableBuffers));

    //#### Our own node
    connection->connectionActive = false;

    simBleEvent s1;
    s1.globalId = simState.globalEventIdCounter++;
    s1.bleEvent.header.evt_id = BLE_GAP_EVT_DISCONNECTED;
    s1.bleEvent.header.evt_len = s1.globalId;
    s1.bleEvent.evt.gap_evt.conn_handle = connection->connectionHandle;
    s1.bleEvent.evt.gap_evt.params.disconnected.reason = hciReason;
    connection->owningNode->eventQueue.push_back(s1);

    //#### Remote node
    partnerConnection->connectionActive = false;

    simBleEvent s2;
    s2.globalId = simState.globalEventIdCounter++;
    s2.bleEvent.header.evt_id = BLE_GAP_EVT_DISCONNECTED;
    s2.bleEvent.header.evt_len = s2.globalId;
    s2.bleEvent.evt.gap_evt.conn_handle = partnerConnection->connectionHandle;
    s2.bleEvent.evt.gap_evt.params.disconnected.reason = hciReasonPartner;
    partnerNode->eventQueue.push_back(s2);

    return NRF_SUCCESS;
}

void CherrySim::SimulateTimeouts() {
    if (currentNode->state.connectingActive && currentNode->state.connectingTimeoutTimestampMs <= (i32)simState.simTimeMs) {
        currentNode->state.connectingActive = false;

        simBleEvent s;
        s.globalId = simState.globalEventIdCounter++;
        s.bleEvent.header.evt_id = BLE_GAP_EVT_TIMEOUT;
        s.bleEvent.header.evt_len = s.globalId;
        s.bleEvent.evt.gap_evt.conn_handle = BLE_CONN_HANDLE_INVALID;
        s.bleEvent.evt.gap_evt.params.timeout.src = BLE_GAP_TIMEOUT_SRC_CONN;

        currentNode->eventQueue.push_back(s);
    }
}

void CherrySim::SimulateUartInterrupts()
{
    const SoftdeviceState &state = currentNode->state;
    while (state.uartReadIndex != state.uartBufferLength && cherrySimInstance->currentNode->state.currentlyEnabledUartInterrupts != 0) {
        UART0_IRQHandler();
    }
}

void CherrySim::SendUartCommand(NodeId nodeId, const u8* message, u32 messageLength)
{
    SoftdeviceState* state = &(cherrySimInstance->FindNodeById(nodeId)->state);
    u32 oldBufferLength = state->uartBufferLength;
    state->uartBufferLength += messageLength;

    if (state->uartBufferLength > state->uartBuffer.size()) {
        SIMEXCEPTION(MessageTooLongException);
    }
    CheckedMemcpy(state->uartBuffer.data() + oldBufferLength, message, messageLength);
}
//################################## GATT Simulation ######################################
// Generates writes
//#########################################################################################

//This function compares the globalPacketIds of all packets in the reliable and unreliable buffers
//then, it returns the packet that was inserted first into one of these buffers and should therefore be sent
SoftDeviceBufferedPacket* getNextPacketToWrite(SoftdeviceConnection* connection)
{
    u32 lowestGlobalId = UINT32_MAX;
    SoftDeviceBufferedPacket* packet = nullptr;

    //We only have 1 unreliable buffer in the Softdevice
    if (connection->reliableBuffers[0].sender != nullptr) {
        lowestGlobalId = connection->reliableBuffers[0].globalPacketId;
        packet = connection->reliableBuffers + 0;
    }

    for (int k = 0; k < SIM_NUM_UNRELIABLE_BUFFERS; k++) {
        if (connection->unreliableBuffers[k].sender != nullptr && connection->unreliableBuffers[k].globalPacketId < lowestGlobalId)
        {
            lowestGlobalId = connection->unreliableBuffers[k].globalPacketId;
            packet = connection->unreliableBuffers + k;
        }
    }

    return packet;
}

void CherrySim::SendUnreliableTxCompleteEvent(NodeEntry* node, int connHandle, u8 packetCount)
{
    if (packetCount > 0) {
        simBleEvent s2;
        s2.globalId = simState.globalEventIdCounter++;
        s2.bleEvent.header.evt_id = BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE;
        s2.bleEvent.header.evt_len = s2.globalId;
        s2.bleEvent.evt.gattc_evt.conn_handle = connHandle;
        s2.bleEvent.evt.gattc_evt.params.write_cmd_tx_complete.count = packetCount;

        node->eventQueue.push_back(s2);
    }
}

void CherrySim::SimulateConnections() {
    /* Currently, the simulation will only take one connection event to transmit a reliable packet and both the packet event and the ACK will be generated
    * at the same time. Also, all unreliable packets are always sent in one conneciton event.
    * If many connections exist with short connection intervals, the behaviour is not realistic as the amount of packets that are being sent should decrease.
    * There is also no probability of failure and the buffer is always emptied
    */

    if (blockConnections) return;

    //Simulate sending data for each connection individually
    for (int i = 0; i < currentNode->state.configuredTotalConnectionCount; i++) {
        SoftdeviceConnection* connection = &currentNode->state.connections[i];
        if (connection->connectionActive) {

            //FIXME: This implementation will currently not calculate a correct throughput for packets
            //if the interval is smaller than the simulation timestep, it will only simulate one connectionEvent
            //To fix this, we should calculate the throughput according to our documented measurements
            //depending on the number of connections, whether scanning / advertising are active, the eventLength and the interval

            u16 connectionIntervalMs = connection->connectionInterval;

            //FIXME: This is a workaround as the simulation timestep is probably not dividable by (int)7.5
            if (connectionIntervalMs == (int)7.5f) connectionIntervalMs = 10;

            //Each connecitonInterval, we see if there are any packets to send
            if (ShouldSimIvTrigger(connectionIntervalMs)) {

                //Depending on the number of connections, we send a random amount of packets from the unreliable buffers
                u8 numConnections = GetNumSimConnections(currentNode);
                u8 numPacketsToSend;
                u32 unreliablePacketsSent = 0;

                if (numConnections == 1) numPacketsToSend = (u8)PSRNGINT(0, SIM_NUM_UNRELIABLE_BUFFERS);
                else if (numConnections == 2) numPacketsToSend = (u8)PSRNGINT(0, 5);
                else numPacketsToSend = (u8)PSRNGINT(0, 3);

                const double rssiMult = CalculateReceptionProbability(connection->owningNode, connection->partner);
                if (rssiMult == 0)
                {
                    numPacketsToSend = 0;
                }
                else
                {
                    currentNode->state.connections[i].lastReceivedPacketTimestampMs = this->simState.simTimeMs;
                }
                

                //Simulate timeouts if messages can't be send anymore.
                SoftDeviceBufferedPacket* packet = getNextPacketToWrite(connection);
                if (packet != nullptr)
                {
                    const u32 timeInQueueMs = simState.simTimeMs - packet->queueTimeMs;
                    if (timeInQueueMs > 30 * 1000)
                    {
                        DisconnectSimulatorConnection(&currentNode->state.connections[i], BLE_HCI_CONNECTION_TIMEOUT, BLE_HCI_CONNECTION_TIMEOUT);
                        continue;
                    }
                }

                // Simulate timeouts if there was no message received within connection interval
                if (simState.simTimeMs >= 
                    (currentNode->state.connections[i].lastReceivedPacketTimestampMs + 
                     currentNode->state.connections[i].connectionSupervisionTimeoutMs))
                {
                    DisconnectSimulatorConnection(&currentNode->state.connections[i], BLE_HCI_CONNECTION_TIMEOUT, BLE_HCI_CONNECTION_TIMEOUT);
                }

                for (int k = 0; k < numPacketsToSend; k++) {
                    SoftDeviceBufferedPacket* packet = getNextPacketToWrite(connection);
                    if (packet == nullptr) break;

                    //Notifications
                    if (packet->isHvx) {
                        GenerateNotification(packet);
                        //Remove packet from softdevice buffer
                        packet->sender = nullptr;
                        unreliablePacketsSent++;
                    }
                    //Unreliable Writes
                    else if (packet->params.writeParams.write_op == BLE_GATT_OP_WRITE_CMD) {
                        GenerateWrite(packet);
                        //Remove packet from softdevice buffer
                        packet->sender = nullptr;
                        unreliablePacketsSent++;
                    }
                    //Reliable Writes
                    else if (packet->params.writeParams.write_op == BLE_GATT_OP_WRITE_REQ) {

                        //Send tx complete for all previous unreliable writes if there were any
                        SendUnreliableTxCompleteEvent(currentNode, connection->connectionHandle, unreliablePacketsSent);
                        unreliablePacketsSent = 0;

                        GenerateWrite(packet);
                        //Remove packet from softdevice buffer
                        packet->sender = nullptr;

                        //Generate the event that the write was successful immediately
                        //TODO: Could be postponed a bit to better match the real world
                        simBleEvent s2;
                        s2.globalId = simState.globalEventIdCounter++;
                        s2.bleEvent.header.evt_id = BLE_GATTC_EVT_WRITE_RSP;
                        s2.bleEvent.header.evt_len = s2.globalId;
                        s2.bleEvent.evt.gattc_evt.conn_handle = connection->connectionHandle;
                        s2.bleEvent.evt.gattc_evt.gatt_status = (u16)FruityHal::BleGattEror::SUCCESS;
                        //Save the global packet id so that we can track where a packet was generated after we receive it
                        s2.additionalInfo = packet->globalPacketId;
                        currentNode->eventQueue.push_back(s2);



                        //Do not send any more packets this connectionEvent as we need to wait for an ACK
                        break;
                    }
                    else {
                        SIMEXCEPTION(IllegalArgumentException);
                    }
                }

                //Send remaining accumulated tx complete events for notifications and unreliable writes
                SendUnreliableTxCompleteEvent(currentNode, connection->connectionHandle, unreliablePacketsSent);
            }
        }
    }

    // Simulate Connection RSSI measurements
    for (int i = 0; i < currentNode->state.configuredTotalConnectionCount; i++) {
        if (ShouldSimIvTrigger(5000)) {
            SoftdeviceConnection* connection = &currentNode->state.connections[i];
            if (connection->connectionActive && connection->rssiMeasurementActive) {
                NodeEntry* master = i == 0 ? connection->partner : currentNode;
                NodeEntry* slave = i == 0 ? currentNode : connection->partner;

                simBleEvent s;
                s.globalId = simState.globalEventIdCounter++;
                s.bleEvent.header.evt_id = BLE_GAP_EVT_RSSI_CHANGED;
                s.bleEvent.header.evt_len = s.globalId;
                s.bleEvent.evt.gap_evt.conn_handle = connection->connectionHandle;
                s.bleEvent.evt.gap_evt.params.rssi_changed.rssi = (i8)GetReceptionRssi(master, slave);

                currentNode->eventQueue.push_back(s);
            }
        }
    }

    //Simulate Connection Loss every second
    if (simConfig.connectionTimeoutProbabilityPerSec != 0) {
        for (int i = 0; i < currentNode->state.configuredTotalConnectionCount; i++) {
            if (currentNode->state.connections[i].connectionActive) {
                if (PSRNG(simConfig.connectionTimeoutProbabilityPerSec)) {
                    SIMSTATCOUNT("simulatedTimeouts");
                    printf("Simulated Connection Loss for node %d to partner %d (handle %d)" EOL, currentNode->id, currentNode->state.connections[i].partner->id, currentNode->state.connections[i].connectionHandle);
                    DisconnectSimulatorConnection(&currentNode->state.connections[i], BLE_HCI_CONNECTION_TIMEOUT, BLE_HCI_CONNECTION_TIMEOUT);
                }
            }
        }
    }
}

//This function generates a WRITE event and a TX for two nodes that want to send data
void CherrySim::GenerateWrite(SoftDeviceBufferedPacket* bufferedPacket) {

    NodeEntry* sender = bufferedPacket->sender;
    NodeEntry* receiver = bufferedPacket->receiver;
    uint16_t conn_handle = bufferedPacket->connHandle;
    const ble_gattc_write_params_t& p_write_params = bufferedPacket->params.writeParams;

    if (this->simConfig.verbose)
    {
        json j;
        j["type"] = "sim_data";
        j["nodeId"] = sender->id;
        j["partnerId"] = receiver->id;
        j["reliable"] = p_write_params.write_op == BLE_GATT_OP_WRITE_REQ;
        j["timeMs"] = simState.simTimeMs;
        char buffer[128];
        Logger::ConvertBufferToHexString(p_write_params.p_value, p_write_params.len, buffer, sizeof(buffer));
        j["data"] = buffer;
        printf("%s" EOL, j.dump().c_str());
    }

    //Generate WRITE event at our partners side
    simBleEvent s;
    s.globalId = simState.globalEventIdCounter++;
    s.bleEvent.header.evt_id = BLE_GATTS_EVT_WRITE;
    s.bleEvent.header.evt_len = s.globalId;

    //Save the global packet id so that we can track where a packet was generated after we receive it
    s.additionalInfo = bufferedPacket->globalPacketId;

    //Generate write event in partners event queue
    s.bleEvent.evt.gatts_evt.conn_handle = conn_handle;

#ifdef SIM_ENABLED
    //If we are dealing with a non mesh access connection, we can check if the message type is invalid and throw an error
    BaseConnection *bc = GS->cm.GetRawConnectionFromHandle(conn_handle);
    MeshAccessConnection *mac = dynamic_cast<MeshAccessConnection*>(bc);
    if (p_write_params.p_value[0] == 0
        &&
        (
            mac == nullptr
            ||
            (
                mac->encryptionState != EncryptionState::ENCRYPTED
                && mac->GetAmountOfCorruptedMessaged() == 0
                )
            )) {
        SIMEXCEPTION(IllegalStateException);
    }
#endif

    CheckedMemcpy(&s.bleEvent.evt.gatts_evt.params.write.data, p_write_params.p_value, p_write_params.len);
    s.bleEvent.evt.gatts_evt.params.write.handle = p_write_params.handle;
    s.bleEvent.evt.gatts_evt.params.write.len = p_write_params.len;
    s.bleEvent.evt.gatts_evt.params.write.offset = 0;
    s.bleEvent.evt.gatts_evt.params.write.op = p_write_params.write_op;

    receiver->eventQueue.push_back(s);
}

void CherrySim::GenerateNotification(SoftDeviceBufferedPacket* bufferedPacket) {

    NodeEntry* sender = bufferedPacket->sender;
    NodeEntry* receiver = bufferedPacket->receiver;
    uint16_t conn_handle = bufferedPacket->connHandle;
    const ble_gatts_hvx_params_t & hvx_params = bufferedPacket->params.hvxParams;

    if (this->simConfig.verbose)
    {
        json j;
        j["type"] = "sim_data";
        j["nodeId"] = sender->id;
        j["partnerId"] = receiver->id;
        j["reliable"] = false;
        j["timeMs"] = simState.simTimeMs;
        char buffer[128];
        Logger::ConvertBufferToHexString(hvx_params.p_data, (u32)hvx_params.p_len, buffer, 128);
        j["data"] = buffer;
        printf("%s" EOL, j.dump().c_str());
    }

    //Generate HVX event at our partners side
    simBleEvent s;
    s.globalId = simState.globalEventIdCounter++;
    s.bleEvent.header.evt_id = BLE_GATTC_EVT_HVX;
    s.bleEvent.header.evt_len = s.globalId;
    s.bleEvent.evt.gattc_evt.conn_handle = conn_handle;

    //jstodo check this workaround again.
    // This is a workaround for hvxParams keeping only pointer to len.
    CheckedMemcpy(&s.bleEvent.evt.gattc_evt.params.hvx.data, hvx_params.p_data, (u32)hvx_params.p_len);
    s.bleEvent.evt.gattc_evt.params.hvx.handle = hvx_params.handle;
    // This is a workaround for hvxParams keeping only pointer to len.
    s.bleEvent.evt.gattc_evt.params.hvx.len = (u16)(u32)hvx_params.p_len;
    s.bleEvent.evt.gattc_evt.params.hvx.type = hvx_params.type;

    receiver->eventQueue.push_back(s);
}

void CherrySim::StartServiceDiscovery(u16 connHandle, const ble_uuid_t &p_uuid, int discoveryTimeMs)
{
    currentNode->state.connHandle = connHandle;
    currentNode->state.uuid = p_uuid;
    currentNode->state.discoveryDoneTime = simState.simTimeMs + discoveryTimeMs;
}

// Simulates discovering services for connection with given handle.
void CherrySim::SimulateServiceDiscovery()
{
    if ((currentNode->state.discoveryDoneTime == 0) ||
        (currentNode->state.discoveryDoneTime >= simState.simTimeMs)) return;

    currentNode->state.discoveryDoneTime = 0;

    SoftdeviceConnection * p_discoveryConnection = nullptr;

    for (int i = 0; i < currentNode->state.configuredTotalConnectionCount; i++)
    {
        SoftdeviceConnection * p_temp = &currentNode->state.connections[i];
        if ((p_temp->connectionActive == true) &&
            (p_temp->isCentral) &&
            (p_temp->connectionHandle == currentNode->state.connHandle)) //TODO: take care that connections are filled from id0
        {
            p_discoveryConnection = &currentNode->state.connections[i];
            break;
        }
    }

    if (p_discoveryConnection == nullptr) return;

    FruityHal::BleGattDBDiscoveryEvent dbEvt;
    CheckedMemset(&dbEvt, 0, sizeof(dbEvt));
    dbEvt.connHandle = currentNode->state.connHandle;

    ServiceDB_t * p_tempService = nullptr;
    for (int i = 0; i < currentNode->state.servicesCount; i++)
    {
        if (currentNode->state.services[i].uuid.uuid == currentNode->state.uuid.uuid &&
            currentNode->state.services[i].uuid.type == currentNode->state.uuid.type)
        {
            p_tempService = &currentNode->state.services[i];
            break;
        }
    }

    if (p_tempService == nullptr)
    {
        dbEvt.type = FruityHal::BleGattDBDiscoveryEventType::SERVICE_NOT_FOUND;
    }
    else
    {
        dbEvt.type = FruityHal::BleGattDBDiscoveryEventType::COMPLETE;
        dbEvt.serviceUUID.uuid = currentNode->state.uuid.uuid;
        dbEvt.serviceUUID.type = currentNode->state.uuid.type;
        dbEvt.charateristicsCount = p_tempService->charCount;
        for (int i = 0; i < p_tempService->charCount; i++)
        {
            dbEvt.dbChar[i].handleValue = p_tempService->charateristics[i].handle;
            dbEvt.dbChar[i].charUUID.uuid = p_tempService->charateristics[i].uuid.uuid;
            dbEvt.dbChar[i].charUUID.type = p_tempService->charateristics[i].uuid.type;
            dbEvt.dbChar[i].cccdHandle = p_tempService->charateristics[i].cccd_handle;
        }
    }

    GS->dbDiscoveryHandler(&dbEvt);
}

#ifndef GITHUB_RELEASE
void CherrySim::SimulateClcData() {

    ClcModule* clcMod = (ClcModule*)currentNode->gs.node.GetModuleById(ModuleId::CLC_MODULE);
    if (ShouldSimIvTrigger(30000) && clcMod != nullptr && currentNode->gs.uartEventHandler != nullptr) {
        currentNode->clcMock.SendPeriodicData();
    }
}
#endif //GITHUB_RELEASE

void CherrySim::SimulateMovement()
{
    if (currentNode->animation.IsStarted())
    {
        auto pos = currentNode->animation.Evaluate(simState.simTimeMs);
        SetPosition(currentNode->index, pos.x, pos.y, pos.z);
    }
}

//################################## Battery Usage Simulation #############################
// Checks the features that are activated on a node and estimates the battery usage
//#########################################################################################

void CherrySim::SimulateBatteryUsage()
{
    //Have a look at: https://devzone.nordicsemi.com/b/blog/posts/nrf51-current-consumption-for-common-scenarios
    //or: https://github.com/mwaylabs/fruitymesh/wiki/Battery-Consumption

    //TODO: Make measurements and check online resources to find out current consumption
    //TODO: provide values for nrf52
    //TODO: Take into account what changes at 0 dbm and at +4dmb
    //TODO: Take into account that each sent packet consumes power in addition to the connectionInterval itself
    //TODO: If too much activity happens at the same time, the scheduler will postpone tasks and use less energy (also relevant for connection / advertising, etc,... performance)

    u32 divider = 1000UL / simConfig.simTickDurationMs;

    //All current usages are given as nano ampere per step
    u32 idleDraw = 10 * 1000 / divider; //10 uA idle current usage
    u32 ledUsage = 10 * 1000 * 1000 / divider; //10 mA led on usage
    u32 adv20Ms = 800 * 1000 / divider; //imaginary value for 20ms advertising
    u32 adv100Ms = 220 * 1000 / divider; //220 uA advertising at 100ms interval
    u32 adv200Ms = 110 * 1000 / divider; //110 uA advertising at 200ms interval
    u32 adv400Ms = 84 * 1000 / divider; //84 uA advertising at 400ms interval
    u32 adv1000Ms = 70 * 1000 / divider; //70 uA advertising at 1000ms interval
    u32 adv2000Ms = 50 * 1000 / divider; //50 uA advertising at 2000ms interval
    u32 adv4000Ms = 63 * 1000 / divider; //63 uA advertising at 4000ms interval
    u32 adv30000Ms = 30 * 1000 / divider; //30 uA advertising at 30000ms interval
    u32 conn100Ms = 130 * 1000 / divider; //70 uA per connection at 100ms interval
    u32 conn7_5Ms = 1000 * 1000 / divider; //1000 uA per connection at 7.5ms interval (imaginary value)
    u32 conn10Ms = 900 * 1000 / divider; //900 uA per connection at 10ms interval (imaginary value)
    u32 conn15Ms = 750 * 1000 / divider; //750 uA per connection at 15ms interval (imaginary value)
    u32 conn30Ms = 600 * 1000 / divider; //600 uA per connection at 30ms interval (imaginary value)
    u32 conn90Ms = 300 * 1000 / divider; //300 uA per connection at 90ms interval (imaginary value)

    //Other values
    u32 scanUsage = 11 * 1000 * 1000 / divider; //11mA for scanning at 100% duty cycle


    //Next, we add up all the numbers for all active features
    currentNode->nanoAmperePerMsTotal += idleDraw;

    if (currentNode->ledOn) {
        currentNode->nanoAmperePerMsTotal += ledUsage;
    }

    if (currentNode->state.advertisingActive) {
        if (currentNode->state.advertisingIntervalMs == 20) currentNode->nanoAmperePerMsTotal += adv20Ms;
        else if (currentNode->state.advertisingIntervalMs == 100) currentNode->nanoAmperePerMsTotal += adv100Ms;
        else if (currentNode->state.advertisingIntervalMs == 200) currentNode->nanoAmperePerMsTotal += adv200Ms;
        else if (currentNode->state.advertisingIntervalMs == 400) currentNode->nanoAmperePerMsTotal += adv400Ms;
        else if (currentNode->state.advertisingIntervalMs == 1000) currentNode->nanoAmperePerMsTotal += adv1000Ms;
        else if (currentNode->state.advertisingIntervalMs == 4000) currentNode->nanoAmperePerMsTotal += adv4000Ms;
        else if (currentNode->state.advertisingIntervalMs == 2000) currentNode->nanoAmperePerMsTotal += adv2000Ms;
        else if (currentNode->state.advertisingIntervalMs == 30000) currentNode->nanoAmperePerMsTotal += adv30000Ms;
        else {
            printf("Adv interval not integrated into battery test, %u" EOL, (u32)currentNode->state.advertisingIntervalMs);
            SIMEXCEPTION(IllegalAdvertismentStateException);
        }
    }

    if (currentNode->state.scanningActive) {
        u32 scanDutyCycle = currentNode->state.scanWindowMs * 1000UL / currentNode->state.scanIntervalMs;
        u32 usagePerStepWithGivenDutyCycle = scanUsage * scanDutyCycle / 1000;
        currentNode->nanoAmperePerMsTotal += usagePerStepWithGivenDutyCycle;
    }

    if (currentNode->state.connectingActive) {
        u32 scanDutyCycle = currentNode->state.connectingWindowMs * 1000UL / currentNode->state.connectingIntervalMs;
        u32 usagePerStepWithGivenDutyCycle = scanUsage * scanDutyCycle / 1000;
        currentNode->nanoAmperePerMsTotal += usagePerStepWithGivenDutyCycle;
    }

    for (u32 i = 0; i < currentNode->state.configuredTotalConnectionCount; i++) {
        SoftdeviceConnection* conn = currentNode->state.connections + i;
        if (conn->connectionActive) {
            if (conn->connectionInterval == 100) {
                currentNode->nanoAmperePerMsTotal += conn100Ms;
            }
            else if (conn->connectionInterval == 7) {
                currentNode->nanoAmperePerMsTotal += conn7_5Ms;
            }
            else if (conn->connectionInterval == 10) {
                currentNode->nanoAmperePerMsTotal += conn10Ms;
            }
            else if (conn->connectionInterval == 15) {
                currentNode->nanoAmperePerMsTotal += conn15Ms;
            }
            else if (conn->connectionInterval == 30) {
                currentNode->nanoAmperePerMsTotal += conn30Ms;
            }
            else if (conn->connectionInterval == 90) {
                currentNode->nanoAmperePerMsTotal += conn90Ms;
            }
            else {
                printf("Conn interval not integrated into battery test" EOL);
                SIMEXCEPTION(IllegalStateException);
            }
        }
    }

    //TODO: Add up current for connections according to connectionIntervals of each connection
}

//################################## Other Simulation #####################################
// Simulation of other parts
//#########################################################################################

//Simulates the timer events
extern "C" void app_timer_handler(void * p_context); //Get access to ap_timer_handler to trigger it
void CherrySim::SimulateTimer() {
    //Advance time of this node
    currentNode->state.timeMs += simConfig.simTickDurationMs;

    if (ShouldSimIvTrigger(100L * MAIN_TIMER_TICK * 10 / ticksPerSecond)) {
        app_timer_handler(nullptr);
    }
}

void CherrySim::SimulateWatchDog()
{
    if (simConfig.simulateWatchdog) {
        if (currentNode->state.timeMs - currentNode->lastWatchdogFeedTime > currentNode->watchdogTimeout)
        {
            SIMEXCEPTION(WatchdogTriggeredException);
            ResetCurrentNode(RebootReason::WATCHDOG);
        }
    }
}

struct InterruptGuard
{
    //Protects us against interrupting inside an interrupt using RAII.

    static inline bool currentlyInAnInterrupt = false;

    InterruptGuard() {
        currentlyInAnInterrupt = true;
    }

    ~InterruptGuard() {
        currentlyInAnInterrupt = false;
    }
};

void CherrySim::SimulateInterrupts()
{
    if (currentNode->interruptQueue.size() > 0 && InterruptGuard::currentlyInAnInterrupt == false)
    {
        if (PSRNG(simConfig.interruptProbability))
        {
            InterruptGuard guard;

            u32 pin = currentNode->interruptQueue.front();
            currentNode->interruptQueue.pop();

            if (cherrySimInstance->currentNode->gpioInitializedPins.find(pin) != cherrySimInstance->currentNode->gpioInitializedPins.end()) {
                InterruptSettings &settings = currentNode->gpioInitializedPins[pin];
                if (settings.isEnabled)
                {
                    //TODO The polarity is currently unused by every interrupt handler, thus we just pass 0 here.
                    settings.handler(pin, 0);
                }
            }
        }
    }
}

//################################## Validity Checks ######################################
// This section contains methods that check the meshing for validity
//#########################################################################################

//This method can be used to check the clustering for potential errors after each simulation step
//Once a clustering error occurs, simply enable checking through the simConfig. Whenever a potential
//clustering issue occurs, it will print a warning and break in the debugger. It might however generate
//false positives. A tip for using this: Enable it and let it run through a simulation without node terminal output.
//Check the log and see at which point the cluster warnings occur for each timestep. This is, when the clustering broke.
//See, which nodes produce the issue and filter the log for these two nodes, then analyze the logs around this timestep.
//
//It tries to predict the outcome of the current cluster configuration if all packets reach their recipient
//It acceses the current cluster size of each node and pulls the clusterInfoUpdatePackets from various places
//in the pipeline and then predicts their flow through the current mesh
//If the clustering is deemed to be wrong, it will print a warning
//
//TODO: This method generates some false positives because it is not feature complete:
//      We should extend the method so that it also predicts what happens if one partner has already destroyed
//      a MeshConnection while the other still thinks it is in the Handshaked state. In these cases, the MeshConnectionBond
//      will not have a partnerConnection. It is however quite complicated to predict how the partner will react on the lost
//      connection, but this would allow us to run the check for all clusterings in the automated test.
void CherrySim::CheckMeshingConsistency()
{
    //Reset all validity information
    u32 numNoneAssetNodes = GetTotalNodes() - GetAssetNodes();
    for (u32 i = 0; i < numNoneAssetNodes; i++)
    {
        nodes[i].state.validityClusterSize = 0;

        for (u32 k = 0; k < currentNode->state.configuredTotalConnectionCount; k++) {
            nodes[i].state.connections[k].validityClusterSizeToSend = 0;
        }
    }

    //Grab information from the current clusterSize
    for (u32 i = 0; i < numNoneAssetNodes; i++)
    {
        nodes[i].state.validityClusterSize = nodes[i].gs.node.clusterSize;

        //printf("NODE %u has clusterSize %d" EOL, nodes[i].id, nodes[i].gs.node.clusterSize);
    }

    //Grab information from the currentClusterInfoUpdatePacket
    for (u32 i = 0; i < numNoneAssetNodes; i++)
    {
        NodeEntry* node = &nodes[i];

        MeshConnections conns = node->gs.cm.GetMeshConnections(ConnectionDirection::INVALID);
        for (int k = 0; k < conns.count; k++) {
            MeshConnection* conn = conns.handles[k].GetConnection();

            if (conn->HandshakeDone()) {
                ConnPacketClusterInfoUpdate* packet = (ConnPacketClusterInfoUpdate*)&(conn->currentClusterInfoUpdatePacket);

                conn->validityClusterUpdatesToSend += packet->payload.clusterSizeChange;

                if (packet->payload.clusterSizeChange != 0) {
                    //printf("NODE %u to %u: Buffered Packet with change %d" EOL, node->id, conn->partnerId, packet->payload.clusterSizeChange);
                }
            }
        }
    }

    //Grab information from the HighPrioQueue
    for (u32 i = 0; i < numNoneAssetNodes; i++)
    {
        NodeEntry* node = &nodes[i];

        MeshConnections conns = node->gs.cm.GetMeshConnections(ConnectionDirection::INVALID);
        for (u32 k = 0; k < conns.count; k++)
        {
            MeshConnection* conn = conns.handles[k].GetConnection();

            if (conn->HandshakeDone()) {
                PacketQueue* queue = &(conn->packetSendQueueHighPrio);
                for (u32 m = 0; m < queue->_numElements; m++) {
                    SizedData data = queue->PeekNext(m);
                    if (data.length >= SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED + SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE) {
                        BaseConnectionSendDataPacked* sendInfo = (BaseConnectionSendDataPacked*)data.data;
                        ConnPacketClusterInfoUpdate* packet = (ConnPacketClusterInfoUpdate*)(data.data + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED);
                        if (packet->header.messageType == MessageType::CLUSTER_INFO_UPDATE)
                        {
                            //We must check if this packet was queued in the Softdevice already, if yes, we must not count it twice
                            //It will be removed after the Transmission Success was delivered from the SoftDevice
                            if (sendInfo->sendHandle == PACKET_QUEUED_HANDLE_NOT_QUEUED_IN_SD)
                            {
                                conn->validityClusterUpdatesToSend += packet->payload.clusterSizeChange;
                            }

                            //TODO: If the simulator will one day support sending a packet but not acknowledging that to the sending node during a connection drop,
                            //The same packet will be sent again (after reestablishing the gap connection) but will be thrown away by the receiving node,
                            //then we have to implement this check as well.

                            if (packet->payload.clusterSizeChange != 0) {
                                //printf("NODE %u to %u: Change in HighPrio %d" EOL, node->id, conn->partnerId, packet->payload.clusterSizeChange);
                            }
                        }
                    }
                }
            }
        }
    }

    //Grab information from SoftDevice send buffers
    //(Only reliable buffers as this is the place where cluster update packets are)
    for (u32 i = 0; i < numNoneAssetNodes; i++)
    {
        NodeEntry* node = &nodes[i];

        for (u32 k = 0; k < currentNode->state.configuredTotalConnectionCount; k++)
        {
            SoftdeviceConnection* sc = &(node->state.connections[k]);
            if (!sc->connectionActive) continue;

            SoftDeviceBufferedPacket* sp = &(sc->reliableBuffers[0]);

            if (sp->sender != nullptr)
            {
                ConnPacketHeader* header = (ConnPacketHeader*)sp->data;

                if (header->messageType == MessageType::CLUSTER_INFO_UPDATE)
                {
                    ConnPacketClusterInfoUpdate* packet = (ConnPacketClusterInfoUpdate*)header;

                    BaseConnection* bc = node->gs.cm.GetRawConnectionFromHandle(sc->connectionHandle);

                    //Save the sent clusterUpdate as part of the connection if the MeshConnection still exists and is handshaked
                    if (bc != nullptr && bc->connectionType == ConnectionType::FRUITYMESH && bc->HandshakeDone())
                    {
                        MeshConnection* conn = (MeshConnection*)bc;
                        conn->validityClusterUpdatesToSend += packet->payload.clusterSizeChange;

                        if (packet->payload.clusterSizeChange != 0) {
                            //printf("NODE %u TO %u: SD Send Buffer size change %u" EOL, node->id, bc->partnerId, packet->payload.clusterSizeChange);
                        }
                    }
                }
            }
        }
    }

    //Grab information from the SoftDevice event queue
    for (u32 i = 0; i < numNoneAssetNodes; i++)
    {
        NodeEntry* node = &nodes[i];

        for (u32 k = 0; k < node->eventQueue.size(); k++)
        {
            simBleEvent bleEvent = node->eventQueue[k];
            if (bleEvent.bleEvent.header.evt_id == BLE_GATTS_EVT_WRITE) {
                ble_gatts_evt_t* gattsEvt = (ble_gatts_evt_t*)&bleEvent.bleEvent.evt;

                ConnPacketHeader* header = (ConnPacketHeader*)gattsEvt->params.write.data;

                if (header->messageType == MessageType::CLUSTER_INFO_UPDATE)
                {
                    ConnPacketClusterInfoUpdate* packet = (ConnPacketClusterInfoUpdate*)header;

                    BaseConnection* bc = node->gs.cm.GetRawConnectionFromHandle(gattsEvt->conn_handle);

                    //Save the sent clusterUpdate as part of the connection if the MeshConnection still exists and is handshaked
                    if (bc != nullptr && bc->connectionType == ConnectionType::FRUITYMESH && bc->HandshakeDone())
                    {
                        MeshConnection* conn = (MeshConnection*)bc;
                        conn->validityClusterUpdatesReceived += packet->payload.clusterSizeChange;
                    }

                    if (packet->payload.clusterSizeChange != 0) {
                        //printf("NODE %u TO %u: Event Queue size change %u" EOL, bc->partnerId, node->id, packet->payload.clusterSizeChange);
                    }
                }

                //TODO: Check characteristic if it's a mesh image and which packet, ...
            }
        }
    }

    //Go through all nodes and its connections and recursively propagate the clusterUpdates
    for (u32 i = 0; i < numNoneAssetNodes; i++)
    {
        NodeEntry* node = &nodes[i];
        DetermineClusterSizeAndPropagateClusterUpdates(node, nullptr);

    }

    //For each cluster, calculate the totals for each node and check if they match with the clusterSize
    for (u32 i = 0; i < numNoneAssetNodes; i++)
    {
        NodeEntry* node = &nodes[i];
        ClusterSize realClusterSize = DetermineClusterSizeAndPropagateClusterUpdates(node, nullptr);

        if (realClusterSize != node->state.validityClusterSize) {
            printf("NODE %d has a real cluster size of %d and predicted size of %d, reported cluster size %d" EOL, node->id, realClusterSize, node->state.validityClusterSize, nodes[i].gs.node.clusterSize);
            printf("-------- POTENTIAL CLUSTERING MISMATCH -----------" EOL);
            //std::cout << "Press Enter to Continue";
            //std::cin.ignore();
        }
    }
}

typedef struct MeshConnectionBond
{
    MeshConnection* startConnection;
    MeshConnection* partnerConnection;
} MeshConnectionBond;

//This method is used for determining the two MeshConnections that link two nodes
//the connections will only be returned if they are handshaked
MeshConnectionBond findBond(NodeEntry* startNode, NodeEntry* partnerNode)
{
    //TODO: This currently uses nodeIds for matching, but should use something more safe that cannot change

    if (startNode == nullptr || partnerNode == nullptr)    SIMEXCEPTIONFORCE(IllegalStateException);

    MeshConnectionBond bond = { nullptr, nullptr };

    //Find the connection on the startNode
    MeshConnections conns = startNode->gs.cm.GetMeshConnections(ConnectionDirection::INVALID);
    for (int i = 0; i < conns.count; i++) {
        if (conns.handles[i].IsHandshakeDone() && conns.handles[i].GetPartnerId() == partnerNode->id) {
            bond.startConnection = conns.handles[i].GetConnection();
        }
    }

    //Find the connection on the partnerNode
    MeshConnections partnerConns = partnerNode->gs.cm.GetMeshConnections(ConnectionDirection::INVALID);
    for (int i = 0; i < partnerConns.count; i++) {
        if (partnerConns.handles[i].IsHandshakeDone() && partnerConns.handles[i].GetPartnerId() == startNode->id) {
            bond.partnerConnection = partnerConns.handles[i].GetConnection();
        }
    }

    return bond;
}

//This will recursively go along all connections and add up the nodes in this cluster
//It will also propagate the cluster size changes along the route
ClusterSize CherrySim::DetermineClusterSizeAndPropagateClusterUpdates(NodeEntry* node, NodeEntry* startNode)
{
    ClusterSize size = 1;

    //FIXME: This propagation should be written with only FruityMesh connections in mind
    //We should propagate along handshaked fruitymesh connections, not matter if a softdevice connection exists or not
    //We should also save the validity cluster values and stuff as part of the fruitymesh connections and not as part of the softdevice connections
    //Above, we can simply ignore the softdevice buffers if there is no gap connection at the moment as the fruitymesh implementation
    //must make sure that this works at all times.

    i16 validitySizeToAdd = 0;

    //Look for the MeshConnection on both sides that connect the startNode and the node
    //Next, check what updates we received from the startNode and clear them
    MeshConnectionBond bond = { nullptr, nullptr };
    if (startNode != nullptr) {
        bond = findBond(startNode, node);

        if (bond.startConnection != nullptr) {
            validitySizeToAdd += bond.startConnection->validityClusterUpdatesToSend;
            bond.startConnection->validityClusterUpdatesToSend = 0;
        }

        if (bond.partnerConnection != nullptr) {
            validitySizeToAdd += bond.partnerConnection->validityClusterUpdatesReceived;
            bond.partnerConnection->validityClusterUpdatesReceived = 0;
        }
    }

    //Add the received updates to our node
    node->state.validityClusterSize += validitySizeToAdd;

    //Go through all connections to propagate the size changes
    MeshConnections conns = node->gs.cm.GetMeshConnections(ConnectionDirection::INVALID);
    for (int i = 0; i < conns.count; i++)
    {
        MeshConnection* conn = conns.handles[i].GetConnection();

        //Make sure that we do not go back the connection where we came from
        if (!conn->HandshakeDone()) continue;
        if (conn == bond.partnerConnection) continue;

        //Send the size update over this connection as well to propagate it along the route
        conn->validityClusterUpdatesToSend += validitySizeToAdd;

        //Calculate the cluster size and propagate size changes further
        NodeEntry* nextPartner = FindNodeById(conn->partnerId);
        size += DetermineClusterSizeAndPropagateClusterUpdates(nextPartner, node);
    }

    return size;
}

//################################## Configuration Management #############################
// 
//#########################################################################################

FeaturesetPointers* getFeaturesetPointers()
{
    if (cherrySimInstance->currentNode->featuresetPointers == nullptr)
    {
        const std::string& configurationName = cherrySimInstance->currentNode->nodeConfiguration;

        auto entry = cherrySimInstance->featuresetPointers.find(configurationName);
        if (entry == cherrySimInstance->featuresetPointers.end())
        {
            printf("Featureset %s was not found!", configurationName.c_str());
            SIMEXCEPTION(IllegalStateException); //Featureset configuration not found
            return nullptr;
        }
        cherrySimInstance->currentNode->featuresetPointers = &(entry->second);
    }
    return cherrySimInstance->currentNode->featuresetPointers;
}

//This function is called by every module's SetToDefaults function
//It can override the code defaults with vendor specific configurations
void SetFeaturesetConfiguration_CherrySim(ModuleConfiguration* config, void* module)
{
    getFeaturesetPointers()->setFeaturesetConfigurationPtr(config, module);
}

void SetFeaturesetConfigurationVendor_CherrySim(VendorModuleConfiguration* config, void* module)
{
    getFeaturesetPointers()->setFeaturesetConfigurationVendorPtr(config, module);
}

void SetBoardConfiguration_CherrySim(BoardConfiguration* config)
{
    getFeaturesetPointers()->setBoardConfigurationPtr(config);
}

uint32_t InitializeModules_CherrySim(bool createModule)
{
    return getFeaturesetPointers()->initializeModulesPtr(createModule);
}

DeviceType GetDeviceType_CherrySim()
{
    return getFeaturesetPointers()->getDeviceTypePtr();
}

Chipset GetChipset_CherrySim()
{
    return getFeaturesetPointers()->getChipsetPtr();
}

FeatureSetGroup GetFeatureSetGroup_CherrySim()
{
    return getFeaturesetPointers()->getFeaturesetGroupPtr();
}

u32 GetWatchdogTimeout_CherrySim()
{
    return getFeaturesetPointers()->getWatchdogTimeout();
}

u32 GetWatchdogTimeoutSafeBoot_CherrySim()
{
    return getFeaturesetPointers()->getWatchdogTimeoutSafeBoot();
}

//This function is responsible for setting all the BLE Stack dependent configurations according to the datasheet of the ble stack used
void CherrySim::SetBleStack(NodeEntry* node)
{
    //The nRF52 S132
    if (node->bleStackType == BleStackType::NRF_SD_132_ANY) {
        node->bleStackMaxPeripheralConnections = 10;
        node->bleStackMaxCentralConnections = 10;
        node->bleStackMaxTotalConnections = 10;
    }
    //Other stacks not currently supported
    else {
        SIMEXCEPTION(IllegalArgumentException);
    }
}

//################################## Helper functions #####################################
// 
//#########################################################################################

bool CherrySim::IsClusteringDone()
{
    u32 numNoneAssetNodes = GetTotalNodes() - GetAssetNodes();
    std::set<ClusterId> clusterIds;
    for (u32 i = 0; i < numNoneAssetNodes; i++) {
        clusterIds.insert(nodes[i].gs.node.clusterId);
        if ((u32)nodes[i].gs.node.clusterSize != numNoneAssetNodes) {
            return false;
        }
    }
    return clusterIds.size() == 1;
}

struct ClusterNetworkPair {
    u32 clusterId;
    u32 networkId;

    bool operator==(const ClusterNetworkPair& other) {
        return this->clusterId == other.clusterId && this->networkId == other.networkId;
    }
};

bool operator<(const ClusterNetworkPair& a, const ClusterNetworkPair& b) {
    if (a.clusterId < b.clusterId) {
        return true;
    }
    else if (a.clusterId > b.clusterId) {
        return false;
    }
    else {
        if (a.networkId < b.networkId) {
            return true;
        }
        else {
            return false;
        }
    }
};
bool CherrySim::IsClusteringDoneWithDifferentNetworkIds()
{
    std::set<u32> networkIds;
    std::set<ClusterNetworkPair> clusterIds;
    u32 numNoneAssetNodes = GetTotalNodes() - GetAssetNodes();
    for (u32 i = 0; i < numNoneAssetNodes; i++)
    {
        networkIds.insert(nodes[i].gs.node.configuration.networkId);
        clusterIds.insert({ nodes[i].gs.node.clusterId, nodes[i].gs.node.configuration.networkId });
    }

    return networkIds.size() == clusterIds.size();
}

bool CherrySim::IsClusteringDoneWithExpectedNumberOfClusters(u32 clusterAmount)
{
    std::set<ClusterNetworkPair> clusterIds;
    u32 numNoneAssetNodes = GetTotalNodes() - GetAssetNodes();
    for (u32 i = 0; i < numNoneAssetNodes; i++)
    {
        clusterIds.insert({ nodes[i].gs.node.clusterId, nodes[i].gs.node.configuration.networkId });
    }

    return clusterAmount == clusterIds.size();
}

//Allows us to activate or deactivate a terminal of a node
void CherrySim::ChooseSimulatorTerminal() {
    if (!currentNode->state.initialized) return;

    //Enable or disable terminal based on the currently set terminal id
    if (simConfig.terminalId == 0 || simConfig.terminalId == currentNode->id) {
        currentNode->gs.terminal.terminalIsInitialized = true;
    }
    else {
        currentNode->gs.terminal.terminalIsInitialized = false;
    }
}

//Takes an RSSI and the calibrated RSSI at 1m and returns the distance in m
float CherrySim::RssiToDistance(int rssi, int calibratedRssi) {
    float distanceInMetre = (float)pow(10, (calibratedRssi - rssi) / (10 * N));
    return distanceInMetre;
}

float CherrySim::GetDistanceBetween(const NodeEntry* nodeA, const NodeEntry* nodeB) {
    float distX = std::abs(nodeA->x - nodeB->x) * simConfig.mapWidthInMeters;
    float distY = std::abs(nodeA->y - nodeB->y) * simConfig.mapHeightInMeters;
    float distZ = std::abs(nodeA->z - nodeB->z) * simConfig.mapElevationInMeters;
    float dist = sqrt(
        distX * distX
      + distY * distY
      + distZ * distZ
    );

    return dist;
}

float CherrySim::GetReceptionRssi(const NodeEntry* sender, const NodeEntry* receiver) {
    return GetReceptionRssi(sender, receiver, sender->gs.boardconf.configuration.calibratedTX, Conf::defaultDBmTX);
}

float CherrySim::GetReceptionRssi(const NodeEntry* sender, const NodeEntry* receiver, int8_t senderDbmTx, int8_t senderCalibratedTx) {
    const float rssi = GetReceptionRssiNoNoise(sender, receiver, senderDbmTx, senderCalibratedTx);
    if (!simConfig.rssiNoise)
    {
        return rssi;
    }
    const float randomNoise = (float)cherrySimInstance->simState.rnd.NextU32(0, 7) - 3.f;
    return rssi + randomNoise;
}

float CherrySim::GetReceptionRssiNoNoise(const NodeEntry* sender, const NodeEntry* receiver) {
    return GetReceptionRssiNoNoise(sender, receiver, sender->gs.boardconf.configuration.calibratedTX, Conf::defaultDBmTX);
}

float CherrySim::GetReceptionRssiNoNoise(const NodeEntry* sender, const NodeEntry* receiver, int8_t senderDbmTx, int8_t senderCalibratedTx) {
    // If either the sender or the receiver has the other marked as as a impossibleConnection, the rssi is set to a unconnectable level.
    if (sender->impossibleConnection.size() > 0
        || receiver->impossibleConnection.size() > 0)
    {
        if (std::find(sender->impossibleConnection.begin(), sender->impossibleConnection.end(), receiver->index) != sender->impossibleConnection.end()
            || std::find(receiver->impossibleConnection.begin(), receiver->impossibleConnection.end(), sender->index) != receiver->impossibleConnection.end())
        {
            return -10000;
        }
    }
    const float dist = GetDistanceBetween(sender, receiver);
    const float rssi = (senderDbmTx + senderCalibratedTx) - log10(dist) * 10 * N;
    return rssi;
}

uint32_t CherrySim::CalculateReceptionProbability(const NodeEntry* sendingNode, const NodeEntry* receivingNode) {
    //TODO: Add some randomness and use a function to do the mapping
    float rssi = GetReceptionRssi(sendingNode, receivingNode);

         if (rssi > -60) return simConfig.receptionProbabilityVeryClose;
    else if (rssi > -80) return simConfig.receptionProbabilityClose;
    else if (rssi > -85) return simConfig.receptionProbabilityFar;
    else if (rssi > -90) return simConfig.receptionProbabilityVeryFar;
    else return 0;
}

SoftdeviceConnection* CherrySim::FindConnectionByHandle(NodeEntry* node, int connectionHandle) {
    for (u32 i = 0; i < node->state.configuredTotalConnectionCount; i++) {
        if (node->state.connections[i].connectionActive && node->state.connections[i].connectionHandle == connectionHandle) {
            return &node->state.connections[i];
        }
    }
    return nullptr;
}

NodeEntry* CherrySim::FindNodeById(int id) {
    for (u32 i = 0; i < GetTotalNodes(); i++) {
        if (nodes[i].id == id) {
            return &nodes[i];
        }
    }
    return nullptr;
}

u8 CherrySim::GetNumSimConnections(const NodeEntry* node) {
    u8 count = 0;
    for (u32 i = 0; i < node->state.configuredTotalConnectionCount; i++) {
        if (node->state.connections[i].connectionActive) {
            count++;
        }
    }
    return count;
}

void CherrySim::FakeVersionOfCurrentNode(u32 version)
{
    currentNode->fakeDfuVersion = version;
}

void CherrySim::EnableTagForAll(const char * tag)
{
    for (u32 i = 0; i < GetTotalNodes(); i++)
    {
        nodes[i].gs.logger.EnableTag(tag);
    }
}

void CherrySim::DisableTagForAll(const char * tag)
{
    for (u32 i = 0; i < GetTotalNodes(); i++)
    {
        nodes[i].gs.logger.DisableTag(tag);
    }
}

void CherrySim::SetPosition(u32 nodeIndex, float x, float y, float z)
{
    if (nodes[nodeIndex].x != x || nodes[nodeIndex].y != y || nodes[nodeIndex].z != z)
    {
        nodes[nodeIndex].x = x;
        nodes[nodeIndex].y = y;
        nodes[nodeIndex].z = z;
        nodes[nodeIndex].lastMovementSimTimeMs = simState.simTimeMs;
    }
}

void CherrySim::AddPosition(u32 nodeIndex, float x, float y, float z)
{
    if (nodes[nodeIndex].x != 0 || nodes[nodeIndex].y != 0 || nodes[nodeIndex].z != 0)
    {
        nodes[nodeIndex].x += x;
        nodes[nodeIndex].y += y;
        nodes[nodeIndex].z += z;
        nodes[nodeIndex].lastMovementSimTimeMs = simState.simTimeMs;
    }
}


void CherrySim::AddPacketToStats(PacketStat* statArray, PacketStat* packet)
{
    if (!simConfig.enableSimStatistics) return;
    if (packet->messageType == MessageType::INVALID) return;

    //Check if we can add it to a previous entry
    PacketStat* emptySlot = nullptr;
    for (u32 i = 0; i < PACKET_STAT_SIZE; i++) {
        PacketStat* entry = statArray + i;

        if (memcmp(packet, entry, packetStatCompareBytes) == 0) {
            entry->count += packet->count;
            return;
        }

        if (!emptySlot && entry->messageType == MessageType::INVALID) emptySlot = entry;
    }

    //If we did not yet return, we have not found an empty slot
    //If we do not have an empty slot for logging, we should increase our PacketStat array size or check if sth. went wrong
    if (!emptySlot) SIMEXCEPTIONFORCE(PacketStatBufferSizeNotEnough);

    *emptySlot = *packet;
}

//Allows us to put a packet into the packet statistics. It will count all similar packets in slots depending on the messageType
//Given stat array has to be of size PACKET_STAT_SIZE
//TODO: This must only be called for unencrypted connections that send mesh-compatible packets
//TODO: Should also be used to check what kind of messages a node generates
void CherrySim::AddMessageToStats(PacketStat* statArray, u8* message, u16 messageLength)
{
    if (!simConfig.enableSimStatistics) return;

    ConnPacketSplitHeader* splitHeader = (ConnPacketSplitHeader*)message;
    ConnPacketHeader* header = nullptr;
    ConnPacketModule* moduleHeader = nullptr;

    PacketStat packet;

    //Check if it is the first part of a split message or not
    if (splitHeader->splitMessageType == MessageType::SPLIT_WRITE_CMD && splitHeader->splitCounter == 0) {
        packet.isSplit = true;
        header = (ConnPacketHeader*)(message + SIZEOF_CONN_PACKET_SPLIT_HEADER);
    }
    else if (splitHeader->splitMessageType == MessageType::SPLIT_WRITE_CMD || splitHeader->splitMessageType == MessageType::SPLIT_WRITE_CMD_END) {
        //Do nothing for now as we only count the first part
        return;
        //A normal not split packet
    }
    else {
        header = (ConnPacketHeader*)message;
        packet.isSplit = false;
    }

    //Fill in basic packet info
    packet.messageType = header->messageType;
    packet.count = 1;

    //Fill in additional info if we have a module message
    if (packet.messageType >= MessageType::MODULE_CONFIG && packet.messageType <= MessageType::COMPONENT_SENSE) {
        moduleHeader = (ConnPacketModule*)header;
        packet.moduleId = Utility::GetWrappedModuleId(moduleHeader->moduleId);
        packet.actionType = moduleHeader->actionType;
    }

    //Add the packet to our stat array
    AddPacketToStats(statArray, &packet);
}

void CherrySim::PrintPacketStats(NodeId nodeId, const char* statId)
{
    if (!simConfig.enableSimStatistics) return;

    PacketStat* stat = nullptr;
    PacketStat sumStat[PACKET_STAT_SIZE];
    u32 numNoneAssetNodes = GetTotalNodes() - GetAssetNodes();
    //We must sum up all stat packets of all nodes to get a stat that covers all nodes
    if (nodeId == 0) {
        stat = sumStat;

        for (u32 i = 0; i < numNoneAssetNodes; i++) {
            for (u32 j = 0; j < PACKET_STAT_SIZE; j++) {
                if (strcmp("SENT", statId) == 0) AddPacketToStats(sumStat, nodes[i].sentPackets + j);
                if (strcmp("ROUTED", statId) == 0) AddPacketToStats(sumStat, nodes[i].routedPackets + j);
            }
        }
    }
    //We simply select the stat from the given nodeId
    else {
        NodeEntry* node = FindNodeById(nodeId);
        if (strcmp("SENT", statId) == 0) stat = node->sentPackets;
        if (strcmp("ROUTED", statId) == 0) stat = node->routedPackets;
    }

    //Print everything
    printf(">----------------------------------------------------<" EOL);
    printf("Message statistics for packets %s on node %u" EOL, statId, nodeId);
    printf("" EOL);

    for (u32 i = 0; i < PACKET_STAT_SIZE; i++)
    {
        PacketStat* entry = stat + i;

        if (entry->messageType != MessageType::INVALID) {
            if (entry->messageType >= MessageType::MODULE_CONFIG && entry->messageType <= MessageType::COMPONENT_SENSE) {
                printf("%u :: mt:%u (mId:%u, at:%u%s)" EOL, entry->count, (u32)entry->messageType, (u32)entry->moduleId, (u32)entry->actionType, entry->isSplit ? ", SPLIT" : "");
            }
            else {
                printf("%u :: mt:%u %s" EOL, entry->count, (u32)entry->messageType, entry->isSplit ? "(SPLIT)" : "");
            }
        }
    }

    printf(">----------------------------------------------------<" EOL);
}

#pragma warning( pop )

#endif
/**
 *@}
 **/

MoveAnimation & CherrySim::AnimationGet(const std::string & name)
{
    if (!AnimationExists(name))
    {
        //Animation does not exist!
        SIMEXCEPTIONFORCE(IllegalStateException);
    }
    else
    {
        return loadedMoveAnimations[name];
    }
}

void CherrySim::AnimationCreate(const std::string & name)
{
    if (AnimationExists(name))
    {
        //Animation does already exist!
        SIMEXCEPTIONFORCE(IllegalStateException);
    }
    MoveAnimation animation;
    animation.SetName(name);
    loadedMoveAnimations.insert({ name, animation });
}

bool CherrySim::AnimationExists(const std::string & name) const
{
    return loadedMoveAnimations.find(name) != loadedMoveAnimations.end();
}

void CherrySim::AnimationRemove(const std::string & name)
{
    if (!AnimationExists(name))
    {
        SIMEXCEPTIONFORCE(IllegalStateException);
    }
    loadedMoveAnimations.erase(name);
}

void CherrySim::AnimationSetDefaultType(const std::string&name, MoveAnimationType type)
{
    AnimationGet(name).SetDefaultType(type);
}

void CherrySim::AnimationAddKeypoint(const std::string & name, float x, float y, float z, float duration)
{
    AnimationGet(name).AddKeyPoint(x, y, z, duration);
}

void CherrySim::AnimationAddKeypoint(const std::string & name, float x, float y, float z, float duration, MoveAnimationType type)
{
    AnimationGet(name).AddKeyPoint(x, y, z, duration, type);
}

void CherrySim::AnimationSetLooped(const std::string & name, bool looped)
{
    if (!AnimationExists(name))
    {
        SIMEXCEPTIONFORCE(IllegalStateException);
    }
    AnimationGet(name).SetLooped(looped);
}

bool CherrySim::AnimationIsRunning(u32 serialNumber)
{
    NodeEntry* entry = GetNodeEntryBySerialNumber(serialNumber);
    if (entry == nullptr)
    {
        SIMEXCEPTIONFORCE(IllegalStateException);
    }
    return entry->animation.IsStarted();
}

std::string CherrySim::AnimationGetName(u32 serialNumber)
{
    NodeEntry* entry = GetNodeEntryBySerialNumber(serialNumber);
    if (entry == nullptr)
    {
        SIMEXCEPTIONFORCE(IllegalStateException);
    }
    return entry->animation.GetName();
}

void CherrySim::AnimationStart(u32 serialNumber, const std::string & name)
{
    NodeEntry* entry = GetNodeEntryBySerialNumber(serialNumber);
    if (entry == nullptr)
    {
        SIMEXCEPTIONFORCE(IllegalStateException);
    }
    if (!AnimationExists(name))
    {
        SIMEXCEPTIONFORCE(IllegalStateException);
    }
    entry->animation = loadedMoveAnimations[name];
    entry->animation.Start(simState.simTimeMs, { entry->x, entry->y, entry->z });
}

void CherrySim::AnimationStop(u32 serialNumber)
{
    NodeEntry* entry = GetNodeEntryBySerialNumber(serialNumber);
    if (entry == nullptr)
    {
        SIMEXCEPTIONFORCE(IllegalStateException);
    }
    entry->animation = MoveAnimation();
}

MoveAnimationType StringToMoveAnimationType(const std::string& s)
{
    if (s == "lerp")
    {
        return MoveAnimationType::LERP;
    }
    if (s == "cosine")
    {
        return MoveAnimationType::COSINE;
    }
    if (s == "boolean")
    {
        return MoveAnimationType::BOOLEAN;
    }

    return MoveAnimationType::INVALID;
}

bool IsNumberType(nlohmann::json::value_t t)
{
    return t == nlohmann::json::value_t::number_float
        || t == nlohmann::json::value_t::number_integer
        || t == nlohmann::json::value_t::number_unsigned;
}

bool CherrySim::IsValidMoveAnimationJson(const nlohmann::json &json) const
{
    bool foundErrorInJson = false;
    for (nlohmann::json::const_iterator it = json.begin(); it != json.end(); ++it)
    {
        //Check if any animation already exists.
        if (AnimationExists(it.key()))
        {
            printf("Animation already exists: %s" EOL, it.key().c_str());
            foundErrorInJson = true;
        }

        //Check that mandatory elements are present.
        if (!it.value().contains("type"))
        {
            printf("Animation did not contain a type: %s" EOL, it.key().c_str());
            foundErrorInJson = true;
        }
        else if (it.value()["type"].type() != nlohmann::json::value_t::string)
        {
            printf("Animation type is not a string: %s" EOL, it.key().c_str());
            foundErrorInJson = true;
        }
        else if (StringToMoveAnimationType(it.value()["type"]) == MoveAnimationType::INVALID)
        {
            const std::string type = it.value()["type"];
            printf("Found illegal type in Animation %s: %s" EOL, it.key().c_str(), type.c_str());
            foundErrorInJson = true;
        }

        if (!it.value().contains("key_points"))
        {
            printf("Animation did not contain key points: %s" EOL, it.key().c_str());
            foundErrorInJson = true;
        }
        else if (it.value()["key_points"].type() != nlohmann::json::value_t::array)
        {
            printf("Animation key_points is not an array: %s" EOL, it.key().c_str());
            foundErrorInJson = true;
        }

        //Check optional elements
        if (it.value().contains("looped") && it.value()["looped"].type() != nlohmann::json::value_t::boolean)
        {
            printf("Animation looped is not a boolean: %s" EOL, it.key().c_str());
            foundErrorInJson = true;
        }

        for (nlohmann::json::const_iterator it2 = it.value()["key_points"].begin(); it2 != it.value()["key_points"].end(); ++it2)
        {
            //Check that mandatory elements within keypoints are present.
            if (!it2.value().contains("x"))
            {
                printf("Key Point in Animation %s did not contain a x value!" EOL, it.key().c_str());
                foundErrorInJson = true;
            }
            else if (!IsNumberType(it2.value()["x"].type()))
            {
                printf("X in Key Point in Animation %s is not a number type!" EOL, it.key().c_str());
                foundErrorInJson = true;
            }

            if (!it2.value().contains("y"))
            {
                printf("Key Point in Animation %s did not contain a y value!" EOL, it.key().c_str());
                foundErrorInJson = true;
            }
            else if (!IsNumberType(it2.value()["y"].type()))
            {
                printf("Y in Key Point in Animation %s is not a number type!" EOL, it.key().c_str());
                foundErrorInJson = true;
            }

            if (!it2.value().contains("z"))
            {
                printf("Key Point in Animation %s did not contain a z value!" EOL, it.key().c_str());
                foundErrorInJson = true;
            }
            else if (!IsNumberType(it2.value()["z"].type()))
            {
                printf("Z in Key Point in Animation %s is not a number type!" EOL, it.key().c_str());
                foundErrorInJson = true;
            }

            if (!it2.value().contains("durationSec"))
            {
                printf("Key Point in Animation %s did not contain a durationSec value!" EOL, it.key().c_str());
                foundErrorInJson = true;
            }
            else if (!IsNumberType(it2.value()["durationSec"].type()))
            {
                printf("durationSec in Key Point in Animation %s is not a number type!" EOL, it.key().c_str());
                foundErrorInJson = true;
            }
            else if (it2.value()["durationSec"] < 0.0)
            {
                printf("durationSec in Key Point in Animation %s was less than zero!" EOL, it.key().c_str());
                foundErrorInJson = true;
            }

            //Check optional elements
            if (it2.value().contains("type") && it.value()["type"].type() != nlohmann::json::value_t::string)
            {
                printf("Animation keypoint type is not a string: %s" EOL, it.key().c_str());
                foundErrorInJson = true;
            }
            else if (it2.value().contains("type") && StringToMoveAnimationType(it2.value()["type"]) == MoveAnimationType::INVALID)
            {
                printf("Animation keypoint type has no valid value: %s" EOL, it.key().c_str());
                foundErrorInJson = true;
            }
        }
    }

    return !foundErrorInJson;
}

bool CherrySim::AnimationLoadJsonFromPath(const char * path)
{
    std::ifstream file(path);
    if (!file) SIMEXCEPTION(FileException);
    nlohmann::json json;
    file >> json;

    if (!IsValidMoveAnimationJson(json)) return false;

    for (nlohmann::json::const_iterator it = json.begin(); it != json.end(); ++it)
    {
        AnimationCreate(it.key());
        AnimationSetDefaultType(it.key(), StringToMoveAnimationType(it.value()["type"]));

        if (it.value().contains("looped"))
        {
            AnimationSetLooped(it.key(), it.value()["looped"]);
        }

        for (nlohmann::json::const_iterator it2 = it.value()["key_points"].begin(); it2 != it.value()["key_points"].end(); ++it2)
        {
            if (it2.value().contains("type"))
            {
                AnimationAddKeypoint(it.key(), it2.value()["x"], it2.value()["y"], it2.value()["z"], it2.value()["durationSec"], StringToMoveAnimationType(it2.value()["type"]));
            }
            else
            {
                AnimationAddKeypoint(it.key(), it2.value()["x"], it2.value()["y"], it2.value()["z"], it2.value()["durationSec"]);
            }
        }
    }

    return true;
}
