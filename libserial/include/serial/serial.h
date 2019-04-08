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
struct serial *serial_init(void);
int serial_send(struct serial *serial, char *data, int len);
int serial_register_handler(struct serial *serial, void (*handler)(struct serial *serial, char c));
