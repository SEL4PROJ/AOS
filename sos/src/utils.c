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
#include "utils.h"

#include <sel4runtime.h>
#include <cspace/cspace.h>
#include <aos/sel4_zf_logif.h>

#include "ut.h"

/* helper to allocate a ut + cslot, and retype the ut into the cslot */
ut_t *alloc_retype(seL4_CPtr *cptr, seL4_Word type, size_t size_bits)
{
    /* Allocate the object */
    ut_t *ut = ut_alloc(size_bits, &cspace);
    if (ut == NULL) {
        ZF_LOGE("No memory for object of size %zu", size_bits);
        return NULL;
    }

    /* allocate a slot to retype the memory for object into */
    *cptr = cspace_alloc_slot(&cspace);
    if (*cptr == seL4_CapNull) {
        ut_free(ut);
        ZF_LOGE("Failed to allocate slot");
        return NULL;
    }

    /* now do the retype */
    seL4_Error err = cspace_untyped_retype(&cspace, ut->cap, *cptr, type, size_bits);
    ZF_LOGE_IFERR(err, "Failed retype untyped");
    if (err != seL4_NoError) {
        ut_free(ut);
        cspace_free_slot(&cspace, *cptr);
        return NULL;
    }

    return ut;
}
