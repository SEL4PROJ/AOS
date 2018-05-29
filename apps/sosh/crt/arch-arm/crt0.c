/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stddef.h>
#include <syscall_stubs_sel4.h>

MUSLC_SYSCALL_TABLE;

int main(void);
void exit(int code);

void __attribute__((externally_visible)) _start(void) {
    SET_MUSLC_SYSCALL_TABLE;
    int ret = main();
    exit(ret);
    /* should not get here */
    while(1);
}
