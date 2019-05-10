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
#include "frame_table.h"
#include "mapping.h"
#include "vmem_layout.h"

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <utils/util.h>

/* Maximum number of frames allowed to be held by the frame table. */
#define MAX_FRAMES 0

/*
 * A numeric reference to a particular frame.
 */
typedef size_t frame_ref_t;

/*
 * The frame table is limited to 2^19 entries as that allows for up to
 * 2GiB in 4K frames.
 */
typedef PACKED struct {
    /* Page used to map frame into SOS memory. */
    seL4_ARM_Page sos_page: 20;
    /* Index in frame table of previous element in list. */
    frame_ref_t prev : 19;
    /* Index in frame table of next element in list. */
    frame_ref_t next : 19;
    /* Unused bits */
    size_t unused : 6;
} frame_t;
compile_time_assert("Small CPtr size", 20 >= INITIAL_TASK_CSPACE_BITS);

typedef PACKED struct {
    unsigned char data[BIT(seL4_PageBits)];
} frame_data_t;

/* Memory-efficient doubly linked list of frames
 *
 * As all frame objects will live in effectively an array, we only need
 * to be able to index into that array.
 */
typedef struct {
    /* Index of first element in list */
    frame_ref_t first;
    /* Index in last element of list */
    frame_ref_t last;
    /* Size of list (useful for debugging) */
    size_t length;
} frame_list_t;

/* This global variable tracks the frame table */
static UNUSED struct {
    /* The array of all frames in memory. */
    frame_t *frames;
    /* The data region of the frame table. */
    frame_data_t *frame_data;
    /* The current capacity of the frame table. */
    size_t capacity;
    /* The current number of frames resident in the table. */
    size_t used;
    /* The current size of the frame table in bytes. */
    size_t byte_length;
    /* The free frames. */
    frame_list_t free;
    /* The allocated frames. */
    frame_list_t allocated;
    /* cspace used to make allocations of capabilities. */
    cspace_t *cspace;
    /* vspace used to map pages into SOS. */
    seL4_ARM_PageGlobalDirectory vspace;
} frame_table = {
    .frames = (void *)SOS_FRAME_TABLE,
    .frame_data = (void *)SOS_FRAME_DATA,
};

/* Management of frame nodes */
static frame_t *frame_from_ref(frame_ref_t frame_ref);
static frame_ref_t ref_from_frame(frame_t *frame);

/* Management of frame list */
static void push_front(frame_list_t *list, frame_t *frame);
static void push_back(frame_list_t *list, frame_t *frame);
static frame_t *pop_front(frame_list_t *list, frame_t *frame);

/* Allocate a new frame. */
static frame_t *alloc_fresh_frame(void);

/* Increase the capacity of the frame table.
 *
 * @return  0 on succuss, -ve on failure. */
static int bump_capacity(void);

void frame_table_init(cspace_t *cspace, seL4_CPtr vspace)
{
    frame_table.cspace = cspace;
    frame_table.vspace = vspace;
}

cspace_t *frame_table_cspace(void)
{
    return NULL;
}

frame_ref_t alloc_frame(void)
{
    return NULL_FRAME;
}

void free_frame(UNUSED frame_ref_t frame_ref)
{
}

seL4_ARM_Page frame_page(UNUSED frame_ref_t frame_ref)
{
}

unsigned char *frame_data(UNUSED frame_ref_t frame_ref)
{
    return NULL;
}

void flush_frame(UNUSED frame_ref_t frame_ref)
{
}

void invalidate_frame(frame_ref_t frame_ref)
{
}

static frame_t *frame_from_ref(frame_ref_t frame_ref)
{
    return NULL;
}

static frame_ref_t ref_from_frame(frame_t *frame)
{
    return NULL_FRAME;
}

static void push_front(frame_list_t *list, frame_t *frame)
{
}

static void push_back(frame_list_t *list, frame_t *frame)
{
}

static frame_t *pop_front(frame_list_t *list, frame_t *frame)
{
    return NULL;
}

static frame_t *alloc_fresh_frame(void)
{
    return NULL;
}

static int bump_capacity(void)
{
    return 0;
}
