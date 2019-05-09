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
#include <stdbool.h>
#include <utils/util.h>
#include <clock/device.h>

/* IRQs */

#define TIMER_A_IRQ  42
#define TIMER_B_IRQ  43
#define TIMER_C_IRQ  38
#define TIMER_D_IRQ  61

#define TIMER_F_IRQ  92
#define TIMER_G_IRQ  93
#define TIMER_H_IRQ  94
#define TIMER_I_IRQ  95

/* Timer mux 0 */

#define TIMER_E_INPUT_CLK  8

#define TIMER_A_INPUT_CLK  0
#define TIMER_B_INPUT_CLK  2
#define TIMER_C_INPUT_CLK  4
#define TIMER_D_INPUT_CLK  6

#define TIMER_A_EN    BIT(16)
#define TIMER_B_EN    BIT(17)
#define TIMER_C_EN    BIT(18)
#define TIMER_D_EN    BIT(19)
#define TIMER_A_MODE  BIT(12)
#define TIMER_B_MODE  BIT(13)
#define TIMER_C_MODE  BIT(14)
#define TIMER_D_MODE  BIT(15)

/* Timer mux 1 */

#define TIMER_F_INPUT_CLK  0
#define TIMER_G_INPUT_CLK  2
#define TIMER_H_INPUT_CLK  4
#define TIMER_I_INPUT_CLK  6

#define TIMER_F_EN    BIT(16)
#define TIMER_G_EN    BIT(17)
#define TIMER_H_EN    BIT(18)
#define TIMER_I_EN    BIT(19)
#define TIMER_F_MODE  BIT(12)
#define TIMER_G_MODE  BIT(13)
#define TIMER_H_MODE  BIT(14)
#define TIMER_I_MODE  BIT(15)

#define TIMESTAMP_TIMEBASE_MASK  MASK(3)
#define TIMEOUT_TIMEBASE_MASK    MASK(2)

/**
 * The layout of the timer device in memory.
 */
typedef struct {
    uint32_t mux;
    uint32_t timer_a;
    uint32_t timer_b;
    uint32_t timer_c;
    uint32_t timer_d;
    uint32_t unused[13];
    uint32_t timer_e;
    uint32_t timer_e_hi;
    uint32_t mux1;
    uint32_t timer_f;
    uint32_t timer_g;
    uint32_t timer_h;
    uint32_t timer_i;
} meson_timer_reg_t;

/**
 * Frequency configuration options for the timestamp of the timer.
 */
typedef enum {
    /* Synchronise with system */
    TIMESTAMP_TIMEBASE_SYSTEM = 0b000,
    /* Tick every 1us */
    TIMESTAMP_TIMEBASE_1_US = 0b001,
    /* Tick every 10us */
    TIMESTAMP_TIMEBASE_10_US = 0b010,
    /* Tick every 100us */
    TIMESTAMP_TIMEBASE_100_US = 0b011,
    /* Tick every 1ms */
    TIMESTAMP_TIMEBASE_1_MS = 0b100,
} timestamp_timebase_t;

/**
 * Frequency configuration options for the timeout timers.
 */
typedef enum {
    /* Tick every 1us */
    TIMEOUT_TIMEBASE_1_US = 0b00,
    /* Tick every 10us */
    TIMEOUT_TIMEBASE_10_US = 0b01,
    /* Tick every 100us */
    TIMEOUT_TIMEBASE_100_US = 0b10,
    /* Tick every 1ms */
    TIMEOUT_TIMEBASE_1_MS = 0b11,
} timeout_timebase_t;

/**
 * Configure the counter timer (timer E) to tick with a given frequency.
 *
 * @param regs      The timer registers.
 * @param timebase  Timer tick frequency for the timestamp.
 */
void configure_timestamp(volatile meson_timer_reg_t *regs, timestamp_timebase_t timebase);

/**
 * Read the current value from the timestamp timer (timer E).
 *
 * @param regs  The timer registers.
 * @return      The current value of the timestamp.
 */
uint64_t read_timestamp(volatile meson_timer_reg_t *regs);

/**
 * Configure a timeout timer.
 *
 * @param regs      The timer registers.
 * @param timer     The timer to be configured.
 * @param enable    Whether or not to enable the timer.
 * @param periodic  Whether the timer should fire periodically (or just
 *                  once).
 * @param timebase  Timer tick frequency.
 */
void configure_timeout(
    volatile meson_timer_reg_t *regs,
    timeout_id_t timer,
    bool enable,
    bool periodic,
    timeout_timebase_t timebase,
    uint16_t timeout
);

/**
 * Read the time remaining on a timeout.
 *
 * @param regs   The timer registers.
 * @param timer  The timer to be configured.
 * @return       Numer of ticks until the timeout.
 */
uint16_t read_timeout(
    volatile meson_timer_reg_t *regs,
    timeout_id_t timer
);

/**
 * Write the time until the next timeout.
 *
 * @param regs   The timer registers.
 * @param timer  The timer to be configured.
 * @param value  The number of ticks to set until the next timeout.
 */
void write_timeout(
    volatile meson_timer_reg_t *regs,
    timeout_id_t timer,
    uint16_t value
);
