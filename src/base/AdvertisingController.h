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
 * broadcasted messages.
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
