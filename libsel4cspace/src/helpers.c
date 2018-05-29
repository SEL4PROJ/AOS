/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <cspace/cspace.h>
#include "helpers.h"
#include <sel4/sel4.h>
#include <assert.h>
#include <stdio.h>



int cspace_retype_depth(cspace_t *c)
{
    if (c->levels == 1) {
	return 0; /* we want the root cnode cap */
    } 
    else if (c->levels == 2) {
	return (32 - CSPACE_NODE_SIZE_IN_SLOTS_BITS); /* translate the skip and the first cnode */
    }
    else {
	assert(0); /* cspace has unsupported number of levels */
	return 0; /* not reached, avoid compiler error */
    }
}

int32_t cspace_alloc_level1_index(cspace_t *c)
{
    int32_t s;
    s = c->next_level1_free_index;
    assert(s != CSPACE_NOSLOT);
    c->next_level1_free_index = c->level1_alloc_table[s];
#ifdef CSPACE_DEBUG
    printf("cspace: alloc level1 index: %d\n",s);
#endif
    return s;
}

cspace_err_t cspace_free_level1_index(cspace_t *c, int32_t s)
{
    assert(s >= 0 && s < CSPACE_NODE_SIZE_IN_SLOTS);
    c->level1_alloc_table[s] = c->next_level1_free_index;
    c->next_level1_free_index = s;
    return CSPACE_NOERROR;
}

seL4_CPtr cspace_alloc_level2_slot(cspace_t *c)
{
    seL4_CPtr s, ut_cptr;
    uint32_t i,j,offset,v;
    seL4_CPtr l1;
    cspace_err_t cerr;
    seL4_Error serr;

    if (c->num_free_slots <= 0) {
	/* allocate a new level 2 cnode here */        
        l1 = cspace_alloc_level1_index(c);
        assert(l1 != CSPACE_NOSLOT);
        
        v = cspace_ut_alloc(CSPACE_NODE_SIZE_IN_MEM_BITS);
        assert (v != 0);
        cerr = cspace_ut_translate(v, &ut_cptr, &offset);
        assert (cerr == CSPACE_NOERROR);
        c->level1_alloc_table[l1] = v; /* save the phys address for later free */

        
        serr = seL4_Untyped_RetypeAtOffset(ut_cptr,
                                           seL4_CapTableObject,
                                           offset,
                                           CSPACE_NODE_SIZE_IN_SLOTS_BITS,
                                           c->root_cnode,
                                           0, 
                                           0,
                                           l1,
                                           1);
        assert(serr == seL4_NoError);
        
        for (j = (l1 << CSPACE_NODE_SIZE_IN_SLOTS_BITS), i = j >> CSPACE_NODE_SIZE_IN_SLOTS_BITS;
             j < ((l1 +1) << CSPACE_NODE_SIZE_IN_SLOTS_BITS)-1;
             j++) {
            if ((j & (CSPACE_NODE_SIZE_IN_SLOTS -1)) == 0) {
                i = j >> CSPACE_NODE_SIZE_IN_SLOTS_BITS;
                c->level2_alloc_tables[i] = 
                    cspace_malloc(sizeof(uint32_t)*CSPACE_NODE_SIZE_IN_SLOTS);
#ifdef CSPACE_DEBUG
                printf("cspace: malloc bookkeeping for leaf node %d\n",i);
#endif
                assert(c->level2_alloc_tables[i]);
            }
            c->level2_alloc_tables[i][j & (CSPACE_NODE_SIZE_IN_SLOTS -1)] = j+1; 
            
        }
        c->level2_alloc_tables[i][j & (CSPACE_NODE_SIZE_IN_SLOTS -1)] = CSPACE_NULL; 

        c->num_free_slots = CSPACE_NODE_SIZE_IN_SLOTS;
        c->next_level2_free_slot = l1 << CSPACE_NODE_SIZE_IN_SLOTS_BITS;
        
#ifdef CSPACE_DEBUG
    printf("cspace: free slots %d\n",  c->num_free_slots);
#endif

    }
    s = c->next_level2_free_slot;
    c->next_level2_free_slot = c->level2_alloc_tables[s>>CSPACE_NODE_SIZE_IN_SLOTS_BITS]
	[s & (CSPACE_NODE_SIZE_IN_SLOTS -1)];
    c->num_free_slots--;
    return s;
}

cspace_err_t cspace_free_level2_slot(cspace_t *c, seL4_CPtr s)
{
    assert(s >= 0 && s < (CSPACE_NODE_SIZE_IN_SLOTS * CSPACE_NODE_SIZE_IN_SLOTS));
    c->level2_alloc_tables[s>>CSPACE_NODE_SIZE_IN_SLOTS_BITS]
	[s & (CSPACE_NODE_SIZE_IN_SLOTS -1)] = c->next_level2_free_slot;
 
    c->next_level2_free_slot = s;
    c->num_free_slots++;

    return CSPACE_NOERROR;
}
