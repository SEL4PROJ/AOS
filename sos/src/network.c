/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "network.h"

#include <autoconf.h>
#undef PACKED // picotcp complains as it redefines this macro
#include <pico_stack.h>
#include <pico_device.h>
#include <clock/timestamp.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <cspace/cspace.h>

#include "ringbuffer.h"
#include "vmem_layout.h"
#include "dma.h"
#include "mapping.h"
#include "ut.h"


#ifndef SOS_NFS_DIR
#  ifdef CONFIG_SOS_NFS_DIR
#    define SOS_NFS_DIR CONFIG_SOS_NFS_DIR
#  else
#    define SOS_NFS_DIR ""
#  endif
#endif

#define ARP_PRIME_TIMEOUT_MS     1000
#define ARP_PRIME_RETRY_DELAY_MS   10
#define N_DMA_BUFS 512
#define N_RX_BUFS 256
#define N_TX_BUFS 128
#define BUF_SIZE 2048

static struct pico_device pico_dev;

typedef struct {
    long buf_no;
    int length;
} rx_t;

/* declare our ring buffer data type macros */
ringBuffer_typedef(long, long_buf_t);
ringBuffer_typedef(rx_t, rx_buf_t);

/* our local structure for tracking network dma buffers */
static struct {
    /* list of allocated dma buffers */
    dma_addr_t dma_bufs[N_DMA_BUFS];

    /* ring buffer of free dma_bufs, tracked by indices into the dma_buf array */
    long_buf_t free_pool;

    /* ring buffer of recieve buffers, tracked by indices into the dma_buf array */
    rx_buf_t rx_queue;

    /* number of transmit buffers allocated */
    int n_tx_bufs;
} buffers;

/* remove a buffer from the pool of dma bufs */
static long alloc_dma_buf(void) {

    long_buf_t *pool = &buffers.free_pool;
    if (unlikely(isBufferEmpty(pool))) {
        ZF_LOGE("Out of preallocated eth buffers.");
        return -1;
    }

    int allocated_buf_no;
    bufferRead(pool, allocated_buf_no);
    return allocated_buf_no;
}

static void free_dma_buf(long buf_no)
{
    assert(buf_no < N_DMA_BUFS);
    long_buf_t *pool = &buffers.free_pool;
    assert(!isBufferFull(pool));
    bufferWrite(pool, buf_no);
}

static void init_buffers(void)
{
    memset(&buffers, 0, sizeof(buffers));

    /* initialise the free buffer pool */
    bufferInit(buffers.free_pool, N_DMA_BUFS, long);
    ZF_LOGF_IF(buffers.free_pool.elems == NULL, "Failed to calloc dma free pool");

    bufferInit(buffers.rx_queue, N_RX_BUFS, rx_t);
    ZF_LOGF_IF(buffers.rx_queue.elems == NULL, "Failed to calloc rx queue");

    long_buf_t *pool = &buffers.free_pool;

	/* allocate dma buffers for ethernet */
    for (int i = 0; i < N_DMA_BUFS; i++) {
        //TODO what is the drivers requirement for alignment
        buffers.dma_bufs[i] = sos_dma_malloc(BUF_SIZE, BUF_SIZE);
        ZF_LOGF_IF(buffers.dma_bufs[i].vaddr == 0, "Failed to dma malloc buffer %d", i);

        seL4_Error err = sos_dma_cache_clean_invalidate(buffers.dma_bufs[i].vaddr, BUF_SIZE);
        ZF_LOGF_IF(err != seL4_NoError, "Failed to clean invalidate buffer %d", i);
        bufferWrite(pool, i);
	}
}

/* pico TCP OS layer */
static UNUSED uintptr_t pico_allocate_rx_buf(UNUSED void *iface, size_t buf_size, void **cookie)
{
    if (buf_size > BUF_SIZE) {
        ZF_LOGE("Requested buf size %zu too large, max %zu", buf_size, (size_t) BUF_SIZE);
	    return 0;
    }

    long buf_no = alloc_dma_buf();
    if (buf_no == -1) {
        /* no buffers left! */
        return 0;
    }

    sos_dma_cache_invalidate(buffers.dma_bufs[buf_no].vaddr, BUF_SIZE);
    *cookie = (void *) buf_no;
    return buffers.dma_bufs[buf_no].paddr;
}

static UNUSED void pico_rx_complete(UNUSED void *iface, size_t num_bufs, void **cookies, int *lens)
{
    if (num_bufs > 1) {
        ZF_LOGW("Frame splitting not handled\n");

         /* Frame splitting is not handled. Warn and return bufs to pool. */
         for (unsigned int i = 0; i < num_bufs; i++) {
            free_dma_buf((long) cookies[i]);
        }
    } else {
        rx_buf_t *rx_queue = &buffers.rx_queue;
        assert(!isBufferFull(rx_queue));
        rx_t rx = {
            .buf_no = (long) cookies[0],
            .length = lens[0]
        };
        bufferWrite(rx_queue, rx);
        //pico_dev.__serving_interrupt = 1; // TODO use if async
    }
    return;
}

static UNUSED void pico_tx_complete(UNUSED void *iface, void *cookie)
{
    free_dma_buf((long) cookie);
    buffers.n_tx_bufs--;
}

static int pico_eth_send(UNUSED struct pico_device *dev, void *input_buf, int len)
{
    if (unlikely(len > BUF_SIZE)) {
        ZF_LOGE("Buffer size %d too big, max %d", len, BUF_SIZE);
        return 0;
    }

    if (buffers.n_tx_bufs == N_TX_BUFS) {
        return 0;
    }

    long buf_no = alloc_dma_buf();
    buffers.n_tx_bufs++;
    dma_addr_t *buf = &buffers.dma_bufs[buf_no];
    memcpy((void *) buf->vaddr, input_buf, len);
    sos_dma_cache_clean(buf->vaddr, len);
    int ret = len;
    //TODO this is what we need from the ethernet driver
    //eth_raw_tx(&buf->paddr, &ret, (void *) buf_no);
    //TODO probably need to handle some errors
    //TODO return how much was sent
    return ret;
}

static int pico_eth_poll(struct pico_device *dev, int loop_score)
{
    while (loop_score > 0) {
        rx_buf_t *rx_queue = &buffers.rx_queue;
        if (isBufferEmpty(rx_queue)) {
            // if async
            // eth_device->pico_dev.__serving_interrupt = 0;
            break;
        }

        /* get data from the rx buffer */
        rx_t rx;
        bufferRead(rx_queue, rx);
        sos_dma_cache_invalidate(buffers.dma_bufs[rx.buf_no].vaddr, rx.length);
        pico_stack_recv(dev, (void *) buffers.dma_bufs[rx.buf_no].vaddr, rx.length);
        free_dma_buf(rx.buf_no);
        loop_score--;
    }

    return loop_score;
}

void network_init(UNUSED cspace_t *cspace, UNUSED seL4_CPtr interrupt_ntfn)
{
    /* Extract IP from .config */
    struct pico_ip4 netmask;
    struct pico_ip4 ipaddr;
    struct pico_ip4 gateway;
    struct pico_ip4 zero;

    ZF_LOGI("\nInitialising network...\n\n");

    pico_stack_init();

    memset(&pico_dev, 0, sizeof(struct pico_device));

    init_buffers();

    pico_dev.send = pico_eth_send;
    pico_dev.poll = pico_eth_poll; // TODO NULL if async
    pico_dev.dsr = pico_eth_poll;

    //TODO init ethdriver here
    //needs to fill in mtu, mac
    //int mtu = 0;
    uint8_t mac[6] = {};
   // it probably also needs to call pico_tx_complete, pico_rx_complete and allocate_rx_buf

    int error = pico_device_init(&pico_dev, "sos picotcp", mac);
    ZF_LOGF_IF(error, "Failed to init picotcp");

    pico_string_to_ipv4(CONFIG_SOS_GATEWAY, &gateway.addr);
    pico_string_to_ipv4(CONFIG_SOS_NETMASK, &netmask.addr);
    pico_string_to_ipv4(CONFIG_SOS_IP, &ipaddr.addr);
    pico_string_to_ipv4("0.0.0.0", &zero.addr);

    pico_ipv4_link_add(&pico_dev, ipaddr, netmask);
    pico_ipv4_route_add(zero, zero, gateway, 1, NULL);

    //TODO from here we should set up a basic tcp echo to see if it works
    // then get libserial going, then libnfs

}
