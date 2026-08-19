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


TEST(TestBaseConnection, TestSimpleTransmissions) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();

    simConfig.SetToPerfectConditions();
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1 });
    //testerConfig.verbose = true;

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.Start();
    tester.SimulateUntilClusteringDone(1 * 60 * 1000);

    //We modify the MTU of the connection and set it so that it is too small for a normal packet
    //smallest value is 10 and componsated with three 3 byte ATT_HEADER_SIZE, so payload mtu would be 7
    for (int i = 0; i < SIM_MAX_CONNECTION_NUM; i++) {
        if (tester.sim->nodes[0].state.connections[i].connectionActive) {
            tester.sim->nodes[0].state.connections[i].connectionMtu = 7;
        }
    }

    // GATT WRITE ERROR is logged via ERROR tag, which is correct behavior.
    Exceptions::ExceptionDisabler<ErrorLoggedException> ele;

    //Send a message to node 2
    tester.SendTerminalCommand(1, "action 2 status get_status");

    //We check, that the connection gets disconnected
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "GATT WRITE ERROR");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "Deleted MeshConnection");

    //We wait until they are connected again
    tester.SimulateUntilClusteringDone(10 * 1000);
}