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
/**
 * This file implements very simple DMA for sos.
 *
 * It uses the ut allocator to allocate a contiguous set of untypeds
 * at unitialisation, then retypes and maps frames on demand.
 *
 * It does not free.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <sel4/types.h>
#include <cspace/cspace.h>
#include "dma.h"
#include "mapping.h"
#include "ut.h"
#include "vmem_layout.h"

#define DMA_ALIGN_BITS  7 /* 128 */
#define DMA_ALIGN(a)    ROUND_UP((a), DMA_ALIGN_BITS)

typedef struct {
    /* the first physical address */
    uintptr_t pstart;
    /* the next available physical address */
    uintptr_t pnext;
    /* the last available physical address */
    uintptr_t pend;
    /* the last mapped paddr */
    uintptr_t last_mapped;
    /* was the last allocation cached or not */
    bool cached;
    /* a ut cptr, sufficient to allocate   */
    seL4_CPtr cap;
    /* a cspace for allocating slots */
    cspace_t *cspace;
    /* vspace for mapping */
    seL4_CPtr vspace;
} dma_t;

/* global dma data structure */
static dma_t dma;

static inline uintptr_t phys_to_virt(uintptr_t phys)
{
    return SOS_DMA_VSTART + (phys - dma.pstart);
}

static inline uintptr_t virt_to_phys(uintptr_t vaddr)
{
    return dma.pstart + (vaddr - SOS_DMA_VSTART);
}

/* ensure a region of dma is mapped */
static void* dma_fill(uintptr_t pstart, uintptr_t pend, bool cached)
{

    seL4_ARM_VMAttributes vm_attr = 0;
    if (cached) {
        vm_attr = seL4_ARM_Default_VMAttributes;
    }

    void *result = (void *) phys_to_virt(pstart);
    for (uintptr_t paddr = ALIGN_DOWN(pstart, PAGE_SIZE_4K); paddr < pend; paddr += PAGE_SIZE_4K) {
        if (paddr > dma.last_mapped) {
            /* allocate a slot to retype into */
            seL4_CPtr slot = cspace_alloc_slot(dma.cspace);
            if (slot == seL4_CapNull) {
                ZF_LOGE("Failed to alloc slot");
                return NULL;
            }
            /* now do the retype */
            seL4_Error err = cspace_untyped_retype(dma.cspace, dma.cap, slot,
                                                   seL4_ARM_SmallPageObject, seL4_PageBits);
            if (err) {
                ZF_LOGE("Failed to retype");
                cspace_free_slot(dma.cspace, slot);
                return NULL;
            }

            /* now map the frame */
            uintptr_t vaddr = phys_to_virt(paddr);
            int error = map_frame(dma.cspace, slot, dma.vspace, vaddr, seL4_AllRights, vm_attr);
            if (error) {
                ZF_LOGE("Failed to map page at %p\n", (void *) vaddr);
                cspace_delete(dma.cspace, slot);
                cspace_free_slot(dma.cspace, slot);
                return NULL;
            }
            dma.last_mapped = paddr;
            dma.cached = cached;
        }
    }

    return result;
}


int dma_init(cspace_t *cspace, size_t sizebits, seL4_CPtr vspace, seL4_CPtr ut, uintptr_t pstart)
{
    if (sizebits < seL4_PageBits) {
        ZF_LOGE("Size bits %zu invalid, minimum %zu\n", sizebits, (size_t) seL4_PageBits);
        return -1;
    }

    if (ut == seL4_CapNull || vspace == seL4_CapNull || cspace == NULL || pstart == 0) {
        return -1;
    }

    dma.pend = dma.pstart + BIT(sizebits);
    dma.pnext = dma.pstart;
    dma.cspace = cspace;
    dma.last_mapped = 0;
    dma.vspace = vspace;
    dma.cap = ut;

    return 0;
}

void *sos_dma_malloc(UNUSED void *cookie, size_t size, int align, int cached, UNUSED ps_mem_flags_t flags)
{
    dma.pnext = DMA_ALIGN(dma.pnext);

    /* If caching policy has changed we round to page boundary */
    if (dma.pnext != dma.pstart && (dma.cached != cached && dma.pnext % PAGE_SIZE_4K != 0)) {
        dma.pnext = ROUND_UP(dma.pnext, BIT(seL4_PageBits));
    }

    /* Round up to the alignment */
    dma.pnext = ROUND_UP(dma.pnext, align);

    if (dma.pnext + size >= dma.pend) {
        ZF_LOGE("Out of 4k untypeds for DMA");
        return NULL;
    }

    void *vaddr = dma_fill(dma.pnext, dma.pnext + size, cached);
    if (vaddr == NULL) {
        ZF_LOGE("Failed to complete dma allocation");
        return NULL;
    }

    /* set return values */
    dma.pnext += size;
    ZF_LOGV("DMA: 0x%x\n", (uintptr_t) vaddr);

    /* Clean invalidate the range to prevent seL4 cache bombs */
    sos_dma_cache_clean_invalidate(addr.vaddr, size);
    return addr;
}

seL4_Error sos_dma_cache_invalidate(uintptr_t addr, size_t size)
{
    return seL4_ARM_PageGlobalDirectory_Invalidate_Data(dma.vspace, addr, addr + size);
}

seL4_Error sos_dma_cache_clean(uintptr_t addr, size_t size)
{
    return seL4_ARM_PageGlobalDirectory_Clean_Data(dma.vspace, addr, addr + size);
}

seL4_Error sos_dma_cache_clean_invalidate(uintptr_t addr, size_t size)
{
    return seL4_ARM_PageGlobalDirectory_CleanInvalidate_Data(dma.vspace, addr, addr + size);
}
