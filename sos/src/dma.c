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
 * This file implements very simple DMA for sos.
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
    /* the first virtual address */
    uintptr_t vstart;
    /* the first physical address */
    uintptr_t pstart;
    /* the next available physical address */
    uintptr_t pnext;
    /* the last available physical address */
    uintptr_t pend;
    /* a ut cptr, sufficient to allocate   */
    seL4_CPtr cap;
    /* vspace for flushing address ranges */
    seL4_CPtr vspace;
} dma_t;

/* global dma data structure */
static dma_t dma;

uintptr_t sos_dma_phys_to_virt(uintptr_t phys)
{
    return dma.vstart + (phys - dma.pstart);
}

uintptr_t sos_dma_virt_to_phys(uintptr_t vaddr)
{
    return dma.pstart + (vaddr - dma.vstart);
}

int dma_init(cspace_t *cspace, seL4_CPtr vspace, seL4_CPtr ut, uintptr_t pstart, uintptr_t vstart)
{
    if (ut == seL4_CapNull || vspace == seL4_CapNull || cspace == NULL || pstart == 0) {
        return -1;
    }

    dma.pstart = pstart;
    dma.vstart = vstart;
    dma.pend = dma.pstart + BIT(seL4_LargePageBits);
    dma.pnext = dma.pstart;
    dma.vspace = vspace;

    /* now map the frame */
    uintptr_t vaddr = sos_dma_phys_to_virt(dma.pstart);
    ZF_LOGI("DMA initialised %p <--> %p\n", (void *) vaddr, (void *) sos_dma_phys_to_virt(dma.pend));
    return map_frame(NULL, ut, dma.vspace, vaddr, seL4_AllRights, seL4_ARM_Default_VMAttributes);
}

dma_addr_t sos_dma_malloc(size_t size, int align)
{
    dma_addr_t addr = {0, 0};

    dma.pnext = DMA_ALIGN(dma.pnext);

    /* Round up to the alignment */
    dma.pnext = ROUND_UP(dma.pnext, align);

    if (dma.pnext + size >= dma.pend) {
        ZF_LOGE("Out of DMA memory");
        return addr;
    }

    addr.vaddr = sos_dma_phys_to_virt(dma.pnext);
    addr.paddr = dma.pnext;

    /* set return values */
    dma.pnext += size;
    ZF_LOGD("DMA: 0x%x\n", (uintptr_t) addr.vaddr);

    /* Clean invalidate the range to prevent seL4 cache bombs */
    sos_dma_cache_clean_invalidate(addr.vaddr, size);
    return addr;
}

seL4_Error sos_dma_cache_invalidate(uintptr_t addr, size_t size)
{
    return seL4_ARM_VSpace_Invalidate_Data(dma.vspace, addr, addr + size);
}

seL4_Error sos_dma_cache_clean(uintptr_t addr, size_t size)
{
    return seL4_ARM_VSpace_Clean_Data(dma.vspace, addr, addr + size);
}

seL4_Error sos_dma_cache_clean_invalidate(uintptr_t addr, size_t size)
{
    return seL4_ARM_VSpace_CleanInvalidate_Data(dma.vspace, addr, addr + size);
}
