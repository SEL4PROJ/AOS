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

/* 
 * Initialise the network console by setting up the udp socket 
 * to communicate with your local "serial" interface. 
 * Note that this function will fail if called before the network has
 * initialised. 
 */
struct network_console *network_console_init(void);

/* 
 * Send data over the network to appear on your local "serial" interface.
 * Returns the length of data sent, which may be less than len, or -1 on error.
 */
int network_console_send(struct network_console *network_console, char *data, int len);

/* 
 * Register a handler function to be called when the network receives 
 * incoming data from the "serial" interface. 
 */
int network_console_register_handler(struct network_console *network_console,
                        void (*handler)(struct network_console *network_console, char c));
