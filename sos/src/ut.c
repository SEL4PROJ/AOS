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
#include "ut.h"
#include "bootstrap.h"

#include <cspace/cspace.h>
#include <stdlib.h>
#include <assert.h>

#define SIZE_BITS_TO_INDEX(x) (x - seL4_EndpointBits)

/* this global variable tracks our ut table */
static ut_table_t table;

static void push(ut_t **head, ut_t *new)
{
    new->next = *head;
    *head = new;
}

static ut_t *pop(ut_t **head)
{
    ut_t *popped = *head;
    *head = popped->next;
    return popped;
}

static inline seL4_Word ut_to_paddr(ut_t *ut)
{
    return (ut - table.untypeds) * PAGE_SIZE_4K + table.first_paddr;
}

static inline ut_t *paddr_to_ut(seL4_Word paddr)
{
    return &table.untypeds[(paddr - table.first_paddr) / PAGE_SIZE_4K];
}

size_t ut_pages_for_region(ut_region_t region)
{
    /* work out the size of the ut table for a region of memory */
    return BYTES_TO_4K_PAGES((region.end - region.start) / PAGE_SIZE_4K * sizeof(ut_t));
}

void ut_init(void *memory, ut_region_t region)
{
    /* the memory is already 0'd, as its just been allocated from raw untyped */
    table.untypeds = memory;
    memset(table.free_untypeds, 0, ARRAY_SIZE(table.free_untypeds) * sizeof(ut_t *));
    table.first_paddr = region.start;
}

size_t ut_size(void)
{
    return table.n_4k_untyped * PAGE_SIZE_4K;
}

void ut_add_untyped_range(seL4_Word paddr, seL4_CPtr cap, size_t n, bool device)
{
    ut_t **list = &table.free_untypeds[SIZE_BITS_TO_INDEX(seL4_PageBits)];
    for (size_t i = 0; i < n; i++) {
        ut_t *node = paddr_to_ut(paddr + (i * PAGE_SIZE_4K));
        node->cap = cap;
        node->valid = 1;
        cap++;
        if (!device) {
            node->size_bits = seL4_PageBits;
            push(list, node);
            table.n_4k_untyped++;
        }
    }
}

ut_t *ut_alloc_4k_untyped(uintptr_t *paddr)
{
    ut_t **list = &table.free_untypeds[SIZE_BITS_TO_INDEX(seL4_PageBits)];
    if (*list == NULL) {
        ZF_LOGE("out of memory");
        return NULL;
    }

    ut_t *n = pop(list);
    if (paddr) {
        *paddr = ut_to_paddr(n);
    }
    ZF_LOGD("Allocated %lx, cap %lx", ut_to_paddr(n), n->cap);
    return n;
}

/* ensure there are at least two spare free structures so we can split an untyped */
static bool ensure_new_structures(cspace_t *cspace)
{
    if (table.free_structures == NULL || table.free_structures->next == 0) {
        /* we need to allocate more spare ut objects */
        ut_t *frame = ut_alloc_4k_untyped(NULL);
        if (frame == NULL) {
            /* No luck */
            ZF_LOGE("No 4K untypeds");
            return false;
        }

        /* allocate a slot to retype the frame into */
        seL4_CPtr cptr = cspace_alloc_slot(cspace);
        if (cptr == seL4_CapNull) {
            /* cspace is full */
            ZF_LOGE("Cspace full");
            return false;
        }

        /* retype */
        seL4_Error err = cspace_untyped_retype(cspace, frame->cap, cptr, seL4_ARM_SmallPageObject, seL4_PageBits);
        if (err) {
            ZF_LOGE("Retype failed");
            return false;
        }

        /* map */
        ut_t *new_uts = bootstrap_map_frame(cspace, cptr);
        if (new_uts == NULL) {
            ZF_LOGE("Failed to map frame");
            return false;
        }

        /* now add all the new uts structures to the free list */
        for (size_t i = 0; i < PAGE_SIZE_4K / sizeof(ut_t); i++) {
            push(&table.free_structures, &new_uts[i]);
        }
    }
    return true;

}

ut_t *ut_alloc(size_t size_bits, cspace_t *cspace)
{
    /* check we can handle the size */
    if (size_bits > seL4_PageBits) {
        ZF_LOGE("UT table can only allocate untypeds <= 4K in size");
        return NULL;
    }

    if (size_bits < seL4_EndpointBits) {
        ZF_LOGE("UT table cannot alloc untyped < %zu in size\n", (size_t) seL4_EndpointBits);
        return NULL;
    }

    if (size_bits == seL4_PageBits) {
        return ut_alloc_4k_untyped(NULL);
    }

    ut_t **list = &table.free_untypeds[SIZE_BITS_TO_INDEX(size_bits)];
    if (*list == NULL) {
        /* need to retype a bigger object into the size requested */
        ut_t *larger = ut_alloc(size_bits + 1, cspace);
        if (larger == NULL) {
            return NULL;
        }

        /* check we have enough memory to account for our new uts */
        if (!ensure_new_structures(cspace)) {
            ut_free(larger);
            return NULL;
        }

        /* now ask the cspace to retype for us */
        ut_t *new1 = pop(&table.free_structures);
        new1->cap = cspace_alloc_slot(cspace);
        if (new1->cap == seL4_CapNull) {
            ut_free(larger);
            return NULL;
        }
        new1->size_bits = size_bits;

        ut_t *new2 = pop(&table.free_structures);
        new2->cap = cspace_alloc_slot(cspace);
        if (new2->cap == seL4_CapNull) {
            cspace_free_slot(cspace, new1->cap);
            ut_free(larger);
            return NULL;
        }
        new2->size_bits = size_bits;

        seL4_Error err = cspace_untyped_retype(cspace, larger->cap, new1->cap, seL4_UntypedObject, size_bits);
        if (err) {
            cspace_free_slot(cspace, new1->cap);
            cspace_free_slot(cspace, new2->cap);
            push(&table.free_structures, new1);
            ut_free(larger);
            return NULL;
        }

        err = cspace_untyped_retype(cspace, larger->cap, new2->cap, seL4_UntypedObject, size_bits);
        if (err) {
            cspace_delete(cspace, new1->cap);
            cspace_free_slot(cspace, new1->cap);
            cspace_free_slot(cspace, new2->cap);
            push(&table.free_structures, new1);
            push(&table.free_structures, new2);
            ut_free(larger);
            return NULL;
        }

        push(list, new1);
        push(list, new2);
        /* finally, we now know there are untyped objects for the requested size */
    }

    return pop(list);
}

void ut_free(ut_t *node)
{
    ut_t **list = &table.free_untypeds[SIZE_BITS_TO_INDEX(node->size_bits)];
    push(list, node);
}

ut_t *ut_alloc_4k_device(uintptr_t paddr)
{
    ut_t *ut = paddr_to_ut(paddr);
    if (!ut->valid) {
        ZF_LOGE("No ut for paddr %p", (void *) paddr);
        return NULL;
    }
    return ut;
}
