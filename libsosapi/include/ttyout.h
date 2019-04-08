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

#include <stdio.h>

/* This is the cptr where a clients syscall enpoint will
 * be stored in the clients cspace. */
#define SYSCALL_ENDPOINT_SLOT          (1)

/* Print to the proper console.  You will need to finish these implementations */
void ttyout_init(void);
size_t sos_write(void *data, size_t count);
size_t sos_read(void *data, size_t count);
