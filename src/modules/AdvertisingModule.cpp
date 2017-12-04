/**

Copyright (c) 2014-2017 "M-Way Solutions GmbH"
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
#include <Node.h>
#include <AdvertisingModule.h>
#include <IoModule.h>
#include <Logger.h>
#include <Utility.h>

extern "C"{
#include <app_error.h>
}

//This module allows a number of advertising messages to be configured.
//These will be broadcasted periodically

/*
TODO: Who's responsible for restoring the mesh-advertising packet? This module or the Node?
 * */


AdvertisingModule::AdvertisingModule(u8 moduleId, Node* node, ConnectionManager* cm, const char* name)
	: Module(moduleId, node, cm, name)
{
	moduleVersion = 1;

	//Register callbacks n' stuff

	//Save configuration to base class variables
	//sizeof configuration must be a multiple of 4 bytes
	configurationPointer = &configuration;
	configurationLength = sizeof(AdvertisingModuleConfiguration);

	advJobHandle = NULL;

	//Start module configuration loading
	LoadModuleConfiguration();
}

void AdvertisingModule::ConfigurationLoadedHandler()
{
	u32 err = 0;
	//Does basic testing on the loaded configuration
	Module::ConfigurationLoadedHandler();

	//Version migration can be added here
	if(configuration.moduleVersion == this->moduleVersion){/* ... */};

	//Do additional initialization upon loading the config
	if(configuration.txPower != (i8)0xFF){ //-1 is the invalid value
		//Error code is not checked, will silently fail
		//FIXME: This will affect the device setting instead of only
		//The advrtising messages, implement advertising message scheduler
		err = sd_ble_gap_tx_power_set(configuration.txPower);
		if(err == NRF_SUCCESS){
			//FIXME: With advertising scheduler, should not Update value from config
			node->persistentConfig.dBmTX = configuration.txPower;
		}
	}

	//Start the Module...
	logt("ADVMOD", "Config set, txPower %d");

	//Delete previous job if it exists
	AdvertisingController::getInstance()->RemoveJob(advJobHandle);

	//Configure Advertising Jobs for all advertising messages
	if(configuration.messageCount == 1){
		AdvJob job = {
			AdvJobTypes::ADV_JOB_TYPE_SCHEDULED,
			3, //Slots
			0, //Delay
			MSEC_TO_UNITS(100, UNIT_0_625_MS), //AdvInterval
			0, //CurrentSlots
			0, //CurrentDelay
			BLE_GAP_ADV_TYPE_ADV_IND, //Advertising Mode
			{0}, //AdvData
			0, //AdvDataLength
			{0}, //ScanData
			0 //ScanDataLength
		};
		memcpy(&job.advData, configuration.messageData[0].messageData, configuration.messageData[0].messageLength);
		job.advDataLength = configuration.messageData[0].messageLength;

		advJobHandle = AdvertisingController::getInstance()->AddJob(&job);
	}


}

void AdvertisingModule::ResetToDefaultConfiguration()
{
	//Set default configuration values
	configuration.moduleId = moduleId;
	configuration.moduleActive = true;
	configuration.moduleVersion = 1;

	configuration.advertisingIntervalMs = 100;
	configuration.messageCount = 0;
	configuration.txPower = (i8)0xFF; //Set to invalid value

}

//void AdvertisingModule::NodeStateChangedHandler(discoveryState newState)
//{
//	if(newState == discoveryState::BACK_OFF || newState == discoveryState::DISCOVERY_OFF){
//		//Activate our advertising
//
//		//This is a small packet for debugging a node's state
//		if(Config->advertiseDebugPackets){
//			u8 buffer[31];
//			memset(buffer, 0, 31);
//
//			advStructureFlags* flags = (advStructureFlags*)buffer;
//			flags->len = SIZEOF_ADV_STRUCTURE_FLAGS-1;
//			flags->type = BLE_GAP_AD_TYPE_FLAGS;
//			flags->flags = BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;
//
//			advStructureManufacturer* manufacturer = (advStructureManufacturer*)(buffer+3);
//			manufacturer->len = 26;
//			manufacturer->type = BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA;
//			manufacturer->companyIdentifier = 0x24D;
//
//			AdvertisingModuleDebugMessage* msg = (AdvertisingModuleDebugMessage*)(buffer+7);
//
//			msg->debugPacketIdentifier = 0xDE;
//			msg->senderId = node->persistentConfig.nodeId;
//			msg->connLossCounter = node->connectionLossCounter;
//
//			for(int i=0; i<Config->meshMaxConnections; i++){
//				if(cm->connections[i]->handshakeDone()){
//					msg->partners[i] = cm->connections[i]->partnerId;
//					msg->rssiVals[i] = cm->connections[i]->rssiAverage;
//					msg->droppedVals[i] = (u8) cm->connections[i]->droppedPackets;
//				} else {
//					msg->partners[i] = 0;
//					msg->rssiVals[i] = 0;
//					msg->droppedVals[i] = 0;
//				}
//			}
//
//
//
//
//			char strbuffer[200];
//			Logger::getInstance()->convertBufferToHexString(buffer, 31, strbuffer, 200);
//
//			logt("ADVMOD", "ADV set to %s", strbuffer);
//
//			u32 err = sd_ble_gap_adv_data_set(buffer, 31, NULL, 0);
//			if(err != NRF_SUCCESS){
//				logt("ADVMOD", "Debug Packet corrupt");
//			}
//
//		}
//		else if(configuration.messageCount > 0){
//			u32 err = sd_ble_gap_adv_data_set(configuration.messageData[0].messageData, configuration.messageData[0].messageLength, NULL, 0);
//			if(err != NRF_SUCCESS){
//				logt("ADVMOD", "Adv msg corrupt");
//			}
//
//
//			char buffer[200];
//			Logger::getInstance()->convertBufferToHexString((u8*)configuration.messageData[0].messageData, 31, buffer, 200);
//
//			logt("ADVMOD", "ADV set to %s", buffer);
//
//
//
//			if(configuration.messageData[0].forceNonConnectable)
//			{
//				//TODO: ADVREF AdvertisingController::getInstance()->SetNonConnectable();
//			}
//
//			//Now, start advertising
//			//TODO: Use advertising parameters from config to advertise
//			//TODO: ADVREF AdvertisingController::getInstance()->SetAdvertisingState(advState::ADV_STATE_HIGH);
//		}
//
//	} else if (newState == discoveryState::DISCOVERY) {
//		//Do not trigger custom advertisings anymore, reset to node's advertising
//		node->UpdateJoinMePacket();
//	}
//}

void AdvertisingModule::ButtonHandler(u8 buttonId, u32 holdTimeDs)
{
//	//Put beacon into Asset mode, it will broadcast it's nodeId as an assetId
//	if(buttonId == 0 && holdTimeDs > SEC_TO_DS(3)){
//		logt("ADVMOD", "Asset mode activated");
//		assetMode = 1;
//
//		IoModule* ioModule = (IoModule*)node->GetModuleById(moduleID::IO_MODULE_ID);
//		if(ioModule != NULL){
//			ioModule->currentLedMode = ledMode::LED_MODE_ASSET;
//		}
//		cm->ForceDisconnectOtherConnections(NULL);
//	}
}


bool AdvertisingModule::TerminalCommandHandler(std::string commandName, std::vector<std::string> commandArgs)
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
