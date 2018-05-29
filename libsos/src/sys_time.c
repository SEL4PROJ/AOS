/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <autoconf.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sos.h>
#include <utils/time.h>

#include <sel4/sel4.h>

long sys_nanosleep(va_list ap) {
    struct timespec *req = va_arg(ap, struct timespec*);
    struct timespec *rem = va_arg(ap, struct timespec*);
    /* We ignore the remaining since we will always wait the full time */
    (void)rem;
    /* construct a sleep call */
    int millis = req->tv_sec * MS_IN_S;
    millis += req->tv_nsec / NS_IN_MS;
    sos_sys_usleep(millis);
    return 0;
}

long sys_clock_gettime(va_list ap) {
    clockid_t clk_id = va_arg(ap, clockid_t);
    struct timespec *res = va_arg(ap, struct timespec*);
    if (clk_id != CLOCK_REALTIME) {
        return -EINVAL;
    }
    int64_t micros = sos_sys_time_stamp();
    res->tv_sec = micros / US_IN_S;
    res->tv_nsec = (micros % US_IN_S) * NS_IN_US;
    return 0;
}
