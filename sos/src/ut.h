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
#pragma once

/*
 * This is an untyped object allocator which tracks objects of 4k and less in size.
 *
 * You may extend this allocator to track further details about 4k frames.
 */

#include <stdint.h>
#include <stddef.h>
#include <sel4/sel4.h>
#include <utils/util.h>
#include <cspace/cspace.h>

#include "bootstrap.h"

typedef struct {
    /* the lowest valid address in the region */
    seL4_Word start;
    /* the highest valid address in the region +1 */
    seL4_Word end;
} ut_region_t;

/* a specific untyped */
typedef struct ut ut_t;
PACKED struct ut {
    /* The capability space of the initial task is small (~20 bits) so
     * we can use the remaining bits to store other information */
    seL4_Untyped cap : 20;
    unsigned long valid : 1;
    unsigned long size_bits : 4;
    unsigned long unused : 39;
    ut_t *next; // pointer to next item in list
};
compile_time_assert("Small cspace bits", INITIAL_TASK_CSPACE_BITS == 20);

/* list of valid object sizes we can allocate */
#define N_UNTYPED_LISTS (seL4_PageBits - seL4_EndpointBits + 1)

/* Untyped memory table */
typedef struct {
    /* first paddr in the table of 4K untypeds */
    seL4_Word first_paddr;
    /* region of 4K untypeds - one for each untyped . */
    ut_t *untypeds;
    /* list of free untypeds, one list per object size. The untyped list at seL4_PageBits
     * is a free list in the untypeds region. The rest are sublists, with bookkeeping data allocated
     * from an untyped */
    ut_t *free_untypeds[N_UNTYPED_LISTS];
    /* the number of non-device 4k untypeds this table is managing */
    size_t n_4k_untyped;
    /* list of unused nodes which can be used to populate untyped lists
     * of untyped objects < 4K in size, where the bookkeping data is allocated on demand from the 4k
     * untypeds free list */
    ut_t *free_structures;
} ut_table_t;

/* return the size (in 4K pages) of the table required to cover a specific region */
size_t ut_pages_for_region(ut_region_t region);

/**
 * Initialise the ut allocator (empty). ut_add_untyped_range must be called to populate the
 * allocator with 4k untyped objects before it can allocate.
 *
 * @param memory pointer to mapped memory sufficient to cover the bookkeeping for the memory provided
 *               in the region parameter. More specifically, memory must be
 *               >= ut_pages_for_region(region) * sizeof(ut_t).
 *               We assume that the memory provided is already zeroed.
 * @param region memory region the ut should cover.
 *
 * */
void ut_init(void *memory, ut_region_t region);

/**
 * Return the amount of non-device memory the ut_table is managing, in bytes.
 *
 * Note: this value *does not change* for the life of the ut allocator.
 * The intended use of this function is to allow you to determine the amount
 * of SOS virtual memory you will need in order to be able to map all of memory.
 */
size_t ut_size(void);

/* Add a contiguous (in memory and capabilities) set of 4k untypeds to the table. The paddr
 * must be in the range provided to ut_init with the region parameter.
 *
 * @param paddr  the physical address that the untypeds start at
 * @param cap    the start of the range of cptrs where the untypeds are
 * @param n      the number of untypeds in the contiguous region of caps and paddrs.
 * @param device is this a device untyped or not.
 */
void ut_add_untyped_range(seL4_Word paddr, seL4_CPtr cap, size_t n, bool device);

/**
 * Allocate an untyped object of 4K in size. This operation will *never* result in a
 * cspace allocation as all 4K objects are pre-allocated.
 *
 * @param paddr[out]  return the physical address of the untyped memory here. NULL
 *                    if no value should be returned.
 * @return            a pointer to the 4K table entry that represents the 4K
 *                    untyped returned, containing the capability which can then be
 *                    passed to seL4_Untyped_Retype.
 *                    NULL if no memory is available.
 */
ut_t *ut_alloc_4k_untyped(uintptr_t *paddr);

/**
 * Allocate an untyped of a specific size < seL4_PageBits.
 *
 * This operation may result in cspace allocations, as we may need to retype untyped
 * objects of bigger sizes until we get the size we need, if free untyped objects of
 * the correct size are unavailable. For this reason this function must be passed a
 * cspace, which must not call back into this function to avoid infinite recursion. The
 * cspace *can* call into ut_alloc_alloc_4k_untyped.
 *
 * @param size_bits    the amount of contiguous and aligned memory to reserve (2^size_bits)
 * @param cspace_alloc a cspace which can be used to allocate slots.
 * @return             A pointer which can be used to free the allocation.
 *                     NULL if no memory is available.
 */

ut_t *ut_alloc(size_t size_bits, cspace_t *cspace_alloc);

/**
 * Mark an untyped object as free. This will make the object available for reallocation.
 * This allocator does not remerge untyped objects: if you allocate your entire
 * set of memory as objects < seL4_PageBits, you will not be able to allocate any
 * bigger objects. It is possible to merge untypeds, but this is not implemented to
 * reduce complexity.
 *
 * Allocations made with ut_alloc_frame and ut_alloc can both be freed with this function.
 *
 * @param ut        the ut pointer returned by ut_alloc or ut_alloc_4k_untyped
 * @param size_bits the size to the memory to free, that was used for the allocation.
 */
void ut_free(ut_t *ut);

/**
 * Allocate 4K of device untyped at a specific physical address. These are only pulled
 * from the untyped which have been passed to ut_add_untyped_region with device=true.
 *
 * @param paddr the physical address to allocate.
 * @return      cptr to the allocated untyped. seL4_CapNull if the paddr is not available.
 */
ut_t *ut_alloc_4k_device(uintptr_t paddr);
