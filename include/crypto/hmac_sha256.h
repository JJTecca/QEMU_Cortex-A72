/******************************************************************************
 * File: include/crypto/hmac_sha256.h
 * Project: RPi5-Industrial-Gateway Bare Metal Development
 * Description: HMAC-SHA256 inter-core message authentication
 *              Operates on mailbox_t — no dynamic allocation
 * Copyright (c) 2026 Maior Cristian
 ******************************************************************************/
#ifndef HMAC_SHA256_H
#define HMAC_SHA256_H

/**************************************************
 * INCLUDE FILES
 ***************************************************/
#include <stdint.h>
#include "ipc/ipc.h"
//Todo : Implement TinyCrypt SHA-256
//#include "crypto/tc_sha256.h"   /* TinyCrypt SHA-256 — Intel, BSD-2 */

/**************************************************
 * MACRO DEFINITIONS
 ***************************************************/
#define HMAC_KEY_SIZE    32
#define HMAC_TAG_SIZE    32
#define HMAC_IPAD        0x36
#define HMAC_OPAD        0x5C

/* Fixed address in shared RAM — 8-byte aligned after ring buffer */
#define HMAC_KEY_ADDR    ((const uint8_t *)0x40220310)

/**************************************************
 * FUNCTION PROTOTYPES
 ***************************************************/

void hmac_key_init(const uint8_t key[HMAC_KEY_SIZE]);
void hmac_tag_compute(const volatile mailbox_t *mb, uint8_t tag_out[HMAC_TAG_SIZE]);
int hmac_tag_verify(const volatile mailbox_t *mb);


#endif /* HMAC_SHA256_H */
