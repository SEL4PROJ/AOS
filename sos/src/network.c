/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/****************************************************************************
 *
 *      $Id: network.c,v 1.1 2003/09/10 11:44:38 benjl Exp $
 *
 *      Description: Initialise the network stack and NFS library.
 *
 *      Author:      Ben Leslie
 *
 ****************************************************************************/

#include "network.h"

#include <autoconf.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <nfs/nfs.h>
#include <lwip/init.h>
#include <netif/etharp.h>
#include <ethdrivers/lwip.h>
#include <ethdrivers/imx6.h>
#include <cspace/cspace.h>

#include "dma.h"
#include "mapping.h"
#include "ut_manager/ut.h"

#define verbose 0
#include <sys/debug.h>
#include <sys/panic.h>


#ifndef SOS_NFS_DIR
#  ifdef CONFIG_SOS_NFS_DIR
#    define SOS_NFS_DIR CONFIG_SOS_NFS_DIR
#  else
#    define SOS_NFS_DIR ""
#  endif
#endif

#define ARP_PRIME_TIMEOUT_MS     1000
#define ARP_PRIME_RETRY_DELAY_MS   10

#define ETHERNET_IRQ 150

extern const seL4_BootInfo* _boot_info;

static struct net_irq {
    int irq;
    seL4_IRQHandler cap;
} _net_irqs[1];

static seL4_CPtr _irq_ep;

fhandle_t mnt_point = { { 0 } };

lwip_iface_t *lwip_iface;

/*******************
 ***  OS support ***
 *******************/

static void *
sos_map_device(void* cookie, uintptr_t addr, size_t size, int cached, ps_mem_flags_t flags){
    (void)cookie;
    return map_device((void*)addr, size);
}

static void
sos_unmap_device(void *cookie, void *addr, size_t size) {
}

void 
sos_usleep(int usecs) {
    /* We need to spin because we do not as yet have a timer interrupt */
    while(usecs-- > 0){
        /* Assume 1 GHz clock */
        volatile int i = 1000;
        while(i-- > 0);
        seL4_Yield();
    }

    /* Handle pending network traffic */
    ethif_lwip_poll(lwip_iface);
}

/*******************
 *** IRQ handler ***
 *******************/
void 
network_irq(void) {
    int err;
    /* skip if the network was not initialised */
    if(_irq_ep == seL4_CapNull){
        return;
    }
    ethif_lwip_handle_irq(lwip_iface, _net_irqs[0].irq);
    err = seL4_IRQHandler_Ack(_net_irqs[0].cap);
    assert(!err);
}

static seL4_CPtr
enable_irq(int irq, seL4_CPtr aep) {
    seL4_CPtr cap;
    int err;
    /* Create an IRQ handler */
    cap = cspace_irq_control_get_cap(cur_cspace, seL4_CapIRQControl, irq);
    conditional_panic(!cap, "Failed to acquire and IRQ control cap");
    /* Assign to an end point */
    err = seL4_IRQHandler_SetEndpoint(cap, aep);
    conditional_panic(err, "Failed to set interrupt endpoint");
    /* Ack the handler before continuing */
    err = seL4_IRQHandler_Ack(cap);
    conditional_panic(err, "Failure to acknowledge pending interrupts");
    return cap;
}

/********************
 *** Network init ***
 ********************/

static void
network_prime_arp(struct ip_addr *gw){
    int timeout = ARP_PRIME_TIMEOUT_MS;
    struct eth_addr* eth;
    struct ip_addr* ip;
    while(timeout > 0){
        /* Send an ARP request */
        etharp_request(lwip_iface->netif, gw);
        /* Wait for the response */
        sos_usleep(ARP_PRIME_RETRY_DELAY_MS * 1000);
        if(etharp_find_addr(lwip_iface->netif, gw, &eth, &ip) == -1){
            timeout += ARP_PRIME_RETRY_DELAY_MS;
        }else{
            return;
        }
    }
}

void 
network_init(seL4_CPtr interrupt_ep) {
    struct ip_addr netmask, ipaddr, gw;
    int err;

    ps_io_mapper_t io_mapper = {
        .cookie = NULL,
        .io_map_fn = sos_map_device,
        .io_unmap_fn = sos_unmap_device
    };
    ps_dma_man_t dma_man = {
        .cookie = NULL,
        .dma_alloc_fn = sos_dma_malloc,
        .dma_free_fn = sos_dma_free,
        .dma_pin_fn = sos_dma_pin,
        .dma_unpin_fn = sos_dma_unpin,
        .dma_cache_op_fn = sos_dma_cache_op
    };

    ps_io_ops_t io_ops = {
        .io_mapper = io_mapper,
        .dma_manager = dma_man
    };

    _irq_ep = interrupt_ep;

    /* Extract IP from .config */
    printf("\nInitialising network...\n\n");
    err = 0;
    err |= !ipaddr_aton(CONFIG_SOS_GATEWAY,      &gw);
    err |= !ipaddr_aton(CONFIG_SOS_IP     ,  &ipaddr);
    err |= !ipaddr_aton(CONFIG_SOS_NETMASK, &netmask);
    conditional_panic(err, "Failed to parse IP address configuration");
    printf("  Local IP Address: %s\n", ipaddr_ntoa( &ipaddr));
    printf("Gateway IP Address: %s\n", ipaddr_ntoa(     &gw));
    printf("      Network Mask: %s\n", ipaddr_ntoa(&netmask));
    printf("\n");

    /* low level initialisation */
    lwip_iface = ethif_new_lwip_driver(io_ops, NULL, ethif_imx6_init, NULL);
    assert(lwip_iface);

    /* Initialise IRQS */
    _net_irqs[0].irq = ETHERNET_IRQ;
    _net_irqs[0].cap = enable_irq(_net_irqs[0].irq, _irq_ep);

    /* Setup the network interface */
    lwip_init();
    struct netif *netif = malloc(sizeof(*netif));
    assert(netif);
    lwip_iface->netif = netif_add(netif, &ipaddr, &netmask, &gw,
                         lwip_iface, ethif_get_ethif_init(lwip_iface), ethernet_input);
    assert(lwip_iface->netif != NULL);
    netif_set_up(lwip_iface->netif);
    netif_set_default(lwip_iface->netif);

    /*
     * LWIP does not queue packets while waiting for an ARP response 
     * Generally this is okay as we block waiting for a response to our
     * request before sending another. On the other hand, priming the
     * table is cheap and can save a lot of heart ache 
     */
    network_prime_arp(&gw);

    /* initialise and mount NFS */
    if(strlen(SOS_NFS_DIR)) {
        /* Initialise NFS */
        int err;
        printf("\nMounting NFS\n");
        if(!(err = nfs_init(&gw))){
            /* Print out the exports on this server */
            nfs_print_exports();
            if ((err = nfs_mount(SOS_NFS_DIR, &mnt_point))){
                printf("Error mounting path '%s'!\n", SOS_NFS_DIR);
            }else{
                printf("\nSuccessfully mounted '%s'\n", SOS_NFS_DIR);
            }
        }
        if(err){
            WARN("Failed to initialise NFS\n");
        }
    }else{
        WARN("Skipping Network initialisation since no mount point was "
             "specified\n");
    }
}
