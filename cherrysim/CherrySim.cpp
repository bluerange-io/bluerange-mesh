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
#ifdef CHERRYSIM_TESTER_ENABLED
#include <Testing.h>
#endif
#include <LedWrapper.h>
#include <Utility.h>
#include <types.h>
#include <Config.h>
#include <Boardconfig.h>
#include <FlashStorage.h>
#ifndef GITHUB_RELEASE
#include <ClcComm.h>
#include "VsComm.h"
#endif //GITHUB_RELEASE
#include <conn_packets.h>


#include <Module.h>
#include <AdvertisingModule.h>
#include <EnrollmentModule.h>
#include <IoModule.h>
#ifndef GITHUB_RELEASE
#include <ClcModule.h>
#include <ClcComm.h>
#include "ClcMock.h"
#include "AssetModule.h"
#endif //GITHUB_RELEASE
#include "ConnectionAllocator.h"


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

void CherrySim::StoreFlashToFile()
{
	if (!simConfig.storeFlashToFile) return;

	std::ofstream file(simConfig.storeFlashToFile, std::ios::binary);
	
	FlashFileHeader ffh;
	CheckedMemset(&ffh, 0, sizeof(ffh));

	ffh.version = FM_VERSION;
	ffh.sizeOfHeader = sizeof(ffh);
	ffh.flashSize = SIM_MAX_FLASH_SIZE;
	ffh.amountOfNodes = getNumNodes();

	file.write((const char*)&ffh, sizeof(ffh));

	for (u32 i = 0; i < getNumNodes(); i++)
	{
		file.write((const char*)this->nodes[i].flash, SIM_MAX_FLASH_SIZE);
	}
}

void CherrySim::LoadFlashFromFile()
{
	if (!simConfig.storeFlashToFile) return;

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
		|| ffh.amountOfNodes != getNumNodes()
		|| length            != sizeof(ffh) + SIM_MAX_FLASH_SIZE * getNumNodes()
		)
	{
		//Probably the correct action if this happens is to just remove the flash safe file (see simConfig.storeFlashToFile)
		//This is NOT automatically performed here as it would be rather rude to just remove it in case the user accidentally
		//launched a different version of CherrySim or another config.
		SIMEXCEPTION(CorruptOrOutdatedSavefile);
		return;
	}

	for (u32 i = 0; i < getNumNodes(); i++)
	{
		CheckedMemcpy(this->nodes[i].flash, buffer + SIM_MAX_FLASH_SIZE * i + sizeof(ffh), SIM_MAX_FLASH_SIZE);
	}

	delete[] buffer;
}

#define AddSimulatedFeatureSet(featureset) \
{ \
	extern FeatureSetGroup getFeatureSetGroup_##featureset(); \
	extern void setBoardConfiguration_##featureset(BoardConfiguration* config); \
	extern void setFeaturesetConfiguration_##featureset(ModuleConfiguration* config, void* module); \
	extern u32 initializeModules_##featureset(bool createModule); \
	extern DeviceType getDeviceType_##featureset(); \
	extern Chipset getChipset_##featureset(); \
	FeaturesetPointers fp; \
	fp.setBoardConfigurationPtr = setBoardConfiguration_##featureset; \
	fp.getFeaturesetGroupPtr = getFeatureSetGroup_##featureset; \
	fp.setFeaturesetConfigurationPtr = setFeaturesetConfiguration_##featureset; \
	fp.initializeModulesPtr = initializeModules_##featureset; \
	fp.getDeviceTypePtr = getDeviceType_##featureset; \
	fp.getChipsetPtr = getChipset_##featureset; \
	featuresetPointers.insert(std::pair<std::string, FeaturesetPointers>(std::string(#featureset), fp)); \
}

void CherrySim::PrepareSimulatedFeatureSets()
{
	AddSimulatedFeatureSet(github_nrf52);
#ifndef GITHUB_RELEASE
	AddSimulatedFeatureSet(prod_mesh_nrf51);
	AddSimulatedFeatureSet(prod_sink_nrf52);
	AddSimulatedFeatureSet(prod_mesh_nrf52);
	AddSimulatedFeatureSet(prod_clc_mesh_nrf52);
	AddSimulatedFeatureSet(prod_asset_nrf52);
	AddSimulatedFeatureSet(prod_asset_ins_nrf52840);
	AddSimulatedFeatureSet(dev51);
	AddSimulatedFeatureSet(dev);
	AddSimulatedFeatureSet(dev_automated_tests_master_nrf52);
	AddSimulatedFeatureSet(dev_automated_tests_slave_nrf52);
	AddSimulatedFeatureSet(dev_vslog);
	AddSimulatedFeatureSet(prod_vs_nrf52);
	AddSimulatedFeatureSet(prod_vs_converter_nrf52);
	AddSimulatedFeatureSet(prod_pcbridge_nrf52);
	AddSimulatedFeatureSet(prod_wm_nrf52840);
#endif //GITHUB_RELEASE
}

#undef AddSimulatedFeatureSet

CherrySim::CherrySim(const SimConfiguration &simConfig)
	: simConfig(simConfig)
{
#ifdef GITHUB_RELEASE
	printf("Vendor code is NOT included!" EOL);
#else
	printf("Vendor code IS included!" EOL);
#endif //GITHUB_RELEASE
	//Set a reference that can be used from fruitymesh if necessary
	cherrySimInstance = this;

	PrepareSimulatedFeatureSets();

	//Reset variables to default
	simState.~SimulatorState();
	new (&simState) SimulatorState();
	simState.simTimeMs = 0;
	simState.globalConnHandleCounter = 0;
	for (int i = 0; i < MAX_NUM_NODES; i++)
	{
		nodes[i].~nodeEntry();
		new (&nodes[i]) nodeEntry();
	}
}

CherrySim::~CherrySim()
{
	StoreFlashToFile();

	//Clean up up all nodes
	for (u32 i = 0; i < getNumNodes(); i++) {
		setNode(i);
		shutdownCurrentNode();
	}

	if(server != nullptr) delete server;
	server = nullptr;
}

void CherrySim::SetCherrySimEventListener(CherrySimEventListener* listener)
{
	this->simEventListener = listener;
}

//This will create the initial node configuration
void CherrySim::Init()
{
	//Generate a psuedo random number generator with a uniform distribution
	simState.rnd = MersenneTwister(simConfig.seed);

	//Load site and device data from a json if given
	if (simConfig.importFromJson) {
		importDataFromJson();
	}
	
	for (u32 i = 0; i<getNumNodes(); i++) {
		initNode(i);
		flashNode(i);
	}

	LoadFlashFromFile();

	//Either use given positions from json or generate them randomly
	if (simConfig.importFromJson) {
		importPositionsFromJson();
	} else {
		PositionNodesRandomly();
		LoadPresetNodePositions();
	}

	server = new FruitySimServer();
}

//This will load the site data from a json and will read the device json to import all devices
void CherrySim::importDataFromJson()
{
	//Load the site json
	std::ifstream siteJsonStream(simConfig.siteJsonPath);
	json siteJson;
	siteJsonStream >> siteJson;

	//Load the devices json
	std::ifstream devicesJsonStream(simConfig.devicesJsonPath);
	json devicesJson;
	devicesJsonStream >> devicesJson;

	//Get some data from the site
	simConfig.mapWidthInMeters = siteJson["results"][0]["lengthInMeter"];
	simConfig.mapHeightInMeters = siteJson["results"][0]["heightInMeter"];

	//Get number of nodes
	int j = 0;
	for (u32 i = 0; i < devicesJson["results"].size(); i++) {
		if (devicesJson["results"][i]["platform"] == "BLENODE" && (devicesJson["results"][i]["properties"]["onMap"] == true || devicesJson["results"][i]["properties"]["onMap"] == "true")) {
			j++;
		}
	}

	simConfig.numNodes = j;
}

//This will read the device json and will set all the node positions from it
void CherrySim::importPositionsFromJson()
{
	//Load the devices json
	std::ifstream devicesJsonStream(simConfig.devicesJsonPath);
	json devicesJson;
	devicesJsonStream >> devicesJson;

	//Get other data from our devices
	int j = 0;
	for (u32 i = 0; i < devicesJson["results"].size(); i++) {
		if (devicesJson["results"][i]["platform"] == "BLENODE" && (devicesJson["results"][i]["properties"]["onMap"] == true || devicesJson["results"][i]["properties"]["onMap"] == "true")) {
			if (devicesJson["results"][i]["properties"]["x"].type_name() == "number")
			{
				nodes[j].x = devicesJson["results"][i]["properties"]["x"];
				nodes[j].y = devicesJson["results"][i]["properties"]["y"];
				//The z coordinate is optional.
				auto jsonEntryZ = devicesJson["results"][i]["properties"]["z"];
				nodes[j].z = jsonEntryZ != nullptr ? (float)jsonEntryZ : 0.0f;
			}
			else if (devicesJson["results"][i]["properties"]["x"].type_name() == "string")
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

//This will position all nodes randomly, by using dbscan to generate a configuration that can be clustered
void CherrySim::PositionNodesRandomly()
{
	//Set some random x and y position for all nodes
	u32 numNodes = getNumNodes();
	for (unsigned int nodeIndex = 0; nodeIndex < numNodes; nodeIndex++) {
		nodes[nodeIndex].x = (float)PSRNG();
		nodes[nodeIndex].y = (float)PSRNG();
		nodes[nodeIndex].z = 0;
	}

	if (simConfig.numNodes > 1) {
		//Next, we must check if the configuraton can cluster
		point_t points[MAX_NUM_NODES] = {};

		//Calculate the epsilon using the rssi threshold and the transmission powers
		double epsilon = pow(10, ((double)-STABLE_CONNECTION_RSSI_THRESHOLD + SIMULATOR_NODE_DEFAULT_CALIBRATED_TX + SIMULATOR_NODE_DEFAULT_DBM_TX) / 10 / N);

		unsigned int minpts = 1; //a point must reach only one of another cluster to become part of its cluster
		unsigned int num_points = numNodes;

		bool retry = false;
		do {
			//printf("try\n");
			retry = false;

			//Create 2D points for all node positions
			for (u32 i = 0; i < numNodes; i++) {
				points[i].cluster_id = -1; //unclassified
				points[i].x = (double)nodes[i].x * (double)simConfig.mapWidthInMeters;
				points[i].y = (double)nodes[i].y * (double)simConfig.mapHeightInMeters;
				points[i].z = (double)nodes[i].z;
			}

			//Use dbscan algorithm to check how many clusters these nodes can generate
			dbscan(points, num_points, epsilon, minpts, euclidean_dist);

			//printf("Epsilon for dbscan: %lf\n", epsilon);
			//printf("Minimum points: %u\n", minpts);
			//print_points(points, num_points);

			for (u32 i = 0; i < numNodes; i++) {
				if (points[i].cluster_id != 0) {
					retry = true;
					nodes[i].x = (float)PSRNG();
					nodes[i].y = (float)PSRNG();
				}
			}
		} while (retry);
	}
}

void CherrySim::LoadPresetNodePositions()
{
	if (simConfig.preDefinedPositions.size() != 0)
	{
		for (u32 nodeIndex = 0; nodeIndex < std::min(simConfig.preDefinedPositions.size(), getNumNodes()); nodeIndex++)
		{
			nodes[nodeIndex].x = (float)simConfig.preDefinedPositions[nodeIndex].first;
			nodes[nodeIndex].y = (float)simConfig.preDefinedPositions[nodeIndex].second;
		}
	}
}


//This simulates a time step for all nodes
void CherrySim::SimulateStepForAllNodes()
{
	CheckForMultiTensorflowUsage();

	//Check if the webserver has some open requests to process
	server->ProcessServerRequests();

	int64_t sumOfAllSimulatedFrames = 0;
	for (u32 i = 0; i < getNumNodes(); i++) {
		setNode(i);
		sumOfAllSimulatedFrames += currentNode->simulatedFrames;
	}
	const int64_t avgSimulatedFrames = sumOfAllSimulatedFrames / getNumNodes();

	//printf("-- %u --" EOL, simState.simTimeMs);
	for (u32 i = 0; i < getNumNodes(); i++) {
		setNode(i);
		bool simulateNode = true;
		if (simConfig.simulateJittering)
		{
			const int64_t frameOffset = currentNode->simulatedFrames - avgSimulatedFrames;
			// Sigmoid function, flipped on the Y-Axis.
			const double probability = 1.0 / (1 + std::exp((double)(frameOffset) * 0.1));
			const double randVal = simState.rnd.nextDouble();
			if (randVal > probability)
			{
				simulateNode = false;
			}
		}
		if (simulateNode)
		{
			StackBaseSetter sbs;

			currentNode->simulatedFrames++;
			simulateTimer();
			simulateTimeouts();
			simulateBroadcast();
			SimulateConnections();
			SimulateServiceDiscovery();
			SimulateUartInterrupts();
#ifndef GITHUB_RELEASE
			SimulateClcData();
#endif //GITHUB_RELEASE
			try {
				FruityHal::EventLooper();
				simulateFlashCommit();
				simulateBatteryUsage();
				simulateWatchDog();
			}
			catch (const NodeSystemResetException& e) {
				UNUSED_PARAMETER(e);
				//Node broke out of its current simulation and rebootet
				if (simEventListener) simEventListener->CherrySimEventHandler("NODE_RESET");
			}
		}

		globalBreakCounter++;
	}

	//Run a check on the current clustering state
	if(simConfig.enableClusteringValidityCheck) CheckMeshingConsistency();

	simState.simTimeMs += simConfig.simTickDurationMs;
	//Initialize RNG with new seed in order to be able to jump to a frame and resimulate it
	simState.rnd = MersenneTwister(simState.simTimeMs + simConfig.seed);

	//Back up the flash every flashToFileWriteInterval's step.
	flashToFileWriteCycle++;
	if (flashToFileWriteCycle % flashToFileWriteInterval == 0) StoreFlashToFile();
}

void CherrySim::quitSimulation()
{
	throw CherrySimQuitException();
}

uint32_t CherrySim::getNumNodes() const
{
	return simConfig.numNodes+simConfig.numAssetNodes;
}

//################################## Terminal #############################################
// Terminal input and output for the nodes and the sim
//#########################################################################################

//Used to register a TerminalListener with FruityMesh
void CherrySim::registerSimulatorTerminalHandler() {
	GS->terminal.AddTerminalCommandListener(this);
}

//Terminal functions to control the simulator (CherrySim registers its TerminalCommandHandler with FruityMesh)
TerminalCommandHandlerReturnType CherrySim::TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize)
{
	//Allows us to switch terminals

	if (TERMARGS(0, "simstat")) {
		printf("---- Configurable via terminal ----\n");
		printf("Terminal (term): %d\n", simConfig.terminalId);
		printf("Number of Nodes (nodes): %u\n", simConfig.numNodes);
		printf("Number of Asset Nodes (nodes): %u\n", simConfig.numAssetNodes);
		printf("Current Seed (seed): %u\n", simConfig.seed);
		printf("Map Width (width): %u\n", simConfig.mapWidthInMeters);
		printf("Map Height (height): %u\n", simConfig.mapHeightInMeters);
		printf("ConnectionLossProbability (lossprob): %f\n", simConfig.connectionTimeoutProbabilityPerSec);
		printf("Play delay (delay): %d\n", simConfig.playDelay);
		printf("Import Json (json): %u\n", simConfig.importFromJson);
		printf("Site json (site): %s\n", simConfig.siteJsonPath);
		printf("Devices json (devices): %s\n", simConfig.devicesJsonPath);


		printf("---- Other ----\n");
		printf("Simtime %u\n", simState.simTimeMs);

		sim_print_statistics();

		printf("Enter 'sendstat {nodeId=0}' or 'routestat {nodeId=0}' for packet statistics" EOL);

		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "term") && commandArgsSize == 2) {
		if (TERMARGS(1, "all")) {
			simConfig.terminalId = 0;
		}
		else {
			simConfig.terminalId = Utility::StringToI32(commandArgs[1]);
		}
		printf("Switched Terminal to %d\n", simConfig.terminalId);

		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "nodes") && commandArgsSize == 2) {
		simConfig.numNodes = Utility::StringToU32(commandArgs[1]);
		quitSimulation();
		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "assetnodes") && commandArgsSize == 2) {
		simConfig.numAssetNodes = Utility::StringToU32(commandArgs[1]);
		quitSimulation();
		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "seed") || TERMARGS(0, "seedr")) {
		if (commandArgsSize == 2) {
			simConfig.seed = Utility::StringToU32(commandArgs[1]);
		}
		else {
			simConfig.seed++;
		}
		printf("Seed set to %u\n", simConfig.seed);

		if (TERMARGS(0, "seedr")) {
			quitSimulation();
		}

		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "width") && commandArgsSize == 2) {
		simConfig.mapWidthInMeters = Utility::StringToU32(commandArgs[1]);
		quitSimulation();
		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "height") && commandArgsSize == 2) {
		simConfig.mapHeightInMeters = Utility::StringToU32(commandArgs[1]);
		quitSimulation();
		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "lossprob") && commandArgsSize == 2) {
		simConfig.connectionTimeoutProbabilityPerSec = atof(commandArgs[1]);
		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "delay") && commandArgsSize == 2) {
		simConfig.playDelay = Utility::StringToI32(commandArgs[1]);
		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "json") && commandArgsSize == 2) {
		simConfig.importFromJson = Utility::StringToU8(commandArgs[1]);
		quitSimulation();
		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "site") && commandArgsSize == 2) {
		strcpy(simConfig.siteJsonPath, commandArgs[1]);
		quitSimulation();
		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "devices") && commandArgsSize == 2) {
		strcpy(simConfig.devicesJsonPath, commandArgs[1]);
		quitSimulation();
		return TerminalCommandHandlerReturnType::SUCCESS;
	}


	//For testing

	else if (TERMARGS(0, "simreset")) {
		quitSimulation();
	}
	else if (TERMARGS(0, "flush") && commandArgsSize == 1) {
		sim_commit_flash_operations();
		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "flushfail") && commandArgsSize == 1) {
		u8 failData[] = { 1,1,1,1,1,1,1,1,1,1 };
		sim_commit_some_flash_operations(failData, 10);
		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "nodestat")) {
		printf("Node advertising %d (iv %d)\n", currentNode->state.advertisingActive, currentNode->state.advertisingIntervalMs);
		printf("Node scanning %d (window %d, iv %d)\n", currentNode->state.scanningActive, currentNode->state.scanWindowMs, currentNode->state.scanIntervalMs);

		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "delay") && commandArgsSize == 2) {
		simConfig.playDelay = Utility::StringToI32(commandArgs[1]);
		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "simloss")) {
		for (int i = 0; i < currentNode->state.configuredTotalConnectionCount; i++) {
			if (currentNode->state.connections[i].connectionActive) {
				printf("Simulated Connection Loss for node %d to partner %d (handle %d)" EOL, currentNode->id, currentNode->state.connections[i].partner->id, currentNode->state.connections[i].connectionHandle);
				DisconnectSimulatorConnection(&currentNode->state.connections[i], BLE_HCI_CONNECTION_TIMEOUT, BLE_HCI_CONNECTION_TIMEOUT);
			}
		}

		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "rees")) {
		int handle = Utility::StringToI32(commandArgs[1]);
		SoftdeviceConnection* conn = nullptr;
		for (int i = 0; i < currentNode->state.configuredTotalConnectionCount; i++) {
			if (currentNode->state.connections[i].connectionHandle == handle) {
				conn = &currentNode->state.connections[i];
			}
		}
		if (conn != nullptr) {
			DisconnectSimulatorConnection(conn, BLE_HCI_MEMORY_CAPACITY_EXCEEDED, BLE_HCI_MEMORY_CAPACITY_EXCEEDED);
			blockConnections = false;
		}
		else {
			printf("Connection with that handle not available" EOL);
		}

		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "blockconn")) {
		blockConnections = !blockConnections;
		printf("Block connections is now %u" EOL, blockConnections);

		return TerminalCommandHandlerReturnType::SUCCESS;
	}

	//For statistics
	else if (TERMARGS(0, "sendstat")) {
		//Print statistics about all packets generated by a node
		NodeId nodeId = commandArgsSize >= 2 ? Utility::StringToU16(commandArgs[1]) : 0;
		PrintPacketStats(nodeId, "SENT");
		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "routestat")) {
		//Print statistics about all packet routed by a node
		NodeId nodeId = commandArgsSize >= 2 ? Utility::StringToU16(commandArgs[1]) : 0;
		PrintPacketStats(nodeId, "ROUTED");
		return TerminalCommandHandlerReturnType::SUCCESS;
	}
	else if (TERMARGS(0, "sim"))
	{
		//sim set_position BBBBD 0.5 0.21 0.17
		if (commandArgsSize >= 5 && (TERMARGS(1, "set_position") || TERMARGS(1, "add_position")))
		{
			size_t index = 0;
			bool nodeFound = false;
			for (u32 i = 0; i < getNumNodes(); i++)
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
				// We divide by the size of the virtual environment because the user
				// should not have to care about these dimensions. What the user most
				// likely wants to do is to move the node X meter in one direction.
				// For this task the mapWidth/Height are just an annoying distraction.
				x = std::stof(commandArgs[3]) / simConfig.mapWidthInMeters;
				y = std::stof(commandArgs[4]) / simConfig.mapHeightInMeters;
				z = std::stof(commandArgs[5]);
			}
			catch (std::invalid_argument &e)
			{
				return TerminalCommandHandlerReturnType::WRONG_ARGUMENT;
			}

			if (TERMARGS(1, "set_position"))
			{
				nodes[index].x = x;
				nodes[index].y = y;
				nodes[index].z = z;
			}
			else if (TERMARGS(1, "add_position"))
			{
				nodes[index].x += x;
				nodes[index].y += y;
				nodes[index].z += z;
			}

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
	if (terminalPrintListener != nullptr) {
		if (currentNode->id == simConfig.terminalId || simConfig.terminalId == 0) {
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
void CherrySim::setNode(u32 i)
{
	if (i >= getNumNodes())
	{
		std::cerr << "Tried to access node: " << i << std::endl;
		SIMEXCEPTION(IndexOutOfBoundsException);
	}

	//printf("**SIM**: Setting node %u\n", i+1);
	currentNode = nodes + i;

	simGlobalStatePtr = &(nodes[i].gs);

	simFicrPtr = &(nodes[i].ficr);
	simUicrPtr = &(nodes[i].uicr);
	simGpioPtr = &(nodes[i].gpio);
	simFlashPtr = nodes[i].flash;
	simUartPtr = &(nodes[i].state.uartType);

	__application_start_address = (uint32_t)simFlashPtr + FruityHal::getSoftDeviceSize();
	__application_end_address = (uint32_t)__application_start_address + chipsetToApplicationSize(GET_CHIPSET());

	//Point the linker sections for connectionTypeResolvers to the correct array
	__start_conn_type_resolvers = (u32)connTypeResolvers;
	__stop_conn_type_resolvers = ((u32)connTypeResolvers) + sizeof(connTypeResolvers);

	//TODO: Find a better way to do this
	ChooseSimulatorTerminal();
}

int CherrySim::chipsetToPageSize(Chipset chipset)
{
	switch (chipset)
	{
	case Chipset::CHIP_NRF51:
		return 1024;
	case Chipset::CHIP_NRF52:
	case Chipset::CHIP_NRF52840:
		return 4096;
	default:
		//I don't know this chipset!
		SIMEXCEPTION(IllegalStateException);
		return -1;
	}
}

int CherrySim::chipsetToCodeSize(Chipset chipset)
{
	switch (chipset)
	{
	case Chipset::CHIP_NRF51:
		return 256;
	case Chipset::CHIP_NRF52:
	case Chipset::CHIP_NRF52840:
		return 128;
	default:
		//I don't know this chipset!
		SIMEXCEPTION(IllegalStateException);
		return -1;
	}
}

int CherrySim::chipsetToApplicationSize(Chipset chipset)
{
	switch (chipset)
	{
	case Chipset::CHIP_NRF51:
		return 64 * 1024;
	case Chipset::CHIP_NRF52:
	case Chipset::CHIP_NRF52840:
		return 128 * 1024;
	default:
		//I don't know this chipset!
		SIMEXCEPTION(IllegalStateException);
		return -1;
	}
}

int CherrySim::chipsetToBootloaderAddr(Chipset chipset)
{
	switch (chipset)
	{
	case Chipset::CHIP_NRF51:
		return 257024;
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
	for (u32 i = 0; i < getNumNodes(); i++)
	{
		setNode(i);
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

/**
Prepares the memory of a node and resets all its data

After calling this function, the node will have its basic data structures prepared, but flashNode
must be called before booting the node in order to have the flash prepared as well.
*/
void CherrySim::initNode(u32 i)
{
	//Clean our node state
	nodes[i].~nodeEntry();
	new (&nodes[i]) nodeEntry();

	//Set index and id
	nodes[i].index = i;
	nodes[i].id = i + 1;

	//Initialize flash memory
	CheckedMemset(nodes[i].flash, 0xFF, sizeof(nodes[i].flash));
	//TODO: We could load a softdevice and app image into flash, would that help for something?

	//Generate device address based on the id
	nodes[i].address.addr_type = FruityHal::BleGapAddrType::RANDOM_STATIC;
	CheckedMemset(&nodes[i].address.addr, 0x00, 6);
	CheckedMemcpy(nodes[i].address.addr + 2, &nodes[i].id, 2);
}

//This will configure UICR / FICR and flash (settings,...) of a node
void CherrySim::flashNode(u32 i) {
	//Configure UICR
	nodes[i].uicr.CUSTOMER[0] = UICR_SETTINGS_MAGIC_WORD; //magicNumber
	nodes[i].uicr.CUSTOMER[1] = 19; //boardType (Simulator board)
	CherrySimUtils::generateBeaconSerialForIndex(i, (char*)(nodes[i].uicr.CUSTOMER + 2)); //serialNumber
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

	//Set the node configuration to default
	if (i < simConfig.numNodes) {
		strcpy(nodes[i].nodeConfiguration, i == 0 ? simConfig.defaultSinkConfigName : simConfig.defaultNodeConfigName);
		//Set the simulated ble stack to default
		nodes[i].bleStackType = simConfig.defaultBleStackType;
	}
	else {
		nodes[i].uicr.CUSTOMER[11] = (u32)DeviceType::ASSET; //deviceType
		strcpy(nodes[i].nodeConfiguration, "prod_asset_nrf52");

		//Set the simulated ble stack to default
		nodes[i].bleStackType = BleStackType::NRF_SD_132_ANY;

		// Set a default network id for assets as they cannot be enrolled
		if (simConfig.defaultNetworkId == 0) {
			nodes[i].uicr.CUSTOMER[9] = 123;
		}
	}
}

void CherrySim::erasePage(u32 pageAddress)
{
	u32* p = (u32*)pageAddress;

	for (u32 i = 0; i < FruityHal::GetCodePageSize() / sizeof(u32); i++) {
		p[i] = 0xFFFFFFFF;
	}
}

void CherrySim::bootCurrentNode()
{
	//Configure FICR
	//We can't do this any earlier because test code might want to change the featureset.
	currentNode->ficr.CODESIZE = chipsetToCodeSize(getChipset_CherrySim());
	currentNode->ficr.CODEPAGESIZE = chipsetToPageSize(getChipset_CherrySim());

	//Initialize UICR
	currentNode->uicr.BOOTLOADERADDR = chipsetToBootloaderAddr(getChipset_CherrySim()); // Set to 0xFFFFFFFFUL to simulate no bootloader, 0x3EC00 simulates a fake bootloader (nrf51)
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

	//Place a new GlobalState instance into our nodeEntry
	if (simGlobalStatePtr != nullptr) {
		simGlobalStatePtr->~GlobalState();
	}

	//Erase bootloader settings page
	erasePage(REGION_BOOTLOADER_SETTINGS_START);

	simGlobalStatePtr = new (&currentNode->gs) GlobalState();

	//Reset our GPIO Peripheral
	CheckedMemset(simGpioPtr, 0x00, sizeof(NRF_GPIO_Type));

	//Create a queue for events	if (simGlobalStatePtr != nullptr) {
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
	GS->moduleAllocator.setMemory(currentNode->moduleMemoryBlock, moduleMemoryBlockSize);
	//Boot the modules
	BootModules();

	//############## Register the simulator terminal and Pre-configure the node

	//Registers Handler for the simulator so that it can react to certain commands to the node
	//TODO: Maybe use a less intrusive way?
	registerSimulatorTerminalHandler();

	//FIXME: Move to runner / tester
	//Lets us do some configuration after the boot
	Conf::getInstance().terminalMode = TerminalMode::PROMPT;
	Conf::getInstance().defaultLedMode = LedMode::OFF;

	char cmd[] = "action this io led off" EOL;
	GS->terminal.ProcessLine(cmd);
}

void CherrySim::resetCurrentNode(RebootReason rebootReason, bool throwException) {
	if (simConfig.verbose) printf("Node %d resetted\n", currentNode->id);

	//Save the node index because it will be gone after node shutdown
	u32 index = currentNode->index;

	//Clean up node
	shutdownCurrentNode();

	//Disconnect all simulator connections to this node
	for (int i = 0; i < currentNode->state.configuredTotalConnectionCount; i++) {
		SoftdeviceConnection* connection = &nodes[index].state.connections[i];
		DisconnectSimulatorConnection(connection, BLE_HCI_CONNECTION_TIMEOUT, BLE_HCI_CONNECTION_TIMEOUT);
	}

	//Boot node again
	setNode(index);
	if (rebootReason != RebootReason::UNKNOWN)
	{
		currentNode->rebootReason = rebootReason;
	}
	bootCurrentNode();

	//throw an exception so we can step out of the current simulation step for that node
	if (throwException)
	{
		throw NodeSystemResetException();
	}
}

void CherrySim::shutdownCurrentNode() {
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

void CherrySim::simulateFlashCommit() {
	if (PSRNG() <= simConfig.asyncFlashCommitTimeProbability) {
		sim_commit_flash_operations();
	}
}

//Calls the system event dispatcher to mark flash operations complete
//The erases/writes themselves are executed immediately at the moment, though
//This will loop until all flash operations (also those that are queued in response to a successfuly operation) are executed
void CherrySim::sim_commit_flash_operations()
{
	if (cherrySimInstance->simConfig.simulateAsyncFlash) {
		while (cherrySimInstance->currentNode->state.numWaitingFlashOperations > 0) {
			DispatchSystemEvents(FruityHal::SystemEvents::FLASH_OPERATION_SUCCESS);
			cherrySimInstance->currentNode->state.numWaitingFlashOperations--;
		}
	}
}

//Uses a list of fails to simulate some successful and some failed flash operations
void CherrySim::sim_commit_some_flash_operations(const uint8_t* failData, uint16_t numMaxEvents)
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
void CherrySim::simulateBroadcast() {
	//Check for other nodes that are scanning and send them the events
	if (currentNode->state.advertisingActive) {
		if (SHOULD_SIM_IV_TRIGGER(currentNode->state.advertisingIntervalMs)) {
			//Distribute the event to all nodes in range
			for (u32 i = 0; i < getNumNodes(); i++) {
				if (i != currentNode->index) {

					//If the other node is scanning
					if (nodes[i].state.scanningActive) {
						//If the random value hits the probability, the event is sent
						double probability = calculateReceptionProbability(currentNode, &nodes[i]);
						if (PSRNG() < probability) {
							simBleEvent s;
							s.globalId = simState.globalEventIdCounter++;
							s.bleEvent.header.evt_id = BLE_GAP_EVT_ADV_REPORT;
							s.bleEvent.header.evt_len = s.globalId;
							s.bleEvent.evt.gap_evt.conn_handle = BLE_CONN_HANDLE_INVALID;

							CheckedMemcpy(&s.bleEvent.evt.gap_evt.params.adv_report.data, &currentNode->state.advertisingData, currentNode->state.advertisingDataLength);
							s.bleEvent.evt.gap_evt.params.adv_report.dlen = currentNode->state.advertisingDataLength;
							CheckedMemcpy(&s.bleEvent.evt.gap_evt.params.adv_report.peer_addr, &currentNode->address, sizeof(ble_gap_addr_t));
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
						if (memcmp(&nodes[i].state.connectingPartnerAddr, &currentNode->address, sizeof(ble_gap_addr_t)) == 0) {
							//If the random value hits the probability, the event is sent
							double probability = calculateReceptionProbability(currentNode, &nodes[i]);
							if (PSRNG() < probability) {

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

//Connects two nodes, the currentNode is the slave
void CherrySim::ConnectMasterToSlave(nodeEntry* master, nodeEntry* slave)
{
	//Increments global counter for global unique connection Handle
	simState.globalConnHandleCounter++;
	if (simState.globalConnHandleCounter > 65000) {
		//We can only simulate this far, output a warning that the state is inconsistent after this point in simulation
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! ATTENTION:Wrapping globalConnHandleCounter !!!!!!!!!!!!!!!!!!!!!!!\n");
		simState.globalConnHandleCounter = 1;
	}

	if (simState.globalConnHandleCounter == 470) {
		u32 i = 0;
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
		SIMEXCEPTION(IllegalStateException);
	}

	freeInConnection->connectionActive = true;
	freeInConnection->rssiMeasurementActive = false;
	freeInConnection->connectionIndex = 0;
	freeInConnection->connectionHandle = simState.globalConnHandleCounter;
	freeInConnection->connectionInterval = UNITS_TO_MSEC(Conf::getInstance().meshMinConnectionInterval, UNIT_1_25_MS); //TODO: which node's params are used?
	freeInConnection->owningNode = slave;
	freeInConnection->partner = master;
	freeInConnection->connectionMtu = GATT_MTU_SIZE_DEFAULT;
	freeInConnection->isCentral = false;

	//Generate an event for the current node
	simBleEvent s2;
	s2.globalId = simState.globalEventIdCounter++;
	s2.bleEvent.header.evt_id = BLE_GAP_EVT_CONNECTED;
	s2.bleEvent.header.evt_len = s2.globalId;
	s2.bleEvent.evt.gap_evt.conn_handle = simState.globalConnHandleCounter;

	s2.bleEvent.evt.gap_evt.params.connected.conn_params.min_conn_interval = slave->state.connectingParamIntervalMs;
	s2.bleEvent.evt.gap_evt.params.connected.conn_params.max_conn_interval = slave->state.connectingParamIntervalMs;
	s2.bleEvent.evt.gap_evt.params.connected.own_addr = FruityHal::Convert(&slave->address);
	s2.bleEvent.evt.gap_evt.params.connected.peer_addr = FruityHal::Convert(&master->address);
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
		SIMEXCEPTION(IllegalStateException);
	}

	freeOutConnection->connectionIndex = connIndex;
	freeOutConnection->connectionActive = true;
	freeOutConnection->rssiMeasurementActive = false;
	freeOutConnection->connectionHandle = simState.globalConnHandleCounter;
	freeOutConnection->connectionInterval = UNITS_TO_MSEC(Conf::getInstance().meshMinConnectionInterval, UNIT_1_25_MS); //TODO: should take the proper interval
	freeOutConnection->owningNode = master;
	freeOutConnection->partner = slave;
	freeOutConnection->connectionMtu = GATT_MTU_SIZE_DEFAULT;
	freeOutConnection->isCentral = true;

	//Save connection references
	freeInConnection->partnerConnection = freeOutConnection;
	freeOutConnection->partnerConnection = freeInConnection;

	//Generate an event for the remote node
	simBleEvent s;
	s.globalId = simState.globalEventIdCounter++;
	s.bleEvent.header.evt_id = BLE_GAP_EVT_CONNECTED;
	s.bleEvent.header.evt_len = s.globalId;
	s.bleEvent.evt.gap_evt.conn_handle = simState.globalConnHandleCounter;

	s.bleEvent.evt.gap_evt.params.connected.conn_params.min_conn_interval = slave->state.connectingParamIntervalMs;
	s.bleEvent.evt.gap_evt.params.connected.conn_params.max_conn_interval = slave->state.connectingParamIntervalMs;
	s.bleEvent.evt.gap_evt.params.connected.own_addr = FruityHal::Convert(&master->address);
	s.bleEvent.evt.gap_evt.params.connected.peer_addr = FruityHal::Convert(&slave->address);
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
	nodeEntry* partnerNode = connection->partner;
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
		SIMEXCEPTION(IllegalStateException);
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

void CherrySim::simulateTimeouts() {
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
	SoftdeviceState &state = currentNode->state;
	while (state.uartReadIndex != state.uartBufferLength && cherrySimInstance->currentNode->state.currentlyEnabledUartInterrupts != 0) {
		UART0_IRQHandler();
	}
}

void CherrySim::SendUartCommand(NodeId nodeId, const u8* message, u32 messageLength)
{
	SoftdeviceState* state = &(cherrySimInstance->findNodeById(nodeId)->state);
	u32 oldBufferLength = state->uartBufferLength;
	state->uartBufferLength += messageLength;

	if (state->uartBufferLength > state->uartBuffer.length) {
		SIMEXCEPTION(MessageTooLongException);
	}
	CheckedMemcpy(state->uartBuffer.getRaw() + oldBufferLength, message, messageLength);
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

void CherrySim::SendUnreliableTxCompleteEvent(nodeEntry* node, int connHandle, u8 packetCount)
{
	if (packetCount > 0) {
		simBleEvent s2;
		s2.globalId = simState.globalEventIdCounter++;
		s2.bleEvent.header.evt_id = BLE_EVT_TX_COMPLETE;
		s2.bleEvent.header.evt_len = s2.globalId;
		s2.bleEvent.evt.common_evt.conn_handle = connHandle;
		s2.bleEvent.evt.common_evt.params.tx_complete.count = packetCount;

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
			if (SHOULD_SIM_IV_TRIGGER(connectionIntervalMs)) {

				//Depending on the number of connections, we send a random amount of packets from the unreliable buffers
				u8 numConnections = getNumSimConnections(currentNode);
				u8 numPacketsToSend;
				u32 unreliablePacketsSent = 0;

				if (numConnections == 1) numPacketsToSend = (u8)PSRNGINT(0, SIM_NUM_UNRELIABLE_BUFFERS);
				else if (numConnections == 2) numPacketsToSend = (u8)PSRNGINT(0, 5);
				else numPacketsToSend = (u8)PSRNGINT(0, 3);

				const double rssiMult = calculateReceptionProbability(connection->owningNode, connection->partner);
				if (rssiMult == 0)
				{
					numPacketsToSend = 0;
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
		if (SHOULD_SIM_IV_TRIGGER(5000)) {
			SoftdeviceConnection* connection = &currentNode->state.connections[i];
			if (connection->connectionActive && connection->rssiMeasurementActive) {
				nodeEntry* master = i == 0 ? connection->partner : currentNode;
				nodeEntry* slave = i == 0 ? currentNode : connection->partner;

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
				if (PSRNG() < simConfig.connectionTimeoutProbabilityPerSec) {
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

	nodeEntry* sender = bufferedPacket->sender;
	nodeEntry* receiver = bufferedPacket->receiver;
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
		Logger::convertBufferToHexString(p_write_params.p_value, p_write_params.len, buffer, sizeof(buffer));
		j["data"] = buffer;
		printf("%s" EOL, j.dump().c_str());
	}

	cherrySimInstance->PacketHandler(sender->id, receiver->id, p_write_params.p_value, p_write_params.len);

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
	BaseConnection *bc = GS->cm.GetConnectionFromHandle(conn_handle);
	MeshAccessConnection *mac = dynamic_cast<MeshAccessConnection*>(bc);
	if (p_write_params.p_value[0] == 0
		&&
		(
			mac == nullptr
			||
			(
				mac->encryptionState != EncryptionState::ENCRYPTED
				&& mac->getAmountOfCorruptedMessaged() == 0
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

	nodeEntry* sender = bufferedPacket->sender;
	nodeEntry* receiver = bufferedPacket->receiver;
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
		Logger::convertBufferToHexString(hvx_params.p_data, (u32)hvx_params.p_len, buffer, 128);
		j["data"] = buffer;
		printf("%s" EOL, j.dump().c_str());
	}

	// This is a workaround for hvxParams keeping only pointer to len.
	cherrySimInstance->PacketHandler(sender->id, receiver->id, hvx_params.p_data, (u32)hvx_params.p_len);

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

//Receives all packets that are send over the mesh and can handle these
//Beware that sender and receiver are per hop. To get original sender and destination, check package contents
void CherrySim::PacketHandler(u32 senderId, u32 receiverId, u8* data, u32 dataLength) {

	connPacketHeader* packet = (connPacketHeader*)data;
	switch (packet->messageType) {
	case MessageType::MODULE_TRIGGER_ACTION: 
		{
			connPacketModule* modPacket = (connPacketModule*)packet;
			break;
		}
	}
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
	if (SHOULD_SIM_IV_TRIGGER(30000) && clcMod != nullptr && currentNode->gs.uartEventHandler != nullptr) {
		currentNode->clcMock.SendPeriodicData();
	}
}
#endif //GITHUB_RELEASE

//################################## Battery Usage Simulation #############################
// Checks the features that are activated on a node and estimates the battery usage
//#########################################################################################

void CherrySim::simulateBatteryUsage()
{
	//Have a look at: https://devzone.nordicsemi.com/b/blog/posts/nrf51-current-consumption-for-common-scenarios
	//or: https://github.com/mwaylabs/fruitymesh/wiki/Battery-Consumption

	//TODO: Make measurements and check online resources to find out current consumption
	//TODO: provide values for nrf51 and nrf52
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
void CherrySim::simulateTimer() {
	//Advance time of this node
	currentNode->state.timeMs += simConfig.simTickDurationMs;

	if (SHOULD_SIM_IV_TRIGGER(100L * MAIN_TIMER_TICK * 10 / ticksPerSecond)) {
		app_timer_handler(nullptr);
	}
}

void CherrySim::simulateWatchDog()
{
#ifdef FM_WATCHDOG_TIMEOUT
	if (simConfig.simulateWatchdog) {
		if (currentNode->state.timeMs - currentNode->lastWatchdogFeedTime > currentNode->watchdogTimeout)
		{
			SIMEXCEPTION(WatchdogTriggeredException);
			resetCurrentNode(RebootReason::WATCHDOG);
		}
	}
#endif
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
	for (u32 i = 0; i < simConfig.numNodes; i++)
	{
		nodes[i].state.validityClusterSize = 0;

		for (u32 k = 0; k < currentNode->state.configuredTotalConnectionCount; k++) {
			nodes[i].state.connections[k].validityClusterSizeToSend = 0;
		}
	}

	//Grab information from the current clusterSize
	for (u32 i = 0; i < simConfig.numNodes; i++)
	{
		nodes[i].state.validityClusterSize = nodes[i].gs.node.clusterSize;

		//printf("NODE %u has clusterSize %d" EOL, nodes[i].id, nodes[i].gs.node.clusterSize);
	}

	//Grab information from the currentClusterInfoUpdatePacket
	for (u32 i = 0; i < simConfig.numNodes; i++)
	{
		nodeEntry* node = nodes + i;

		MeshConnections conns = node->gs.cm.GetMeshConnections(ConnectionDirection::INVALID);
		for (int k = 0; k < conns.count; k++) {
			MeshConnection* conn = conns.connections[k];

			if (conn->handshakeDone()) {
				connPacketClusterInfoUpdate* packet = (connPacketClusterInfoUpdate*)&(conn->currentClusterInfoUpdatePacket);

				conn->validityClusterUpdatesToSend += packet->payload.clusterSizeChange;

				if (packet->payload.clusterSizeChange != 0) {
					//printf("NODE %u to %u: Buffered Packet with change %d" EOL, node->id, conn->partnerId, packet->payload.clusterSizeChange);
				}
			}
		}
	}

	//Grab information from the HighPrioQueue
	for (u32 i = 0; i < simConfig.numNodes; i++)
	{
		nodeEntry* node = nodes + i;

		MeshConnections conns = node->gs.cm.GetMeshConnections(ConnectionDirection::INVALID);
		for (u32 k = 0; k < conns.count; k++)
		{
			MeshConnection* conn = conns.connections[k];

			if (conn->handshakeDone()) {
				PacketQueue* queue = &(conn->packetSendQueueHighPrio);
				for (u32 m = 0; m < queue->_numElements; m++) {
					SizedData data = queue->PeekNext(m);
					if (data.length >= SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED + SIZEOF_CONN_PACKET_CLUSTER_INFO_UPDATE) {
						BaseConnectionSendDataPacked* sendInfo = (BaseConnectionSendDataPacked*)data.data;
						connPacketClusterInfoUpdate* packet = (connPacketClusterInfoUpdate*)(data.data + SIZEOF_BASE_CONNECTION_SEND_DATA_PACKED);
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
	for (u32 i = 0; i < simConfig.numNodes; i++)
	{
		nodeEntry* node = nodes + i;

		for (u32 k = 0; k < currentNode->state.configuredTotalConnectionCount; k++)
		{
			SoftdeviceConnection* sc = &(node->state.connections[k]);
			if (!sc->connectionActive) continue;

			SoftDeviceBufferedPacket* sp = &(sc->reliableBuffers[0]);

			if (sp->sender != nullptr)
			{
				connPacketHeader* header = (connPacketHeader*)sp->data;

				if (header->messageType == MessageType::CLUSTER_INFO_UPDATE)
				{
					connPacketClusterInfoUpdate* packet = (connPacketClusterInfoUpdate*)header;

					BaseConnection* bc = node->gs.cm.GetConnectionFromHandle(sc->connectionHandle);

					//Save the sent clusterUpdate as part of the connection if the MeshConnection still exists and is handshaked
					if (bc != nullptr && bc->connectionType == ConnectionType::FRUITYMESH && bc->handshakeDone())
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
	for (u32 i = 0; i < simConfig.numNodes; i++)
	{
		nodeEntry* node = nodes + i;

		for (u32 k = 0; k < node->eventQueue.size(); k++)
		{
			simBleEvent bleEvent = node->eventQueue[k];
			if (bleEvent.bleEvent.header.evt_id == BLE_GATTS_EVT_WRITE) {
				ble_gatts_evt_t* gattsEvt = (ble_gatts_evt_t*)&bleEvent.bleEvent.evt;

				connPacketHeader* header = (connPacketHeader*)gattsEvt->params.write.data;

				if (header->messageType == MessageType::CLUSTER_INFO_UPDATE)
				{
					connPacketClusterInfoUpdate* packet = (connPacketClusterInfoUpdate*)header;

					BaseConnection* bc = node->gs.cm.GetConnectionFromHandle(gattsEvt->conn_handle);

					//Save the sent clusterUpdate as part of the connection if the MeshConnection still exists and is handshaked
					if (bc != nullptr && bc->connectionType == ConnectionType::FRUITYMESH && bc->handshakeDone())
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
	for (u32 i = 0; i < simConfig.numNodes; i++)
	{
		nodeEntry* node = nodes + i;
		DetermineClusterSizeAndPropagateClusterUpdates(node, nullptr);

	}

	//For each cluster, calculate the totals for each node and check if they match with the clusterSize
	for (u32 i = 0; i < simConfig.numNodes; i++)
	{
		nodeEntry* node = nodes + i;
		u32 realClusterSize = DetermineClusterSizeAndPropagateClusterUpdates(node, nullptr);

		if (realClusterSize != node->state.validityClusterSize) {
			printf("NODE %d has a real cluster size of %u and predicted size of %d, reported cluster size %d" EOL, node->id, realClusterSize, node->state.validityClusterSize, nodes[i].gs.node.clusterSize);
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
MeshConnectionBond findBond(nodeEntry* startNode, nodeEntry* partnerNode)
{
	//TODO: This currently uses nodeIds for matching, but should use something more safe that cannot change

	if (startNode == nullptr || partnerNode == nullptr) SIMEXCEPTION(IllegalStateException);

	MeshConnectionBond bond = { nullptr, nullptr };

	//Find the connection on the startNode
	MeshConnections conns = startNode->gs.cm.GetMeshConnections(ConnectionDirection::INVALID);
	for (int i = 0; i < conns.count; i++) {
		if (conns.connections[i]->handshakeDone() && conns.connections[i]->partnerId == partnerNode->id) {
			bond.startConnection = conns.connections[i];
		}
	}

	//Find the connection on the partnerNode
	MeshConnections partnerConns = partnerNode->gs.cm.GetMeshConnections(ConnectionDirection::INVALID);
	for (int i = 0; i < partnerConns.count; i++) {
		if (partnerConns.connections[i]->handshakeDone() && partnerConns.connections[i]->partnerId == startNode->id) {
			bond.partnerConnection = partnerConns.connections[i];
		}
	}

	return bond;
}

//This will recursively go along all connections and add up the nodes in this cluster
//It will also propagate the cluster size changes along the route
u32 CherrySim::DetermineClusterSizeAndPropagateClusterUpdates(nodeEntry* node, nodeEntry* startNode)
{
	u32 size = 1;

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
		MeshConnection* conn = conns.connections[i];

		//Make sure that we do not go back the connection where we came from
		if (!conn->handshakeDone()) continue;
		if (conn == bond.partnerConnection) continue;

		//Send the size update over this connection as well to propagate it along the route
		conn->validityClusterUpdatesToSend += validitySizeToAdd;

		//Calculate the cluster size and propagate size changes further
		nodeEntry* nextPartner = findNodeById(conn->partnerId);
		size += DetermineClusterSizeAndPropagateClusterUpdates(nextPartner, node);
	}

	return size;
}

//################################## Configuration Management #############################
// 
//#########################################################################################

CherrySim::FeaturesetPointers* getFeaturesetPointers()
{
	std::string configurationName = cherrySimInstance->currentNode->nodeConfiguration;
#ifdef GITHUB_RELEASE
	configurationName = "github_nrf52";
#endif //GITHUB_RELEASE
	auto entry = cherrySimInstance->featuresetPointers.find(configurationName);
	if (entry == cherrySimInstance->featuresetPointers.end())
	{
		printf("Featureset %s was not found!", configurationName.c_str());
		SIMEXCEPTION(IllegalStateException); //Featureset configuration not found
		return nullptr;
	}
	return &(entry->second);
}

//This function is called by every module's SetToDefaults function
//It can override the code defaults with vendor specific configurations
void setFeaturesetConfiguration_CherrySim(ModuleConfiguration* config, void* module)
{
	getFeaturesetPointers()->setFeaturesetConfigurationPtr(config, module);
}

void setBoardConfiguration_CherrySim(BoardConfiguration* config)
{
	getFeaturesetPointers()->setBoardConfigurationPtr(config);
}

uint32_t initializeModules_CherrySim(bool createModule)
{
	return getFeaturesetPointers()->initializeModulesPtr(createModule);
}

DeviceType getDeviceType_CherrySim()
{
	return getFeaturesetPointers()->getDeviceTypePtr();
}

Chipset getChipset_CherrySim()
{
	return getFeaturesetPointers()->getChipsetPtr();
}

FeatureSetGroup getFeatureSetGroup_CherrySim()
{
	return getFeaturesetPointers()->getFeaturesetGroupPtr();
}

//This function is responsible for setting all the BLE Stack dependent configurations according to the datasheet of the ble stack used
void CherrySim::SetBleStack(nodeEntry* node)
{
	//The nRF51 S130
	if (node->bleStackType == BleStackType::NRF_SD_130_ANY) {
		node->bleStackMaxPeripheralConnections = 1;
		node->bleStackMaxCentralConnections = 3;
		node->bleStackMaxTotalConnections = 4;
	}
	//The nRF52 S132
	else if (node->bleStackType == BleStackType::NRF_SD_132_ANY) {
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
	for (u32 i = 0; i < simConfig.numNodes; i++) {
		if (nodes[i].gs.node.clusterSize != simConfig.numNodes) {
			return false;
		}
	}
	return true;
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
	for (u32 i = 0; i < simConfig.numNodes; i++)
	{
		networkIds.insert(nodes[i].gs.node.configuration.networkId);
		clusterIds.insert({ nodes[i].gs.node.clusterId, nodes[i].gs.node.configuration.networkId });
	}

	return networkIds.size() == clusterIds.size();
}

bool CherrySim::IsClusteringDoneWithExpectedNumberOfClusters(int clusterAmount)
{
	std::set<ClusterNetworkPair> clusterIds;
	for (u32 i = 0; i < simConfig.numNodes; i++)
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

float CherrySim::GetDistanceBetween(const nodeEntry* nodeA, const nodeEntry* nodeB) {
	float distX = std::abs(nodeA->x - nodeB->x) * simConfig.mapWidthInMeters;
	float distY = std::abs(nodeA->y - nodeB->y) * simConfig.mapHeightInMeters;
	float distZ = std::abs(nodeA->z - nodeB->z);
	float dist = sqrt(
	    distX * distX
	  + distY * distY
	  + distZ * distZ
	);

	return dist;
}

float CherrySim::GetReceptionRssi(const nodeEntry* sender, const nodeEntry* receiver) {
	return GetReceptionRssi(sender, receiver, sender->gs.boardconf.configuration.calibratedTX, Conf::defaultDBmTX);
}

float CherrySim::GetReceptionRssi(const nodeEntry* sender, const nodeEntry* receiver, int8_t senderDbmTx, int8_t senderCalibratedTx) {
	// If either the sender or the receiver has the other marked as as a impossibleConnection, the rssi is set to a unconnectable level.
	if (   std::find(sender  ->impossibleConnection.begin(), sender  ->impossibleConnection.end(), receiver->index) != sender  ->impossibleConnection.end()
		|| std::find(receiver->impossibleConnection.begin(), receiver->impossibleConnection.end(), sender  ->index) != receiver->impossibleConnection.end())
	{
		return -10000;
	}
	float dist = GetDistanceBetween(sender, receiver);
	if (!simConfig.rssiNoise)
	{
		return (senderDbmTx + senderCalibratedTx) - log10(dist) * 10 * N;
	}
	/*The rssi noise is modeled based on the paper http://www1.cs.columbia.edu/~andreaf/downloads/01331706.pdf */
	float rssi = (senderDbmTx + senderCalibratedTx) - log10(dist) * 10 * N;
	float rssiNoiseStd = (float)(0.0497 * rssi + 6.3438);
	float randomNoise = (float)cherrySimInstance->simState.rnd.nextNormal(0.0, rssiNoiseStd);
	return rssi + randomNoise;
}

double CherrySim::calculateReceptionProbability(const nodeEntry* sendingNode, const nodeEntry* receivingNode) {
	//TODO: Add some randomness and use a function to do the mapping
	float rssi = GetReceptionRssi(sendingNode, receivingNode);

	if (rssi > -60) return 0.9;
	else if (rssi > -80) return 0.8;
	else if (rssi > -85) return 0.5;
	else if (rssi > -90) return 0.3;
	else return 0;
}

SoftdeviceConnection* CherrySim::findConnectionByHandle(nodeEntry* node, int connectionHandle) {
	for (u32 i = 0; i < node->state.configuredTotalConnectionCount; i++) {
		if (node->state.connections[i].connectionActive && node->state.connections[i].connectionHandle == connectionHandle) {
			return &node->state.connections[i];
		}
	}
	return nullptr;
}

nodeEntry* CherrySim::findNodeById(int id) {
	for (u32 i = 0; i < getNumNodes(); i++) {
		if (nodes[i].id == id) {
			return &nodes[i];
		}
	}
	return nullptr;
}

u8 CherrySim::getNumSimConnections(const nodeEntry* node) {
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

void CherrySim::enableTagForAll(const char * tag)
{
	for (u32 i = 0; i < getNumNodes(); i++)
	{
		nodes[i].gs.logger.enableTag(tag);
	}
}

void CherrySim::disableTagForAll(const char * tag)
{
	for (u32 i = 0; i < getNumNodes(); i++)
	{
		nodes[i].gs.logger.disableTag(tag);
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
	if (!emptySlot) SIMEXCEPTION(PacketStatBufferSizeNotEnough);

	*emptySlot = *packet;
}

//Allows us to put a packet into the packet statistics. It will count all similar packets in slots depending on the messageType
//Given stat array has to be of size PACKET_STAT_SIZE
//TODO: This must only be called for unencrypted connections that send mesh-compatible packets
//TODO: Should also be used to check what kind of messages a node generates
void CherrySim::AddMessageToStats(PacketStat* statArray, u8* message, u16 messageLength)
{
	if (!simConfig.enableSimStatistics) return;

	connPacketSplitHeader* splitHeader = (connPacketSplitHeader*)message;
	connPacketHeader* header = nullptr;
	connPacketModule* moduleHeader = nullptr;

	PacketStat packet;

	//Check if it is the first part of a split message or not
	if (splitHeader->splitMessageType == MessageType::SPLIT_WRITE_CMD && splitHeader->splitCounter == 0) {
		packet.isSplit = true;
		header = (connPacketHeader*)(message + SIZEOF_CONN_PACKET_SPLIT_HEADER);
	}
	else if (splitHeader->splitMessageType == MessageType::SPLIT_WRITE_CMD || splitHeader->splitMessageType == MessageType::SPLIT_WRITE_CMD_END) {
		//Do nothing for now as we only count the first part
		return;
		//A normal not split packet
	}
	else {
		header = (connPacketHeader*)message;
		packet.isSplit = false;
	}

	//Fill in basic packet info
	packet.messageType = header->messageType;
	packet.count = 1;

	//Fill in additional info if we have a module message
	if (packet.messageType >= MessageType::MODULE_CONFIG && packet.messageType <= MessageType::COMPONENT_SENSE) {
		moduleHeader = (connPacketModule*)header;
		packet.moduleId = moduleHeader->moduleId;
		packet.actionType = moduleHeader->actionType;
	}

	//Add the packet to our stat array
	AddPacketToStats(statArray, &packet);
}

void CherrySim::PrintPacketStats(NodeId nodeId, char* statId)
{
	if (!simConfig.enableSimStatistics) return;

	PacketStat* stat = nullptr;
	PacketStat sumStat[PACKET_STAT_SIZE];

	//We must sum up all stat packets of all nodes to get a stat that covers all nodes
	if (nodeId == 0) {
		stat = sumStat;

		for (u32 i = 0; i < simConfig.numNodes; i++) {
			for (u32 j = 0; j < PACKET_STAT_SIZE; j++) {
				if (strcmp("SENT", statId) == 0) AddPacketToStats(sumStat, nodes[i].sentPackets + j);
				if (strcmp("ROUTED", statId) == 0) AddPacketToStats(sumStat, nodes[i].routedPackets + j);
			}
		}
	}
	//We simply select the stat from the given nodeId
	else {
		nodeEntry* node = findNodeById(nodeId);
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
