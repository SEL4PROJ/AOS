/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
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
