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

/*
 * This file implements the usb cdc acm terminal functionality for the nrf52840 chipset.
 * The USBD IRQ Priority must be lower than the SD_EVT IRQ Priority
 *
 *
 *
 * Known issues:
 *   - If many short lines (e.g. 3 characters) are sent to the node within a short time, reading from the usb port
 *     will stop working. This is probably due to a lost update issue with the lineToReadAvailable variable.
 *     If this error happens from time to time, it should be fixed.
 *     Workaround: Closing and opening the port again will restart the communication correctly.
 * */

#include <sdk_config.h>

#if (ACTIVATE_VIRTUAL_COM_PORT == 1)

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>

#if (ACTIVATE_SEGGER_RTT == 1)
#include <SEGGER_RTT.h>
#endif

#include "nrf.h"
#include "nrf_drv_usbd.h"
#include "nrf_drv_clock.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_power.h"
#include "nrf_drv_clock.h"

#include "app_error.h"
#include "app_util.h"
#include "app_usbd_core.h"
#include "app_usbd.h"
#include "app_usbd_string_desc.h"
#include "app_usbd_cdc_acm.h"
#include "app_usbd_serial_num.h"

#include "virtual_com_port.h"

//Forward declarations
extern void SeggerRttPrintf_c(const char* message, ...);
static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const * p_inst, app_usbd_cdc_acm_user_event_t event);

// CDC_ACM class instance
APP_USBD_CDC_ACM_GLOBAL_DEF(m_app_cdc_acm,
                            cdc_acm_user_ev_handler,
                            0, //CDC_ACM_COMM_INTERFACE
                            1, //CDC_ACM_DATA_INTERFACE
							NRF_DRV_USBD_EPIN2, //CDC_ACM_COMM_EPIN
							NRF_DRV_USBD_EPIN1, //CDC_ACM_DATA_EPIN
							NRF_DRV_USBD_EPOUT1, //CDC_ACM_DATA_EPOUT
                            APP_USBD_CDC_COMM_PROTOCOL_AT_V250
);


//We can only read one byte at a time, otherwhise, we will not get the input before a chunk is completed
#define READ_SIZE 1
static char m_rx_buffer[READ_SIZE];

//We store received data in this buffer, once we have a full line, we copy the data and can continue storing data here
static char lineBuffer[VIRTUAL_COM_LINE_BUFFER_SIZE];
static uint16_t lineBufferOffset = 0;

static bool lineToReadAvailable = false;

static void (*portEventHandlerPtr)(bool) = NULL;

static bool virtualComInitialized = false;
static bool virtualComOpened = false;

//Set to true once data is being sent out, we must wait for the completion event
static volatile bool currentlySendingData = false;

uint32_t storeReadByte(uint8_t byte)
{
	lineBuffer[lineBufferOffset] =  byte;
	lineBufferOffset++;

	//If the line is finished, it should be processed before additional data is read
	if(byte == '\r' || lineBufferOffset > VIRTUAL_COM_LINE_BUFFER_SIZE - 1)
	{
		lineBuffer[lineBufferOffset-1] = '\0';
		lineToReadAvailable = true;

		SeggerRttPrintf_c("USB Line available: %s\n", lineBuffer);

		return NRF_ERROR_BUSY;
	}

	return NRF_SUCCESS;
}

/**
 * @brief User event handler @ref app_usbd_cdc_acm_user_ev_handler_t (headphones)
 * */
static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                    app_usbd_cdc_acm_user_event_t event)
{
    app_usbd_cdc_acm_t const * p_cdc_acm = app_usbd_cdc_acm_class_get(p_inst);

    switch (event)
    {
        case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN:
        {
        	//Workaround for a weird bug that occurs when testing with minicom on linux
        	//An echo back of our sent data would be generated and garbage data as well
        	for(int i=0; i<10; i++){
            	app_usbd_event_queue_process();
            	nrf_delay_us(10000);
        	}

        	virtualComOpened = true;
        	if(portEventHandlerPtr) portEventHandlerPtr(true);

            SeggerRttPrintf_c("USB port open\n");

            // Setup first transfer
            ret_code_t ret = app_usbd_cdc_acm_read(&m_app_cdc_acm,
                                                   m_rx_buffer,
                                                   READ_SIZE);

            UNUSED_VARIABLE(ret);
            break;
        }
        case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE:
        	virtualComOpened = false;
        	if(portEventHandlerPtr) portEventHandlerPtr(false);

            SeggerRttPrintf_c("USB port close\n");
            break;
        case APP_USBD_CDC_ACM_USER_EVT_TX_DONE:
        	currentlySendingData = false;
            break;
        case APP_USBD_CDC_ACM_USER_EVT_RX_DONE:
        {
            ret_code_t ret = NRF_SUCCESS;
            do
            {
                // Print received char
                //SeggerRttPrintf_c("char: %u\n", m_rx_buffer[0]);

                ret = storeReadByte(m_rx_buffer[0]);

                if(ret == NRF_SUCCESS)
                {
                	// Fetch data until internal buffer is empty
					ret = app_usbd_cdc_acm_read(&m_app_cdc_acm, m_rx_buffer, READ_SIZE);
                } else {
                	//Make sure that we fetch this full line as soon as possible by setting the IRQ
                	sd_nvic_SetPendingIRQ(SD_EVT_IRQn);

                	//Full line available, reading from USB is not restarted
                	break;
                }

            } while (ret == NRFX_SUCCESS);

            break;
        }
        default:
            break;
    }
}

static void usbd_user_ev_handler(app_usbd_event_type_t event)
{
    switch (event)
    {
        case APP_USBD_EVT_DRV_SUSPEND:
            SeggerRttPrintf_c("USB suspend\n");
        	virtualComInitialized = false;
        	virtualComOpened = false;
            break;
        case APP_USBD_EVT_DRV_RESUME:
            SeggerRttPrintf_c("USB resume\n");
        	virtualComInitialized = true;
            break;
        case APP_USBD_EVT_STOPPED:
            SeggerRttPrintf_c("USB stopped\n");
            app_usbd_disable();
        	virtualComInitialized = false;
        	virtualComOpened = true;
            break;
        case APP_USBD_EVT_STARTED:
            SeggerRttPrintf_c("USB started\n");
        	virtualComInitialized = true;
            break;
        case APP_USBD_EVT_POWER_DETECTED:
            SeggerRttPrintf_c("USB power detected\n");

            if (!nrf_drv_usbd_is_enabled())
            {
                app_usbd_enable();
            }
            break;
        case APP_USBD_EVT_POWER_REMOVED:
            SeggerRttPrintf_c("USB power removed\n");
            app_usbd_stop();
            break;
        case APP_USBD_EVT_POWER_READY:
		SeggerRttPrintf_c("USB ready\n");
            app_usbd_start();
            break;
        default:
            break;
    }
}

uint32_t virtualComInit()
{
    ret_code_t ret;
    static const app_usbd_config_t usbd_config = {
        .ev_state_proc = usbd_user_ev_handler
    };

    app_usbd_serial_num_generate();

    ret = nrf_drv_clock_init();
    APP_ERROR_CHECK(ret);

    ret = app_usbd_init(&usbd_config);
    APP_ERROR_CHECK(ret);

    app_usbd_class_inst_t const * class_cdc_acm = app_usbd_cdc_acm_class_inst_get(&m_app_cdc_acm);
    ret = app_usbd_class_append(class_cdc_acm);
    APP_ERROR_CHECK(ret);

	return NRF_SUCCESS;
}

uint32_t virtualComStart(void (*portEventHandler)(bool))
{
	virtualComOpened = false;

	//Make sure that the clock driver knows that the LWCLK is needed, otherwise it might get stuck
	//while releasing the lw_clock once the softdevice is deinitialized
	//See: https://devzone.nordicsemi.com/f/nordic-q-a/40319/sd_softdevice_disable-not-returning-during-transport-shutdown-in-dfu-bootloader
	nrf_drv_clock_lfclk_request(NULL);

	ret_code_t ret = app_usbd_power_events_enable();
	APP_ERROR_CHECK(ret);

	portEventHandlerPtr = portEventHandler;

	return NRF_SUCCESS;
}

uint32_t virtualComCheck()
{
	//Fetch all queued events
	while (app_usbd_event_queue_process())
	{
		/* Nothing to do */
	}

	return NRF_SUCCESS;
}

//If a line is available, it is copied to the buffer and reading is restarted
uint32_t virtualComCheckAndProcessLine(uint8_t* buffer, uint16_t bufferLength)
{
	if(lineToReadAvailable){

		//The buffer provided must be bigger or equal to the line buffer size
		if(bufferLength < VIRTUAL_COM_LINE_BUFFER_SIZE){
			SeggerRttPrintf_c("Wrong buffer size\n");
			return NRF_ERROR_NO_MEM;
		}

		memcpy(buffer, lineBuffer, lineBufferOffset);

		lineBufferOffset = 0;
		lineToReadAvailable = false;
		SeggerRttPrintf_c("LN false\n");

		//Continue reading data from USB
		uint32_t ret = NRF_SUCCESS;

		do
		{
			//Setup reading again
			ret = app_usbd_cdc_acm_read(&m_app_cdc_acm,
										m_rx_buffer,
										READ_SIZE);

			//If success is returned, we must continue reading because no event will be generated
			if(ret == NRF_SUCCESS){
				//We store the read byte, but we need to break if a full line was read
				uint32_t ret2 = storeReadByte(m_rx_buffer[0]);
				if(ret2 != NRF_SUCCESS) break;
			} else {
				//Full line available
				break;
			}


		} while (ret == NRF_SUCCESS);


		return NRF_SUCCESS;
	} else {
		return NRF_ERROR_BUSY;
	}
}

uint32_t virtualComWriteData(const uint8_t* buffer, uint16_t bufferLength)
{
	if(!virtualComInitialized || !virtualComOpened) return 0;

	uint32_t err;

	//We try to send the data a few times and drop it if we failed multiple times
	for(int i=0; i<100; i++){
		err = app_usbd_cdc_acm_write(&m_app_cdc_acm, buffer, bufferLength);
		if(err == NRF_SUCCESS){
			currentlySendingData = true;
		}

		//We must wait until the data was sent
		for(int j = 0; j<1000; j++){
			if(!currentlySendingData) break;
			nrf_delay_us(1);
		}

		if(err == 0) break;
		SeggerRttPrintf_c(".");
	}

	return err;
}

#endif //IS_ACTIVE(VIRTUAL_COM_PORT)
