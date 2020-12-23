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
#pragma once

#include <CherrySim.h>
#include <thread>
#include <string>
#include <vector>

struct CherrySimRunnerConfig
{
    bool enableClusteringTest;
    bool verbose;
};

class CherrySimRunner : public TerminalPrintListener, public CherrySimEventListener
{
private:
    volatile bool terminalReaderLaunched = false;
    std::thread terminalReader;
public:
    CherrySim* sim;
    static CherrySimRunnerConfig CreateDefaultTesterConfiguration();

    bool meshGwCommunication;

    volatile bool running = true;

    static constexpr int32_t MESH_GW_NODE = 1;

    explicit CherrySimRunner(const CherrySimRunnerConfig &runnerConfig, const SimConfiguration &simConfig, bool meshGwCommunication);

    void Start();
    bool Simulate();
    ~CherrySimRunner() {};

    //### Callbacks
    //Inherited via TerminalPrintListener
    void TerminalPrintHandler(NodeEntry* currentNode, const char* message) override;
    //Inherited via CherrySimEventListener
    void CherrySimEventHandler(const char* eventType) override;
    void CherrySimBleEventHandler(NodeEntry* currentNode, simBleEvent* simBleEvent, u16 eventSize) override;

    void TerminalReaderMain();

    static SimConfiguration CreateDefaultRunConfiguration();

private:
    bool shouldRestartSim;
    CherrySimRunnerConfig runnerConfig;
    SimConfiguration simConfig;

};

