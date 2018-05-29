/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "mapping.h"

#include <ut_manager/ut.h>
#include "vmem_layout.h"

#define verbose 0
#include <sys/panic.h>
#include <sys/debug.h>
#include <cspace/cspace.h>

extern const seL4_BootInfo* _boot_info;


/**
 * Maps a page table into the root servers page directory
 * @param vaddr The virtual address of the mapping
 * @return 0 on success
 */
static int 
_map_page_table(seL4_ARM_PageDirectory pd, seL4_Word vaddr){
    seL4_Word pt_addr;
    seL4_ARM_PageTable pt_cap;
    int err;

    /* Allocate a PT object */
    pt_addr = ut_alloc(seL4_PageTableBits);
    if(pt_addr == 0){
        return !0;
    }
    /* Create the frame cap */
    err =  cspace_ut_retype_addr(pt_addr, 
                                 seL4_ARM_PageTableObject,
                                 seL4_PageTableBits,
                                 cur_cspace,
                                 &pt_cap);
    if(err){
        return !0;
    }
    /* Tell seL4 to map the PT in for us */
    err = seL4_ARM_PageTable_Map(pt_cap, 
                                 pd, 
                                 vaddr, 
                                 seL4_ARM_Default_VMAttributes);
    return err;
}

int 
map_page(seL4_CPtr frame_cap, seL4_ARM_PageDirectory pd, seL4_Word vaddr, 
                seL4_CapRights rights, seL4_ARM_VMAttributes attr){
    int err;

    /* Attempt the mapping */
    err = seL4_ARM_Page_Map(frame_cap, pd, vaddr, rights, attr);
    if(err == seL4_FailedLookup){
        /* Assume the error was because we have no page table */
        err = _map_page_table(pd, vaddr);
        if(!err){
            /* Try the mapping again */
            err = seL4_ARM_Page_Map(frame_cap, pd, vaddr, rights, attr);
        }
    }

    return err;
}

void* 
map_device(void* paddr, int size){
    static seL4_Word virt = DEVICE_START;
    seL4_Word phys = (seL4_Word)paddr;
    seL4_Word vstart = virt;

    dprintf(1, "Mapping device memory 0x%x -> 0x%x (0x%x bytes)\n",
                phys, vstart, size);
    while(virt - vstart < size){
        seL4_Error err;
        seL4_ARM_Page frame_cap;
        /* Retype the untype to a frame */
        err = cspace_ut_retype_addr(phys,
                                    seL4_ARM_SmallPageObject,
                                    seL4_PageBits,
                                    cur_cspace,
                                    &frame_cap);
        conditional_panic(err, "Unable to retype device memory");
        /* Map in the page */
        err = map_page(frame_cap, 
                       seL4_CapInitThreadPD, 
                       virt, 
                       seL4_AllRights,
                       0);
        conditional_panic(err, "Unable to map device");
        /* Next address */
        phys += (1 << seL4_PageBits);
        virt += (1 << seL4_PageBits);
    }
    return (void*)vstart;
}


