////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// ** BlueRange Mesh – Community Edition (CE)
// ** Copyright (c) 2015-2021 MWAY DIGITAL GmbH, Germany
// ** Copyright (c) 2021-2026 BlueRange GmbH, Germany
// **
// ** This file is part of BlueRange Mesh Community Edition (formerly known as
// ** FruityMesh).
// **
// ** BlueRange Mesh Community Edition is free software: you can redistribute it
// ** and/or modify it under the terms of the GNU General Public License as
// ** published by the Free Software Foundation, either version 3 of the
// ** License, or (at your option) any later version.
// **
// ** BlueRange Mesh Community Edition is distributed in the hope that it will
// ** be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// ** See the GNU General Public License for more details.
// **
// ** You should have received a copy of the GNU General Public License along
// ** with this program. If not, see https://www.gnu.org/licenses/.
// **
// ** IMPORTANT:
// ** Any modification, extension, or derivative work of this file MUST also be
// ** licensed under the GNU General Public License v3 or later and the complete
// ** corresponding source code MUST be made available.
// **
// ** Commercial Use:
// ** If you wish to use this software without the obligations of the GPLv3
// ** (including source code disclosure), a commercial license for
// ** BlueRange Mesh OEM Edition is required.
// **
// ** License violations automatically terminate your rights under this license
// ** and may result in legal action under applicable law.
// ** For further information please use the contact form at:
// ** https://bluerange.io/en/contact
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////

/*
 * This file implements the usb cdc acm terminal functionality for the nrf52840 chipset.
 * The USBD IRQ Priority must be lower than the SD_EVT IRQ Priority
 *
 * # Sequences:
 *
 * ## Firmware boots
 *
 * USB power detected
 * USB ready
 * USB started
 * USB suspend
 * USB resume
 *
 * ## USB plugged in
 *
 * USB port open
 *
 * ## USB disconnected from software on PC (while USB still plugged in)
 *
 * USB port close
 *
 * ## USB plugged out (device still powered externally to get logs)
 *
 * USB port open
 * USB suspend
 * USB resume
 * USB power removed
 * USB stopped
 *
 * ## USB plugged in after plugging out
 *
 * USB power detected
 * USB ready
 * USB started
 * USB suspend
 * USB resume
 * USB suspend
 * USB resume
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


// Some helper macros for debugging

#if ACTIVATE_SEGGER_RTT == 1
extern void SeggerRttPrintf_c(const char* message, ...);
#define FRUITYMESH_DETAIL_VCOM_LOG_IMPL(...) SeggerRttPrintf_c(__VA_ARGS__)
#else
#define FRUITYMESH_DETAIL_VCOM_LOG_IMPL(...) do {} while(0)
#endif

#if 1
#define FRUITYMESH_VCOM_LOG_ERROR(...) FRUITYMESH_DETAIL_VCOM_LOG_IMPL(__VA_ARGS__)
#else
#define FRUITYMESH_VCOM_LOG_ERROR(...) do {} while(0)
#endif

#if 0
#define FRUITYMESH_VCOM_LOG_DEBUG(...) FRUITYMESH_DETAIL_VCOM_LOG_IMPL(__VA_ARGS__)
#else
#define FRUITYMESH_VCOM_LOG_DEBUG(...) do {} while(0)
#endif

static bool isRxInProgress = false;
static void onRxDone();

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


//We can only read one byte at a time, otherwise, we will not get the input before a chunk is completed
#define READ_SIZE 1
static char m_rx_buffer[READ_SIZE];

//We store received data in this buffer, once we have a full line, we copy the data and can continue storing data here
static char lineBuffer[VIRTUAL_COM_LINE_BUFFER_SIZE];
static uint16_t lineBufferOffset = 0;

static bool lineToReadAvailable = false;

static void (*portEventHandlerPtr)(bool) = NULL;

/// Whether the underlying nrf usbd driver is initialized
static bool virtualComUsbdInitialized = false;
/// Whether the actual USB port virtual COM port is opened
static bool virtualComOpened = false;
static uint32_t virtualComInitializedCounter = 0;

//Set to true once data is being sent out, we must wait for the completion event
static volatile bool currentlySendingData = false;

/// Set to true if the event irq should be set to pending after the event processing function has returned. Will be
/// reset to false once the irq was set to pending.
static volatile bool setEventIrqPendingAfterProcessUsbEvents = false;

/// Process events from the app_usbd event queue and potentially set the event irq to pending, if required for
/// handling a completed input line.
static bool ProcessAppUsbdEventQueue()
{
    const bool result = app_usbd_event_queue_process();

    // If requested, set the event irq to pending. Depending on the current interrupt priority (if any) this may or
    // may not call the event callback immediately. The interrupt must _not_ be set pending inside of the
    // cdc_acm_user_ev_handler in case the handler runs at a lower interrupt level than the event interrupt (this
    // will make any received data be processed twice).
    if (setEventIrqPendingAfterProcessUsbEvents)
    {
        setEventIrqPendingAfterProcessUsbEvents = false;
        sd_nvic_SetPendingIRQ(SD_EVT_IRQn);
    }

    return result;
}

typedef enum
{
    FRUITYMESH_VCOM_MORE_BYTES_REQUIRED = NRF_SUCCESS,
    FRUITYMESH_VCOM_WHOLE_LINE_BUFFERED = NRF_ERROR_BUSY,
}
ProcessSingleReceivedByteResult;

/// Process a single byte received from the virtual com port. The return value indicates whether a complete line was
/// read (in which case it should be processed as soon as possible) or if more bytes are required.
static ProcessSingleReceivedByteResult ProcessSingleReceivedByte(uint8_t byte)
{
    if (lineToReadAvailable) {
        // Ignore received byte, if the line is complete and has not been processed, yet.
        return FRUITYMESH_VCOM_WHOLE_LINE_BUFFERED;
    }

    // Check if buffer is full to avoid potential buffer overflow.
    if (lineBufferOffset >= VIRTUAL_COM_LINE_BUFFER_SIZE - 1) {
        // Terminate the line to avoid buffer overflow
        lineBuffer[VIRTUAL_COM_LINE_BUFFER_SIZE-1] = '\0';
        lineToReadAvailable = true;
        return FRUITYMESH_VCOM_WHOLE_LINE_BUFFERED;
    }

    lineBuffer[lineBufferOffset] = byte;
    lineBufferOffset++;

    //If the line is finished, it should be processed before additional data is read
    if (byte == '\r' || lineBufferOffset > VIRTUAL_COM_LINE_BUFFER_SIZE - 1)
    {
        lineBuffer[lineBufferOffset-1] = '\0';
        lineToReadAvailable = true;

        FRUITYMESH_VCOM_LOG_DEBUG("USB Line available: %s\n", lineBuffer);

        return FRUITYMESH_VCOM_WHOLE_LINE_BUFFERED;
    }

    return FRUITYMESH_VCOM_MORE_BYTES_REQUIRED;
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

            FRUITYMESH_VCOM_LOG_DEBUG("USB port open\n");

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

            FRUITYMESH_VCOM_LOG_DEBUG("USB port close\n");
            break;
        case APP_USBD_CDC_ACM_USER_EVT_TX_DONE:
            FRUITYMESH_VCOM_LOG_DEBUG("USB TX DONE\n");
            currentlySendingData = false;
            break;
        case APP_USBD_CDC_ACM_USER_EVT_RX_DONE:
        {
            FRUITYMESH_VCOM_LOG_DEBUG("USB RX DONE\n");
            onRxDone();

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
            FRUITYMESH_VCOM_LOG_DEBUG("USB suspend\n");
            virtualComUsbdInitialized = false;
            virtualComOpened = false;
            break;
        case APP_USBD_EVT_DRV_RESUME:
            FRUITYMESH_VCOM_LOG_DEBUG("USB resume\n");
            virtualComUsbdInitialized = true;
            ++virtualComInitializedCounter;
            break;
        case APP_USBD_EVT_DRV_RESET:
            FRUITYMESH_VCOM_LOG_DEBUG("USB reset\n");
            break;
        case APP_USBD_EVT_STOPPED:
            FRUITYMESH_VCOM_LOG_DEBUG("USB stopped\n");
            app_usbd_disable();
            virtualComUsbdInitialized = false;
            virtualComOpened = false;
            break;
        case APP_USBD_EVT_STARTED:
            FRUITYMESH_VCOM_LOG_DEBUG("USB started\n");
            virtualComUsbdInitialized = true;
            ++virtualComInitializedCounter;
            break;
        case APP_USBD_EVT_POWER_DETECTED:
            FRUITYMESH_VCOM_LOG_DEBUG("USB power detected\n");

            if (!nrf_drv_usbd_is_enabled())
            {
                app_usbd_enable();
            }
            break;
        case APP_USBD_EVT_POWER_REMOVED:
            FRUITYMESH_VCOM_LOG_DEBUG("USB power removed\n");
            app_usbd_stop();
            break;
        case APP_USBD_EVT_POWER_READY:
            FRUITYMESH_VCOM_LOG_DEBUG("USB ready\n");
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

uint32_t virtualComEventLoop()
{
    // Process all queued USB events.
    while (ProcessAppUsbdEventQueue())
    {
        /* Nothing to do */
    }

    return NRF_SUCCESS;
}

//If a line is available, it is copied to the buffer and reading is restarted
uint32_t virtualComCheckAndProcessLine(uint8_t* buffer, uint16_t bufferLength)
{
    if (lineToReadAvailable)
    {
        //The buffer provided must be bigger or equal to the line buffer size
        if (bufferLength < VIRTUAL_COM_LINE_BUFFER_SIZE)
        {
            // TODO / BUG: If this ever happens, the virtual com port will never read again, as the read is never
            //             rescheduled. This should cause a reboot with an _appropriate_ `RebootReason` being set.
            //             Tracked in BR-2093.
            FRUITYMESH_VCOM_LOG_ERROR("Wrong buffer size\n");
            return NRF_ERROR_NO_MEM;
        }

        memcpy(buffer, lineBuffer, lineBufferOffset);

        lineBufferOffset = 0;
        lineToReadAvailable = false;
        FRUITYMESH_VCOM_LOG_DEBUG("LN false\n");


        uint32_t ret;

        do
        {
            // Restart the _potentially_ asynchronous read. This will cause execution to break out of the loop
            // if the read could not be fulfilled immediately.
            ret = app_usbd_cdc_acm_read(&m_app_cdc_acm, m_rx_buffer, READ_SIZE);

            if (ret == NRF_SUCCESS)
            {
                const uint32_t processingResult = ProcessSingleReceivedByte(m_rx_buffer[0]);

                if (processingResult == FRUITYMESH_VCOM_WHOLE_LINE_BUFFERED)
                {
                    // If another whole line was read, indicate that another line is available and break out of the
                    // loop. Otherwise we might lose the terminal as no read is ever scheduled again.
                    lineToReadAvailable = true;
                    break;
                }
            }
            else
            {
                // Since we rely on a single read-buffer, we should never get into the situation that two buffers were
                // scheduled already.
                ASSERT(ret == NRF_ERROR_IO_PENDING);

                // The read was scheduled asynchronously and we will be informed via the TX_DONE event of completion.
                break;
            }
        }
        while (ret == NRF_SUCCESS);

        return NRF_SUCCESS;
    }
    else
    {
        return NRF_ERROR_BUSY;
    }
}

uint32_t virtualComWriteData(const uint8_t* buffer, uint16_t bufferLength)
{
    if (!virtualComUsbdInitialized || !virtualComOpened) return 0;

    uint32_t err;

    // Try to write the data to the virtual com port a number of times, drop the write if it does not succeed.
    for (int i = 0; i < 100; ++i)
    {
        // Start the asynchronous write.
        err = app_usbd_cdc_acm_write(&m_app_cdc_acm, buffer, bufferLength);

        // If the write was scheduled wait for it to complete.
        if (err == NRF_SUCCESS)
        {
            currentlySendingData = true;

            // Process all USB events until the pending write was completed. We must process the events here, as the
            // event queue internal to the app_usbd library can 'overflow' and drop events otherwise. If a RX_DONE event
            // is dropped our input processing stops working. See BR-1987 and BR-1580 for more information.
            uint_fast32_t processEventQueueCounter = 0;
            while (currentlySendingData)
            {
                // Drop the write if we are stuck in an 'infinite' loop. On a nRF52840-DK the counter reached at most
                // 550 when sending large messages (578 bytes written at a time).
                if (++processEventQueueCounter == 10000u || !virtualComOpened || !virtualComUsbdInitialized)
                {
                    FRUITYMESH_VCOM_LOG_ERROR("Write dropped due to timeout\n");
                    currentlySendingData = false;
                    break;
                }

                // Ignore if no event has been generated, we need to wait until it has been processed.
                ProcessAppUsbdEventQueue();
            }

            // The write has been completed successfully, break out of the loop.
            break;
        }
    }

    return err;
}

bool isVirtualComPortInitialized() {
    return virtualComUsbdInitialized;
}

uint32_t getVirtualComPortInitializedCounter() {
    return virtualComInitializedCounter;
}

static void onRxDone() {
    bool shouldExit = false; // necessary as critical region will not be exited if we returned directly inside the if below
CRITICAL_REGION_ENTER()
    if (isRxInProgress) {
        // this method may be called from the outside, so we have to ensure that it is not called again by an interrupt handler
        FRUITYMESH_VCOM_LOG_DEBUG("concurrent\n");
        shouldExit = true;
    }
    isRxInProgress = true;
CRITICAL_REGION_EXIT();
    if (shouldExit) {
        return;
    }

    ret_code_t ret;

    do
    {
        // Print received char
        //FRUITYMESH_VCOM_LOG_DEBUG("char: %u\n", m_rx_buffer[0]);

        const uint32_t processingResult = ProcessSingleReceivedByte(m_rx_buffer[0]);

        if (processingResult == FRUITYMESH_VCOM_MORE_BYTES_REQUIRED)
        {
            // Restart the _potentially_ asynchronous read. This will cause execution to break out of the loop
            // if the read could not be fulfilled immediately and allows the event handler to end.
            ret = app_usbd_cdc_acm_read(&m_app_cdc_acm, m_rx_buffer, READ_SIZE);
        }
        else
        {
            // Instruct the event processing function to set the event irq to pending. The event handler _must
            // exit_ before the irq is set to pending.
            setEventIrqPendingAfterProcessUsbEvents = true;

            // Since we have read a full line, we defer rescheduling another read until the line has actually
            // been processed, after the event handler has exited.
            break;
        }
    }
    while (ret == NRF_SUCCESS);

    isRxInProgress = false;
}

uint32_t virtualComProcessReceivedBytesIfAvailable() {
    app_usbd_cdc_acm_ctx_t * p_cdc_acm_ctx = &(&m_app_cdc_acm)->specific.p_data->ctx;
    uint32_t bytesLeft = p_cdc_acm_ctx->bytes_left;
    if (bytesLeft > 0) {
        onRxDone();
    }
    return bytesLeft;
}

#endif //IS_ACTIVE(VIRTUAL_COM_PORT)
