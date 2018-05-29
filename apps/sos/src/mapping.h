/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _MAPPING_H_
#define _MAPPING_H_

#include <sel4/sel4.h>

 /**
 * Maps a page into a page table. 
 * A 2nd level table will be created if required
 *
 * @param frame_cap a capbility to the page to be mapped
 * @param pd A capability to the page directory to map to
 * @param vaddr The virtual address for the mapping
 * @param rights The access rights for the mapping
 * @param attr The VM attributes to use for the mapping
 * @return 0 on success
 */
int map_page(seL4_CPtr frame_cap, seL4_ARM_PageDirectory pd, seL4_Word vaddr, 
                seL4_CapRights rights, seL4_ARM_VMAttributes attr);
 
 /**
 * Maps a device to virtual memory
 * A 2nd level table will be created if required
 *
 * @param paddr the physical address of the device
 * @param size the number of bytes that this device occupies
 * @return The new virtual address of the device
 */
void* map_device(void* paddr, int size);

#endif /* _MAPPING_H_ */
