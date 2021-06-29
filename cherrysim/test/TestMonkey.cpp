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
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include "gtest/gtest.h"
#include "CherrySimTester.h"
#include "CherrySimUtils.h"

const std::vector<std::string> templates{
/****************/
/* DEBUG MODULE */
/****************/
//Documented
    "action [[[0-4]]] debug get_buffer",
    "action [[[0-4]]] debug ping [[[0-20]]] {{{r|u}}}",
    "action [[[0-4]]] debug pingpong [[[0-20]]] {{{r|u}}}",
    "lping [[[1-1234]]] {{{r|u}}}",
    "action [[[0-4]]] debug flood [[[0-4]]] {{{0|2|3|4}}} [[[1-100]]]",
    "saverec [[[1-1234]]] AA:BB:CC",
    "getrec [[[1-1234]]]",
    "delrec [[[1-1234]]]",
    "advjobs",
    "heap",
    "memorymap",
//    "readblock {{{[[[1-1234]]]|uicr|ficr|ram}}} [[[1-1234]]]",    //Seems to be incorrectly implemented! Note: arg parameter 1!
//Undocumented
    "action [[[0-4]]] debug reset_connection_loss_counter",
    "action [[[0-4]]] debug get_stats",
//    "action [[[0-4]]] debug hardfault", //Disabled on purpose.
    "floodstat",
    "log_error [[[0-1000000000]]] [[[0-65535]]]",
    "send {{{r|u|b}}} [[[0-255]]]",
    "advadd [[[0-255]]] [[[0-255]]] [[[0-255]]]",
    "advrem [[[0-3]]]",
    "feed",
//    "nswrite [[[0-1000]]] AA:BB:CC:DD:EE:FF",    //dangerzone proposal
//    "erasepage [[[0-10]]]",    //dangerzone proposal
//    "erasepages [[[0-10]]] [[[0-10]]]",    //dangerzone proposal
    "filltx",
    "getpending",
//    "writedata [[[0-1000]]] AA:BB:CC:DD:EE:FF", //dangerzone proposal
    "printqueue [[[0-255]]]",

/*********************/
/* ENROLLMENT MODULE */
/*********************/
//Documented
    "action [[[0-4]]] enroll basic %%%SERIAL%%% [[[2-1234]]] [[[2-1234]]] 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22 33:33:33:33:33:33:33:33:33:33:33:33:33:33:33:33 0A:00:00:00:0A:00:00:00:0A:00:00:00:0A:00:00:00 [[[0-255]]] [[[0-1]]] [[[0-255]]]",
    "action [[[0-4]]] enroll remove %%%SERIAL%%%",

/*************/
/* IO MODULE */
/*************/
//Documented
    "action [[[0-4]]] io led {{{on|off|connections}}}",
    "action [[[0-4]]] io pinset [[[1-128]]] {{{low|high}}}",
    "action [[[0-4]]] io identify {{{on|off}}}",
//Undocumented

/**********************/
/* MESH ACCESS MODULE */
/**********************/
//Documented
    "action [[[0-4]]] ma connect 00:00:00:{{{01|02|03|04}}}:00:00",
    "action [[[0-4]]] ma disconnect 00:00:00:{{{01|02|03|04}}}:00:00",
//Undocumented
    "maconn 00:00:00:{{{01|02|03|04}}}:00:00 [[[0-1234]]]",
    "malog",

/**********/
/* MODULE */
/**********/
//Documented
    //"set_config [[[0-4]]] {{{adv|debug|enroll|io|ma|scan|status}}} AA:BB:CC:DD:EE:FF", //dangerzone proposal
    "get_config [[[0-4]]] {{{adv|debug|enroll|io|ma|scan|status}}}",
    "set_active [[[0-4]]] {{{adv|debug|enroll|io|ma|scan|status}}} {{{on|off}}}",
//Undocumented

/********/
/* NODE */
/********/
//Documented
    "action [[[0-4]]] node discovery {{{on|off}}}",
    "status",
    "startterm",
    "stopterm",
    "reset",
    "settime [[[0-1000000000]]] [[[0-60]]]",
    "gettime",
    "get_modules [[[0-4]]]",
    "raw_data_start [[[0-4]]] [[[0-255]]] [[[1-1024]]] 2",
    "raw_data_start_received [[[0-4]]] [[[0-255]]]",
    "raw_data_chunk [[[0-4]]] [[[0-255]]] [[[0-255]]] AA:BB:CC:DD:EE:FF",
    "raw_data_report [[[0-4]]] [[[0-255]]] [[[0-255]]]",
    "raw_data_error [[[0-4]]] [[[0-128]]] 2 [[[1-3]]]",
//Undocumented
    "action [[[0-4]]] node reset [[[0-10]]]",
    //"rawsend AA:BB:CC:DD:EE:FF", //Hard to test with a monkey because the sender must be correct.
    "data {{{sink|hop|other}}}",
    "bufferstat",
    "datal {{{u|r}}}",
    "stop",
    "start",
    "disconnect [[[0-65535]]]",
    "gap_disconnect [[[0-255]]]",
    "update_iv [[[0-4]]] [[[0-65535]]]",
    "get_plugged_in",
    "sep",

/*****************/
/* STATUS MODULE */
/*****************/
//Documented
    "action [[[0-4]]] status get_device_info",
    "action [[[0-4]]] status get_status",
    "action [[[0-4]]] status get_connections",
    "action [[[0-4]]] status get_nearby",
    "action [[[0-4]]] status livereports [[[1-100]]]",
//Undocumented
    "action [[[0-4]]] status set_init",
    "action [[[0-4]]] status keep_alive",
    "action [[[0-4]]] status get_errors",
    "action [[[0-4]]] status get_rebootreason",

/******************/
/* TESTING MODULE */
/******************/
//Documented
    //"action [[[0-4]]] testing starttest 00:00:00:{{{01|02|03|04}}}:00:00" //Wrong documentation and implementation!
//Undocumented
};


const std::string& getRandomEntry(MersenneTwister& rand, const std::vector<std::string> &vec, i32 indexOverride) {
    if (indexOverride >= 0) return vec[indexOverride];
    return vec[rand.NextU32(0, vec.size() - 1)];
}

std::string fillStringPlaceholder(MersenneTwister& rand, const std::string& command, const std::string& placeholder, const std::vector<std::string>& possibleReplacements) {
    size_t index = command.find(placeholder);
    if (index != std::string::npos) {
        std::string retVal = command;
        std::string replacement = getRandomEntry(rand, possibleReplacements, -1);
        retVal.replace(index, placeholder.size(), replacement);

        //Recursively call this function in case the placeholder is inside the string more than once.
        return fillStringPlaceholder(rand, retVal, placeholder, possibleReplacements);
    }
    return command;
}

std::string fillNumberPlaceholder(MersenneTwister& rand, const std::string& command) {
    size_t startIndex = command.find("[[[");
    if (startIndex != std::string::npos) {
        size_t endIndex = command.find("]]]");
        if (endIndex == std::string::npos) {
            std::string error = "Missing ]]] for starting [[[ in command \"";
            error += command;
            error += "\"";
            throw std::runtime_error(error.c_str());
        }
        size_t dashIndex = command.substr(startIndex + 3, endIndex - startIndex - 3).find("-") + startIndex + 3;
        if (dashIndex == std::string::npos) {
            std::string error = "Missing dash \"-\" in command \"";
            error += command;
            error += "\"";
            throw std::runtime_error(error.c_str());
        }
        std::string sStart = command.substr(startIndex + 3, dashIndex - startIndex - 3);
        std::string sEnd = command.substr(dashIndex + 1, endIndex - dashIndex - 1);

        int iStart = std::stoi(sStart);
        int iEnd = std::stoi(sEnd);

        std::string replacement = "";
        replacement += std::to_string(rand.NextU32(iStart, iEnd));

        std::string retVal = command;
        retVal.replace(startIndex, endIndex - startIndex + 3, replacement);

        //Recursively call this function in case the placeholder is inside the string more than once.
        return fillNumberPlaceholder(rand, retVal);
    }
    return command;
}

std::string fillArbitraryPlaceholder(MersenneTwister& rand, std::string& command) {
    size_t startIndex = command.find("{{{");
    if (startIndex != std::string::npos) {
        size_t endIndex = command.find("}}}");
        if (endIndex == std::string::npos) {
            std::string error = "Missing }}} for starting {{{ in command \"";
            error += command;
            error += "\"";
            throw std::runtime_error(error.c_str());
        }
        std::vector<int> separatorInices;

        while (true) {
            size_t currSep = command.substr(startIndex + 3, endIndex - startIndex - 3).find("|");
            if (currSep == std::string::npos) {
                break;
            }
            currSep += startIndex + 3;
            separatorInices.push_back(currSep);
            command[currSep] = 'R';
        }

        if (separatorInices.size() < 1) {
            std::string error = "Missing | in command \"";
            error += command;
            error += "\"";
            throw std::runtime_error(error.c_str());
        }
        separatorInices.push_back(endIndex);

        std::vector<std::string> possibleReplacements;

        int prevSep = startIndex + 3;
        for (size_t i = 0; i < separatorInices.size(); i++) {
            possibleReplacements.push_back(command.substr(prevSep, separatorInices[i] - prevSep));
            prevSep = separatorInices[i] + 1;
        }

        std::string replacement = possibleReplacements[rand.NextU32(0, possibleReplacements.size() - 1)];

        std::string retVal = command;
        retVal.replace(startIndex, endIndex - startIndex + 3, replacement);

        //Recursively call this function in case the placeholder is inside the string more than once.
        return fillArbitraryPlaceholder(rand, retVal);
    }
    return command;
}

std::string getRandomCommand(MersenneTwister& rand, const std::vector<std::string> &templates, const std::vector<std::string> &possibleSerialNumber, i32 indexOverride) {
    std::string command = getRandomEntry(rand, templates, indexOverride);
    command = fillArbitraryPlaceholder(rand, command);
    command = fillNumberPlaceholder(rand, command);
    command = fillStringPlaceholder(rand, command, "%%%SERIAL%%%", possibleSerialNumber);
    return command;
}

std::vector<std::string> tokenizeString(const std::string &command) {
    std::vector<std::string> retVal;

    size_t spacePos = command.find(" ");
    if (spacePos == std::string::npos) {
        retVal.push_back(command);
    }
    else {
        size_t previousSpacePos = -1;
        while (spacePos != std::string::npos) {
            retVal.push_back(command.substr(previousSpacePos + 1, spacePos - previousSpacePos - 1));
            previousSpacePos = spacePos;
            spacePos = command.find(" ", spacePos + 1);
        }
        retVal.push_back(command.substr(previousSpacePos + 1, command.size() - previousSpacePos));
    }

    return retVal;
}

void corruptTokenizedVectorInPlace(MersenneTwister& rand, std::vector<std::string> &tokens) {
    if (rand.NextPsrng(UINT32_MAX / 2) && tokens.size() > 1) {
        //Remove a token!
        int index = rand.NextU32(1, tokens.size() - 1);
        auto iter = tokens.begin() + index;
        tokens.erase(iter);
    }
    else {
        //Add a token!    
        int index = rand.NextU32(1, tokens.size());
        auto iter = tokens.begin() + index;
        std::string token = std::to_string(rand.NextU32(0, 10));
        tokens.insert(iter, token);
    }

    if (rand.NextPsrng(UINT32_MAX / 2)) {
        corruptTokenizedVectorInPlace(rand, tokens);
    }
}

std::string corruptCommand(MersenneTwister& rand, const std::string &command) {
    auto tokens = tokenizeString(command);
    corruptTokenizedVectorInPlace(rand, tokens);

    std::string retVal = "";
    for (size_t i = 0; i < tokens.size(); i++) {
        retVal += tokens[i];
        if (i != tokens.size() - 1) {
            retVal += " ";
        }
    }

    return retVal;
}

struct CommandWithTarget
{
    std::string command;
    int target;
    int sleepSteps;
};

std::vector<CommandWithTarget> CreateCommands(int amount, u32 seed, u32 amountOfNodes, bool onlyValidCommands, bool allCommandsInOrder)
{
    MersenneTwister monkeyRand(seed);
    std::vector<CommandWithTarget> retVal;
    for (int commandNum = 0; commandNum < amount; commandNum++)
    {
        std::string command = getRandomCommand(monkeyRand, templates, { "BBBBB", "BBBBC", "BBBBD" }, (allCommandsInOrder ? commandNum : -1));
        if (!onlyValidCommands) {
            command = corruptCommand(monkeyRand, command);
        }
        int target = monkeyRand.NextU32(0, amountOfNodes);
        retVal.push_back({command, target, 1});
    }
    return retVal;
}

void ExecuteCommands(std::vector<CommandWithTarget> &commands, u32 seed, u32 amountOfNodes)
{
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.verboseCommands = false;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", amountOfNodes - 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    if (seed % 2 == 0) {
        tester.SimulateUntilClusteringDone(100 * 1000);
    }

    for (u32 i = 0; i < commands.size(); i++)
    {
        tester.SendTerminalCommand(commands[i].target, commands[i].command.c_str());
        tester.SimulateGivenNumberOfSteps(commands[i].sleepSteps);
    }

    tester.SimulateForGivenTime(50 * 1000); //Simulate a little longer to make sure that the simulator is able to recover.
}

void ReduceCommandVector(std::vector<CommandWithTarget> &commands, u32 seed, u32 amountOfNodes)
{
    u32 amountOfNoops = 0;
    for (u32 i = 0; i < commands.size(); i++)
    {
        //Remove a command and see if the exception still occures...
        auto smallerCopy = commands;
        smallerCopy.erase(smallerCopy.begin() + i);

        bool exceptionStillOccured = false;
        try
        {
            ExecuteCommands(smallerCopy, seed, amountOfNodes);
        }
        catch (...)
        {
            exceptionStillOccured = true;
        }

        //If it didn't happen again, it does not mean that the command is evil yet! Maybe it was just the additional tick that
        //automatically happens with each command. In other words: we try if the exception still happens if we replace the
        //command with a sleep.
        if(!exceptionStillOccured && i > 0)
        {
            smallerCopy[i - 1].sleepSteps++;
            try
            {
                ExecuteCommands(smallerCopy, seed, amountOfNodes);
            }
            catch (...)
            {
                exceptionStillOccured = true;
                amountOfNoops++;
            }
        }

        if (exceptionStillOccured)
        {
            commands = smallerCopy;
            i--;
        }
        std::cout << i << "/" << commands.size() << "/" << amountOfNoops << std::endl;
    }

    for (u32 i = 0; i < commands.size(); i++)
    {
        std::cout << "tester.SendTerminalCommand(" << commands[i].target << ", \"" << commands[i].command << "\");" << std::endl;
        std::cout << "tester.SimulateGivenNumberOfSteps(" << commands[i].sleepSteps << ");" << std::endl;
    }
}

void StartTestMonkey(bool onlyValidCommands, bool allCommandsInOrder, bool reduceCommandsOnFail) 
{
    //We do not care for this exception as it might happen (and that's ok), if the user logs an error that is unknown
    Exceptions::ExceptionDisabler<ErrorCodeUnknownException> errorCodeUnknownException;
    //Can happen if a enroll command is followed by a saverec command.
    Exceptions::ExceptionDisabler<RecordStorageIsLockedDownException> recordStorageIsLockedDownException;
    Exceptions::ExceptionDisabler<WrongCommandParameterException> wrongCommandParameterException;
    Exceptions::ExceptionDisabler<InternalTerminalCommandErrorException> internalTerminalCommandErrorException;
    // Added this as sendMeshMessage may return error which in turn will cause log error exception
    Exceptions::ExceptionDisabler<ErrorLoggedException> ErrorLoggedException;
    Exceptions::DisableDebugBreakOnException antiDebugBreak;
    constexpr u32 amountOfNodes = 3;

    const u32 t = (u32)time(nullptr);
    // Tuned to take roughly 7.5 minutes on CI.
    for (int repeats = 0; repeats < (allCommandsInOrder ? 1 : 225); repeats++) {
        u32 seed = t + repeats;
        if (allCommandsInOrder)
        {
            seed = 1337; // We don't want any randomness if we run in a non scheduled pipeline.
        }
        printf("Seed: %u" EOL, seed);
        
        std::vector<CommandWithTarget> commands = CreateCommands((allCommandsInOrder ? templates.size() : 1024), seed, amountOfNodes, onlyValidCommands, allCommandsInOrder);
        try 
        {
            ExecuteCommands(commands, seed, amountOfNodes);
        }
        catch (...)
        {
            if (reduceCommandsOnFail)
            {
                std::cout << "An exception occured. Trying to reduce commands for easier reproducibility..." << std::endl;
                ReduceCommandVector(commands, seed, amountOfNodes);
            }
            throw;
        }
    }
    const u32 t2 = (u32)time(nullptr);
    const u32 diff = t2 - t;
    printf("It took %u seconds!\n", diff);
}

TEST(TestMonkey, TestMonkeyValid_scheduled) {
    StartTestMonkey(true, false, true);
}

TEST(TestMonkey, TestMonkeyValidAllInOrder) {
    StartTestMonkey(true, true, false);
}

/*TEST(TestMonkey, ReproductionTest) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.numNodes = 3;
    simConfig.terminalId = 0;
    //simConfig.verboseCommands = false;
    //testerConfig.verbose = true;

    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    // Add your Monkey Test code here!
}*/

/*TEST(TestMonkey, TestMonkeyInvalid_scheduled) {
    Exceptions::ExceptionDisabler<IllegalAdvertismentStateException> iasDisabler;
    Exceptions::ExceptionDisabler<CommandNotFoundException> cnfDisabler;
    Exceptions::ExceptionDisabler<TooManyArgumentsException> tmaDisabler;
    Exceptions::ExceptionDisabler<IllegalArgumentException> iaDisabler;
    Exceptions::ExceptionDisabler<CommandTooLongException> ctlDisabler;
    StartTestMonkey(false, false);
}*/

