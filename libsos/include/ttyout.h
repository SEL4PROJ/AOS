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

/* Print to the proper console.  You will need to finish these implementations */
extern size_t
sos_write(void *data, size_t count);
extern size_t
sos_read(void *data, size_t count);

#endif
