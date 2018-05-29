/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "panic.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <sel4/sel4.h>

#define verbose 1

inline void __conditional_panic(int condition, const char *message,
        const char *file, const char *func, int line) {
    if (condition) {
        _dprintf(0, "\033[1;31m", "\nPANIC %s-%s:%d %s\n\n", file, func, line, message);
        abort();
    }
}

