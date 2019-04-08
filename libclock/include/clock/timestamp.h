/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
#pragma once

#include <assert.h>
#include <stdint.h>
#include <utils/time.h>
#include <utils/builtin.h>

/* Timestamp and non-blocking timer functionality using the ARMv8 generic timer,
 * exported by the seL4 to user level. This cannot be used for blocking timeouts,
 * as the kernel does not export control of this timer to the user. */

static inline uint64_t timestamp_get_freq(void)
{
    /* ask the coprocessor for the frequency of the timer */
    uint64_t freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;
}

/* return the timestamp in raw clock ticks */
static inline uint64_t timestamp_ticks(void)
{
    uint64_t time = 0;
    /* ask the coprocessor for the current time */
    asm volatile("mrs %0, cntvct_el0" : "=r"(time));
    return time;
}

static inline uint64_t timestamp_ms(uint64_t freq)
{
    assert(freq != 0);
    return timestamp_ticks() / (freq / MS_IN_S);
}

static inline uint64_t timestamp_us(uint64_t freq)
{
    assert(freq != 0);
    return (US_IN_S * timestamp_ticks()) / freq;
}

static inline void udelay(uint64_t us, uint64_t freq)
{
    uint64_t start = timestamp_us(freq);
    while (timestamp_us(freq) - start < us);
}
