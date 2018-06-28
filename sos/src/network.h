/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#pragma once

#include <sel4/types.h>
#include <cspace/cspace.h>

/**
 * Initialises the network stack
 *
 * @param cspace         for creating slots for mappings
 * @param interrupt_ntfn The notification object that the driver should use for registering IRQs
 *
 */
void network_init(cspace_t *cspace, seL4_CPtr interrupt_ntfn);

/**
 * Tell the network driver to handle any pending events
 */
void network_irq(void);
