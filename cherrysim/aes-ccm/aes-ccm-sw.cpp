#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "crys_aesccm.h"
#include "aes.h"

extern "C"{
#include <ccm_soft.h>
}

#define USE_NORDIC_SW 0

CIMPORT_C CRYSError_t  CC_AESCCM(
         SaSiAesEncryptMode_t       EncrDecrMode,     /*!< [in] A flag specifying whether an AES Encrypt (::SASI_AES_ENCRYPT) or Decrypt
                      (::SASI_AES_DECRYPT) operation should be performed. */
         CRYS_AESCCM_Key_t          CCM_Key,          /*!< [in] Pointer to AES-CCM key. */
         CRYS_AESCCM_KeySize_t      KeySizeId,        /*!< [in] Enumerator defining the key size (only 128 bit is valid). */
         uint8_t                   *N_ptr,            /*!< [in] Pointer to the Nonce. */
         uint8_t                    SizeOfN,          /*!< [in] Nonce byte size. The valid values depend on the ccm mode:
                                                                                        <ul><li>CCM:  valid values = [7 .. 13].</li>
                                                                                        <li>CCM*: valid values = [13].</li></ul> */
         uint8_t                   *ADataIn_ptr,      /*!< [in] Pointer to the additional input data. The buffer must be contiguous. */
         uint32_t                   ADataInSize,      /*!< [in] Byte size of the additional data. */
         uint8_t                   *TextDataIn_ptr,   /*!< [in] Pointer to the plain-text data for encryption or cipher-text data for decryption.
                      The buffer must be contiguous. */
         uint32_t                   TextDataInSize,   /*!< [in] Byte size of the full text data. */
         uint8_t                   *TextDataOut_ptr,  /*!< [out] Pointer to the output (cipher or plain text data according to encrypt-decrypt mode)
                       data. The buffer must be contiguous. */
         uint8_t                    SizeOfT,          /*!< [in] AES-CCM MAC (tag) byte size. The valid values depend on the ccm mode:
                                                                                        <ul><li>CCM:  valid values = [4, 6, 8, 10, 12, 14, 16].</li>
                                                                                        <li>CCM*: valid values = [0, 4, 8, 16].</li></ul>*/
         CRYS_AESCCM_Mac_Res_t      Mac_Res,        /*!< [in/out] Pointer to the MAC result buffer. */
                           uint32_t ccmMode                             /*!< [in] Flag specifying whether AES-CCM or AES-CCM* should be performed. */
)
{
  CRYSError_t ret = CRYS_FATAL_ERROR;

  if (KeySizeId != CRYS_AES_Key128BitSize) return ret;
  if (SizeOfN != 13) return ret;  // 13 is hardcoded in aes-ccm lib
  if (SizeOfT != 4) return ret;
  if (ccmMode != CRYS_AESCCM_MODE_CCM) return ret;
#if USE_NORDIC_SW
  if (EncrDecrMode == SASI_AES_ENCRYPT)
  {
    ccm_soft_data_t ccme;
    ccme.a_len = ADataInSize;
    ccme.mic_len = 4;
    ccme.m_len = TextDataInSize;
    ccme.p_a = ADataIn_ptr;
    ccme.p_key = CCM_Key;
    ccme.p_m = TextDataIn_ptr;
    ccme.p_mic = Mac_Res;
    ccme.p_nonce = N_ptr;
    ccme.p_out = TextDataOut_ptr;
    ccm_soft_encrypt(&ccme);
    ret = CRYS_OK;
  }
  else if (EncrDecrMode == SASI_AES_DECRYPT)
  {
    ccm_soft_data_t ccmd;
    ccmd.a_len = ADataInSize;
    ccmd.mic_len = 4;
    ccmd.m_len = TextDataInSize;
    ccmd.p_a = ADataIn_ptr;
    ccmd.p_key = CCM_Key;
    ccmd.p_m = TextDataIn_ptr;
    ccmd.p_mic = Mac_Res;
    ccmd.p_nonce = N_ptr;
    ccmd.p_out = TextDataOut_ptr;
    memset(TextDataOut_ptr, 0x00, TextDataInSize);
    bool result = false;
    ccm_soft_decrypt(&ccmd, &result);
    if (result == true) ret = CRYS_OK;
  }
#else
  if (EncrDecrMode == SASI_AES_ENCRYPT)
  {
    ret = aes_ccm_ae(CCM_Key, //const u8 *key, 
                     16, //size_t key_len, 
                     N_ptr, //const u8 *nonce,
                     4, //size_t M, 
                     TextDataIn_ptr,// const u8 *plain, 
                     TextDataInSize ,//size_t plain_len,
                     ADataIn_ptr,// const u8 *aad, 
                     ADataInSize, //size_t aad_len, 
                     TextDataOut_ptr,// u8 *crypt, 
                     Mac_Res);// u8 *auth);
  }
  else if (EncrDecrMode == SASI_AES_DECRYPT)
  {
    ret = aes_ccm_ad(CCM_Key, 
                     16, 
                     N_ptr,
                     4, 
                     TextDataIn_ptr, 
                     TextDataInSize,
                     ADataIn_ptr, 
                     ADataInSize, 
                     Mac_Res, 
                     TextDataOut_ptr);
  }
#endif
  return ret;
}
