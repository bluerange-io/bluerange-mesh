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
#include "gtest/gtest.h"

#include <HelperFunctions.h>
#include <CherrySimTester.h>
#include <IoModule.h>

TEST(TestIoModule, TestCommands) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 2 });
    simConfig.SetToPerfectConditions();
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    //Could modify the default test or sim config here,...
    tester.Start();

    tester.SimulateUntilClusteringDone(1000 * 1000);
    tester.SendTerminalCommand(2, "action this io led on");
    tester.SimulateUntilMessageReceived(500, 2, "set_led_result");

    tester.SendTerminalCommand(1, "action 2 io pinset 1 high 2 high");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":2,\"type\":\"set_pin_config_result\",\"module\":6");

    // Reading a pin will configure it as an input and as there is no one setting it to high, it will be low
    // If tested for pinset low it will not work.
    // However, it's enough to test the command with response format.
    tester.SendTerminalCommand(1, "action 2 io pinread 1 2");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":2,\"type\":\"pin_level_result\",\"module\":6,\"pins\":[{\"pin_number\":1,\"pin_level\":0},{\"pin_number\":2,\"pin_level\":0}]");

    // Test min. number of arguments
    tester.SendTerminalCommand(1, "action 2 io pinread 1");
    tester.SimulateUntilMessageReceived(100 * 1000, 1, "{\"nodeId\":2,\"type\":\"pin_level_result\",\"module\":6,\"pins\":[{\"pin_number\":1,\"pin_level\":0}]");

    // Test max. number of arguments
    tester.SendTerminalCommand(1, "action 2 io pinread 1 2 3 4 5");
    tester.SimulateUntilMessageReceived(
        100 * 1000, 1, 
        "{\"nodeId\":2,\"type\":\"pin_level_result\",\"module\":6,\"pins\":"
        "[{\"pin_number\":1,\"pin_level\":0},{\"pin_number\":2,\"pin_level\":0},"
        "{\"pin_number\":3,\"pin_level\":0},{\"pin_number\":4,\"pin_level\":0},"
        "{\"pin_number\":5,\"pin_level\":0}]"
    );
}

TEST(TestIoModule, TestIdentifyCommands)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = false;

    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 2 });
    simConfig.SetToPerfectConditions();

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.Start();
    tester.SimulateUntilClusteringDone(60 * 1000);

    {
        NodeIndexSetter nodeIndexSetter{0};
        GS->logger.EnableTag("IOMOD");
    }

    // Turn identification ON on node 1 from node 2.
    tester.SendTerminalCommand(2, "action 1 io identify on");

    // Check that identification was turned on on node 1.
    tester.SimulateUntilRegexMessageReceived(60 * 1000, 1, "identification started by SET_IDENTIFICATION message");

    // Turn identification OFF on node 1 from node 2.
    tester.SendTerminalCommand(2, "action 1 io identify off");

    // Check that identification was turned off on node 1.
    tester.SimulateUntilRegexMessageReceived(60 * 1000, 1, "identification stopped by SET_IDENTIFICATION message");
}

#if defined(PROD_SINK_NRF52) && defined(PROD_ASSET_NRF52)
TEST(TestIoModule, TestLedOnAssetOverMeshAccessSerialConnectWithOrgaKey)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose               = true;
    SimConfiguration simConfig    = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId          = 0;
    simConfig.defaultNetworkId    = 0;
    simConfig.preDefinedPositions = {{0.1, 0.1}, {0.2, 0.1}, {0.3, 0.1}};
    simConfig.nodeConfigName.insert({"prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({"prod_mesh_nrf52", 1});
    simConfig.nodeConfigName.insert({"prod_asset_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateGivenNumberOfSteps(10);

    tester.SendTerminalCommand(
        1, "action this enroll basic BBBBB 1 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 "
           "22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 "
           "01:00:00:00:01:00:00:00:01:00:00:00:01:00:00:00 10 0 0");
    tester.SendTerminalCommand(
        2, "action this enroll basic BBBBC 2 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 "
           "22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 "
           "02:00:00:00:02:00:00:00:02:00:00:00:02:00:00:00 10 0 0");
    tester.SendTerminalCommand(
        3, "action this enroll basic BBBBD 33000 10000 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 "
           "22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 "
           "03:00:00:00:03:00:00:00:03:00:00:00:03:00:00:00 10 0 0");

    tester.SimulateUntilMessageReceived(100 * 1000, 1, "clusterSize\":2"); // Wait until the nodes have clustered.

    RetryOrFail<TimeoutException>(
        32, [&] {
            // Initiate a connection from the sink on the mesh node to the asset tag using the organization key.
            tester.SendTerminalCommand(
                1, "action 2 ma serial_connect BBBBD 4 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 33000 20 13");
        }, [&] {
            tester.SimulateUntilMessageReceived(10 * 1000, 1,
                                                R"({"type":"serial_connect_response","module":10,"nodeId":2,)"
                                                R"("requestHandle":13,"code":0,"partnerId":33000})");
        });

    // Send a 'led on' from the sink to the asset tag using it's partner id.
    tester.SendTerminalCommand(1, "action 33000 io led on 55");
    // Wait for the response on the sink.
    tester.SimulateUntilMessageReceived(
        10 * 1000, 1, R"({"nodeId":33000,"type":"set_led_result","module":6,"requestHandle":55,"code":0})");
}
#endif

#if defined(PROD_MESH_NRF52840_SDK17)
TEST(TestIoModule, TestRegisterAccessBasic)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;

    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52840_sdk17", 1 });
    simConfig.SetToPerfectConditions();

    //The pin settings for the second node are loaded from board_19, which is the simulator board
    //this board configures some acessible virtual pins that can be checked for their state

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.Start();

    // Test reading IO_OUTPUT_NUM
    tester.SendTerminalCommand(1, "component_act 1 6 read 0 100 01");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, R"({"nodeId":1,"type":"component_sense","module":6,"requestHandle":0,"actionType":2,"component":"0x0000","register":"0x0064","payload":"Ag==")");

    // Test reading IO_INPUT_NUM
    tester.SendTerminalCommand(1, "component_act 1 6 read 0 101 01");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, R"({"nodeId":1,"type":"component_sense","module":6,"requestHandle":0,"actionType":2,"component":"0x0000","register":"0x0065","payload":"Aw==")");

}

//This tests that digital out registers can be written, read and change their state accordingly
TEST(TestIoModule, TestRegisterAccessForDigitalOut)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;

    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1 });
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52840_sdk17", 1 });
    simConfig.SetToPerfectConditions();

    //The pin settings for the second node are loaded from board_19, which is the simulator board
    //this board configures some acessible virtual pins that can be checked for their state

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.Start();
    tester.SimulateUntilClusteringDone(60 * 1000);

    tester.sim->EnableTagForAll("IOMOD");

    {
        // Set Register 20000 to 1
        tester.SendTerminalCommand(1, "component_act 2 6 writeack 0 20000 01");
        tester.SimulateUntilMessageReceived(10 * 1000, 1, R"({"nodeId":2,"type":"component_sense","module":6,"requestHandle":0,"actionType":4,"component":"0x0000","register":"0x4E20")");

        // The corresponding gpio should now be active
        PinSettings& settings = tester.sim->nodes[1].gpioInitializedPins.at(100);
        if (settings.currentState != true) SIMEXCEPTION(IllegalStateException);
    }

    {
        // Set Register 20000 to 0 and 20001 to 1
        tester.SendTerminalCommand(1, "component_act 2 6 writeack 0 20000 00:01");
        tester.SimulateUntilMessageReceived(10 * 1000, 1, R"({"nodeId":2,"type":"component_sense","module":6,"requestHandle":0,"actionType":4,"component":"0x0000","register":"0x4E20")");

        // The corresponding first gpio should now be inactive again
        PinSettings& settings = tester.sim->nodes[1].gpioInitializedPins.at(100);
        if (settings.currentState != false) SIMEXCEPTION(IllegalStateException);

        //Read back the two registers
        tester.SendTerminalCommand(1, "component_act 2 6 read 0 20000 02");

        //The states should be as written
        tester.SimulateUntilMessageReceived(10 * 1000, 1, R"({"nodeId":2,"type":"component_sense","module":6,"requestHandle":0,"actionType":2,"component":"0x0000","register":"0x4E20","payload":"AAE=")");

    }
}

//This tests that digital input registers can be read
TEST(TestIoModule, TestRegisterAccessForDigitalIn)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;

    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1 });
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52840_sdk17", 1 });
    simConfig.SetToPerfectConditions();

    //The pin settings for the second node are loaded from board_19, which is the simulator board
    //this board configures some acessible virtual pins that can be checked for their state

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.Start();
    tester.SimulateUntilClusteringDone(60 * 1000);

    tester.sim->EnableTagForAll("IOMOD");

    {
        // Read the gpio pin state of the second input pin
        tester.SendTerminalCommand(1, "component_act 2 6 read 0 30001 01");
        tester.SimulateUntilMessageReceived(10 * 1000, 1, R"({"nodeId":2,"type":"component_sense","module":6,"requestHandle":0,"actionType":2,"component":"0x0000","register":"0x7531","payload":"AA==")");
    }

    {
        //Modify the pin state of the second input pin
        tester.sim->nodes[1].gpioInitializedPins.at(103).currentState = true;
        
        // Read the gpio pin state of both input pins
        tester.SendTerminalCommand(1, "component_act 2 6 read 0 30000 02");
        tester.SimulateUntilMessageReceived(10 * 1000, 1, R"({"nodeId":2,"type":"component_sense","module":6,"requestHandle":0,"actionType":2,"component":"0x0000","register":"0x7530","payload":"AAE=")");
    }

    {
        //Modify the pin state of the first input pin as well
        tester.sim->nodes[1].gpioInitializedPins.at(102).currentState = true;

        // Read the gpio pin state of both input pins again
        tester.SendTerminalCommand(1, "component_act 2 6 read 0 30000 02");
        tester.SimulateUntilMessageReceived(10 * 1000, 1, R"({"nodeId":2,"type":"component_sense","module":6,"requestHandle":0,"actionType":2,"component":"0x0000","register":"0x7530","payload":"AQE=")");
    }
}

//This uses AutoSense to capture events for a toggle input and uses AutoAct to set an output pin
TEST(TestIoModule, TestRegisterInToOutLink)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;

    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52840_sdk17", 1 });
    simConfig.SetToPerfectConditions();

    //The pin settings for the second node are loaded from board_19, which is the simulator board
    //this board configures some acessible virtual pins that can be checked for their state

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);

    tester.Start();

    tester.sim->EnableTagForAll("IOMOD");

    //Check that setting the output pin works as expected
    {
        //Pin set to 1
        tester.SendTerminalCommand(1, "component_act this 6 writeack 0 20000 01");
        tester.SimulateUntilMessageReceived(10 * 1000, 1, "component_sense");
        tester.SendTerminalCommand(1, "component_act this 6 read 0 20000 01");
        tester.SimulateUntilMessageReceived(10 * 1000, 1, R"({"nodeId":1,"type":"component_sense","module":6,"requestHandle":0,"actionType":2,"component":"0x0000","register":"0x4E20","payload":"AQ=="})");

        //Pin set to 0
        tester.SendTerminalCommand(1, "component_act this 6 writeack 0 20000 00");
        tester.SimulateUntilMessageReceived(10 * 1000, 1, "component_sense");
        tester.SendTerminalCommand(1, "component_act this 6 read 0 20000 01");
        tester.SimulateUntilMessageReceived(10 * 1000, 1, R"({"nodeId":1,"type":"component_sense","module":6,"requestHandle":0,"actionType":2,"component":"0x0000","register":"0x4E20","payload":"AA=="})");
    }

    //Configure AutoSense to report the first toggle input on change
    AutoSenseTableEntryBuilder ast;
    ast.entry.destNodeId = 0;
    ast.entry.moduleId = Utility::GetWrappedModuleId(ModuleId::IO_MODULE);
    ast.entry.component = 0;
    ast.entry.register_ = IoModule::REGISTER_DIO_TOGGLE_PAIR_START;
    ast.entry.length = 1;
    ast.entry.dataType = DataTypeDescriptor::U8_LE;
    ast.entry.pollingIvDs = 1;
    ast.entry.reportingIvDs = 1;
    ast.entry.reportFunction = AutoSenseFunction::ON_CHANGE_RATE_LIMITED;
    tester.SendTerminalCommand(1, "action this autosense set_autosense_entry 0 0 %s", ast.getEntry().data());

    tester.SimulateUntilMessageReceived(10 * 1000, 1, "set_autosense_entry_result");

    //Configure AutoAct to relay the message into the first output register
    AutoActTableEntryBuilder aat;
    aat.entry.receiverNodeIdFilter = 0;
    aat.entry.moduleIdFilter = Utility::GetWrappedModuleId(ModuleId::IO_MODULE);
    aat.entry.componentFilter = 0; //0xF7 modbus device, 0x03 READ_SINGLE_HOLDING_REGISTER
    aat.entry.registerFilter = IoModule::REGISTER_DIO_TOGGLE_PAIR_START;
    aat.entry.targetModuleId = Utility::GetWrappedModuleId(ModuleId::IO_MODULE);
    aat.entry.targetComponent = 0; //0xF7 modbus device, 0x03 WRITE_SINGLE_HOLDING_REGISTER
    aat.entry.targetRegister = IoModule::REGISTER_DIO_OUTPUT_STATE_START;
    aat.entry.orgDataType = DataTypeDescriptor::U8_LE;
    aat.entry.targetDataType = DataTypeDescriptor::U8_LE;
    aat.entry.flags = 0;
    aat.addFunctionNoop();

    tester.SendTerminalCommand(1, "action this autoact set_autoact_entry 0 0 %s", aat.getEntry().data());

    tester.SimulateUntilMessageReceived(10 * 1000, 1, "set_autoact_entry_result");

    tester.SimulateForGivenTime(1 * 1000);

    //Set fake timestamps to make sure the toggle pair triggers
    {
        NodeIndexSetter setter(0);
        IoModule* ioMod = (IoModule*)GS->node.GetModuleById(ModuleId::IO_MODULE);
        ioMod->digitalInPinSettings[0].lastActiveTimeDs = 100;
        ioMod->digitalInPinSettings[1].lastActiveTimeDs = 200;
    }

    tester.SendTerminalCommand(1, "sep");

    //AutoSense correctly reports the DIO_TOGGLE_PAIR_1
    tester.SimulateUntilMessageReceived(10 * 1000, 1, R"({"nodeId":1,"type":"component_sense","module":6,"requestHandle":0,"actionType":0,"component":"0x0000","register":"0x7594","payload":"AQ=="})");

    //Now, we expect that AutoAct generates the write to register 20000 for us
    //Read register 20000 to see that it is now set to 1
    tester.SendTerminalCommand(1, "component_act this 6 read 0 20000 01");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, R"({"nodeId":1,"type":"component_sense","module":6,"requestHandle":0,"actionType":2,"component":"0x0000","register":"0x4E20","payload":"AQ=="})");
}

#endif //defined(PROD_MESH_NRF52840_SDK17)
