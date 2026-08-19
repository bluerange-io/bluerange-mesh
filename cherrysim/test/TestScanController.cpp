////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// ** BlueRange Mesh – Community Edition (CE)
// ** Copyright (c) 2015-2021 MWAY DIGITAL GmbH, Germany
// ** Copyright (c) 2021-2026 BlueRange GmbH, Germany
// **
// ** This file is part of BlueRange Mesh Community Edition (formerly known as
// ** FruityMesh).
// **
// ** BlueRange Mesh Community Edition is free software: you can redistribute it
// ** and/or modify it under the terms of the GNU General Public License as
// ** published by the Free Software Foundation, either version 3 of the
// ** License, or (at your option) any later version.
// **
// ** BlueRange Mesh Community Edition is distributed in the hope that it will
// ** be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// ** See the GNU General Public License for more details.
// **
// ** You should have received a copy of the GNU General Public License along
// ** with this program. If not, see https://www.gnu.org/licenses/.
// **
// ** IMPORTANT:
// ** Any modification, extension, or derivative work of this file MUST also be
// ** licensed under the GNU General Public License v3 or later and the complete
// ** corresponding source code MUST be made available.
// **
// ** Commercial Use:
// ** If you wish to use this software without the obligations of the GPLv3
// ** (including source code disclosure), a commercial license for
// ** BlueRange Mesh OEM Edition is required.
// **
// ** License violations automatically terminate your rights under this license
// ** and may result in legal action under applicable law.
// ** For further information please use the contact form at:
// ** https://bluerange.io/en/contact
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include <CherrySimTester.h>
#include <CherrySimUtils.h>
#include <Node.h>
#include <ScanController.h>

static void simulateAndCheckScanning(int simulate_time, bool is_scanning_active, CherrySimTester &tester)
{
    tester.SimulateForGivenTime(simulate_time);
    NodeIndexSetter setter(0);
    ASSERT_TRUE(tester.sim->currentNode->state.scanningActive == is_scanning_active);
}

static void simulateAndCheckWindow(int simulate_time, int window, CherrySimTester &tester)
{
    tester.SimulateForGivenTime(simulate_time);
    NodeIndexSetter setter(0);
    ASSERT_EQ(tester.sim->currentNode->state.scanWindowMs, window);
}

static ScanJob * AddJob(ScanJob &scan_job, const CherrySimTester &tester)
{
    ScanJob * p_job;
    NodeIndexSetter setter(0);
    p_job = tester.sim->currentNode->gs.scanController.AddJob(scan_job);
    return p_job;
}

static void RemoveJob(ScanJob * p_scan_job, const CherrySimTester &tester)
{
    NodeIndexSetter setter(0);
    tester.sim->currentNode->gs.scanController.RemoveJob(p_scan_job);
}

static void ForceStopAllScanJobs(const CherrySimTester &tester)
{
    NodeIndexSetter setter(0);
    for (int i = 0; i < tester.sim->currentNode->gs.scanController.GetAmountOfJobs(); i++)
    {
        tester.sim->currentNode->gs.scanController.RemoveJob(tester.sim->currentNode->gs.scanController.GetJob(i));
    }
}

TEST(TestScanController, TestIfScannerGetsEnabled) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 1;
    simConfig.SetToPerfectConditions();
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);
    ForceStopAllScanJobs(tester);

    ScanJob job;
    job.timeMode = ScanJobTimeMode::ENDLESS;
    job.interval = MSEC_TO_UNITS(100, CONFIG_UNIT_0_625_MS);
    job.window = MSEC_TO_UNITS(50, CONFIG_UNIT_0_625_MS);
    job.state = ScanJobState::ACTIVE;
    job.type = ScanState::CUSTOM;
    AddJob(job, tester);

    simulateAndCheckWindow(1000, 50, tester);
}

TEST(TestScanController, TestScannerStopsAfterTimeoutTime) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 1;
    //testerConfig.verbose = true;
    simConfig.SetToPerfectConditions();
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.sim->nodes[0].nodeConfiguration = "prod_sink_nrf52";
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);
    ForceStopAllScanJobs(tester);

    ScanJob job;
    job.timeMode = ScanJobTimeMode::TIMED;
    job.timeLeftDs = SEC_TO_DS(10);
    job.interval = MSEC_TO_UNITS(50, CONFIG_UNIT_0_625_MS);
    job.window = MSEC_TO_UNITS(50, CONFIG_UNIT_0_625_MS);
    job.state = ScanJobState::ACTIVE;
    job.type = ScanState::CUSTOM;
    AddJob(job, tester);


    simulateAndCheckWindow(1000, 50, tester);
    simulateAndCheckScanning(10000, false, tester);
}

TEST(TestScanController, TestScannerChooseJobWithHighestDutyCycle) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 1;
    //testerConfig.verbose = true;
    simConfig.SetToPerfectConditions();
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);
    ForceStopAllScanJobs(tester);

    ScanJob job;
    job.timeMode = ScanJobTimeMode::ENDLESS;
    job.interval = MSEC_TO_UNITS(100, CONFIG_UNIT_0_625_MS);
    job.window = MSEC_TO_UNITS(50, CONFIG_UNIT_0_625_MS);
    job.state = ScanJobState::ACTIVE;
    job.type = ScanState::CUSTOM;
    AddJob(job, tester);


    simulateAndCheckWindow(1000, 50, tester);

    job.window = MSEC_TO_UNITS(40, CONFIG_UNIT_0_625_MS);
    AddJob(job, tester);

    simulateAndCheckWindow(1000, 50, tester);

    job.window = MSEC_TO_UNITS(60, CONFIG_UNIT_0_625_MS);
    AddJob(job, tester);

    simulateAndCheckWindow(1000, 60, tester);

    job.timeMode = ScanJobTimeMode::TIMED;
    job.timeLeftDs = SEC_TO_DS(3);
    job.window = MSEC_TO_UNITS(70, CONFIG_UNIT_0_625_MS);
    AddJob(job, tester);

    simulateAndCheckWindow(1000, 70, tester);

    // previous job should timeout
    simulateAndCheckWindow(2000, 60, tester);
}

TEST(TestScanController, TestScannerWillStopOnceAllJobsTimeout) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 1;
    //testerConfig.verbose = true;
    simConfig.SetToPerfectConditions();
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);
    ForceStopAllScanJobs(tester);

    ScanJob job;
    job.timeMode = ScanJobTimeMode::TIMED;
    job.timeLeftDs = SEC_TO_DS(10);
    job.interval = MSEC_TO_UNITS(100, CONFIG_UNIT_0_625_MS);
    job.window = MSEC_TO_UNITS(50, CONFIG_UNIT_0_625_MS);
    job.state = ScanJobState::ACTIVE;
    job.type = ScanState::CUSTOM;
    AddJob(job, tester);

    simulateAndCheckScanning(1000, true, tester);

    job.timeMode = ScanJobTimeMode::TIMED;
    job.timeLeftDs = SEC_TO_DS(10);
    AddJob(job, tester);

    simulateAndCheckScanning(9000, true, tester);
    simulateAndCheckScanning(1000, false, tester);
}

TEST(TestScanController, TestScannerWillStopOnceAllJobsAreDeleted) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 1;
    //testerConfig.verbose = true;
    simConfig.SetToPerfectConditions();
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);
    ForceStopAllScanJobs(tester);

    ScanJob job;
    job.timeMode = ScanJobTimeMode::ENDLESS;
    job.interval = MSEC_TO_UNITS(100, CONFIG_UNIT_0_625_MS);
    job.window = MSEC_TO_UNITS(50, CONFIG_UNIT_0_625_MS);
    job.state = ScanJobState::ACTIVE;
    job.type = ScanState::CUSTOM;
    ScanJob * p_job_1 = AddJob(job, tester);


    simulateAndCheckScanning(1000, true, tester);
    ScanJob * p_job_2 = AddJob(job, tester);


    simulateAndCheckScanning(1000, true, tester);
    RemoveJob(p_job_1, tester);

    simulateAndCheckScanning(1000, true, tester);
    RemoveJob(p_job_2, tester);

    simulateAndCheckScanning(1000, false, tester);
}