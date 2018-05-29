/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _MEM_LAYOUT_H_
#define _MEM_LAYOUT_H_

/* Address where memory used for DMA starts getting mapped.
 * Do not use the address range between DMA_VSTART and DMA_VEND */
#define DMA_VSTART          (0x10000000)
#define DMA_SIZE_BITS       (22)
#define DMA_VEND            (DMA_VSTART + (1ull << DMA_SIZE_BITS))

/* From this address onwards is where any devices will get mapped in
 * by the map_device function. You should not use any addresses beyond
 * here without first modifying map_device */
#define DEVICE_START        (0xB0000000)

#define ROOT_VSTART         (0xC0000000)

/* Constants for how SOS will layout the address space of any
 * processes it loads up */
#define PROCESS_STACK_TOP   (0x90000000)
#define PROCESS_IPC_BUFFER  (0xA0000000)
#define PROCESS_VMEM_START  (0xC0000000)

#define PROCESS_SCRATCH     (0xD0000000)


#endif /* _MEM_LAYOUT_H_ */
