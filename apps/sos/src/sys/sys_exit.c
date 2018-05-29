/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdio.h>
#include <stdlib.h>
#include <sel4/sel4.h>

#include "execinfo.h" /*for backtrace()*/

static void sel4_abort(void) {
    printf("seL4 root server aborted\n");

    /* Printout backtrace*/
    void *array[10] = {NULL};
    int size = 0;

    size = backtrace(array, 10);
    if (size) {

        printf("Backtracing stack PCs:  ");

        for (int i = 0; i < size; i++) {
            printf("0x%x  ", (unsigned int)array[i]);
        }
        printf("\n");
    }

#if defined(CONFIG_DEBUG_BUILD)
    seL4_DebugHalt();
#endif
    while (1)
        ; /* We don't return after this */
}

long
sys_exit(va_list ap)
{
    abort();
    return 0;
}

long
sys_rt_sigprocmask(va_list ap)
{
    printf("Ignoring call to %s\n", __FUNCTION__);
    return 0;
}

long
sys_gettid(va_list ap)
{
    printf("Ignoring call to %s\n", __FUNCTION__);
    return 0;
}

long
sys_getpid(va_list ap)
{
    printf("Ignoring call to %s\n", __FUNCTION__);
    return 0;
}

long
sys_tgkill(va_list ap)
{
    printf("%s assuming self kill\n", __FUNCTION__);
    sel4_abort();
    return 0;
}

