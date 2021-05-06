////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2021 M-Way Solutions GmbH
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

#include <Config.h>
#include <Node.h>
#include <AdvertisingController.h>
#include <ScanController.h>
#include <Logger.h>
#include <GAPController.h>
#include "FruityHal.h"
#include <GlobalState.h>
#include <ConnectionManager.h>

#include <Utility.h>

GAPController & GAPController::GetInstance()
{
    return GS->gapController;
}

//This function configures the Generic access Profile
//GAP is a service that must be implemented on all BLE devices
//These are some basic values that other devices can request
//Some of these values might be writable over the air
//The current characteristics are
//        Device name
//        Appearance
//        Peripheral Preferred Connection Parameters
void GAPController::BleConfigureGAP() const{
    ErrorType err = ErrorType::SUCCESS;
    //Set up an open link write permission
    //There are multiple security modes defined in the BLE spec
    //Use these macros: http://developer.nordicsemi.com/nRF51_SDK/doc/7.1.0/s110/html/a00813.html
    
    // no access
    FruityHal::BleGapConnSecMode secPermissionClosed;
    FH_CONNECTION_SECURITY_MODE_SET_NO_ACCESS(&secPermissionClosed);

    err = FruityHal::BleGapNameSet(secPermissionClosed, (const u8*)DEVICE_NAME, (u16)strlen(DEVICE_NAME));
    FRUITYMESH_ERROR_CHECK(err); //OK

    //Set the appearance of the device as defined in http://developer.nordicsemi.com/nRF51_SDK/doc/7.1.0/s110/html/a00837.html
    err = FruityHal::BleGapAppearance(FruityHal::BleAppearance::GENERIC_COMPUTER);
    FRUITYMESH_ERROR_CHECK(err); //OK

    //Set gap peripheral preferred connection parameters (not used by the mesh implementation)
    FruityHal::BleGapConnParams gapConnectionParams;
    CheckedMemset(&gapConnectionParams, 0, sizeof(gapConnectionParams));

    gapConnectionParams.minConnInterval = Conf::GetInstance().meshMinConnectionInterval;
    gapConnectionParams.maxConnInterval = Conf::GetInstance().meshMaxConnectionInterval;
    gapConnectionParams.slaveLatency = Conf::meshPeripheralSlaveLatency;
    gapConnectionParams.connSupTimeout = Conf::meshConnectionSupervisionTimeout;
    err = FruityHal::BleGapConnectionPreferredParamsSet(gapConnectionParams);
    FRUITYMESH_ERROR_CHECK(err); //OK
}

//Connect to a specific peripheral
ErrorType GAPController::ConnectToPeripheral(const FruityHal::BleGapAddr &address, u16 connectionInterval, u16 timeout) const
{
    ErrorType err = ErrorType::SUCCESS;

    FruityHal::BleGapScanParams scanParams;
    CheckedMemset(&scanParams, 0x00, sizeof(scanParams));
    FruityHal::BleGapConnParams connectionParams;
    CheckedMemset(&connectionParams, 0x00, sizeof(connectionParams));

    scanParams.interval = Conf::meshConnectingScanInterval;
    scanParams.window = Conf::meshConnectingScanWindow;
    scanParams.timeout = timeout;

    connectionParams.minConnInterval = connectionInterval;
    connectionParams.maxConnInterval = connectionInterval;
    connectionParams.slaveLatency = Conf::meshPeripheralSlaveLatency;
    connectionParams.connSupTimeout = Conf::meshConnectionSupervisionTimeout;

    //Connect to the peripheral
    err = FruityHal::BleGapConnect(address, scanParams, connectionParams);
    if(err != ErrorType::SUCCESS){
        logt("WARNING", "GATT connect fail %d", (u8)err);
        GS->logger.LogCustomCount(CustomErrorTypes::COUNT_GATT_CONNECT_FAILED);
        //Just ignore it, the connection will not happen
        return err;
    }

    logt("CONN", "pairing");

    return ErrorType::SUCCESS;
}

void GAPController::GapDisconnectedEventHandler(const FruityHal::GapDisconnectedEvent & disconnectEvent)
{
    logt("C", "Disconnected device %d", (u8)disconnectEvent.GetReason());
    ConnectionManager::GetInstance().GapConnectionDisconnectedHandler(disconnectEvent);
}

void GAPController::GapConnectedEventHandler(const FruityHal::GapConnectedEvent & connvectedEvent)
{
    logt("C", "Connected device");

    //Connection stops advertising
    ConnectionManager::GetInstance().GapConnectionConnectedHandler(connvectedEvent);
}

void GAPController::GapTimeoutEventHandler(const FruityHal::GapTimeoutEvent& gapTimeoutEvent)
{
    if (gapTimeoutEvent.GetSource() == FruityHal::GapTimeoutSource::CONNECTION)
    {
        ConnectionManager::GetInstance().GapConnectingTimeoutHandler(gapTimeoutEvent);
    }
}

void GAPController::GapSecurityInfoRequestEvenetHandler(const FruityHal::GapSecurityInfoRequestEvent & securityInfoRequestEvent)
{
    if (Conf::encryptionEnabled) {
        //With this request, we receive the key identifier and a random number
        //This identification is used to select the correct key

        //TODO: Currently, we skip that process and select our mesh network key
        //If we want multiple keys, we need to implement some more logic to select the key

        //This is our security key
        FruityHal::BleGapEncInfo key;
        CheckedMemset(&key, 0x00, sizeof(key));
        key.isGeneratedUsingLeSecureConnections  = 0;
        key.isAuthenticatedKey = 1; //This key is authenticated
        CheckedMemcpy(&key.longTermKey, GS->node.configuration.networkKey, 16); //Copy our mesh network key
        key.longTermKeyLength = 16;

        //Reply  with our stored key
        const ErrorType err = FruityHal::BleGapSecInfoReply(
            securityInfoRequestEvent.GetConnectionHandle(),
            &key, //This is our stored long term key
            nullptr, //We do not have an identity resolving key
            nullptr //We do not have signing info
        );

        if (err != ErrorType::SUCCESS)
        {
            GS->logger.LogCustomError(CustomErrorTypes::WARN_GAP_SEC_INFO_REPLY_FAILED, (u32)err);
        }

        logt("SEC", "SEC_INFO_REQUEST received, replying with key %02x:%02x:%02x...%02x:%02x", 
            key.longTermKey[0], key.longTermKey[1], key.longTermKey[2], key.longTermKey[14], key.longTermKey[15]);
    }
    else {
        logt("SEC", "No Encryption available");
        const ErrorType err = FruityHal::Disconnect(securityInfoRequestEvent.GetConnectionHandle(), FruityHal::BleHciError::REMOTE_USER_TERMINATED_CONNECTION);
        if (err != ErrorType::SUCCESS)
        {
            GS->logger.LogCustomError(CustomErrorTypes::WARN_GAP_SEC_DISCONNECT_FAILED, (u32)err);
        }
    }
}

void GAPController::GapConnectionSecurityUpdateEventHandler(const FruityHal::GapConnectionSecurityUpdateEvent &connectionSecurityUpdateEvent)
{
    //This event tells us that the security keys are now in use
    u8 keySize = connectionSecurityUpdateEvent.GetKeySize();

    FruityHal::SecurityLevel level = connectionSecurityUpdateEvent.GetSecurityLevel();
    FruityHal::SecurityMode securityMode = connectionSecurityUpdateEvent.GetSecurityMode();

    logt("SEC", "Connection key is now %u bytes, level %u, securityMode %u", keySize, (u8)level, (u8)securityMode);

    ConnectionManager::GetInstance().GapConnectionEncryptedHandler(connectionSecurityUpdateEvent);
}

#if IS_ACTIVE(CONN_PARAM_UPDATE)
void GAPController::GapConnParamUpdateEventHandler(
        const FruityHal::GapConnParamUpdateEvent& connParamUpdateEvent)
{
    const auto rawConnectionHandle = connParamUpdateEvent.GetConnectionHandle();
    auto connection = ConnectionManager::GetInstance()
        .GetConnectionFromHandle(rawConnectionHandle);

    if (connection.Exists())
    {
        FruityHal::BleGapConnParams params = {
            connParamUpdateEvent.GetMinConnectionInterval(),
            connParamUpdateEvent.GetMaxConnectionInterval(),
            connParamUpdateEvent.GetSlaveLatency(),
            connParamUpdateEvent.GetConnectionSupervisionTimeout(),
        };
        connection.GetConnection()->GapConnParamUpdateHandler(params);
    }
#if IS_ACTIVE(CONN_PARAM_UPDATE_LOGGING)
    else
    {
        logt("CONN", "Received GapConnParamUpdateEvent but connection %x does not exist.", rawConnectionHandle);
    }
#endif
}

void GAPController::GapConnParamUpdateRequestEventHandler(const FruityHal::GapConnParamUpdateRequestEvent& connParamUpdateRequestEvent)
{
    const auto rawConnectionHandle =
        connParamUpdateRequestEvent.GetConnectionHandle();
    auto connection = ConnectionManager::GetInstance()
        .GetConnectionFromHandle(rawConnectionHandle);

    if (connection.Exists())
    {
        FruityHal::BleGapConnParams params = {
            connParamUpdateRequestEvent.GetMinConnectionInterval(),
            connParamUpdateRequestEvent.GetMaxConnectionInterval(),
            connParamUpdateRequestEvent.GetSlaveLatency(),
            connParamUpdateRequestEvent.GetConnectionSupervisionTimeout(),
        };
        connection.GetConnection()->GapConnParamUpdateRequestHandler(params);
    }
#if IS_ACTIVE(CONN_PARAM_UPDATE_LOGGING)
    else
    {
        logt("CONN", "Received GapConnParamUpdateRequestEvent but connection %x does not exist.", rawConnectionHandle);
    }
#endif
}
#endif

void GAPController::StartEncryptingConnection(u16 connectionHandle) const
{
    //Key identification data
    //We do not need a key identification currently, because we only have one
    FruityHal::BleGapMasterId keyId;
    CheckedMemset(&keyId, 0x00, sizeof(keyId));
    keyId.encryptionDiversifier = 0;
    CheckedMemset(&keyId.rand, 0x00, sizeof(keyId.rand));

    //Our mesh network key
    FruityHal::BleGapEncInfo key;
    CheckedMemset(&key, 0x00, sizeof(key));
    key.isGeneratedUsingLeSecureConnections  = 0;
    key.isAuthenticatedKey = 1;
    CheckedMemcpy(&key.longTermKey, GS->node.configuration.networkKey, sizeof(key.longTermKey));
    key.longTermKeyLength = 16;

    //This starts the Central Encryption Establishment using stored keys
    //http://infocenter.nordicsemi.com/topic/com.nordic.infocenter.s130.api.v1.0.0/group___b_l_e___g_a_p___c_e_n_t_r_a_l___e_n_c___m_s_c.html
    //http://infocenter.nordicsemi.com/topic/com.nordic.infocenter.s130.api.v1.0.0/group___b_l_e___g_a_p___p_e_r_i_p_h___e_n_c___m_s_c.html
    ErrorType err = FruityHal::BleGapEncrypt(connectionHandle, keyId, key);
    logt("SEC", "encrypting connection handle %u with key. result %u", connectionHandle, (u32)err);
}

ErrorType GAPController::RequestConnectionParameterUpdate(
        u16 connectionHandle, u16 minConnectionInterval,
        u16 maxConnectionInterval, u16 slaveLatency,
        u16 supervisionTimeout) const
{
#if IS_ACTIVE(CONN_PARAM_UPDATE_LOGGING)
    logt(
        "CONN",
        "Started connection parameter update on connection handle=%u "
            "(min=%u, max=%u, sl=%u, st=%u)",
        connectionHandle, minConnectionInterval, maxConnectionInterval,
        slaveLatency, supervisionTimeout
    );
#endif

    FruityHal::BleGapConnParams connParams = {};
    connParams.minConnInterval = minConnectionInterval;
    connParams.maxConnInterval = maxConnectionInterval;
    connParams.slaveLatency = slaveLatency;
    connParams.connSupTimeout = supervisionTimeout;

    // Try to update the connection parameters. If successful an event
    // (BLE_GAP_EVT_CONN_PARAM_UPDATE) will be generated which indicates
    // the result of the update procedure.
    const auto err = FruityHal::BleGapConnectionParamsUpdate(connectionHandle, connParams);
    if (err != ErrorType::SUCCESS)
    {
        logt(
            "CONN",
            "BleGapConnectionParamsUpdate failed (%u): %s",
            static_cast<u32>(err),
            Logger::GetGeneralErrorString(err)
        );
    }

    // The GAPController will handle the events and dispatch them to the
    // respective connection.

    // TODO: Think about using the connection parameters negitation library [1].
    //       The library only works for peripherals, since the central does not
    //       provide a way to negotiate with the peripheral, it just changes
    //       the parameters.
    // [1] http://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.sdk5.v11.0.0%2Fgroup__ble__sdk__lib__conn__params.html

    return err;
}
