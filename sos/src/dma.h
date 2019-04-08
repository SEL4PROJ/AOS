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

typedef struct {
    uintptr_t vaddr; // virtual  address of the allocated DMA memory
    uintptr_t paddr; // physical address of the allocated DMA memory
} dma_addr_t;

/**
 * Initialises a pool of DMA memory.
 *
 * @param cspace       cspace that this allocator can use to allocate a slot with and
 *                     retype the passed in untyped with. Only used in init.
 * @param vspace       cptr to the vspace to use for mapping and cache operations.
 * @param ut           cptr to a ut that is seL4_LargePageBits in size
 * @param pstart       The base physical address of the ut
 * @param vstart       Virtual address to map the large page
 * @return             0 on success
 */
int dma_init(cspace_t *cspace, seL4_CPtr vspace, seL4_CPtr ut, uintptr_t pstart, uintptr_t vstart);

/**
 * Allocate an amount of DMA memory.
 *
 * @param size in bytes to allocate
 * @param align alignment to align start of address to
 * @return a dma_addr_t representing the memory. On failure values will be 0.
 */
dma_addr_t sos_dma_malloc(size_t size, int align);

/**
 * Convert the provided physical DMA address into a virtual address
 *
 * @param address to convert
 */
uintptr_t sos_dma_phys_to_virt(uintptr_t phys);

/**
 * Convert the provided virtual DMA address into a physical address
 *
 * @param address to convert
 */
uintptr_t sos_dma_virt_to_phys(uintptr_t virt);

/* Cache operation functions.
 * @param addr the start address to operate on.
 * @param size amount in bytes to operate on.
 * @return seL4_Error code that results from the invocation.
 */
seL4_Error sos_dma_cache_invalidate(uintptr_t addr, size_t size);
seL4_Error sos_dma_cache_clean(uintptr_t addr, size_t size);
seL4_Error sos_dma_cache_clean_invalidate(uintptr_t addr, size_t size);
