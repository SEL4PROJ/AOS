/*
 * Copyright 2018, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#include <platsupport/io.h>
#include <cspace/cspace.h>

/**
 * Initialises DMA memory for the network driver
 *
 * @param cspace       cspace that this allocator can use to allocate slots with and retype.
 *                     Not used to allocate slots in dma_init, just for dma_malloc.
 * @param sizebits     The size (1 << sizebits) bytes of the memory provided.
 * @param vspace       cptr to the vspace to use for mapping operations
 * @param ut           cptr to a ut that is sizebits in size and can be used to retype
 *                     4k untyped objects
 * @param pstart       The base physical address of the memory to use for DMA
 * @return             0 on success
 */
int dma_init(cspace_t *cspace, size_t sizebits, seL4_CPtr vspace, seL4_CPtr ut, uintptr_t pstart);


void *sos_dma_malloc(void* cookie, size_t size, int align, int cached, ps_mem_flags_t flags);
void sos_dma_free(void *cookie, void *addr, size_t size);
uintptr_t sos_dma_pin(void *cookie, void *addr, size_t size);
void sos_dma_unpin(void *cookie, void *addr, size_t size);
void sos_dma_cache_op(void *cookie, void *addr, size_t size, dma_cache_op_t op);
