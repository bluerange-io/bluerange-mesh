////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2019 M-Way Solutions GmbH
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

#pragma once

#define INVALID_U8_CONFIG 0xFF
#define INVALID_U16_CONFIG 0xFFFF
#define INVALID_U32_CONFIG 0xFFFFFFFF

#include <Config.h>
#include <Boardconfig.h>
#include <Logger.h>
#include <ConnectionManager.h>
#include <Terminal.h>
#include <RecordStorage.h>

extern "C"{
#include <ble.h>
}


class Node;

#define MODULE_NAME_MAX_SIZE 10

class Module:
		public RecordStorageEventListener,
		public TerminalCommandListener
{
	friend class FruityMesh;

protected:
		struct ModuleInformation {
			u8 moduleId;
			u8 moduleVersion;
		};

		//Called when the load failed
		virtual void ResetToDefaultConfiguration(){};


	public:
		u8 moduleId;
		u8 moduleVersion;
		char moduleName[MODULE_NAME_MAX_SIZE];

		//Constructor is passed
		Module(u8 moduleId, const char* name);
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
			moduleID moduleId;
			u8 requestHandle;
		};

		//This function is called on the module to load its saved configuration and start
		void LoadModuleConfigurationAndStart();

		//Constructs a simple TriggerAction message and sends it
		void SendModuleActionMessage(u8 messageType, NodeId toNode, u8 actionType, u8 requestHandle, const u8* additionalData, u16 additionalDataSize, bool reliable, bool loopback) const;
		void SendModuleActionMessage(u8 messageType, NodeId toNode, u8 actionType, u8 requestHandle, const u8* additionalData, u16 additionalDataSize, bool reliable) const;


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

		//Called when the module should update configuration parameters
		virtual void SetConfigurationHandler(u8* configuration, u8 length){};

		//Called when the module should send back its data
		virtual void GetDataHandler(u8* request, u8 length){};

		//Handle system events
		virtual void SystemEventHandler(u32 systemEvent){};

		//This handler receives all timer events
		virtual void TimerEventHandler(u16 passedTimeDs){};

		//This handler receives all ble events and can act on them
		virtual void BleEventHandler(const ble_evt_t &bleEvent){};

		//When a mesh connection is connected with handshake and everything or if it is disconnected, the ConnectionManager will call this handler
		virtual void MeshConnectionChangedHandler(MeshConnection& connection){};

		//This handler receives all connection packets
		virtual void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader);

		//This handler is called before the node is enrolled, it can return PRE_ENROLLMENT_ codes
		virtual PreEnrollmentReturnCode PreEnrollmentHandler(connPacketModule* packet, u16 packetLength);

		//Changing the node state will affect some module's behaviour
		virtual void NodeStateChangedHandler(discoveryState newState){};

		virtual void ConnectionDisconnectedHandler(BaseConnection* connection){};

		virtual void ConnectionTimeoutHandler(ConnectionTypes connectionType, ble_evt_t* bleEvent){};

		virtual void RecordStorageEventHandler(u16 recordId, RecordStorageResultCode resultCode, u32 userType, u8* userData, u16 userDataLength) override;

		//MeshAccessConnections should only allow authorized packets to be sent into the mesh
		//This function is called once a packet was received through a meshAccessConnection to
		//query if the packet can be sent through. It can also be modified by this handler
		virtual MeshAccessAuthorization CheckMeshAccessPacketAuthorization(BaseConnectionSendData* sendData, u8* data, u32 fmKeyId){ return MeshAccessAuthorization::MA_AUTH_UNDETERMINED; };

		//This method must be implemented by modules that support component updates
		//The module must answer weather it wants to accept the update (0) or not (negative result)
		//If the request is handled asynchronously, the module must return dfu start response QUERY_WAITING and must then manually call ContinueDfuStart
		virtual i32 CheckComponentUpdateRequest(connPacketModule* inPacket, u32 version, u8 imageType, u8 componentId){ return -1; };

		//This method allows a module to update its component
		//The module must ensure that subsequent calls to this method do not interfere with the update process
		virtual void StartComponentUpdate(u8 componentId, u8* imagePtr, u32 imageLength){};

		//The Terminal Command handler is called for all modules with the user input
#ifdef TERMINAL_ENABLED
		virtual bool TerminalCommandHandler(char* commandArgs[],u8 commandArgsSize);
#endif

#ifdef USE_BUTTONS
		virtual void ButtonHandler(u8 buttonId, u32 holdTime) {};
#define USE_BUTTONS_OVERRIDE override
#else
		void ButtonHandler(u8 buttonId, u32 holdTime) {};
#define USE_BUTTONS_OVERRIDE 
#endif


};
