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
#include <cspace/cspace.h>
/* A very basic UART (universal asynchronous receiver and transmitter) driver */

/* initialise the uart device */
void uart_init(cspace_t *cspace);
/* output a single chart on the uart device */
void uart_putchar(char c);
