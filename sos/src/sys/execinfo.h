/*
 * Copyright 2018, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
#ifndef _EXECINFO_H
#define _EXECINFO_H
//#include <debug/execinfo.h>

extern int backtrace (void **__array, int __size);
//libc_hidden_proto (__backtrace)

//extern char **backtrace_symbols (void *const *__array, int __size);

//extern void backtrace_symbols_fd (void *const *__array, int __size,
//				    int __fd);
//libc_hidden_proto (__backtrace_symbols_fd)

#endif
