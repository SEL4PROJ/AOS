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
#pragma once

#include <sel4/types.h>
#include <cspace/cspace.h>

/**
 * Initialises the network stack
 *
 * @param cspace         for creating slots for mappings
 * @param ntfn_irq       badged notification object bound to SOS's endpoint, for ethernet IRQs
 * @param ntfn_tick      badged notification object bound to SOS's endpoint, for network tick IRQs
 * @param timer_vaddr    mapped timer device. network_init will set up a periodic network_tick
 *                       using the SoC's watchdog timer (which is not used by your timer driver
 *                       and has a completely different programming model!)
 */
void network_init(cspace_t *cspace, seL4_CPtr ntfn_irq, seL4_CPtr ntfn_tick, void *timer_vaddr);

/**
 * Tell the network driver to handle a watchdog tick IRQ. This is required as the TCP/IP stack and libnfs
 * need to check internal timeouts even if there are no network interrupts.
 */
void network_tick(void);

/**
 * Tell the network driver to handle an ethernet device IRQ
 */
void network_irq(void);
