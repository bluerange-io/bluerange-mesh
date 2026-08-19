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

#include <Module.h>

//This should be set to the correct vendor and subId
constexpr VendorModuleId VENDOR_TEMPLATE_MODULE_ID = GET_VENDOR_MODULE_ID(0xABCD, 1);

#if IS_ACTIVE(VENDOR_TEMPLATE_MODULE)

/*
 * This is a template for a FruityMesh module.
 * A comment should be here to provide a least a short description of its purpose.
 */

constexpr u8 VENDOR_TEMPLATE_MODULE_CONFIG_VERSION = 1;

#pragma pack(push)
#pragma pack(1)
//Module configuration that is saved persistently (size must be multiple of 4)
struct VendorTemplateModuleConfiguration : VendorModuleConfiguration {
    //Insert more persistent config values here
    u8 exampleValue;
};
#pragma pack(pop)

class VendorTemplateModule : public Module
{
public:

    enum VendorTemplateModuleTriggerActionMessages {
        COMMAND_ONE_MESSAGE = 0,
        COMMAND_TWO_MESSAGE = 1,
    };

    enum VendorTemplateModuleActionResponseMessages {
        COMMAND_ONE_MESSAGE_RESPONSE = 0,
        COMMAND_TWO_MESSAGE_RESPONSE = 1,
    };

    //####### Module messages (these need to be packed)
#pragma pack(push)
#pragma pack(1)

    static constexpr int SIZEOF_VENDOR_TEMPLATE_MODULE_COMMAND_ONE_MESSAGE = 1;
    typedef struct
    {
        //Insert values here
        u8 exampleValue;

    } VendorTemplateModuleCommandOneMessage;
    STATIC_ASSERT_SIZE(VendorTemplateModuleCommandOneMessage, SIZEOF_VENDOR_TEMPLATE_MODULE_COMMAND_ONE_MESSAGE);

#pragma pack(pop)
    //####### Module messages end

    //Declare the configuration used for this module
    DECLARE_CONFIG_AND_PACKED_STRUCT(VendorTemplateModuleConfiguration);

    VendorTemplateModule();

    void ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength) override;

    void ResetToDefaultConfiguration() override;

    void TimerEventHandler(u16 passedTimeDs) override;

    void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader) override;

    #ifdef TERMINAL_ENABLED
    TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override;
    #endif

    CapabilityEntry GetCapability(u32 index, bool firstCall) override;

#if IS_ACTIVE(REGISTER_HANDLER)
public:
    //####### Register Handler
    constexpr static u32 DEMO_REGISTER_WRITABLE           = 20000; //Size 1
    constexpr static u32 DEMO_REGISTER_SOME_STRING_BASE   = 20100; //Size 16
    constexpr static u32 DEMO_REGISTER_CLAMPED_VALUE      = 20200; //Size 4

    constexpr static u32 DEMO_REGISTER_READ_ONLY          = 30000; //Size 2

    //Some variables to implement the demo functionality
    u8 demoVarWritable = 0;
    u16 demoVarReadOnly = 123;
    REGISTER_STRING(demoVarSomeString, 16);
    u32 demoVarClampedValue = 1000;

protected:
    virtual RegisterGeneralChecks GetGeneralChecks(u16 component, u16 reg, u16 length) const override final;
    virtual RegisterHandlerCode CheckValues(u16 component, u16 reg, const u8* values, u16 length) const override final;
    virtual void MapRegister(u16 component, u16 reg, SupervisedValue& out, u32& persistedId) override final;
    virtual void ChangeValue(u16 component, u16 reg, u8* values, u16 length) override final;
    virtual void OnRegisterRead(u16 component, u16 reg) override final;
#endif //IS_ACTIVE(REGISTER_HANDLER)
};

#endif //IS_ACTIVE(VENDOR_TEMPLATE_MODULE)
