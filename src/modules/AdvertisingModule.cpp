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

#include <adv_packets.h>
#include <AdvertisingController.h>
#include <AdvertisingModule.h>
#include <Logger.h>
#include <Utility.h>
#include <Storage.h>

extern "C"{
#include <app_error.h>
}

//This module allows a number of advertising messages to be configured.
//These will be broadcasted periodically

/*
TODO: Who's responsible for restoring the mesh-advertising packet? This module or the Node?
 * */


AdvertisingModule::AdvertisingModule(u8 moduleId, Node* node, ConnectionManager* cm, const char* name, u16 storageSlot)
	: Module(moduleId, node, cm, name, storageSlot)
{
	//Register callbacks n' stuff

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(AdvertisingModuleConfiguration);

	//Start module configuration loading
	LoadModuleConfiguration();
}

void AdvertisingModule::ConfigurationLoadedHandler()
{
	u32 err = 0;
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if(configuration.moduleVersion == 1){/* ... */};

	//Do additional initialization upon loading the config
	if(configuration.txPower != 0xFF){
		//Error code is not checked, will silently fail
		err = sd_ble_gap_tx_power_set(configuration.txPower);
		if(err == NRF_SUCCESS){
			//Update value from config
			Config->radioTransmitPower = configuration.txPower;
		}
	}

	//Start the Module...
	logt("ADVMOD", "Config set, txPower %d");



}

void AdvertisingModule::TimerEventHandler(u16 passedTimeDs, u32 appTimerDs)
{
	//Activate asset adv message only after some time
	//because if we had connections, the node would have reset the packet
	//to a join_me packet (that's a hack,...)
	//The assetId is 50 less than the nodeId because of another hack
	//while implementing serialNumbers .... :-(
	if(assetMode != 0 && assetMode < 11) assetMode++;
	if(assetMode == 10){
		const char* command = "set_config this adv 01:01:01:00:64:00:01:04:05:61:02:01:06:08:FF:4D:02:02:%02x:%02x:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00 0";
		char tmp[strlen(command)];
		sprintf(tmp, command, Config->serialNumberIndex & 0xFF, (Config->serialNumberIndex) >> 8);

		Terminal::ProcessLine(tmp);
		node->Stop();

	}

}

void AdvertisingModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = true;
	configuration.moduleVersion = 1;

	memset(configuration.messageData[0].messageData, 0, 31);

	advStructureFlags flags;
	advStructureName name;

	flags.len = SIZEOF_ADV_STRUCTURE_FLAGS-1;
	flags.type = BLE_GAP_AD_TYPE_FLAGS;
	flags.flags = BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;

	name.len = SIZEOF_ADV_STRUCTURE_NAME-1;
	name.type = BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME;
	name.name[0] = 'A';
	name.name[1] = 'B';

	configuration.advertisingIntervalMs = 100;
	configuration.messageCount = 1;
	configuration.txPower = 0xFF; //Set to invalid value

	configuration.messageData[0].messageLength = 31;
	memcpy(configuration.messageData[0].messageData, &flags, SIZEOF_ADV_STRUCTURE_FLAGS);
	memcpy(configuration.messageData[0].messageData+SIZEOF_ADV_STRUCTURE_FLAGS, &name, SIZEOF_ADV_STRUCTURE_NAME);
}

void AdvertisingModule::NodeStateChangedHandler(discoveryState newState)
{
	if(newState == discoveryState::BACK_OFF || newState == discoveryState::DISCOVERY_OFF){
		//Activate our advertising

		//This is a small packet for debugging a node's state
		if(Config->advertiseDebugPackets){
			u8 buffer[31];
			memset(buffer, 0, 31);

			advStructureFlags* flags = (advStructureFlags*)buffer;
			flags->len = SIZEOF_ADV_STRUCTURE_FLAGS-1;
			flags->type = BLE_GAP_AD_TYPE_FLAGS;
			flags->flags = BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;

			advStructureManufacturer* manufacturer = (advStructureManufacturer*)(buffer+3);
			manufacturer->len = 26;
			manufacturer->type = BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA;
			manufacturer->companyIdentifier = 0x24D;

			AdvertisingModuleDebugMessage* msg = (AdvertisingModuleDebugMessage*)(buffer+7);

			msg->debugPacketIdentifier = 0xDE;
			msg->senderId = node->persistentConfig.nodeId;
			msg->connLossCounter = node->persistentConfig.connectionLossCounter;

			for(int i=0; i<Config->meshMaxConnections; i++){
				if(cm->connections[i]->handshakeDone()){
					msg->partners[i] = cm->connections[i]->partnerId;
					msg->rssiVals[i] = cm->connections[i]->rssiAverage;
					msg->droppedVals[i] = cm->connections[i]->droppedPackets;
				} else {
					msg->partners[i] = 0;
					msg->rssiVals[i] = 0;
					msg->droppedVals[i] = 0;
				}
			}




			char strbuffer[200];
			Logger::getInstance().convertBufferToHexString(buffer, 31, strbuffer, 200);

			logt("ADVMOD", "ADV set to %s", strbuffer);

			u32 err = sd_ble_gap_adv_data_set(buffer, 31, NULL, 0);
			if(err != NRF_SUCCESS){
				logt("ADVMOD", "Debug Packet corrupt");
			}

			AdvertisingController::SetAdvertisingState(advState::ADV_STATE_HIGH);
		}
		else if(configuration.messageCount > 0){
			u32 err = sd_ble_gap_adv_data_set(configuration.messageData[0].messageData, configuration.messageData[0].messageLength, NULL, 0);
			if(err != NRF_SUCCESS){
				logt("ADVMOD", "Adv msg corrupt");
			}


			char buffer[200];
			Logger::getInstance().convertBufferToHexString((u8*)configuration.messageData[0].messageData, 31, buffer, 200);

			logt("ADVMOD", "ADV set to %s", buffer);



			if(configuration.messageData[0].forceNonConnectable)
			{
				AdvertisingController::SetNonConnectable();
			}

			//Now, start advertising
			//TODO: Use advertising parameters from config to advertise
			AdvertisingController::SetAdvertisingState(advState::ADV_STATE_HIGH);
		}

	} else if (newState == discoveryState::DISCOVERY) {
		//Do not trigger custom advertisings anymore, reset to node's advertising
		node->UpdateJoinMePacket();
	}
}

void AdvertisingModule::ButtonHandler(u8 buttonId, u32 holdTimeDs)
{
	//Put beacon into Asset mode, it will broadcast it's nodeId as an assetId
	if(buttonId == 0 && holdTimeDs > SEC_TO_DS(3)){
		logt("ADVMOD", "Asset mode activated");
		assetMode = 1;
		LedRed->On();
		LedGreen->On();
		LedBlue->On();
		node->currentLedMode = ledMode::LED_MODE_ASSET;
		cm->ForceDisconnectOtherConnections(NULL);
	}
}


bool AdvertisingModule::TerminalCommandHandler(string commandName, vector<string> commandArgs)
{
	if(commandArgs.size() >= 2 && commandArgs[1] == moduleName)
	{
		if(commandName == "action")
		{
			if(commandArgs[1] != moduleName) return false;

			if(commandArgs[2] == "broadcast_debug")
			{
				Config->advertiseDebugPackets = !Config->advertiseDebugPackets;
				logt("ADVMOD", "Debug Packets are now set to %u", Config->advertiseDebugPackets);

				return true;
			}
		}

	}

	//Must be called to allow the module to get and set the config
	return Module::TerminalCommandHandler(commandName, commandArgs);
}
