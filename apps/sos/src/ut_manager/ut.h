/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _UT_H_
#define _UT_H_

#include <sel4/sel4.h>


/* 
 * To abstract the details of untyped objects, we must restrict the
 * minimum size_bits of untyped objects such that any object can be
 * allocated provided that the address is aligned correctly
 */
#define MIN_UT_SIZE_BITS seL4_PageDirBits


/*
 * Linear address mapping
 *                      high  _____
 *  _____                    |ADDR |
 * |UT   |                   |     |
 * |_____|     _____         |     |
 *            |UT   |        |     |
 *            |     |        |     |
 *   _____    |     |   <=>  |     |
 *  |UT   |   |     |   <=>  |     |
 *  |     |   |     |        |     |
 *  |     |   |     |        |     |
 *  |_____|   |     |        |     |
 *            |_____|        |     |
 *                           |     |
 *                      low  |_____|
 */



/**
 * initialises the untyped->linear  mapping system
 * @param bi seL4 boot information
 * @return 0 on success
 */
int ut_table_init(const seL4_BootInfo *bi);

/**
 * Returns a valid memory range that can be used for ut_translate
 * @param low on return, contains the lowest valid address
 * @param high on return, contains the highest valid address +1
 */
void ut_find_memory(seL4_Word* low, seL4_Word* high);

/**
 * Steals memory from the untyped abstraction layer
 * @pre ut table must be initialised
 * @param sizebits the size of memory to steal in bits
 * @return a reserved size aligned memory region
 */
seL4_Word ut_steal_mem(int sizebits);


/**
 * Translates an address in to a untyped object capability pointer and offset
 * @param addr the address to translate
 * @param ret_cptr the returned untyped capability
 * @param ret_offset the returned offset within the untyped offset
 * @return 0 on success
 */
int ut_translate(seL4_Word addr, seL4_Untyped* ret_cptr, seL4_Word* ret_offset);

/**
 * Initialise the allocator to manage memory from "low" to "high"
 * @param low the base address that the allocator should manage
 * @param high the last address that the allocator should manage
 */
void ut_allocator_init(seL4_Word low, seL4_Word high);

/**
 * Reserve memory using the allocator
 * @param sizebits the amount of contiguous and aligned memory to reserve (2^sizebits)
 * @return the physical address of the reserved memory which can be passed to ut_translate
 */
seL4_Word ut_alloc(int sizebits);

/**
 * Free reserved memory
 * @param addr the address that should be freed
 * @param sizebits the size that was used for the allocation
 */
void ut_free(seL4_Word addr, int sizebits);

#endif /* _UT_H_ */

