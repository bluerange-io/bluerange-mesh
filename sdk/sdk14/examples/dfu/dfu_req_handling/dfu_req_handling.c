/**
 * Copyright (c) 2016 - 2017, Nordic Semiconductor ASA
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 * 
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
#include "dfu_req_handling.h"

#include <stdint.h>
#include <stdbool.h>
#include "nrf_dfu.h"
#include "nrf_dfu_types.h"
#include "nrf_dfu_req_handler.h"
#include "nrf_dfu_handling_error.h"
#include "nrf_dfu_settings.h"
#include "nrf_dfu_transport.h"
#include "nrf_dfu_utils.h"
#include "nrf_dfu_flash.h"
#include "nrf_bootloader_info.h"
#include "nrf_delay.h"
#include "app_util.h"
#include "app_timer.h"
#include "pb.h"
#include "pb_common.h"
#include "pb_decode.h"
#include "dfu-cc.pb.h"
#include "crc32.h"
#ifdef SOFTDEVICE_PRESENT
#include "nrf_sdm.h"
#endif
#include "sdk_macros.h"
#include "nrf_crypto.h"

#define NRF_LOG_MODULE_NAME dfq_req_handling
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
NRF_LOG_MODULE_REGISTER();

STATIC_ASSERT(DFU_SIGNED_COMMAND_SIZE <= INIT_COMMAND_MAX_SIZE);

extern app_timer_id_t const nrf_dfu_post_sd_bl_timeout_timer_id;

/** @brief Macro for the hardware version of the kit used for requirement-match
 *
 * @note If not set, this will default to 51 or 52 according to the architecture
 */
#if defined (NRF51) && !defined(NRF_DFU_HW_VERSION)
    #define NRF_DFU_HW_VERSION (51)
#elif defined(NRF52_SERIES)
    #define NRF_DFU_HW_VERSION (52)
#else
    #error No target set for HW version.
#endif

/** @brief Cyclic buffers for storing data that is to be written to flash.
 *         This is because the RAM copy must be kept alive until copying is
 *         done and the DFU process must be able to progress while waiting for flash.
 */
#define FLASH_BUFFER_SWAP()                                                                         \
            do                                                                                      \
            {                                                                                       \
                m_current_data_buffer = (m_current_data_buffer + 1) & 0x03;                         \
                m_data_buf_pos        = 0;                                                          \
            } while (0)


__ALIGN(4) static uint8_t  m_data_buf[FLASH_BUFFER_COUNT][FLASH_BUFFER_LENGTH];

static uint16_t m_data_buf_pos;                 /**< The number of bytes written in the current buffer. */
static uint8_t  m_current_data_buffer;          /**< Index of the current data buffer. Must be between 0 and FLASH_BUFFER_COUNT - 1. */

static uint32_t m_firmware_start_addr;          /**< Start address of the current firmware image. */
static uint32_t m_firmware_size_req;            /**< The size of the entire firmware image. Defined by the init command. */

static bool m_valid_init_packet_present;        /**< Global variable holding the current flags indicating the state of the DFU process. */
static bool m_is_dfu_complete_response_sent;
static bool m_dfu_last_settings_write_started;

static dfu_packet_t packet = DFU_PACKET_INIT_DEFAULT;
static pb_istream_t stream;

APP_TIMER_DEF(m_reset_timer);                   /**< A timer to reset the device when DFU is completed. */

static nrf_value_length_t init_packet_data = {0};

extern app_timer_id_t const nrf_dfu_inactivity_timeout_timer_id;

nrf_crypto_hash_info_t const hash_info_sha256 =
{
    .hash_type   = NRF_CRYPTO_HASH_TYPE_SHA256,
    .endian_type = NRF_CRYPTO_ENDIAN_LE
};

nrf_crypto_signature_info_t const sig_info_p256 =
{
    .curve_type  = NRF_CRYPTO_CURVE_SECP256R1,
    .hash_type   = NRF_CRYPTO_HASH_TYPE_SHA256,
    .endian_type = NRF_CRYPTO_ENDIAN_LE
};

/** @brief Value length structure holding the public key.
 *
 * @details The pk value pointed to is the public key present in dfu_public_key.c
 */
NRF_CRYPTO_ECC_PUBLIC_KEY_RAW_CREATE_FROM_ARRAY(crypto_key_pk, SECP256R1, pk);

/** @brief Value length structure to hold a signature
 */
NRF_CRYPTO_ECDSA_SIGNATURE_CREATE(crypto_sig, SECP256R1);

/** @brief Value length structure to hold the hash for the init packet
 */
NRF_CRYPTO_HASH_CREATE(init_packet_hash, SHA256);

/** @brief Value length structure to hold the hash for the firmware image
 */
NRF_CRYPTO_HASH_CREATE(fw_hash, SHA256);


static void reset_device(void * p_context)
{
    NRF_LOG_DEBUG("Attempting to reset the device.");
    if (!m_is_dfu_complete_response_sent)
    {
        NRF_LOG_DEBUG("Waiting until the response is sent.");
    }
    else
    {
#ifndef NRF_DFU_NO_TRANSPORT
        (void)nrf_dfu_transports_close();
#endif
#ifdef NRF_DFU_DEBUG_VERSION
        NRF_LOG_DEBUG("Reset.");
        NRF_LOG_FLUSH();
        nrf_delay_ms(100);
#endif
        NVIC_SystemReset();
    }
}


static void on_dfu_complete(nrf_fstorage_evt_t * p_evt)
{
    static bool timer_started;
    NRF_LOG_DEBUG("All flash operations have completed.");

    // Start a timer to reset the device.
    // Wait for the response to be sent to the peer.
    if (!timer_started)
    {
        ret_code_t err_code;
        NRF_LOG_DEBUG("Starting reset timer.");
        err_code = app_timer_create(&m_reset_timer, APP_TIMER_MODE_REPEATED, reset_device);
        APP_ERROR_CHECK(err_code);
        err_code = app_timer_start(m_reset_timer, APP_TIMER_TICKS(50), NULL);
        APP_ERROR_CHECK(err_code);
        timer_started = true;
    }
}


void nrf_dfu_req_handler_reset_if_dfu_complete(void)
{
    if (m_dfu_last_settings_write_started)
    {
        // Set a flag and wait for the on_dfu_complete callback.
        NRF_LOG_DEBUG("Response sent.");
        m_is_dfu_complete_response_sent = true;
    }
}

static void inactivity_timer_restart(void)
{
    APP_ERROR_CHECK(app_timer_stop(nrf_dfu_inactivity_timeout_timer_id));
    APP_ERROR_CHECK(app_timer_start(nrf_dfu_inactivity_timeout_timer_id,
                                    APP_TIMER_TICKS(NRF_DFU_INACTIVITY_TIMEOUT_MS),
                                    NULL));
}


static void pb_decoding_callback(pb_istream_t *str, uint32_t tag, pb_wire_type_t wire_type, void *iter)
{
    pb_field_iter_t* p_iter = (pb_field_iter_t *) iter;

    // match the beginning of the init command
    if (p_iter->pos->ptr == &dfu_init_command_fields[0])
    {
        uint8_t *ptr = (uint8_t *) str->state;
        uint32_t size = str->bytes_left;

        // remove tag byte
        ptr++;
        size--;

        // store the info in init_packet_data
        init_packet_data.p_value = ptr;
        init_packet_data.length = size;

        NRF_LOG_INFO("PB: Init packet data len: %d", size);
    }
}


static nrf_dfu_res_code_t dfu_handle_prevalidate(dfu_signed_command_t const * p_command, pb_istream_t * p_stream, uint8_t * p_init_cmd, uint32_t init_cmd_len)
{
    dfu_init_command_t const *  p_init = &p_command->command.init;
    uint32_t                    err_code;
    uint32_t                    hw_version = NRF_DFU_HW_VERSION;
    uint32_t                    fw_version = 0;

    // check for init command found during decoding
    if (!p_init_cmd || !init_cmd_len)
    {
        return ext_error_set(NRF_DFU_EXT_ERROR_INIT_COMMAND_INVALID);
    }

#ifndef NRF_DFU_DEBUG_VERSION
    if (p_init->has_is_debug && p_init->is_debug == true)
    {
        // Prevalidation should fail if debug is set in an init package for a non-debug Secure DFU bootloader.
        return NRF_DFU_RES_CODE_OPERATION_FAILED;
    }
#endif

#ifdef NRF_DFU_DEBUG_VERSION
    if (p_init->has_is_debug == false || p_init->is_debug == false)
    {
#endif
        if (p_init->has_hw_version == false)
        {
            NRF_LOG_ERROR("No HW version");
            return ext_error_set(NRF_DFU_EXT_ERROR_INIT_COMMAND_INVALID);
        }

        // Check of init command HW version
        if (p_init->hw_version != hw_version)
        {
            NRF_LOG_ERROR("Faulty HW version");
            return ext_error_set(NRF_DFU_EXT_ERROR_HW_VERSION_FAILURE);
        }

#ifdef SOFTDEVICE_PRESENT
        // Precheck the SoftDevice version
        bool found_sd_ver = false;
        for (int i = 0; i < p_init->sd_req_count; i++)
        {
            if (p_init->sd_req[i] == SD_FWID_GET(MBR_SIZE))
            {
                found_sd_ver = true;
                break;
            }
        }
        if (!found_sd_ver)
        {
            NRF_LOG_ERROR("SD req not met");
            return ext_error_set(NRF_DFU_EXT_ERROR_SD_VERSION_FAILURE);
        }
#endif

        // Get the fw version
        switch (p_init->type)
        {
            case DFU_FW_TYPE_APPLICATION:
                if (p_init->has_fw_version == false)
                {
                    return ext_error_set(NRF_DFU_EXT_ERROR_INIT_COMMAND_INVALID);
                }
                // Get the application FW version
                fw_version = s_dfu_settings.app_version;
                break;

            case DFU_FW_TYPE_SOFTDEVICE:
                // not loaded
                break;

            case DFU_FW_TYPE_BOOTLOADER: // fall through
            case DFU_FW_TYPE_SOFTDEVICE_BOOTLOADER:
                if (p_init->has_fw_version == false)
                {
                    return ext_error_set(NRF_DFU_EXT_ERROR_INIT_COMMAND_INVALID);
                }
                fw_version = s_dfu_settings.bootloader_version;
                break;

            default:
                NRF_LOG_INFO("Unknown FW update type");
                return ext_error_set(NRF_DFU_EXT_ERROR_INIT_COMMAND_INVALID);
        }

        NRF_LOG_INFO("Req version: %d, Expected: %d", p_init->fw_version, fw_version);

        // Check of init command FW version
        switch (p_init->type)
        {
            case DFU_FW_TYPE_APPLICATION:
                if (p_init->fw_version < fw_version)
                {
                    NRF_LOG_ERROR("FW version too low");
                    return ext_error_set(NRF_DFU_EXT_ERROR_FW_VERSION_FAILURE);
                }
                break;

            case DFU_FW_TYPE_BOOTLOADER:            // fall through
            case DFU_FW_TYPE_SOFTDEVICE_BOOTLOADER:
                // updating the bootloader is stricter. There must be an increase in version number
                if (p_init->fw_version <= fw_version)
                {
                    NRF_LOG_ERROR("BL FW version too low");
                    return ext_error_set(NRF_DFU_EXT_ERROR_FW_VERSION_FAILURE);
                }
                break;

            default:
                // do not care about fw_version in the case of a softdevice transfer
                break;
        }

#ifdef NRF_DFU_DEBUG_VERSION
    }
#endif

    // Check the signature
    switch (p_command->signature_type)
    {
        case DFU_SIGNATURE_TYPE_ECDSA_P256_SHA256:
            {
                NRF_LOG_INFO("Calculating init packet hash");
                err_code = nrf_crypto_hash_compute(hash_info_sha256, p_init_cmd, init_cmd_len, &init_packet_hash);
                if (err_code != NRF_SUCCESS)
                {
                    return NRF_DFU_RES_CODE_OPERATION_FAILED;
                }

                if (crypto_sig.length != p_command->signature.size)
                {
                    return NRF_DFU_RES_CODE_OPERATION_FAILED;
                }

                // Prepare the signature received over the air.
                memcpy(crypto_sig.p_value, p_command->signature.bytes, p_command->signature.size);

                // calculate the signature
                NRF_LOG_INFO("Verify signature");
                err_code = nrf_crypto_ecdsa_verify_hash(sig_info_p256, &crypto_key_pk, &init_packet_hash, &crypto_sig);
                if (err_code != NRF_SUCCESS)
                {
                    NRF_LOG_ERROR("Signature failed");
                    return NRF_DFU_RES_CODE_INVALID_OBJECT;
                }

                NRF_LOG_INFO("Image verified");
            }
            break;

        default:
            NRF_LOG_INFO("Invalid signature type");
            return ext_error_set(NRF_DFU_EXT_ERROR_WRONG_SIGNATURE_TYPE);
    }

    // Get the update size
    m_firmware_size_req = 0;

    switch (p_init->type)
    {
        case DFU_FW_TYPE_APPLICATION:
            if (p_init->has_app_size == false)
            {
                NRF_LOG_ERROR("No app image size");
                return ext_error_set(NRF_DFU_EXT_ERROR_INIT_COMMAND_INVALID);
            }
            m_firmware_size_req += p_init->app_size;
            break;

        case DFU_FW_TYPE_BOOTLOADER:
            if (p_init->has_bl_size == false)
            {
                NRF_LOG_ERROR("No bl image size");
                return ext_error_set(NRF_DFU_EXT_ERROR_INIT_COMMAND_INVALID);
            }
            m_firmware_size_req += p_init->bl_size;
            // check that the size of the bootloader is not larger than the present one.
#if defined ( NRF51 )
            if (p_init->bl_size > BOOTLOADER_SETTINGS_ADDRESS - BOOTLOADER_START_ADDR)
#elif defined( NRF52_SERIES )
            if (p_init->bl_size > NRF_MBR_PARAMS_PAGE_ADDRESS - BOOTLOADER_START_ADDR)
#endif
            {
                NRF_LOG_ERROR("BL too large");
                return NRF_DFU_RES_CODE_INSUFFICIENT_RESOURCES;
            }
            break;

        case DFU_FW_TYPE_SOFTDEVICE:
            if (p_init->has_sd_size == false)
            {
                NRF_LOG_ERROR("No SD image size");
                return ext_error_set(NRF_DFU_EXT_ERROR_INIT_COMMAND_INVALID);
            }
            m_firmware_size_req += p_init->sd_size;
            break;

        case DFU_FW_TYPE_SOFTDEVICE_BOOTLOADER:
            if (p_init->has_bl_size == false || p_init->has_sd_size == false)
            {
                NRF_LOG_ERROR("NO BL/SD size");
                return ext_error_set(NRF_DFU_EXT_ERROR_INIT_COMMAND_INVALID);
            }
            m_firmware_size_req += p_init->sd_size + p_init->bl_size;
            if (p_init->sd_size == 0 || p_init->bl_size == 0)
            {
                NRF_LOG_ERROR("BL+SD size 0");
                return NRF_DFU_RES_CODE_INVALID_PARAMETER;
            }

            // check that the size of the bootloader is not larger than the present one.
#if defined ( NRF51 )
            if (p_init->bl_size > BOOTLOADER_SETTINGS_ADDRESS - BOOTLOADER_START_ADDR)
#elif defined ( NRF52_SERIES )
            if (p_init->bl_size > NRF_MBR_PARAMS_PAGE_ADDRESS - BOOTLOADER_START_ADDR)
#endif
            {
                NRF_LOG_ERROR("BL too large (SD+BL)");
                return NRF_DFU_RES_CODE_INSUFFICIENT_RESOURCES;
            }
            break;

        default:
            NRF_LOG_ERROR("Unknown FW update type");
            return ext_error_set(NRF_DFU_EXT_ERROR_INIT_COMMAND_INVALID);
    }

    NRF_LOG_INFO("Running hash check");
    // SHA256 is the only supported hash
    memcpy(fw_hash.p_value, &p_init->hash.hash.bytes[0], NRF_CRYPTO_HASH_SIZE_SHA256);

    // Instead of checking each type with has-check, check the result of the size_req to
    // Validate its content.
    if (m_firmware_size_req == 0)
    {
        NRF_LOG_ERROR("No FW size");
        return NRF_DFU_RES_CODE_INVALID_PARAMETER;
    }

    // Find the location to place the DFU updates
    err_code = nrf_dfu_find_cache(m_firmware_size_req, false, &m_firmware_start_addr);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("Can't find room for update");
        return NRF_DFU_RES_CODE_INSUFFICIENT_RESOURCES;
    }

    NRF_LOG_INFO("Write address set to 0x%08x", m_firmware_start_addr);

    NRF_LOG_INFO("DFU prevalidate SUCCESSFUL!");

    return NRF_DFU_RES_CODE_SUCCESS;
}


/** @brief Function for validating the received image after all objects have been received and executed.
 *
 * @param[in]   p_init Pointer to
 *
 * @retval NRF_DFU_RES_CODE_INVALID_OBJECT Object invalid, update rejected.
 */
static nrf_dfu_res_code_t nrf_dfu_postvalidate(dfu_init_command_t * p_init)
{
    uint32_t                   err_code;
    nrf_dfu_res_code_t         res_code = NRF_DFU_RES_CODE_SUCCESS;
    nrf_dfu_bank_t           * p_bank;
#ifdef SOFTDEVICE_PRESENT
    uint32_t                   current_SD_major;
    uint32_t                   new_SD_major;
#endif

    switch (p_init->hash.hash_type)
    {
        case DFU_HASH_TYPE_SHA256:
            err_code = nrf_crypto_hash_compute(hash_info_sha256, (uint8_t*)m_firmware_start_addr, m_firmware_size_req, &fw_hash);
            if (err_code != NRF_SUCCESS)
            {
                res_code = NRF_DFU_RES_CODE_OPERATION_FAILED;
            }

            if (memcmp(fw_hash.p_value, p_init->hash.hash.bytes, NRF_CRYPTO_HASH_SIZE_SHA256) != 0)
            {
                NRF_LOG_ERROR("Hash failure");

                res_code = ext_error_set(NRF_DFU_EXT_ERROR_VERIFICATION_FAILED);
            }
            break;

        default:
            res_code = ext_error_set(NRF_DFU_EXT_ERROR_WRONG_HASH_TYPE);
            break;
    }

    if (s_dfu_settings.bank_current == NRF_DFU_CURRENT_BANK_0)
    {
        NRF_LOG_INFO("Current bank is bank 0");
        p_bank = &s_dfu_settings.bank_0;
    }
    else if (s_dfu_settings.bank_current == NRF_DFU_CURRENT_BANK_1)
    {
        NRF_LOG_INFO("Current bank is bank 1");
        p_bank = &s_dfu_settings.bank_1;
    }
    else
    {
        NRF_LOG_ERROR("Internal error, invalid current bank");
        return NRF_DFU_RES_CODE_OPERATION_FAILED;
    }

#ifdef SOFTDEVICE_PRESENT
    NRF_LOG_INFO("Running SD version check ============== ");
    // Verify the API version of potential new SoftDevices to prevent incompatibility.
    switch (p_init->type)
    {
        case DFU_FW_TYPE_APPLICATION:
            break;
        case DFU_FW_TYPE_SOFTDEVICE:
            // A new SD API in a SD update is not valid.
            current_SD_major = SD_VERSION_GET(MBR_SIZE)/1000000;
            new_SD_major = SD_VERSION_GET(m_firmware_start_addr)/1000000;
            if (current_SD_major != new_SD_major)
            {
                NRF_LOG_WARNING("SD major version numbers mismatch. Update invalid."
                                "Current: %d. New: %d.", current_SD_major, new_SD_major);
                res_code = NRF_DFU_RES_CODE_INVALID_OBJECT;
            }
            break;
        case DFU_FW_TYPE_BOOTLOADER:
            break;
        case DFU_FW_TYPE_SOFTDEVICE_BOOTLOADER:
            // A new SD API in a SD+BL update invalidates the existing application.
            current_SD_major = SD_VERSION_GET(MBR_SIZE)/1000000;
            new_SD_major = SD_VERSION_GET(m_firmware_start_addr)/1000000;
            if (current_SD_major != new_SD_major)
            {
                NRF_LOG_WARNING("SD major version numbers mismatch. Current app will be invalidated."
                                "Current: %d. New: %d.", current_SD_major, new_SD_major);
                if (s_dfu_settings.bank_current == NRF_DFU_CURRENT_BANK_1)
                {
                    s_dfu_settings.bank_0.bank_code = NRF_DFU_BANK_INVALID;
                }
            }
            break;
        default:
            res_code = NRF_DFU_RES_CODE_OPERATION_FAILED;
            break;
    }
#endif

    // Finalize postvalidation by updating DFU settings.
    if (res_code == NRF_DFU_RES_CODE_SUCCESS)
    {
        NRF_LOG_INFO("Successfully ran the postvalidation check!");

        switch (p_init->type)
        {
            case DFU_FW_TYPE_APPLICATION:
                p_bank->bank_code = NRF_DFU_BANK_VALID_APP;
                // If this update is in bank 1, invalidate bank 0 before starting copy routine.
                if (s_dfu_settings.bank_current == NRF_DFU_CURRENT_BANK_1)
                {
                    NRF_LOG_INFO("Invalidating old application in bank 0.");
                    s_dfu_settings.bank_0.bank_code = NRF_DFU_BANK_INVALID;
                }
                break;
            case DFU_FW_TYPE_SOFTDEVICE:
                p_bank->bank_code = NRF_DFU_BANK_VALID_SD;
                s_dfu_settings.sd_size = p_init->sd_size;
                break;
            case DFU_FW_TYPE_BOOTLOADER:
                p_bank->bank_code = NRF_DFU_BANK_VALID_BL;
                break;
            case DFU_FW_TYPE_SOFTDEVICE_BOOTLOADER:
                p_bank->bank_code = NRF_DFU_BANK_VALID_SD_BL;
                s_dfu_settings.sd_size = p_init->sd_size;
                break;
            default:
                res_code = ext_error_set(NRF_DFU_EXT_ERROR_INIT_COMMAND_INVALID);
                break;
        }

#ifdef NRF_DFU_DEBUG_VERSION
        if (p_init->has_is_debug == false || p_init->is_debug == false)
        {
#endif
            switch (p_init->type)
            {
                case DFU_FW_TYPE_APPLICATION:
                    s_dfu_settings.app_version = p_init->fw_version;
                    break;
                case DFU_FW_TYPE_BOOTLOADER:
                case DFU_FW_TYPE_SOFTDEVICE_BOOTLOADER:
                    s_dfu_settings.bootloader_version = p_init->fw_version;
                    break;
                default:
                    // no implementation
                    break;
            }
#ifdef NRF_DFU_DEBUG_VERSION
        }
#endif
        // Calculate CRC32 for image
        p_bank->image_crc = s_dfu_settings.progress.firmware_image_crc;
        p_bank->image_size = m_firmware_size_req;
    }
    else
    {
        p_bank->bank_code = NRF_DFU_BANK_INVALID;

        // Calculate CRC32 for image
        p_bank->image_crc = 0;
        p_bank->image_size = 0;
    }

    // Set the progress to zero and remove the last command
    memset(&s_dfu_settings.progress, 0, sizeof(dfu_progress_t));
    memset(s_dfu_settings.init_command, 0xFF, DFU_SIGNED_COMMAND_SIZE);
    s_dfu_settings.write_offset = 0;

    // Store the settings to flash and reset after that
    m_dfu_last_settings_write_started = true;
    if (nrf_dfu_settings_write(on_dfu_complete) != NRF_SUCCESS)
    {
        res_code = NRF_DFU_RES_CODE_OPERATION_FAILED;
    }

    return res_code;
}


/** @brief Function to handle signed command
 *
 * @param[in]   p_command   Signed
 */
static nrf_dfu_res_code_t dfu_handle_signed_command(dfu_signed_command_t const * p_command, pb_istream_t * p_stream)
{
    nrf_dfu_res_code_t ret_val = NRF_DFU_RES_CODE_SUCCESS;

    // Currently only init-packet is signed
    if (p_command->command.has_init != true)
    {
        return NRF_DFU_RES_CODE_INVALID_OBJECT;
    }

    ret_val = dfu_handle_prevalidate(p_command, p_stream, init_packet_data.p_value, init_packet_data.length);
    if (ret_val == NRF_DFU_RES_CODE_SUCCESS)
    {
        NRF_LOG_INFO("Prevalidate OK.");

        // This saves the init command to flash
        NRF_LOG_INFO("Saving init command...");
        if (nrf_dfu_settings_write(NULL) != NRF_SUCCESS)
        {
            return NRF_DFU_RES_CODE_OPERATION_FAILED;
        }
    }
    else
    {
        NRF_LOG_ERROR("Prevalidate failed!");
    }
    return ret_val;
}


static nrf_dfu_res_code_t dfu_handle_command(dfu_command_t const * p_command)
{
    return ext_error_set(NRF_DFU_EXT_ERROR_UNKNOWN_COMMAND);
}


static uint32_t dfu_decode_commmand(void)
{
    stream = pb_istream_from_buffer(s_dfu_settings.init_command, s_dfu_settings.progress.command_size);

    // Attach our callback to follow the field decoding
    stream.decoding_callback = pb_decoding_callback;
    // reset the variable where the init pointer and length will be stored.
    init_packet_data.p_value = NULL;
    init_packet_data.length = 0;

    if (!pb_decode(&stream, dfu_packet_fields, &packet))
    {
        NRF_LOG_ERROR("Handler: Invalid protocol buffer stream");
        return 0;
    }

    return 1;
}


/** @brief Function handling command requests from the transport layer.
 *
 * @param   p_context[in,out]   Pointer to structure holding context-specific data
 * @param   p_req[in]           Pointer to the structure holding the DFU request.
 * @param   p_res[out]          Pointer to the structure holding the DFU response.
 *
 * @retval NRF_SUCCESS     If the command request was executed successfully.
 *                         Any other error code indicates that the data request
 *                         could not be handled.
 */
static nrf_dfu_res_code_t nrf_dfu_command_req(void * p_context, nrf_dfu_req_t * p_req, nrf_dfu_res_t * p_res)
{
    nrf_dfu_res_code_t ret_val = NRF_DFU_RES_CODE_SUCCESS;
    bool restart_inactivity_timer = false;

    switch (p_req->req_type)
    {
        case NRF_DFU_OBJECT_OP_CREATE:
            NRF_LOG_INFO("Before OP create command");

            restart_inactivity_timer = true;

            if (p_req->object_size == 0)
            {
                return NRF_DFU_RES_CODE_INVALID_PARAMETER;
            }

            if (p_req->object_size > INIT_COMMAND_MAX_SIZE)
            {
                // It is impossible to handle the command because the size is too large
                return NRF_DFU_RES_CODE_INSUFFICIENT_RESOURCES;
            }

            NRF_LOG_INFO("Valid Command Create");

            // Stop post SD/BL timeout timer.
            (void)app_timer_stop(nrf_dfu_post_sd_bl_timeout_timer_id);

            // Setting DFU to uninitialized.
            m_valid_init_packet_present = false;

            // Reset all progress to zero.
            memset(&s_dfu_settings.progress, 0, sizeof(dfu_progress_t));
            s_dfu_settings.write_offset = 0;

            // Set the init command size.
            s_dfu_settings.progress.command_size = p_req->object_size;
            break;

        case NRF_DFU_OBJECT_OP_CRC:
            NRF_LOG_INFO("Valid Command CRC");
            p_res->offset = s_dfu_settings.progress.command_offset;
            p_res->crc = s_dfu_settings.progress.command_crc;
            break;

        case NRF_DFU_OBJECT_OP_WRITE:
            NRF_LOG_INFO("Before OP write command");

            if ((p_req->req_len + s_dfu_settings.progress.command_offset) > s_dfu_settings.progress.command_size)
            {
                // Too large for the command that was requested
                p_res->offset = s_dfu_settings.progress.command_offset;
                p_res->crc = s_dfu_settings.progress.command_crc;
                NRF_LOG_ERROR("Error. Init command larger than expected. ");
                return NRF_DFU_RES_CODE_INVALID_PARAMETER;
            }

            // Copy the received data to RAM, updating offset and calculating CRC.
            memcpy(&s_dfu_settings.init_command[s_dfu_settings.progress.command_offset], p_req->p_req, p_req->req_len);
            s_dfu_settings.progress.command_offset += p_req->req_len;
            s_dfu_settings.progress.command_crc = crc32_compute(p_req->p_req, p_req->req_len, &s_dfu_settings.progress.command_crc);

            // Set output values.
            p_res->offset = s_dfu_settings.progress.command_offset;
            p_res->crc = s_dfu_settings.progress.command_crc;
            break;

        case NRF_DFU_OBJECT_OP_EXECUTE:
            NRF_LOG_INFO("Before OP execute command");
            restart_inactivity_timer = true;
            if (s_dfu_settings.progress.command_offset != s_dfu_settings.progress.command_size)
            {
                // The object wasn't the right (requested) size
                NRF_LOG_ERROR("Execute with faulty offset");
                return NRF_DFU_RES_CODE_OPERATION_NOT_PERMITTED;
            }

            NRF_LOG_INFO("Valid command execute");

            if (m_valid_init_packet_present)
            {
                // Init command already executed
                return NRF_DFU_RES_CODE_SUCCESS;
            }

            if (dfu_decode_commmand() != true)
            {
                return NRF_DFU_RES_CODE_INVALID_OBJECT;
            }

            // We have a valid DFU packet
            if (packet.has_signed_command)
            {
                NRF_LOG_INFO("Handling signed command");
                ret_val = dfu_handle_signed_command(&packet.signed_command, &stream);
            }
            else if (packet.has_command)
            {
                NRF_LOG_INFO("Handling unsigned command");
                ret_val = dfu_handle_command(&packet.command);
            }
            else
            {
                // We had no regular or signed command.
                NRF_LOG_ERROR("Decoded command but it has no content!!");
                return NRF_DFU_RES_CODE_INVALID_OBJECT;
            }

            if (ret_val == NRF_DFU_RES_CODE_SUCCESS)
            {
                // Setting DFU to initialized
                NRF_LOG_INFO("Setting DFU flag to initialized");
                m_valid_init_packet_present = true;
            }
            break;

        case NRF_DFU_OBJECT_OP_SELECT:
            NRF_LOG_INFO("Valid Command: NRF_DFU_OBJECT_OP_SELECT");
            p_res->offset   = s_dfu_settings.progress.command_offset;
            p_res->crc      = s_dfu_settings.progress.command_crc;
            p_res->max_size = INIT_COMMAND_MAX_SIZE;
            break;

        default:
            NRF_LOG_ERROR("Invalid Command Operation");
            ret_val = NRF_DFU_RES_CODE_OP_CODE_NOT_SUPPORTED;
            break;
    }

    if (restart_inactivity_timer)
    {
        inactivity_timer_restart();
    }

    return ret_val;
}


static nrf_dfu_res_code_t nrf_dfu_data_req(void * p_context, nrf_dfu_req_t * p_req, nrf_dfu_res_t * p_res)
{
    uint32_t            write_addr;
    nrf_dfu_res_code_t  ret_val = NRF_DFU_RES_CODE_SUCCESS;
    bool                restart_inactivity_timer = false;

    ASSERT(p_req != NULL);

    NRF_WDT->RR[0] = WDT_RR_RR_Reload;

    switch (p_req->req_type)
    {
        case NRF_DFU_OBJECT_OP_CREATE:
            NRF_LOG_INFO("Before OP create");
            restart_inactivity_timer = true;

            if (p_req->object_size == 0)
            {
                // Empty object is not possible
                //NRF_LOG_INFO("Trying to create data object of size 0");
                return NRF_DFU_RES_CODE_INVALID_PARAMETER;
            }

            if ( (p_req->object_size & (CODE_PAGE_SIZE - 1)) != 0 &&
                (s_dfu_settings.progress.firmware_image_offset_last + p_req->object_size != m_firmware_size_req) )
            {
                NRF_LOG_ERROR("Trying to create an object with a size that is not page aligned");
                return NRF_DFU_RES_CODE_INVALID_PARAMETER;
            }

            if (p_req->object_size > DATA_OBJECT_MAX_SIZE)
            {
                // It is impossible to handle the command because the size is too large
                NRF_LOG_ERROR("Invalid size for object (too large)");
                return NRF_DFU_RES_CODE_INSUFFICIENT_RESOURCES;
            }

            if (m_valid_init_packet_present == false)
            {
                // Can't accept data because DFU isn't initialized by init command.
                NRF_LOG_ERROR("Trying to create data object without valid init command");
                return NRF_DFU_RES_CODE_OPERATION_NOT_PERMITTED;
            }

            if ((s_dfu_settings.progress.firmware_image_offset_last + p_req->object_size) > m_firmware_size_req)
            {
                NRF_LOG_ERROR("Trying to create an object of size %d, when offset is 0x%08x and firmware size is 0x%08x",
                              p_req->object_size, s_dfu_settings.progress.firmware_image_offset_last, m_firmware_size_req);
                return NRF_DFU_RES_CODE_OPERATION_NOT_PERMITTED;
            }

            NRF_LOG_INFO("Valid Data Create");


            s_dfu_settings.progress.firmware_image_crc    = s_dfu_settings.progress.firmware_image_crc_last;
            s_dfu_settings.progress.data_object_size      = p_req->object_size;
            s_dfu_settings.progress.firmware_image_offset = s_dfu_settings.progress.firmware_image_offset_last;
            s_dfu_settings.write_offset                   = s_dfu_settings.progress.firmware_image_offset_last;
            // If current bank is bank 0, invalidate it.
            // This may only happen on the first CREATE after a new init packet is executed.
            if (s_dfu_settings.bank_current == NRF_DFU_CURRENT_BANK_0)
            {
                s_dfu_settings.bank_0.bank_code = NRF_DFU_BANK_INVALID;
            }
            FLASH_BUFFER_SWAP();

            // Erase the page we're at.
            if (nrf_dfu_flash_erase((m_firmware_start_addr + s_dfu_settings.progress.firmware_image_offset),
                                    CEIL_DIV(p_req->object_size, CODE_PAGE_SIZE), NULL) != NRF_SUCCESS)
            {
                NRF_LOG_ERROR("Erase operation failed");
                return NRF_DFU_RES_CODE_INVALID_OBJECT;
            }

            NRF_LOG_INFO("Creating object with size: %d. Offset: 0x%08x, CRC: 0x%08x",
                         s_dfu_settings.progress.data_object_size,
                         s_dfu_settings.progress.firmware_image_offset,
                         s_dfu_settings.progress.firmware_image_crc);

            break;

        case NRF_DFU_OBJECT_OP_WRITE:

            // Setting to ensure we are not sending faulty information in case of an early return.
            p_res->offset = s_dfu_settings.progress.firmware_image_offset;
            p_res->crc = s_dfu_settings.progress.firmware_image_crc;

            if (m_valid_init_packet_present == false)
            {
                // Can't accept data because DFU isn't initialized by init command.
                return NRF_DFU_RES_CODE_OPERATION_NOT_PERMITTED;
            }
            if (p_req->req_len > FLASH_BUFFER_LENGTH)
            {
                return NRF_DFU_RES_CODE_INSUFFICIENT_RESOURCES;
            }

            if ((p_req->req_len + s_dfu_settings.progress.firmware_image_offset - s_dfu_settings.progress.firmware_image_offset_last) > s_dfu_settings.progress.data_object_size)
            {
                // Can't accept data because too much data has been received.
                NRF_LOG_ERROR("Write request too long");
                return NRF_DFU_RES_CODE_INVALID_PARAMETER;
            }

            // Update the CRC of the firmware image.
            s_dfu_settings.progress.firmware_image_crc = crc32_compute(p_req->p_req, p_req->req_len, &s_dfu_settings.progress.firmware_image_crc);
            s_dfu_settings.progress.firmware_image_offset += p_req->req_len;

            // Update the return values
            p_res->offset = s_dfu_settings.progress.firmware_image_offset;
            p_res->crc = s_dfu_settings.progress.firmware_image_crc;

            if (m_data_buf_pos + p_req->req_len < FLASH_BUFFER_LENGTH)
            {
                //If there is enough space in the current buffer, store the received data.
                memcpy(&m_data_buf[m_current_data_buffer][m_data_buf_pos],
                       p_req->p_req, p_req->req_len);
                m_data_buf_pos += p_req->req_len;
            }
            else
            {
                // If there is not enough space in the current buffer, utilize what is left in the buffer, write it to flash and start using a new buffer.

                // Fill the remaining part of the current buffer
                uint16_t first_segment_length = FLASH_BUFFER_LENGTH - m_data_buf_pos;
                memcpy(&m_data_buf[m_current_data_buffer][m_data_buf_pos],
                       p_req->p_req,
                       first_segment_length);

                m_data_buf_pos += first_segment_length;

                // Keep only the remaining part which should be put in the next buffer.
                p_req->req_len -= first_segment_length;
                p_req->p_req += first_segment_length;

                // Write to flash.
                write_addr = m_firmware_start_addr + s_dfu_settings.write_offset;
                if (nrf_dfu_flash_store(write_addr, &m_data_buf[m_current_data_buffer][0], m_data_buf_pos, NULL) == NRF_SUCCESS)
                {
                    NRF_LOG_INFO("Storing %d bytes at: 0x%08x", m_data_buf_pos, write_addr);
                    // Pre-calculate Offset + CRC assuming flash operation went OK
                    s_dfu_settings.write_offset += m_data_buf_pos;
                }
                else
                {
                    NRF_LOG_ERROR("!!! Failed storing %d B at address: 0x%08x", m_data_buf_pos, write_addr);
                    // Previous flash operation failed. Revert CRC and offset.
                    s_dfu_settings.progress.firmware_image_crc = s_dfu_settings.progress.firmware_image_crc_last;
                    s_dfu_settings.progress.firmware_image_offset = s_dfu_settings.progress.firmware_image_offset_last;

                    // Update the return values
                    p_res->offset = s_dfu_settings.progress.firmware_image_offset_last;
                    p_res->crc = s_dfu_settings.progress.firmware_image_crc_last;
                }

                FLASH_BUFFER_SWAP();

                //Copy the remaining segment of the request into the next buffer.
                if (p_req->req_len)
                {
                    memcpy(&m_data_buf[m_current_data_buffer][m_data_buf_pos],
                           p_req->p_req, p_req->req_len);
                    m_data_buf_pos += p_req->req_len;
                }
            }

            if ((m_data_buf_pos) &&
                ( s_dfu_settings.write_offset -
                  s_dfu_settings.progress.firmware_image_offset_last +
                  m_data_buf_pos >=
                  s_dfu_settings.progress.data_object_size)
               )
            {
                //End of an object and there is still data in the write buffer. Flush the write buffer.
                write_addr = m_firmware_start_addr + s_dfu_settings.write_offset;
                if (nrf_dfu_flash_store(write_addr, &m_data_buf[m_current_data_buffer][0], m_data_buf_pos, NULL) == NRF_SUCCESS)
                {
                    NRF_LOG_INFO("Storing %d bytes at: 0x%08x", m_data_buf_pos, write_addr);
                    s_dfu_settings.write_offset += m_data_buf_pos;
                }
                else
                {
                    NRF_LOG_ERROR("!!! Failed storing %d B at address: 0x%08x", m_data_buf_pos, write_addr);
                    // Previous flash operation failed. Revert CRC and offset.
                    s_dfu_settings.progress.firmware_image_crc = s_dfu_settings.progress.firmware_image_crc_last;
                    s_dfu_settings.progress.firmware_image_offset = s_dfu_settings.progress.firmware_image_offset_last;

                    // Update the return values
                    p_res->offset = s_dfu_settings.progress.firmware_image_offset_last;
                    p_res->crc = s_dfu_settings.progress.firmware_image_crc_last;
                }

                // Swap buffers.
                FLASH_BUFFER_SWAP();
            }

            break;

        case NRF_DFU_OBJECT_OP_CRC:
            NRF_LOG_INFO("Before OP crc");
            p_res->offset = s_dfu_settings.progress.firmware_image_offset;
            p_res->crc = s_dfu_settings.progress.firmware_image_crc;
            break;

        case NRF_DFU_OBJECT_OP_EXECUTE:
            NRF_LOG_INFO("Before OP execute");
            restart_inactivity_timer = true;

            if (s_dfu_settings.progress.data_object_size !=
                s_dfu_settings.progress.firmware_image_offset -
                s_dfu_settings.progress.firmware_image_offset_last)
            {
                // The size of the written object was not as expected.
                NRF_LOG_ERROR("Invalid data here: exp: %d, got: %d",
                              s_dfu_settings.progress.data_object_size,
                              s_dfu_settings.progress.firmware_image_offset - s_dfu_settings.progress.firmware_image_offset_last);
                return NRF_DFU_RES_CODE_OPERATION_NOT_PERMITTED;
            }

            NRF_LOG_INFO("Valid Data Execute");

            // Update the offset and crc values for the last object written.
            s_dfu_settings.progress.data_object_size           = 0;
            s_dfu_settings.progress.firmware_image_offset_last = s_dfu_settings.progress.firmware_image_offset;
            s_dfu_settings.progress.firmware_image_crc_last    = s_dfu_settings.progress.firmware_image_crc;

            if (nrf_dfu_settings_write(NULL) != NRF_SUCCESS)
            {
                return NRF_DFU_RES_CODE_OPERATION_FAILED;
            }

            if (s_dfu_settings.progress.firmware_image_offset == m_firmware_size_req)
            {
                // Received the whole image. Doing postvalidate.
                NRF_LOG_INFO("Doing postvalidate");
                ret_val = nrf_dfu_postvalidate(&packet.signed_command.command.init);
            }
            break;

        case NRF_DFU_OBJECT_OP_SELECT:
            NRF_LOG_INFO("Valid Data Read info");
            p_res->crc = s_dfu_settings.progress.firmware_image_crc;
            p_res->offset = s_dfu_settings.progress.firmware_image_offset;
            p_res->max_size = DATA_OBJECT_MAX_SIZE;
            break;

        default:
            NRF_LOG_ERROR("Invalid Data Operation");
            ret_val = NRF_DFU_RES_CODE_OP_CODE_NOT_SUPPORTED;
            break;
    }

    if (restart_inactivity_timer)
    {
        inactivity_timer_restart();
    }

    return ret_val;
}


uint32_t nrf_dfu_req_handler_init(void)
{
#ifdef BLE_STACK_SUPPORT_REQD
    uint32_t ret_val = nrf_dfu_flash_init(true);
#else
    uint32_t ret_val = nrf_dfu_flash_init(false);
#endif
    VERIFY_SUCCESS(ret_val);

    // If the command is stored to flash, init command was valid.
    if (s_dfu_settings.progress.command_size != 0 && dfu_decode_commmand())
    {
        // Get the previously stored firmware size
        if (s_dfu_settings.bank_0.bank_code == NRF_DFU_BANK_INVALID && s_dfu_settings.bank_0.image_size != 0)
        {
            m_firmware_size_req = s_dfu_settings.bank_0.image_size;
        }
        else if (s_dfu_settings.bank_1.bank_code == NRF_DFU_BANK_INVALID && s_dfu_settings.bank_0.image_size != 0)
        {
            m_firmware_size_req = s_dfu_settings.bank_1.image_size;
        }
        else
        {
            return NRF_SUCCESS;
        }

        // Location should still be valid, expecting result of find-cache to be true
        (void)nrf_dfu_find_cache(m_firmware_size_req, false, &m_firmware_start_addr);

        // Setting valid init command to true to
        m_valid_init_packet_present = true;
    }

    // Initialize extended error handling with "No error" as the most recent error.
    (void) ext_error_set(NRF_DFU_EXT_ERROR_NO_ERROR);

    return NRF_SUCCESS;
}


nrf_dfu_res_code_t nrf_dfu_req_handler_on_req(void * p_context, nrf_dfu_req_t * p_req, nrf_dfu_res_t * p_res)
{
    nrf_dfu_res_code_t ret_val;

    static nrf_dfu_obj_type_t cur_obj_type = NRF_DFU_OBJ_TYPE_COMMAND;
    switch (p_req->req_type)
    {
        case NRF_DFU_OBJECT_OP_CREATE:
        case NRF_DFU_OBJECT_OP_SELECT:
            if ((nrf_dfu_obj_type_t)p_req->obj_type == NRF_DFU_OBJ_TYPE_COMMAND)
            {
                cur_obj_type = NRF_DFU_OBJ_TYPE_COMMAND;
            }
            else if ((nrf_dfu_obj_type_t)p_req->obj_type == NRF_DFU_OBJ_TYPE_DATA)
            {
                cur_obj_type = NRF_DFU_OBJ_TYPE_DATA;
            }
            else
            {
                return NRF_DFU_RES_CODE_UNSUPPORTED_TYPE;
            }
            break;
        default:
            // no implementation
            break;
    }

    switch (cur_obj_type)
    {
        case NRF_DFU_OBJ_TYPE_COMMAND:
            ret_val = nrf_dfu_command_req(p_context, p_req, p_res);
            break;

        case NRF_DFU_OBJ_TYPE_DATA:
            ret_val = nrf_dfu_data_req(p_context, p_req, p_res);
            break;

        default:
            NRF_LOG_ERROR("Invalid request type");
            ret_val = NRF_DFU_RES_CODE_INVALID_OBJECT;
            break;
    }

    return ret_val;
}

