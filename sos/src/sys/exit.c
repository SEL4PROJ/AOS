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
#include <stdio.h>
#include <stdlib.h>
#include <sel4/sel4.h>
#include <utils/util.h>

static void sos_abort(void)
{
    printf("SOS aborted\n");
    printf("To get a stack trace, call print_backtrace() from sos/src/execinfo.h\n");
    printf("at the location of the cause of the abort.\n");
    printf("And paste the resulting addresses into backtrace.py\n");
    while (1) {
        seL4_Yield();
    }
    /* We don't return after this */
}

long sys_exit(UNUSED va_list ap)
{
    abort();
    return 0;
}

long sys_rt_sigprocmask(UNUSED va_list ap)
{
    ZF_LOGV("Ignoring call to %s\n", __FUNCTION__);
    return 0;
}

long sys_gettid(UNUSED va_list ap)
{
    ZF_LOGV("Ignoring call to %s\n", __FUNCTION__);
    return 0;
}

long sys_getpid(UNUSED va_list ap)
{
    ZF_LOGV("Ignoring call to %s\n", __FUNCTION__);
    return 0;
}

long sys_tgkill(UNUSED va_list ap)
{
    ZF_LOGV("%s assuming self kill\n", __FUNCTION__);
    sos_abort();
    return 0;
}

long sys_tkill(UNUSED va_list ap)
{
    ZF_LOGV("%s assuming self kill\n", __FUNCTION__);
    sos_abort();
    return 0;
}

long sys_exit_group(UNUSED va_list ap)
{
    ZF_LOGV("Ignoring call to %s", __FUNCTION__);
    return 0;
}
