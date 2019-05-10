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
#include <sel4/sel4.h>
#include <sel4/sel4_arch/mapping.h>

#include "mapping.h"
#include "ut.h"
#include "vmem_layout.h"

/**
 * Retypes and maps a page table into the root servers page global directory
 * @param cspace that the cptrs refer to
 * @param vaddr  the virtual address of the mapping
 * @param ut     a 4k untyped object
 * @param empty  an empty slot to retype into a pt
 * @return 0 on success
 */
static seL4_Error retype_map_pt(cspace_t *cspace, seL4_CPtr vspace, seL4_Word vaddr, seL4_CPtr ut, seL4_CPtr empty)
{

    seL4_Error err = cspace_untyped_retype(cspace, ut, empty, seL4_ARM_PageTableObject, seL4_PageBits);
    if (err) {
        return err;
    }

    return seL4_ARM_PageTable_Map(empty, vspace, vaddr, seL4_ARM_Default_VMAttributes);
}

/**
 * Retypes and maps a page directory into the root servers page global directory
 * @param cspace that the cptrs refer to
 * @param vaddr  the virtual address of the mapping
 * @param ut     a 4k untyped object
 * @param empty  an empty slot to retype into a pd
 * @return 0 on success
 */
static seL4_Error retype_map_pd(cspace_t *cspace, seL4_CPtr vspace, seL4_Word vaddr, seL4_CPtr ut, seL4_CPtr empty)
{

    seL4_Error err = cspace_untyped_retype(cspace, ut, empty, seL4_ARM_PageDirectoryObject, seL4_PageBits);
    if (err) {
        return err;
    }

    return seL4_ARM_PageDirectory_Map(empty, vspace, vaddr, seL4_ARM_Default_VMAttributes);
}

/**
 * Retypes and maps a page upper directory into the root servers page global directory
 * @param cspace that the cptrs refer to
 * @param vaddr  the virtual address of the mapping
 * @param ut     a 4k untyped object
 * @param empty  an empty slot to retype into a pud
 * @return 0 on success
 */
static seL4_Error retype_map_pud(cspace_t *cspace, seL4_CPtr vspace, seL4_Word vaddr, seL4_CPtr ut,
                                 seL4_CPtr empty)
{

    seL4_Error err = cspace_untyped_retype(cspace, ut, empty, seL4_ARM_PageUpperDirectoryObject, seL4_PageBits);
    if (err) {
        return err;
    }
    return seL4_ARM_PageUpperDirectory_Map(empty, vspace, vaddr, seL4_ARM_Default_VMAttributes);
}

static seL4_Error map_frame_impl(cspace_t *cspace, seL4_CPtr frame_cap, seL4_CPtr vspace, seL4_Word vaddr,
                                 seL4_CapRights_t rights, seL4_ARM_VMAttributes attr,
                                 seL4_CPtr *free_slots, seL4_Word *used)
{
    /* Attempt the mapping */
    seL4_Error err = seL4_ARM_Page_Map(frame_cap, vspace, vaddr, rights, attr);
    for (size_t i = 0; i < MAPPING_SLOTS && err == seL4_FailedLookup; i++) {
        /* save this so nothing else trashes the message register value */
        seL4_Word failed = seL4_MappingFailedLookupLevel();

        /* Assume the error was because we are missing a paging structure */
        ut_t *ut = ut_alloc_4k_untyped(NULL);
        if (ut == NULL) {
            ZF_LOGE("Out of 4k untyped");
            return -1;
        }

        /* figure out which cptr to use to retype into*/
        seL4_CPtr slot;
        if (used != NULL) {
            slot = free_slots[i];
            *used |= BIT(i);
        } else {
            slot = cspace_alloc_slot(cspace);
        }

        if (slot == seL4_CapNull) {
            ZF_LOGE("No cptr to alloc paging structure");
            return -1;
        }

        switch (failed) {
        case SEL4_MAPPING_LOOKUP_NO_PT:
            err = retype_map_pt(cspace, vspace, vaddr, ut->cap, slot);
            break;
        case SEL4_MAPPING_LOOKUP_NO_PD:
            err = retype_map_pd(cspace, vspace, vaddr, ut->cap, slot);
            break;

        case SEL4_MAPPING_LOOKUP_NO_PUD:
            err = retype_map_pud(cspace, vspace, vaddr, ut->cap, slot);
            break;
        }

        if (!err) {
            /* Try the mapping again */
            err = seL4_ARM_Page_Map(frame_cap, vspace, vaddr, rights, attr);
        }
    }

    return err;
}

seL4_Error map_frame_cspace(cspace_t *cspace, seL4_CPtr frame_cap, seL4_CPtr vspace, seL4_Word vaddr,
                            seL4_CapRights_t rights, seL4_ARM_VMAttributes attr,
                            seL4_CPtr free_slots[MAPPING_SLOTS], seL4_Word *used)
{
    if (cspace == NULL) {
        ZF_LOGE("Invalid arguments");
        return -1;
    }
    return map_frame_impl(cspace, frame_cap, vspace, vaddr, rights, attr, free_slots, used);
}

seL4_Error map_frame(cspace_t *cspace, seL4_CPtr frame_cap, seL4_CPtr vspace, seL4_Word vaddr,
                     seL4_CapRights_t rights, seL4_ARM_VMAttributes attr)
{
    return map_frame_impl(cspace, frame_cap, vspace, vaddr, rights, attr, NULL, NULL);
}


static uintptr_t device_virt = SOS_DEVICE_START;

void *sos_map_device(cspace_t *cspace, uintptr_t addr, size_t size)
{
    assert(cspace != NULL);
    void *vstart = (void *) device_virt;

    for (uintptr_t curr = addr; curr < (addr + size); curr += PAGE_SIZE_4K) {
        ut_t *ut = ut_alloc_4k_device(curr);
        if (ut == NULL) {
            ZF_LOGE("Failed to find ut for phys address %p", (void *) curr);
            return NULL;
        }

        /* allocate a slot to retype into */
        seL4_CPtr frame = cspace_alloc_slot(cspace);
        if (frame == seL4_CapNull) {
            ZF_LOGE("Out of caps");
            return NULL;
        }

        /* retype */
        seL4_Error err = cspace_untyped_retype(cspace, ut->cap, frame, seL4_ARM_SmallPageObject,
                                               seL4_PageBits);
        if (err != seL4_NoError) {
            ZF_LOGE("Failed to retype %lx", (seL4_CPtr)ut->cap);
            cspace_free_slot(cspace, frame);
            return NULL;
        }

        /* map */
        err = map_frame(cspace, frame, seL4_CapInitThreadVSpace, device_virt, seL4_AllRights, false);
        if (err != seL4_NoError) {
            ZF_LOGE("Failed to map device frame at %p", (void *) device_virt);
            cspace_delete(cspace, frame);
            cspace_free_slot(cspace, frame);
            return NULL;
        }

        device_virt += PAGE_SIZE_4K;
    }

    return vstart;
}
