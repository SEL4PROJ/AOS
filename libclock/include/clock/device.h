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
#pragma once

#include <utils/util.h>

/* This file provides the hardware details about the four GPT timers on the odroid-c2. See
 * the TIMER chapter, 26, in the manual for further info */

#define TIMER_MUX 0xc1109990
/* this is the frame we actually need to map */
#define TIMER_PADDR PAGE_ALIGN_4K(TIMER_MUX)

/* Four 16 bit timers which count down to 0 */
#define TIMER_F_PADDR    0xc1109994
#define TIMER_G_PADDR    0xc1109998
#define TIMER_H_PADDR    0xc110999c
#define TIMER_I_PADDR    0xc11099a0

/* Bits in the TIMER_MUX register */

/* Set to 1 to enable respective timer */
#define TIMER_I_EN   BIT(19)
#define TIMER_H_EN   BIT(18)
#define TIMER_G_EN   BIT(17)
#define TIMER_F_EN   BIT(16)

/* Set to 1 to set timer to periodic, 0 for one-shot */
#define TIMER_I_MODE BIT(15)
#define TIMER_H_MODE BIT(14)
#define TIMER_G_MODE BIT(13)
#define TIMER_F_MODE BIT(12)

/* these fields are two bits and select the input clock for each timer, values follow */
#define TIMER_I_INPUT_CLK 6
#define TIMER_H_INPUT_CLK 4
#define TIMER_G_INPUT_CLK 2
#define TIMER_F_INPUT_CLK 0

/* values for the timer input clock bits above */
#define TIMEBASE_1_US    0b00 // tick every us
#define TIMEBASE_10_US   0b01 // tick every 10 us
#define TIMEBASE_100_US  0b10 // tick every 100 us
#define TIMEBASE_1000_US 0b11 // tick every 1 ms

/* timers are only 16 bits */
#define TIMER_MAX 0xFFFF

/* A macro to get the timer value from the timer register. The top 16 bits are the
 * current value of the counter, and the bottom 16 bits are the value to start counting from.
 * If the timer is enabled, writing to the bottom 16 bits starts the counter.
 * If the timer is periodic, when the counter gets to 0 it will reload from the bottom 16 bits.
 */
#define TIMER_VAL(x) ((x) >> 16u)

/* Interrupt numbers for the timers */

#define TIMER_I_IRQ 95
#define TIMER_H_IRQ 94
#define TIMER_G_IRQ 93
#define TIMER_F_IRQ 92
