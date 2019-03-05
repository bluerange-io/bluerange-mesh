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
 * This is a test Module for testing stuff
 */

#pragma once

#include <Module.h>

#ifdef ACTIVATE_DEBUG_MODULE

enum class FloodMode : u8{
	OFF = 0,
	RELIABLE = 1,
	UNRELIABLE = 2,
	LISTEN = 3,
	UNRELIABLE_SPLIT = 4
};


class DebugModule: public Module
{
	private:

		#pragma pack(push, 1)
		//Module configuration that is saved persistently
		struct DebugModuleConfiguration : ModuleConfiguration{
			u8 deprecated_debugButtonRemoveEnrollmentDs;
			u8 debugButtonEnableUartDs;
			//Insert more persistent config values here
		};
		#pragma pack(pop)

		u32 rebootTimeDs; //Time until reboot

		//Counters for flood messages
		FloodMode floodMode;
		NodeId floodDestinationId;
		u32 floodMessagesPer100Sec; // the number of ticks per 100 sec should ideally be dividable by this or should be a multiple
		//Counters for flood messages
		u32 packetsOut;
		u32 packetsIn;

		//Counters for ping
		u32 pingSentTicks;
		u8 pingHandle;
		u16 pingCount;
		u16 pingCountResponses;

		#pragma pack(push)
		#pragma pack(1)

		#define SIZEOF_DEBUG_MODULE_INFO_MESSAGE 6
		typedef struct
		{
			u16 connectionLossCounter;
			u16 droppedPackets;
			u16 sentPackets;
		} DebugModuleInfoMessage;
		STATIC_ASSERT_SIZE(DebugModuleInfoMessage, 6);

		#define SIZEOF_DEBUG_MODULE_FORCE_REESTABLISH_MESSAGE 2
		typedef struct
		{
			u16 partnerId;

		} DebugModuleForceReestablishMessage;
		STATIC_ASSERT_SIZE(DebugModuleForceReestablishMessage, 2);

		#define SIZEOF_DEBUG_MODULE_PINGPONG_MESSAGE 1
		typedef struct
		{
			u8 ttl;

		} DebugModulePingpongMessage;
		STATIC_ASSERT_SIZE(DebugModulePingpongMessage, 1);

		#define SIZEOF_DEBUG_MODULE_LPING_MESSAGE 4
		typedef struct
		{
			NodeId leafNodeId;
			u16 hops;

		} DebugModuleLpingMessage;
		STATIC_ASSERT_SIZE(DebugModuleLpingMessage, 4);


		#define SIZEOF_DEBUG_MODULE_SET_DISCOVERY_MESSAGE 1
		typedef struct
		{
			u8 discoveryMode;

		} DebugModuleSetDiscoveryMessage;
		STATIC_ASSERT_SIZE(DebugModuleSetDiscoveryMessage, 1);


		#define SIZEOF_DEBUG_MODULE_RESET_MESSAGE 1
		typedef struct
		{
			u8 resetSeconds;

		} DebugModuleResetMessage;
		STATIC_ASSERT_SIZE(DebugModuleResetMessage, 1);


		#define SIZEOF_DEBUG_MODULE_FLOOD_MESSAGE 4
		typedef struct
		{
			u16 packetsIn;
			u16 packetsOut;

		} DebugModuleFloodMessage;
		STATIC_ASSERT_SIZE(DebugModuleFloodMessage, 4);


#define SIZEOF_DEBUG_MODULE_SET_FLOOD_MODE_MESSAGE 5
		typedef struct
		{
			NodeId floodDestinationId;
			u16 packetsPer100Sec;
			u8 floodMode;

		} DebugModuleSetFloodModeMessage;
		STATIC_ASSERT_SIZE(DebugModuleSetFloodModeMessage, 5);

		#pragma pack(pop)

		void CauseHardfault() const;


	public:
		DECLARE_CONFIG_AND_PACKED_STRUCT(DebugModuleConfiguration);

		enum class DebugModuleTriggerActionMessages : u8{
			RESET_NODE = 0,
			RESET_CONNECTION_LOSS_COUNTER = 1,
			FLOOD_MESSAGE = 2,
			GET_STATS_MESSAGE = 3,
			CAUSE_HARDFAULT_MESSAGE = 4,
			REQUEST_FORCE_REESTABLISH = 5,
			PING = 6,
			PINGPONG = 7,
			//SET_DISCOVERY = 8, deprecated
			LPING = 9,
			SET_FLOOD_MODE = 10,
			FORCE_REESTABLISH = 11, //Must be sent to a node that is connected to us to force a reestablishment
			SET_LIVEREPORTING_DEPRECATED = 12,
			EINK_SETANDDRAW_DEPRECATED = 13,
			GET_JOIN_ME_BUFFER = 14,
			RESET_FLOOD_COUNTER = 15

		};

		enum class DebugModuleActionResponseMessages : u8{
			STATS_MESSAGE = 3,
			PING_RESPONSE = 6,
			//SET_DISCOVERY_RESPONSE = 8, deprecated
			LPING_RESPONSE = 9,
			JOIN_ME_BUFFER_ITEM = 10,
			EINK_SETANDDRAW_RESPONSE_DEPRECATED = 11
		};

		DebugModule();

		void ConfigurationLoadedHandler(ModuleConfiguration* migratableConfig, u16 migratableConfigLength) override;

		void ResetToDefaultConfiguration() override;

		void TimerEventHandler(u16 passedTimeDs) override;

		void SendStatistics(NodeId receiver) const;

		void ButtonHandler(u8 buttonId, u32 holdTimeDs) USE_BUTTONS_OVERRIDE;

		#ifdef TERMINAL_ENABLED
		bool TerminalCommandHandler(char* commandArgs[], u8 commandArgsSize) override;
		#endif

		void MeshMessageReceivedHandler(BaseConnection* connection, BaseConnectionSendData* sendData, connPacketHeader* packetHeader) override;
		u32 getPacketsIn();
		u32 getPacketsOut();
		
};

#endif
