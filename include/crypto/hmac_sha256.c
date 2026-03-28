/******************************************************************************
 * File: include/crypto/hmac_sha256.h
 * Description: None
 * Copyright (c) 2026 Maior Cristian
 ******************************************************************************/

/**************************************************
 * INCLUDE FILES
 ***************************************************/
#include <stdbool.h>
#include "crypto/hmac_sha256.h"
#include "crypto/sha256.h"
#include "uart/uart0.h"

/**************************************************
 * MACRO DEFINITIONS
 ***************************************************/

/**************************************************
 * GLOBAL VARIABLES
 ***************************************************/

/**************************************************
 * HELPER FUNCTIONS
 ***************************************************/

/******************************************************************************
* Function: hmac_key_init
* Description: copy the key from the HMAC_KEY_ADDRESS into the local copy_key
* Returns: none
*****************************************************************************/
void hmac_key_init(const uint8_t key[HMAC_KEY_SIZE])
{
    volatile uint8_t *copy_key = ((volatile uint8_t*) HMAC_KEY_ADDR);
    for(unsigned i = 0; i < 32; i++) {
        *(copy_key + i) = key[i];
    }

    // data memory barrier asm instruction
    __asm__ volatile("dmb sy" ::: "memory");
    uart_puts("[CRYPTO] HMAC key loaded\n"); // Assist instruction
} 

/******************************************************************************
* Function: hmac_tag_compute
* Description: 1. assign byte by byte into the msg array so that each index contains 1 byte out of 4
               2. k_padded = HMAC_KEY_ADDRESS + full zeros
               3. ipad_key = (HMAC_KEY_ADDR00000000) ^ 0x36
               4. read the .md for the concrete example
* Returns: none
*****************************************************************************/
void hmac_tag_compute(const volatile mailbox_t *mb, uint8_t tag_out[HMAC_TAG_SIZE])
{
    uint8_t  k_padded[64];       // key zero-padded to SHA-256 block size
    uint8_t  ipad_key[64];       // k_padded XOR 0x36
    uint8_t  opad_key[64];       // k_padded XOR 0x5C
    uint8_t  msg[16];            // the 4 mailbox fields as raw bytes
    uint8_t  inner_hash[32];     // result of inner SHA-256
    struct tc_sha256_state_struct state;
    // Copy all the fields into the msg[] array
    /* sender_id = 0x01020304 */
    msg[0] = (uint8_t)(mb->sender_id >> 24);  /* 0x01 */
    msg[1] = (uint8_t)(mb->sender_id >> 16);  /* 0x02 */
    msg[2] = (uint8_t)(mb->sender_id >> 8);   /* 0x03 */
    msg[3] = (uint8_t)(mb->sender_id);        /* 0x04 */

    /* msg_type */
    msg[4] = (uint8_t)(mb->msg_type >> 24);
    msg[5] = (uint8_t)(mb->msg_type >> 16);
    msg[6] = (uint8_t)(mb->msg_type >> 8);
    msg[7] = (uint8_t)(mb->msg_type);

    /* msg_data */
    msg[8]  = (uint8_t)(mb->msg_data >> 24);
    msg[9]  = (uint8_t)(mb->msg_data >> 16);
    msg[10] = (uint8_t)(mb->msg_data >> 8);
    msg[11] = (uint8_t)(mb->msg_data);

    /* counter */
    msg[12] = (uint8_t)(mb->counter >> 24);
    msg[13] = (uint8_t)(mb->counter >> 16);
    msg[14] = (uint8_t)(mb->counter >> 8);
    msg[15] = (uint8_t)(mb->counter);

    /* k_padded init with HMAC_KEY_ADDR values*/
    /* First for loop copies the first HMAC_KEY_ADDR with the first half*/
    /* Second for copies full 0 into the other half */
    for (unsigned int i = 0; i < 32; i++) k_padded[i] = HMAC_KEY_ADDR[i];
    for (unsigned int i = 32; i < 64; i++) k_padded[i] = 0x00;

    /* ipad_key = (000000HMAC_KEY_ADDR) ^ 0x36 */
    for (unsigned int i = 0; i < 64; i++) ipad_key[i] = k_padded[i] ^ HMAC_IPAD;
    tc_sha256_init(&state);
    tc_sha256_update(&state, ipad_key, 64);
    tc_sha256_update(&state, msg, 16);
    tc_sha256_final(inner_hash, &state);

    /* opad_key = (000000HMAC_KEY_ADDR) ^ 0x5C */
    for (unsigned int i = 0; i < 64; i++) opad_key[i] = k_padded[i] ^ HMAC_OPAD;
    tc_sha256_init(&state);
    tc_sha256_update(&state, opad_key, 64);
    tc_sha256_update(&state, inner_hash, 32);
    tc_sha256_final(tag_out, &state);
}

/******************************************************************************
* Function: hmac_tag_verify
* Description: expected[i] MUST equal to mb->tag[i] i.e 1 XOR 1 = 0 
* Returns: true or false
*****************************************************************************/
int hmac_tag_verify(const volatile mailbox_t *mb)
{
    uint8_t  expected[32];       // used in the verify hmac function
    uint8_t change = 0x00;

    hmac_tag_compute(mb, expected);
    for(unsigned i = 0; i < 32; i++) {
        expected[i] = expected[i] ^ mb->tag[i];
        change |= expected[i];
    }

    return (change == 0x00);
}