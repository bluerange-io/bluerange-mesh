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

#include <Config.h>
#include <Node.h>
#include <AdvertisingController.h>
#include <ScanController.h>
#include <Logger.h>
#include <GAPController.h>

extern "C"{
#include <app_error.h>
#include <ble.h>
#include <ble_gap.h>
#include <ble_hci.h>
#include <ble_radio_notification.h>
}




//Initialize
void (*GAPController::connectionSuccessCallback)(ble_evt_t* bleEvent);
void (*GAPController::connectionTimeoutCallback)(ble_evt_t* bleEvent);
void (*GAPController::disconnectionCallback)(ble_evt_t* bleEvent);

bool GAPController::currentlyConnecting = false;

//This function configures the Generic access Profile
//GAP is a service that must be implemented on all BLE devices
//These are some basic values that other devices can request
//Some of these values might be writable over the air
//The current characteristics are
//		Device name
//		Appearance
//		Peripheral Preferred Connection Parameters
void GAPController::bleConfigureGAP(){
	u32 err = 0;
	//Set up an open link write permission
	//There are multiple security modes defined in the BLE spec
	//Use these macros: http://developer.nordicsemi.com/nRF51_SDK/doc/7.1.0/s110/html/a00813.html
	ble_gap_conn_sec_mode_t secPermissionOpen;
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&secPermissionOpen);

	//Set the GAP device name
	err = sd_ble_gap_device_name_set(&secPermissionOpen, (u8*)DEVICE_NAME, strlen(DEVICE_NAME));
	APP_ERROR_CHECK(err);

	//Set the appearance of the device as defined in http://developer.nordicsemi.com/nRF51_SDK/doc/7.1.0/s110/html/a00837.html
	err = sd_ble_gap_appearance_set(BLE_APPEARANCE_GENERIC_COMPUTER);
	APP_ERROR_CHECK(err);

	//Set gap peripheral preferred connection parameters (not used by the mesh implementation)
	ble_gap_conn_params_t gapConnectionParams;
	memset(&gapConnectionParams, 0, sizeof(gapConnectionParams));
	gapConnectionParams.min_conn_interval = Config->meshMinConnectionInterval;
	gapConnectionParams.max_conn_interval = Config->meshMaxConnectionInterval;
	gapConnectionParams.slave_latency = Config->meshPeripheralSlaveLatency;
	gapConnectionParams.conn_sup_timeout = Config->meshConnectionSupervisionTimeout;
	err = sd_ble_gap_ppcp_set(&gapConnectionParams);
	APP_ERROR_CHECK(err);


}


//Connect to a specific peripheral
bool GAPController::connectToPeripheral(ble_gap_addr_t* address)
{
	if(currentlyConnecting) return false;

	currentlyConnecting = true;

	u32 err = 0;

	ble_gap_scan_params_t scan_params;
	ble_gap_conn_params_t conn_params;

	scan_params.selective = 0;
	scan_params.active = 0; /* No active scanning */
	scan_params.interval = Config->meshConnectingScanInterval;
	scan_params.window = Config->meshConnectingScanWindow;
	scan_params.timeout = Config->meshConnectingScanTimeout;

	conn_params.min_conn_interval = Config->meshMinConnectionInterval;
	conn_params.max_conn_interval = Config->meshMaxConnectionInterval;
	conn_params.slave_latency = Config->meshPeripheralSlaveLatency;
	conn_params.conn_sup_timeout = Config->meshConnectionSupervisionTimeout;

	//Connect to the peripheral
	err = sd_ble_gap_connect(address, &scan_params, &conn_params);
	APP_ERROR_CHECK(err);

	//Set scan state off. Connecting uses scan itself and deactivates the scan procedure
	ScanController::scanningState = SCAN_STATE_OFF;

	logt("CONN", "pairing");

	return true;
}


//Disconnect from paired peripheral
void GAPController::disconnectFromPeripheral(u16 connectionHandle)
{
	u32 err = 0;

	err = sd_ble_gap_disconnect(connectionHandle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
	APP_ERROR_CHECK(err);
}





bool GAPController::bleConnectionEventHandler(ble_evt_t* bleEvent)
{
	u32 err = 0;

	//Depending on the type of the BLE event, we have to do different stuff
	//Further Events: http://developer.nordicsemi.com/nRF51_SDK/doc/7.1.0/s120/html/a00746.html#gada486dd3c0cce897b23a887bed284fef
	switch (bleEvent->header.evt_id)
	{
	//************************** GAP *****************************
	//########## Connection with other device
	case BLE_GAP_EVT_CONNECTED:
	{
		currentlyConnecting = false;

		// Our advertisement stopped because we received a connection on this packet (we are peripheral)
		if (bleEvent->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_PERIPH){
			AdvertisingController::AdvertisingInterruptedBecauseOfIncomingConnectionHandler();
		}

		logt("C", "Connected device");

		//Connection stops advertising
		connectionSuccessCallback(bleEvent);

		return true;
	}
		//########## Another Device disconnected
	case BLE_GAP_EVT_DISCONNECTED:
	{
		logt("C", "Disconnected device %d", bleEvent->evt.gap_evt.params.disconnected.reason);
		disconnectionCallback(bleEvent);

		return true;
	}
		break;

	case BLE_GAP_EVT_TIMEOUT:
		if (bleEvent->evt.gap_evt.params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN)
		{
			currentlyConnecting = false;

			connectionTimeoutCallback(bleEvent);
		}
		break;

	default:
		break;
	}
	return false;
}

void GAPController::setConnectionTimeoutHandler(void (*callback)(ble_evt_t* bleEvent))
{
	connectionTimeoutCallback = callback;
}

void GAPController::setConnectionSuccessfulHandler(void (*callback)(ble_evt_t* bleEvent))
{
	connectionSuccessCallback = callback;
}

void GAPController::setDisconnectionHandler(void (*callback)(ble_evt_t* bleEvent))
{
	disconnectionCallback = callback;
}

