/*
 * Copyright 2018, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
#include <errno.h>
#include <assert.h>

#include "uboot/common.h"
#include "uboot/net.h"
#include "uboot/miiphy.h"
#include "io.h"
#include "uboot/netdev.h"
#include "unimplemented.h"

#include <ethernet/ethernet.h>
#include <utils/zf_log.h>

static struct eth_device uboot_eth_dev;

ethif_dma_ops_t dma_ops = {NULL};
ethif_recv_callback_t ethif_recv_callback = NULL;

ethif_err_t ethif_send(uint8_t *buf, uint32_t len)
{
    assert(buf);
    return (uboot_eth_dev.send(&uboot_eth_dev, buf, len) == 0) ? ETHIF_NOERROR : ETHIF_ERROR;
}

ethif_err_t ethif_recv(int *len)
{
    assert(len);
    int result = uboot_eth_dev.recv(&uboot_eth_dev);
    if(result >= 0) {
        *len = result;
        return ETHIF_NOERROR;
    }
    return ETHIF_ERROR;
}

void uboot_process_received_packet(uint8_t *in_packet, int len)
{
    ethif_recv_callback(in_packet, len);
}

ethif_dma_ops_t *uboot_get_dma_ops()
{
    return &dma_ops;
}

int designware_ack(struct eth_device *dev);
void ethif_irq(void)
{
    designware_ack(&uboot_eth_dev);
}

ethif_err_t ethif_init(uint64_t base_addr, const uint8_t mac[6], ethif_dma_ops_t *ops,
                       ethif_recv_callback_t recv_callback)
{
    assert(ops);
    assert(recv_callback);

    ZF_LOGI("Initialising ethernet interface...");

    /* Save a copy of the DMA ops for use by the driver */
    memcpy(&dma_ops, ops, sizeof(ethif_dma_ops_t));

    ethif_recv_callback = recv_callback;

    uboot_timer_init();

    /* Initialise the MII abstraction layer */
    miiphy_init();

    /* Initialise the actual Realtek PHY */
    phy_init();

    /* Populate the eth_dev functions, also does some more PHY init */
    int ret = designware_initialize(base_addr, 0, &uboot_eth_dev);

    if (ret != 0) {
        ZF_LOGE("Failed: designware_initialize.");
        return ETHIF_ERROR;
    }

    /* NOTE: This line determines what our MAC address will be, it is
     * internally programmed into the device during the next init step*/
    memcpy(uboot_eth_dev.enetaddr, mac, 6);

    /* Bring up the interface - the last initialisation step */
    ret = uboot_eth_dev.init(&uboot_eth_dev);

    if (ret != 0) {
        ZF_LOGE("Failed: uboot_eth_dev.init().");
        return ETHIF_ERROR;
    }

    ZF_LOGI("interface UP");

    return ETHIF_NOERROR;
}
