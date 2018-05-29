/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef PANIC_H
#define PANIC_H

#include <stdio.h>
/**
 * Panic if condition is true
 */
#define conditional_panic(a, b) __conditional_panic(a, b, __FILE__, __func__, __LINE__)
#define panic(b) conditional_panic(1, b)

void __conditional_panic(int condition, const char *message, const char *file,
        const char *func, int line);

#endif
