/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef CSPACE_HELPERS_H
#define CSPACE_HELPERS_H
#include <sel4/sel4.h>

#define CSPACE_NOINDEX (-1)
#define CSPACE_NOSLOT (-1)

// The size of the guard skip for a single level cspace
#define CSPACE_ONE_LEVEL_SKIP_BITS (32 - CSPACE_NODE_SIZE_IN_SLOTS_BITS)
// The size of the guard skip for a two-level cspace
#define CSPACE_TWO_LEVEL_SKIP_BITS (32 - 2 * CSPACE_NODE_SIZE_IN_SLOTS_BITS)


#define CSPACE_LEAF_OFFSET(x) (x & (CSPACE_NODE_SIZE_IN_SLOTS -1))

extern cspace_ut_alloc_t cspace_ut_alloc; 
extern cspace_ut_free_t cspace_ut_free; 
extern cspace_ut_translate_t cspace_ut_translate; 
extern cspace_malloc_t cspace_malloc;
extern cspace_free_t cspace_free;

int32_t cspace_alloc_level1_index(cspace_t *c);
seL4_CPtr cspace_alloc_level2_slot(cspace_t *c);
cspace_err_t cspace_free_slot(cspace_t *c, seL4_CPtr slot);
cspace_err_t cspace_free_level1_index(cspace_t *c, int32_t s);
cspace_err_t cspace_free_level2_slot(cspace_t *c, seL4_CPtr s);
int cspace_retype_depth(cspace_t *cur_cspace);


#endif /* CSPACE_HELPERS_H */
