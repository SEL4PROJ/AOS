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
#pragma once

#include "bootstrap.h"
#include "ut.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <cspace/cspace.h>

/*
 * Every frame in the frame table is referenced by a compact index into
 * the table. As the table is only large enough to contain 2GiB worth of
 * frames, every frame reference will be 2^19 bits.
 */
typedef size_t frame_ref_t;

/*
 * The 0 frame is a sentinel NULL frame and indicates a lack of
 * reference to any particular frame.
 */
#define NULL_FRAME ((frame_ref_t)0)

/*
 * Identifiers of the different lists in the frame table.
 *
 * These are used to ensure that frame table entries move correctly
 * between the two lists and that those lists maintain a consistently
 * correct structure.
 */
typedef enum {
    NO_LIST = 1,
    FREE_LIST = 2,
    ALLOCATED_LIST = 3,
} list_id_t;

/* Array of names for each of the lists above. */
extern char *frame_table_list_names[];

/* Debugging macro to get the human-readable name of a particular list ID. */
#define LIST_ID_NAME(list_id) (frame_table_list_names[list_id])

/* The actual representation of a frame in the frame table. */
typedef struct frame frame_t;
PACKED struct frame {
    /* Page used to map frame into SOS memory. */
    seL4_ARM_Page sos_page: 20;
    /* Index in frame table of previous element in list. */
    frame_ref_t prev : 19;
    /* Index in frame table of next element in list. */
    frame_ref_t next : 19;
    /* Indicates which list the frame is in. */
    list_id_t list_id : 2;
    /* Unused bits */
    size_t unused : 4;
};
compile_time_assert("Small CPtr size", 20 >= INITIAL_TASK_CSPACE_BITS);

/*
 * Initialise frame table.
 *
 * @param cspace  root cspace object from SOS.
 * @param vspace  virtual address space of SOS.
 */
void frame_table_init(cspace_t *cspace, seL4_CPtr vspace);

/*
 * Get the cspace used by the frame table.
 */
cspace_t *frame_table_cspace(void);

/*
 * Allocate a frame from the frame table.
 *
 * This allocates a frame from the frame table, using a 4K untyped if no
 * frames are spare. This frame may be dirty so make sure to zero-out
 * any memory in the frame that is not explicitly written over with
 * data.
 *
 * DO NOT append the untypeds returned from this function into another
 * list. When they are allocated they are still tracked in a list within
 * the frame table.
 *
 * The capability associated with a frame returned from this is a
 * Page, referring to the mapping of the frame in SOS, rather than to an
 * untyped. This means that additional mappings to the frame can be made
 * by copying the capability.
 *
 * This function returns NULL if there are no free untypeds and an
 * untyped could not be allocated from the untyped manager.
 *
 * You will need to modify the frame table to deal with the case where
 * only a limited number of frames may be held by the frame table.
 */
frame_ref_t alloc_frame(void);

/*
 * Free a frame allocated by the frame table.
 *
 * This returns the frame to the frame table for re-use rather than
 * returning it to the untyped allocator.
 */
void free_frame(frame_ref_t frame_ref);

/*
 * Get the contents of a frame as mapped into SOS.
 *
 * @returns a pointer to a 4096 byte character array representing the
 * frame data as mapped into SOS.
 */
unsigned char *frame_data(frame_ref_t frame_ref);

/*
 * Flush any cached values for a frame out to physical memory.
 *
 * Before allowing data to be accessed in another virtual address space
 * the cache should be flushed to ensure that the correct data is
 * observed in that address space.
 */
void flush_frame(frame_ref_t frame_ref);

/*
 * Invalidate any cached memory of a frame requiring data to be fetched
 * from physical memory.
 *
 * If the frame is likely to have been changed in another virtual
 * address space it should be invalidated before being read.
 */
void invalidate_frame(frame_ref_t frame_ref);

/*
 * Get the capability to the page used to map the frame into SOS.
 *
 * This can be copied to create mappings into additional virtual address
 * spaces.
 */
seL4_ARM_Page frame_page(frame_ref_t frame_ref);

/*
 * Get the underlying frame reference by a frame ID.
 *
 * This should only be used for debugging.
 */
frame_t *frame_from_ref(frame_ref_t frame_ref);
