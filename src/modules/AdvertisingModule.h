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
 * The AdvertisingModule is used to broadcast user-data that is not related with
 * the mesh during times where no mesh discovery is ongoing. It is used
 * to broadcast messages to smartphones or other devices from all mesh nodes.
 */

#pragma once

#include <Module.h>

#ifdef ACTIVATE_ADVERTISING_MODULE

// Be sure to check the advertising controller for the maximum number of supported jobs before increasing this
#define ADVERTISING_MODULE_MAX_MESSAGES 1
#define ADVERTISING_MODULE_MAX_MESSAGE_LENGTH 31

class AdvertisingModule: public Module
{
	private:

		u32 assetMode = false;

		#pragma pack(push, 1)
		struct AdvertisingMessage{
			u8 messageId;
			u8 forceNonConnectable : 1; //Always send this message non-connectable
			u8 forceConnectable : 1; //Message is only sent, when it is possible to send it in connectable mode (if we have a free slave connection)
			u8 reserved : 1;
			u8 messageLength : 5;
			SimpleArray<u8, ADVERTISING_MODULE_MAX_MESSAGE_LENGTH> messageData;
		};

		//Module configuration that is saved persistently
		struct AdvertisingModuleConfiguration : ModuleConfiguration{
			//The interval at which the device advertises
			//If multiple messages are configured, they will be distributed round robin
			u16 advertisingIntervalMs;
			//Number of messages
			u8 messageCount;
			i8 txPower;
			SimpleArray<AdvertisingMessage, ADVERTISING_MODULE_MAX_MESSAGES> messageData;
			//Insert more persistent config values here
		};
		#pragma pack(pop)

		SimpleArray<AdvJob*, ADVERTISING_MODULE_MAX_MESSAGES> advJobHandles;

		u16 maxMessages = ADVERTISING_MODULE_MAX_MESSAGES; //Save this, so that it can be requested

		//Set all advertising messages at once, the old configuration will be overwritten
		void SetAdvertisingMessages(u8* data, u16 dataLength);

		#pragma pack(push)
		#pragma pack(1)

		typedef struct
		{
			u8 debugPacketIdentifier;
			NodeId senderId;
			u16 connLossCounter;
			SimpleArray<NodeId, 4> partners;
			SimpleArray<i8, 3> rssiVals;
			SimpleArray<u8, 3> droppedVals;

		} AdvertisingModuleDebugMessage;

		#pragma pack(pop)


	public:
		DECLARE_CONFIG_AND_PACKED_STRUCT(AdvertisingModuleConfiguration);

		AdvertisingModule();

		void ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength) override;

		void ResetToDefaultConfiguration() override;

		void ButtonHandler(u8 buttonId, u32 holdTime) USE_BUTTONS_OVERRIDE;

		#ifdef TERMINAL_ENABLED
		bool TerminalCommandHandler(char* commandArgs[], u8 commandArgsSize) override;
		#endif
};

#endif

