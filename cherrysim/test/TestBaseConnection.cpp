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
    tester.SimulateUntilClusteringDone(10 * 1000);

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