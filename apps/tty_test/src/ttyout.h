/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _TTYOUT_H
#define _TTYOUT_H

#include <stdio.h>

/* Initialise tty output.
 * Must be called prior to any IPC receive operation.
 * Returns task ID of initial server (OS).
 */
extern void ttyout_init(void);

/**
 * This is our system call endpoint cap, as defined by the root server
 */
#define SYSCALL_ENDPOINT_SLOT  (1)


/* routines needed by the libs/c i.e. -lc implementation */

/* Print to the proper console.  You will need to finish these implementations
 */
extern size_t
sos_write(void *data, size_t count);
extern size_t
sos_read(void *data, size_t count);

#endif
