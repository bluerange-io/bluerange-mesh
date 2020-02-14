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
#include "CherrySimRunner.h"
#include "CherrySim.h"
#include "CherrySimUtils.h"
#include <string>
#include <iostream>
#include <chrono>
#include <cmath>
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
			simConfig.terminalId = CherrySimRunner::MESH_GW_NODE;
			simConfig.defaultNetworkId = 0;
			simConfig.numNodes = 10;
			simConfig.numAssetNodes = 2;
			simConfig.rssiNoise = true;
			simConfig.storeFlashToFile = "CherrySimFlashState.bin";
			strcpy(simConfig.defaultSinkConfigName, "prod_sink_nrf52");
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

	//ErrorCodeUnknownExceptions are correctly handled by FruityMesh, they don't require us to terminate the simulator.
	Exceptions::ExceptionDisabler<ErrorCodeUnknownException> ecue;

	CherrySimRunner* runner = new CherrySimRunner(runnerConfig, simConfig, meshGwCommunication);
	printf("Launching Runner..." EOL);
	startTime = std::chrono::high_resolution_clock::now();

	Exceptions::ExceptionDisabler<CommandNotFoundException> disabler;
	runner->Start();

	return 0;
}
#endif

void CherrySimRunner::TerminalReaderMain() {
	while (true) {
		std::string input;
		try
		{
			std::getline(std::cin, input);
		}
		catch (const std::ios_base::failure &e)
		{
			//Some communication failure happend which probably means that the meshgw closed the connection. Thus this application is no longer needed.
			running = false;
			return;
		}

		if (input == "")
		{
			//Normally a null char should not be sent. If this happend, the communication probably hung up.
			running = false;
			return;
		}

		sim->findNodeById(MESH_GW_NODE)->gs.terminal.PutIntoReadBuffer(input.c_str());
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

	simConfig.numNodes = 10;
	simConfig.numAssetNodes = 0; //asset node ids are at the end e.g if we have numNode 2 and numAssetNode 1, the node id of asset node will be 3.
	simConfig.seed = 117;
	simConfig.mapWidthInMeters = 60;
	simConfig.mapHeightInMeters = 40;
	simConfig.simTickDurationMs = 50;
	simConfig.terminalId = 1; //Enter -1 to disable, 0 for all nodes, or a specific id

	simConfig.simOtherDelay = 1; // Enter 1 - 100000 to send sim_other message only each ... simulation steps, this increases the speed significantly
	simConfig.playDelay = 10; //Allows us to view the simulation slower than simulated, is added after each step

	simConfig.connectionTimeoutProbabilityPerSec = 0;// 0.00001; //Every minute or so: 0.00001, randomly generates timout events for connections and disconnects them;
	simConfig.sdBleGapAdvDataSetFailProbability = 0;// 0.0001; //Simulate fails on setting adv Data in the softdevice
	simConfig.sdBusyProbability = 0.01;// 0.0001; //Simulates getting back busy errors from the softdevice
	simConfig.simulateAsyncFlash = true; //Simulates asynchronous flash operations, rather then sending the ACK immediately
	simConfig.asyncFlashCommitTimeProbability = 0.9;

	simConfig.importFromJson = false; //Set to true in order to not generate nodes
	strcpy(simConfig.siteJsonPath, "testsite.json");
	strcpy(simConfig.devicesJsonPath, "testdevices.json");

	strcpy(simConfig.defaultNodeConfigName, "prod_mesh_nrf52");
	strcpy(simConfig.defaultSinkConfigName, "prod_sink_nrf52");

	simConfig.defaultBleStackType = BleStackType::NRF_SD_132_ANY;

	simConfig.defaultNetworkId = 10;

	simConfig.rssiNoise = true;

	simConfig.verboseCommands = true;
	simConfig.enableSimStatistics = true;

	return simConfig;
}

CherrySimRunner::CherrySimRunner(const CherrySimRunnerConfig &runnerConfig, const SimConfiguration &simConfig, bool meshGwCommunication)
	: meshGwCommunication(meshGwCommunication),
	simConfig(simConfig),
	runnerConfig(runnerConfig)
{
	shouldRestartSim = false;
	this->sim = nullptr;

	if (meshGwCommunication) {
		terminalReader = std::thread(&CherrySimRunner::TerminalReaderMain, this);
	}
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

#ifndef GITHUB_RELEASE
		// The last numNode will have prod_clc_configuration
		strcpy(sim->nodes[simConfig.numNodes - 1].nodeConfiguration, "prod_clc_mesh_nrf52");
		// The second last numNode will have prod_vs_nrf52 configuration
		if (simConfig.numNodes >= 2) {
			strcpy(sim->nodes[simConfig.numNodes - 2].nodeConfiguration, "prod_vs_nrf52");
		}
#endif //GITHUB_RELEASE

		//Boot up all nodes
		u32 numNodes = sim->getNumNodes();
		for (u32 i = 0; i < numNodes; i++) {
			sim->setNode(i);
			sim->bootCurrentNode();
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

			if (PSRNG() < 0.4) {
				printf("Clustering seed %u" EOL, simConfig.seed);
				sim->simConfig.seed++;
				return false;
			}
			else {
				u32 numNodesToReset = (u32)(std::ceil(sim->simConfig.numNodes / 5.0) * PSRNG() + 1);
				auto nodeIndizesToReset = CherrySimUtils::generateRandomNumbers(0, sim->simConfig.numNodes - 1, numNodesToReset);

				for (auto const nodeIdx : nodeIndizesToReset) {
					sim->setNode(nodeIdx);
					try {
						sim->resetCurrentNode(RebootReason::UNKNOWN);
					}
					catch (NodeSystemResetException& e) {
						UNUSED_PARAMETER(e);
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
			UNUSED_PARAMETER(e);
			return false;
		}
	}
	return true;
}

//########################### Callbacks ###############################

void CherrySimRunner::TerminalPrintHandler(nodeEntry* currentNode, const char* message)
{
	if (runnerConfig.verbose) {
		//Send to console
		printf("%s", message);
	}
}

void CherrySimRunner::CherrySimEventHandler(const char* eventType)
{

}

void CherrySimRunner::CherrySimBleEventHandler(nodeEntry* currentNode, simBleEvent* simBleEvent, u16 eventSize)
{

}