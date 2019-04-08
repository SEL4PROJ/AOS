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

#include <stdarg.h>

/* call this to initialise the syscall table for muslc, this
 * will install the following functions such that the C library will
 * use them to interact with sos */
void sosapi_init_syscall_table(void);

/* prototype all the syscalls we implement */
long sys_set_tid_address(va_list ap);
long sys_exit(va_list ap);
long sys_rt_sigprocmask(va_list ap);
long sys_gettid(va_list ap);
long sys_getpid(va_list ap);
long sys_tgkill(va_list ap);
long sys_exit_group(va_list ap);
long sys_openat(va_list ap);
long sys_close(va_list ap);
long sys_readv(va_list ap);
long sys_read(va_list ap);
long sys_ioctl(va_list ap);
long sys_brk(va_list ap);
long sys_mmap(va_list ap);
long sys_writev(va_list ap);
