/*
 * Copyright 2018, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#pragma once

#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <utils/util.h>

#define WORD_BITS (sizeof(unsigned long) * 8u)
#define WORD_INDEX(bit) ((bit) / WORD_BITS)
#define BIT_INDEX(bit)  ((bit) % WORD_BITS)

/* A bit field is simply an array of words, indexed by bit */

/* set a bit in the bitfield to 1 */
static inline void bf_set_bit(unsigned long *bits, unsigned long bit)
{
    bits[WORD_INDEX(bit)] |= (BIT(BIT_INDEX(bit)));
}

/* set a bit in the bitfield to 0 */
static inline void bf_clr_bit(unsigned long *bits, unsigned long bit)
{
    bits[WORD_INDEX(bit)] &= ~(BIT(BIT_INDEX(bit)));
}

static inline bool bf_get_bit(unsigned long *bits, unsigned long bit)
{
    return (bool) !!(bits[WORD_INDEX(bit)] & (BIT(BIT_INDEX(bit))));
}

static inline unsigned long bf_first_free(size_t words, unsigned long bits[words])
{
    /* find the first free word */
    int i = 0;
    for (; i < words && bits[i] == ULONG_MAX; i++);

    unsigned long bit = i * WORD_BITS;
    /* it's illegal to call CLZL on 0, so check first */
    if (i < words && bits[i] > 0) {
        bit += (WORD_BITS - CLZL(bits[i]));
    }
    return bit;
}

