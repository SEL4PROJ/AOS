/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <sel4/types.h>
#include <nfs/nfs.h>

extern fhandle_t mnt_point;

/**
 * Initialises the network stack
 * @param[in] interrupt_ep The asynchronous endpoint that the 
 *                         driver should use for registering IRQs
 */
extern void network_init(seL4_CPtr interrupt_ep);

/**
 * Initialises DMA memory for the network driver
 * @param[in] paddr    The base physical address of the memory to use for DMA
 * @param[in] sizebits The size (1 << sizebits bytes) of the memory provided.
 * @return             0 on success
 */
extern int dma_init(seL4_Word paddr, int sizebits);

/**
 * Allows the network driver to handle any pending events
 */
extern void network_irq(void);

#endif
