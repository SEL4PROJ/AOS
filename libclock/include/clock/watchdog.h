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

#include <utils/io.h>
#include <utils/util.h>

/* This file provides the hardware details about the watchdog timer on the odroid-c2. This device
 * is used by the network & reset infrastructure of this project. It has a different programming model
 * to ordinary timers. Do not use it for your timer driver! See the TIMER chapter, 26, in the manual
 * for further info, and device.h for registers intended for use by your timer driver */

#define WATCHDOG_IRQ              (32)

/* Watchdog device offset from TIMER_PADDR as defined in device.h */
#define WDOG_OFFSET               0x8D0

/* Register offsets from TIMER_PADDR+WDOG_OFFSET */
#define WDOG_CNTL                 0x0
#define WDOG_TCNT                 0x8
#define WDOG_RESET                0xC

/* Parts of CNTL register */
#define WDOG_CNTL_EN              BIT(18)
#define WDOG_CNTL_SYS_RESET_EN    BIT(21)
#define WDOG_CNTL_INTERRUPT_EN    BIT(23)
#define WDOG_CNTL_CLK_EN          BIT(24)
#define WDOG_CNTL_CLK_DIV_EN      BIT(25)
#define WDOG_CNTL_SYS_RESET_NOW   BIT(26)
#define WDOG_CNTL_CNTL_WDOG_RESET BIT(31)

static volatile void *wdog = NULL;

static inline void watchdog_init(void *timer_vaddr, uint16_t timeout_us)
{
    wdog = timer_vaddr + WDOG_OFFSET;

    /* enable the watchdog timer in interrupt mode */
    RAW_WRITE32(
        (WDOG_CNTL_EN |
         WDOG_CNTL_CLK_EN |
         WDOG_CNTL_CLK_DIV_EN |
         WDOG_CNTL_INTERRUPT_EN |
         24),
        wdog + WDOG_CNTL); /* For 24MHz => 1 watchdog tick/us */
    COMPILER_MEMORY_FENCE();

    /* Set the expiry and restart it */
    RAW_WRITE32(timeout_us, wdog + WDOG_TCNT);
    RAW_WRITE32(0, wdog + WDOG_RESET);
    COMPILER_MEMORY_FENCE();
}

static inline void watchdog_reset(void)
{
    ZF_LOGF_IF(wdog == NULL, "WDOG uninitialised!");

    RAW_WRITE32(0, wdog + WDOG_RESET);
    COMPILER_MEMORY_FENCE();
}
