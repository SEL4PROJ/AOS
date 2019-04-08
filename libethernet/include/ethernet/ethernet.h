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

#include <stdint.h>

/**
 * This file provides a low-level interface to the designware ethernet MAC on this platform.
 * Link-layer frames move between this driver and higher layers through ethif_send/recv.
 * Most of this driver is ported from u-boot.
 */

/**
 * MMIO address of the ethernet MAC, for use by sos_map_device. The virtual address
 * that the device is mapped at should then be passed to ethif_init as the base address.
 */
#define ODROIDC2_ETH_PHYS_ADDR      0xc9410000
#define ODROIDC2_ETH_PHYS_SIZE         0x10000

#define MAXIMUM_TRANSFER_UNIT             1500

typedef enum {
    ETHIF_NOERROR = 0,
    ETHIF_ERROR = -1
} ethif_err_t;

typedef struct {
    uintptr_t vaddr; /* virtual  address of the allocated DMA memory */
    uintptr_t paddr; /* physical address of the allocated DMA memory */
    size_t size;     /* size of the allocated DMA memory */
} ethif_dma_addr_t;

/*
 * DMA operations required by the ethernet interface driver
 */
typedef struct {
    ethif_dma_addr_t (*dma_malloc)(uint32_t size, uint32_t align);
    uintptr_t (*dma_phys_to_virt)(uintptr_t phys);
    uint32_t (*flush_dcache_range)(uintptr_t addr, size_t size);
    uint32_t (*invalidate_dcache_range)(uintptr_t addr, size_t size);
} ethif_dma_ops_t;

/**
 * Called by ethernet driver when a frame is received (inside an ethif_recv())
 * This function must be defined by code which uses this driver, and passed into ethif_init.
 *
 * See ethif_recv for more information.
 *
 * @param in_packet packet received - *must* be copied in this function, memory will be re-used later
 * @param len       length of received packet
 */
typedef void (*ethif_recv_callback_t)(uint8_t *in_packet, int len);

/**
 * Initialise the ethernet interface.
 *
 * @param base_addr      virtual address of the ethernet MAC (from sos_map_device)
 * @param mac            pointer to MAC address to be populated (using u-boot settings)
 * @param ops            populated ethif_dma_ops_t containg DMA operations for use by the driver
 * @param recv_callback  user-defined ethif_recv_callback_t.
 * @return ethif_err_t
 */
ethif_err_t ethif_init(uint64_t base_addr, uint8_t mac_out[6], ethif_dma_ops_t *ops,
                       ethif_recv_callback_t recv_callback);

/**
 * Queue the provided frame for transmission, and send at next opportunity.
 *
 * This function does not block.
 * If there are no free transmit descriptors, the packet will be dropped and ETHIF_ERROR returned
 *
 * @param buf   buffer containing frame contents to send
 * @param len   length of buffer to send
 * @return ethif_err_t
 */
ethif_err_t ethif_send(uint8_t *buf, uint32_t len);

/**
 * Poll the receive buffers for a packet.
 *
 * This function does not block.
 * Calls user-defined 'ethif_recv_callback_t' if a packet arrives, with the packet contents.
 * Note that ETHIF_NOERROR is still returned if no packets are available, however the packet
 * length will be set to 0.
 *
 * @param len  pointer to variable to store packet length (0 if no packets)
 * @return ethif_err_t
 */
ethif_err_t ethif_recv(int *len);

void ethif_irq(void);
