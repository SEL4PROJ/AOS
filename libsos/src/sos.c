/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sos.h>

#include <sel4/sel4.h>

int sos_sys_open(const char *path, fmode_t mode) {
    assert(!"You need to implement this");
    return -1;
}

int sos_sys_read(int file, char *buf, size_t nbyte) {
    assert(!"You need to implement this");
    return -1;
}

int sos_sys_write(int file, const char *buf, size_t nbyte) {
    assert(!"You need to implement this");
    return -1;
}

void sos_sys_usleep(int msec) {
    assert(!"You need to implement this");
}

int64_t sos_sys_time_stamp(void) {
    assert(!"You need to implement this");
    return -1;
}

