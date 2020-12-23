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
#include "Utility.h"
#include "CherrySimTester.h"
#include "CherrySimUtils.h"
#ifndef GITHUB_RELEASE
#include "AssetModule.h"
#endif //GITHUB_RELEASE
#include "ScanningModule.h"

TEST(TestScanningModule, TestCommands) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();



    tester.SimulateUntilClusteringDone(100 * 1000);

    //Creates a asset ble event and later checks if this asset is now tracked by the scanning module.
    alignas(ble_evt_t) u8 buffer[1024];
    CheckedMemset(buffer, 0, sizeof(buffer));
    ble_evt_t& evt = *(ble_evt_t*)buffer;
    AdvPacketServiceAndDataHeader* packet = (AdvPacketServiceAndDataHeader*)evt.evt.gap_evt.params.adv_report.data;
    AdvPacketLegacyAssetServiceData* assetPacket = (AdvPacketLegacyAssetServiceData*)&packet->data;
    evt.header.evt_id = BLE_GAP_EVT_ADV_REPORT;
    evt.evt.gap_evt.params.adv_report.dlen = SIZEOF_ADV_STRUCTURE_LEGACY_ASSET_SERVICE_DATA;
    evt.evt.gap_evt.params.adv_report.rssi = -45;
    packet->flags.len = SIZEOF_ADV_STRUCTURE_FLAGS - 1;
    packet->uuid.len = SIZEOF_ADV_STRUCTURE_UUID16 - 1;
    packet->data.uuid.type = (u8)BleGapAdType::TYPE_SERVICE_DATA;
    packet->data.uuid.uuid = MESH_SERVICE_DATA_SERVICE_UUID16;
    packet->data.messageType = ServiceDataMessageType::LEGACY_ASSET;
    assetPacket->serialNumberIndex = 10;
    assetPacket->nodeId = 1337;

    NodeIndexSetter setter(0);
    FruityHal::DispatchBleEvents(&evt);
    tester.SimulateGivenNumberOfSteps(1);

    //jstodo This test currently doesn't do much. Investigate if it is still needed.
}