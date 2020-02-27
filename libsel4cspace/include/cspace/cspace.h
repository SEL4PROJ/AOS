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
/**
 * @file   cspace.h
 *
 * @brief  A Cspace management library
 *
 * Specifically, the library allocates and de-allocates cnodes as required and combines them to form
 * a capability address space indexable by a seL4_CPtr. The library supports 1-level and 2-level
 * cspaces in a in an analogous way to 1-level and 2-level page tables. However, except for the
 * maximum potential cspace index, the underlying number of levels is hidden from the interface.
 *
 * The library also provides free slot management in the cspace, and wrappers for convenient
 * access to seL4_CNode_* functions, seL4_Untyped_Retype, and seL4_IRQHandler_Get
 */

#pragma once

#include <utils/util.h>
#include <stdint.h>
#include <sel4/sel4.h>
#include <aos/strerror.h>
#include <stdlib.h>
#include <cspace/bitfield.h>

/* the cspace needs to keep a set of free slots to ensure it can allocate further structures
 * when we run out of 2nd level cnodes. 3 for potential mapping structures, 1 for a bookkeeping frame */
#define MAPPING_SLOTS 3u
#define WATERMARK_SLOTS (MAPPING_SLOTS + 1)

/* All cnodes created by this library are exactly this size, where size = 2^size_bits */
#define CNODE_SIZE_BITS 12u
/* convert from size bits in memory, to number of slots in size_bits, where the number of slots is 2^n */
#define CNODE_SLOT_BITS(x) (x - seL4_SlotBits)
/* convert from cnode size bits to number of slots */
#define CNODE_SLOTS(x) (BIT(CNODE_SLOT_BITS(x)))

/* calculate the bf size in words */
#define BITFIELD_SIZE(x) (CNODE_SLOTS(x) / seL4_WordBits)

#define BOT_LVL_NODES(x) BYTES_TO_4K_PAGES(sizeof(bot_lvl_t) * CNODE_SLOTS(x))

/* macros for 2nd level cspace */

/* get the bottom level node index from a cptr */
#define NODE_INDEX(cptr) ((cptr) / (CNODE_SLOTS(CNODE_SIZE_BITS)) / BOT_LVL_PER_NODE)
/* get the cnode index from within a bottom level node from a cptr */
#define CNODE_INDEX(cptr) ((cptr) / (CNODE_SLOTS(CNODE_SIZE_BITS)) % BOT_LVL_PER_NODE)
/* get the top level bf index from a cptr */
#define TOP_LVL_INDEX(cptr) ((cptr) >> CNODE_SLOT_BITS(CNODE_SIZE_BITS))
/* get the bottom level bf index from a cptr */
#define BOT_LVL_INDEX(cptr) ((cptr) & MASK(CNODE_SLOT_BITS(CNODE_SIZE_BITS)))

/* tracks info about a bottom level cnode */
typedef struct {
    /* tracks if slots are free / empty in this cnode */
    unsigned long bf[BITFIELD_SIZE(CNODE_SIZE_BITS)];
    /* handle to the 4k untyped used for this cnode */
    void *untyped;
} PACKED bot_lvl_t;

/* the amount of bottom level cnode data that will fit into a 4K page */
#define BOT_LVL_PER_NODE ((PAGE_SIZE_4K - sizeof(seL4_Word) * 3) / sizeof(bot_lvl_t))
typedef struct {
    /* number of cnodes allocated in this bookkeeping node */
    seL4_Word n_cnodes;
    /* handle to the 4k untyped used to allocate this bookkeeping frame */
    void *untyped;
    /* cptr to the frame */
    seL4_CPtr frame;
    /* array of bottom level cnodes that this node contains book-keeping for */
    bot_lvl_t cnodes[BOT_LVL_PER_NODE];
} ALIGN(PAGE_SIZE_4K) PACKED  bot_lvl_node_t ;
/* check the size of bot_lvl_node_t is correct */
static_assert(sizeof(bot_lvl_node_t) <= PAGE_SIZE_4K, "bot level node size is correct");

/*
 Map a 4K frame capability, and return the mapped address.

 When calling this function the cspace *must* provide MAPPING_SLOTS free slots
 in which the mapping function can retype intermediate paging structures.
 See mapping.h for further explanation.

 This function is only used in 2 level cspaces.
*/
typedef void *(*cspace_map_frame_fn)(void *cookie, seL4_CPtr frame, seL4_CPtr free_slots[], seL4_Word *used);
/*
 Allocate a 4k untyped object. Return a handle that can be used to free the
 object, and the cptr to the object capability, in *cap.
 */
typedef void *(*cspace_alloc_4k_ut_fn)(void *cookie, seL4_CPtr *cap);
/*
 Free a previously allocated 4k untyped object
*/
typedef void (*cspace_free_4k_ut_fn)(void *cookie, void *untyped);

/* cspace allocation functions, which are passed on cspace create to allow
   a cspace to allocate its own resources */
typedef struct {
    cspace_map_frame_fn map_frame;
    cspace_alloc_4k_ut_fn alloc_4k_ut;
    cspace_free_4k_ut_fn free_4k_ut;
    void *cookie;
} cspace_alloc_t;

/**
 * A representation of a cspace.
 */
typedef struct cspace cspace_t;
struct cspace {
    /* A seL4_CPtr to the root cnode of this cspace */
    seL4_CPtr root_cnode;
    /* cspace is either 1 or 2 level */
    bool two_level;
    /* size (memory) of the top level cnode */
    int top_lvl_size_bits;

    /* This field is treated differently depending on the number of cspace levels.
     *
     * For a two level cspace, each set bit marks a full 2nd level cnode.
     * For a one level cspace, each set bit marks a taken slot.
     */
    unsigned long *top_bf;

    /* NULL for one level cspaces, otherwise 2nd level book keeping nodes */
    bot_lvl_node_t **bot_lvl_nodes;

    /* track how many bottom level accounting nodes have been added */
    int n_bot_lvl_nodes;

    /* The untyped object which the root cnode was retyped from. */
    void *untyped;

    /* the cspace used to bootstrap this one (NULL if this is the initial tasks cspace) */
    cspace_t *bootstrap;

    /* functions and data passed to the cspace on creation which allow it to allocate
     * and map untyped and frames */
    cspace_alloc_t alloc;

    /* backup slots to use if we need to map frames for cspace bookkeeping */
    seL4_CPtr watermark[WATERMARK_SLOTS];
};

typedef enum {
    CSPACE_NOERROR = 0,
    CSPACE_ERROR = -1
} cspace_err_t;

/**
 * Allocate a new two-level cspace.
 *
 * A two level cspace has 1 << ((seL4_PageBits - seL4_SlotBits) * 2) available slots.
 *
 * @param bootstrap cspace used to create the new cspace
 * @param target    cspace to initialise
 * @param alloc     interface used to allocate frames for cspace bookeeping.
 * @return cspace_err_t
 */
int cspace_create_two_level(cspace_t *bootstrap, cspace_t *target, cspace_alloc_t cspace_alloc);

/**
 * Allocate a new one-level cspace.
 *
 * A one level cspace has 1 << (seL4_PageBits - seL4_SlotBits) available slots
 *
 * @param bootstrap cspace used to create the new cspace
 * @param target    cspace to initialise
 * @return cspace_err_t
 */
int cspace_create_one_level(cspace_t *bootstrap, cspace_t *target);

/**
 * Destroy the specified cspace and return it's resources. This cannot be called on the
 * initial, root task cspace.
 *
 * @param c The specified cspace
 *
 * The routine deletes all caps in the cspace and returns the untyped memory back to the
 * untyped memory allocator.
 *
 * NOTE: The cnode deletion assumes the internal cnode caps have not been copied elsewhere, so that
 * deleting them here results in free untyped memory. If the cnode caps have been
 * copied, the delete will only not delete the copy and the object will remain present. Thus you will
 * get errors down the track when trying to retype memory that is still in use.
 */
void cspace_destroy(cspace_t *c);

/**
 * Reserve a free slot in the specified cspace.
 *
 * @param c Specified cspace
 *
 * @return CSPACE_NULL on error.
 *
 * This function reserves a slot in the specified cspace that can be used directly with seL4
 * operations. The allocated slot is at depth seL4_WordBits.
 */
seL4_CPtr cspace_alloc_slot(cspace_t *c);

/**
 * Return an empty slot back to the cspace slot allocator.
 *
 * @param c    The cspace
 * @param slot an empty slot to mark as free.
 * @return Either CSPACE_ERROR or CSPACE_NOERROR
 *
 * The function puts the slot back into the cspace's free slot list. Note: It assumes there is no
 * capability in the slot (i.e. the slot is actually empty) and it does NOT perform a
 * cspace_delete_cap operation.
 */
void cspace_free_slot(cspace_t *c, seL4_CPtr slot);

/* helper functions for cspace operations */

/**
 * Copy a capability from one cspace to the same or another cspace.
 *
 * @param dest      The destination cspace.
 * @param dest_cptr The cptr to a free slot in the destination cspace.
 * @param src       The source cspace.
 * @param src_cptr   A cptr to the source slot in the source cspace.
 * @param rights    Rights that the copied cap should contain. Must be <= the rights on the source cap. Usualy seL4_AllRights.
 * @return seL4_NoError on success.
 */
static inline seL4_Error cspace_copy(cspace_t *dest, seL4_CPtr dest_cptr, cspace_t *src, seL4_CPtr src_cptr,
                                     seL4_CapRights_t rights)
{
    return seL4_CNode_Copy(dest->root_cnode, dest_cptr, seL4_WordBits,
                           src->root_cnode, src_cptr, seL4_WordBits, rights);
}

/**
 * Delete a specified capability. Does not free the cslot.
 *
 * @param cspace The cspace containing the cap.
 * @param cptr   CPtr to the slot to delete the cap from.
 * @return seL4_NoError on success.
 */
static inline seL4_Error cspace_delete(cspace_t *cspace, seL4_CPtr cptr)
{
    return seL4_CNode_Delete(cspace->root_cnode, cptr, seL4_WordBits);
}

/**
 * Mint a new capability into the specified cspace. Mint is like copy, except you can also set
 * the badge on the capability (if it is an endpoint or notification) and the guard on a cnode.
 *
 * @param dest      The destination cspace.
 * @param dest_cptr The cptr to a free slot in the destination cspace.
 * @param src       The source cspace.
 * @param src_cptr   A cptr to the source slot in the source cspace.
 * @param rights    Rights that the copied cap should contain. Must be <= the rights on the source cap. Usualy seL4_AllRights.
 * @param badge     The data for the new capability. A badge in the case of an endpoint or notification cap, or a guard for a cnode cap.
 * @return seL4_NoError on success.
 */
static inline seL4_Error cspace_mint(cspace_t *dest, seL4_CPtr dest_cptr, cspace_t *src, seL4_CPtr src_cptr,
                                     seL4_CapRights_t rights, seL4_Word badge)
{
    return seL4_CNode_Mint(dest->root_cnode, dest_cptr, seL4_WordBits,
                           src->root_cnode, src_cptr, seL4_WordBits, rights, badge);
}

/**
 * Move a cap from one cspace slot to another. After this operation, the slot indicated by
 * src_cptr is empty.
 *
 * @param dest      The destination cspace.
 * @param dest_cptr The cptr to a free slot in the destination cspace.
 * @param src       The source cspace of the capability to move.
 * @param src_cptr   CPtr to the slot containing the capability to move.
 * @return seL4_NoError on success.
 *
 */
static inline seL4_Error cspace_move(cspace_t *dest, seL4_CPtr dest_cptr, cspace_t *src, seL4_CPtr src_cptr)
{
    return seL4_CNode_Move(dest->root_cnode, dest_cptr, seL4_WordBits, src->root_cnode, src_cptr, seL4_WordBits);
}

/**
 * Move a capability and set it's badge in the process. As mint is to copy, mutate is to move.
 *
 * @param dest      The destination cspace.
 * @param dest_cptr The cptr to a free slot in the destination cspace.
 * @param src       The source cspace of the capability to move.
 * @param src_cptr  CPtr to the slot containing the capability to move.
 * @param badge     The data to mutate the capability with. A badge in the case of an endpoint or notification cap, or a guard for a cnode cap.
 * @return seL4_NoError on success.
 */
static inline seL4_Error cspace_mutate(cspace_t *dest, seL4_CPtr dest_cptr, cspace_t *src, seL4_CPtr src_cap,
                                       seL4_Word badge)
{
    return seL4_CNode_Mutate(dest->root_cnode, dest_cptr, seL4_WordBits,
                             src->root_cnode, src_cap, seL4_WordBits, badge);
}

/**
 * Revoke all capabilities derived from the specified capability.
 *
 * @param cspace The cspace.
 * @param cptr   CPtr into the cspace to revoke.
 * @return seL4_NoError on success.
 */
static inline seL4_Error cspace_revoke(cspace_t *cspace, seL4_CPtr cptr)
{
    return seL4_CNode_Revoke(cspace->root_cnode, cptr, seL4_WordBits);

}

#ifndef CONFIG_KERNEL_MCS
/**
 * Move the callers reply capability from the implicit slot in the current TCB's CNode to a slot in the target cspace.
 * The reply capability can then be invoked with seL4_Send to send a reply message, at which point the slot
 * used will become free, as reply capabilities are consumed by invocation.
 *
 * @param cspace cspace to save the cap into.
 * @param cptr   cptr to a slot in the cspace to save the reply capability.
 * @return seL4_NoError on success.
 *
 * This function move the reply capability generated in the process of receiving IPC and moves it
 * from the TCB into the destination cspace.
 */
static inline seL4_Error cspace_save_reply_cap(cspace_t *cspace, seL4_CPtr cptr)
{
    return seL4_CNode_SaveCaller(cspace->root_cnode, cptr, seL4_WordBits);
}
#endif

/**
 * Create an IRQ handler capability in a cptr in the specified cspace.
 *
 * @param dest    The specified cspace.
 * @param cptr    An empty slot in the dest cspace.
 * @param irq_cap The seL4_CPtr of the IRQControl capability in the current cspace.
 * @param irq The hardware IRQ number that you want to handle.
 * @return seL4_NoError on success.
 */
static inline seL4_Error cspace_irq_control_get(cspace_t *dest, seL4_CPtr cptr, seL4_IRQControl irq_cap, int irq,
                                                int level)
{
    return seL4_IRQControl_GetTrigger(irq_cap, irq, level, dest->root_cnode, cptr, seL4_WordBits);
}

/**
 * Create a new kernel object from untyped memory.
 *
 * @param cspace   the cspace that the cptrs refer to.
 * @param ut       a cptr to the untyped object to retype
 * @param target   a cptr to a free slot in the cspace
 * @param type     The type of the seL4 object to create. See the objecttype.h
 *                 files in libsel4 for the types to use.
 * @param size_bits The objects size, where the object size in bytes is 2^size_bits.
 *                  In general, if the object is a seL4_XObject, the size of seL4_XBits,
 *                  except for variably sized objects, like untyped, captables and frames.
 *
 * @return seL4_NoError on success.
 *
 * Note: One should cspace_delete_cap() the cap to objects (and all copies of any caps
 *       made) in order to return the memory used by the object to free untyped memory.
 *
 *       One could also rely on cspace_destroy() to free object, iff there are no
 *       copies of caps to the object outside of the cspace being destroyed.
 */
seL4_Error cspace_untyped_retype(cspace_t *cspace, seL4_CPtr ut, seL4_CPtr target,
                                 seL4_Word type, size_t size_bits);
