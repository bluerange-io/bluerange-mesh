////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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

#if IS_ACTIVE(SIG_MESH)
#include <SigElement.h>
#include <SigModel.h>
#include <SigState.h>
#endif

enum class CapabilityEntryType : u8
{
    INVALID = 0,
    HARDWARE = 1,
    SOFTWARE = 2,

    NOT_READY = 100,    //The module is currently not ready to report the capability with the provided index but will be in the near future.
};

struct CapabilityEntry
{
    CapabilityEntryType type;
    //WARNING: The following values are not guaranteed to have a terminating zero!
    char manufacturer[32];
    char modelName[53];
    char revision[32];
};

enum class SetActiveReturnValues : u8
{
    SUCCESS              = 0,
    NO_CONFIGURATION     = 1,
    NO_SUCH_MODULE       = 2,
    RECORD_STORAGE_ERROR = 3,
};

class Node;

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
 * Module ids must persist between updates to guearantee that the
 * same module receives the same storage slot.
 *
 * ModuleIds must also be the same within a mesh network to guarantee the correct
 * delivery of actions and responses.
 */
class Module:
        public RecordStorageEventListener
{
    friend class FruityMesh;

protected:
        struct ModuleInformation {
            ModuleId moduleId;
            u8 moduleVersion;
        };
        STATIC_ASSERT_SIZE(ModuleInformation, 2);

        //This must be called in the constructor to reset all values to default
        virtual void ResetToDefaultConfiguration() = 0;


    public:
        const ModuleId moduleId;
        const char* moduleName;


        //The constructor is used to initialize all members and must call ResetToDefaultConfiguration in all subclasses
        Module(ModuleId moduleId, const char* name);
        virtual ~Module();

        //These two variables must be set by the submodule in the constructor before loading the configuration
        ModuleConfiguration* configurationPointer;
        u16 configurationLength;

        enum class ModuleConfigMessages : u8
        {
            SET_CONFIG = 0, 
            SET_CONFIG_RESULT = 1,
            SET_ACTIVE = 2, 
            SET_ACTIVE_RESULT = 3,
            GET_CONFIG = 4, 
            CONFIG = 5,
            GET_MODULE_LIST = 6, 
            MODULE_LIST = 7
        };

        enum class ModuleSaveAction : u8{
            SAVE_MODULE_CONFIG_ACTION,
            PRE_ENROLLMENT_RECORD_DELETE
        };

        struct SaveModuleConfigAction {
            NodeId sender;
            ModuleId moduleId;
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
        virtual void ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength){};

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
        virtual RoutingDecision MessageRoutingInterceptor(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader const * packetHeader) { return 0; };

        //This handler receives all connection packets addressed to this node
        virtual void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader const * packetHeader);

        //This handler is called before the node is enrolled, it can return PRE_ENROLLMENT_ codes
        virtual PreEnrollmentReturnCode PreEnrollmentHandler(connPacketModule* packet, u16 packetLength);

        virtual void RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength) override;

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
        virtual DfuStartDfuResponseCode CheckComponentUpdateRequest(connPacketModule const * inPacket, u32 version, ImageType imageType, u8 componentId){ return DfuStartDfuResponseCode::MODULE_NOT_UPDATABLE; };

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
        virtual void ButtonHandler(u8 buttonId, u32 holdTime) {};
#endif

        //This method indicates if this module is interested in a mesh access connection that
        //should be created from a different node to this node. Typically is used by assets
        //as they are not permanent members of the mesh but regularly have some new sensor values
        //to publish.
        virtual bool IsInterestedInMeshAccessConnection() { return false; }

};
