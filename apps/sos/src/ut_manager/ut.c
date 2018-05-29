/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "ut.h"
#include <cspace/cspace.h>
#include <stdlib.h>
#include <assert.h>

#include <autoconf.h>

#define verbose 0
#include <sys/debug.h>

#define ALIGN(x, base)     ((x) & ~((base) - 1))
#define ALIGN_TOP(x, base) (ALIGN((x) + ((base) - 1), base))

#define UT_SIZEBITS(i) (_bi->untypedSizeBitsList[_ut.map[i]])
#define UT_PSTART(i)   (   _bi->untypedPaddrList[_ut.map[i]])
#define UT_PEND(i)     (UT_PSTART(i) + (1 << (UT_SIZEBITS(i))))
#define UT_CAP(i)      (_bi->untyped.start + _ut.map[i])

const seL4_BootInfo* _bi = NULL;

struct {
    int map[CONFIG_MAX_NUM_BOOTINFO_UNTYPED_CAPS];
    int count;
}_ut;

static seL4_Word _low, _high;


static inline void _fill_table(const seL4_BootInfo *bi){
    int i;

    _bi = bi;
    _ut.count = _bi->untyped.end - _bi->untyped.start;

    for(i = 0; i < _ut.count; i++){
        _ut.map[i] = i;
    }
    assert(i == _ut.count);
}

static inline void _strip_min(int min_bits){
    int i, j;
    for(i = j = 0; i < _ut.count; i++){
        if(UT_SIZEBITS(i) >= min_bits){
            _ut.map[j++] = _ut.map[i];
        }
    }
    _ut.count = j;
}

static inline void _strip_max(int max_bits){
    int i, j;
    for(i = j = 0; i < _ut.count; i++){
        if(UT_SIZEBITS(i) <= max_bits){
            _ut.map[j++] = _ut.map[i];
        }
    }
    _ut.count = j;
}

static int __ut_cmp(const void* _a, const void* _b){
    return  _bi->untypedPaddrList[*(int*)_a] - 
            _bi->untypedPaddrList[*(int*)_b];
}
static inline void _sort_table(void){
    qsort(_ut.map, _ut.count, sizeof(*_ut.map), &__ut_cmp);
}

static inline void _reduce_table(int min_bits, int max_bits){
    if(min_bits > 0){ _strip_min(min_bits); }
    if(max_bits > 0){ _strip_max(max_bits); }
}

static inline void _select_largest_contiguous(void){
    int i;
    int contig_start, this_start;
    seL4_Word contig_size, this_size;
    int contig_end;

    assert(_ut.count);

    contig_start = 0;
    contig_size = 0;
    contig_end = 0;

    this_start = 0;
    this_size = 0;

    for(i = 0; i < _ut.count; i++){
        this_size += 1 << UT_SIZEBITS(i);
        if(i + 1 >= _ut.count || UT_PEND(i) != UT_PSTART(i + 1)){
            if(contig_size < this_size){
                contig_size = this_size;
                contig_start = this_start;
                contig_end = i + 1;
            }
            this_start = i + 1;
            this_size = 0;
        }
    }

    i = 0;
    while(contig_start < contig_end){
        _ut.map[i++] = _ut.map[contig_start++];
    }

    _ut.count = i;
}


static inline void _print_table(void){
    int i;
    dprintf(0, "\n"
                       "+-----------------------------------------------+\n"
                       "|                 Untyped Table                 |\n"
                       "|-----------------------------------------------|\n"
                       "| ut(bi) |    start   ->    end     | size bits |\n"
                       "|-----------------------------------------------|\n");
    for(i = 0; i < _ut.count; i++){
        dprintf(0,     "| %2d(%2d) | 0x%08x -> 0x%08x | %9d |\n", 
                i, _ut.map[i], UT_PSTART(i), UT_PEND(i), UT_SIZEBITS(i));
        if(i != _ut.count - 1 && UT_PEND(i) != UT_PSTART(i + 1)){
            dprintf(0, "|-----------------------------------------------|\n");
        }
    }
    dprintf(0,         "+-----------------------------------------------+\n");
}

static int ut_translate_device(seL4_Word addr, seL4_Untyped* ret_cptr, 
                               seL4_Word* ret_offset){
    int i = _bi->untyped.end - _bi->untyped.start;
    const seL4_Word* paddr = &_bi->untypedPaddrList[i];
    const uint8_t* sizebits = &_bi->untypedSizeBitsList[i];
    seL4_Untyped last_cap = _bi->deviceUntyped.end;
    seL4_Untyped cap = _bi->deviceUntyped.start;

    for(; cap < last_cap; cap++, paddr++, sizebits++){
        if(*paddr <= addr && addr < *paddr + (1 << *sizebits)){
            *ret_cptr = cap;
            *ret_offset = addr - *paddr;
            return 0;
        }
    }

    return !0;
}

int ut_translate(seL4_Word addr, seL4_Untyped* ret_cptr, seL4_Word* ret_offset){
    int start, end;

    start = 0;
    end = _ut.count;
    while(start < end){
        int index;

        index = (start + end) >> 1;
        if(addr < UT_PSTART(index)){
            end = index;
        }else if(UT_PEND(index) <= addr){
            start = index + 1;
        }else{
            /* Success */
            *ret_cptr = UT_CAP(index);
            *ret_offset = addr - UT_PSTART(index);
            return 0;
        }
    }

    /* Check if the address matches a device */
    return ut_translate_device(addr, ret_cptr, ret_offset);
}

int ut_table_init(const seL4_BootInfo *bi){
    if(!bi){
        return !0;
    }

    _fill_table(bi);
    _reduce_table(MIN_UT_SIZE_BITS, 0);
    _sort_table();
    _select_largest_contiguous();

    _low = UT_PSTART(0);
    _high = UT_PEND(_ut.count - 1);

    _print_table();

    return 0;
}

void ut_find_memory(seL4_Word* low, seL4_Word* high){
    *low = _low;
    *high = _high;
}

seL4_Word ut_steal_mem(int sizebits){
    seL4_Word paddr;

    paddr = ALIGN_TOP(_low, 1 << sizebits);

    if(paddr + (1 << sizebits) <= _high){
        _low = paddr + (1 << sizebits);
    }else{
        paddr = 0;
    }

    return paddr;
}

