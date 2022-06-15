/*********************************************************************
* Filename:   sha256_external.h
* Author:     Brad Conte (brad AT bradconte.com)
* Copyright:
* Disclaimer: This code is presented "as is" without any guarantees.
* Details:    Defines the API for the corresponding SHA256 implementation.
*********************************************************************/
#ifndef SHA256_EXTERNAL_H__
#define SHA256_EXTERNAL_H__
#ifdef __cplusplus
extern "C"
{
#endif
/*************************** HEADER FILES ***************************/
#include <stddef.h>

/****************************** MACROS ******************************/
#define SHA256_BLOCK_SIZE 32            // SHA256 outputs a 32 byte digest

/**************************** DATA TYPES ****************************/
typedef unsigned char BYTE;             // 8-bit byte
typedef unsigned int  WORD;             // 32-bit word, change to "long" for 16-bit machines

typedef struct {
    BYTE data[64];
    WORD datalen;
    unsigned long long bitlen;
    WORD state[8];
} SHA256_CTX;

/*********************** FUNCTION DECLARATIONS **********************/
void sha256_external_init(SHA256_CTX *ctx);
void sha256_external_update(SHA256_CTX *ctx, const BYTE data[], size_t len);
void sha256_external_final(SHA256_CTX *ctx, BYTE hash[]);

#ifdef __cplusplus
}
#endif
#endif   // SHA256_EXTERNAL_H__