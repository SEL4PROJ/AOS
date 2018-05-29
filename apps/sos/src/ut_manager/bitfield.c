/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "bitfield.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define verbose 0
#include <sys/debug.h>
#include <sys/panic.h>

#define BITS_PER_BYTE 8
#define FLOOR(x)      ((x) & ~(BITS_PER_BYTE - 1))
#define CEILING(x)    (FLOOR((x) + (BITS_PER_BYTE-1)))

void debug_print(bitfield_t* bf){
#if 0
    int i, j;
    for(i = 0, j=1; i < CEILING(bf->size)/BITS_PER_BYTE; i++, j++){
        printf("%02x", bf->b[i]&0xff);
        if(!(j & 0x3)){
            printf(" ");
        }
        if(j == 4*8){
            printf("\n");
            j=0;
        }
    }
    printf("\n\n");
#else
#endif
}

bitfield_t* new_bitfield(int size, enum bf_init_state state){
    bitfield_t* bf;
    int bytes;

    /* Allocate memory */
    bf = (bitfield_t*)malloc(sizeof(bitfield_t));
    if(bf == NULL){
        return NULL;
    }

    bytes = CEILING(size)/BITS_PER_BYTE;
    bf->b = (char*)malloc(bytes);
    if(bf->b == NULL){
        free(bf);
        return NULL;
    }

    /* initialise the bitfield */
    bf->size = size;
    bf->next_free = 0;
    if(state == BITFIELD_INIT_FILLED){
        bf->available = 0;
        memset(bf->b, 0xff, bytes);
    }else{
        int i;

        bf->available = size;
        memset(bf->b, 0x00, bytes);

        /* mark overflow as used if the size is not a a multiple of 8 */
        for(i = size; i < bytes * BITS_PER_BYTE; i++){
            bf_set(bf, i);
        }
    }

    return bf;
}

void destroy_bitfield(bitfield_t* bf){
    free(bf->b);
    free(bf);
}


/* Find a byte that is not completely marked */
static inline int _bf_find_next_free_byte(char* field, int next, int size){
    int current;
    next = FLOOR(next)/BITS_PER_BYTE;
    size = CEILING(size)/BITS_PER_BYTE;

    /* Search from next to limit */
    for(current = next; current < size; current++){
        if(field[current] != 0xff){
            return current;
        }
    }

    /* Search from limit to next */
    for(current = 0; current < next; current++){
        if(field[current] != 0xff){
            return current;
        }
    }

    return -1;
}

/* 
 * find a bit that is clear 
 * @pre the byte must have a free bit
 */
static inline int _bf_find_next_free_bit(char field){
    int i;
    assert(field != 0xff);
    for(i = 0; i < BITS_PER_BYTE; i++){
        if((field & (1 << i)) == 0){
            return i;
        }
    }
    return -1;
}

int bf_set_next_free(bitfield_t* bf){
    if(bf->available != 0){
        int byte;

        /* Bytewise search for != 0xff */
        byte = _bf_find_next_free_byte(bf->b, bf->next_free, bf->size);
        if(byte != -1){
            int bit;
            int offset;

            /* bitwise search within byte */
            bit = _bf_find_next_free_bit(bf->b[byte]);
            offset = (byte * BITS_PER_BYTE) + bit;

            /* mark and return */
            bf_set(bf, offset); 
            bf->next_free = offset + 1;
            if(bf->next_free > bf->size){
                bf->next_free = 0;
            }

            return offset;
        }
    }

    return -1;
}


static inline void _bf_decode(int offset, int* byte, int* bitmask){
    int bit;
    *byte = offset >> 3;
    bit = offset & 0x7;

    *bitmask = 1;
    while(bit-- > 0){
        *bitmask <<= 1;
    }
}


void bf_set(bitfield_t* bf, int offset){
    int byte, bitmask;

    assert(!bf_get(bf, offset));

    _bf_decode(offset, &byte, &bitmask);

    assert(byte < CEILING(bf->size)/BITS_PER_BYTE);
    bf->b[byte] |= bitmask;
    bf->available--;
}

void bf_clr(bitfield_t* bf, int offset){
    int byte, bitmask;

    assert(bf_get(bf, offset));

    _bf_decode(offset, &byte, &bitmask);

    assert(byte < CEILING(bf->size)/BITS_PER_BYTE);
    bf->b[byte] &= ~bitmask;
    bf->available++;

    debug_print(bf);
}

int bf_get(const bitfield_t* bf, int offset){
    int byte, bitmask;
    _bf_decode(offset, &byte, &bitmask);

    assert(byte < CEILING(bf->size)/BITS_PER_BYTE);
    return (bf->b[byte] & bitmask) != 0;
}


