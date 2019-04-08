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
#include <utils/util.h>

#include "../mapping.h"
#include "uart.h"

#define UART_PADDR 0xc81004c0

static volatile struct {
    uint32_t wfifo;
    uint32_t rfifo;
    uint32_t control;
    uint32_t status;
    uint32_t misc;
    uint32_t reg5;
} *uart;

#define UART_STATUS_TX_FIFO_FULL BIT(21)
#define UART_CONTROL_TX_ENABLE   BIT(12)

void uart_init(cspace_t *cspace)
{
    /* map the device */
    void *uart_vaddr = sos_map_device(cspace, PAGE_ALIGN_4K(UART_PADDR), PAGE_SIZE_4K);
    ZF_LOGF_IF(uart_vaddr == NULL, "Failed to map uart");

    uart = uart_vaddr + (UART_PADDR & MASK((size_t) seL4_PageBits));

    /* enable the device */
    uart->control |= UART_CONTROL_TX_ENABLE;
}

void uart_putchar(char c)
{
    /* spin until there is space in the buffer */
    while (uart->status & UART_STATUS_TX_FIFO_FULL);
    uart->wfifo = c;

    if (c == '\n') {
        uart_putchar('\r');
    }
}
