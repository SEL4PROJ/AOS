/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/**
 * This file implements very simple DMA for sos.
 *
 * It does not free and only keeps a memory pool
 * big enough to get the network drivers booted.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <sel4/types.h>
#include <cspace/cspace.h>
#include <dma.h>
#include <mapping.h>
#include <ut_manager/ut.h>
#include <vmem_layout.h>

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

#define DMA_SIZE     (_dma_pend - _dma_pstart)
#define DMA_PAGES    (DMA_SIZE >> seL4_PageBits)

#define PHYS(vaddr)  ((vaddr) - DMA_VSTART + _dma_pstart)
#define VIRT(paddr)  ((paddr) + DMA_VSTART - _dma_pstart)

#define PAGE_OFFSET(a) ((a) & ((1 << seL4_PageBits) - 1))

#define DMA_ALIGN_BITS  7 /* 128 */
#define DMA_ALIGN(a)    ROUND_UP(a,DMA_ALIGN_BITS)

static seL4_CPtr* _dma_caps;

static seL4_Word _dma_pstart = 0;
static seL4_Word _dma_pend = 0;
static seL4_Word _dma_pnext = 0;

static inline void
_dma_fill(seL4_Word pstart, seL4_Word pend, int cached){
    seL4_CPtr* caps = &_dma_caps[(pstart - _dma_pstart) >> seL4_PageBits];
    seL4_ARM_VMAttributes vm_attr = 0;
    int err;

    if(cached){
        vm_attr = seL4_ARM_Default_VMAttributes;
        vm_attr = 0 /* TODO L2CC currently not controlled by kernel */;
    }

    pstart -= PAGE_OFFSET(pstart);
    while(pstart < pend){
        if(*caps == seL4_CapNull){
            /* Create the frame cap */
            err = cspace_ut_retype_addr(pstart, seL4_ARM_SmallPageObject,
                                        seL4_PageBits, cur_cspace, caps);
            assert(!err);
            /* Map in the frame */
            err = map_page(*caps, seL4_CapInitThreadPD, VIRT(pstart), 
                           seL4_AllRights, vm_attr);
            assert(!err);
        }
        /* Next */
        pstart += (1 << seL4_PageBits);
        caps++;
    }
}


int 
dma_init(seL4_Word dma_paddr_start, int sizebits){
    assert(_dma_pstart == 0);

    _dma_pstart = _dma_pnext = dma_paddr_start;
    _dma_pend = dma_paddr_start + (1 << sizebits);
    _dma_caps = (seL4_CPtr*)malloc(sizeof(seL4_CPtr) * DMA_PAGES);
    conditional_panic(!_dma_caps, "Not enough heap space for dma frame caps");

    memset(_dma_caps, 0, sizeof(seL4_CPtr) * DMA_PAGES);
    return 0;
}


void *
sos_dma_malloc(void* cookie, size_t size, int align, int cached, ps_mem_flags_t flags) {
    static int alloc_cached = 0;
    void *dma_addr;
    (void)cookie;

    assert(_dma_pstart);
    _dma_pnext = DMA_ALIGN(_dma_pnext);
    if(_dma_pnext < _dma_pend){
        /* If caching policy has changed we round to page boundary */
        if(alloc_cached != cached && PAGE_OFFSET(_dma_pnext) != 0){
            _dma_pnext = ROUND_UP(_dma_pnext, BIT(seL4_PageBits));
        }
        /* Round up to the alignment */
        _dma_pnext = ROUND_UP(_dma_pnext, align);
        alloc_cached = cached;
        /* Fill the dma memory */
        _dma_fill(_dma_pnext, _dma_pnext + size, cached);
        /* set return values */
        dma_addr = (void*)VIRT(_dma_pnext);
        _dma_pnext += size;
    }else{
        dma_addr = NULL;
    }
    dprintf(5, "DMA: 0x%x\n", (uint32_t)dma_addr);
    /* Clean invalidate the range to prevent seL4 cache bombs */
    sos_dma_cache_op(NULL, dma_addr, size, DMA_CACHE_OP_CLEAN_INVALIDATE);
    return dma_addr;
}

void sos_dma_free(void *cookie, void *addr, size_t size) {
    /* do not support free */
}

uintptr_t sos_dma_pin(void *cookie, void *addr, size_t size) {
    if ((uintptr_t)addr < DMA_VSTART || (uintptr_t)addr >= DMA_VEND) {
        return 0;
    } else {
        return PHYS((uintptr_t)addr);
    }
}

void sos_dma_unpin(void *cookie, void *addr, size_t size) {
    /* no op */
}

typedef int (*sel4_cache_op_fn_t)(seL4_ARM_PageDirectory, seL4_Word, seL4_Word);

static void
cache_foreach(void *vaddr, int range, sel4_cache_op_fn_t proc)
{
    int error;
    uintptr_t next;
    uintptr_t end = (uintptr_t)(vaddr + range);
    for (uintptr_t addr = (uintptr_t)vaddr; addr < end; addr = next) {
        next = MIN(PAGE_ALIGN_4K(addr + PAGE_SIZE_4K), end);
        error = proc(seL4_CapInitThreadPD, addr, next);
        assert(!error);
    }
}

void sos_dma_cache_op(void *cookie, void *addr, size_t size, dma_cache_op_t op) {
    /* everything is mapped uncached at the moment */
    switch(op) {
    case DMA_CACHE_OP_CLEAN:
        cache_foreach(addr, size, seL4_ARM_PageDirectory_Clean_Data);
        break;
    case DMA_CACHE_OP_INVALIDATE:
        cache_foreach(addr, size, seL4_ARM_PageDirectory_Invalidate_Data);
        break;
    case DMA_CACHE_OP_CLEAN_INVALIDATE:
        cache_foreach(addr, size, seL4_ARM_PageDirectory_CleanInvalidate_Data);
        break;
    }
}
