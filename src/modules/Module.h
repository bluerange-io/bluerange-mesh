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

constexpr int INVALID_U8_CONFIG  = 0xFF;
constexpr int INVALID_U16_CONFIG = 0xFFFF;
constexpr int INVALID_U32_CONFIG = 0xFFFFFFFF;

#include <Config.h>
#include <Boardconfig.h>
#include <Logger.h>
#include <Terminal.h>
#include <RecordStorage.h>
#include <MeshConnection.h>
#include <BaseConnection.h>
#include <RegisterHandler.h>

#if IS_ACTIVE(SIG_MESH)
#include <SigElement.h>
#include <SigModel.h>
#include <SigState.h>
#endif

#ifdef FM_NATIVE_RENDERER_ENABLED
namespace bbe
{
    class PrimitiveBrush2D;
}
#endif

//This struct is used to report a list of modules and their state for a node
#pragma pack(push, 1)
    struct ModuleInformation {
        ModuleIdWrapper moduleId;
        u8 moduleVersion;
        u8 moduleActive : 1;
        u8 reserved : 7;
    };
    STATIC_ASSERT_SIZE(ModuleInformation, 6);
#pragma pack(pop)

enum class SetConfigResultCodes : u8
{
    SUCCESS              = 0, //Something that actually went right

    // => This space was left empty to leave space to use all RecordStorageResultCodes

    NO_CONFIGURATION     = 51, //Module does not have a configuration
    WRONG_CONFIGURATION  = 52, //Configuration was invalid
    OTHER_ERROR          = 53, //Other error types are mapped to this generic error
};

static_assert((u8)RecordStorageResultCode::LAST_ENTRY < 50, "RecordStorageResultCodes too big");

class Node;

#define REGISTER_STRING(name, size) alignas(u8) u8 name[size] = {};
#define GET_REGISTER_STRING_ADDR(regIn, buffer) (reg >= (regIn) && register_ < (regIn) + sizeof(buffer)) addr.Set(((u8*)(buffer)) + reg - (regIn))
#define GENERAL_CHECK_STRING(regIn, buffer)     if (    reg                >= (regIn) &&  reg                < (regIn) + sizeof(buffer) \
                                                    && (reg + length - 1u) >= (regIn) && (reg + length - 1u) < (regIn) + sizeof(buffer)) return RGC_STRING

/**
 * @brief Declares a register with a default value.
 *
 * Defines a constant REGISTER_##name for the given address and a variable with the given name and default value.
 *
 * @param[in] address      Register address.
 * @param[in] type         Data type of the register value.
 * @param[in] name         Register name (used as identifier).
 * @param[in] defaultValue Default value of the register.
 */
#define DECLARE_REGISTER(address, type, name, defaultValue)                                                            \
    constexpr static u32 REGISTER_##name = address;                                                                    \
    type name = defaultValue;

/**
 * @brief Declares a register with clamping bounds.
 *
 * Extends DECLARE_REGISTER by additionally defining the constants
 * REGISTER_MIN_##name and REGISTER_MAX_##name for the given bounds.
 *
 * @param[in] address      Register address.
 * @param[in] type         Data type of the register value.
 * @param[in] name         Register name (used as identifier).
 * @param[in] defaultValue Default value of the register.
 * @param[in] minVal       Lower clamping bound.
 * @param[in] maxVal       Upper clamping bound.
 *
 * @warning This macro only declares the bounds. The actual clamping must be
 *          performed separately via CLAMP_REGISTER_U8 / _U16 / _U32.
 */
#define DECLARE_REGISTER_CLAMPED(address, type, name, defaultValue, minVal, maxVal)                                    \
    DECLARE_REGISTER(address, type, name, defaultValue)                                                                \
    constexpr static type REGISTER_MIN_##name = minVal;                                                                \
    constexpr static type REGISTER_MAX_##name = maxVal;

/**
 * @brief Declares a register with validation bounds and a default fallback.
 *
 * Extends DECLARE_REGISTER by additionally defining REGISTER_MIN_##name,
 * REGISTER_MAX_##name, and REGISTER_DEFAULT_##name. The default is used for
 * register initialization and as a fallback if validation fails.
 *
 * @param[in] address      Register address.
 * @param[in] type         Data type of the register value.
 * @param[in] name         Register name (used as identifier).
 * @param[in] defaultValue Default value applied when validation fails.
 * @param[in] minVal       Lower validation bound.
 * @param[in] maxVal       Upper validation bound.
 *
 * @warning This macro only declares the bounds and default. The actual range
 *          check must be performed separately via CHECK_REGISTER_U8 or an
 *          equivalent macro.
 */
#define DECLARE_REGISTER_CHECKED(address, type, name, defaultValue, minVal, maxVal)                                    \
    DECLARE_REGISTER(address, type, name, defaultValue)                                                                \
    constexpr static type REGISTER_MIN_##name = minVal;                                                                \
    constexpr static type REGISTER_MAX_##name = maxVal;                                                                \
    constexpr static type REGISTER_DEFAULT_##name = defaultValue;

/**
 * @brief Clamps a u32 register value to its declared bounds.
 *
 * Reads the u32 at @p reg from @p values, clamps it to
 * [REGISTER_MIN_##name, REGISTER_MAX_##name], and writes it back.
 * Requires the register to have been declared with DECLARE_REGISTER_CLAMPED.
 *
 * @param[in] name Register name whose REGISTER_MIN/MAX constants are used.
 */
#define CLAMP_REGISTER_U32(name)                                                                                       \
    if (reg == REGISTER_##name) {                                                                                      \
        u32 _v = Utility::ToAlignedU32(values);                                                                        \
        _v = Utility::Clamp<u32>(_v, REGISTER_MIN_##name, REGISTER_MAX_##name);                                        \
        CheckedMemcpy(values, &_v, sizeof(_v));                                                                        \
    }

/**
 * @brief Clamps a u16 register value to its declared bounds.
 *
 * Reads the u16 at @p reg from @p values, clamps it to
 * [REGISTER_MIN_##name, REGISTER_MAX_##name], and writes it back.
 * Requires the register to have been declared with DECLARE_REGISTER_CLAMPED.
 *
 * @param[in] name Register name whose REGISTER_MIN/MAX constants are used.
 */
#define CLAMP_REGISTER_U16(name)                                                                                       \
    if (reg == REGISTER_##name) {                                                                                      \
        u16 _v = Utility::ToAlignedU16(values);                                                                        \
        _v = Utility::Clamp<u16>(_v, REGISTER_MIN_##name, REGISTER_MAX_##name);                                        \
        CheckedMemcpy(values, &_v, sizeof(_v));                                                                        \
    }

/**
 * @brief Clamps a u8 register value to its declared bounds.
 *
 * Reads the u8 at @p reg from @p values[0], clamps it to
 * [REGISTER_MIN_##name, REGISTER_MAX_##name], and writes it back.
 * Requires the register to have been declared with DECLARE_REGISTER_CLAMPED.
 *
 * @param[in] name Register name whose REGISTER_MIN/MAX constants are used.
 */
#define CLAMP_REGISTER_U8(name)                                                                                        \
    if (reg == REGISTER_##name) {                                                                                      \
        values[0] = Utility::Clamp<u8>(values[0], REGISTER_MIN_##name, REGISTER_MAX_##name);                           \
    }

/**
 * @brief Validates a u8 register value and resets it to the default on failure.
 *
 * If @p values[0] exceeds REGISTER_MAX_##name, it is replaced with
 * REGISTER_DEFAULT_##name. Requires the register to have been declared
 * with DECLARE_REGISTER_CHECKED.
 *
 * @param[in] name Register name whose REGISTER_MAX and REGISTER_DEFAULT constants
 *                 are used.
 */
#define CHECK_REGISTER_U8(name)                                                                                        \
    if (reg == REGISTER_##name) {                                                                                      \
        if (values[0] > REGISTER_MAX_##name) {                                                                         \
            values[0] = REGISTER_DEFAULT_##name;                                                                       \
        }                                                                                                              \
    }

/**
 * @brief Maps a register to a persistent configuration field.
 *
 * If @p reg matches REGISTER_##name, exposes the field as writable via
 * @p out and marks it as a persisted configuration entry (persistedId = 1).
 *
 * @param[in] name Register name to match against @p reg.
 */
#define MAP_CONFIGURATION_REGISTER(name)                                                                               \
    if (reg == REGISTER_##name) {                                                                                      \
        out.SetWritable(&name);                                                                                        \
        persistedId = 1;                                                                                               \
}

/**
 * @brief Maps a register to a writable control field.
 *
 * If @p reg matches REGISTER_##name, exposes the field as writable via
 * @p out. Unlike MAP_CONFIGURATION_REGISTER, no persistent ID is set.
 *
 * @param[in] name Register name to match against @p reg.
 */
#define MAP_CONTROL_REGISTER(name)                                                                                     \
    if (reg == REGISTER_##name) {                                                                                      \
        out.SetWritable(&name);                                                                                        \
    }

/**
 * @brief Maps a register to a readable field.
 *
 * If @p reg matches REGISTER_##name, exposes the field as readable via
 * @p out. The field is not writable through this mapping.
 *
 * @param[in] name Register name to match against @p reg.
 */
#define MAP_READABLE_REGISTER(name)                                                                                    \
    if (reg == REGISTER_##name) {                                                                                      \
        out.SetReadable(name);                                                                                         \
    }

constexpr u32 REGISTER_RECORDS_PER_MODULE = 4;

class RegisterHandlerEventListener
{
public:
    RegisterHandlerEventListener() {};

    virtual ~RegisterHandlerEventListener() {};

    //Struct is passed by value so that it can be dequeued before calling this handler
    //If we passed a reference, this handler would have to clear the item from the TaskQueue
    virtual void RegisterHandlerEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength, bool dataChanged) = 0;

};

/*
 * The Module class can be subclassed for a number of purposes:
 * - Implement a driver for a sensor or an actuator
 * - Add functionality like parsing advertising data, etc...
 *
 * It provides a basic set of handlers that are called from the main event handling
 * routines and the received events can be used and acted upon.
 *
 *
 * Module ids start with 1, this id is also used for saving persistent
 * module configurations with the RecordStorage class
 * Module ids must persist between updates to guarantee that the
 * same module receives the same storage slot.
 *
 * ModuleIds must also be the same within a mesh network to guarantee the correct
 * delivery of actions and responses.
 */
class Module
#if IS_ACTIVE(REGISTER_HANDLER)
    : public RegisterHandlerEventListener
#endif //IS_ACTIVE(REGISTER_HANDLER)
{
    friend class FruityMesh;

protected:
    //This must be called in the constructor to reset all values to default
    virtual void ResetToDefaultConfiguration() = 0;

public:
    //This funny little construct allows us to represent both a ModuleId and a VendorModuleId
    //This is the same struct as the ModuleIdWrapper but uses anonymous parts for easier access
    const union {
        const struct {
            const ModuleId moduleId;
            const u8 subId;
            const u16 vendorId;
        };
        const VendorModuleId vendorModuleId;
    };

    const char* moduleName;

    //The constructor is used to initialize all members and must call ResetToDefaultConfiguration in all subclasses
    Module(ModuleId moduleId, const char* name);

    //For vendor modules, this constructor must be used instead of the one above
    Module(VendorModuleId moduleId, const char* name);

    virtual ~Module();

    //These two variables must be set by the submodule in the constructor before loading the configuration
    union {
        ModuleConfiguration* configurationPointer;
        VendorModuleConfiguration* vendorConfigurationPointer;
    };
    u16 configurationLength;

    //This is automatically set to the moduleId for core modules, vendor modules must set this to a defined record storage id
    u16 recordStorageId = RECORD_STORAGE_RECORD_ID_INVALID;

    //Can be checked to make sure that certain module settings are not modified during runtime
    //or that tasks are only performed once when starting the module for the first time
    bool moduleStarted = false;

    enum class ModuleConfigMessages : u8
    {
        SET_CONFIG = 0, 
        SET_CONFIG_RESULT = 1,
        SET_ACTIVE = 2, 
        SET_ACTIVE_RESULT = 3,
        GET_CONFIG = 4, 
        CONFIG = 5,
        GET_MODULE_LIST = 6, 
        MODULE_LIST = 7,
        MODULE_LIST_V2 = 8,
        GET_CONFIG_ERROR = 9,
    };

    enum class ModuleSaveAction : u8{
        SAVE_MODULE_CONFIG_ACTION,
        SET_ACTIVE_CONFIG_ACTION
    };

    struct SaveModuleConfigAction {
        NodeId sender;
        ModuleIdWrapper moduleId;
        u8 requestHandle;
    };

    //This function is called on the module to load its saved configuration from flash and start
    void LoadModuleConfigurationAndStart();

    //Constructs a simple TriggerAction message and sends it
    ErrorTypeUnchecked SendModuleActionMessage(MessageType messageType, NodeId toNode, u8 actionType, u8 requestHandle, const u8* additionalData, u16 additionalDataSize, bool reliable, bool loopback) const;
    ErrorTypeUnchecked SendModuleActionMessage(MessageType messageType, NodeId toNode, u8 actionType, u8 requestHandle, const u8* additionalData, u16 additionalDataSize, bool reliable) const;


    //##### Handlers that can be implemented by any module, but are implemented empty here

    /**
     * This function is called as soon as the module settings have been loaded or updated.
     *
     * If the loaded configuration has a different version than the current moduleVersion it will not
     * have been copied to the module configuration and a pointer and size of the migratableConfig
     * are given to the function (otherwise null). The module can do migration itself if it desires so.
     *
     * The module must make sure to disable all its tasks once moduleActive is set to false.
     *
     * If moduleActive is set to false, this is the only call the module will get, other listeners will be disabled.
     */
    virtual void ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength){};

    //This handler receives all timer events
    virtual void TimerEventHandler(u16 passedTimeDs){};

    //This handler receives all ble events and can act on them
    virtual void GapAdvertisementReportEventHandler(const FruityHal::GapAdvertisementReportEvent& advertisementReportEvent) {};
    virtual void GapConnectedEventHandler(const FruityHal::GapConnectedEvent& connectedEvent) {};
    virtual void GapDisconnectedEventHandler(const FruityHal::GapDisconnectedEvent& disconnectedEvent) {};
    virtual void GattDataTransmittedEventHandler(const FruityHal::GattDataTransmittedEvent& gattDataTransmittedEvent) {};

    //When a mesh connection is connected with handshake and everything or if it is disconnected, the ConnectionManager will call this handler
    virtual void MeshConnectionChangedHandler(MeshConnection& connection){};

    //This can be used to get access to all routed messages and modify their content, block them or re-route them
    //A routing decision must be returned and all the routing decisions are ORed together so that a block from one module
    //will definitely block the message
    virtual RoutingDecision MessageRoutingInterceptor(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader) { return 0; };

    //Gives a message an arbitrary DeliveryPriority. Can return DeliveryPriority::INVALID in which case the priority is not changed.
    //The most important priority returned by all modules wins. If all modules return DeliveryPriority::INVALID, DeliveryPriority::MEDIUM
    //is used.
    virtual DeliveryPriority GetPriorityOfMessage(const u8* data, MessageLength size) { return DeliveryPriority::INVALID; };

    //This handler receives all connection packets addressed to this node.
    //CAREFUL: In some situations, the message may not actually have come through a connection, but
    //         from the node itself. One example is when the terminal command handler is able to
    //         dispatch the message to the node itself. In such a case, connection is nullptr.
    virtual void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const * packetHeader);

    //This handler is called before the node is enrolled, it can return PRE_ENROLLMENT_ codes
    //The enrollment packet that was received with the enrollment data is passed to the handler and can be checked
    virtual PreEnrollmentReturnCode PreEnrollmentHandler(ConnPacketModule* enrollmentPacket, MessageLength packetLength);

    //Queries a single capability of a module. If no capability with the given index is available the value INVALID must be returned.
    //After the first invalid index, no valid indices must follow. To limit the amount of virtual methods, this method is called once
    //for every capability per capability in the module, thus leading to a time complexity of O(n^2). This means that this method
    //should not do complex tasks! The firstCall parameter tells the callee if this is the firstCall to the method for the current
    //Capability retrieval run. This can be used for initialization tasks. Note that testing for "index == 0" is not sufficient as it
    //is possible that the CapabilityEntryType is NOT_READY, which then later would call the function again with index == 0 but with
    //firstCall false.
    virtual CapabilityEntry GetCapability(u32 index, bool firstCall) {
        CapabilityEntry retVal;
        retVal.type = CapabilityEntryType::INVALID;
        return retVal;
    };

#ifdef FM_NATIVE_RENDERER_ENABLED
    // Let's you render custom things in native rendering mode.
    virtual void NativeDraw(bbe::PrimitiveBrush2D& brush) {};
#endif

#if IS_ACTIVE(SIG_MESH)
    //This handler is called once a sig mesh state changes. This is called on all modules for all states so that they can also react
    //on state changes for elements or models that they have not originally created
    virtual void SigMeshStateChangedHandler(SigElement* element, SigModel* model, SigState* state) {};
#endif

    //MeshAccessConnections should only allow authorized packets to be sent into the mesh
    //This function is called once a packet was received through a meshAccessConnection to
    //query if the packet can be sent through. It can also be modified by this handler
    virtual MeshAccessAuthorization CheckMeshAccessPacketAuthorization(BaseConnectionSendData* sendData, u8 const * data, FmKeyId fmKeyId, DataDirection direction){ return MeshAccessAuthorization::UNDETERMINED; };

    //This method must be implemented by modules that support component updates
    //The module must answer weather it wants to accept the update (0) or not (negative result)
    //If the request is handled asynchronously, the module must return dfu start response QUERY_WAITING and must then manually call ContinueDfuStart
    virtual DfuStartDfuResponseCode CheckComponentUpdateRequest(ConnPacketModule const * inPacket, u32 version, ImageType imageType, u8 componentId){ return DfuStartDfuResponseCode::MODULE_NOT_UPDATABLE; };

    //This method allows a module to update its component
    //The module must ensure that subsequent calls to this method do not interfere with the update process
    virtual void StartComponentUpdate(u8 componentId, u8* imagePtr, u32 imageLength){};

    //The Terminal Command handler is called for all modules with the user input
#ifdef TERMINAL_ENABLED
    //This method can be implemented by any subclass and will be notified when
    //a command is entered.
    virtual TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) /*nonconst*/;
#endif

#if IS_ACTIVE(BUTTONS)
    virtual void ButtonHandler(u8 buttonId, u32 holdTimeDs) {};
#endif

    //This method indicates if this module is interested in a mesh access connection that
    //should be created from a different node to this node. Typically is used by assets
    //as they are not permanent members of the mesh but regularly have some new sensor values
    //to publish.
    virtual bool IsInterestedInMeshAccessConnection() { return false; }

private:
    //####### Module specific message structs (these need to be packed)
    #pragma pack(push)
    #pragma pack(1)
    //This message is used to return a result code for different actions
    typedef struct
    {
        SetConfigResultCodes result;

    } ModuleConfigResultCodeMessage;
    STATIC_ASSERT_SIZE(ModuleConfigResultCodeMessage, 1);
    #pragma pack(pop)

    void SendModuleConfigResult(NodeId senderId, ModuleIdWrapper moduleId, ModuleConfigMessages actionType, SetConfigResultCodes result, u8 requestHandle);

    // If any subclass of the Module wants to be a RecordStorageEventListener, then this Proxy implementation
    // avoids an ambiguity which RecordStorageEventHandler should be called. In addition, it avoids that clashes
    // between userTypes cause any issues.
    class RecordStorageEventListenerProxy : public RecordStorageEventListener
    {
    private:
        Module& mod;
    public:
        explicit  RecordStorageEventListenerProxy(Module& mod);

        virtual void RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength) override;
    };
    RecordStorageEventListenerProxy proxy;
    void ProxyRecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength);


#if IS_ACTIVE(REGISTER_HANDLER)

private:
    // To make it easy for Modules to simply inherit from RecordStorageEventListener
    // even if they inherit from RegisterHandler we proxy the callback here.
    class RecordStorageEventListenerRegisterProxy : public RecordStorageEventListener
    {
        Module& mod;
    public:
        explicit RecordStorageEventListenerRegisterProxy(Module& handler);
        virtual void RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength) override;
    };
    RecordStorageEventListenerRegisterProxy registerProxy;

public:
    void LoadRegisterHandlerDataFromFlash();

    struct RegisterHandlerRecordStorageUserData
    {
        u16 component;
        u16 reg;
        u16 length;
        RegisterHandlerSetSource source;
        // Instead of giving the callback from the user, we give our own callback
        // which is calling commit and then calls the callback from the user, which
        // is this member.
        RegisterHandlerEventListener* callback;
        bool dataChanged;
        u8 userData[1]; // More data follows
    };

private:
    virtual void RegisterHandlerEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength, bool dataChanged) override;
    void RecordStorageEventHandlerRegisterProxy(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength);
    u16 GetRecordBaseId() const;

protected:
    //In order to enable the register handler, this method has to be implemented and must return RGC_SUCCESS for regions managed by the RegisterHandler
    virtual RegisterGeneralChecks GetGeneralChecks(u16 component, u16 reg, u16 length) const { return RegisterGeneralChecks::RGC_LOCATION_DISABLED; }
    virtual RegisterHandlerCode CheckValues(u16 component, u16 reg, const u8* values, u16 length) const { return RegisterHandlerCode::SUCCESS; }
    // CAREFUL! Mapping is assumed to be static, meaning two calls with the same parameters must always return the same value in the out parameters.
    // Reporting different registers at different times will result in undefined behavior.
    virtual void MapRegister(u16 component, u16 reg, SupervisedValue& out, u32& persistedId) { };
    // Length of data was reported by the RegisterHandler through MapRegister.
    virtual void ChangeValue(u16 component, u16 reg, u8* data, u16 length) { };
    virtual void CommitRegisterChange(u16 component, u16 reg, RegisterHandlerSetSource source) { };
    virtual void OnRegisterRead(u16 component, u16 reg) { };

public:
    RegisterHandlerCode GetRegisterValues(u16 component, u16 reg, u8* values, u16 length);
    RegisterHandlerCodeStage SetRegisterValues(u16 component, u16 reg, const u8* values, u16 length, RegisterHandlerEventListener* callback = nullptr, u32 userType = 0, u8* userData = nullptr, u16 userDataLength = 0, RegisterHandlerSetSource source = RegisterHandlerSetSource::INTERNAL);


#endif //IS_ACTIVE(REGISTER_HANDLER)

private:

    // Inherited via RecordStorageEventListener
    constexpr static u32 USER_TYPE_COMPONENT_ACT_WRITE = 1;
    struct RecordStorageUserData
    {
        NodeId receiver;
        ModuleIdWrapper moduleId;
        u16 component;
        u16 registerAddress;
        u8 requestHandle;
    };
};
