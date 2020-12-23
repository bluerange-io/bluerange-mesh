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

#include <FmTypes.h>
#include <Config.h>
#include <FruityHal.h>
#include <array>

enum class AdvJobTypes : u8{
    INVALID,
    SCHEDULED, //Automatically scheduled with other jobs
    IMMEDIATE  //Will be executed immediately until done

};

struct AdvJob {
    AdvJobTypes type;
    //For Scheduler
    u8 slots; //Number of slots this advertising message will get (1-10), 0 = Invalid
    u8 delay; //Number of slots that this message will be delayed
    u16 advertisingInterval; //In units of 0.625ms
    u8 advertisingChannelMask;

    //Internal Scheduling
    u8 currentSlots;
    u8 currentDelay;

    //Advertising Data
    FruityHal::BleGapAdvType advertisingType; //BLE_GAP_ADV_TYPES
    u8 advData[31];
    u8 advDataLength;
    u8 scanData[31];
    u8 scanDataLength;
    
};

struct AdvData {
    bool inUse;
    u8 advData[31];
    u8 advDataLength;
    u8 scanData[31];
    u8 scanDataLength;
};

/*
 * The Advertising Controller is responsible for wrapping all advertising
 * functionality and the necessary softdevice calls in one class.
 * It provides a scheduler that can be used to schedule a number of messages.
 * The current message broadcast is then automatically switched between all
 * croadcasted messages.
 */
class AdvertisingController
{
private:
    u32 sumSlots = 0;
    u16 currentAdvertisingInterval = UINT16_MAX;
    u8 handle = 0xFF; //BLE_GAP_ADV_SET_HANDLE_NOT_SET

    //The address that should be used for advertising, the Least Significant Byte
    //May be changed by the advertiser to account for different advertising services
    FruityHal::BleGapAddr baseGapAddress;

    bool isActive = true;

public:
    AdvertisingController();

    std::array<AdvJob, ADVERTISING_CONTROLLER_MAX_NUM_JOBS> jobs{};
    std::array<AdvData, 2> advData{};
    u8 currentSlotUsed = 0;

    enum class AdvertisingState : u8{
        DISABLED,
        ENABLED
    };

    enum class AdvertisingStateAction : u8{
        OK,
        DISABLE,
        RESTART,
    };

    AdvertisingState advertisingState = AdvertisingState::DISABLED;
    AdvertisingStateAction advertisingStateAction = AdvertisingStateAction::OK;

    FruityHal::BleGapAdvParams currentAdvertisingParams;
    u8 currentNumJobs = 0;


    AdvJob* currentActiveJob = nullptr;
    AdvJob* jobToSet = nullptr;

    static AdvertisingController& GetInstance();


    void Initialize();

    //Job Scheduling
    void InitJobScheduling();
    AdvJob* AddJob(const AdvJob& job);
    void RefreshJob(const AdvJob* jobHandle);
    void RemoveJob(AdvJob* jobHandle);
    AdvJob* DetermineCurrentAdvertisingJob();
    void DetermineAndSetAdvertisingJob();

    //Change Advertising with Softdevice
    void SetAdvertisingData(AdvJob* job);
    void SetAdvertisingState(AdvJob* job);

    u16 GetLowestAdvertisingInterval();
    void RestartAdvertising();

    void Deactivate();


    void TimerEventHandler(u16 passedTimeDs);

    void GapConnectedEventHandler(const FruityHal::GapConnectedEvent& connectedEvent);
    void GapDisconnectedEventHandler(const FruityHal::GapDisconnectedEvent& disconnectedEvent);


};

