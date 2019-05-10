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

#include <utils/util.h>
#include <cspace/cspace.h>
#include <sel4/sel4.h>

/* top level cspace node size, for the root cnode, in bits, where size = 2^bits */
#define INITIAL_TASK_CNODE_SIZE_BITS 18u
#define INITIAL_TASK_CSPACE_BITS (CNODE_SLOT_BITS(INITIAL_TASK_CNODE_SIZE_BITS) + \
                                  CNODE_SLOT_BITS(CNODE_SIZE_BITS))
#define INITIAL_TASK_CSPACE_SLOTS BIT(INITIAL_TASK_CSPACE_BITS)

/**
 * Bootstrap the initial tasks cspace and 4k untyped allocator from what seL4 provides to the initial task.
 */
void sos_bootstrap(cspace_t *cspace, const seL4_BootInfo *bi);

/* Map a frame, using cspace to allocate any slots. Must *not* be called from
 * cspace code. Frames are allocated contiguously from SOS_UT_TABLE, and no
 * mechanism is implemented to free them, avoid using for memory you would like
 * to be able to free.
 */
void *bootstrap_map_frame(cspace_t *cspace, seL4_CPtr cap);

/* these are exported for the tests to use */
void *bootstrap_cspace_map_frame(void *cookie, seL4_CPtr cap, seL4_CPtr free_slots[MAPPING_SLOTS], seL4_Word *used);
void *bootstrap_cspace_alloc_4k_ut(void *cookie, seL4_CPtr *cap);
void bootstrap_cspace_free_4k_ut(void *cookie, void *untyped);
