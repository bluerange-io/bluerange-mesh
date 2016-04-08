/**

Copyright (c) 2014-2015 "M-Way Solutions GmbH"
FruityMesh - Bluetooth Low Energy mesh protocol [http://mwaysolutions.com/]

This file is part of FruityMesh

FruityMesh is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

/*
 * The Module class can be subclassed for a number of purposes:
 * - Implement a driver for a sensor or an actuator
 * - Add functionality like parsing advertising data, etc...
 *
 * It provides a basic set of handlers that are called from the main event handling
 * routines and the received events can be used and acted upon.
 *
 */

#pragma once


#include <conn_packets.h>
#include <Config.h>
#include <Logger.h>
#include <ConnectionManager.h>
#include <Terminal.h>
#include <Storage.h>

extern "C"{
#include <ble.h>
}



class Node;

#define MODULE_NAME_MAX_SIZE 10

class Module : public StorageEventListener, public TerminalCommandListener
{
	private:


protected:
		Node* node;
		ConnectionManager* cm;
		u8 moduleId;
		u16 storageSlot;

		//Pay attention that the module configuration is not packed and will
		//therefore be padded by the compiler!!!
		struct ModuleConfiguration{
			u8 moduleId; //Id of the module, compared upon load and must match
			u8 moduleVersion; //version of the configuration
			u8 moduleActive; //Activate or deactivate the module
			u8 reserved;
		};


		//This function is called by the module itself when it wants to save its configuration
		virtual void SaveModuleConfiguration();

		//This function is called by the module to get its configuration on startup
		virtual void LoadModuleConfiguration();

		//Called when the load failed
		virtual void ResetToDefaultConfiguration(){};

		//Constructs a simple TriggerAction message and sends it
		void SendModuleActionMessage(u8 messageType, nodeID toNode, u8 actionType, u8 requestHandle, u8* additionalData, u16 additionalDataSize, bool reliable);



	public:
		char moduleName[MODULE_NAME_MAX_SIZE];

		//Constructor is passed
		Module(u8 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot);
		virtual ~Module();

		//These two variables must be set by the submodule in the constructor before loading the configuration
		ModuleConfiguration* configurationPointer;
		u16 configurationLength;

		enum ModuleConfigMessages
		{
			SET_CONFIG = 0, SET_CONFIG_RESULT = 1,
			SET_ACTIVE = 2, SET_ACTIVE_RESULT = 3,
			GET_CONFIG = 4, CONFIG = 5,
			GET_MODULE_LIST = 6, MODULE_LIST = 7
		};


		//##### Handlers that can be implemented by any module, but are implemented empty here

		//This function is called as soon as the module settings have been loaded or updated
		//A basic error-checking implementation is provided and can be called by the subclass
		//before reading the configuration
		void ConfigurationLoadedHandler();

		//Called when the module should update configuration parameters
		virtual void SetConfigurationHandler(u8* configuration, u8 length){};

		//Called when the module should send back its data
		virtual void GetDataHandler(u8* request, u8 length){};

		//Handle system events
		virtual void SystemEventHandler(u32 systemEvent){};

		//This handler receives all timer events
		virtual void TimerEventHandler(u16 passedTime, u32 appTimer){};

		//This handler receives all ble events and can act on them
		virtual void BleEventHandler(ble_evt_t* bleEvent){};

		//When a mesh connection is connected with handshake and everything or if it is disconnected, the ConnectionManager will call this handler
		virtual void MeshConnectionChangedHandler(Connection* connection){};

		//This handler receives all connection packets
		virtual void ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength);

		//Changing the node state will affect some module's behaviour
		virtual void NodeStateChangedHandler(discoveryState newState){};

		//The Terminal Command handler is called for all modules with the user input
#ifdef TERMINAL_ENABLED
		virtual bool TerminalCommandHandler(string commandName, vector<string> commandArgs);
#else
		bool TerminalCommandHandler(string commandName, vector<string> commandArgs);
#endif

};
