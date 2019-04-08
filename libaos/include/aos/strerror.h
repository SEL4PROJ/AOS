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

#define sel4_error(e, str)  ((e == seL4_NoError) ? (void)0 : __sel4_error(e, __FILE__, __func__, __LINE__, str))

void __sel4_error(int, const char *, const char *, int, char *);

const char *sel4_strerror(int errcode);
extern char *sel4_errlist[];
