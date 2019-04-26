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
#include "network.h"

#include <autoconf.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>

#include <cspace/cspace.h>
#include <clock/timestamp.h>
#include <clock/watchdog.h>

#undef PACKED // picotcp complains as it redefines this macro
#include <pico_stack.h>
#include <pico_device.h>
#include <pico_config.h>
#include <pico_ipv4.h>
#include <pico_socket.h>
#include <pico_nat.h>
#include <pico_icmp4.h>
#include <pico_dns_client.h>
#include <pico_dev_loop.h>
#include <pico_dhcp_client.h>
#include <pico_dhcp_server.h>
#include <pico_ipfilter.h>
#include "pico_bsd_sockets.h"

#include <ethernet/ethernet.h>

#include <nfsc/libnfs.h>

#include "vmem_layout.h"
#include "dma.h"
#include "mapping.h"
#include "irq.h"
#include "ut.h"


#ifndef SOS_NFS_DIR
#  ifdef CONFIG_SOS_NFS_DIR
#    define SOS_NFS_DIR CONFIG_SOS_NFS_DIR
#  else
#    define SOS_NFS_DIR ""
#  endif
#endif

#define NETWORK_IRQ (40)
#define WATCHDOG_TIMEOUT 1000

static struct pico_device pico_dev;
static struct nfs_context *nfs = NULL;
static void nfs_mount_cb(int status, struct nfs_context *nfs, void *data, void *private_data);

static int pico_eth_send(UNUSED struct pico_device *dev, void *input_buf, int len)
{
    if (ethif_send(input_buf, len) != ETHIF_NOERROR) {
        /* If we get an error, just report that we didn't send anything */
        return 0;
    }
    /* Currently assuming that sending always succeeds unless we get an error code.
     * Given how the u-boot driver is structured, this seems to be a safe assumption. */
    return len;
}

static int pico_eth_poll(UNUSED struct pico_device *dev, int loop_score)
{
    while (loop_score > 0) {
        int len;
        int retval = ethif_recv(&len); /* This will internally call 'raw_recv_callback'
                                        * (if a packet is actually available) */
        if (retval == ETHIF_ERROR || len == 0) {
            break;
        }
        loop_score--;
    }

    /* return (original_loop_score - amount_of_packets_received) */
    return loop_score;
}

/* Called by ethernet driver when a frame is received (inside an ethif_recv()) */
void raw_recv_callback(uint8_t *in_packet, int len)
{
    /* Note that in_packet *must* be copied somewhere in this function, as the memory
     * will be re-used by the ethernet driver after this function returns. */
    pico_stack_recv(&pico_dev, in_packet, len);
}

/* This is a bit of a hack - we need a DMA size field in the ethif driver. */
ethif_dma_addr_t ethif_dma_malloc(uint32_t size, uint32_t align)
{
    dma_addr_t addr = sos_dma_malloc(size, align);
    ethif_dma_addr_t eaddr =
    { .paddr = addr.paddr, .vaddr = addr.vaddr, .size = size };
    ZF_LOGD("ethif_dma_malloc -> vaddr: %lx, paddr: %lx\n, sz: %lx",
            eaddr.vaddr, eaddr.paddr, eaddr.size);
    return eaddr;
}

void nfslib_poll()
{
    struct pollfd pfd = {
        .fd = nfs_get_fd(nfs),
        .events = nfs_which_events(nfs)
    };

    /* Poll with zero timeout, so we return immediately */
    int poll_ret = poll(&pfd, 1, 0);

    ZF_LOGF_IF(poll_ret < 0, "poll() failed");

    if (poll_ret == 0) {
        /* Nothing of interest to NFS happened on the IP stack since last
         * time we checked, so don't bother continuing */
        return;
    }

    if (nfs_service(nfs, pfd.revents) < 0) {
        printf("nfs_service failed\n");
    }
}

static void network_tick_internal(void)
{
    pico_bsd_stack_tick();
    nfslib_poll();
}

/* Handler for IRQs from the ethernet MAC */
static int network_irq(
    UNUSED void *data,
    UNUSED seL4_Word irq,
    seL4_IRQHandler irq_handler
)
{
    ethif_irq();
    seL4_IRQHandler_Ack(irq_handler);
    network_tick_internal();
    return 0;
}

/* Handler for IRQs from the watchdog timer */
static int network_tick(
    UNUSED void *data,
    UNUSED seL4_Word irq,
    seL4_IRQHandler irq_handler
)
{
    network_tick_internal();
    watchdog_reset();
    seL4_IRQHandler_Ack(irq_handler);
    return 0;
}

static void init_irq(
    int irq_number,
    int edge_triggered,
    sos_irq_callback_t callback
)
{
    seL4_IRQHandler irq_handler = 0;
    int init_irq_err = sos_register_irq_handler(irq_number, edge_triggered, callback, NULL, &irq_handler);
    ZF_LOGF_IF(init_irq_err != 0, "Failed to initialise IRQ");
    seL4_IRQHandler_Ack(irq_handler);
}

void network_init(cspace_t *cspace, void *timer_vaddr)
{
    int error;
    ZF_LOGI("\nInitialising network...\n\n");

    /* set up the network device irq */
    init_irq(NETWORK_IRQ, true, network_irq);

    /* set up the network tick irq (watchdog timer) */
    init_irq(WATCHDOG_IRQ, true, network_tick);

    /* Configure a watchdog IRQ for 1 millisecond from now. Whenever the watchdog is reset
     * using watchdog_reset(), we will get another IRQ 1ms later */
    watchdog_init(timer_vaddr, WATCHDOG_TIMEOUT);

    /* Initialise ethernet interface first, because we won't bother initialising
     * picotcp if the interface fails to be brought up */

    /* Map the ethernet MAC MMIO registers into our address space */
    uint64_t eth_base_vaddr =
        (uint64_t)sos_map_device(cspace, ODROIDC2_ETH_PHYS_ADDR, ODROIDC2_ETH_PHYS_SIZE);

    /* Populate DMA operations required by the ethernet driver */
    ethif_dma_ops_t ethif_dma_ops;
    ethif_dma_ops.dma_malloc = &ethif_dma_malloc;
    ethif_dma_ops.dma_phys_to_virt = &sos_dma_phys_to_virt;
    ethif_dma_ops.flush_dcache_range = &sos_dma_cache_clean_invalidate;
    ethif_dma_ops.invalidate_dcache_range = &sos_dma_cache_invalidate;

    /* Try initializing the device.
     *
     * This function will also check what MAC address u-boot programmed into
     * the interface, copy it into mac_addr, and reprogram it into the interface */

    uint8_t mac_addr[6];
    error = ethif_init(eth_base_vaddr, mac_addr, &ethif_dma_ops, &raw_recv_callback);
    ZF_LOGF_IF(error != 0, "Failed to initialise ethernet interface");

    /* Extract IP from .config */
    struct pico_ip4 netmask;
    struct pico_ip4 ipaddr;
    struct pico_ip4 gateway;
    struct pico_ip4 zero;

    pico_bsd_init();
    pico_stack_init();

    memset(&pico_dev, 0, sizeof(struct pico_device));

    pico_dev.send = pico_eth_send;
    pico_dev.poll = pico_eth_poll;

    pico_dev.mtu = MAXIMUM_TRANSFER_UNIT;

    error = pico_device_init(&pico_dev, "sos picotcp", mac_addr);
    ZF_LOGF_IF(error, "Failed to init picotcp");

    pico_string_to_ipv4(CONFIG_SOS_GATEWAY, &gateway.addr);
    pico_string_to_ipv4(CONFIG_SOS_NETMASK, &netmask.addr);
    pico_string_to_ipv4(CONFIG_SOS_IP, &ipaddr.addr);
    pico_string_to_ipv4("0.0.0.0", &zero.addr);

    pico_ipv4_link_add(&pico_dev, ipaddr, netmask);
    pico_ipv4_route_add(zero, zero, gateway, 1, NULL);

    nfs = nfs_init_context();
    ZF_LOGF_IF(nfs == NULL, "Failed to init NFS context");

    nfs_set_debug(nfs, 10);
    int ret = nfs_mount_async(nfs, CONFIG_SOS_GATEWAY, SOS_NFS_DIR, nfs_mount_cb, NULL);
    ZF_LOGF_IF(ret != 0, "NFS Mount failed: %s", nfs_get_error(nfs));
}

void nfs_mount_cb(int status, UNUSED struct nfs_context *nfs, void *data,
                  UNUSED void *private_data)
{
    if (status < 0) {
        ZF_LOGF("mount/mnt call failed with \"%s\"\n", (char *)data);
    }

    printf("Mounted nfs dir %s\n", SOS_NFS_DIR);
}
