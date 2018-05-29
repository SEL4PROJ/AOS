/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdlib.h>
#include <assert.h>

#include "ut.h"
#include "bitfield.h"


#define verbose 1
#include <sys/debug.h>
#include <sys/panic.h>
 
#define FLOOR14(x) ((x) & ~((1 << 14) - 1))
#define CEILING14(x) FLOOR14((x) + (1 << 14) - 1)

typedef struct suballocator {
    struct suballocator** prev;
    struct suballocator* next;
    seL4_Word base;
    bitfield_t* bitfield;
} suballocator_t;



static bitfield_t*     _pool14 = NULL;
static bitfield_t*     _pool12 = NULL;
static bitfield_t*     _pool10 = NULL;
static suballocator_t* _pool9  = NULL;
static suballocator_t* _pool4  = NULL;

static int _initialised = 0;
static seL4_Word _pool_base = 0;

#define PRIMARY_POOL_SIZEBITS 14
#define PRIMARY_POOL          _pool14


/*********************
 *** bitfield pool ***
 *********************/

static void _bitfield_fill_pool(bitfield_t* bf, int sizebits){
    if(sizebits != PRIMARY_POOL_SIZEBITS){
        int offset;

        /* grab a slab from the highest pool */
        offset = bf_set_next_free(PRIMARY_POOL);
        if(offset != -1){
            int units;
            int i;

            offset <<= (PRIMARY_POOL_SIZEBITS - sizebits);
            units = (1 << (PRIMARY_POOL_SIZEBITS - sizebits));
            for(i = 0; i < units; i++){
                bf_clr(bf, offset + i);
            }
        }

        /* Set the next free offset to improve search efficiency */
        bf->next_free = offset;
    }else{
        /* We have no higher pool to fill from */
    }
}

static void _bitfield_merge_up(bitfield_t* pool, int sizebits, int offset){
    if(sizebits != PRIMARY_POOL_SIZEBITS){
        int primary_base;
        int sublevel_base;
        int sublevel_units;
        int i;

        /* Find the number of offsets covered and align to the starting offset */
        sublevel_units = 1 << (PRIMARY_POOL_SIZEBITS - sizebits);
        sublevel_base = offset & ~(sublevel_units - 1);
        primary_base = sublevel_base >> (PRIMARY_POOL_SIZEBITS - sizebits);

        /* Check if all offsets are free */
        for(i = 0; i < sublevel_units; i++){
            if(bf_get(pool, sublevel_base + i)){
                /* Cannot merge */
                return;
            }
        }

        /* Merge back to primary pool */
        for(i = 0; i < sublevel_units; i++){
            bf_set(pool, sublevel_base + i);
        }
        bf_clr(PRIMARY_POOL, primary_base);

    }else{
        /* We have no higher pool to merge to */
    }
}


static seL4_Word do_ut_alloc_from_bitfield(int sizebits){
    seL4_Word addr;
    int offset;
    bitfield_t* pool;

    /* Select the appropriate pool */
    switch(sizebits){
    case 14: 
        pool = _pool14;
        break;
    case 12:
        pool = _pool12;
        break;
    case 10:
        pool = _pool10;
        break;
    default:
        /* invalid size */
        assert(!"Invalid size");
        return 0;
    }

    /* Allocate, fill as needed */
    offset = bf_set_next_free(pool);
    if(offset == -1){
        _bitfield_fill_pool(pool, sizebits);
        offset = bf_set_next_free(pool);
    }

    /* Translate the bitfield offset into an address */
    if(offset == -1){
        addr = 0;
    }else{
        addr = (offset << sizebits) + _pool_base;
    }

    return addr;
}

static void do_ut_free_from_bitfield(seL4_Word addr, int sizebits){
    int offset;
    bitfield_t* pool;

    /* Select the correct pool */
    switch(sizebits){
    case 14:
        pool = _pool14;
        break;
    case 12:
        pool = _pool12;
        break;
    case 10:
        pool = _pool10;
        break;
    default:
        /* invalid size */
        assert(!"Invalid size");
        return;
    }

    /* find the bitfield offset from the address */
    offset = (addr - _pool_base) >> sizebits;
    bf_clr(pool, offset);

    /* Merge the freed memory back up to the primary pool if we can */
    _bitfield_merge_up(pool, sizebits, offset);

}

/************************
 *** linked list pool ***
 ************************/
static void _pool_list_detach(suballocator_t* node){
    suballocator_t* next;

    assert(node != NULL);
    assert(*node->prev != NULL);

    /* Update next node */
    next = node->next;
    if(next){
        next->prev = node->prev;
    }

    /* update previous node */
    *node->prev = node->next;

    /* Clear the internal links */
    node->prev = NULL;
    node->next = NULL;
}

static void _pool_list_attach(suballocator_t** pool, suballocator_t* node){
    assert(node);
    assert(node->next == NULL);
    assert(node->prev == NULL);

    /* Link to next */
    node->next = *pool;
    if(node->next){
        node->next->prev = &node->next;
    }

    /* link to prev */
    node->prev = pool;
    *pool = node;
}



static suballocator_t* _pool_list_find_base(suballocator_t* pool, seL4_Word base){
    while(pool != NULL){
        if(pool->base == base){
            return pool;
        }

        pool = pool->next;
    }
    return NULL;
}

static suballocator_t* _pool_list_find_free(suballocator_t* pool){
    while(pool != NULL){
        if(pool->bitfield->available != 0){
            return pool;
        }

        pool = pool->next;
    }
    return NULL;
}

static suballocator_t* _new_suballocator(int sizebits){
    suballocator_t* pool_node;
    seL4_Word primary_addr;
    int units;

    /* Reserve memory from the primary pool */
    primary_addr = do_ut_alloc_from_bitfield(PRIMARY_POOL_SIZEBITS);
    if(primary_addr == 0){
        return NULL;
    }

    /* create the pool node */
    units = 1 << (PRIMARY_POOL_SIZEBITS - sizebits);
    pool_node = (suballocator_t*)malloc(sizeof(suballocator_t));
    if(pool_node == NULL){
        do_ut_free_from_bitfield(primary_addr, PRIMARY_POOL_SIZEBITS);
        return NULL;
    }
    pool_node->base = primary_addr;
    pool_node->next = NULL;
    pool_node->prev = NULL;

    /* Create the bitfield */
    pool_node->bitfield = new_bitfield(units, BITFIELD_INIT_EMPTY);
    if(pool_node->bitfield == NULL){
        free(pool_node);
        do_ut_free_from_bitfield(primary_addr, PRIMARY_POOL_SIZEBITS);
        return NULL;
    }

    return pool_node;
}

static seL4_Word do_ut_alloc_from_list(int sizebits){
    suballocator_t **pool;
    suballocator_t *pool_node;
    seL4_Word addr;
    int offset;

    /* Acquire the correct pool */
    if(sizebits == 9){
        pool = &_pool9;
    }else if(sizebits == 4){
        pool = &_pool4;
    }else{
        assert(!"Invalid size");
        return 0;
    }

    /* Find a node in the list that we can use -- or create a new one */
    pool_node = _pool_list_find_free(*pool);
    if(pool_node == NULL){
        pool_node = _new_suballocator(sizebits);
        if(pool_node == NULL){
            return 0;
        }
        _pool_list_attach(pool, pool_node);
    }

    /* Allocate from the pool */
    offset = bf_set_next_free(pool_node->bitfield);
    assert(offset != -1);

    addr = (offset << sizebits) + pool_node->base;

    return addr;
}

static void do_ut_free_from_list(seL4_Word addr, int sizebits){
    suballocator_t *pool;
    suballocator_t *pool_node;
    seL4_Word base;
    int offset;
    int units;

    /* Acquire the correct pool */
    if(sizebits == 9){
        pool = _pool9;
    }else if(sizebits == 4){
        pool = _pool4;
    }else{
        assert(!"Invalid size");
        return;
    }

    base = addr & ~((1 << PRIMARY_POOL_SIZEBITS) - 1);
    offset = (addr - base) >> sizebits;
    units = 1 << (PRIMARY_POOL_SIZEBITS - sizebits);

    /* Find the pool node that matches the base address */
    pool_node = _pool_list_find_base(pool, base);
    assert(pool_node);

    /* Clear memory and free pool node if empty */
    bf_clr(pool_node->bitfield, offset);
    if(pool_node->bitfield->available == units){
        _pool_list_detach(pool_node);
        do_ut_free_from_bitfield(pool_node->base, PRIMARY_POOL_SIZEBITS);
        destroy_bitfield(pool_node->bitfield);
        free(pool_node);
    }
}



/**************************
 *** Exported functions ***
 **************************/
void ut_allocator_init(seL4_Word low, seL4_Word high){
    seL4_Word mem_size;
    int i;

    assert(!_initialised);

    /* Align memory bounds */
    low = FLOOR14(low);
    high = CEILING14(high);

    mem_size = high - low;

    _pool_base = low;
    _pool14 = new_bitfield(mem_size >> 14, BITFIELD_INIT_FILLED);
    _pool12 = new_bitfield(mem_size >> 12, BITFIELD_INIT_FILLED);
    _pool10 = new_bitfield(mem_size >> 10, BITFIELD_INIT_FILLED);

    /* Marked untyped as available */
    for(i = 0; i < mem_size >> 14; i++){
        bf_clr(_pool14, i);
    }

    /* Initialise sub allocators */
    _pool9 = NULL;
    _pool4 = NULL;

    _initialised = 1;
}

seL4_Word ut_alloc(int sizebits){
    seL4_Word addr;

    assert(_initialised);

    /* make sure that we are initialised */
    assert(PRIMARY_POOL != NULL);

    /* forward to appropriate functions */
    switch(sizebits){
    case 4:
    case 9:
        addr = do_ut_alloc_from_list(sizebits);
        break;
    case 10:
    case 12:
    case 14:
        addr = do_ut_alloc_from_bitfield(sizebits);
        break;
    default:
        assert(!"ut_free received invalid size");
        return 0;
    }

    return addr;
}

void ut_free(seL4_Word addr, int sizebits){
    assert(addr != 0);
    assert((addr & ((1 << sizebits) - 1)) == 0 || !"Address not aligned");

    /* forward to appropriate functions */
    switch(sizebits){
    case 4:
    case 9:
        do_ut_free_from_list(addr, sizebits);
        break;
    case 10:
    case 12:
    case 14:
        do_ut_free_from_bitfield(addr, sizebits);
        break;
    default:
        assert(!"ut_free received invalid size");
    }
}



