////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2022 M-Way Solutions GmbH
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

#include <ConnectionHandle.h>
#include <FmTypes.h>
#include <Module.h>
#include <PacketQueue.h>
#include <atomic>

#if IS_ACTIVE(APP_UART)

/*
 * This module allows sending terminal commands and receiving terminal logs via
 * a mesh access connection.
 */

// This should be set to the correct vendor and subId
constexpr VendorModuleId APP_UART_MODULE_ID = GET_VENDOR_MODULE_ID(0x024D, 1);

constexpr u8 APP_UART_MODULE_CONFIG_VERSION = 1;

constexpr u32 APP_UART_LOG_BUFFER_SIZE = 4096;

constexpr u16 APP_UART_LOG_SEND_INTERVAL_DS = 1;

constexpr u8 APP_UART_MAX_NUM_PARTNERS = 8;

#pragma pack(push)
#pragma pack(1)
// Module configuration that is saved persistently (size must be multiple of 4)
struct AppUartModuleConfiguration : VendorModuleConfiguration {
    // Insert more persistent config values here
    u8 exampleValue;
};

struct AppUartLogRemain {
    u16 sentLength;
    u16 logLen;
};

#pragma pack(pop)

class AppUartModule : public Module {
private:
    u16 readBufferOffset = 0;
    char readBuffer[TERMINAL_READ_BUFFER_LENGTH];
    bool lineToReadAvailable = false;
    u32 logBuffer[APP_UART_LOG_BUFFER_SIZE / sizeof(u32)] = {};

    struct AppUartPartner {
        NodeId partnerId;
        u16 connectionHandle;
    };

    AppUartPartner partners[APP_UART_MAX_NUM_PARTNERS];

    PacketQueue logQueue;
    std::atomic_bool isSendingLog = { false };

public:
    enum AppUartModuleTriggerActionMessages {
        TERMINAL_COMMAND = 0,
        SEND_LOG = 1,
        HANDSHAKE = 2,
    };

    enum AppUartModuleActionResponseMessages {
        TERMINAL_RETURN_TYPE = 0,
        RECEIVE_LOG = 1,
        HANDSHAKE_DONE = 2,
    };

    //####### Module messages (these need to be packed)
#pragma pack(push)
#pragma pack(1)

    static constexpr int SIZEOF_APP_UART_MODULE_HANDSHAKE_MESSAGE = 2;

    typedef struct {
        NodeId partnerId;
    } AppUartModuleHandshakeMessage;

    STATIC_ASSERT_SIZE(AppUartModuleHandshakeMessage, SIZEOF_APP_UART_MODULE_HANDSHAKE_MESSAGE);

    static constexpr int SIZEOF_APP_UART_MODULE_TERMINAL_RESPONSE_MESSAGE = 1;

    typedef struct {
        u8 commandSuccess;
    } AppUartModuleTerminalResponseMessage;

    STATIC_ASSERT_SIZE(AppUartModuleTerminalResponseMessage, SIZEOF_APP_UART_MODULE_TERMINAL_RESPONSE_MESSAGE);

    static constexpr int SIZEOF_APP_UART_MODULE_TERMINAL_COMMAND_MESSAGE_STATIC = 3;
    static constexpr int SIZEOF_APP_UART_MODULE_TERMINAL_COMMAND_MESSAGE
        = MAX_MESH_PACKET_SIZE - SIZEOF_CONN_PACKET_MODULE_VENDOR;
    static constexpr int DATA_MAX_LEN
        = SIZEOF_APP_UART_MODULE_TERMINAL_COMMAND_MESSAGE - SIZEOF_APP_UART_MODULE_TERMINAL_COMMAND_MESSAGE_STATIC;
    typedef struct {
        MessageType splitHeader;
        u8 splitCount;
        u8 partLen;
        char data[DATA_MAX_LEN];
    } AppUartModuleTerminalCommandMessage;
    STATIC_ASSERT_SIZE(AppUartModuleTerminalCommandMessage, SIZEOF_APP_UART_MODULE_TERMINAL_COMMAND_MESSAGE);

    typedef AppUartModuleTerminalCommandMessage AppUartModuleTerminalLogMessage;

#pragma pack(pop)
    //####### Module messages end

    // Declare the configuration used for this module
    DECLARE_CONFIG_AND_PACKED_STRUCT(AppUartModuleConfiguration);

    AppUartModule();

    void ConfigurationLoadedHandler(u8* migratableConfig, u16 migratableConfigLength) override;

    void ResetToDefaultConfiguration() override;

    void TimerEventHandler(u16 passedTimeDs) override;

    void GapDisconnectedEventHandler(const FruityHal::GapDisconnectedEvent& disconnectedEvent);

    void MeshMessageReceivedHandler(
        BaseConnection* connection, BaseConnectionSendData* sendData, ConnPacketHeader const* packetHeader) override;

#ifdef TERMINAL_ENABLED
    TerminalCommandHandlerReturnType TerminalCommandHandler(const char* commandArgs[], u8 commandArgsSize) override;
#endif

    MeshAccessConnectionHandle GetMeshAccessConnectionHandleByPartnerId(NodeId nodeId) const;

    /// Returns the input buffer read by terminal.
    const char* GetReadBuffer() { return readBuffer; }
    /// Returns the current offset into the input buffer.
    u16 GetReadBufferOffset() const { return readBufferOffset; }
    /// Returns true if there is a line available in the input buffer.
    bool GetLineToReadAvailable() const { return lineToReadAvailable; }

    /// Tries to place a log entry in the internal queue.
    bool PutAppLogQueue(const char* log, u16 length);

private:
    /// Adds a partner to the list of nodes the logs are sent to.
    void PushPartner(NodeId nodeId);
    /// Removes a partner from the list of nodes the logs are sent to.
    void RemovePartner(u16 connectionHandle);

    /// Checks if there are currently any partners to send logs to.
    bool HasPartner();

    /// Change the flag indicating if the module is currently sending logs.
    void SetIsSendingLog(bool state) { isSendingLog.store(state, std::memory_order_release); }
    /// Returns true if the module is currently in the process of sending logs.
    bool GetIsSendingLog() { return isSendingLog.load(std::memory_order_acquire); }

    /// Validate if queuing is currently possible.
    bool CanQueueLog();

    /// Initiates sending the queued log messages to the stored partners.
    ErrorTypeUnchecked DoSendAppLogQueue();

    /// Implements sending the queued log messages to the stored partners. Do not call this method
    /// directly, call DoSendAppLogQueue instead.
    ErrorTypeUnchecked SendAppLogQueue();

    /// Clears the internal log queues.
    void DiscardLogQueue();
};

#endif // IS_ACTIVE(APP_UART)
