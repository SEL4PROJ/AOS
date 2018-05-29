/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <autoconf.h>
#include <cspace/cspace.h>
#include "helpers.h"
#include <sel4/sel4.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef NDEBUG
#undef NDEBUG /* We rely on assert to print error info */
#endif
#include <assert.h>
#include "sel4_debug.h"

cspace_t *cur_cspace = NULL;

/* Intantiate the function pointers to the outside allocators */
cspace_ut_alloc_t cspace_ut_alloc; 
cspace_ut_free_t cspace_ut_free; 
cspace_ut_translate_t cspace_ut_translate; 
cspace_malloc_t cspace_malloc;
cspace_free_t cspace_free;



cspace_err_t cspace_root_task_bootstrap(cspace_ut_alloc_t alloc_func, 
                                        cspace_ut_free_t free_func, 
                                        cspace_ut_translate_t translate_func,
                                        cspace_malloc_t malloc_func,
                                        cspace_free_t mfree_func)
{
    seL4_BootInfo *bi = seL4_GetBootInfo();
    seL4_CPtr level1_cptr;
    seL4_CPtr boot_cptr;
    seL4_CPtr ut_cptr;
    uint32_t offset;
    uint32_t addr;
    uint32_t err;
    uint32_t j;

    seL4_CPtr i;
    cspace_t *space;
    assert(bi);
    
    /* Initialise the allocation functions. We are good to go now */
    cspace_ut_alloc = alloc_func;
    cspace_ut_free = free_func;
    cspace_ut_translate = translate_func;
    cspace_malloc = malloc_func;
    cspace_free = mfree_func;
    
    /* use three slots from the current boot cspace */
    assert((bi->empty.end - bi->empty.start) >= 2); /* Confirm we have room to get started */
    level1_cptr = bi->empty.start;
    boot_cptr = 0; /* only valid in the new cspace, relies on first slot being free */

#ifdef CSPACE_DEBUG
    printf("cspace: root_task_bootstrap()\n");
    printf("cspace: bi start = %d\n", level1_cptr);
    printf("cspace: seL4_CapInitThreadCNode = %d\n", seL4_CapInitThreadCNode);
    printf("cspace: CSPACE_CNODE_SIZE_IN_SLOTS = %d\n", CSPACE_NODE_SIZE_IN_SLOTS);
#endif

    space = cspace_malloc(sizeof(cspace_t));
    assert(space);
 
    space->levels = 2; /* root task cspace will be 2 levels */
    
    /* initialise the free level1 index list for list based allocation */
    for (i = 0; i < (CSPACE_NODE_SIZE_IN_SLOTS-1) ; i++) {
        space->level1_alloc_table[i] = i+1;
    }

    /* Terminate the list with NOSLOT (-1) so we can use the 0 slot in level 1 */
    space->level1_alloc_table[CSPACE_NODE_SIZE_IN_SLOTS-1] = CSPACE_NOSLOT; 
    
    /* create the new level 1 cnode */
    addr = cspace_ut_alloc(CSPACE_NODE_SIZE_IN_MEM_BITS);
    assert(addr != 0);
    err = cspace_ut_translate(addr, &ut_cptr, &offset);
    assert(err == CSPACE_NOERROR);
    
    space->addr = addr;
    space->root_cnode = seL4_CapInitThreadCNode; // The root cnode cap will end up here eventually
    err = seL4_Untyped_RetypeAtOffset(ut_cptr,
                                      seL4_CapTableObject,
                                      offset,
                                      CSPACE_NODE_SIZE_IN_SLOTS_BITS,
                                      seL4_CapInitThreadCNode,
                                      seL4_CapInitThreadCNode, 
                                      CSPACE_DEPTH,
                                      level1_cptr,
                                      1);
    sel4_error(err, "Allocating new root cnode");
    

    /*
     * Create allocate the required number of level 2 cnodes, i.e up
     * to emtpy.start -1.
     */
    for (i = 0;
         i <= ((bi->empty.start -1) >> CSPACE_NODE_SIZE_IN_SLOTS_BITS);
         i++) {
        
        addr = cspace_ut_alloc(CSPACE_NODE_SIZE_IN_MEM_BITS);
        assert (addr != 0);
        err = cspace_ut_translate(addr, &ut_cptr, &offset);
        assert (err == CSPACE_NOERROR);
        
        /*
	 * the alloc table serves a dual purpose, a free list, and addr of the level 2 node when
	 * allocated.
	 */
        space->level1_alloc_table[i] = addr; /* save the phys address for later free */

        err = seL4_Untyped_RetypeAtOffset(ut_cptr,
                                          seL4_CapTableObject,
                                          offset,
                                          CSPACE_NODE_SIZE_IN_SLOTS_BITS,
                                          seL4_CapInitThreadCNode,   /*
								      * allocate the new level 2s
								      * directly in the new cspace
								      */
                                          level1_cptr,
                                          CSPACE_DEPTH,
                                          i,
                                          1);
        sel4_error(err, "Allocating new level 2 cnodes");
#ifdef CSPACE_DEBUG
        printf("cspace: added level 2 cnode @ index %d\n", i);
#endif
    }

    space->next_level1_free_index = i; /*
				        * next empty level1 index, free list initialised earlier
				        */
    


    /*
     * We need references in the new cspace to both itself (i.e. level_cptr from boot cspace) and
     * the boot cspace (seL4_CapInitThreadCNode in the boot cspace).
     *
     *
     * BOOT CSPACE             -> NEW CSPACE
     *
     * seL4_CapInitThreadCNode -> boot_cptr
     * level1_cptr             -> seL4_CapInitThreadCNode
     *
     * Note: we have a cspace at level1_cptr that is 2 * CSPACE_NODE_SIZE_IN_SLOTS_BITS deep, but
     * with no initial guard skip. So we insert in the leaf cnode (level2_cptr) for now.
     */


    err = seL4_CNode_Copy(level1_cptr, 
                          boot_cptr, /* = 0 first slot in new space */
                          2 * CSPACE_NODE_SIZE_IN_SLOTS_BITS,  
                          seL4_CapInitThreadCNode, 
                          seL4_CapInitThreadCNode, 
                          CSPACE_DEPTH, 
                          seL4_AllRights);
    sel4_error(err, "Making copy of root task's initial cnode cap");
    
    
    err = seL4_CNode_Mint(level1_cptr,
                          seL4_CapInitThreadCNode, /* this seems to be the index @ depth specified */
                          2 * CSPACE_NODE_SIZE_IN_SLOTS_BITS,
                          seL4_CapInitThreadCNode, 
                          level1_cptr, 
                          CSPACE_DEPTH, 
                          seL4_AllRights, 
                          seL4_CapData_Guard_new(0, CSPACE_TWO_LEVEL_SKIP_BITS));
    sel4_error(err, "Making new cap to new cspace");
    
    space->guard = seL4_CapData_Guard_new(0, CSPACE_TWO_LEVEL_SKIP_BITS);
    
    /*
     * Switch from BOOT CSPACE -> NEW CSPACE by switching current threads cnode
     */
    err = seL4_TCB_SetSpace(seL4_CapInitThreadTCB, 
                            0, 
                            level1_cptr, 
                            seL4_CapData_Guard_new(0, CSPACE_TWO_LEVEL_SKIP_BITS), 
                            seL4_CapInitThreadPD, 
                            seL4_NilData);
    sel4_error(err, "Replacing intial cnode with new cspace");

    /*
     * Okay, now seL4_CapInitThreadCNode refers to the new cspace, the old boot cspace is boot_cptr.
     *
     * Copy all the caps in the original boot cspace (except for the one already done), i.e. from 1
     * to the first empty cap in the original boot cspace.
     */
    
    for (i = 1; i < bi->empty.start; i++) {
        switch (i) {
        case seL4_CapInitThreadCNode:
        case seL4_CapIPI:
        case seL4_CapIOPort:
        case seL4_CapIOSpace:
#ifdef CONFIG_ARCH_ARM
            /* no arch bootinfo frame on arm */
        case seL4_CapArchBootInfoFrame:
#endif
            continue;
        }
#ifdef CSPACE_DEBUG
        printf("cspace: moving cap %d boot -> new cspace\n",i);
#endif
        err = seL4_CNode_Move(seL4_CapInitThreadCNode,
                        i, 
                        CSPACE_DEPTH, 
                        boot_cptr, /* src */
                        i, 
                        CSPACE_DEPTH);
        sel4_error(err, "Copying initial cnode caps to new cspace");
    }


    /*
     * Nothing is left in the original boot cnode now, so lets delete it.
     */
    err = seL4_CNode_Delete(seL4_CapInitThreadCNode, 
                            boot_cptr,
                            CSPACE_DEPTH);
    sel4_error(err, "Deleting root task's original cnode cap");
#ifdef CSPACE_DEBUG
    printf("cspace: deleted the boot cnode\n");
#endif

    /*
     * Fix up the cspace struct's internal book keeping to reflect
     * whats allocated thus far.
     *
     * 1 .. (bi.empty.start - 1) slots are allocated in the cspace
     *
     * 0 .. ((bi.empty.start -1) >> CSPACE_NODE_SIZE_IN_SLOTS_BITS)
     * are allocated in the level 1
     */

    
    for (j = 0; j < bi->empty.start; j++) {
        if ((j & (CSPACE_NODE_SIZE_IN_SLOTS -1)) == 0) {
            i = j >> CSPACE_NODE_SIZE_IN_SLOTS_BITS;
            space->level2_alloc_tables[i] = cspace_malloc(sizeof(uint32_t)*CSPACE_NODE_SIZE_IN_SLOTS);
#ifdef CSPACE_DEBUG
            printf("cspace: malloc bookkeeping for leaf node %d\n",i);
#endif
            assert(space->level2_alloc_tables[i]);
        }
        space->level2_alloc_tables[i][j & (CSPACE_NODE_SIZE_IN_SLOTS -1)] = 0; 
        /* sentinal phys address => initial caps can't be freed :( */
    }
    /*
     * i now refers to last level 2 allocated, j to the next free slot 
     */
    
    space->next_level2_free_slot = bi->empty.start;
    space->num_free_slots = 
        CSPACE_NODE_SIZE_IN_SLOTS - (j & (CSPACE_NODE_SIZE_IN_SLOTS -1));
#ifdef CSPACE_DEBUG
    printf("cspace: free slots %d\n",  space->num_free_slots);
#endif
    for (; /* make a free list with the remaining slots */ 
         (j & (CSPACE_NODE_SIZE_IN_SLOTS -1)) < (CSPACE_NODE_SIZE_IN_SLOTS -1);
         j++) {
        space->level2_alloc_tables[i][j & (CSPACE_NODE_SIZE_IN_SLOTS -1)] = j + 1 ;
    }
    /* mark end of the free list */
    space->level2_alloc_tables[i][j & (CSPACE_NODE_SIZE_IN_SLOTS -1)] = CSPACE_NULL; 
    
    /*
     * initialise the rest of the pointer corresponding to unallocated level 2 leaf cnodes.
     */
    for (i =  space->next_level1_free_index;
         i < CSPACE_NODE_SIZE_IN_SLOTS;
         i++) {
        space->level2_alloc_tables[i] = NULL;
    }

    cur_cspace = space; /* set the global for the rest of the library use */
#ifdef CSPACE_DEBUG
    printf("cspace: root tasks cspace bootstrapped\n");
#endif
    return CSPACE_NOERROR;
}



cspace_t *cspace_create(int levels) /* either 1 or 2 level */
{
    cspace_t *c;
    int i;
    cspace_err_t err;
    seL4_Error r;
    seL4_CPtr ut_cptr; seL4_Word offset; uint32_t addr;
    seL4_CPtr slot;

    
    c = cspace_malloc(sizeof(cspace_t));
    assert(c != NULL);
    
    addr = cspace_ut_alloc(CSPACE_NODE_SIZE_IN_MEM_BITS);
    assert(addr != 0);
    c->addr = addr;
    err = cspace_ut_translate(addr, &ut_cptr, &offset);
    assert(err == CSPACE_NOERROR);
    
    slot = cspace_alloc_slot(cur_cspace);
    assert(slot != CSPACE_NULL);
    
    r = seL4_Untyped_RetypeAtOffset(ut_cptr, 
                                    seL4_CapTableObject,
                                    offset,
                                    CSPACE_NODE_SIZE_IN_SLOTS_BITS,
                                    cur_cspace->root_cnode, /* current cspace */
                                    slot >>  CSPACE_NODE_SIZE_IN_SLOTS_BITS, /* index */
                                    cspace_retype_depth(cur_cspace), /* depth */
                                    CSPACE_LEAF_OFFSET(slot), /* offset */ 
                                    1);
    sel4_error(r, "Creating first cnode in new cspace");
    c->root_cnode = slot;
    
    /* keep a linked list of free slots (true for both 1- and 2-level cspaces) */

    for (i = 0; i < (CSPACE_NODE_SIZE_IN_SLOTS-1) ; i++) {
        c->level1_alloc_table[i] = i+1;
    }
    c->level1_alloc_table[CSPACE_NODE_SIZE_IN_SLOTS-1] = CSPACE_NOSLOT; /* end of level 1 list */
    switch (levels) {
    case 1:
        c->next_level1_free_index = 1; /* user visible slots start at 1 */
        break;
    case 2:
        c->next_level1_free_index = 0; /* level 1 slots start at 0, with a second level cnode */ 
        break;
    default:
        assert(!"unsupported number of levels");
        break;
    }
    c->levels = levels;
    
    if (levels == 1) {
        c->num_free_slots = CSPACE_NODE_SIZE_IN_SLOTS -1;
    }
    else if (levels == 2) {
        seL4_CPtr l1slot;
        uint32_t l1index;
        seL4_Error r;
        
        /*
	 * Allocate and initialise a level 2 node if we have a two level cspace.
	 */

        addr = cspace_ut_alloc(CSPACE_NODE_SIZE_IN_MEM_BITS);
        assert(addr!= 0);
        
        err = cspace_ut_translate(addr, &ut_cptr, &offset);
        assert(err == CSPACE_NOERROR);

        l1index = cspace_alloc_level1_index(c); /* this frees an entry in the alloc table */
        assert(l1index != CSPACE_NOINDEX);
        
        l1slot = l1index << CSPACE_NODE_SIZE_IN_SLOTS_BITS; /*
							     * extend the level 1 index into a full
							     * 32-bit slot address. 
							     */
        c->level1_alloc_table[l1index] = addr; /* which we now use to store addr */
        
        /* allocate cnode with the cap in level 1 of the new cspace */
        r = seL4_Untyped_RetypeAtOffset(ut_cptr, 
                                        seL4_CapTableObject, 
                                        offset,
                                        CSPACE_NODE_SIZE_IN_SLOTS_BITS,
                                        c->root_cnode, /* current cspace */
                                        0, /* index  */
                                        0, /* depth */
                                        l1index, /* offset */
                                        1);
        sel4_error(r, "Creating 2-level cnode in new cspace");
        
        /* initialise the pointer to second level allocation tables to NULL */
        for (i = 0; i < CSPACE_NODE_SIZE_IN_SLOTS; i++) {
            c->level2_alloc_tables[i] = NULL; 
        }
        
        /* Allocate one table for the single L2 cnode */
        c->level2_alloc_tables[l1index] = 
            cspace_malloc(sizeof(uint32_t)*CSPACE_NODE_SIZE_IN_SLOTS);
        
        /* Initialise a free list */
        for (i = 0; i < (CSPACE_NODE_SIZE_IN_SLOTS-1) ; i++) {
            c->level2_alloc_tables[l1index][i] = l1slot+i+1;
        }
        
        c->level2_alloc_tables[l1index][CSPACE_NODE_SIZE_IN_SLOTS-1] = CSPACE_NULL; /*
										     * Sentinal for
										     * level 2 is 0
										     */
        c->next_level2_free_slot = l1slot+1;
        c->num_free_slots = CSPACE_NODE_SIZE_IN_SLOTS -1;
    }
    else {
        assert(0); /* Number of specified levels unsupported in new cspace */
    }
    
    /*
     * now make a guard to the new cspace - all cpaces translate 32-bits
     */
    switch (c->levels) {
    case 1:
        c->guard = seL4_CapData_Guard_new(0, CSPACE_ONE_LEVEL_SKIP_BITS);
        break;
    case 2:
        c->guard = seL4_CapData_Guard_new(0, CSPACE_TWO_LEVEL_SKIP_BITS);
        break;
    }

    /*
     * Mint a cap with that guard and make it the cap to the root_cnode for this new cspace
     */
    
    c->root_cnode = cspace_mint_cap(cur_cspace,
                                    cur_cspace, 
                                    slot,
                                    seL4_AllRights,
                                    c->guard);
    assert(c->root_cnode != CSPACE_NULL);
    cspace_delete_cap(cur_cspace,slot); /* delete the old 'short' cap to the cnode */ 
    return c;
}


cspace_err_t cspace_destroy(cspace_t *c)
{


    int i;
    seL4_Error serr;

    assert(cur_cspace != c); /* suicide is not supported */
    assert(cur_cspace != NULL);
    
    /*
     * we don't do anything clever here:
     *  * we delete the level2s (together with their allocation tables)
     *  * then the level1
     *  * then cspace struct
     *
     *
     * Note: We assume cnode caps internal to the cspace have not been copied (or moved) out of the
     * cspace. The code will free the untyped physical address associated with the cnodes whether
     * they are really free or not.
     */


    if (c->levels == 2) {
        for (i = 0; i < CSPACE_NODE_SIZE_IN_SLOTS; i++) {
            if (c->level2_alloc_tables[i] != NULL) {
                /* delete the l2 cnode and the allocation table with it */
                serr = seL4_CNode_Delete(c->root_cnode,
                                         i,
                                         CSPACE_DEPTH - CSPACE_NODE_SIZE_IN_SLOTS_BITS);
                sel4_error(serr, "Deleting level-2 cnodes");
                cspace_ut_free(c->level1_alloc_table[i], CSPACE_NODE_SIZE_IN_MEM_BITS);
                cspace_free(c->level2_alloc_tables[i]);
            }
        }
    }
    
    cspace_delete_cap(cur_cspace, c->root_cnode); /* expectation is that this is last cap to cnode */
    cspace_ut_free(c->addr,CSPACE_NODE_SIZE_IN_MEM_BITS); 
    
    cspace_free(c);
    return CSPACE_NOERROR;
}

seL4_CPtr cspace_alloc_slot(cspace_t *c)
{
    /*
     * Allocate requires some thought as the following can happen with a naive design.
     *
     * 1. alloc_slot gets called
     * 2. alloc_slot needs to allocate a cnode and calls the cnode
     * allocation routine
     * 3. the cnode allocation routine calls alloc_slot
     * 4. repeat until stack overflows :)
     *
     * Note: variations exist for malloc if morecore can dynamically allocate Frames, but we avoid
     * that by static allocation for now.
     *
     * We can deal with cnodes by calling the external allocator for reserving untyped, but actually
     * doing the slot allocation and cnode retype internally. Thus, we don't ever call back in to
     * the library if we call out.
     */
    seL4_CPtr r;
    assert(c != NULL);
    
    switch(c->levels) {
    case 1:
        /*
	 * For the single-level cspace we re-use the internal cnode allocator of the level 1 cnode
	 * in two-level cspaces. However, in this case it's a leaf node.
	 *   
	 * We have to manually update the slot count (the two level code does it in the allocator
	 * itself).
	 *
	 * Note: The slot count is there to keep a low-water mark at some point in the future.
	 */
        if (c->num_free_slots > 0) {
            c->num_free_slots--;
            r = cspace_alloc_level1_index(c); /* index same as CPtr for single level */
            if (r == CSPACE_NOSLOT){
                return CSPACE_NULL;
            }
            return r;
        }
        else {
            return CSPACE_NULL; /* we've fill the single level cspace */
        }
        break;
    case 2:
        /*
	 * for the level 2, the alloc function does everything, including potentially allocating new
	 * level-2 cnodes to make space. 
	 */ 
        return cspace_alloc_level2_slot(c);
        break;
        
    default:
        assert(0); /* should never get here */
        break;
    }
    return CSPACE_NULL;

}
cspace_err_t cspace_free_slot(cspace_t *c, seL4_CPtr slot)
{
    assert(c != NULL);
    assert(slot != CSPACE_NULL);

    /*
     * Mark appropriate slot as free.
     *
     * Note: we postpone freeing leaf cnodes until cspace is destroyed. We could do it eagerly, but
     * suspect that is just overhead in practice.
     */
        
    switch(c->levels) {
    case 1:
        c->num_free_slots++;
        return cspace_free_level1_index(c,slot); /* index same as CPtr for single level */
        break;
    case 2:
        return cspace_free_level2_slot(c,slot);
        break;
    default:
        assert(0); /* should never get here */
    }
    return CSPACE_NOERROR;
}

seL4_Error cspace_ut_retype_addr(seL4_Word addr,
                                 seL4_Word type,
                                 seL4_Word size_bits,
                                 cspace_t *c,
                                 seL4_CPtr *p)
{
    seL4_CPtr ut_cptr; 
    seL4_Word offset;
    seL4_CPtr new;
    seL4_Error err;

                                         
    new = cspace_alloc_slot(c);
    if (new == CSPACE_NULL) {
        return seL4_NotEnoughMemory; /* Nearest sane error */
    }
    assert(p != NULL);
    *p = new;
    
    err = cspace_ut_translate(addr, &ut_cptr,&offset);
    if(err){
        return err;
    }

    err = seL4_Untyped_RetypeAtOffset(ut_cptr, 
                                      type,
                                      offset,
                                      size_bits,
                                      c->root_cnode,
                                      new >> CSPACE_NODE_SIZE_IN_SLOTS_BITS, /* First level index */
                                      CSPACE_DEPTH - CSPACE_NODE_SIZE_IN_SLOTS_BITS, /* only go to first level */
                                      new & (CSPACE_NODE_SIZE_IN_SLOTS -1), /* the index in the leaf node */
                                      1);
#ifdef CSPACE_DEBUG
    printf("cspace: ut_retype ut:%d type:%d off:%d size:%d root:%d ind:%d dep:%d sl:%d err:%d\n",
           ut_cptr, 
           type,
           offset,
           size_bits,
           c->root_cnode,
           new >> CSPACE_NODE_SIZE_IN_SLOTS_BITS, /* First level index */
           CSPACE_DEPTH - CSPACE_NODE_SIZE_IN_SLOTS_BITS, /* only go to first level */
           new & (CSPACE_NODE_SIZE_IN_SLOTS -1), 
           err    
           );
#endif
    return err;
}

seL4_CPtr cspace_copy_cap(cspace_t *dest,
                          cspace_t *src, 
                          seL4_CPtr src_cap,
                          seL4_CapRights rights)
{
    seL4_CPtr slot;
    seL4_Error err;

    assert(dest != NULL && src != NULL);
        
    slot = cspace_alloc_slot(dest);
    assert(slot != CSPACE_NULL);
    
    err = seL4_CNode_Copy(dest->root_cnode, 
                          slot, 
                          CSPACE_DEPTH,  
                          src->root_cnode, 
                          src_cap, 
                          CSPACE_DEPTH, 
                          rights);
    sel4_error(err, "Copying cap");
    
    return slot;
}

cspace_err_t cspace_delete_cap(cspace_t *c, seL4_CPtr cap)
{
    seL4_Error err;
    assert(c != NULL);
    
    err = seL4_CNode_Delete(c->root_cnode,
                            cap,
                            CSPACE_DEPTH);
    sel4_error(err, "Copying cap");
    
    return cspace_free_slot(c,cap);
}

seL4_CPtr cspace_mint_cap(cspace_t *dest, 
                          cspace_t *src, 
                          seL4_CPtr src_cap,
                          seL4_CapRights rights, 
                          seL4_CapData_t badge)
{
    seL4_CPtr slot;
    seL4_Error err;

    assert(dest != NULL && src != NULL);
    
    slot = cspace_alloc_slot(dest);
    assert(slot != CSPACE_NULL);
    
    err = seL4_CNode_Mint(dest->root_cnode, 
                          slot,
                          CSPACE_DEPTH,  
                          src->root_cnode, 
                          src_cap, 
                          CSPACE_DEPTH, 
                          rights,
                          badge);
    sel4_error(err, "Minting a cap");
    
    return slot;
}

seL4_CPtr cspace_move_cap(cspace_t *dest, cspace_t *src, seL4_CPtr src_cap)
{
    seL4_CPtr slot;
    seL4_Error err;
    assert(dest != NULL && src != NULL);
    
    slot = cspace_alloc_slot(dest);
    assert(slot != CSPACE_NULL);
    
    err = seL4_CNode_Move(dest->root_cnode, 
                          slot,
                          CSPACE_DEPTH,  
                          src->root_cnode, 
                          src_cap, 
                          CSPACE_DEPTH);
    sel4_error(err, "Moving cap");
    
    return slot;
}

seL4_CPtr cspace_mutate_cap(cspace_t *dest, 
                            cspace_t *src, 
                            seL4_CPtr src_cap,
                            seL4_CapData_t badge)
{
    seL4_CPtr slot;
    seL4_Error err;
    assert(dest != NULL && src != NULL);
    
    slot = cspace_alloc_slot(dest);
    assert(slot != CSPACE_NULL);
    
    err = seL4_CNode_Mutate(dest->root_cnode, 
                            slot,
                            CSPACE_DEPTH,  
                            src->root_cnode, 
                            src_cap, 
                            CSPACE_DEPTH, 
                            badge);
    sel4_error(err,"Mutating cap");
    
    return slot;
}

cspace_err_t cspace_recycle_cap(cspace_t *c, 
                                seL4_CPtr cap)
{
    seL4_Error err;
    assert(c != NULL);
    
    err = seL4_CNode_Recycle(c->root_cnode,
                             cap,
                             CSPACE_DEPTH);
    sel4_error(err, "Recycling cap");
    
    return CSPACE_NOERROR;
}


cspace_err_t cspace_revoke_cap(cspace_t *c, 
                               seL4_CPtr cap)
{
    seL4_Error err;
    assert(c != NULL);
    
    err = seL4_CNode_Revoke(c->root_cnode,
                            cap,
                            CSPACE_DEPTH);
    sel4_error(err, "Revoking cap");

    return CSPACE_NOERROR;
}

seL4_CPtr cspace_rotate_cap(cspace_t *dest,
                            seL4_CapData_t dest_badge,
                            cspace_t *pivot,
                            seL4_CPtr pivot_cap,
                            seL4_CapData_t pivot_badge,
                            cspace_t *src, 
                            seL4_CPtr src_cap)
{
    assert(!"This is not implemented. Why do you want to use it?");
    return CSPACE_NULL;
}


seL4_CPtr cspace_save_reply_cap(cspace_t *c)
{
    
    seL4_CPtr slot;
    seL4_Error err;
    slot = cspace_alloc_slot(c);
    assert(slot != CSPACE_NULL);
    
    err = seL4_CNode_SaveCaller(c->root_cnode, 
                                slot, 
                                CSPACE_DEPTH);
    sel4_error(err, "Saving reply cap");
    
    return slot;
}
seL4_CPtr cspace_irq_control_get_cap(cspace_t *dest, 
                                     seL4_IRQControl irq_cap, 
                                     int irq)
{
    seL4_CPtr slot;
    seL4_Error err;
    slot = cspace_alloc_slot(dest);
    assert(slot != CSPACE_NULL);
    
    err = seL4_IRQControl_Get(irq_cap,irq,
                              dest->root_cnode, 
                              slot, 
                              CSPACE_DEPTH);
    sel4_error(err, "Getting an IRQ control cap");
    
    return slot;
    
}
