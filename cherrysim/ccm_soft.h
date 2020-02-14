/* Copyright (c) 2010 - 2017, Nordic Semiconductor ASA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
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
 */
#ifndef CCM_SOFT_H__
#define CCM_SOFT_H__

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup CCM_SOFT Software CCM encryption library
 * @ingroup MESH_CORE
 * This library provides a software implementation of the AES-CCM encryption
 * algorithm
 * @{
 */

/** Length/size of length variable. */
#define CCM_LENGTH_FIELD_LENGTH (2)

/** Length of key. */
#define CCM_KEY_LENGTH (16)

/** Length of nonce. */
#define CCM_NONCE_LENGTH (13)

/**
 * Struct for passing AES-CCM encryption data.
 *
 * Same struct is used for both encryption and decryption.
 */
typedef struct
{
    const uint8_t * p_key;                  /**< Block cipher key. */
    const uint8_t * p_nonce;                /**< Nonce. */
    const uint8_t * p_m;                    /**< Message to authenticate and encrypt/decrypt. */

    uint16_t  m_len;                        /**< Message size (in octets). */

    const uint8_t * p_a;                    /**< Additional authenticated data. */
    uint16_t a_len;                         /**< Additional data size (in octets). */

    uint8_t * p_out;                        /**< (Out) Encrypted/decrypted output. */

    uint8_t * p_mic;                        /**< (Out) Message Integrety Check value */
    uint8_t   mic_len;                      /**< Length of the message integrity check value. */
} ccm_soft_data_t;

/**
 * Encrypts data with the AES-CCM algorithm.
 *
 * @param p_data Structure with the needed parameters to encrypt the cleartext
 *               message. See @ref ccm_soft_data_t.
 */
void ccm_soft_encrypt(ccm_soft_data_t * p_data);

/**
 * Decrypts data using the AES-CCM algorithm.
 *
 * @param p_data       Pointer to structure with parameters for decrypting a message.
 *                     See @ref ccm_soft_data_t.
 * @param p_mic_passed Pointer to bool for storing result of MIC
 */
void ccm_soft_decrypt(ccm_soft_data_t * p_data, bool * p_mic_passed);

/**
 * @}
 */
#endif
