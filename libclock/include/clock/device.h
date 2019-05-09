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

#define TIMER_BASE      0xc1100000
#define TIMER_MAP_BASE  0xc1109000

#define TIMER_REG_START   0x940    // TIMER_MUX

/**
 * Identifiers for each of the timeout timers.
 *
 * Whilst there are 8 countdown timers provided by the hardware, we only
 * make 4 accessible here as timers F-I cannot have their current
 * timeout read.
 */
typedef enum {
    MESON_TIMER_A,
    MESON_TIMER_B,
    MESON_TIMER_C,
    MESON_TIMER_D,
} timeout_id_t;

/**
 * Get the IRQ associated with a particular timer.
 *
 * @param timer  The timer to find the associated IRQ for.
 * @return       The IRQ number needed to handle timeouts for the timer.
 */
seL4_Word meson_timeout_irq(timeout_id_t timer);
