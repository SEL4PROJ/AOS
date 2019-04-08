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

typedef void (*vputchar_t)(char c);
void update_vputchar(vputchar_t vputchar);

/* prototype all the syscalls we implement */
long sys_set_tid_address(va_list ap);
long sys_exit(va_list ap);
long sys_rt_sigprocmask(va_list ap);
long sys_gettid(va_list ap);
long sys_getpid(va_list ap);
long sys_tgkill(va_list ap);
long sys_tkill(va_list ap);
long sys_exit_group(va_list ap);
long sys_ioctl(va_list ap);
long sys_brk(va_list ap);
long sys_mmap2(va_list ap);
long sys_mmap(va_list ap);
long sys_writev(va_list ap);
long sys_nanosleep(va_list ap);
long sys_clock_gettime(va_list ap);
long sys_getuid(va_list ap);
long sys_getgid(va_list ap);
long sys_close(va_list ap);
long sys_openat(va_list ap);
long sys_socket(va_list ap);
long sys_bind(va_list ap);
long sys_listen(va_list ap);
long sys_connect(va_list ap);
long sys_accept(va_list ap);
long sys_sendto(va_list ap);
long sys_recvfrom(va_list ap);
long sys_readv(va_list ap);
long sys_getsockname(va_list ap);
long sys_getpeername(va_list ap);
long sys_fcntl(va_list ap);
long sys_setsockopt(va_list ap);
long sys_getsockopt(va_list ap);
long sys_ppoll(va_list ap);
long sys_madvise(UNUSED va_list ap);
