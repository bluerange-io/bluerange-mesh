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

#include <Config.h>
#include <Node.h>
#include <AdvertisingController.h>
#include <ScanController.h>
#include <Logger.h>
#include <GAPController.h>

#include <Utility.h>

extern "C"{
#include <app_error.h>
#include <ble.h>
#include <ble_gap.h>
#include <ble_hci.h>
}




GAPController::GAPController()
{
	gapControllerHandler = nullptr;
}

//This function configures the Generic access Profile
//GAP is a service that must be implemented on all BLE devices
//These are some basic values that other devices can request
//Some of these values might be writable over the air
//The current characteristics are
//		Device name
//		Appearance
//		Peripheral Preferred Connection Parameters
void GAPController::bleConfigureGAP() const{
	u32 err = 0;
	//Set up an open link write permission
	//There are multiple security modes defined in the BLE spec
	//Use these macros: http://developer.nordicsemi.com/nRF51_SDK/doc/7.1.0/s110/html/a00813.html
	ble_gap_conn_sec_mode_t secPermissionClosed;
	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&secPermissionClosed);

	//Set the GAP device name
	err = sd_ble_gap_device_name_set(&secPermissionClosed, (u8*)DEVICE_NAME, (u16)strlen(DEVICE_NAME));
	APP_ERROR_CHECK(err); //OK

	//Set the appearance of the device as defined in http://developer.nordicsemi.com/nRF51_SDK/doc/7.1.0/s110/html/a00837.html
	err = sd_ble_gap_appearance_set(BLE_APPEARANCE_GENERIC_COMPUTER);
	APP_ERROR_CHECK(err); //OK

	//Set gap peripheral preferred connection parameters (not used by the mesh implementation)
	ble_gap_conn_params_t gapConnectionParams;
	memset(&gapConnectionParams, 0, sizeof(gapConnectionParams));
	gapConnectionParams.min_conn_interval = Config->meshMinConnectionInterval;
	gapConnectionParams.max_conn_interval = Config->meshMaxConnectionInterval;
	gapConnectionParams.slave_latency = Config->meshPeripheralSlaveLatency;
	gapConnectionParams.conn_sup_timeout = Config->meshConnectionSupervisionTimeout;
	err = sd_ble_gap_ppcp_set(&gapConnectionParams);
	APP_ERROR_CHECK(err); //OK
}

//Connect to a specific peripheral
u32 GAPController::connectToPeripheral(const fh_ble_gap_addr_t &address, u16 connectionInterval, u16 timeout) const
{
	u32 err = 0;

	fh_ble_gap_scan_params_t scanParams;
	memset(&scanParams, 0x00, sizeof(scanParams));
	fh_ble_gap_conn_params_t connectionParams;
	memset(&connectionParams, 0x00, sizeof(connectionParams));

	scanParams.interval = Config->meshConnectingScanInterval;
	scanParams.window = Config->meshConnectingScanWindow;
	scanParams.timeout = timeout;

	connectionParams.min_conn_interval = connectionInterval;
	connectionParams.max_conn_interval = connectionInterval;
	connectionParams.slave_latency = Config->meshPeripheralSlaveLatency;
	connectionParams.conn_sup_timeout = Config->meshConnectionSupervisionTimeout;

	//Connect to the peripheral
	err = FruityHal::BleGapConnect(&address, &scanParams, &connectionParams);
	if(err != NRF_SUCCESS){
		logt("ERROR", "GATT connect fail %d", err);
		//Just ignore it, the connection will not happen
		return err;
	}

	logt("CONN", "pairing");

	return NRF_SUCCESS;
}


//Disconnect from paired peripheral
void GAPController::disconnectFromPartner(u16 connectionHandle) const
{
	u32 err = 0;

	err = FruityHal::Disconnect(connectionHandle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
	if(err != NRF_SUCCESS){
		//Simply ignore it, we are disconnected
	}
}





bool GAPController::bleConnectionEventHandler(ble_evt_t& bleEvent) const
{
	u32 err = 0;

	//Depending on the type of the BLE event, we have to do different stuff
	//Further Events: http://developer.nordicsemi.com/nRF51_SDK/doc/7.1.0/s120/html/a00746.html#gada486dd3c0cce897b23a887bed284fef
	switch (bleEvent.header.evt_id)
	{
	//************************** GAP *****************************
	//########## Connection with other device
	case BLE_GAP_EVT_CONNECTED:
	{
		logt("C", "Connected device");

		//Connection stops advertising
		if(gapControllerHandler != nullptr){
			gapControllerHandler->GapConnectionConnectedHandler(bleEvent);
		}

		return true;
	}
		//########## Another Device disconnected
	case BLE_GAP_EVT_DISCONNECTED:
	{
		logt("C", "Disconnected device %d", bleEvent.evt.gap_evt.params.disconnected.reason);
		if(gapControllerHandler != nullptr){
			gapControllerHandler->GapConnectionDisconnectedHandler(bleEvent);
		}

		return true;
	}
		break;

	case BLE_GAP_EVT_TIMEOUT:
	{
		if (bleEvent.evt.gap_evt.params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN)
		{
			if(gapControllerHandler != nullptr){
				gapControllerHandler->GapConnectingTimeoutHandler(bleEvent);
			}
		}
		break;
	}
	//The other device requested an encrypted connection
	case BLE_GAP_EVT_SEC_INFO_REQUEST:
	{
		if(Config->encryptionEnabled){
			//With this request, we receive the key identifier and a random number
			//This identification is used to select the correct key

			//TODO: Currently, we skip that process and select our mesh network key
			//If we want multiple keys, we need to implement some more logic to select the key
			ble_gap_evt_sec_info_request_t securityRequest = bleEvent.evt.gap_evt.params.sec_info_request;

			//This is our security key
			ble_gap_enc_info_t key;
			key.lesc = 0;
			key.auth = 1; //This key is authenticated
			memcpy(&key.ltk, GS->node->configuration.networkKey, 16); //Copy our mesh network key
			key.ltk_len = 16;

			//Reply  with our stored key
			sd_ble_gap_sec_info_reply(
				bleEvent.evt.gap_evt.conn_handle,
				&key, //This is our stored long term key
				nullptr, //We do not have an identity resolving key
				nullptr //We do not have signing info
			);

			logt("SEC", "SEC_INFO_REQUEST received, replying with key %02x:%02x:%02x...%02x:%02x. result %d", key.ltk[0], key.ltk[1], key.ltk[2], key.ltk[14], key.ltk[15], err);
		} else {
			logt("SEC", "No Encryption available");
			FruityHal::Disconnect(bleEvent.evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		}


		break;
	}
	//This event tells us that the security keys are now in use
	case BLE_GAP_EVT_CONN_SEC_UPDATE:
	{
		u8 keySize = bleEvent.evt.gap_evt.params.conn_sec_update.conn_sec.encr_key_size;

		u8 level = bleEvent.evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.lv;
		u8 securityMode = bleEvent.evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.sm;

		logt("SEC", "Connection key is now %u bytes, level %u, securityMode %u", keySize, level, securityMode);

		if(gapControllerHandler != nullptr){
			gapControllerHandler->GapConnectionEncryptedHandler(bleEvent);
		}
		break;
	}

#if defined(NRF52)
#ifndef NRF52840
	case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
	{
		ble_gap_phys_t phy;
		phy.rx_phys = BLE_GAP_PHY_1MBPS;
		phy.tx_phys = BLE_GAP_PHY_1MBPS;

		err = sd_ble_gap_phy_update(bleEvent.evt.gap_evt.conn_handle, &phy);

		break;
	}
#endif
	case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
	{
		err = sd_ble_gap_data_length_update(bleEvent.evt.gap_evt.conn_handle, nullptr, nullptr);

		break;
	}
#endif

	default:
		break;
	}
	return false;
}

void GAPController::setGAPControllerHandler(GAPControllerHandler* handler)
{
	gapControllerHandler = handler;
}

void GAPController::startEncryptingConnection(u16 connectionHandle) const
{
	u32 err = 0;

	//Key identification data
	//We do not need a key identification currently, because we only have one
	ble_gap_master_id_t keyId;
	keyId.ediv = 0;
	memset(&keyId.rand, 0x00, BLE_GAP_SEC_RAND_LEN);

	//Our mesh network key
	ble_gap_enc_info_t key;
	key.auth = 1;
	memcpy(&key.ltk, GS->node->configuration.networkKey, BLE_GAP_SEC_KEY_LEN);
	key.ltk_len = 16;


	//This starts the Central Encryption Establishment using stored keys
	//http://infocenter.nordicsemi.com/topic/com.nordic.infocenter.s130.api.v1.0.0/group___b_l_e___g_a_p___c_e_n_t_r_a_l___e_n_c___m_s_c.html
	//http://infocenter.nordicsemi.com/topic/com.nordic.infocenter.s130.api.v1.0.0/group___b_l_e___g_a_p___p_e_r_i_p_h___e_n_c___m_s_c.html
	err = sd_ble_gap_encrypt(connectionHandle, &keyId, &key);

	logt("SEC", "encrypting connection handle %u with key. result %u", connectionHandle, err);
}

void GAPController::RequestConnectionParameterUpdate(u16 connectionHandle, u16 minConnectionInterval, u16 maxConnectionInterval, u16 slaveLatency, u16 supervisionTimeout) const
{
	u32 err = 0;

	ble_gap_conn_params_t connParams;
	connParams.min_conn_interval = minConnectionInterval;
	connParams.max_conn_interval = maxConnectionInterval;
	connParams.slave_latency = slaveLatency;
	connParams.conn_sup_timeout = supervisionTimeout;

	//TODO: Check against compatibility with gap connection parameters limits
	err = sd_ble_gap_conn_param_update(connectionHandle, &connParams);
	if (err != NRF_ERROR_BUSY) {
		APP_ERROR_CHECK(err);
	}

	//TODO: error handling: What if it doesn't work, what if the other side does not agree, etc....

	//TODO: Use connection parameters negitation library: http://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.sdk5.v11.0.0%2Fgroup__ble__sdk__lib__conn__params.html


}
