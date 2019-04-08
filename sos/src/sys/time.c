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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <clock/timestamp.h>
#include <sel4/sel4.h>
#include <utils/util.h>

static uint64_t freq = 0;

long sys_nanosleep(va_list ap)
{
    if (unlikely(freq == 0)) {
        freq = timestamp_get_freq();
    }

    struct timespec *req = va_arg(ap, struct timespec *);

    /* for now we just spin and yield -- TODO consider using a continuation
     * and setting a timeout so interrupts can be handled while sos sleeps, after the timer
     * milestone has been implemented */
    uint64_t us = req->tv_sec * US_IN_S;
    us += req->tv_nsec / NS_IN_US;

    uint64_t start = timestamp_us(freq);
    while (timestamp_us(freq) - start < us) {
        seL4_Yield();
    }

    return 0;
}

long sys_clock_gettime(va_list ap)
{
    if (unlikely(freq == 0)) {
        freq = timestamp_get_freq();
    }

    clockid_t clk_id = va_arg(ap, clockid_t);
    struct timespec *res = va_arg(ap, struct timespec *);
    if (clk_id != CLOCK_REALTIME) {
        return -EINVAL;
    }
    uint64_t micros = timestamp_us(freq);
    res->tv_sec = micros / US_IN_S;
    res->tv_nsec = (micros % US_IN_S) * NS_IN_US;
    return 0;
}
