/******************************************************************************
* File: tc_defs.h
* Description: Here we have all the constants and _set function
* Copyright (c) 2026 Maior Cristian
*****************************************************************************/

/**************************************************
 * INCLUDE FILES
 ***************************************************/
#ifndef TC_DEFS_H
#define TC_DEFS_H

#include <stdint.h>
#include <stddef.h>
/**************************************************
 * MACRO DEFINTIONS
 ***************************************************/
/* TinyCrypt return codes */
#define TC_CRYPTO_SUCCESS  1
#define TC_CRYPTO_FAIL     0

/* Replaces TinyCrypt _set() — zeroes or fills a memory region */
static inline void _set(void *to, uint8_t val, size_t len)
{
    volatile uint8_t *p = (volatile uint8_t *)to;
    for (size_t i = 0; i < len; i++)
        p[i] = val;
}

#endif /* TC_DEFS_H */
