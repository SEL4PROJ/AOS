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

/* Constants for the layout of the SOS address space */

/* Address where memory used for DMA starts getting mapped.
 * Do not use the address range between SOS_DMA_VSTART and SOS_DMA_VEND */
#define SOS_DMA_SIZE_BITS    (seL4_LargePageBits)

#define SOS_SCRATCH          (0xA0000000)
#define SOS_DEVICE_START     (0xB0000000)
#define SOS_STACK            (0xC0000000)
#define SOS_STACK_PAGES      100
#define SOS_UT_TABLE         (0x8000000000)
#define SOS_FRAME_TABLE      (0x8100000000)
#define SOS_FRAME_DATA       (0x8200000000)

/* Constants for how SOS will layout the address space of any processes it loads up */
#define PROCESS_STACK_TOP   (0x90000000)
#define PROCESS_IPC_BUFFER  (0xA0000000)
#define PROCESS_VMEM_START  (0xC0000000)

