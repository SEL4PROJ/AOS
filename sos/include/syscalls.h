/* @TAG(DATA61_BSD) */
#pragma once

#include <stdarg.h>

/* prototype all the syscalls we implement */
long sys_set_thread_area(va_list ap);
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
