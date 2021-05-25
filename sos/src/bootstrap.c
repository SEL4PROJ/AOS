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
/* This file bootstraps the system, setting up a cspace and the ut allocator which can
   then be used by the initial task to create more objects */

#include <stdlib.h>
#include <stdio.h>

#include <autoconf.h>
#include <utils/util.h>
#include <cspace/cspace.h>
#include <aos/sel4_zf_logif.h>

#include "mapping.h"
#include "bootstrap.h"
#include "ut.h"
#include "dma.h"
#include "vmem_layout.h"

/* extra cspace info for the initial bootstrapped cspace */
typedef struct {
    /* track the next free vaddr we have for mapping in frames
     * that the cspace uses to track its own data */
    volatile uintptr_t next_free_vaddr;
    /* the vspace used to map those frames */
    seL4_CPtr vspace;
} bootstrap_cspace_t;

/* statically allocated data for initial cspace */
static bootstrap_cspace_t bootstrap_data;
static bot_lvl_node_t *bot_lvl_nodes[INITIAL_TASK_CSPACE_SLOTS / BOT_LVL_PER_NODE + 1];
static unsigned long top_bf[BITFIELD_SIZE(INITIAL_TASK_CNODE_SIZE_BITS)];
/* track the amount of bootinfo untyped we have stolen for bootstrapping,
 * indexed is offest by bootinfo->untyped.start. For example, bootinfo->untyped.start + 1
 * has the amount of bytes available tracked in index[1]. */
static size_t boot_info_avail_bytes[CONFIG_MAX_NUM_BOOTINFO_UNTYPED_CAPS];

/* calculate the number of ut caps, and initialise boot_info_avail_bytes */
static size_t calculate_ut_caps(const seL4_BootInfo *bi, size_t size_bits)
{
    size_t n_caps = 0;
    for (size_t i = 0; i < bi->untyped.end - bi->untyped.start; i++) {
        if (!untyped_in_range(bi->untypedList[i])) {
            continue;
        }

        if (!bi->untypedList[i].isDevice) {
            boot_info_avail_bytes[i] = BIT(bi->untypedList[i].sizeBits);
        } else {
            boot_info_avail_bytes[i] = 0;
        }

        if (bi->untypedList[i].sizeBits >= size_bits) {
            n_caps += BIT(bi->untypedList[i].sizeBits - size_bits);
        } else {
            ZF_LOGW("Untyped too small to break into size_bits");
        }
    }
    return n_caps;
}

uintptr_t paddr_from_avail_bytes(const seL4_BootInfo *bi, int i, size_t size_bits)
{
    /* how much has been stolen */
    size_t taken = 0;
    if (!bi->untypedList[i].isDevice) {
        taken = BIT(bi->untypedList[i].sizeBits) - boot_info_avail_bytes[i];
    }
    /* round up to the size we want to allocate from this untyped */
    taken = ROUND_UP(taken, BIT(size_bits));
    return bi->untypedList[i].paddr + taken;

}

/* Find the first untyped that will fit a specific size in bytes which is not a device. */
static seL4_CPtr steal_untyped(const seL4_BootInfo *bi, size_t size_bits, uintptr_t *paddr)
{
    /* don't try to untype beyond a 4k boundary, or our future reyptes of 4k objects will fail */
    assert(size_bits >= seL4_PageBits);
    assert(size_bits <= seL4_MaxUntypedBits);

    ZF_LOGD("looking for untyped %zu in size", size_bits);
    for (size_t i = 0; i < bi->untyped.end - bi->untyped.start; i++) {
        if (untyped_in_range(bi->untypedList[i]) && boot_info_avail_bytes[i] >= BIT(size_bits)) {
            if (paddr) {
                *paddr = paddr_from_avail_bytes(bi, i, size_bits);
            }
            /* mark the bytes as unavailable */
            boot_info_avail_bytes[i] -= BIT(size_bits);
            return i + bi->untyped.start;
        }
    }
    return seL4_CapNull;
}

/* scan the available untypeds and return the highest and lowest physical addresses */
static ut_region_t find_memory_bounds(const seL4_BootInfo *bi)
{
    ut_region_t memory = {
        .start = PHYSICAL_ADDRESS_LIMIT, /* highest possible physical address */
        .end = 0 /* lowest possible physical address */
    };

    for (size_t i = 0; i <  bi->untyped.end - bi->untyped.start; i++) {
        if (!untyped_in_range(bi->untypedList[i])) {
            continue;
        }

        memory.start = MIN(memory.start, bi->untypedList[i].paddr);
        seL4_Word end = bi->untypedList[i].paddr + BIT(bi->untypedList[i].sizeBits);
        ZF_LOGD("Found untyped %p <--> %p (%s, %u bits)", (void*)bi->untypedList[i].paddr, (void*)end, (bi->untypedList[i].isDevice) ? "device" : "non-device", bi->untypedList[i].sizeBits);
        memory.end = MAX(end, memory.end);
    }

    ZF_LOGD("Found memory %p <--> %p", (void *) memory.start, (void *) memory.end);
    assert(memory.end > memory.start);
    return memory;
}

static inline void *alloc_vaddr(int err)
{
    void *res = NULL;
    if (!err) {
        res = (void *) bootstrap_data.next_free_vaddr;
        bootstrap_data.next_free_vaddr += PAGE_SIZE_4K;
    }
    return res;
}

void *bootstrap_map_frame(cspace_t *cspace, seL4_CPtr cap)
{
    int err = map_frame(cspace, cap, bootstrap_data.vspace, bootstrap_data.next_free_vaddr,
                        seL4_AllRights, seL4_ARM_Default_VMAttributes);
    return alloc_vaddr(err);
}

/* cspace allocation functions for the bootstrapped cspace */
void *bootstrap_cspace_map_frame(void *cookie, seL4_CPtr cap, seL4_CPtr free_slots[MAPPING_SLOTS],
                                 seL4_Word *used)
{
    seL4_Error err = map_frame_cspace((cspace_t *) cookie, cap, bootstrap_data.vspace, bootstrap_data.next_free_vaddr,
                                      seL4_AllRights, seL4_ARM_Default_VMAttributes, free_slots, used);
    return alloc_vaddr(err);
}


void *bootstrap_cspace_alloc_4k_ut(UNUSED void *cookie, seL4_CPtr *cap)
{
    ut_t *untyped = ut_alloc_4k_untyped(NULL);
    *cap = untyped->cap;
    return untyped;
}

void bootstrap_cspace_free_4k_ut(UNUSED void *cookie, void *untyped)
{
    ut_free(untyped);
}

void sos_bootstrap(cspace_t *cspace, const seL4_BootInfo *bi)
{
    /* zero the struct we were provided */
    memset(cspace, 0, sizeof(cspace_t));
    memset(&bootstrap_data, 0, sizeof(bootstrap_cspace_t));

    /* this cspace is bootstrapping itself */
    cspace->bootstrap = NULL;

    seL4_Word err;

    /* use three slots from the current boot cspace */
    assert((bi->empty.end - bi->empty.start) >= 2);
    seL4_CPtr level1_cptr = bi->empty.start;
    /* We will temporarily store the boot cptr here, and remove it before we finish */
    seL4_CPtr boot_cptr = 0;

    /* work out the number of slots used by the cspace we are provided on on init */
    size_t n_slots = bi->empty.start - 1;

    /* we need enough memory to create and map the ut table - first all the frames */
    ut_region_t memory = find_memory_bounds(bi);
    size_t ut_pages = ut_pages_for_region(memory);
    ZF_LOGD("Need %zu pages for ut table", ut_pages);
    n_slots += ut_pages;
    /* track how much memory we need here */
    size_t size = (ut_pages) * PAGE_SIZE_4K;

    /* account for the number of page tables we need - plus a buffer of 1 */
    size_t n_pts = (ut_pages >> seL4_PageTableIndexBits) + 1;
    size += (n_pts * BIT(seL4_PageTableBits));
    n_slots += n_pts;

    /* and the other paging structures */
    size += (BIT(seL4_PUDBits));
    size += (BIT(seL4_PageDirBits));
    n_slots += 2;

    /* 1 cptr for dma */
    n_slots++;

    /* now work out the number of slots required to retype the untyped memory provided by
     * boot info into 4K untyped objects. We aren't going to initialise these objects yet,
     * but before we have bootstrapped the frame table we cannot allocate memory from it --
     * to avoid this circular dependency we create enough cnodes here to cover our initial
     * requirements, up until the frame table is created*/
    n_slots += calculate_ut_caps(bi, seL4_PageBits);

    /* subtract what we don't need for dma */
    n_slots -= BIT(SOS_DMA_SIZE_BITS - seL4_PageBits);

    /* now work out how many 2nd level nodes are required - with a buffer */
    size_t n_cnodes = n_slots / CNODE_SLOTS(CNODE_SIZE_BITS) + 2;
    ZF_LOGD("%zu slots needed, %zu cnodes", n_slots, n_cnodes);
    size += (n_cnodes * BIT(CNODE_SIZE_BITS)) + BIT(INITIAL_TASK_CNODE_SIZE_BITS);

    /* check our cnodes will fit into our top level cnode */
    ZF_LOGF_IF(n_cnodes > CNODE_SLOTS(INITIAL_TASK_CNODE_SIZE_BITS), "Insufficient slots %lu for"
               "bottom level cnodes %lu", CNODE_SLOTS(INITIAL_TASK_CNODE_SIZE_BITS), n_cnodes);

    /* now we have worked out how much memory we need to set up the system -
     * steal some memory from an untyped big enough to allocate it all from */
    seL4_CPtr ut_cptr = steal_untyped(bi, BYTES_TO_SIZE_BITS(size) + 1, NULL);
    ZF_LOGF_IF(ut_cptr == seL4_CapNull, "Could not find memory to boostrap cspace");

    /* The root cnode cap will end up here once we are done bootstrapping the cspace */
    cspace->root_cnode = seL4_CapInitThreadCNode;
    /* create the new level 1 cnode from the untyped we found */
    err = seL4_Untyped_Retype(ut_cptr, seL4_CapTableObject, CNODE_SLOT_BITS(INITIAL_TASK_CNODE_SIZE_BITS),
                              seL4_CapInitThreadCNode, 0, 0, level1_cptr, 1);
    ZF_LOGF_IFERR(err, "Allocating new root cnode");

    /* now create the 2nd level nodes, directly in the node we just created */
    size_t chunk = 0;
    for (size_t total = n_cnodes; total > 0; total -= chunk) {
        chunk = MIN((size_t) CONFIG_RETYPE_FAN_OUT_LIMIT, total);
        err = seL4_Untyped_Retype(ut_cptr, seL4_CapTableObject, CNODE_SLOT_BITS(CNODE_SIZE_BITS),
                                  level1_cptr, 0, 0, n_cnodes - total, chunk);
        ZF_LOGF_IFERR(err, "Failed to allocate 2nd level cnodes");
    }
    seL4_Word depth = CNODE_SLOT_BITS(INITIAL_TASK_CNODE_SIZE_BITS) +
                      CNODE_SLOT_BITS(CNODE_SIZE_BITS);

    /* copy the old root cnode to cptr 0 in the new cspace */
    err = seL4_CNode_Copy(level1_cptr, boot_cptr, depth,
                          seL4_CapInitThreadCNode, seL4_CapInitThreadCNode, seL4_WordBits, seL4_AllRights);
    ZF_LOGF_IFERR(err, "Making copy of root task's initial cnode cap");

    /* mint a cap to our new cnode at seL4_CapInitThreadCnode in the new cspace with the correct guard */
    seL4_Word guard = seL4_CNode_CapData_new(0, seL4_WordBits - depth).words[0];
    err = seL4_CNode_Mint(level1_cptr, seL4_CapInitThreadCNode, depth,
                          seL4_CapInitThreadCNode, level1_cptr, seL4_WordBits, seL4_AllRights, guard);
    ZF_LOGF_IFERR(err, "Making new cap to new cspace");

    /* Switch from BOOT CSPACE -> NEW CSPACE by switching current threads cnode */
    err = seL4_TCB_SetSpace(seL4_CapInitThreadTCB, 0, level1_cptr, guard,
                            seL4_CapInitThreadVSpace, seL4_NilData);
    ZF_LOGF_IFERR(err, "Replacing intial cnode with new cspace");

    /* Now seL4_CapInitThreadCNode refers to the new cspace, the old boot cspace is boot_cptr.
     *
     * Copy all the caps in the original boot cspace (except for the one already done), i.e. from 1
     * to the first empty cap in the original boot cspace.
     */
    for (seL4_CPtr i = 1; i < bi->empty.start; i++) {
        switch (i) {
        /* skip the initial cspace -> we already dealt with it */
        case seL4_CapInitThreadCNode:
        /* these don't exist in our platform, skip them too */
        case seL4_CapIOPortControl:
        case seL4_CapIOSpace:
        case seL4_CapSMMUSIDControl:
        case seL4_CapSMMUCBControl:
            continue;
        }
        ZF_LOGV("cspace: moving cap %lu boot -> new cspace", i);
        err = seL4_CNode_Move(seL4_CapInitThreadCNode,
                              i,
                              seL4_WordBits,
                              boot_cptr, /* src */
                              i,
                              seL4_WordBits);
        ZF_LOGF_IFERR(err, "Copying initial initial cnode cap %p to new cspace", (void *) i);
    }

    /* Remove the original cnode -- it's empty and we need slot 0 to be free as it acts
     * as the NULL capability and should be empty, or any invocation of seL4_CapNull will
     * invoke this cnode. */
    err = seL4_CNode_Delete(seL4_CapInitThreadCNode,
                            boot_cptr,
                            seL4_WordBits);
    ZF_LOGF_IFERR(err, "Deleting root task's original cnode cap");

    /* Next, allocate and map enough paging structures and frames to create the
     * untyped table */

    /* set the levels to 2 so we can use cspace_untyped_retype */
    cspace->two_level = true;

    /* allocate the PUD */
    seL4_CPtr first_free_slot = bi->empty.start;
    err = cspace_untyped_retype(cspace, ut_cptr, first_free_slot, seL4_ARM_PageUpperDirectoryObject, seL4_PageBits);
    ZF_LOGF_IFERR(err, "Failed to create page upper directory");

    /* map it */
    err = seL4_ARM_PageUpperDirectory_Map(first_free_slot, seL4_CapInitThreadVSpace, SOS_UT_TABLE,
                                          seL4_ARM_Default_VMAttributes);
    ZF_LOGF_IFERR(err, "Failed to map page upper directory");
    first_free_slot++;

    /* then the page dir */
    err = cspace_untyped_retype(cspace, ut_cptr, first_free_slot, seL4_ARM_PageDirectoryObject, seL4_PageBits);
    ZF_LOGF_IFERR(err, "Failed to create page directory");

    err = seL4_ARM_PageDirectory_Map(first_free_slot, seL4_CapInitThreadVSpace, SOS_UT_TABLE,
                                     seL4_ARM_Default_VMAttributes);
    ZF_LOGF_IFERR(err, "Failed to map page directory");
    first_free_slot++;

    /* then the page tables */
    for (size_t i = 0; i < (ut_pages >> seL4_PageTableIndexBits) + 1; i++) {
        err = cspace_untyped_retype(cspace, ut_cptr, first_free_slot, seL4_ARM_PageTableObject, seL4_PageBits);
        ZF_LOGF_IFERR(err, "Failed to create page table");

        seL4_Word vaddr = SOS_UT_TABLE + i * (BIT(seL4_PageTableIndexBits + seL4_PageBits));
        ZF_LOGV("Mapping page table at %p", (void *) vaddr);
        err = seL4_ARM_PageTable_Map(first_free_slot, seL4_CapInitThreadVSpace, vaddr,
                                     seL4_ARM_Default_VMAttributes);
        ZF_LOGF_IFERR(err, "Failed to map page table at %p", (void *) vaddr);
        first_free_slot++;
    }

    bootstrap_data.vspace = seL4_CapInitThreadVSpace;
    bootstrap_data.next_free_vaddr = SOS_UT_TABLE;

    /* then the pages to cover the ut table */
    size_t slots_per_cnode = CNODE_SLOTS(CNODE_SIZE_BITS);
    for (size_t i = 0; i < ut_pages; i++) {
        /* allocate page */
        err = cspace_untyped_retype(cspace, ut_cptr, first_free_slot, seL4_ARM_SmallPageObject, seL4_PageBits);
        ZF_LOGF_IFERR(err, "Failed to allocate page for ut table");

        err = seL4_ARM_Page_Map(first_free_slot, seL4_CapInitThreadVSpace, bootstrap_data.next_free_vaddr,
                                seL4_AllRights, seL4_ARM_Default_VMAttributes);
        ZF_LOGF_IFERR(err, "Failed to map page at %p", (void *) bootstrap_data.next_free_vaddr);
        first_free_slot++;
        bootstrap_data.next_free_vaddr += PAGE_SIZE_4K;
    }

    /* before we add all the 4k untypeds to the ut table, steal some for DMA */
    uintptr_t dma_paddr;
    seL4_CPtr dma_ut = steal_untyped(bi, SOS_DMA_SIZE_BITS, &dma_paddr);
    ZF_LOGF_IF(dma_ut == seL4_CapNull, "Could not find DMA memory");

    err = cspace_untyped_retype(cspace, dma_ut, first_free_slot, seL4_ARM_LargePageObject, SOS_DMA_SIZE_BITS);
    ZF_LOGF_IFERR(err, "Failed to retype dma untyped");
    seL4_CPtr dma_cptr = first_free_slot;
    first_free_slot++;

    /* initialise the ut table */
    ut_init((void *) SOS_UT_TABLE, memory);

    /* create all the 4K untypeds and build the ut table, from the first available empty slot */
    for (size_t i = 0; i < bi->untyped.end - bi->untyped.start; i++) {
        if (!untyped_in_range(bi->untypedList[i])) {
            continue;
        }

        size_t n_caps = BIT(bi->untypedList[i].sizeBits) / PAGE_SIZE_4K;
        if (!bi->untypedList[i].isDevice) {
            n_caps = boot_info_avail_bytes[i] / PAGE_SIZE_4K;
        }
        seL4_Word paddr = paddr_from_avail_bytes(bi, i, seL4_PageBits);
        if (n_caps > 0) {
            ZF_LOGD("Creating %zu 4KiB untyped capabilities at %p", n_caps, (void *)paddr);
            ut_add_untyped_range(paddr, first_free_slot, n_caps, bi->untypedList[i].isDevice);
        }
        while (n_caps > 0) {
            seL4_CPtr cnode = first_free_slot / slots_per_cnode;

            /* we can only retype the amount that will fit in a 2nd lvl cnode */
            int retype = MIN((size_t) CONFIG_RETYPE_FAN_OUT_LIMIT, MIN(n_caps,
                                                                       slots_per_cnode - (first_free_slot % slots_per_cnode)));
            err = seL4_Untyped_Retype(bi->untyped.start + i, seL4_UntypedObject, seL4_PageBits,
                                      seL4_CapInitThreadCNode, cnode,
                                      seL4_WordBits - CNODE_SLOT_BITS(CNODE_SIZE_BITS),
                                      first_free_slot % slots_per_cnode, retype);
            first_free_slot += retype;
            n_caps -= retype;
            ZF_LOGF_IFERR(err, "Failed retype untyped");
        }
    }

    /* finally, finish setting up the cspace */
    cspace->top_lvl_size_bits = INITIAL_TASK_CNODE_SIZE_BITS;
    cspace->top_bf = top_bf;
    cspace->n_bot_lvl_nodes = 0;
    cspace->bot_lvl_nodes = bot_lvl_nodes;
    cspace->alloc = (cspace_alloc_t) {
        .map_frame = bootstrap_cspace_map_frame,
        .alloc_4k_ut = bootstrap_cspace_alloc_4k_ut,
        .free_4k_ut = bootstrap_cspace_free_4k_ut,
        .cookie = cspace
    };

    /* allocate and map enough frames to track the bottom levels nodes required */
    size_t n_bot_lvl = MAX((first_free_slot / slots_per_cnode + 1), n_cnodes) / BOT_LVL_PER_NODE + 1;
    for (size_t i = 0; i < n_bot_lvl; i++) {
        ZF_LOGD("Allocating node %zu for cspace book keeping", i);
        seL4_Word paddr;
        ut_t *ut = ut_alloc_4k_untyped(&paddr);
        err = cspace_untyped_retype(cspace, ut->cap, first_free_slot,
                                    seL4_ARM_SmallPageObject, seL4_PageBits);
        ZF_LOGF_IFERR(err, "Failed to retype initial cspace frame");

        err = seL4_ARM_Page_Map(first_free_slot, seL4_CapInitThreadVSpace, bootstrap_data.next_free_vaddr,
                                seL4_AllRights, seL4_ARM_Default_VMAttributes);
        ZF_LOGF_IFERR(err, "Failed to map page at %p", (void *) bootstrap_data.next_free_vaddr);
        cspace->bot_lvl_nodes[i] = (void *) bootstrap_data.next_free_vaddr;
        bootstrap_data.next_free_vaddr += PAGE_SIZE_4K;

        memset(cspace->bot_lvl_nodes[i], 0, PAGE_SIZE_4K);
        cspace->bot_lvl_nodes[i]->untyped = ut;
        cspace->bot_lvl_nodes[i]->frame = first_free_slot;
        cspace->n_bot_lvl_nodes++;
        first_free_slot++;
    }

    /* we're done mapping things for the cspace, now place the dma region at the next large page boundary */
    uintptr_t dma_vaddr = ALIGN_UP(bootstrap_data.next_free_vaddr + PAGE_SIZE_4K, BIT(seL4_LargePageBits));
    err = dma_init(cspace, seL4_CapInitThreadVSpace, dma_cptr, dma_paddr, dma_vaddr);
    ZF_LOGF_IF(err, "Failed to initialise DMA");
    bootstrap_data.next_free_vaddr = dma_vaddr + BIT(seL4_LargePageBits) + PAGE_SIZE_4K;

    /* now record all the cptrs we have already used to bootstrap */
    for (seL4_CPtr i = 0; i < ALIGN_DOWN(first_free_slot, slots_per_cnode); i += slots_per_cnode) {
        /* we just allocated all these, should be not be NULL */
        bot_lvl_node_t *bot_lvl_node = cspace->bot_lvl_nodes[NODE_INDEX(i)];
        assert(bot_lvl_node != NULL);
        bot_lvl_node->n_cnodes++;
        for (size_t i = 0; i < CNODE_SLOTS(CNODE_SIZE_BITS) / seL4_WordBits; i++) {
            bot_lvl_node->cnodes[CNODE_INDEX(i)].bf[i] = UINTPTR_MAX;
        }
        /* this cnode is full */
        bf_set_bit(cspace->top_bf, TOP_LVL_INDEX(i));
    }

    /* now update the partially full cnode */
    bot_lvl_node_t *bot_lvl_node = cspace->bot_lvl_nodes[first_free_slot / slots_per_cnode / BOT_LVL_PER_NODE];
    bot_lvl_node->n_cnodes++;

    for (seL4_CPtr i = ALIGN_DOWN(first_free_slot, slots_per_cnode); i < first_free_slot; i++) {
        bf_set_bit(bot_lvl_node->cnodes[CNODE_INDEX(i)].bf, BOT_LVL_INDEX(i));
    }

    /* mark any extra cnodes we created as allocated - this occurs as we over
     * estimate when considering the initial untyped */
    for (size_t i = (first_free_slot / slots_per_cnode + 1); i < n_cnodes; i++) {
        cspace->bot_lvl_nodes[i / BOT_LVL_PER_NODE]->n_cnodes++;
    }

    /* finally allocate the watermark */
    for (size_t i = 0; i < WATERMARK_SLOTS; i++) {
        cspace->watermark[i] = cspace_alloc_slot(cspace);
        ZF_LOGF_IF(cspace->watermark[i] == seL4_CapNull, "Failed to allocate watermark cslot");
    }

    ZF_LOGD("cspace: root tasks cspace bootstrapped");
}

