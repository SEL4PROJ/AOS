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
#pragma once

#include <utils/util.h>
#include <sel4/sel4.h>

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
