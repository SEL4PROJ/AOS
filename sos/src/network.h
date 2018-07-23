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
 * @param ntfn           notification object bound to SOS's endpoint
 */
void network_init(cspace_t *cspace, seL4_CPtr ntfn);

/**
 * Tell the network driver to handle any pending events.
 * This should be called every 100ms or so even when no network IRQs
 * are present - to keep libnfs chugging along.
 */
void network_tick(void);

/**
 * Tell the network driver to handle a network IRQ
 */
void network_irq(void);
