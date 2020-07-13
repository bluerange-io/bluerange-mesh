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
#include <string.h>
#include <nrf_error.h>
#include <nrf_soc.h>

#include "malloc.h"

#include "ccm_soft.h"

#ifdef __GNUC__
#define DYNAMIC_ARRAY(arrayName, size) uint8_t arrayName[size]
#endif
#if defined(_MSC_VER)
#define DYNAMIC_ARRAY(arrayName, size) uint8_t* arrayName = (uint8_t*)alloca(size)
#endif

#define L_LEN CCM_LENGTH_FIELD_LENGTH

/**
 * Reverse memcpy.
 *
 * Writes size bytes from p_src to p_dst in reverse order.
 *
 * @param p_dst Destination address.
 * @param p_src Source address.
 * @param size  Number of bytes to write.
 */
static inline void utils_reverse_memcpy(uint8_t * p_dst, const uint8_t * p_src, uint16_t size)
{
    p_src += size;
    while (size--)
    {
        *((uint8_t *) p_dst++) = *((const uint8_t *) --p_src);
    }
}

/**
 * Bytewise XOR for an array.
 *
 * XORs size amount of data from p_src1 and p_src2 and stores it in p_dst.
 *
 * @note p_dst may be equal to one or more of the sources.
 *
 * @param p_dst  Destination address.
 * @param p_src1 First source address.
 * @param p_src2 Secound source address.
 * @param size   Number of bytes to XOR.
 */
static inline void utils_xor(uint8_t * p_dst, const uint8_t * p_src1, const uint8_t * p_src2, uint16_t size)
{
    while (0 != size)
    {
        size--;
        p_dst[size] = p_src1[size] ^ p_src2[size];
    }
}

void aes_encrypt(const uint8_t * const key, const uint8_t * const clear_text, uint8_t * const cipher_text)
{
    nrf_ecb_hal_data_t aes_data;
    memcpy(aes_data.key, key, 16);
    memcpy(aes_data.cleartext, clear_text, 16);
    (void) sd_ecb_block_encrypt((nrf_ecb_hal_data_t *)&aes_data);
    memcpy(cipher_text, aes_data.ciphertext, 16);
}


static void ccm_soft_authenticate_blocks(const uint8_t * p_key,
                                         const uint8_t * p_data,
                                         uint16_t data_size,
                                         uint8_t B[16],
                                         uint8_t offset_B,
                                         uint8_t X[16])
{
    while (data_size)
    {
        if (data_size < (16 - offset_B))
        {
            memcpy(&B[offset_B], p_data, data_size);
            memset(&B[offset_B + data_size], 0x00, 16 - (offset_B + data_size));
            data_size = 0;
        }
        else
        {
            memcpy(&B[offset_B], p_data, (16 - offset_B));
            data_size -= (16 - offset_B);
            p_data += (16 - offset_B);
        }

        offset_B = 0;

        utils_xor(B, X, B, 16);

        aes_encrypt(p_key, B, X);
    }
}

static void ccm_soft_authenticate(const ccm_soft_data_t * p_data, uint8_t T[])
{

    uint8_t B[16];
    uint8_t X[16];

    B[0] = (
        ((p_data->a_len > 0 ? 1 : 0) << 6)        |
        ((((p_data->mic_len - 2)/2) & 0x07) << 3) |
        ((L_LEN - 1) & 0x07));

    memcpy(&B[1], p_data->p_nonce, (15 - L_LEN));
    utils_reverse_memcpy(&B[16-L_LEN], (const uint8_t *) &p_data->m_len, L_LEN);

    aes_encrypt(p_data->p_key, B, X);

    if (p_data->a_len > 0)
    {
        utils_reverse_memcpy(&B[0], (const uint8_t*) &p_data->a_len, 2);
        ccm_soft_authenticate_blocks(p_data->p_key, p_data->p_a, p_data->a_len, B, 2, X);
    }

    ccm_soft_authenticate_blocks(p_data->p_key, p_data->p_m, p_data->m_len, B, 0, X);

    memcpy(T, X, p_data->mic_len);
}

static void ccm_soft_crypt(const ccm_soft_data_t * p_data, uint8_t * A, uint8_t * S, uint16_t i)
{
    uint16_t octets_m = p_data->m_len;

    while (octets_m)
    {
        i++;

        utils_reverse_memcpy(&A[16-L_LEN], (uint8_t *) &i, L_LEN);
        aes_encrypt(p_data->p_key, A, S);

        if (octets_m < 16)
        {
            utils_xor(p_data->p_out + 16*(i-1), p_data->p_m + 16*(i-1), S, octets_m);
            octets_m = 0;
        }
        else
        {
            utils_xor(p_data->p_out + 16*(i-1), p_data->p_m + 16*(i-1), S, 16);
            octets_m -= 16;
        }
    }
}

void ccm_soft_encrypt(ccm_soft_data_t * p_data)
{
#if CCM_DEBUG_MODE_ENABLED
    __LOG_XB(LOG_SRC_CCM, LOG_LEVEL_INFO, "ccm_soft_encrypt: IN ",  p_data->p_m, p_data->m_len);
#endif
    uint8_t A[16];
    uint8_t S[16];
    DYNAMIC_ARRAY(T, p_data->mic_len);

    uint16_t i = 0;

    ccm_soft_authenticate(p_data, T);

    A[0] = ((L_LEN - 1) & 0x07);

    memcpy(&A[1], p_data->p_nonce, (15 - L_LEN));
    utils_reverse_memcpy(&A[16 - L_LEN], (uint8_t *) &i, L_LEN);

    aes_encrypt(p_data->p_key, A, S);

    utils_xor(p_data->p_mic, T, S, p_data->mic_len);

    ccm_soft_crypt(p_data, A, S, i);

#if CCM_DEBUG_MODE_ENABLED
    __LOG_XB(LOG_SRC_CCM, LOG_LEVEL_INFO, "ccm_soft_encrypt: OUT", p_data->p_out, p_data->m_len);
    __LOG_XB(LOG_SRC_CCM, LOG_LEVEL_INFO, "ccm_soft_encrypt: MIC", p_data->p_mic, p_data->mic_len);
#endif
}

void ccm_soft_decrypt(ccm_soft_data_t * p_data, bool * p_mic_passed)
{
#if CCM_DEBUG_MODE_ENABLED
    __LOG_XB(LOG_SRC_CCM, LOG_LEVEL_INFO, "ccm_soft_decrypt: IN",  p_data->p_m, p_data->m_len);
#endif

    uint8_t A[16];
    uint8_t S[16];
   DYNAMIC_ARRAY(T, p_data->mic_len);
    uint16_t i = 0;

    /* Recreate block ciphers. */
    A[0] = ((L_LEN - 1) & 0x07);

    memcpy(&A[1], p_data->p_nonce, (15 - L_LEN));
    utils_reverse_memcpy(&A[16 - L_LEN], (uint8_t *) &i, L_LEN);

    aes_encrypt(p_data->p_key, A, S);

    utils_xor(S, p_data->p_mic, S, p_data->mic_len);

    /* Try to decrypt data with ciphers. */
    ccm_soft_crypt(p_data, A, S, i);


    /* Authenticate data */
    ccm_soft_data_t auth_data;
    DYNAMIC_ARRAY(mic, p_data->mic_len);

    auth_data.p_key   = p_data->p_key;
    auth_data.p_nonce = p_data->p_nonce;
    auth_data.p_m     = p_data->p_out;
    auth_data.m_len   = p_data->m_len;
    auth_data.p_a     = p_data->p_a;
    auth_data.a_len   = p_data->a_len;
    auth_data.p_mic   = mic;
    auth_data.p_out   = NULL;
    auth_data.mic_len = p_data->mic_len;

    ccm_soft_authenticate(&auth_data, T);

    /* Generate MIC */
    i = 0;
    A[0] = ((L_LEN - 1) & 0x07);

    memcpy(&A[1], p_data->p_nonce, (15 - L_LEN));
    utils_reverse_memcpy(&A[16 - L_LEN], (uint8_t *) &i, L_LEN);

    aes_encrypt(p_data->p_key, A, S);

    utils_xor(mic, T, S, p_data->mic_len);

#if CCM_DEBUG_MODE_ENABLED
    __LOG_XB(LOG_SRC_CCM, LOG_LEVEL_INFO, "ccm_soft_decrypt: OUT", p_data->p_out, p_data->m_len);
    __LOG_XB(LOG_SRC_CCM, LOG_LEVEL_INFO, "ccm_soft_decrypt: MIC", mic, p_data->mic_len);
#endif

    *p_mic_passed = memcmp(mic, p_data->p_mic, p_data->mic_len) == 0;
#if CCM_DEBUG_MODE_ENABLED
    if (!*p_mic_passed)
    {
        /* No MIC match. */
        __LOG_XB(LOG_SRC_CCM, LOG_LEVEL_INFO, "ccm_soft_decrypt: mic_in", p_data->p_mic, p_data->mic_len);
        __LOG_XB(LOG_SRC_CCM, LOG_LEVEL_INFO, "ccm_soft_decrypt: mic_out", mic, p_data->mic_len);
    }
#endif
}
