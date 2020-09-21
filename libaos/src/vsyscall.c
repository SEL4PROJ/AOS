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
#include <utils/util.h>
#include <aos/vsyscall.h>
#include <sys/uio.h>
#include <sel4/sel4.h>
#include <errno.h>

/* For each TLS syscalls we record up to one occurance here that happens
 * on startup before the syscall table is initialized. In the case of more
 * than one occurance we will panic */
static bool boot_set_tid_address_happened;
static int *boot_set_tid_address_arg;

static long boot_set_tid_address(va_list ap)
{
    int *tid = va_arg(ap, int *);
    if (boot_set_tid_address_happened) {
        ZF_LOGE("Boot version of set_tid_address somehow got called twice");
        return 1;
    }
    boot_set_tid_address_happened = true;
    boot_set_tid_address_arg = tid;
    return 1;
}

bool muslcsys_get_boot_set_tid_address(int **arg)
{
    *arg = boot_set_tid_address_arg;
    return boot_set_tid_address_happened;
}


/* Basic sys_writev for use during booting that will only use seL4_DebugPutChar */
long boot_sys_writev(va_list ap)
{
    int UNUSED fildes = va_arg(ap, int);
    struct iovec *iov = va_arg(ap, struct iovec *);
    int iovcnt = va_arg(ap, int);

    ssize_t ret = 0;

    for (int i = 0; i < iovcnt; i++) {
        char *UNUSED base = (char *)iov[i].iov_base;
        for (int j = 0; j < iov[i].iov_len; j++) {
#ifdef CONFIG_PRINTING
            seL4_DebugPutChar(base[j]);
#endif
            ret++;
        }
    }

    return ret;
}

static muslcsys_syscall_t syscall_table[MUSLC_NUM_SYSCALLS] = {
    [__NR_set_tid_address] = boot_set_tid_address,
    [__NR_writev] = boot_sys_writev,
};

muslcsys_syscall_t muslcsys_install_syscall(int syscall, muslcsys_syscall_t new_syscall)
{
    muslcsys_syscall_t ret;
    if (syscall >= ARRAY_SIZE(syscall_table)) {
        ZF_LOGF("Syscall %d exceeds syscall table size of %zu", syscall, ARRAY_SIZE(syscall_table));
    } else {
        ret = syscall_table[syscall];
        syscall_table[syscall] = new_syscall;
    }
    return ret;
}

static void debug_error(int sysnum)
{
    char buf[100];
    int i;
    sprintf(buf, "aos: Error attempting syscall %d\n", sysnum);
    for (i = 0; buf[i]; i++) {
#ifdef CONFIG_PRINTING
        seL4_DebugPutChar(buf[i]);
#endif
    }
}

long sel4_vsyscall(long sysnum, ...)
{
    va_list al;
    va_start(al, sysnum);
    muslcsys_syscall_t syscall;
    if (sysnum < 0 || sysnum >= ARRAY_SIZE(syscall_table)) {
        debug_error(sysnum);
        return -ENOSYS;
    } else {
        syscall = syscall_table[sysnum];
    }
    /* Check a syscall is implemented there */
    if (!syscall) {
        debug_error(sysnum);
        return -ENOSYS;
    }
    /* Call it */
    long ret = syscall(al);
    va_end(al);
    return ret;
}

extern void *__sysinfo;

__attribute__((constructor))
static void init_vsyscall(void)
{
    __sysinfo = sel4_vsyscall;
}

/* Put a pointer to sel4_vsyscall in a special section so anyone loading us
 * knows how to configure our syscall table */
uintptr_t VISIBLE SECTION("__vsyscall") __vsyscall_ptr = (uintptr_t) sel4_vsyscall;
