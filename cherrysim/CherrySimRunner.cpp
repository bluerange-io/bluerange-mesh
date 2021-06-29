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
#include "CherrySimRunner.h"
#include "CherrySim.h"
#include "CherrySimUtils.h"
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <cmath>
#include <regex>
#include "json.hpp"
#ifdef _MSC_VER
#include <filesystem>
#endif

/**
The CherrySimRunner is used to start the simulator in a forever running loop.
Terminal input into all nodes is possible and visualization works using FruityMap.
*/

static bool shortLived = false; //Used for making sure that the Runner is able to run on CI.
static std::chrono::high_resolution_clock::time_point startTime;
extern bool meshGwCommunication;

#ifdef CHERRYSIM_RUNNER_ENABLED
int main(int argc, char** argv) {
    printf("#################################################" EOL
           "#                  CherrySim                    #" EOL
           "#################################################" EOL
           "#  Open your browser at http://localhost:5555/  #" EOL
           "#  to view the visualization of the simulation  #" EOL
           "#################################################" EOL);

    CherrySimRunnerConfig runnerConfig = CherrySimRunner::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimRunner::CreateDefaultRunConfiguration();

    for (int i = 0; i < argc; i++)
    {
        std::string s = argv[i];
        if (s == "MeshGwCommunication")
        {
            meshGwCommunication = true;
            std::ifstream i("MeshGWCommunicationConfig.json");
            if (!i)
            {
                SIMEXCEPTION(FileException);
            }
            nlohmann::json configJson;
            i >> configJson;
            simConfig = configJson;
            printf("Launching with MeshGwCommunication!" EOL);
        }
        else if (s == "shortLived")
        {
            shortLived = true;
        }
        else
        {
            if (i != 0) std::cerr << "WARNING: unknown parameter " << s << "\n";
        }
    }

#ifdef _MSC_VER
    std::filesystem::path currentWorkingDir = std::filesystem::current_path();
    printf("Current Working Directory: %s" EOL, currentWorkingDir.string().c_str());
#endif

    //The following exceptions are correctly handled by FruityMesh, they don't require us to terminate the simulator.
    Exceptions::ExceptionDisabler<ErrorCodeUnknownException> ecue;
    Exceptions::ExceptionDisabler<CRCMissingException> crcme;
    Exceptions::ExceptionDisabler<CRCInvalidException> crcie;
    Exceptions::ExceptionDisabler<CommandNotFoundException> disabler;

    //Failing a SystemTest just because one Error Message was logged somewhere is
    //probably too harsh and will lead to more issues than it solves in the future.
    Exceptions::ExceptionDisabler<ErrorLoggedException> ele;

    //@ReplayFeature@ <- Don't change this, it's a label used in the documentation.
    //You may use the following line to enable the replay feature. As this change
    //should not get commited anyway, you may use absolut or a relative path.
    //simConfig.replayPath = "../../cherry-sim.log";

    CherrySimRunner* runner = new CherrySimRunner(runnerConfig, simConfig, meshGwCommunication);
    printf("Launching Runner..." EOL);
    startTime = std::chrono::high_resolution_clock::now();

    runner->Start();

    return 0;
}
#endif

void CherrySimRunner::TerminalReaderMain() {
    //Half RAII struct. The boolean is set on the outside before the thread is started.
    struct TerminalReaderLaunchedSetter
    {
        CherrySimRunner* instance = nullptr;
        explicit TerminalReaderLaunchedSetter(CherrySimRunner* instance)
        {
            this->instance = instance;
        }
        ~TerminalReaderLaunchedSetter()
        {
            instance->terminalReaderLaunched = false;
        }
    };
    TerminalReaderLaunchedSetter trls = TerminalReaderLaunchedSetter(this);

    while (true) {
        std::string input;
        try
        {
            std::getline(std::cin, input);
        }
        catch (const std::ios_base::failure &e)
        {
            running = false;
            std::cout << "Some communication failure happend which probably means that the meshgw "
                "closed the connection. Thus this application is no longer needed." << std::endl;
            std::cout << "The details: " << std::endl;
            std::cout << e.what() << std::endl;
            return;
        }

        if (input == "")
        {
            //Normally a null char should not be sent. If this happend, the communication probably hung up.
            running = false;
            return;
        }

        sim->receivedDataFromMeshGw = true;
        sim->FindNodeById(MESH_GW_NODE)->gs.terminal.PutIntoTerminalCommandQueue(input, false);
    }
}

CherrySimRunnerConfig CherrySimRunner::CreateDefaultTesterConfiguration()
{
    CherrySimRunnerConfig config;
    config.enableClusteringTest = false; //If enabled, the simulator be reset after all nodes have clustered
    config.verbose = true;

    return config;
}

SimConfiguration CherrySimRunner::CreateDefaultRunConfiguration()
{
    SimConfiguration simConfig;

    simConfig.seed = 117;
    simConfig.mapWidthInMeters = 60;
    simConfig.mapHeightInMeters = 40;
    simConfig.mapElevationInMeters = 1;
    simConfig.simTickDurationMs = 50;
    simConfig.terminalId = 1; //Enter -1 to disable, 0 for all nodes, or a specific id

    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 7 });
#ifdef PROD_VS_NRF52
    simConfig.nodeConfigName.insert({ "prod_vs_nrf52", 1 }); // vs node ids are before assetNodes and clcNodes
#endif // PROD_VS_NRF52
#ifdef PROD_CLC_MESH_NRF52
    simConfig.nodeConfigName.insert({ "prod_clc_mesh_nrf52", 1 });//clc node ids are before assetNodes
#endif // PROD_CLC_MESH_NRF52

    simConfig.simOtherDelay = 1; // Enter 1 - 100000 to send sim_other message only each ... simulation steps, this increases the speed significantly
    simConfig.playDelay = 10; //Allows us to view the simulation slower than simulated, is added after each step

    simConfig.interruptProbability = UINT32_MAX / 10;

    simConfig.connectionTimeoutProbabilityPerSec = 0;// UINT32_MAX * 0.00001; //Every minute or so: 0.00001, randomly generates timout events for connections and disconnects them;
    simConfig.sdBleGapAdvDataSetFailProbability = 0;// UINT32_MAX * 0.0001; //Simulate fails on setting adv Data in the softdevice
    simConfig.sdBusyProbability = UINT32_MAX / 100;// UINT32_MAX * 0.0001; //Simulates getting back busy errors from the softdevice
    simConfig.simulateAsyncFlash = true; //Simulates asynchronous flash operations, rather then sending the ACK immediately
    simConfig.asyncFlashCommitTimeProbability = UINT32_MAX / 10 * 9;

    simConfig.importFromJson = false; //Set to true in order to not generate nodes
    simConfig.siteJsonPath = "testsite.json";
    simConfig.devicesJsonPath = "testdevices.json";


    simConfig.defaultBleStackType = BleStackType::NRF_SD_132_ANY;

    simConfig.defaultNetworkId = 10;

    simConfig.rssiNoise = true;

    simConfig.verboseCommands = true;
    simConfig.enableSimStatistics = true;

    simConfig.fastLaneToSimTimeMs = 0;

    simConfig.logReplayCommands = true;

    return simConfig;
}

CherrySimRunner::CherrySimRunner(const CherrySimRunnerConfig &runnerConfig, const SimConfiguration &simConfig, bool meshGwCommunication)
    : meshGwCommunication(meshGwCommunication),
    runnerConfig(runnerConfig),
    simConfig(simConfig)
{
    shouldRestartSim = false;
    this->sim = nullptr;
}

void CherrySimRunner::Start()
{
    while (running) {
        printf("## SIM Starting ##" EOL);
        sim = new CherrySim(simConfig);
        sim->SetCherrySimEventListener(this);
        sim->RegisterTerminalPrintListener(this);
        sim->Init();

        //We can now modify the nodes to use a different configuration
        //Set the first node to deviceType sink
        sim->nodes[0].uicr.CUSTOMER[11] = (u32)DeviceType::SINK; //deviceType


        //Boot up all nodes
        for (u32 i = 0; i < sim->GetTotalNodes(); i++) {
            NodeIndexSetter setter(i);
            sim->BootCurrentNode();
        }

        if (shortLived)
        {
            //ShortLived mode is only used for dry runs on the pipeline.
            //As we don't have a communication partner in that scenario,
            //we immediately tell that we received some data so that the
            //simulation starts properly.
            sim->receivedDataFromMeshGw = true;
        }
        if (meshGwCommunication && !terminalReaderLaunched)
        {
            terminalReaderLaunched = true;
            terminalReader = std::thread(&CherrySimRunner::TerminalReaderMain, this);
        }

        if (Simulate()) {
            break;
        }

        //Save the sim config as it might have changed during runtime
        simConfig = sim->simConfig;

        delete sim;
    }
}

bool CherrySimRunner::Simulate()
{
    //Simulate all nodes
    while (running) {
        if (sim->simConfig.playDelay > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sim->simConfig.playDelay));
        }

        //If we should run the clustering test, we sometimes reset the sim or some nodes after clustering
        if (runnerConfig.enableClusteringTest && sim->IsClusteringDone())
        {
            //Store the average clustering time for reference
            SIMSTATAVG("ClusteringTestTimeAvgSec", sim->simState.simTimeMs / 1000);

            if (PSRNG(UINT32_MAX / 10 * 4)) {
                printf("Clustering seed %u" EOL, simConfig.seed);
                sim->simConfig.seed++;
                return false;
            }
            else {
                u32 numNoneAssetNodes = sim->GetTotalNodes() - sim->GetAssetNodes();
                u32 numNodesToReset = (u32)(PSRNG(std::ceil(numNoneAssetNodes / 5.0)) + 1);
                auto nodeIndizesToReset = CherrySimUtils::GenerateRandomNumbers(0, numNoneAssetNodes - 1, numNodesToReset);

                for (auto const nodeIdx : nodeIndizesToReset) {
                    NodeIndexSetter setter(nodeIdx);
                    try {
                        sim->ResetCurrentNode(RebootReason::UNKNOWN);
                    }
                    catch (NodeSystemResetException& e) {
                        //Nothing to do
                    }
                }
            }
        }

        try {
            sim->SimulateStepForAllNodes();
            if (shortLived)
            {
                std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
                auto diff = now - startTime;
                int seconds = (int)std::chrono::duration_cast<std::chrono::seconds>(diff).count();
                if (seconds >= 60) {
                    printf("----------------------------------------------" EOL
                           "--- Killing Process because of shortLived! ---" EOL
                           "----------------------------------------------" EOL);

                    return true;
                }
            }
        }
        catch (CherrySimQuitException& e) {
            return false;
        }
    }
    return true;
}

//########################### Callbacks ###############################

void CherrySimRunner::TerminalPrintHandler(NodeEntry* currentNode, const char* message)
{
    // Important: The check _must_ succeed if both are 0, otherwise the
    // configuration will not be printed in (e.g.) the System Test pipeline.
    if (sim->simConfig.fastLaneToSimTimeMs <= sim->simState.simTimeMs) {
        printf("%s", message);
    }
}

void CherrySimRunner::CherrySimEventHandler(const char* eventType)
{

}

void CherrySimRunner::CherrySimBleEventHandler(NodeEntry* currentNode, simBleEvent* simBleEvent, u16 eventSize)
{

}
